#include "Commands/EpicUnrealMCPNiagaraCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "NiagaraHelpers.h"

#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraScript.h"
#include "NiagaraCommon.h"
#include "NiagaraRendererProperties.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "EditorAssetLibrary.h"

#if WITH_EDITORONLY_DATA
#include "NiagaraSystemEditorData.h"
#include "NiagaraSystemFactoryNew.h"
#endif

// ---------------------------------------------------------------------------
// HandleCreateNiagaraSystem
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleCreateNiagaraSystem(
	const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));
	}

	FString TemplateName = TEXT("empty");
	Params->TryGetStringField(TEXT("template"), TemplateName);

	if (UEditorAssetLibrary::DoesAssetExist(AssetPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Asset already exists: %s"), *AssetPath));
	}

	// Split path into directory + name
	FString Path;
	FString Name;
	int32 LastSlash = INDEX_NONE;
	if (!AssetPath.FindLastChar('/', LastSlash) || LastSlash <= 0)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Invalid asset_path format. Expected '/Game/Path/AssetName'"));
	}
	Path = AssetPath.Left(LastSlash);
	Name = AssetPath.Mid(LastSlash + 1);

	if (Name.IsEmpty())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Asset name is empty"));
	}

	// Use the Niagara system factory for proper script source + graph initialization
	UNiagaraSystemFactoryNew* Factory = NewObject<UNiagaraSystemFactoryNew>();
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UObject* NewAsset = AssetTools.CreateAsset(Name, Path, UNiagaraSystem::StaticClass(), Factory);
	UNiagaraSystem* System = Cast<UNiagaraSystem>(NewAsset);

	if (!System)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create Niagara system"));
	}

	// Template handling: load known emitter templates if requested
	bool bTemplateApplied = false;
	if (!TemplateName.Equals(TEXT("empty"), ESearchCase::IgnoreCase))
	{
		FString EmitterAssetPath;
		FString LowerTemplate = TemplateName.ToLower();

		if (LowerTemplate == TEXT("simple_sprite") || LowerTemplate == TEXT("sprite"))
		{
			EmitterAssetPath = TEXT("/Niagara/DefaultAssets/Templates/Systems/Simple Sprite Burst.Simple Sprite Burst");
		}
		else if (LowerTemplate == TEXT("fountain") || LowerTemplate == TEXT("default"))
		{
			EmitterAssetPath = TEXT("/Niagara/DefaultAssets/Templates/Systems/Fountain.Fountain");
		}
		else if (LowerTemplate == TEXT("mesh"))
		{
			EmitterAssetPath = TEXT("/Niagara/DefaultAssets/Templates/Emitters/UpwardMeshBurst.UpwardMeshBurst");
		}
		else if (LowerTemplate == TEXT("ribbon"))
		{
			EmitterAssetPath = TEXT("/Niagara/DefaultAssets/Templates/Emitters/LocationBasedRibbon.LocationBasedRibbon");
		}
		else
		{
			// Treat template string as direct asset path
			EmitterAssetPath = TemplateName;
		}

		// First try as a system template (contains emitters we can copy)
		UNiagaraSystem* TemplateSystem = Cast<UNiagaraSystem>(
			UEditorAssetLibrary::LoadAsset(EmitterAssetPath));

		if (TemplateSystem)
		{
			const TArray<FNiagaraEmitterHandle>& TemplateHandles = TemplateSystem->GetEmitterHandles();
			for (const FNiagaraEmitterHandle& TH : TemplateHandles)
			{
				FVersionedNiagaraEmitter EmitterInstance = TH.GetInstance();
				if (EmitterInstance.Emitter)
				{
					System->AddEmitterHandle(
						*EmitterInstance.Emitter,
						TH.GetName(),
						EmitterInstance.Version);
					bTemplateApplied = true;
				}
			}
		}
		else
		{
			// Try loading as standalone emitter
			UNiagaraEmitter* DirectEmitter = Cast<UNiagaraEmitter>(
				UEditorAssetLibrary::LoadAsset(EmitterAssetPath));

			if (DirectEmitter)
			{
				System->AddEmitterHandle(
					*DirectEmitter,
					DirectEmitter->GetFName(),
					DirectEmitter->GetExposedVersion().VersionGuid);
				bTemplateApplied = true;
			}
		}
	}

	NiagaraHelpers::CompileAndSync(System);
	UEditorAssetLibrary::SaveAsset(AssetPath);

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("asset_name"), Name);
	Result->SetStringField(TEXT("asset_path"), AssetPath);
	Result->SetStringField(TEXT("package_path"), Path);
	Result->SetStringField(TEXT("template"), TemplateName);
	Result->SetBoolField(TEXT("template_applied"), bTemplateApplied);
	Result->SetNumberField(TEXT("emitter_count"), System->GetNumEmitters());
	return Result;
#else
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Requires editor mode"));
#endif
}

