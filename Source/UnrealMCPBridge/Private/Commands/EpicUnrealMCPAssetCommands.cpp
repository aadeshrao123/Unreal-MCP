#include "Commands/EpicUnrealMCPAssetCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "EditorAssetLibrary.h"
#include "EditorUtilityLibrary.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AssetImportTask.h"
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "FileHelpers.h"
#include "UObject/Field.h"
#include "JsonObjectConverter.h"
#include "Factories/Factory.h"

// ---------------------------------------------------------------------------
// Helper: Find the best UFactory for a given filename by extension.
// Setting an explicit factory on UAssetImportTask bypasses the Interchange
// async pipeline, which crashes when called from the game-thread task graph
// (re-entrant ProcessTasksUntilIdle).
// ---------------------------------------------------------------------------
static UFactory* FindFactoryForFile(const FString& Filename)
{
    const FString Extension = FPaths::GetExtension(Filename).ToLower();
    UFactory* BestFactory = nullptr;
    int32 BestPriority = -1;

    for (TObjectIterator<UClass> It; It; ++It)
    {
        if (!It->IsChildOf(UFactory::StaticClass()) || It->HasAnyClassFlags(CLASS_Abstract))
        {
            continue;
        }

        UFactory* TestFactory = It->GetDefaultObject<UFactory>();
        if (!TestFactory || TestFactory->bEditorImport == false)
        {
            continue;
        }

        // Check if this factory handles our extension
        bool bSupportsExtension = false;
        TArray<FString> Formats;
        TestFactory->GetSupportedFileExtensions(Formats);
        for (const FString& Fmt : Formats)
        {
            if (Fmt.Equals(Extension, ESearchCase::IgnoreCase))
            {
                bSupportsExtension = true;
                break;
            }
        }

        if (bSupportsExtension && TestFactory->FactoryCanImport(Filename))
        {
            if (TestFactory->ImportPriority > BestPriority)
            {
                BestPriority = TestFactory->ImportPriority;
                BestFactory = NewObject<UFactory>(GetTransientPackage(), *It);
                BestFactory->AddToRoot(); // prevent GC during import
            }
        }
    }

    return BestFactory;
}

FEpicUnrealMCPAssetCommands::FEpicUnrealMCPAssetCommands()
{
}

// ---------------------------------------------------------------------------
// Dispatch
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FEpicUnrealMCPAssetCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("find_assets"))          return HandleFindAssets(Params);
    if (CommandType == TEXT("list_assets"))          return HandleListAssets(Params);
    if (CommandType == TEXT("open_asset"))           return HandleOpenAsset(Params);
    if (CommandType == TEXT("get_asset_info"))       return HandleGetAssetInfo(Params);
    if (CommandType == TEXT("get_asset_properties")) return HandleGetAssetProperties(Params);
    if (CommandType == TEXT("set_asset_property"))   return HandleSetAssetProperty(Params);
    if (CommandType == TEXT("find_references"))      return HandleFindReferences(Params);
    if (CommandType == TEXT("duplicate_asset"))      return HandleDuplicateAsset(Params);
    if (CommandType == TEXT("rename_asset"))         return HandleRenameAsset(Params);
    if (CommandType == TEXT("delete_asset"))         return HandleDeleteAsset(Params);
    if (CommandType == TEXT("save_asset"))           return HandleSaveAsset(Params);
    if (CommandType == TEXT("save_all"))             return HandleSaveAll(Params);
    if (CommandType == TEXT("import_asset"))           return HandleImportAsset(Params);
    if (CommandType == TEXT("import_assets_batch"))   return HandleImportAssetsBatch(Params);
    if (CommandType == TEXT("get_selected_assets"))   return HandleGetSelectedAssets(Params);
    if (CommandType == TEXT("sync_browser"))          return HandleSyncBrowser(Params);

    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
        FString::Printf(TEXT("Unknown asset command: %s"), *CommandType));
}

