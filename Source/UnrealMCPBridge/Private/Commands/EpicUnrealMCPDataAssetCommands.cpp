#include "Commands/EpicUnrealMCPDataAssetCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"

#include "Engine/DataAsset.h"
#include "Factories/DataAssetFactory.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "JsonObjectConverter.h"
#include "UObject/Field.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectIterator.h"
#include "InstancedStruct.h"
#include "StructUtils/InstancedStruct.h"

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
FEpicUnrealMCPDataAssetCommands::FEpicUnrealMCPDataAssetCommands()
{
}

// ---------------------------------------------------------------------------
// Dispatch
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FEpicUnrealMCPDataAssetCommands::HandleCommand(
    const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("list_data_asset_classes"))     return HandleListDataAssetClasses(Params);
    if (CommandType == TEXT("create_data_asset"))           return HandleCreateDataAsset(Params);
    if (CommandType == TEXT("get_data_asset_properties"))   return HandleGetDataAssetProperties(Params);
    if (CommandType == TEXT("set_data_asset_property"))     return HandleSetDataAssetProperty(Params);
    if (CommandType == TEXT("set_data_asset_properties"))   return HandleSetDataAssetProperties(Params);
    if (CommandType == TEXT("list_data_assets"))            return HandleListDataAssets(Params);
    if (CommandType == TEXT("get_property_valid_types"))    return HandleGetPropertyValidTypes(Params);

    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
        FString::Printf(TEXT("Unknown data asset command: %s"), *CommandType));
}

// ===========================================================================
// INTERNAL: ResolveDataAssetClass
// ===========================================================================
UClass* FEpicUnrealMCPDataAssetCommands::ResolveDataAssetClass(const FString& ClassName)
{
    if (ClassName.IsEmpty()) return nullptr;

    // 1. Already a full path like "/Script/Jiggify.UItemData"
    if (ClassName.Contains(TEXT(".")))
    {
        UClass* C = FindObject<UClass>(nullptr, *ClassName);
        if (C) return C;
    }

    // 2. Try short name via GetDerivedClasses
    TArray<UClass*> Derived;
    GetDerivedClasses(UDataAsset::StaticClass(), Derived, /*bRecursive=*/true);
    for (UClass* C : Derived)
    {
        if (!C) continue;
        // Match "UItemData" or "ItemData"
        if (C->GetName() == ClassName ||
            C->GetName() == (TEXT("U") + ClassName) ||
            C->GetName() == ClassName.RightChop(1))   // strip leading U
        {
            return C;
        }
    }

    return nullptr;
}

// ===========================================================================
// INTERNAL: ResolveAnyClass
// Finds ANY loaded UClass — not limited to UDataAsset subclasses.
// Accepts full path ("/Script/MassCrowd.MassCrowdVisualizationTrait"),
// short name ("MassCrowdVisualizationTrait"), or with/without "U" prefix.
// ===========================================================================
UClass* FEpicUnrealMCPDataAssetCommands::ResolveAnyClass(const FString& ClassName)
{
    if (ClassName.IsEmpty()) return nullptr;

    // 1. Full path
    if (ClassName.Contains(TEXT(".")))
    {
        UClass* C = FindObject<UClass>(nullptr, *ClassName);
        if (C) return C;
        C = LoadObject<UClass>(nullptr, *ClassName);
        if (C) return C;
    }

    // 2. Short name — iterate all loaded classes (fast in-editor)
    for (TObjectIterator<UClass> It; It; ++It)
    {
        UClass* C = *It;
        if (!C) continue;
        const FString Name = C->GetName();
        if (Name == ClassName ||
            Name == (TEXT("U") + ClassName) ||
            (ClassName.StartsWith(TEXT("U")) && Name == ClassName.RightChop(1)))
        {
            return C;
        }
    }
    return nullptr;
}

// ===========================================================================
// INTERNAL: SetPropertiesFromJson
// Applies all JSON fields from a JSON object to a UObject, skipping the
// "_ClassName" discriminator key. Called recursively for instanced subobjects.
// ===========================================================================
void FEpicUnrealMCPDataAssetCommands::SetPropertiesFromJson(
    UObject* Target, const TSharedPtr<FJsonObject>& Json, FString& OutErrors)
{
    if (!Target || !Json.IsValid()) return;
    for (const auto& Pair : Json->Values)
    {
        if (Pair.Key == TEXT("_ClassName")) continue;
        FString Err;
        if (!SetProperty(Target, Pair.Key, Pair.Value, Err))
            OutErrors += FString::Printf(TEXT("[%s: %s] "), *Pair.Key, *Err);
    }
}

