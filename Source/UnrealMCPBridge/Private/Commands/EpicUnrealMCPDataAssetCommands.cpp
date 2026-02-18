#include "Commands/EpicUnrealMCPDataAssetCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "Commands/EpicUnrealMCPPropertyUtils.h"

#include "Engine/DataAsset.h"
#include "Factories/DataAssetFactory.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectIterator.h"

// Convenience alias so callers don't repeat the long prefix everywhere
using PU = FEpicUnrealMCPPropertyUtils;

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
// Limited to UDataAsset subclasses. For any class use PU::ResolveAnyClass.
// ===========================================================================
UClass* FEpicUnrealMCPDataAssetCommands::ResolveDataAssetClass(const FString& ClassName)
{
    if (ClassName.IsEmpty()) return nullptr;

    if (ClassName.Contains(TEXT(".")))
    {
        UClass* C = FindObject<UClass>(nullptr, *ClassName);
        if (C) return C;
    }

    TArray<UClass*> Derived;
    GetDerivedClasses(UDataAsset::StaticClass(), Derived, true);
    for (UClass* C : Derived)
    {
        if (!C) continue;
        if (C->GetName() == ClassName ||
            C->GetName() == (TEXT("U") + ClassName) ||
            C->GetName() == ClassName.RightChop(1))
            return C;
    }
    return nullptr;
}