// ---------------------------------------------------------------------------
// Helper: shortcut / "Package.Class" resolution
// ---------------------------------------------------------------------------
bool FEpicUnrealMCPAssetCommands::ResolveClassPath(const FString& ClassType,
                                                     FString& OutPackagePath,
                                                     FString& OutClassName)
{
    static const TMap<FString, TPair<FString, FString>> Shortcuts =
    {
        { TEXT("material"),          { TEXT("/Script/Engine"),    TEXT("Material") }},
        { TEXT("material_instance"), { TEXT("/Script/Engine"),    TEXT("MaterialInstanceConstant") }},
        { TEXT("static_mesh"),       { TEXT("/Script/Engine"),    TEXT("StaticMesh") }},
        { TEXT("skeletal_mesh"),     { TEXT("/Script/Engine"),    TEXT("SkeletalMesh") }},
        { TEXT("texture"),           { TEXT("/Script/Engine"),    TEXT("Texture2D") }},
        { TEXT("blueprint"),         { TEXT("/Script/Engine"),    TEXT("Blueprint") }},
        { TEXT("widget_blueprint"),  { TEXT("/Script/UMG"),       TEXT("WidgetBlueprint") }},
        { TEXT("data_table"),        { TEXT("/Script/Engine"),    TEXT("DataTable") }},
        { TEXT("sound_wave"),        { TEXT("/Script/Engine"),    TEXT("SoundWave") }},
        { TEXT("sound_cue"),         { TEXT("/Script/Engine"),    TEXT("SoundCue") }},
        { TEXT("particle_system"),   { TEXT("/Script/Engine"),    TEXT("ParticleSystem") }},
        { TEXT("niagara_system"),    { TEXT("/Script/Niagara"),   TEXT("NiagaraSystem") }},
        { TEXT("niagara_emitter"),   { TEXT("/Script/Niagara"),   TEXT("NiagaraEmitter") }},
        { TEXT("anim_blueprint"),    { TEXT("/Script/Engine"),    TEXT("AnimBlueprint") }},
        { TEXT("anim_sequence"),     { TEXT("/Script/Engine"),    TEXT("AnimSequence") }},
        { TEXT("anim_montage"),      { TEXT("/Script/Engine"),    TEXT("AnimMontage") }},
        { TEXT("level"),             { TEXT("/Script/Engine"),    TEXT("World") }},
        { TEXT("curve_float"),       { TEXT("/Script/Engine"),    TEXT("CurveFloat") }},
        { TEXT("enum"),              { TEXT("/Script/Engine"),    TEXT("UserDefinedEnum") }},
        { TEXT("struct"),            { TEXT("/Script/Engine"),    TEXT("UserDefinedStruct") }},
    };

    const FString Lower = ClassType.ToLower();
    const TPair<FString, FString>* Found = Shortcuts.Find(Lower);
    if (Found)
    {
        OutPackagePath = Found->Key;
        OutClassName   = Found->Value;
        return true;
    }

    // "PackagePath.ClassName" format
    int32 DotIdx = INDEX_NONE;
    if (ClassType.FindLastChar('.', DotIdx) && DotIdx > 0)
    {
        OutPackagePath = ClassType.Left(DotIdx);
        OutClassName   = ClassType.Mid(DotIdx + 1);
        return true;
    }

    return false;
}