// ---------------------------------------------------------------------------
// HandleGetNiagaraSystemInfo
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleGetNiagaraSystemInfo(
	const TSharedPtr<FJsonObject>& Params)
{
	FString SystemPath;
	if (!Params->TryGetStringField(TEXT("system_path"), SystemPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'system_path' parameter"));
	}

	FString Error;
	UNiagaraSystem* System = NiagaraHelpers::LoadNiagaraSystem(SystemPath, Error);
	if (!System)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	FString IncludeStr = TEXT("all");
	Params->TryGetStringField(TEXT("include"), IncludeStr);
	IncludeStr = IncludeStr.ToLower();

	bool bAll = IncludeStr.Contains(TEXT("all"));
	bool bEmitters = bAll || IncludeStr.Contains(TEXT("emitters"));
	bool bParameters = bAll || IncludeStr.Contains(TEXT("parameters"));
	bool bCompilation = bAll || IncludeStr.Contains(TEXT("compilation"));

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("system_name"), System->GetName());
	Result->SetStringField(TEXT("asset_path"), SystemPath);
	Result->SetNumberField(TEXT("emitter_count"), System->GetNumEmitters());

	if (bEmitters)
	{
		TArray<TSharedPtr<FJsonValue>> EmitterArr;
		const TArray<FNiagaraEmitterHandle>& Handles = System->GetEmitterHandles();
		for (int32 i = 0; i < Handles.Num(); ++i)
		{
			EmitterArr.Add(MakeShared<FJsonValueObject>(
				NiagaraHelpers::EmitterHandleToJson(Handles[i], i)));
		}
		Result->SetArrayField(TEXT("emitters"), EmitterArr);
	}

	if (bParameters)
	{
		TArray<TSharedPtr<FJsonValue>> ParamArr;
		const FNiagaraUserRedirectionParameterStore& Store = System->GetExposedParameters();
		TArrayView<const FNiagaraVariableWithOffset> Vars = Store.ReadParameterVariables();
		for (const FNiagaraVariableWithOffset& V : Vars)
		{
			auto PObj = MakeShared<FJsonObject>();
			PObj->SetStringField(TEXT("name"), V.GetName().ToString());
			PObj->SetStringField(TEXT("type"), V.GetType().GetName());
			ParamArr.Add(MakeShared<FJsonValueObject>(PObj));
		}
		Result->SetArrayField(TEXT("parameters"), ParamArr);
	}

	if (bCompilation)
	{
		auto CompObj = MakeShared<FJsonObject>();
		CompObj->SetBoolField(TEXT("is_valid"), System->IsValid());
		CompObj->SetBoolField(TEXT("is_ready_to_run"), System->IsReadyToRun());
		Result->SetObjectField(TEXT("compilation"), CompObj);
	}

	return Result;
}

// ---------------------------------------------------------------------------
// HandleListNiagaraSystems
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleListNiagaraSystems(
	const TSharedPtr<FJsonObject>& Params)
{
	FString SearchPath = TEXT("/Game");
	Params->TryGetStringField(TEXT("path"), SearchPath);

	FString NameFilter;
	Params->TryGetStringField(TEXT("name_filter"), NameFilter);

	double MaxD = 100.0;
	Params->TryGetNumberField(TEXT("max_results"), MaxD);
	int32 MaxResults = FMath::Max(1, (int32)MaxD);

	IAssetRegistry& Registry =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

	const FTopLevelAssetPath ClassPath(TEXT("/Script/Niagara.NiagaraSystem"));
	TArray<FAssetData> Assets;
	Registry.GetAssetsByClass(ClassPath, Assets, true);

	// Filter by path
	if (!SearchPath.IsEmpty())
	{
		Assets.RemoveAll([&SearchPath](const FAssetData& A)
		{
			return !A.PackageName.ToString().StartsWith(SearchPath);
		});
	}

	// Filter by name
	if (!NameFilter.IsEmpty())
	{
		FString Lower = NameFilter.ToLower();
		Assets.RemoveAll([&Lower](const FAssetData& A)
		{
			return !A.AssetName.ToString().ToLower().Contains(Lower);
		});
	}

	if (Assets.Num() > MaxResults)
	{
		Assets.SetNum(MaxResults);
	}

	TArray<TSharedPtr<FJsonValue>> SystemArr;
	for (const FAssetData& A : Assets)
	{
		auto Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("name"), A.AssetName.ToString());
		Obj->SetStringField(TEXT("path"), A.GetObjectPathString());
		SystemArr.Add(MakeShared<FJsonValueObject>(Obj));
	}

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetArrayField(TEXT("systems"), SystemArr);
	Result->SetNumberField(TEXT("count"), SystemArr.Num());
	return Result;
}

