#include "Commands/EpicUnrealMCPNiagaraCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "NiagaraHelpers.h"
#include "NiagaraPropertyIntrospection.h"

#include "NiagaraSystem.h"
#include "NiagaraScript.h"

#if WITH_EDITORONLY_DATA
#include "NiagaraEditorModule.h"
#include "ViewModels/NiagaraSystemViewModel.h"
#include "ViewModels/NiagaraScratchPadViewModel.h"
#include "ViewModels/NiagaraScratchPadScriptViewModel.h"
#include "EditorAssetLibrary.h"
#endif


// ---------------------------------------------------------------------------
// Phase 2 — Apply / Apply&Save per-script
// Phase 3 — Script details panel get/set (FVersionedNiagaraScriptData fields)
// ---------------------------------------------------------------------------

#if WITH_EDITORONLY_DATA
namespace
{

/** Resolve a scratch pad script's ScriptViewModel (one per script, held by
 *  the system's scratch pad view model when editor open). */
TSharedPtr<FNiagaraScratchPadScriptViewModel> GetScriptViewModel(
    UNiagaraSystem* System,
    const FString& ModuleName)
{
    if (!System) return nullptr;
    FNiagaraEditorModule& Module = FNiagaraEditorModule::Get();
    TSharedPtr<FNiagaraSystemViewModel> SystemVM = Module.GetExistingViewModelForSystem(System);
    if (!SystemVM.IsValid()) return nullptr;
    UNiagaraScratchPadViewModel* ScratchVM = SystemVM->GetScriptScratchPadViewModel();
    if (!ScratchVM) return nullptr;
    return ScratchVM->GetViewModelForScript(FName(*ModuleName));
}

/** Resolve a UNiagaraScript from either (system+module) or direct script_path. */
UNiagaraScript* ResolveScript(
    const TSharedPtr<FJsonObject>& Params,
    UNiagaraSystem*& OutSystem,
    FString& OutModuleName,
    FString& OutError)
{
    OutSystem = nullptr;
    OutModuleName.Reset();

    FString SystemPath, ModuleName;
    Params->TryGetStringField(TEXT("system_path"), SystemPath);
    Params->TryGetStringField(TEXT("module_name"), ModuleName);
    if (!SystemPath.IsEmpty() && !ModuleName.IsEmpty())
    {
        OutSystem = NiagaraHelpers::LoadNiagaraSystem(SystemPath, OutError);
        if (!OutSystem) return nullptr;
        OutModuleName = ModuleName;
        UNiagaraScript* Script = NiagaraHelpers::FindScratchPadScript(OutSystem, ModuleName);
        if (!Script)
        {
            OutError = FString::Printf(TEXT("Scratch pad module '%s' not found"), *ModuleName);
        }
        return Script;
    }

    FString ScriptPath;
    if (Params->TryGetStringField(TEXT("script_path"), ScriptPath))
    {
        UNiagaraScript* Script = Cast<UNiagaraScript>(UEditorAssetLibrary::LoadAsset(ScriptPath));
        if (!Script) OutError = FString::Printf(TEXT("Script asset not found: %s"), *ScriptPath);
        return Script;
    }

    OutError = TEXT("Provide either (system_path + module_name) or script_path");
    return nullptr;
}

} // namespace
#endif


// ---------------------------------------------------------------------------
// HandleApplyNiagaraScratchPad — commit edit-copy to original script asset
//   (mirrors the "Apply" button on the scratch pad toolbar)
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleApplyNiagaraScratchPad(
    const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
    FString SystemPath, ModuleName;
    if (!Params->TryGetStringField(TEXT("system_path"), SystemPath) ||
        !Params->TryGetStringField(TEXT("module_name"), ModuleName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            TEXT("Missing 'system_path' or 'module_name'"));
    }
    FString Error;
    UNiagaraSystem* System = NiagaraHelpers::LoadNiagaraSystem(SystemPath, Error);
    if (!System) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);

    TSharedPtr<FNiagaraScratchPadScriptViewModel> ScriptVM =
        GetScriptViewModel(System, ModuleName);
    if (!ScriptVM.IsValid())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(
                TEXT("No open scratch pad view model for '%s' — open the system in the editor"),
                *ModuleName));
    }

    ScriptVM->ApplyChanges();

    auto R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetStringField(TEXT("system_path"), SystemPath);
    R->SetStringField(TEXT("module_name"), ModuleName);
    R->SetStringField(TEXT("action"), TEXT("apply"));
    return R;