// ===========================================================================
// get_property_valid_types
// Returns the exact set of classes/structs/enum-values the editor dropdown
// would show for a given property slot on any UClass.
//
// Optional "filter" param (case-insensitive substring) trims valid_types
// so the response stays small even for large dropdowns (e.g. 216 fragments).
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

    FString Filter;
    Params->TryGetStringField(TEXT("filter"), Filter);
    const FString FilterLower = Filter.ToLower();

    // ------------------------------------------------------------------
    // 1. Resolve the starting UStruct
    // ------------------------------------------------------------------
    UStruct* CurrentStruct = PU::ResolveAnyClass(ClassName);
    if (!CurrentStruct)
    {
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

        if (i < Parts.Num() - 1)
        {
            FProperty* Inner = TargetProp;
            if (FArrayProperty* ArrProp = CastField<FArrayProperty>(TargetProp))
                Inner = ArrProp->Inner;

            if (FStructProperty* SP = CastField<FStructProperty>(Inner))
                CurrentStruct = SP->Struct;
            else if (FObjectProperty* OP = CastField<FObjectProperty>(Inner))
                CurrentStruct = OP->PropertyClass;
            else
                return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
                    FString::Printf(TEXT("Cannot navigate into '%s' (not struct/object)"), *Parts[i]));
        }
    }
    if (!TargetProp)
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Property path '%s' resolved to null"), *PropertyPath));

    // ------------------------------------------------------------------
    // 3. Unwrap outer array
    // ------------------------------------------------------------------
    FProperty* ElementProp = TargetProp;
    bool bIsArray = false;
    if (FArrayProperty* ArrProp = CastField<FArrayProperty>(TargetProp))
    {
        ElementProp = ArrProp->Inner;
        bIsArray = true;
    }

    // ------------------------------------------------------------------
    // Helpers
    // ------------------------------------------------------------------
    auto MakeClassEntry = [](UClass* C) -> TSharedPtr<FJsonObject>
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetStringField(TEXT("name"),        C->GetName());
        Obj->SetStringField(TEXT("path"),        C->GetPathName());
        Obj->SetStringField(TEXT("parent"),      C->GetSuperClass() ? C->GetSuperClass()->GetName() : TEXT("None"));
        Obj->SetBoolField(TEXT("is_abstract"),   C->HasAnyClassFlags(CLASS_Abstract));
        Obj->SetBoolField(TEXT("is_deprecated"), C->HasAnyClassFlags(CLASS_Deprecated));
        return Obj;
    };

    auto MakeStructEntry = [](UScriptStruct* S) -> TSharedPtr<FJsonObject>
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetStringField(TEXT("name"),   S->GetName());
        Obj->SetStringField(TEXT("path"),   S->GetPathName());
        Obj->SetStringField(TEXT("parent"), S->GetSuperStruct() ? S->GetSuperStruct()->GetName() : TEXT("None"));
        return Obj;
    };

    // Filter: name or path must contain the filter string (case-insensitive)
    auto PassesFilter = [&](const FString& Name, const FString& Path) -> bool
    {
        if (FilterLower.IsEmpty()) return true;
        return Name.ToLower().Contains(FilterLower) || Path.ToLower().Contains(FilterLower);
    };

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("class_name"),    ClassName);
    Result->SetStringField(TEXT("property_path"), PropertyPath);
    Result->SetBoolField(TEXT("is_array"),         bIsArray);
    if (!FilterLower.IsEmpty())
        Result->SetStringField(TEXT("filter"), Filter);

    // ------------------------------------------------------------------
    // 4a. FClassProperty (TSubclassOf<T>)
    // ------------------------------------------------------------------
    if (FClassProperty* ClassProp = CastField<FClassProperty>(ElementProp))
    {
        Result->SetStringField(TEXT("kind"),       TEXT("subclass"));
        Result->SetStringField(TEXT("base_class"), ClassProp->MetaClass ? ClassProp->MetaClass->GetName() : TEXT("UObject"));
        TArray<UClass*> Derived;
        if (ClassProp->MetaClass) GetDerivedClasses(ClassProp->MetaClass, Derived, true);
        TArray<TSharedPtr<FJsonValue>> Items;
        for (UClass* C : Derived)
        {
            if (!C || (!bIncludeAbstract && C->HasAnyClassFlags(CLASS_Abstract)) || C->HasAnyClassFlags(CLASS_Deprecated)) continue;
            if (!PassesFilter(C->GetName(), C->GetPathName())) continue;
            Items.Add(MakeShared<FJsonValueObject>(MakeClassEntry(C)));
        }
        Result->SetNumberField(TEXT("count"), Items.Num());
        Result->SetArrayField(TEXT("valid_types"), Items);
        return Result;
    }

    // ------------------------------------------------------------------
    // 4b. FObjectProperty (instanced or plain)
    // ------------------------------------------------------------------
    if (FObjectProperty* ObjProp = CastField<FObjectProperty>(ElementProp))
    {
        const bool bInstanced = ObjProp->HasAnyPropertyFlags(CPF_PersistentInstance | CPF_InstancedReference);
        Result->SetStringField(TEXT("kind"),       bInstanced ? TEXT("instanced_object") : TEXT("object_reference"));
        Result->SetStringField(TEXT("base_class"), ObjProp->PropertyClass ? ObjProp->PropertyClass->GetName() : TEXT("UObject"));
        TArray<UClass*> Derived;
        if (ObjProp->PropertyClass) GetDerivedClasses(ObjProp->PropertyClass, Derived, true);
        if (ObjProp->PropertyClass && !ObjProp->PropertyClass->HasAnyClassFlags(CLASS_Abstract))
            Derived.AddUnique(ObjProp->PropertyClass);
        TArray<TSharedPtr<FJsonValue>> Items;
        for (UClass* C : Derived)
        {
            if (!C || (!bIncludeAbstract && C->HasAnyClassFlags(CLASS_Abstract)) || C->HasAnyClassFlags(CLASS_Deprecated)) continue;
            if (!PassesFilter(C->GetName(), C->GetPathName())) continue;
            Items.Add(MakeShared<FJsonValueObject>(MakeClassEntry(C)));
        }
        Result->SetNumberField(TEXT("count"), Items.Num());
        Result->SetArrayField(TEXT("valid_types"), Items);
        return Result;
    }

    // ------------------------------------------------------------------
    // 4c. FStructProperty where Struct == FInstancedStruct
    // ------------------------------------------------------------------
    if (FStructProperty* StructProp = CastField<FStructProperty>(ElementProp))
    {
        if (StructProp->Struct && StructProp->Struct->GetName() == TEXT("InstancedStruct"))
        {
            FString BaseStructMeta = TargetProp->GetMetaData(TEXT("BaseStruct"));
            const bool bExcludeBase = TargetProp->HasMetaData(TEXT("ExcludeBaseStruct"));

            UScriptStruct* BaseStruct = nullptr;
            if (!BaseStructMeta.IsEmpty())
            {
                BaseStruct = FindObject<UScriptStruct>(nullptr, *BaseStructMeta);
                if (!BaseStruct)
                {
                    for (TObjectIterator<UScriptStruct> It; It; ++It)
                    {
                        const FString N = (*It)->GetName();
                        if (N == BaseStructMeta || N == (TEXT("F") + BaseStructMeta) ||
                            (*It)->GetPathName() == BaseStructMeta)
                        {
                            BaseStruct = *It; break;
                        }
                    }
                }
            }

            Result->SetStringField(TEXT("kind"),        TEXT("instanced_struct"));
            Result->SetStringField(TEXT("base_struct"),  BaseStruct ? BaseStruct->GetName() : TEXT("(any)"));

            TArray<TSharedPtr<FJsonValue>> Items;
            for (TObjectIterator<UScriptStruct> It; It; ++It)
            {
                UScriptStruct* S = *It;
                if (!S || S->GetOutermost() == GetTransientPackage()) continue;
                if (BaseStruct && !S->IsChildOf(BaseStruct)) continue;
                if (bExcludeBase && S == BaseStruct) continue;
                if (!PassesFilter(S->GetName(), S->GetPathName())) continue;
                Items.Add(MakeShared<FJsonValueObject>(MakeStructEntry(S)));
            }
            Result->SetNumberField(TEXT("count"), Items.Num());
            Result->SetArrayField(TEXT("valid_types"), Items);
            return Result;
        }

        Result->SetStringField(TEXT("kind"),       TEXT("struct"));
        Result->SetStringField(TEXT("struct_type"), StructProp->Struct ? StructProp->Struct->GetName() : TEXT("unknown"));
        Result->SetNumberField(TEXT("count"), 0);
        Result->SetArrayField(TEXT("valid_types"), TArray<TSharedPtr<FJsonValue>>());
        return Result;
    }

    // ------------------------------------------------------------------
    // 4d. Enum
    // ------------------------------------------------------------------
    auto SerialiseEnum = [&](UEnum* Enum) -> TSharedPtr<FJsonObject>
    {
        TArray<TSharedPtr<FJsonValue>> Items;
        for (int32 i = 0; i < Enum->NumEnums() - 1; ++i)
        {
            const FString EntryName = Enum->GetNameStringByIndex(i);
            if (!PassesFilter(EntryName, TEXT(""))) continue;
            TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
            Obj->SetStringField(TEXT("name"),         EntryName);
            Obj->SetStringField(TEXT("display_name"), Enum->GetDisplayNameTextByIndex(i).ToString());
            Obj->SetNumberField(TEXT("value"),        static_cast<double>(Enum->GetValueByIndex(i)));
            Items.Add(MakeShared<FJsonValueObject>(Obj));
        }
        Result->SetStringField(TEXT("kind"),      TEXT("enum"));
        Result->SetStringField(TEXT("enum_name"),  Enum->GetName());
        Result->SetNumberField(TEXT("count"),      Items.Num());
        Result->SetArrayField(TEXT("valid_types"), Items);
        return Result;
    };

    if (FEnumProperty* EnumProp = CastField<FEnumProperty>(ElementProp))
        return SerialiseEnum(EnumProp->GetEnum());
    if (FByteProperty* ByteProp = CastField<FByteProperty>(ElementProp))
        if (UEnum* Enum = ByteProp->GetIntPropertyEnum())
            return SerialiseEnum(Enum);

    // ------------------------------------------------------------------
    // 4e. Primitive / unsupported
    // ------------------------------------------------------------------
    Result->SetStringField(TEXT("kind"),  FString::Printf(TEXT("primitive_%s"), *ElementProp->GetClass()->GetName()));
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
    FString Filter;
    Params->TryGetStringField(TEXT("filter"), Filter);
    const FString FilterLower = Filter.ToLower();

    bool bIncludeAbstract = false;
    Params->TryGetBoolField(TEXT("include_abstract"), bIncludeAbstract);

    TArray<UClass*> Derived;
    GetDerivedClasses(UDataAsset::StaticClass(), Derived, true);

    TArray<TSharedPtr<FJsonValue>> ClassArray;
    for (UClass* C : Derived)
    {
        if (!C || (!bIncludeAbstract && C->HasAnyClassFlags(CLASS_Abstract))) continue;
        const FString Name = C->GetName();
        if (!FilterLower.IsEmpty() && !Name.ToLower().Contains(FilterLower)) continue;

        int32 PropCount = 0;
        for (TFieldIterator<FProperty> It(C, EFieldIteratorFlags::IncludeSuper); It; ++It)
            if ((*It)->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible)) ++PropCount;

        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetStringField(TEXT("name"),           Name);
        Obj->SetStringField(TEXT("path"),           C->GetPathName());
        Obj->SetStringField(TEXT("parent_class"),   C->GetSuperClass() ? C->GetSuperClass()->GetName() : TEXT("None"));
        Obj->SetBoolField(TEXT("is_abstract"),      C->HasAnyClassFlags(CLASS_Abstract));
        Obj->SetBoolField(TEXT("is_primary"),       C->IsChildOf(UPrimaryDataAsset::StaticClass()));
        Obj->SetNumberField(TEXT("property_count"), PropCount);
        ClassArray.Add(MakeShared<FJsonValueObject>(Obj));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetNumberField(TEXT("count"),  ClassArray.Num());
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

    UClass* AssetClass = ResolveDataAssetClass(ClassName);
    if (!AssetClass)
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Data asset class not found: '%s'"), *ClassName));
    if (!AssetClass->IsChildOf(UDataAsset::StaticClass()))
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Class '%s' is not a UDataAsset subclass"), *ClassName));

    FString PackagePath, AssetName;
    int32 LastSlash = INDEX_NONE;
    if (AssetPath.FindLastChar('/', LastSlash) && LastSlash != INDEX_NONE)
    {
        PackagePath = AssetPath.Left(LastSlash);
        AssetName   = AssetPath.Mid(LastSlash + 1);
    }
    else { AssetName = AssetPath; PackagePath = TEXT("/Game"); }

    const FString FullPath = PackagePath + TEXT("/") + AssetName;
    if (UEditorAssetLibrary::DoesAssetExist(FullPath))
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Asset already exists: %s"), *FullPath));

    UDataAssetFactory* Factory = NewObject<UDataAssetFactory>();
    Factory->DataAssetClass = AssetClass;

    IAssetTools& AssetTools =
        FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();

    UObject* NewAsset = AssetTools.CreateAsset(AssetName, PackagePath, AssetClass, Factory);
    if (!NewAsset)
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Failed to create data asset '%s'"), *FullPath));

    const TSharedPtr<FJsonObject>* InitialProps = nullptr;
    if (Params->TryGetObjectField(TEXT("initial_properties"), InitialProps) && InitialProps)
    {
        TArray<FString> FailedProps;
        for (const auto& Pair : (*InitialProps)->Values)
        {
            FString Err;
            if (!PU::SetProperty(NewAsset, Pair.Key, Pair.Value, Err))
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
            for (const FString& W : FailedProps) WarnArr.Add(MakeShared<FJsonValueString>(W));
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
            FString::Printf(TEXT("Asset '%s' is not a UDataAsset"), *AssetPath));

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"),       true);
    Result->SetStringField(TEXT("asset_path"),  AssetPath);
    Result->SetStringField(TEXT("class"),       Asset->GetClass()->GetName());
    Result->SetStringField(TEXT("parent_class"), Asset->GetClass()->GetSuperClass()
                                                  ? Asset->GetClass()->GetSuperClass()->GetName()
                                                  : TEXT("None"));
    Result->SetObjectField(TEXT("properties"),
        PU::SerializeAllProperties(Asset, Filter.ToLower(), bIncludeInherited));
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
    if (!PU::SetProperty(Asset, PropertyName, PropertyValue, Err))
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Err);

    Asset->MarkPackageDirty();
    UEditorAssetLibrary::SaveAsset(AssetPath);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"),         true);
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
        if (PU::SetProperty(Asset, Pair.Key, Pair.Value, Err))
            Set.Add(Pair.Key);
        else
            Failed.Add(FString::Printf(TEXT("%s: %s"), *Pair.Key, *Err));
    }

    Asset->MarkPackageDirty();
    UEditorAssetLibrary::SaveAsset(AssetPath);

    TArray<TSharedPtr<FJsonValue>> SetArr, FailArr;
    for (const FString& S : Set)    SetArr.Add(MakeShared<FJsonValueString>(S));
    for (const FString& F : Failed) FailArr.Add(MakeShared<FJsonValueString>(F));

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"),        Failed.IsEmpty());
    Result->SetStringField(TEXT("asset_path"),   AssetPath);
    Result->SetNumberField(TEXT("set_count"),    Set.Num());
    Result->SetNumberField(TEXT("failed_count"), Failed.Num());
    Result->SetArrayField(TEXT("set"),           SetArr);
    if (!FailArr.IsEmpty()) Result->SetArrayField(TEXT("failed"), FailArr);
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

    FARFilter Filter;
    Filter.PackagePaths.Add(FName(*SearchPath));
    Filter.bRecursivePaths   = bRecursive;
    Filter.bRecursiveClasses = true;

    if (!ClassFilter.IsEmpty())
    {
        UClass* FilterClass = ResolveDataAssetClass(ClassFilter);
        if (FilterClass)
            Filter.ClassPaths.Add(FilterClass->GetClassPathName());
        else
            Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Engine"), FName(*ClassFilter)));
    }
    else
    {
        Filter.ClassPaths.Add(UDataAsset::StaticClass()->GetClassPathName());
    }

    TArray<FAssetData> Assets;
    Registry.GetAssets(Filter, Assets);
    if (Assets.Num() > MaxResults) Assets.SetNum(MaxResults);

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