// ---------------------------------------------------------------------------
// find_assets
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FEpicUnrealMCPAssetCommands::HandleFindAssets(const TSharedPtr<FJsonObject>& Params)
{
    FString ClassType, SearchPath, NamePattern;
    bool bRecursive = true;
    double MaxResultsD = 200.0;
    int32 MaxResults = 200;

    Params->TryGetStringField(TEXT("class_type"),    ClassType);
    Params->TryGetStringField(TEXT("path"),          SearchPath);
    Params->TryGetStringField(TEXT("name_pattern"),  NamePattern);
    Params->TryGetBoolField(TEXT("recursive"),        bRecursive);
    if (Params->TryGetNumberField(TEXT("max_results"), MaxResultsD))
        MaxResults = (int32)MaxResultsD;

    IAssetRegistry& AssetRegistry =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

    TArray<FAssetData> AllAssets;

    FString PackagePath, ClassName;
    const bool bHasClass = !ClassType.IsEmpty() && ResolveClassPath(ClassType, PackagePath, ClassName);

    if (bHasClass)
    {
        // Use string-path constructor to avoid "most vexing parse"
        const FString FullClassPath = PackagePath + TEXT(".") + ClassName;
        const FTopLevelAssetPath ClassPath(*FullClassPath);
        AssetRegistry.GetAssetsByClass(ClassPath, AllAssets, /*bSearchSubClasses=*/true);

        // Optional path filter
        if (!SearchPath.IsEmpty())
        {
            AllAssets.RemoveAll([&SearchPath](const FAssetData& AD)
            {
                return !AD.PackageName.ToString().StartsWith(SearchPath);
            });
        }
    }
    else if (!SearchPath.IsEmpty())
    {
        FARFilter Filter;
        Filter.PackagePaths.Add(FName(*SearchPath));
        Filter.bRecursivePaths = bRecursive;
        AssetRegistry.GetAssets(Filter, AllAssets);
    }
    // No class and no path → return empty

    // Name pattern filter
    if (!NamePattern.IsEmpty())
    {
        const FString LowerPattern = NamePattern.ToLower();
        AllAssets.RemoveAll([&LowerPattern](const FAssetData& AD)
        {
            return !AD.AssetName.ToString().ToLower().Contains(LowerPattern);
        });
    }

    // Cap results
    if (AllAssets.Num() > MaxResults)
        AllAssets.SetNum(MaxResults);

    TArray<TSharedPtr<FJsonValue>> AssetArray;
    for (const FAssetData& AD : AllAssets)
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetStringField(TEXT("name"),  AD.AssetName.ToString());
        Obj->SetStringField(TEXT("path"),  AD.GetObjectPathString());
        Obj->SetStringField(TEXT("class"), AD.AssetClassPath.GetAssetName().ToString());
        AssetArray.Add(MakeShared<FJsonValueObject>(Obj));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetNumberField(TEXT("count"),  AllAssets.Num());
    Result->SetArrayField(TEXT("assets"), AssetArray);
    return Result;
}

// ---------------------------------------------------------------------------
// list_assets
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FEpicUnrealMCPAssetCommands::HandleListAssets(const TSharedPtr<FJsonObject>& Params)
{
    FString Path = TEXT("/Game");
    bool bRecursive = true;
    Params->TryGetStringField(TEXT("path"),      Path);
    Params->TryGetBoolField(TEXT("recursive"),   bRecursive);

    FString ClassFilter;
    if (Params->TryGetStringField(TEXT("class_filter"), ClassFilter) && !ClassFilter.IsEmpty())
    {
        TSharedPtr<FJsonObject> FindParams = MakeShared<FJsonObject>();
        FindParams->SetStringField(TEXT("class_type"), ClassFilter);
        FindParams->SetStringField(TEXT("path"),       Path);
        FindParams->SetBoolField(TEXT("recursive"),    bRecursive);
        return HandleFindAssets(FindParams);
    }

    TArray<FString> AssetPaths = UEditorAssetLibrary::ListAssets(Path, bRecursive, false);

    TArray<TSharedPtr<FJsonValue>> AssetArray;
    for (const FString& AssetPath : AssetPaths)
        AssetArray.Add(MakeShared<FJsonValueString>(AssetPath));

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetNumberField(TEXT("count"),  AssetPaths.Num());
    Result->SetArrayField(TEXT("assets"), AssetArray);
    return Result;
}

// ---------------------------------------------------------------------------
// open_asset
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FEpicUnrealMCPAssetCommands::HandleOpenAsset(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));

    UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
    if (!Asset)
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Asset not found: %s"), *AssetPath));

    UAssetEditorSubsystem* EditorSub = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
    if (!EditorSub)
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("AssetEditorSubsystem not available"));

    TArray<UObject*> Assets = { Asset };
    EditorSub->OpenEditorForAssets(Assets);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Opened %s"), *AssetPath));
    return Result;
}