// ===========================================================================
// INTERNAL: SetStructFieldsFromJson
// Like SetPropertiesFromJson but works on raw struct memory (UScriptStruct + void*).
// Needed when a top-level FStructProperty (e.g. FMassEntityConfig) contains
// instanced object arrays or FInstancedStruct arrays that FJsonObjectConverter
// cannot deserialize. Falls through to FJsonObjectConverter for simple fields.
// ===========================================================================
void FEpicUnrealMCPDataAssetCommands::SetStructFieldsFromJson(
    UScriptStruct* Struct, void* StructData,
    const TSharedPtr<FJsonObject>& Json, UObject* Outer, FString& OutErrors)
{
    if (!Struct || !StructData || !Json.IsValid()) return;

    for (const auto& Pair : Json->Values)
    {
        if (Pair.Key == TEXT("_ClassName")) continue;

        FProperty* FieldProp = nullptr;
        for (UStruct* S = Struct; S && !FieldProp; S = S->GetSuperStruct())
            FieldProp = S->FindPropertyByName(*Pair.Key);

        if (!FieldProp)
        {
            OutErrors += FString::Printf(TEXT("[field '%s' not found] "), *Pair.Key);
            continue;
        }

        void* FieldAddr = FieldProp->ContainerPtrToValuePtr<void>(StructData);

        // ---------------------------------------------------------------
        // Special case: FArrayProperty — check for instanced object arrays
        // and FInstancedStruct arrays that need manual handling
        // ---------------------------------------------------------------
        if (FArrayProperty* ArrProp = CastField<FArrayProperty>(FieldProp))
        {
            // Instanced object array (e.g. TArray<UMassEntityTraitBase*> with Instanced)
            FObjectProperty* InnerObj = CastField<FObjectProperty>(ArrProp->Inner);
            if (InnerObj
                && InnerObj->HasAnyPropertyFlags(CPF_PersistentInstance | CPF_InstancedReference)
                && Pair.Value->Type == EJson::Array)
            {
                const TArray<TSharedPtr<FJsonValue>>& JsonArr = Pair.Value->AsArray();
                FScriptArrayHelper ArrHelper(ArrProp, FieldAddr);
                ArrHelper.EmptyValues();
                ArrHelper.AddValues(JsonArr.Num());
                for (int32 i = 0; i < JsonArr.Num(); ++i)
                {
                    const TSharedPtr<FJsonValue>& Elem = JsonArr[i];
                    void* ElemAddr = ArrHelper.GetRawPtr(i);
                    if (Elem->Type == EJson::Object)
                    {
                        const TSharedPtr<FJsonObject>& ElemJson = Elem->AsObject();
                        FString ClassPath;
                        ElemJson->TryGetStringField(TEXT("_ClassName"), ClassPath);
                        UClass* ElemClass = InnerObj->PropertyClass;
                        if (!ClassPath.IsEmpty() && ClassPath != TEXT("None"))
                        {
                            UClass* R = FindObject<UClass>(nullptr, *ClassPath);
                            if (!R) R = LoadObject<UClass>(nullptr, *ClassPath);
                            if (R) ElemClass = R;
                        }
                        UObject* SubObj = NewObject<UObject>(Outer, ElemClass);
                        FString SubErrors;
                        SetPropertiesFromJson(SubObj, ElemJson, SubErrors);
                        InnerObj->SetObjectPropertyValue(ElemAddr, SubObj);
                    }
                    else if (Elem->Type == EJson::String)
                    {
                        FString ElemPath = Elem->AsString();
                        if (!ElemPath.IsEmpty() && ElemPath != TEXT("None"))
                        {
                            UObject* Loaded = StaticLoadObject(InnerObj->PropertyClass, nullptr, *ElemPath);
                            InnerObj->SetObjectPropertyValue(ElemAddr, Loaded);
                        }
                    }
                }
                continue;
            }

            // FInstancedStruct array (e.g. TArray<FInstancedStruct> Fragments)
            FStructProperty* InnerSP = CastField<FStructProperty>(ArrProp->Inner);
            if (InnerSP && InnerSP->Struct
                && InnerSP->Struct->GetName() == TEXT("InstancedStruct")
                && Pair.Value->Type == EJson::Array)
            {
                const TArray<TSharedPtr<FJsonValue>>& JsonArr = Pair.Value->AsArray();
                FScriptArrayHelper ArrHelper(ArrProp, FieldAddr);
                ArrHelper.EmptyValues();
                ArrHelper.AddValues(JsonArr.Num());
                for (int32 i = 0; i < JsonArr.Num(); ++i)
                {
                    const TSharedPtr<FJsonValue>& Elem = JsonArr[i];
                    void* ElemAddr = ArrHelper.GetRawPtr(i);
                    FInstancedStruct* InstStruct = reinterpret_cast<FInstancedStruct*>(ElemAddr);

                    FString StructPath;
                    if (Elem->Type == EJson::Object)
                        Elem->AsObject()->TryGetStringField(TEXT("_ClassName"), StructPath);
                    else if (Elem->Type == EJson::String)
                        StructPath = Elem->AsString();

                    if (!StructPath.IsEmpty() && StructPath != TEXT("None"))
                    {
                        UScriptStruct* TargetStruct = FindObject<UScriptStruct>(nullptr, *StructPath);
                        if (!TargetStruct) TargetStruct = LoadObject<UScriptStruct>(nullptr, *StructPath);
                        if (!TargetStruct)
                        {
                            for (TObjectIterator<UScriptStruct> It; It; ++It)
                            {
                                const FString N = (*It)->GetName();
                                if (N == StructPath || N == (TEXT("F") + StructPath) ||
                                    (StructPath.StartsWith(TEXT("F")) && N == StructPath.RightChop(1)) ||
                                    (*It)->GetPathName() == StructPath)
                                {
                                    TargetStruct = *It; break;
                                }
                            }
                        }
                        if (TargetStruct)
                            InstStruct->InitializeAs(TargetStruct);
                    }
                }
                continue;
            }
        }

        // Fall back to FJsonObjectConverter for all other field types
        FJsonObjectConverter::JsonValueToUProperty(Pair.Value, FieldProp, FieldAddr);
    }
}