#else
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only command"));
#endif
}


// ---------------------------------------------------------------------------
// HandleApplyAndSaveNiagaraScratchPad — apply + save asset to disk
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleApplyAndSaveNiagaraScratchPad(
    const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
    FString SystemPath, ModuleName;
    if (!Params->TryGetStringField(TEXT("system_path"), SystemPath) ||
        !Params->TryGetStringField(TEXT("module_name"), ModuleName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            TEXT("Missing 'system_path' or 'module_name'"));
    }
    FString Error;
    UNiagaraSystem* System = NiagaraHelpers::LoadNiagaraSystem(SystemPath, Error);
    if (!System) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);

    TSharedPtr<FNiagaraScratchPadScriptViewModel> ScriptVM =
        GetScriptViewModel(System, ModuleName);
    if (!ScriptVM.IsValid())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(
                TEXT("No open scratch pad view model for '%s' — open the system in the editor"),
                *ModuleName));
    }

    ScriptVM->ApplyChangesAndSave();

    auto R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetStringField(TEXT("system_path"), SystemPath);
    R->SetStringField(TEXT("module_name"), ModuleName);
    R->SetStringField(TEXT("action"), TEXT("apply_and_save"));
    return R;
#else
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only command"));
#endif
}


// ---------------------------------------------------------------------------
// Phase 3 — Script details panel properties
//
// All UPROPERTYs live on FVersionedNiagaraScriptData, accessed via
// UNiagaraScript::GetLatestScriptData(). Reading and writing via FProperty
// reflection keeps us robust across UE versions.
// ---------------------------------------------------------------------------