// ---------------------------------------------------------------------------
// get_asset_info
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FEpicUnrealMCPAssetCommands::HandleGetAssetInfo(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));

    UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
    if (!Asset)
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Asset not found: %s"), *AssetPath));

    TSharedPtr<FJsonObject> Info = MakeShared<FJsonObject>();
    Info->SetStringField(TEXT("name"),    Asset->GetName());
    Info->SetStringField(TEXT("class"),   Asset->GetClass()->GetName());
    Info->SetStringField(TEXT("path"),    Asset->GetPathName());
    Info->SetStringField(TEXT("package"), Asset->GetOutermost()->GetName());

    // Collect up to 50 editable properties
    TSharedPtr<FJsonObject> Props = MakeShared<FJsonObject>();
    int32 PropCount = 0;
    for (TFieldIterator<FProperty> PropIt(Asset->GetClass()); PropIt; ++PropIt)
    {
        FProperty* Prop = *PropIt;
        if (!Prop->HasAnyPropertyFlags(CPF_Edit)) continue;

        const void* PropAddr = Prop->ContainerPtrToValuePtr<void>(Asset);
        TSharedPtr<FJsonValue> JsonValue = FJsonObjectConverter::UPropertyToJsonValue(Prop, PropAddr);
        if (JsonValue.IsValid())
            Props->SetField(Prop->GetName(), JsonValue);

        if (++PropCount >= 50) break;
    }
    Info->SetObjectField(TEXT("properties"), Props);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetObjectField(TEXT("info"), Info);
    return Result;
}

// ---------------------------------------------------------------------------
// get_asset_properties
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FEpicUnrealMCPAssetCommands::HandleGetAssetProperties(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));

    UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
    if (!Asset)
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Asset not found: %s"), *AssetPath));

    TSharedPtr<FJsonObject> Props = MakeShared<FJsonObject>();
    for (TFieldIterator<FProperty> PropIt(Asset->GetClass()); PropIt; ++PropIt)
    {
        FProperty* Prop = *PropIt;
        if (!Prop->HasAnyPropertyFlags(CPF_Edit)) continue;

        const void* PropAddr = Prop->ContainerPtrToValuePtr<void>(Asset);
        TSharedPtr<FJsonValue> JsonValue = FJsonObjectConverter::UPropertyToJsonValue(Prop, PropAddr);
        if (JsonValue.IsValid())
            Props->SetField(Prop->GetName(), JsonValue);
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("class"),      Asset->GetClass()->GetName());
    Result->SetStringField(TEXT("path"),       AssetPath);
    Result->SetObjectField(TEXT("properties"), Props);
    return Result;
}

// ---------------------------------------------------------------------------
// set_asset_property
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FEpicUnrealMCPAssetCommands::HandleSetAssetProperty(const TSharedPtr<FJsonObject>& Params)
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

    FString ErrorMsg;
    if (!FEpicUnrealMCPCommonUtils::SetObjectProperty(Asset, PropertyName, PropertyValue, ErrorMsg))
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(ErrorMsg);

    Asset->MarkPackageDirty();
    UEditorAssetLibrary::SaveAsset(AssetPath);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("message"),
        FString::Printf(TEXT("Set '%s' on %s"), *PropertyName, *AssetPath));
    return Result;
}

// ---------------------------------------------------------------------------
// find_references
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FEpicUnrealMCPAssetCommands::HandleFindReferences(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath, Direction;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));

    Direction = TEXT("both");
    Params->TryGetStringField(TEXT("direction"), Direction);

    // Strip object suffix to get package name
    FString PackageName = AssetPath;
    int32 DotIdx = INDEX_NONE;
    if (AssetPath.FindLastChar('.', DotIdx) && DotIdx > 0)
        PackageName = AssetPath.Left(DotIdx);

    IAssetRegistry& AssetRegistry =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

    TArray<TSharedPtr<FJsonValue>> DepsArray, RefsArray;

    if (Direction == TEXT("dependencies") || Direction == TEXT("both"))
    {
        TArray<FName> RawDeps;
        AssetRegistry.GetDependencies(FName(*PackageName), RawDeps,
            UE::AssetRegistry::EDependencyCategory::Package);
        for (const FName& Dep : RawDeps)
            DepsArray.Add(MakeShared<FJsonValueString>(Dep.ToString()));
    }

    if (Direction == TEXT("dependents") || Direction == TEXT("both"))
    {
        TArray<FName> RawRefs;
        AssetRegistry.GetReferencers(FName(*PackageName), RawRefs,
            UE::AssetRegistry::EDependencyCategory::Package);
        for (const FName& Ref : RawRefs)
            RefsArray.Add(MakeShared<FJsonValueString>(Ref.ToString()));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("asset"),        PackageName);
    Result->SetArrayField(TEXT("dependencies"), DepsArray);
    Result->SetArrayField(TEXT("dependents"),   RefsArray);
    return Result;
}