// ===========================================================================
// INTERNAL: SetProperty
// Comprehensive property setter covering all UE5 property types.
// ===========================================================================
bool FEpicUnrealMCPDataAssetCommands::SetProperty(UObject* Object, const FString& PropertyName,
                                                    const TSharedPtr<FJsonValue>& Value, FString& OutError)
{
    if (!Object)
    {
        OutError = TEXT("Null object");
        return false;
    }

    // Find property by walking the full class hierarchy
    FProperty* Prop = nullptr;
    for (UClass* C = Object->GetClass(); C && !Prop; C = C->GetSuperClass())
        Prop = C->FindPropertyByName(*PropertyName);

    if (!Prop)
    {
        OutError = FString::Printf(TEXT("Property '%s' not found on class '%s'"),
                                   *PropertyName, *Object->GetClass()->GetName());
        return false;
    }

    void* PropAddr = Prop->ContainerPtrToValuePtr<void>(Object);

    // -----------------------------------------------------------------------
    // 1a. FObjectProperty — instanced subobject (JSON object with _ClassName)
    //     OR non-instanced (path string / null)
    // -----------------------------------------------------------------------
    if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Prop))
    {
        // Instanced subobject: value is a JSON object → NewObject + recurse
        if (ObjProp->HasAnyPropertyFlags(CPF_PersistentInstance | CPF_InstancedReference)
            && Value->Type == EJson::Object)
        {
            const TSharedPtr<FJsonObject>& JsonObj = Value->AsObject();
            FString ClassPath;
            JsonObj->TryGetStringField(TEXT("_ClassName"), ClassPath);

            UClass* SubClass = ObjProp->PropertyClass;
            if (!ClassPath.IsEmpty() && ClassPath != TEXT("None"))
            {
                UClass* Resolved = FindObject<UClass>(nullptr, *ClassPath);
                if (!Resolved) Resolved = LoadObject<UClass>(nullptr, *ClassPath);
                if (Resolved) SubClass = Resolved;
            }

            UObject* SubObj = NewObject<UObject>(Object, SubClass);
            FString SubErrors;
            SetPropertiesFromJson(SubObj, JsonObj, SubErrors);
            ObjProp->SetObjectPropertyValue(PropAddr, SubObj);
            if (!SubErrors.IsEmpty())
                UE_LOG(LogTemp, Warning, TEXT("SetProperty '%s' subobject warnings: %s"),
                       *PropertyName, *SubErrors);
            return true;
        }

        // Null / clear
        if (Value->Type == EJson::Null || Value->AsString() == TEXT("None") || Value->AsString().IsEmpty())
        {
            ObjProp->SetObjectPropertyValue(PropAddr, nullptr);
            return true;
        }

        // Non-instanced: load by path
        FString PathStr = Value->AsString();
        UObject* LoadedObj = StaticLoadObject(ObjProp->PropertyClass, nullptr, *PathStr);
        if (!LoadedObj) LoadedObj = UEditorAssetLibrary::LoadAsset(PathStr);
        if (!LoadedObj)
        {
            OutError = FString::Printf(TEXT("Could not load object '%s' for property '%s'"),
                                       *PathStr, *PropertyName);
            return false;
        }
        ObjProp->SetObjectPropertyValue(PropAddr, LoadedObj);
        return true;
    }

    // -----------------------------------------------------------------------
    // 1b. Other FObjectPropertyBase subclasses (FWeakObjectProperty, etc.)
    //     — always path-string based
    // -----------------------------------------------------------------------
    if (FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(Prop))
    {
        if (Value->Type == EJson::Null || Value->AsString() == TEXT("None") || Value->AsString().IsEmpty())
        {
            ObjProp->SetObjectPropertyValue(PropAddr, nullptr);
            return true;
        }
        FString PathStr = Value->AsString();
        UObject* LoadedObj = StaticLoadObject(ObjProp->PropertyClass, nullptr, *PathStr);
        if (!LoadedObj) LoadedObj = UEditorAssetLibrary::LoadAsset(PathStr);
        if (!LoadedObj)
        {
            OutError = FString::Printf(TEXT("Could not load object '%s' for property '%s'"),
                                       *PathStr, *PropertyName);
            return false;
        }
        ObjProp->SetObjectPropertyValue(PropAddr, LoadedObj);
        return true;
    }

    // -----------------------------------------------------------------------
    // 2. FSoftObjectProperty — store as path, don't force-load
    // -----------------------------------------------------------------------
    if (FSoftObjectProperty* SoftProp = CastField<FSoftObjectProperty>(Prop))
    {
        FSoftObjectPtr SoftPtr(FSoftObjectPath(Value->AsString()));
        SoftProp->SetPropertyValue(PropAddr, SoftPtr);
        return true;
    }

    // -----------------------------------------------------------------------
    // 3. FSoftClassProperty
    // -----------------------------------------------------------------------
    if (FSoftClassProperty* SoftClassProp = CastField<FSoftClassProperty>(Prop))
    {
        FSoftObjectPtr SoftPtr(FSoftObjectPath(Value->AsString()));
        SoftClassProp->SetPropertyValue(PropAddr, SoftPtr);
        return true;
    }

    // -----------------------------------------------------------------------
    // 4. FNameProperty
    // -----------------------------------------------------------------------
    if (FNameProperty* NameProp = CastField<FNameProperty>(Prop))
    {
        NameProp->SetPropertyValue(PropAddr, FName(*Value->AsString()));
        return true;
    }

    // -----------------------------------------------------------------------
    // 5. FTextProperty
    // -----------------------------------------------------------------------
    if (FTextProperty* TextProp = CastField<FTextProperty>(Prop))
    {
        TextProp->SetPropertyValue(PropAddr, FText::FromString(Value->AsString()));
        return true;
    }

    // -----------------------------------------------------------------------
    // 6. Numeric — int32, int64, double, float
    // -----------------------------------------------------------------------
    if (FIntProperty* IntProp = CastField<FIntProperty>(Prop))
    {
        IntProp->SetPropertyValue(PropAddr, static_cast<int32>(Value->AsNumber()));
        return true;
    }
    if (FInt64Property* Int64Prop = CastField<FInt64Property>(Prop))
    {
        Int64Prop->SetPropertyValue(PropAddr, static_cast<int64>(Value->AsNumber()));
        return true;
    }
    if (FDoubleProperty* DblProp = CastField<FDoubleProperty>(Prop))
    {
        DblProp->SetPropertyValue(PropAddr, Value->AsNumber());
        return true;
    }
    if (FFloatProperty* FltProp = CastField<FFloatProperty>(Prop))
    {
        FltProp->SetPropertyValue(PropAddr, static_cast<float>(Value->AsNumber()));
        return true;
    }
    if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Prop))
    {
        BoolProp->SetPropertyValue(PropAddr, Value->AsBool());
        return true;
    }
    if (FStrProperty* StrProp = CastField<FStrProperty>(Prop))
    {
        StrProp->SetPropertyValue(PropAddr, Value->AsString());
        return true;
    }

    // -----------------------------------------------------------------------
    // 7. Enums — FByteProperty (TEnumAsByte) and FEnumProperty
    // -----------------------------------------------------------------------
    if (FByteProperty* ByteProp = CastField<FByteProperty>(Prop))
    {
        UEnum* Enum = ByteProp->GetIntPropertyEnum();
        if (Enum)
        {
            int64 EnumVal = INDEX_NONE;
            if (Value->Type == EJson::Number)
                EnumVal = static_cast<int64>(Value->AsNumber());
            else
            {
                FString Str = Value->AsString();
                if (Str.Contains(TEXT("::"))) Str.Split(TEXT("::"), nullptr, &Str);
                EnumVal = Enum->GetValueByNameString(Str);
                if (EnumVal == INDEX_NONE) EnumVal = Enum->GetValueByNameString(Value->AsString());
            }
            if (EnumVal == INDEX_NONE) { OutError = FString::Printf(TEXT("Invalid enum value for '%s'"), *PropertyName); return false; }
            ByteProp->SetPropertyValue(PropAddr, static_cast<uint8>(EnumVal));
        }
        else
        {
            ByteProp->SetPropertyValue(PropAddr, static_cast<uint8>(Value->AsNumber()));
        }
        return true;
    }
    if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Prop))
    {
        UEnum* Enum = EnumProp->GetEnum();
        FNumericProperty* Underlying = EnumProp->GetUnderlyingProperty();
        if (Enum && Underlying)
        {
            int64 EnumVal = INDEX_NONE;
            if (Value->Type == EJson::Number)
                EnumVal = static_cast<int64>(Value->AsNumber());
            else
            {
                FString Str = Value->AsString();
                if (Str.Contains(TEXT("::"))) Str.Split(TEXT("::"), nullptr, &Str);
                EnumVal = Enum->GetValueByNameString(Str);
                if (EnumVal == INDEX_NONE) EnumVal = Enum->GetValueByNameString(Value->AsString());
            }
            if (EnumVal == INDEX_NONE) { OutError = FString::Printf(TEXT("Invalid enum value for '%s'"), *PropertyName); return false; }
            Underlying->SetIntPropertyValue(PropAddr, EnumVal);
        }
        return true;
    }

    // -----------------------------------------------------------------------
    // 8. FStructProperty — use SetStructFieldsFromJson so that nested instanced
    //    object arrays (e.g. FMassEntityConfig::Traits) and FInstancedStruct
    //    arrays (e.g. MassAssortedFragmentsTrait::Fragments) are handled
    //    correctly. Simple struct fields fall through to FJsonObjectConverter.
    // -----------------------------------------------------------------------
    if (FStructProperty* StructProp = CastField<FStructProperty>(Prop))
    {
        if (Value->Type == EJson::Object)
        {
            FString SubErrors;
            SetStructFieldsFromJson(StructProp->Struct, PropAddr, Value->AsObject(), Object, SubErrors);
            if (!SubErrors.IsEmpty())
                UE_LOG(LogTemp, Warning, TEXT("SetProperty '%s' struct warnings: %s"),
                       *PropertyName, *SubErrors);
            return true;
        }
        OutError = FString::Printf(TEXT("Expected JSON object for struct property '%s'"), *PropertyName);
        return false;
    }

    // -----------------------------------------------------------------------
    // 9. FArrayProperty — instanced object arrays get special handling;
    //    all other arrays/sets/maps fall through to FJsonObjectConverter.
    // -----------------------------------------------------------------------
    if (FArrayProperty* ArrProp = CastField<FArrayProperty>(Prop))
    {
        FObjectProperty* InnerObj = CastField<FObjectProperty>(ArrProp->Inner);

        // Instanced object array: each JSON element is either an object with
        // "_ClassName" (→ NewObject + SetPropertiesFromJson) or a path string.
        if (InnerObj
            && InnerObj->HasAnyPropertyFlags(CPF_PersistentInstance | CPF_InstancedReference)
            && Value->Type == EJson::Array)
        {
            const TArray<TSharedPtr<FJsonValue>>& JsonArr = Value->AsArray();
            FScriptArrayHelper ArrHelper(ArrProp, PropAddr);
            ArrHelper.EmptyValues();
            ArrHelper.AddValues(JsonArr.Num());

            for (int32 i = 0; i < JsonArr.Num(); ++i)
            {
                const TSharedPtr<FJsonValue>& Elem = JsonArr[i];
                void* ElemAddr = ArrHelper.GetRawPtr(i);

                if (Elem->Type == EJson::Object)
                {
                    const TSharedPtr<FJsonObject>& ElemJson = Elem->AsObject();

                    // Resolve the concrete class from "_ClassName"
                    FString ClassPath;
                    ElemJson->TryGetStringField(TEXT("_ClassName"), ClassPath);

                    UClass* ElemClass = InnerObj->PropertyClass;
                    if (!ClassPath.IsEmpty() && ClassPath != TEXT("None"))
                    {
                        UClass* Resolved = FindObject<UClass>(nullptr, *ClassPath);
                        if (!Resolved) Resolved = LoadObject<UClass>(nullptr, *ClassPath);
                        if (Resolved) ElemClass = Resolved;
                    }

                    // Create the subobject and apply all JSON properties to it
                    UObject* SubObj = NewObject<UObject>(Object, ElemClass);
                    FString SubErrors;
                    SetPropertiesFromJson(SubObj, ElemJson, SubErrors);
                    InnerObj->SetObjectPropertyValue(ElemAddr, SubObj);

                    if (!SubErrors.IsEmpty())
                        UE_LOG(LogTemp, Warning,
                               TEXT("SetProperty '%s'[%d] subobject warnings: %s"),
                               *PropertyName, i, *SubErrors);
                }
                else if (Elem->Type == EJson::String)
                {
                    FString ElemPath = Elem->AsString();
                    if (!ElemPath.IsEmpty() && ElemPath != TEXT("None"))
                    {
                        UObject* LoadedObj = StaticLoadObject(InnerObj->PropertyClass, nullptr, *ElemPath);
                        InnerObj->SetObjectPropertyValue(ElemAddr, LoadedObj);
                    }
                }
            }
            return true;
        }

        // FInstancedStruct array (TArray<FInstancedStruct>): inner is FStructProperty
        // whose Struct is named "InstancedStruct". Each JSON element specifies a
        // UScriptStruct via "_ClassName" and the FInstancedStruct is zero-initialised
        // to that type (matching what the editor does when you pick a struct in a dropdown).
        FStructProperty* InnerStructProp = CastField<FStructProperty>(ArrProp->Inner);
        if (InnerStructProp
            && InnerStructProp->Struct
            && InnerStructProp->Struct->GetName() == TEXT("InstancedStruct")
            && Value->Type == EJson::Array)
        {
            const TArray<TSharedPtr<FJsonValue>>& JsonArr = Value->AsArray();
            FScriptArrayHelper ArrHelper(ArrProp, PropAddr);
            ArrHelper.EmptyValues();
            ArrHelper.AddValues(JsonArr.Num());

            for (int32 i = 0; i < JsonArr.Num(); ++i)
            {
                const TSharedPtr<FJsonValue>& Elem = JsonArr[i];
                void* ElemAddr = ArrHelper.GetRawPtr(i);
                FInstancedStruct* InstStruct = reinterpret_cast<FInstancedStruct*>(ElemAddr);

                auto ResolveScriptStruct = [](const FString& StructPath) -> UScriptStruct*
                {
                    if (StructPath.IsEmpty() || StructPath == TEXT("None")) return nullptr;
                    // Full path
                    UScriptStruct* S = FindObject<UScriptStruct>(nullptr, *StructPath);
                    if (S) return S;
                    S = LoadObject<UScriptStruct>(nullptr, *StructPath);
                    if (S) return S;
                    // Short name fallback
                    for (TObjectIterator<UScriptStruct> It; It; ++It)
                    {
                        const FString N = (*It)->GetName();
                        if (N == StructPath || N == (TEXT("F") + StructPath) ||
                            (StructPath.StartsWith(TEXT("F")) && N == StructPath.RightChop(1)) ||
                            (*It)->GetPathName() == StructPath)
                        {
                            return *It;
                        }
                    }
                    return nullptr;
                };

                if (Elem->Type == EJson::Object)
                {
                    const TSharedPtr<FJsonObject>& ElemJson = Elem->AsObject();
                    FString StructPath;
                    ElemJson->TryGetStringField(TEXT("_ClassName"), StructPath);

                    UScriptStruct* TargetStruct = ResolveScriptStruct(StructPath);
                    if (TargetStruct)
                    {
                        InstStruct->InitializeAs(TargetStruct);

                        // Apply any additional JSON fields onto the struct memory
                        uint8* StructMem = InstStruct->GetMutableMemory();
                        if (StructMem)
                        {
                            for (const auto& Pair : ElemJson->Values)
                            {
                                if (Pair.Key == TEXT("_ClassName")) continue;
                                FProperty* FieldProp = TargetStruct->FindPropertyByName(*Pair.Key);
                                if (!FieldProp) continue;
                                void* FieldAddr = FieldProp->ContainerPtrToValuePtr<void>(StructMem);
                                FJsonObjectConverter::JsonValueToUProperty(Pair.Value, FieldProp, FieldAddr);
                            }
                        }
                    }
                    else if (!StructPath.IsEmpty())
                    {
                        UE_LOG(LogTemp, Warning,
                               TEXT("SetProperty '%s'[%d]: could not resolve struct '%s'"),
                               *PropertyName, i, *StructPath);
                    }
                }
                else if (Elem->Type == EJson::String)
                {
                    UScriptStruct* TargetStruct = ResolveScriptStruct(Elem->AsString());
                    if (TargetStruct)
                        InstStruct->InitializeAs(TargetStruct);
                }
            }
            return true;
        }

        // Non-instanced array (structs, primitives, etc.)
        if (FJsonObjectConverter::JsonValueToUProperty(Value, Prop, PropAddr))
            return true;
        OutError = FString::Printf(TEXT("Failed to set array property '%s'"), *PropertyName);
        return false;
    }

    if (Prop->IsA<FSetProperty>() || Prop->IsA<FMapProperty>())
    {
        if (FJsonObjectConverter::JsonValueToUProperty(Value, Prop, PropAddr))
            return true;
        OutError = FString::Printf(TEXT("Failed to set container property '%s'"), *PropertyName);
        return false;
    }

    // -----------------------------------------------------------------------
    // 10. Fallback: let FJsonObjectConverter try everything else
    // -----------------------------------------------------------------------
    if (FJsonObjectConverter::JsonValueToUProperty(Value, Prop, PropAddr))
        return true;

    OutError = FString::Printf(TEXT("Unsupported property type '%s' for property '%s'"),
                               *Prop->GetClass()->GetName(), *PropertyName);
    return false;
}