#if WITH_EDITORONLY_DATA
namespace
{

/** Read a property from a FVersionedNiagaraScriptData* into a JSON value.
 *
 *  Uses explicit per-FProperty-type reads rather than the generic
 *  NiagaraIntrospection::SerializeProperty walker. The generic walker
 *  crashes deep inside FText's shared-pointer accessor when traversing
 *  nested TArray<FNiagaraModuleDependency> entries — likely because their
 *  internal FText data isn't initialized until the details panel forces a
 *  first access via PostInitProperties. Explicit reads keep us shallow and
 *  only touch the outer FText one-at-a-time.
 *
 *  Supported property shapes:
 *    - bool / int (signed / unsigned) / float / double
 *    - FString / FName / FText
 *    - FEnumProperty
 *    - TArray<FName>  (outputs names only, no nested struct walk)
 *    - TArray<FNiagaraModuleDependency>  (outputs {id, type, constraint} only)
 *    - TMap<FName, FString>  (ScriptMetaData)
 *    - Class reference (ConversionUtility) — path only
 *  Any other type falls through to a schema-only stub (no value). */
void AddScriptPropertyToJson(
    FVersionedNiagaraScriptData* ScriptData,
    UScriptStruct* DataStruct,
    const TCHAR* PropName,
    TSharedPtr<FJsonObject>& Out)
{
    if (!ScriptData || !DataStruct) return;
    FProperty* Prop = DataStruct->FindPropertyByName(FName(PropName));
    if (!Prop) return;
    const void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(ScriptData);

    auto PropObj = MakeShared<FJsonObject>();
    PropObj->SetStringField(TEXT("type"), Prop->GetCPPType());

    auto SetAndReturn = [&](const FString& Kind, TSharedPtr<FJsonValue> Value)
    {
        PropObj->SetStringField(TEXT("kind"), Kind);
        if (Value.IsValid())
        {
            PropObj->SetField(TEXT("value"), Value);
        }
        Out->SetObjectField(PropName, PropObj);
    };

    if (const FBoolProperty* P = CastField<FBoolProperty>(Prop))
    {
        SetAndReturn(TEXT("bool"),
            MakeShared<FJsonValueBoolean>(P->GetPropertyValue(ValuePtr)));
        return;
    }
    if (const FIntProperty* P = CastField<FIntProperty>(Prop))
    {
        SetAndReturn(TEXT("int"),
            MakeShared<FJsonValueNumber>(P->GetPropertyValue(ValuePtr)));
        return;
    }
    if (const FUInt32Property* P = CastField<FUInt32Property>(Prop))
    {
        SetAndReturn(TEXT("int"),
            MakeShared<FJsonValueNumber>((double)P->GetPropertyValue(ValuePtr)));
        return;
    }
    if (const FFloatProperty* P = CastField<FFloatProperty>(Prop))
    {
        SetAndReturn(TEXT("float"),
            MakeShared<FJsonValueNumber>(P->GetPropertyValue(ValuePtr)));
        return;
    }
    if (const FDoubleProperty* P = CastField<FDoubleProperty>(Prop))
    {
        SetAndReturn(TEXT("float"),
            MakeShared<FJsonValueNumber>(P->GetPropertyValue(ValuePtr)));
        return;
    }
    if (const FStrProperty* P = CastField<FStrProperty>(Prop))
    {
        SetAndReturn(TEXT("string"),
            MakeShared<FJsonValueString>(P->GetPropertyValue(ValuePtr)));
        return;
    }
    if (const FNameProperty* P = CastField<FNameProperty>(Prop))
    {
        SetAndReturn(TEXT("name"),
            MakeShared<FJsonValueString>(P->GetPropertyValue(ValuePtr).ToString()));
        return;
    }
    if (const FTextProperty* P = CastField<FTextProperty>(Prop))
    {
        // FText::ToString() on a default-constructed FText returns empty —
        // but we guard anyway in case the editor hasn't initialized it.
        const FText& TextVal = P->GetPropertyValue(ValuePtr);
        SetAndReturn(TEXT("text"),
            MakeShared<FJsonValueString>(TextVal.IsEmpty() ? FString() : TextVal.ToString()));
        return;
    }
    if (const FEnumProperty* P = CastField<FEnumProperty>(Prop))
    {
        const UEnum* Enum = P->GetEnum();
        const int64 Val = P->GetUnderlyingProperty()->GetSignedIntPropertyValue(ValuePtr);
        const FString ValueStr = Enum
            ? Enum->GetNameStringByValue(Val)
            : FString::Printf(TEXT("%lld"), Val);
        auto Arr = MakeShared<FJsonObject>();
        Arr->SetStringField(TEXT("kind"), TEXT("enum"));
        Arr->SetStringField(TEXT("enum"), Enum ? Enum->GetName() : TEXT(""));
        Arr->SetStringField(TEXT("value"), ValueStr);
        Arr->SetNumberField(TEXT("raw"), (double)Val);
        Out->SetObjectField(PropName, Arr);
        return;
    }
    if (const FByteProperty* P = CastField<FByteProperty>(Prop))
    {
        // Byte-backed enum (older UE pattern): still emit name if possible.
        UEnum* Enum = P->Enum;
        const int64 Val = P->GetPropertyValue(ValuePtr);
        auto Arr = MakeShared<FJsonObject>();
        Arr->SetStringField(TEXT("kind"), Enum ? TEXT("enum") : TEXT("byte"));
        if (Enum)
        {
            Arr->SetStringField(TEXT("enum"), Enum->GetName());
            Arr->SetStringField(TEXT("value"), Enum->GetNameStringByValue(Val));
        }
        Arr->SetNumberField(TEXT("raw"), (double)Val);
        Out->SetObjectField(PropName, Arr);
        return;
    }
    if (const FArrayProperty* P = CastField<FArrayProperty>(Prop))
    {
        PropObj->SetStringField(TEXT("kind"), TEXT("array"));
        FScriptArrayHelper Helper(P, ValuePtr);
        const int32 Num = Helper.Num();
        PropObj->SetNumberField(TEXT("count"), Num);

        TArray<TSharedPtr<FJsonValue>> Items;
        const int32 MaxItems = FMath::Min(Num, 16);
        // Specialize: TArray<FName> — emit as list of strings.
        if (CastField<FNameProperty>(P->Inner))
        {
            for (int32 i = 0; i < MaxItems; ++i)
            {
                FName* NamePtr = reinterpret_cast<FName*>(Helper.GetRawPtr(i));
                Items.Add(MakeShared<FJsonValueString>(NamePtr->ToString()));
            }
        }
        // Specialize: TArray<FNiagaraModuleDependency> — emit {id, type, constraint}
        // without touching the inner FText Description (that's what crashed
        // the generic walker).
        else if (const FStructProperty* InnerS = CastField<FStructProperty>(P->Inner))
        {
            FProperty* IdProp   = InnerS->Struct ? InnerS->Struct->FindPropertyByName(TEXT("Id")) : nullptr;
            FProperty* TypeProp = InnerS->Struct ? InnerS->Struct->FindPropertyByName(TEXT("Type")) : nullptr;
            FProperty* ScriptConstraintProp = InnerS->Struct ? InnerS->Struct->FindPropertyByName(TEXT("ScriptConstraint")) : nullptr;
            FProperty* VersionProp = InnerS->Struct ? InnerS->Struct->FindPropertyByName(TEXT("RequiredVersion")) : nullptr;
            for (int32 i = 0; i < MaxItems; ++i)
            {
                const void* ElemPtr = Helper.GetRawPtr(i);
                auto Entry = MakeShared<FJsonObject>();
                if (IdProp)
                {
                    if (FNameProperty* N = CastField<FNameProperty>(IdProp))
                    {
                        Entry->SetStringField(TEXT("id"),
                            N->GetPropertyValue(IdProp->ContainerPtrToValuePtr<void>(ElemPtr)).ToString());
                    }
                }
                if (TypeProp)
                {
                    if (FEnumProperty* E = CastField<FEnumProperty>(TypeProp))
                    {
                        const int64 Val = E->GetUnderlyingProperty()->GetSignedIntPropertyValue(
                            TypeProp->ContainerPtrToValuePtr<void>(ElemPtr));
                        Entry->SetStringField(TEXT("type"),
                            E->GetEnum() ? E->GetEnum()->GetNameStringByValue(Val) : TEXT(""));
                    }
                    else if (FByteProperty* B = CastField<FByteProperty>(TypeProp))
                    {
                        const int64 Val = B->GetPropertyValue(
                            TypeProp->ContainerPtrToValuePtr<void>(ElemPtr));
                        Entry->SetStringField(TEXT("type"),
                            B->Enum ? B->Enum->GetNameStringByValue(Val) : FString::Printf(TEXT("%lld"), Val));
                    }
                }
                if (ScriptConstraintProp)
                {
                    if (FEnumProperty* E = CastField<FEnumProperty>(ScriptConstraintProp))
                    {
                        const int64 Val = E->GetUnderlyingProperty()->GetSignedIntPropertyValue(
                            ScriptConstraintProp->ContainerPtrToValuePtr<void>(ElemPtr));
                        Entry->SetStringField(TEXT("script_constraint"),
                            E->GetEnum() ? E->GetEnum()->GetNameStringByValue(Val) : TEXT(""));
                    }
                }
                if (VersionProp)
                {
                    if (FStrProperty* S = CastField<FStrProperty>(VersionProp))
                    {
                        Entry->SetStringField(TEXT("required_version"),
                            S->GetPropertyValue(VersionProp->ContainerPtrToValuePtr<void>(ElemPtr)));
                    }
                }
                Items.Add(MakeShared<FJsonValueObject>(Entry));
            }
        }
        else
        {
            // Unknown element type — emit length only.
        }
        PropObj->SetArrayField(TEXT("items"), Items);
        if (Num > MaxItems) PropObj->SetBoolField(TEXT("truncated"), true);
        Out->SetObjectField(PropName, PropObj);
        return;
    }
    if (const FMapProperty* P = CastField<FMapProperty>(Prop))
    {
        PropObj->SetStringField(TEXT("kind"), TEXT("map"));
        FScriptMapHelper Helper(P, ValuePtr);
        PropObj->SetNumberField(TEXT("count"), Helper.Num());
        // Only FName->FString pairs are shallow-serialized (that's
        // ScriptMetaData). Other map types emit count only.
        if (CastField<FNameProperty>(P->KeyProp) && CastField<FStrProperty>(P->ValueProp))
        {
            auto Entries = MakeShared<FJsonObject>();
            for (int32 i = 0; i < Helper.Num(); ++i)
            {
                if (!Helper.IsValidIndex(i)) continue;
                const FName Key = *reinterpret_cast<FName*>(Helper.GetKeyPtr(i));
                const FString& Val = *reinterpret_cast<FString*>(Helper.GetValuePtr(i));
                Entries->SetStringField(Key.ToString(), Val);
            }
            PropObj->SetObjectField(TEXT("entries"), Entries);
        }
        Out->SetObjectField(PropName, PropObj);
        return;
    }
    if (const FClassProperty* P = CastField<FClassProperty>(Prop))
    {
        UClass* C = Cast<UClass>(P->GetPropertyValue(ValuePtr));
        PropObj->SetStringField(TEXT("kind"), TEXT("class"));
        PropObj->SetStringField(TEXT("value"), C ? C->GetPathName() : FString());
        Out->SetObjectField(PropName, PropObj);
        return;
    }

    // Fallback — schema only.
    PropObj->SetStringField(TEXT("kind"), TEXT("unsupported"));
    Out->SetObjectField(PropName, PropObj);
}

/** Import a JSON value into a property on FVersionedNiagaraScriptData via
 *  ImportText_Direct — handles any property type uniformly. */
bool WriteScriptProperty(
    FVersionedNiagaraScriptData* ScriptData,
    UScriptStruct* DataStruct,
    const FString& PropName,
    const TSharedPtr<FJsonValue>& Value,
    FString& OutError)
{
    FProperty* Prop = DataStruct->FindPropertyByName(FName(*PropName));
    if (!Prop)
    {
        OutError = FString::Printf(TEXT("Unknown property '%s'"), *PropName);
        return false;
    }
    void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(ScriptData);

    // Scalars: set via direct path when easy.
    if (FBoolProperty* B = CastField<FBoolProperty>(Prop))
    {
        bool V = false;
        if (!Value->TryGetBool(V))
        {
            OutError = FString::Printf(TEXT("'%s' expects bool"), *PropName);
            return false;
        }
        B->SetPropertyValue(ValuePtr, V);
        return true;
    }
    if (FIntProperty* I = CastField<FIntProperty>(Prop))
    {
        int32 V = 0;
        if (!Value->TryGetNumber(V))
        {
            OutError = FString::Printf(TEXT("'%s' expects int"), *PropName);
            return false;
        }
        I->SetPropertyValue(ValuePtr, V);
        return true;
    }
    if (FStrProperty* S = CastField<FStrProperty>(Prop))
    {
        FString V;
        if (!Value->TryGetString(V))
        {
            OutError = FString::Printf(TEXT("'%s' expects string"), *PropName);
            return false;
        }
        S->SetPropertyValue(ValuePtr, V);
        return true;
    }
    if (FNameProperty* N = CastField<FNameProperty>(Prop))
    {
        FString V;
        if (!Value->TryGetString(V))
        {
            OutError = FString::Printf(TEXT("'%s' expects string"), *PropName);
            return false;
        }
        N->SetPropertyValue(ValuePtr, FName(*V));
        return true;
    }
    if (FTextProperty* T = CastField<FTextProperty>(Prop))
    {
        FString V;
        if (!Value->TryGetString(V))
        {
            OutError = FString::Printf(TEXT("'%s' expects string"), *PropName);
            return false;
        }
        T->SetPropertyValue(ValuePtr, FText::FromString(V));
        return true;
    }
    if (FEnumProperty* E = CastField<FEnumProperty>(Prop))
    {
        FString V;
        if (!Value->TryGetString(V))
        {
            OutError = FString::Printf(TEXT("'%s' expects enum string"), *PropName);
            return false;
        }
        UEnum* Enum = E->GetEnum();
        int64 EnumValue = Enum ? Enum->GetValueByNameString(V) : INDEX_NONE;
        if (EnumValue == INDEX_NONE && Enum)
        {
            EnumValue = Enum->GetValueByNameString(Enum->GenerateEnumPrefix() + TEXT("::") + V);
        }
        if (EnumValue == INDEX_NONE)
        {
            OutError = FString::Printf(TEXT("Unknown enum '%s' for '%s'"), *V, *PropName);
            return false;
        }
        E->GetUnderlyingProperty()->SetIntPropertyValue(ValuePtr, EnumValue);
        return true;
    }

    // Specialized: TArray with inner primitive (FName / FString) — accept a
    // JSON array and populate via FScriptArrayHelper. Covers the common
    // ProvidedDependencies (TArray<FName>) case that otherwise chokes
    // ImportText's tuple-format requirement.
    if (const FArrayProperty* AP = CastField<FArrayProperty>(Prop))
    {
        if (Value->Type == EJson::Array)
        {
            const TArray<TSharedPtr<FJsonValue>>& Arr = Value->AsArray();
            FScriptArrayHelper Helper(AP, ValuePtr);
            Helper.EmptyValues();
            Helper.AddValues(Arr.Num());
            for (int32 i = 0; i < Arr.Num(); ++i)
            {
                void* ElemPtr = Helper.GetRawPtr(i);
                if (FNameProperty* NP = CastField<FNameProperty>(AP->Inner))
                {
                    FString S;
                    if (!Arr[i]->TryGetString(S))
                    {
                        OutError = FString::Printf(
                            TEXT("'%s'[%d] expects string element"), *PropName, i);
                        return false;
                    }
                    NP->SetPropertyValue(ElemPtr, FName(*S));
                }
                else if (FStrProperty* SP = CastField<FStrProperty>(AP->Inner))
                {
                    FString S;
                    if (!Arr[i]->TryGetString(S))
                    {
                        OutError = FString::Printf(
                            TEXT("'%s'[%d] expects string element"), *PropName, i);
                        return false;
                    }
                    SP->SetPropertyValue(ElemPtr, S);
                }
                else
                {
                    // Fallback to ImportText on the element (e.g. struct inner).
                    FString Serialized;
                    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
                    FJsonSerializer::Serialize(Arr[i].ToSharedRef(), TEXT(""), Writer);
                    if (!AP->Inner->ImportText_Direct(*Serialized, ElemPtr, nullptr, PPF_None))
                    {
                        OutError = FString::Printf(
                            TEXT("'%s'[%d] failed ImportText from '%s'"),
                            *PropName, i, *Serialized);
                        return false;
                    }
                }
            }
            return true;
        }
    }

    // Fallback: generic ImportText — handles structs, tuple-form arrays, etc.
    FString Serialized;
    if (Value->Type == EJson::String)
    {
        Value->TryGetString(Serialized);
    }
    else
    {
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
        FJsonSerializer::Serialize(Value.ToSharedRef(), TEXT(""), Writer);
    }
    const TCHAR* Imported = Prop->ImportText_Direct(*Serialized, ValuePtr, nullptr, PPF_None);
    if (!Imported)
    {
        OutError = FString::Printf(TEXT("Failed to import '%s' from value '%s'"),
            *PropName, *Serialized);
        return false;
    }
    return true;
}

} // namespace
#endif