// ---------------------------------------------------------------------------
// HandleDeleteNiagaraSystem
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleDeleteNiagaraSystem(
	const TSharedPtr<FJsonObject>& Params)
{
	FString SystemPath;
	if (!Params->TryGetStringField(TEXT("system_path"), SystemPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'system_path' parameter"));
	}

	bool bForce = false;
	Params->TryGetBoolField(TEXT("force"), bForce);

	if (!UEditorAssetLibrary::DoesAssetExist(SystemPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("System not found: %s"), *SystemPath));
	}

	if (!bForce)
	{
		IAssetRegistry& Registry =
			FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

		FString PkgName = SystemPath;
		int32 DotIdx;
		if (SystemPath.FindLastChar('.', DotIdx))
		{
			PkgName = SystemPath.Left(DotIdx);
		}

		TArray<FName> Refs;
		Registry.GetReferencers(FName(*PkgName), Refs,
			UE::AssetRegistry::EDependencyCategory::Package);

		TArray<TSharedPtr<FJsonValue>> RefArr;
		for (const FName& R : Refs)
		{
			if (!R.ToString().StartsWith(TEXT("/Script/")))
			{
				RefArr.Add(MakeShared<FJsonValueString>(R.ToString()));
			}
		}
		if (RefArr.Num() > 0)
		{
			auto Result = MakeShared<FJsonObject>();
			Result->SetBoolField(TEXT("success"), false);
			Result->SetStringField(TEXT("error"), TEXT("Asset has references. Pass force=true to delete."));
			Result->SetArrayField(TEXT("referencers"), RefArr);
			return Result;
		}
	}

	bool bDeleted = UEditorAssetLibrary::DeleteAsset(SystemPath);

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), bDeleted);
	if (!bDeleted)
	{
		Result->SetStringField(TEXT("error"), TEXT("Failed to delete asset"));
	}
	return Result;
}

// ---------------------------------------------------------------------------
// HandleCompileNiagaraSystem
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleCompileNiagaraSystem(
	const TSharedPtr<FJsonObject>& Params)
{
	FString SystemPath;
	if (!Params->TryGetStringField(TEXT("system_path"), SystemPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'system_path' parameter"));
	}

	bool bWait = true;
	Params->TryGetBoolField(TEXT("wait_for_completion"), bWait);

	FString Error;
	UNiagaraSystem* System = NiagaraHelpers::LoadNiagaraSystem(SystemPath, Error);
	if (!System)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	System->RequestCompile(true);
	if (bWait)
	{
		System->WaitForCompilationComplete(/*bIncludingGPUShaders=*/ false, /*bShowProgress=*/ false);
	}

	UEditorAssetLibrary::SaveAsset(SystemPath);

	// Gather per-script compile status and errors
	TArray<TSharedPtr<FJsonValue>> ScriptStatuses;
	int32 ErrorCount = 0;

	auto ReportScript = [&ScriptStatuses, &ErrorCount](const FString& ScriptLabel, UNiagaraScript* Script)
	{
		if (!Script)
		{
			return;
		}

		ENiagaraScriptCompileStatus Status = Script->GetLastCompileStatus();
		FString StatusStr;
		switch (Status)
		{
		case ENiagaraScriptCompileStatus::NCS_UpToDate:
			StatusStr = TEXT("up_to_date");
			break;
		case ENiagaraScriptCompileStatus::NCS_Dirty:
			StatusStr = TEXT("dirty");
			break;
		case ENiagaraScriptCompileStatus::NCS_Error:
			StatusStr = TEXT("error");
			ErrorCount++;
			break;
		default:
			StatusStr = TEXT("unknown");
			break;
		}

		auto Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("script"), ScriptLabel);
		Obj->SetStringField(TEXT("status"), StatusStr);
		ScriptStatuses.Add(MakeShared<FJsonValueObject>(Obj));
	};

	// System-level scripts
	ReportScript(TEXT("system_spawn"), System->GetSystemSpawnScript());
	ReportScript(TEXT("system_update"), System->GetSystemUpdateScript());

	// Per-emitter scripts
	const TArray<FNiagaraEmitterHandle>& Handles = System->GetEmitterHandles();
	for (int32 i = 0; i < Handles.Num(); ++i)
	{
		FVersionedNiagaraEmitterData* EmitterData =
			const_cast<FNiagaraEmitterHandle&>(Handles[i]).GetEmitterData();
		if (!EmitterData)
		{
			continue;
		}

		FString Prefix = FString::Printf(TEXT("%s"), *Handles[i].GetName().ToString());

		ReportScript(Prefix + TEXT(".emitter_spawn"), EmitterData->EmitterSpawnScriptProps.Script);
		ReportScript(Prefix + TEXT(".emitter_update"), EmitterData->EmitterUpdateScriptProps.Script);
		ReportScript(Prefix + TEXT(".particle_spawn"), EmitterData->SpawnScriptProps.Script);
		ReportScript(Prefix + TEXT(".particle_update"), EmitterData->UpdateScriptProps.Script);
	}

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetBoolField(TEXT("is_valid"), System->IsValid());
	Result->SetBoolField(TEXT("is_ready_to_run"), System->IsReadyToRun());
	Result->SetBoolField(TEXT("has_outstanding_compilations"), System->HasOutstandingCompilationRequests());
	Result->SetNumberField(TEXT("error_count"), ErrorCount);
	Result->SetArrayField(TEXT("script_statuses"), ScriptStatuses);
	return Result;
}