// ---------------------------------------------------------------------------
// duplicate_asset
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FEpicUnrealMCPAssetCommands::HandleDuplicateAsset(const TSharedPtr<FJsonObject>& Params)
{
    FString SourcePath, DestPath, DestName;
    if (!Params->TryGetStringField(TEXT("source_path"), SourcePath))
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'source_path' parameter"));
    if (!Params->TryGetStringField(TEXT("dest_path"), DestPath))
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'dest_path' parameter"));
    if (!Params->TryGetStringField(TEXT("dest_name"), DestName))
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'dest_name' parameter"));

    if (!UEditorAssetLibrary::DoesAssetExist(SourcePath))
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Source not found: %s"), *SourcePath));

    const FString FullDestPath = DestPath + TEXT("/") + DestName;
    UObject* Dup = UEditorAssetLibrary::DuplicateAsset(SourcePath, FullDestPath);
    if (!Dup)
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            TEXT("Duplicate failed (destination may already exist)"));

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("path"), FullDestPath);
    return Result;
}

// ---------------------------------------------------------------------------
// rename_asset
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FEpicUnrealMCPAssetCommands::HandleRenameAsset(const TSharedPtr<FJsonObject>& Params)
{
    FString SourcePath, DestPath;
    if (!Params->TryGetStringField(TEXT("source_path"), SourcePath))
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'source_path' parameter"));
    if (!Params->TryGetStringField(TEXT("dest_path"), DestPath))
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'dest_path' parameter"));

    if (!UEditorAssetLibrary::DoesAssetExist(SourcePath))
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Asset not found: %s"), *SourcePath));

    const bool bOk = UEditorAssetLibrary::RenameAsset(SourcePath, DestPath);
    if (!bOk)
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Rename failed"));

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("from"), SourcePath);
    Result->SetStringField(TEXT("to"),   DestPath);
    return Result;
}

// ---------------------------------------------------------------------------
// delete_asset
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FEpicUnrealMCPAssetCommands::HandleDeleteAsset(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    bool bForce = false;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));
    Params->TryGetBoolField(TEXT("force"), bForce);

    IAssetRegistry& AssetRegistry =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

    if (UEditorAssetLibrary::DoesAssetExist(AssetPath))
    {
        if (!bForce)
        {
            FString PackageName = AssetPath;
            int32 DotIdx = INDEX_NONE;
            if (AssetPath.FindLastChar('.', DotIdx) && DotIdx > 0)
                PackageName = AssetPath.Left(DotIdx);

            TArray<FName> Referencers;
            AssetRegistry.GetReferencers(FName(*PackageName), Referencers,
                UE::AssetRegistry::EDependencyCategory::Package);

            TArray<TSharedPtr<FJsonValue>> RefArray;
            for (const FName& Ref : Referencers)
            {
                const FString RefStr = Ref.ToString();
                if (!RefStr.StartsWith(TEXT("/Script/")))
                    RefArray.Add(MakeShared<FJsonValueString>(RefStr));
            }

            if (RefArray.Num() > 0)
            {
                TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
                Result->SetBoolField(TEXT("success"),  false);
                Result->SetStringField(TEXT("error"),   TEXT("Asset has references"));
                Result->SetArrayField(TEXT("referencers"), RefArray);
                return Result;
            }
        }

        const bool bOk = UEditorAssetLibrary::DeleteAsset(AssetPath);
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetBoolField(TEXT("success"), bOk);
        if (bOk) Result->SetStringField(TEXT("deleted"), AssetPath);
        else      Result->SetStringField(TEXT("error"),   TEXT("Delete failed"));
        return Result;
    }
    else if (UEditorAssetLibrary::DoesDirectoryExist(AssetPath))
    {
        const bool bOk = UEditorAssetLibrary::DeleteDirectory(AssetPath);
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetBoolField(TEXT("success"), bOk);
        if (bOk) Result->SetStringField(TEXT("deleted_directory"), AssetPath);
        else      Result->SetStringField(TEXT("error"),             TEXT("Directory delete failed"));
        return Result;
    }

    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
        FString::Printf(TEXT("Not found: %s"), *AssetPath));
}