// ---------------------------------------------------------------------------
// HandleGetNiagaraScriptProperties — read the full details panel
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleGetNiagaraScriptProperties(
    const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
    FString Error;
    UNiagaraSystem* System = nullptr;
    FString ModuleName;
    UNiagaraScript* Script = ResolveScript(Params, System, ModuleName, Error);
    if (!Script) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);

    FVersionedNiagaraScriptData* Data = Script->GetLatestScriptData();
    if (!Data)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            TEXT("Script has no latest data"));
    }

    UScriptStruct* DataStruct = FVersionedNiagaraScriptData::StaticStruct();
    TSharedPtr<FJsonObject> Props = MakeShared<FJsonObject>();

    // Every property the details panel exposes — mirrors screenshots 3-5.
    static const TCHAR* PropNames[] = {
        TEXT("Category"),
        TEXT("Description"),
        TEXT("Keywords"),
        TEXT("ModuleUsageBitmask"),
        TEXT("ProvidedDependencies"),
        TEXT("RequiredDependencies"),
        TEXT("LibraryVisibility"),
        TEXT("bDeprecated"),
        TEXT("DeprecationMessage"),
        TEXT("bExperimental"),
        TEXT("ExperimentalMessage"),
        TEXT("NumericOutputTypeSelectionMode"),
        TEXT("ScriptMetaData"),
        TEXT("ConversionUtility"),
    };
    for (const TCHAR* P : PropNames)
    {
        AddScriptPropertyToJson(Data, DataStruct, P, Props);
    }

    auto R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetStringField(TEXT("script_path"), Script->GetPathName());
    R->SetObjectField(TEXT("properties"), Props);
    return R;
