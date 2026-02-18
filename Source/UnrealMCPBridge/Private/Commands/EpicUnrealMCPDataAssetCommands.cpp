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
    if (CommandType == TEXT("list_data_asset_classes"))   return HandleListDataAssetClasses(Params);
    if (CommandType == TEXT("create_data_asset"))         return HandleCreateDataAsset(Params);
    if (CommandType == TEXT("get_data_asset_properties")) return HandleGetDataAssetProperties(Params);
    if (CommandType == TEXT("set_data_asset_property"))   return HandleSetDataAssetProperty(Params);
    if (CommandType == TEXT("set_data_asset_properties")) return HandleSetDataAssetProperties(Params);
    if (CommandType == TEXT("list_data_assets"))          return HandleListDataAssets(Params);

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
    // 1. FObjectProperty / FWeakObjectProperty — load UObject by path string
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
        if (!LoadedObj)
            LoadedObj = UEditorAssetLibrary::LoadAsset(PathStr);
        if (!LoadedObj)
        {
            OutError = FString::Printf(TEXT("Could not load object at path '%s' for property '%s'"),
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
    // 8. FStructProperty — FJsonObjectConverter handles all structs:
    //    FGameplayTag, FVector, FRotator, FColor, FLinearColor, FTransform,
    //    FDataTableRowHandle, custom structs, etc.
    // -----------------------------------------------------------------------
    if (FStructProperty* StructProp = CastField<FStructProperty>(Prop))
    {
        if (Value->Type == EJson::Object)
        {
            if (FJsonObjectConverter::JsonValueToUProperty(Value, Prop, PropAddr))
                return true;
            OutError = FString::Printf(TEXT("Failed to set struct property '%s'"), *PropertyName);
            return false;
        }
        OutError = FString::Printf(TEXT("Expected JSON object for struct property '%s'"), *PropertyName);
        return false;
    }

    // -----------------------------------------------------------------------
    // 9. FArrayProperty — JSON array → TArray<T>
    // -----------------------------------------------------------------------
    if (Prop->IsA<FArrayProperty>() || Prop->IsA<FSetProperty>() || Prop->IsA<FMapProperty>())
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