// ---------------------------------------------------------------------------
// save_asset
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FEpicUnrealMCPAssetCommands::HandleSaveAsset(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));

    if (!UEditorAssetLibrary::DoesAssetExist(AssetPath))
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Asset not found: %s"), *AssetPath));

    const bool bOk = UEditorAssetLibrary::SaveAsset(AssetPath);
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), bOk);
    if (bOk) Result->SetStringField(TEXT("saved"), AssetPath);
    else      Result->SetStringField(TEXT("error"),  TEXT("Save failed"));
    return Result;
}

// ---------------------------------------------------------------------------
// save_all
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FEpicUnrealMCPAssetCommands::HandleSaveAll(const TSharedPtr<FJsonObject>& Params)
{
    FEditorFileUtils::SaveDirtyPackages(/*bPromptUserToSave=*/false, /*bSaveMapPackages=*/true, /*bSaveContentPackages=*/true);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"),  true);
    Result->SetStringField(TEXT("message"), TEXT("All dirty assets saved"));
    return Result;
}

// ---------------------------------------------------------------------------
// import_asset
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FEpicUnrealMCPAssetCommands::HandleImportAsset(const TSharedPtr<FJsonObject>& Params)
{
    FString SourceFile, DestinationPath;
    if (!Params->TryGetStringField(TEXT("source_file"),      SourceFile))
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'source_file' parameter"));
    if (!Params->TryGetStringField(TEXT("destination_path"), DestinationPath))
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'destination_path' parameter"));

    SourceFile = SourceFile.Replace(TEXT("\\"), TEXT("/"));

    // Validate source file exists
    if (!FPaths::FileExists(SourceFile))
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Source file does not exist: %s"), *SourceFile));

    // Optional params
    FString DestinationName;
    const bool bHasDestName = Params->TryGetStringField(TEXT("destination_name"), DestinationName);
    bool bReplaceExisting = true;
    Params->TryGetBoolField(TEXT("replace_existing"), bReplaceExisting);

    // Find a legacy UFactory to bypass the Interchange async pipeline
    // (Interchange causes task graph recursion when called from the game thread)
    UFactory* Factory = FindFactoryForFile(SourceFile);

    UAssetImportTask* Task = NewObject<UAssetImportTask>();
    Task->Filename        = SourceFile;
    Task->DestinationPath = DestinationPath;
    Task->bAutomated      = true;
    Task->bSave           = true;
    Task->bAsync          = false;
    Task->bReplaceExisting = bReplaceExisting;
    Task->Factory          = Factory;
    if (bHasDestName && !DestinationName.IsEmpty())
    {
        Task->DestinationName = DestinationName;
    }

    IAssetTools& AssetTools =
        FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
    TArray<UAssetImportTask*> Tasks = { Task };
    AssetTools.ImportAssetTasks(Tasks);

    // Clean up factory root reference
    if (Factory)
    {
        Factory->RemoveFromRoot();
    }

    TArray<TSharedPtr<FJsonValue>> ImportedArray;
    TArray<TSharedPtr<FJsonValue>> ImportedClassArray;
    for (UObject* ImportedObj : Task->GetObjects())
    {
        if (ImportedObj)
        {
            ImportedArray.Add(MakeShared<FJsonValueString>(ImportedObj->GetPathName()));
            ImportedClassArray.Add(MakeShared<FJsonValueString>(ImportedObj->GetClass()->GetName()));
        }
    }
    // Fallback to ImportedObjectPaths if GetObjects() is empty
    if (ImportedArray.IsEmpty())
    {
        for (const FString& Imported : Task->ImportedObjectPaths)
        {
            ImportedArray.Add(MakeShared<FJsonValueString>(Imported));
        }
    }

    const bool bSuccess = !ImportedArray.IsEmpty();

    // Get source file size
    const int64 FileSize = IFileManager::Get().FileSize(*SourceFile);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), bSuccess);
    Result->SetStringField(TEXT("source_file"), SourceFile);
    Result->SetStringField(TEXT("destination"), DestinationPath);
    Result->SetNumberField(TEXT("source_file_size"), static_cast<double>(FileSize));
    Result->SetArrayField(TEXT("imported_paths"), ImportedArray);
    if (!ImportedClassArray.IsEmpty())
    {
        Result->SetArrayField(TEXT("imported_classes"), ImportedClassArray);
    }
    if (!bSuccess)
    {
        Result->SetStringField(TEXT("error"), TEXT("Import produced no assets — check file format and destination path"));
    }
    return Result;
}