// ===========================================================================
// INTERNAL: SerializeAllProperties
// ===========================================================================
TSharedPtr<FJsonObject> FEpicUnrealMCPDataAssetCommands::SerializeAllProperties(
    UObject* Object, const FString& FilterLower, bool bIncludeInherited)
{
    TSharedPtr<FJsonObject> Props = MakeShared<FJsonObject>();

    const EFieldIteratorFlags::SuperClassFlags SuperFlag =
        bIncludeInherited ? EFieldIteratorFlags::IncludeSuper : EFieldIteratorFlags::ExcludeSuper;

    for (TFieldIterator<FProperty> It(Object->GetClass(), SuperFlag); It; ++It)
    {
        FProperty* Prop = *It;
        const FString Name = Prop->GetName();
        if (!FilterLower.IsEmpty() && !Name.ToLower().Contains(FilterLower))
            continue;

        const void* Addr = Prop->ContainerPtrToValuePtr<void>(Object);
        TSharedPtr<FJsonValue> Val = FJsonObjectConverter::UPropertyToJsonValue(Prop, Addr);
        if (Val.IsValid())
            Props->SetField(Name, Val);
    }
    return Props;
}

// ===========================================================================
// get_property_valid_types
// Returns the exact set of classes/structs/enum-values the editor dropdown
// would show for a given property slot on any UClass.
//
// Handles:
//  • TArray<UObject*> Instanced / single UObject* Instanced
//      → GetDerivedClasses(PropertyClass) — same as editor's class picker
//  • TSubclassOf<T> (FClassProperty)
//      → GetDerivedClasses(MetaClass)
//  • TArray<FInstancedStruct> with meta=(BaseStruct=...)
//      → TObjectIterator<UScriptStruct> filtered by IsChildOf(BaseStruct)
//      → reads ExcludeBaseStruct meta flag to mirror the editor picker
//  • FEnumProperty / FByteProperty with enum
//      → returns all named enum entries
//
// property_path supports dot-notation to navigate into structs/objects:
//  "Config.Traits"  →  finds "Config" (FStructProperty → FMassEntityConfig),
//                       then "Traits" inside it
// ===========================================================================
TSharedPtr<FJsonObject> FEpicUnrealMCPDataAssetCommands::HandleGetPropertyValidTypes(
    const TSharedPtr<FJsonObject>& Params)
{
    FString ClassName, PropertyPath;
    if (!Params->TryGetStringField(TEXT("class_name"), ClassName))
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'class_name'"));
    if (!Params->TryGetStringField(TEXT("property_path"), PropertyPath))
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'property_path'"));

    bool bIncludeAbstract = false;
    Params->TryGetBoolField(TEXT("include_abstract"), bIncludeAbstract);

    // ------------------------------------------------------------------
    // 1. Resolve the starting UStruct (UClass is a UStruct)
    // ------------------------------------------------------------------
    UStruct* CurrentStruct = ResolveAnyClass(ClassName);
    if (!CurrentStruct)
    {
        // Also try as a UScriptStruct (e.g. "MassEntityConfig")
        for (TObjectIterator<UScriptStruct> It; It; ++It)
        {
            const FString N = (*It)->GetName();
            if (N == ClassName || N == (TEXT("F") + ClassName) ||
                (ClassName.StartsWith(TEXT("F")) && N == ClassName.RightChop(1)))
            {
                CurrentStruct = *It;
                break;
            }
        }
    }
    if (!CurrentStruct)
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Class/struct not found: '%s'"), *ClassName));

    // ------------------------------------------------------------------
    // 2. Navigate dot-separated property path
    //    e.g. "Config.Traits"  →  Config (FStructProperty), then Traits
    // ------------------------------------------------------------------
    TArray<FString> Parts;
    PropertyPath.ParseIntoArray(Parts, TEXT("."), true);

    FProperty* TargetProp = nullptr;
    for (int32 i = 0; i < Parts.Num(); ++i)
    {
        TargetProp = CurrentStruct->FindPropertyByName(FName(*Parts[i]));
        if (!TargetProp)
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("Property '%s' not found on '%s'"),
                                *Parts[i], *CurrentStruct->GetName()));

        // If not the last segment, drill into the property's struct/class
        if (i < Parts.Num() - 1)
        {
            // Unwrap array to get at the inner property type
            FProperty* Inner = TargetProp;
            if (FArrayProperty* ArrProp = CastField<FArrayProperty>(TargetProp))
                Inner = ArrProp->Inner;

            if (FStructProperty* SP = CastField<FStructProperty>(Inner))
                CurrentStruct = SP->Struct;
            else if (FObjectProperty* OP = CastField<FObjectProperty>(Inner))
                CurrentStruct = OP->PropertyClass;
            else
                return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
                    FString::Printf(TEXT("Cannot navigate into property '%s' (not struct/object)"),
                                    *Parts[i]));
        }
    }

    if (!TargetProp)
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Property path '%s' resolved to null"), *PropertyPath));

    // ------------------------------------------------------------------
    // 3. Unwrap outer array to inspect the element type
    // ------------------------------------------------------------------
    FProperty* ElementProp = TargetProp;
    bool bIsArray = false;
    if (FArrayProperty* ArrProp = CastField<FArrayProperty>(TargetProp))
    {
        ElementProp = ArrProp->Inner;
        bIsArray = true;
    }

    // ------------------------------------------------------------------
    // Helper: build a class entry JSON object
    // ------------------------------------------------------------------
    auto MakeClassEntry = [](UClass* C) -> TSharedPtr<FJsonObject>
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetStringField(TEXT("name"),         C->GetName());
        Obj->SetStringField(TEXT("path"),         C->GetPathName());
        Obj->SetStringField(TEXT("parent"),       C->GetSuperClass()
                                                   ? C->GetSuperClass()->GetName()
                                                   : TEXT("None"));
        Obj->SetBoolField(TEXT("is_abstract"),    C->HasAnyClassFlags(CLASS_Abstract));
        Obj->SetBoolField(TEXT("is_deprecated"),  C->HasAnyClassFlags(CLASS_Deprecated));
        return Obj;
    };

    auto MakeStructEntry = [](UScriptStruct* S) -> TSharedPtr<FJsonObject>
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetStringField(TEXT("name"),   S->GetName());
        Obj->SetStringField(TEXT("path"),   S->GetPathName());
        Obj->SetStringField(TEXT("parent"), S->GetSuperStruct()
                                            ? S->GetSuperStruct()->GetName()
                                            : TEXT("None"));
        return Obj;
    };

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("class_name"),     ClassName);
    Result->SetStringField(TEXT("property_path"),  PropertyPath);
    Result->SetBoolField(TEXT("is_array"),          bIsArray);

    // ------------------------------------------------------------------
    // 4a. FClassProperty (TSubclassOf<T>) — enumerate MetaClass descendants
    // ------------------------------------------------------------------
    if (FClassProperty* ClassProp = CastField<FClassProperty>(ElementProp))
    {
        Result->SetStringField(TEXT("kind"),      TEXT("subclass"));
        Result->SetStringField(TEXT("base_class"), ClassProp->MetaClass
                                                    ? ClassProp->MetaClass->GetName()
                                                    : TEXT("UObject"));
        TArray<UClass*> Derived;
        if (ClassProp->MetaClass)
            GetDerivedClasses(ClassProp->MetaClass, Derived, true);

        TArray<TSharedPtr<FJsonValue>> Items;
        for (UClass* C : Derived)
        {
            if (!C) continue;
            if (!bIncludeAbstract && C->HasAnyClassFlags(CLASS_Abstract)) continue;
            if (C->HasAnyClassFlags(CLASS_Deprecated)) continue;
            Items.Add(MakeShared<FJsonValueObject>(MakeClassEntry(C)));
        }
        Result->SetNumberField(TEXT("count"), Items.Num());
        Result->SetArrayField(TEXT("valid_types"), Items);
        return Result;
    }

    // ------------------------------------------------------------------
    // 4b. FObjectProperty (instanced or plain) — GetDerivedClasses(PropertyClass)
    // ------------------------------------------------------------------
    if (FObjectProperty* ObjProp = CastField<FObjectProperty>(ElementProp))
    {
        const bool bInstanced = ObjProp->HasAnyPropertyFlags(
            CPF_PersistentInstance | CPF_InstancedReference);
        Result->SetStringField(TEXT("kind"),       bInstanced
                                                    ? TEXT("instanced_object")
                                                    : TEXT("object_reference"));
        Result->SetStringField(TEXT("base_class"), ObjProp->PropertyClass
                                                    ? ObjProp->PropertyClass->GetName()
                                                    : TEXT("UObject"));

        TArray<UClass*> Derived;
        if (ObjProp->PropertyClass)
            GetDerivedClasses(ObjProp->PropertyClass, Derived, true);

        // Also include the base class itself if it's concrete
        if (ObjProp->PropertyClass &&
            !ObjProp->PropertyClass->HasAnyClassFlags(CLASS_Abstract))
            Derived.AddUnique(ObjProp->PropertyClass);

        TArray<TSharedPtr<FJsonValue>> Items;
        for (UClass* C : Derived)
        {
            if (!C) continue;
            if (!bIncludeAbstract && C->HasAnyClassFlags(CLASS_Abstract)) continue;
            if (C->HasAnyClassFlags(CLASS_Deprecated)) continue;
            Items.Add(MakeShared<FJsonValueObject>(MakeClassEntry(C)));
        }
        Result->SetNumberField(TEXT("count"), Items.Num());
        Result->SetArrayField(TEXT("valid_types"), Items);
        return Result;
    }

    // ------------------------------------------------------------------
    // 4c. FStructProperty where Struct == FInstancedStruct
    //     Uses TObjectIterator<UScriptStruct> + BaseStruct meta,
    //     mirroring SStructViewer.cpp line 940 and FInstancedStructFilter
    // ------------------------------------------------------------------
    if (FStructProperty* StructProp = CastField<FStructProperty>(ElementProp))
    {
        if (StructProp->Struct && StructProp->Struct->GetName() == TEXT("InstancedStruct"))
        {
            // Read BaseStruct metadata from the ARRAY property (not the inner)
            // so we catch meta=(BaseStruct=...) on the TArray declaration
            FProperty* MetaSource = bIsArray ? TargetProp : TargetProp;
            FString BaseStructMeta = MetaSource->GetMetaData(TEXT("BaseStruct"));
            const bool bExcludeBase = MetaSource->HasMetaData(TEXT("ExcludeBaseStruct"));

            UScriptStruct* BaseStruct = nullptr;
            if (!BaseStructMeta.IsEmpty())
            {
                // Try full path first, then short name (matches FInstancedStructDetails)
                BaseStruct = FindObject<UScriptStruct>(nullptr, *BaseStructMeta);
                if (!BaseStruct)
                {
                    for (TObjectIterator<UScriptStruct> It; It; ++It)
                    {
                        const FString N = (*It)->GetName();
                        if (N == BaseStructMeta ||
                            N == (TEXT("F") + BaseStructMeta) ||
                            (*It)->GetPathName() == BaseStructMeta)
                        {
                            BaseStruct = *It;
                            break;
                        }
                    }
                }
            }

            Result->SetStringField(TEXT("kind"),        TEXT("instanced_struct"));
            Result->SetStringField(TEXT("base_struct"),  BaseStruct
                                                         ? BaseStruct->GetName()
                                                         : TEXT("(any)"));

            // Enumerate: mirrors TObjectIterator<UScriptStruct> in SStructViewer.cpp
            TArray<TSharedPtr<FJsonValue>> Items;
            for (TObjectIterator<UScriptStruct> It; It; ++It)
            {
                UScriptStruct* S = *It;
                if (!S) continue;
                if (S->GetOutermost() == GetTransientPackage()) continue;
                if (BaseStruct && !S->IsChildOf(BaseStruct)) continue;
                if (bExcludeBase && S == BaseStruct) continue;
                Items.Add(MakeShared<FJsonValueObject>(MakeStructEntry(S)));
            }
            Result->SetNumberField(TEXT("count"), Items.Num());
            Result->SetArrayField(TEXT("valid_types"), Items);
            return Result;
        }

        // Plain struct — not a dropdown, but report the struct type
        Result->SetStringField(TEXT("kind"),        TEXT("struct"));
        Result->SetStringField(TEXT("struct_type"),  StructProp->Struct
                                                      ? StructProp->Struct->GetName()
                                                      : TEXT("unknown"));
        Result->SetNumberField(TEXT("count"), 0);
        Result->SetArrayField(TEXT("valid_types"), TArray<TSharedPtr<FJsonValue>>());
        return Result;
    }

    // ------------------------------------------------------------------
    // 4d. FEnumProperty / FByteProperty — return all named enum entries
    // ------------------------------------------------------------------
    auto SerialiseEnum = [&](UEnum* Enum) -> TSharedPtr<FJsonObject>
    {
        TArray<TSharedPtr<FJsonValue>> Items;
        for (int32 i = 0; i < Enum->NumEnums() - 1; ++i)  // skip _MAX
        {
            TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
            Obj->SetStringField(TEXT("name"),        Enum->GetNameStringByIndex(i));
            Obj->SetStringField(TEXT("display_name"), Enum->GetDisplayNameTextByIndex(i).ToString());
            Obj->SetNumberField(TEXT("value"),       static_cast<double>(Enum->GetValueByIndex(i)));
            Items.Add(MakeShared<FJsonValueObject>(Obj));
        }
        Result->SetStringField(TEXT("kind"),       TEXT("enum"));
        Result->SetStringField(TEXT("enum_name"),   Enum->GetName());
        Result->SetNumberField(TEXT("count"),       Items.Num());
        Result->SetArrayField(TEXT("valid_types"),  Items);
        return Result;
    };

    if (FEnumProperty* EnumProp = CastField<FEnumProperty>(ElementProp))
        return SerialiseEnum(EnumProp->GetEnum());

    if (FByteProperty* ByteProp = CastField<FByteProperty>(ElementProp))
        if (UEnum* Enum = ByteProp->GetIntPropertyEnum())
            return SerialiseEnum(Enum);

    // ------------------------------------------------------------------
    // 4e. Anything else — report the property type but no dropdown
    // ------------------------------------------------------------------
    Result->SetStringField(TEXT("kind"),  FString::Printf(TEXT("primitive_%s"),
                                          *ElementProp->GetClass()->GetName()));
    Result->SetNumberField(TEXT("count"), 0);
    Result->SetArrayField(TEXT("valid_types"), TArray<TSharedPtr<FJsonValue>>());
    return Result;
}