#else
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only command"));
#endif
}


// ---------------------------------------------------------------------------
// HandleSetNiagaraScriptProperties — batch-set any subset of the above
// Params: properties = { "Category": "My/Category", "bDeprecated": true, ... }
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleSetNiagaraScriptProperties(
    const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
    FString Error;
    UNiagaraSystem* System = nullptr;
    FString ModuleName;
    UNiagaraScript* Script = ResolveScript(Params, System, ModuleName, Error);
    if (!Script) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);

    const TSharedPtr<FJsonObject>* PropsObj = nullptr;
    if (!Params->TryGetObjectField(TEXT("properties"), PropsObj) || !PropsObj)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            TEXT("Missing 'properties' object"));
    }

    FVersionedNiagaraScriptData* Data = Script->GetLatestScriptData();
    if (!Data)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            TEXT("Script has no latest data"));
    }
    UScriptStruct* DataStruct = FVersionedNiagaraScriptData::StaticStruct();

    Script->Modify();

    TArray<TSharedPtr<FJsonValue>> Applied, Failed;
    for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*PropsObj)->Values)
    {
        FString WriteErr;
        if (WriteScriptProperty(Data, DataStruct, Pair.Key, Pair.Value, WriteErr))
        {
            Applied.Add(MakeShared<FJsonValueString>(Pair.Key));
        }
        else
        {
            auto F = MakeShared<FJsonObject>();
            F->SetStringField(TEXT("name"), Pair.Key);
            F->SetStringField(TEXT("error"), WriteErr);
            Failed.Add(MakeShared<FJsonValueObject>(F));
        }
    }

    Script->MarkPackageDirty();
    Script->PostEditChange();

    // If this is a scratch pad script, sync the editor view model so the
    // details panel updates without requiring close/reopen.
    if (System && !ModuleName.IsEmpty())
    {
        NiagaraHelpers::NotifyScratchPadScriptChanged(System, ModuleName);
    }

    auto R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), Failed.Num() == 0);
    R->SetStringField(TEXT("script_path"), Script->GetPathName());
    R->SetNumberField(TEXT("applied"), Applied.Num());
    R->SetArrayField(TEXT("applied_properties"), Applied);
    if (Failed.Num() > 0)
    {
        R->SetArrayField(TEXT("failed_properties"), Failed);
    }
    return R;
#else
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only command"));
#endif
}