// ---------------------------------------------------------------------------
// import_assets_batch
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FEpicUnrealMCPAssetCommands::HandleImportAssetsBatch(const TSharedPtr<FJsonObject>& Params)
{
    FString DestinationPath;
    if (!Params->TryGetStringField(TEXT("destination_path"), DestinationPath))
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'destination_path' parameter"));

    bool bReplaceExisting = true;
    Params->TryGetBoolField(TEXT("replace_existing"), bReplaceExisting);

    // Collect source files — either from explicit "files" array or directory scan
    TArray<FString> SourceFiles;

    const TArray<TSharedPtr<FJsonValue>>* FilesArray = nullptr;
    FString SourceDirectory;

    if (Params->TryGetArrayField(TEXT("files"), FilesArray) && FilesArray && FilesArray->Num() > 0)
    {
        for (const TSharedPtr<FJsonValue>& Val : *FilesArray)
        {
            FString FilePath = Val->AsString().Replace(TEXT("\\"), TEXT("/"));
            if (!FilePath.IsEmpty())
            {
                SourceFiles.Add(FilePath);
            }
        }
    }
    else if (Params->TryGetStringField(TEXT("source_directory"), SourceDirectory))
    {
        SourceDirectory = SourceDirectory.Replace(TEXT("\\"), TEXT("/"));

        if (!FPaths::DirectoryExists(SourceDirectory))
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("Source directory does not exist: %s"), *SourceDirectory));

        // Collect extension filters
        TArray<FString> Extensions;
        const TArray<TSharedPtr<FJsonValue>>* ExtArray = nullptr;
        if (Params->TryGetArrayField(TEXT("extensions"), ExtArray) && ExtArray)
        {
            for (const TSharedPtr<FJsonValue>& Val : *ExtArray)
            {
                FString Ext = Val->AsString().ToLower();
                if (!Ext.StartsWith(TEXT(".")))
                {
                    Ext = TEXT(".") + Ext;
                }
                Extensions.Add(Ext);
            }
        }

        // Scan directory for matching files
        IFileManager& FM = IFileManager::Get();
        TArray<FString> FoundFiles;
        FM.FindFilesRecursive(FoundFiles, *SourceDirectory, TEXT("*.*"), /*Files=*/true, /*Directories=*/false);

        for (const FString& Found : FoundFiles)
        {
            if (Extensions.Num() > 0)
            {
                const FString FileExt = FPaths::GetExtension(Found, /*bIncludeDot=*/true).ToLower();
                if (!Extensions.Contains(FileExt))
                {
                    continue;
                }
            }
            SourceFiles.Add(Found.Replace(TEXT("\\"), TEXT("/")));
        }

        if (SourceFiles.Num() == 0)
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("No matching files found in directory: %s"), *SourceDirectory));
    }
    else
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            TEXT("Must provide either 'files' array or 'source_directory' parameter"));
    }

    // Create import tasks
    IAssetTools& AssetTools =
        FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();

    TArray<UAssetImportTask*> ImportTasks;
    TArray<UFactory*> Factories;  // track for cleanup
    ImportTasks.Reserve(SourceFiles.Num());

    for (const FString& SourceFile : SourceFiles)
    {
        UFactory* Factory = FindFactoryForFile(SourceFile);
        if (Factory)
        {
            Factories.Add(Factory);
        }

        UAssetImportTask* Task = NewObject<UAssetImportTask>();
        Task->Filename         = SourceFile;
        Task->DestinationPath  = DestinationPath;
        Task->bAutomated       = true;
        Task->bSave            = true;
        Task->bAsync           = false;
        Task->bReplaceExisting = bReplaceExisting;
        Task->Factory          = Factory;
        ImportTasks.Add(Task);
    }

    AssetTools.ImportAssetTasks(ImportTasks);

    // Clean up factory root references
    for (UFactory* Factory : Factories)
    {
        Factory->RemoveFromRoot();
    }

    // Build per-file results
    TArray<TSharedPtr<FJsonValue>> ResultsArray;
    int32 SuccessCount = 0;
    int32 FailCount = 0;

    for (int32 i = 0; i < ImportTasks.Num(); ++i)
    {
        UAssetImportTask* Task = ImportTasks[i];
        TSharedPtr<FJsonObject> FileResult = MakeShared<FJsonObject>();
        FileResult->SetStringField(TEXT("source_file"), SourceFiles[i]);

        TArray<TSharedPtr<FJsonValue>> ImportedPaths;
        for (UObject* ImportedObj : Task->GetObjects())
        {
            if (ImportedObj)
            {
                ImportedPaths.Add(MakeShared<FJsonValueString>(ImportedObj->GetPathName()));
            }
        }
        if (ImportedPaths.IsEmpty())
        {
            for (const FString& Path : Task->ImportedObjectPaths)
            {
                ImportedPaths.Add(MakeShared<FJsonValueString>(Path));
            }
        }

        const bool bFileSuccess = !ImportedPaths.IsEmpty();
        FileResult->SetBoolField(TEXT("success"), bFileSuccess);
        FileResult->SetArrayField(TEXT("imported_paths"), ImportedPaths);

        if (bFileSuccess)
        {
            ++SuccessCount;
        }
        else
        {
            ++FailCount;
            FileResult->SetStringField(TEXT("error"), TEXT("Import produced no assets"));
        }

        ResultsArray.Add(MakeShared<FJsonValueObject>(FileResult));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), FailCount == 0);
    Result->SetStringField(TEXT("destination"), DestinationPath);
    Result->SetNumberField(TEXT("total_files"), SourceFiles.Num());
    Result->SetNumberField(TEXT("succeeded"), SuccessCount);
    Result->SetNumberField(TEXT("failed"), FailCount);
    Result->SetArrayField(TEXT("results"), ResultsArray);
    return Result;
}