// ===========================================================================
// list_data_asset_classes
// ===========================================================================
TSharedPtr<FJsonObject> FEpicUnrealMCPDataAssetCommands::HandleListDataAssetClasses(
    const TSharedPtr<FJsonObject>& Params)
{
    // Optional: filter by name substring
    FString Filter;
    Params->TryGetStringField(TEXT("filter"), Filter);
    const FString FilterLower = Filter.ToLower();

    // Optional: include abstract classes (default false)
    bool bIncludeAbstract = false;
    Params->TryGetBoolField(TEXT("include_abstract"), bIncludeAbstract);

    TArray<UClass*> Derived;
    GetDerivedClasses(UDataAsset::StaticClass(), Derived, /*bRecursive=*/true);

    TArray<TSharedPtr<FJsonValue>> ClassArray;
    for (UClass* C : Derived)
    {
        if (!C) continue;
        if (!bIncludeAbstract && C->HasAnyClassFlags(CLASS_Abstract)) continue;

        const FString Name = C->GetName();
        if (!FilterLower.IsEmpty() && !Name.ToLower().Contains(FilterLower)) continue;

        // Count editable properties
        int32 PropCount = 0;
        for (TFieldIterator<FProperty> It(C, EFieldIteratorFlags::IncludeSuper); It; ++It)
            if ((*It)->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible)) ++PropCount;

        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetStringField(TEXT("name"),         Name);
        Obj->SetStringField(TEXT("path"),         C->GetPathName());
        Obj->SetStringField(TEXT("parent_class"), C->GetSuperClass() ? C->GetSuperClass()->GetName() : TEXT("None"));
        Obj->SetBoolField(TEXT("is_abstract"),    C->HasAnyClassFlags(CLASS_Abstract));
        Obj->SetBoolField(TEXT("is_primary"),     C->IsChildOf(UPrimaryDataAsset::StaticClass()));
        Obj->SetNumberField(TEXT("property_count"), PropCount);
        ClassArray.Add(MakeShared<FJsonValueObject>(Obj));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetNumberField(TEXT("count"),   ClassArray.Num());
    Result->SetArrayField(TEXT("classes"), ClassArray);
    return Result;
}