// ---------------------------------------------------------------------------
// get_selected_assets
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FEpicUnrealMCPAssetCommands::HandleGetSelectedAssets(const TSharedPtr<FJsonObject>& Params)
{
    TArray<UObject*> SelectedAssets = UEditorUtilityLibrary::GetSelectedAssets();

    TArray<TSharedPtr<FJsonValue>> AssetArray;
    for (UObject* Asset : SelectedAssets)
    {
        if (!Asset) continue;
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetStringField(TEXT("name"),  Asset->GetName());
        Obj->SetStringField(TEXT("path"),  Asset->GetPathName());
        Obj->SetStringField(TEXT("class"), Asset->GetClass()->GetName());
        AssetArray.Add(MakeShared<FJsonValueObject>(Obj));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetNumberField(TEXT("count"),  SelectedAssets.Num());
    Result->SetArrayField(TEXT("assets"), AssetArray);
    return Result;
}

// ---------------------------------------------------------------------------
// sync_browser
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FEpicUnrealMCPAssetCommands::HandleSyncBrowser(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));

    // UEditorAssetLibrary::SyncBrowserToObjects takes a TArray<FString> of asset paths
    TArray<FString> AssetPaths = { AssetPath };
    UEditorAssetLibrary::SyncBrowserToObjects(AssetPaths);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("message"),
        FString::Printf(TEXT("Synced Content Browser to %s"), *AssetPath));
    return Result;
}