// ===========================================================================
// create_data_asset
// ===========================================================================
TSharedPtr<FJsonObject> FEpicUnrealMCPDataAssetCommands::HandleCreateDataAsset(
    const TSharedPtr<FJsonObject>& Params)
{
    FString ClassName, AssetPath;
    if (!Params->TryGetStringField(TEXT("class_name"), ClassName))
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'class_name' parameter"));
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));

    // Resolve class
    UClass* AssetClass = ResolveDataAssetClass(ClassName);
    if (!AssetClass)
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Data asset class not found: '%s'"), *ClassName));
    if (!AssetClass->IsChildOf(UDataAsset::StaticClass()))
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Class '%s' is not a UDataAsset subclass"), *ClassName));

    // Split path into package dir + asset name
    FString PackagePath, AssetName;
    int32 LastSlash = INDEX_NONE;
    if (AssetPath.FindLastChar('/', LastSlash) && LastSlash != INDEX_NONE)
    {
        PackagePath = AssetPath.Left(LastSlash);
        AssetName   = AssetPath.Mid(LastSlash + 1);
    }
    else
    {
        AssetName   = AssetPath;
        PackagePath = TEXT("/Game");
    }

    // Check if it already exists
    const FString FullPath = PackagePath + TEXT("/") + AssetName;
    if (UEditorAssetLibrary::DoesAssetExist(FullPath))
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Asset already exists: %s"), *FullPath));

    // Create via UDataAssetFactory
    UDataAssetFactory* Factory = NewObject<UDataAssetFactory>();
    Factory->DataAssetClass = AssetClass;

    IAssetTools& AssetTools =
        FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();

    UObject* NewAsset = AssetTools.CreateAsset(AssetName, PackagePath, AssetClass, Factory);
    if (!NewAsset)
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Failed to create data asset '%s' of class '%s'"), *FullPath, *ClassName));

    // Apply optional initial_properties
    const TSharedPtr<FJsonObject>* InitialProps = nullptr;
    if (Params->TryGetObjectField(TEXT("initial_properties"), InitialProps) && InitialProps)
    {
        TArray<FString> FailedProps;
        for (const auto& Pair : (*InitialProps)->Values)
        {
            FString Err;
            if (!SetProperty(NewAsset, Pair.Key, Pair.Value, Err))
                FailedProps.Add(FString::Printf(TEXT("%s: %s"), *Pair.Key, *Err));
        }
        NewAsset->MarkPackageDirty();
        UEditorAssetLibrary::SaveAsset(FullPath);

        if (FailedProps.Num() > 0)
        {
            TSharedPtr<FJsonObject> Res = MakeShared<FJsonObject>();
            Res->SetBoolField(TEXT("success"), true);
            Res->SetStringField(TEXT("path"),  FullPath);
            Res->SetStringField(TEXT("class"), AssetClass->GetName());
            TArray<TSharedPtr<FJsonValue>> WarnArr;
            for (const FString& W : FailedProps)
                WarnArr.Add(MakeShared<FJsonValueString>(W));
            Res->SetArrayField(TEXT("property_warnings"), WarnArr);
            return Res;
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("path"),  FullPath);
    Result->SetStringField(TEXT("class"), AssetClass->GetName());
    return Result;
}

// ===========================================================================
// get_data_asset_properties
// ===========================================================================
TSharedPtr<FJsonObject> FEpicUnrealMCPDataAssetCommands::HandleGetDataAssetProperties(
    const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));

    FString Filter;
    Params->TryGetStringField(TEXT("filter"), Filter);

    bool bIncludeInherited = true;
    Params->TryGetBoolField(TEXT("include_inherited"), bIncludeInherited);

    UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
    if (!Asset)
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Asset not found: %s"), *AssetPath));

    if (!Asset->IsA(UDataAsset::StaticClass()))
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Asset '%s' is not a UDataAsset (class: %s)"),
                           *AssetPath, *Asset->GetClass()->GetName()));

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"),          true);
    Result->SetStringField(TEXT("asset_path"),     AssetPath);
    Result->SetStringField(TEXT("class"),          Asset->GetClass()->GetName());
    Result->SetStringField(TEXT("parent_class"),   Asset->GetClass()->GetSuperClass()
                                                    ? Asset->GetClass()->GetSuperClass()->GetName()
                                                    : TEXT("None"));
    Result->SetObjectField(TEXT("properties"),
        SerializeAllProperties(Asset, Filter.ToLower(), bIncludeInherited));
    return Result;
}

// ===========================================================================
// set_data_asset_property  (single)
// ===========================================================================
TSharedPtr<FJsonObject> FEpicUnrealMCPDataAssetCommands::HandleSetDataAssetProperty(
    const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath, PropertyName;
    if (!Params->TryGetStringField(TEXT("asset_path"),    AssetPath))
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));
    if (!Params->TryGetStringField(TEXT("property_name"), PropertyName))
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'property_name' parameter"));

    TSharedPtr<FJsonValue> PropertyValue = Params->TryGetField(TEXT("property_value"));
    if (!PropertyValue.IsValid())
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'property_value' parameter"));

    UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
    if (!Asset)
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Asset not found: %s"), *AssetPath));

    FString Err;
    if (!SetProperty(Asset, PropertyName, PropertyValue, Err))
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Err);

    Asset->MarkPackageDirty();
    UEditorAssetLibrary::SaveAsset(AssetPath);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("asset_path"),    AssetPath);
    Result->SetStringField(TEXT("property_name"), PropertyName);
    return Result;
}

// ===========================================================================
// set_data_asset_properties  (bulk)
// ===========================================================================
TSharedPtr<FJsonObject> FEpicUnrealMCPDataAssetCommands::HandleSetDataAssetProperties(
    const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));

    const TSharedPtr<FJsonObject>* PropsObj = nullptr;
    if (!Params->TryGetObjectField(TEXT("properties"), PropsObj) || !PropsObj)
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'properties' object parameter"));

    UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
    if (!Asset)
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Asset not found: %s"), *AssetPath));

    TArray<FString> Set, Failed;
    for (const auto& Pair : (*PropsObj)->Values)
    {
        FString Err;
        if (SetProperty(Asset, Pair.Key, Pair.Value, Err))
            Set.Add(Pair.Key);
        else
            Failed.Add(FString::Printf(TEXT("%s: %s"), *Pair.Key, *Err));
    }

    Asset->MarkPackageDirty();
    UEditorAssetLibrary::SaveAsset(AssetPath);

    TArray<TSharedPtr<FJsonValue>> SetArr, FailArr;
    for (const FString& S : Set)   SetArr.Add(MakeShared<FJsonValueString>(S));
    for (const FString& F : Failed) FailArr.Add(MakeShared<FJsonValueString>(F));

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"),        Failed.IsEmpty());
    Result->SetStringField(TEXT("asset_path"),   AssetPath);
    Result->SetNumberField(TEXT("set_count"),    Set.Num());
    Result->SetNumberField(TEXT("failed_count"), Failed.Num());
    Result->SetArrayField(TEXT("set"),           SetArr);
    if (!FailArr.IsEmpty())
        Result->SetArrayField(TEXT("failed"), FailArr);
    return Result;
}

// ===========================================================================
// list_data_assets
// ===========================================================================
TSharedPtr<FJsonObject> FEpicUnrealMCPDataAssetCommands::HandleListDataAssets(
    const TSharedPtr<FJsonObject>& Params)
{
    FString SearchPath = TEXT("/Game");
    bool bRecursive = true;
    FString ClassFilter;
    double MaxResultsD = 200.0;
    int32 MaxResults = 200;

    Params->TryGetStringField(TEXT("path"),         SearchPath);
    Params->TryGetBoolField(TEXT("recursive"),      bRecursive);
    Params->TryGetStringField(TEXT("class_filter"), ClassFilter);
    if (Params->TryGetNumberField(TEXT("max_results"), MaxResultsD))
        MaxResults = static_cast<int32>(MaxResultsD);

    IAssetRegistry& Registry =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

    // Build filter: all assets whose class is a child of UDataAsset
    FARFilter Filter;
    Filter.PackagePaths.Add(FName(*SearchPath));
    Filter.bRecursivePaths      = bRecursive;
    Filter.bRecursiveClasses    = true;

    if (!ClassFilter.IsEmpty())
    {
        // Resolve the exact class and use its path
        UClass* FilterClass = ResolveDataAssetClass(ClassFilter);
        if (FilterClass)
            Filter.ClassPaths.Add(FilterClass->GetClassPathName());
        else
            Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Engine"), FName(*ClassFilter)));
    }
    else
    {
        // All UDataAsset subclasses
        Filter.ClassPaths.Add(UDataAsset::StaticClass()->GetClassPathName());
    }

    TArray<FAssetData> Assets;
    Registry.GetAssets(Filter, Assets);

    if (Assets.Num() > MaxResults)
        Assets.SetNum(MaxResults);

    TArray<TSharedPtr<FJsonValue>> AssetArray;
    for (const FAssetData& AD : Assets)
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetStringField(TEXT("name"),  AD.AssetName.ToString());
        Obj->SetStringField(TEXT("path"),  AD.GetObjectPathString());
        Obj->SetStringField(TEXT("class"), AD.AssetClassPath.GetAssetName().ToString());
        AssetArray.Add(MakeShared<FJsonValueObject>(Obj));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetNumberField(TEXT("count"),  Assets.Num());
    Result->SetArrayField(TEXT("assets"), AssetArray);
    return Result;
}
