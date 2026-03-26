#include "Commands/EpicUnrealMCPNiagaraCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "NiagaraHelpers.h"

#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraComponent.h"
#include "NiagaraCommon.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraSystemInstanceController.h"
#include "NiagaraEmitterInstance.h"
#include "NiagaraMessageStore.h"
#include "NiagaraMessageDataBase.h"
#include "NiagaraRendererProperties.h"
#include "NiagaraShared.h"
#include "NiagaraTypes.h"

#include "EditorAssetLibrary.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"

#if WITH_EDITORONLY_DATA
#include "NiagaraSystemEditorData.h"
#endif

// ---------------------------------------------------------------------------
// Local helpers
// ---------------------------------------------------------------------------

static FString CompileStatusToString(ENiagaraScriptCompileStatus Status)
{
	switch (Status)
	{
	case ENiagaraScriptCompileStatus::NCS_Unknown:
		return TEXT("unknown");
	case ENiagaraScriptCompileStatus::NCS_Dirty:
		return TEXT("dirty");
	case ENiagaraScriptCompileStatus::NCS_Error:
		return TEXT("error");
	case ENiagaraScriptCompileStatus::NCS_UpToDate:
		return TEXT("up_to_date");
	case ENiagaraScriptCompileStatus::NCS_BeingCreated:
		return TEXT("being_created");
	case ENiagaraScriptCompileStatus::NCS_UpToDateWithWarnings:
		return TEXT("warning");
	case ENiagaraScriptCompileStatus::NCS_ComputeUpToDateWithWarnings:
		return TEXT("compute_warning");
	default:
		return TEXT("unknown");
	}
}

static FString ExecutionStateToString(ENiagaraExecutionState State)
{
	switch (State)
	{
	case ENiagaraExecutionState::Active:
		return TEXT("active");
	case ENiagaraExecutionState::Inactive:
		return TEXT("inactive");
	case ENiagaraExecutionState::InactiveClear:
		return TEXT("inactive_clear");
	case ENiagaraExecutionState::Complete:
		return TEXT("complete");
	case ENiagaraExecutionState::Disabled:
		return TEXT("disabled");
	default:
		return TEXT("unknown");
	}
}

/** Map compile status to a severity string for filtering. */
static FString CompileStatusToSeverity(ENiagaraScriptCompileStatus Status)
{
	switch (Status)
	{
	case ENiagaraScriptCompileStatus::NCS_Error:
		return TEXT("error");
	case ENiagaraScriptCompileStatus::NCS_UpToDateWithWarnings:
	case ENiagaraScriptCompileStatus::NCS_ComputeUpToDateWithWarnings:
		return TEXT("warning");
	case ENiagaraScriptCompileStatus::NCS_Dirty:
	case ENiagaraScriptCompileStatus::NCS_BeingCreated:
	case ENiagaraScriptCompileStatus::NCS_Unknown:
		return TEXT("info");
	default:
		return TEXT("info");
	}
}

/** Check if a severity matches the requested filter. "all" matches everything. */
static bool SeverityMatchesFilter(const FString& Severity, const FString& Filter)
{
	if (Filter.Equals(TEXT("all"), ESearchCase::IgnoreCase))
	{
		return true;
	}
	return Severity.Equals(Filter, ESearchCase::IgnoreCase);
}

// ---------------------------------------------------------------------------
// HandleGetNiagaraSystemErrors
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleGetNiagaraSystemErrors(
	const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
	FString SystemPath;
	if (!Params->TryGetStringField(TEXT("system_path"), SystemPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Missing required 'system_path' parameter"));
	}

	FString LoadError;
	UNiagaraSystem* System = NiagaraHelpers::LoadNiagaraSystem(SystemPath, LoadError);
	if (!System)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(LoadError);
	}

	// Optional filters
	FString EmitterFilter;
	Params->TryGetStringField(TEXT("emitter_name"), EmitterFilter);

	FString SeverityFilter = TEXT("all");
	Params->TryGetStringField(TEXT("severity"), SeverityFilter);

	TArray<TSharedPtr<FJsonValue>> IssuesArray;

	// Iterate all emitter handles
	const TArray<FNiagaraEmitterHandle>& Handles = System->GetEmitterHandles();
	for (int32 HandleIdx = 0; HandleIdx < Handles.Num(); ++HandleIdx)
	{
		const FNiagaraEmitterHandle& Handle = Handles[HandleIdx];
		FString EmitterName = Handle.GetName().ToString();

		// Filter by emitter name if specified
		if (!EmitterFilter.IsEmpty()
			&& !EmitterName.Equals(EmitterFilter, ESearchCase::IgnoreCase))
		{
			continue;
		}

		FVersionedNiagaraEmitterData* EmitterData = NiagaraHelpers::GetEmitterData(
			const_cast<FNiagaraEmitterHandle*>(&Handle));
		if (!EmitterData)
		{
			continue;
		}

		// Get all scripts for this emitter
		TArray<UNiagaraScript*> Scripts;
		EmitterData->GetScripts(Scripts, false, false);

		for (UNiagaraScript* Script : Scripts)
		{
			if (!Script)
			{
				continue;
			}

			ENiagaraScriptCompileStatus CompileStatus = Script->GetLastCompileStatus();

			// Skip up-to-date scripts with no issues
			if (CompileStatus == ENiagaraScriptCompileStatus::NCS_UpToDate)
			{
				continue;
			}

			FString Severity = CompileStatusToSeverity(CompileStatus);

			if (!SeverityMatchesFilter(Severity, SeverityFilter))
			{
				continue;
			}

			FString StatusStr = CompileStatusToString(CompileStatus);
			FString UsageStr = NiagaraHelpers::ScriptUsageToString(Script->GetUsage());

			TSharedPtr<FJsonObject> IssueObj = MakeShared<FJsonObject>();
			IssueObj->SetStringField(TEXT("emitter_name"), EmitterName);
			IssueObj->SetStringField(TEXT("severity"), Severity);
			IssueObj->SetStringField(TEXT("compile_status"), StatusStr);
			IssueObj->SetStringField(TEXT("script_usage"), UsageStr);
			IssueObj->SetStringField(TEXT("source"), TEXT("compile_status"));
			IssueObj->SetStringField(TEXT("description"),
				FString::Printf(TEXT("Script '%s' in emitter '%s' has status: %s"),
					*UsageStr, *EmitterName, *StatusStr));
			IssueObj->SetBoolField(TEXT("has_fix"), false);

			IssuesArray.Add(MakeShared<FJsonValueObject>(IssueObj));

			// Script->ErrorMsg and LastCompileEvents use FNiagaraCompileEvent from
			// NiagaraShader module which has include resolution issues in unity builds.
			// Renderer feedback below is the primary source for actionable warnings.
		}

		// ---- Renderer Feedback (NO ViewModel needed) ----
		// GetRendererFeedback returns errors, warnings, and info per renderer
		FVersionedNiagaraEmitter VersionedEmitter = Handle.GetInstance();
		const TArray<UNiagaraRendererProperties*>& Renderers = EmitterData->GetRenderers();
		for (int32 RendererIdx = 0; RendererIdx < Renderers.Num(); ++RendererIdx)
		{
			UNiagaraRendererProperties* Renderer = Renderers[RendererIdx];
			if (!Renderer)
			{
				continue;
			}

			TArray<FNiagaraRendererFeedback> RendererErrors;
			TArray<FNiagaraRendererFeedback> RendererWarnings;
			TArray<FNiagaraRendererFeedback> RendererInfo;

			Renderer->GetRendererFeedback(VersionedEmitter, RendererErrors, RendererWarnings, RendererInfo);

			// Process errors
			for (const FNiagaraRendererFeedback& Feedback : RendererErrors)
			{
				if (!SeverityMatchesFilter(TEXT("error"), SeverityFilter))
				{
					continue;
				}

				TSharedPtr<FJsonObject> FeedbackObj = MakeShared<FJsonObject>();
				FeedbackObj->SetStringField(TEXT("emitter_name"), EmitterName);
				FeedbackObj->SetStringField(TEXT("severity"), TEXT("error"));
				FeedbackObj->SetStringField(TEXT("source"), TEXT("renderer"));
				FeedbackObj->SetStringField(TEXT("renderer_type"), Renderer->GetClass()->GetName());
				FeedbackObj->SetNumberField(TEXT("renderer_index"), RendererIdx);
				FeedbackObj->SetStringField(TEXT("description"), Feedback.GetDescriptionText().ToString());
				FeedbackObj->SetBoolField(TEXT("has_fix"), Feedback.IsFixable());
				if (Feedback.IsFixable())
				{
					FeedbackObj->SetStringField(TEXT("fix_description"), Feedback.GetFixDescriptionText().ToString());
				}
				IssuesArray.Add(MakeShared<FJsonValueObject>(FeedbackObj));
			}

			// Process warnings
			for (const FNiagaraRendererFeedback& Feedback : RendererWarnings)
			{
				if (!SeverityMatchesFilter(TEXT("warning"), SeverityFilter))
				{
					continue;
				}

				TSharedPtr<FJsonObject> FeedbackObj = MakeShared<FJsonObject>();
				FeedbackObj->SetStringField(TEXT("emitter_name"), EmitterName);
				FeedbackObj->SetStringField(TEXT("severity"), TEXT("warning"));
				FeedbackObj->SetStringField(TEXT("source"), TEXT("renderer"));
				FeedbackObj->SetStringField(TEXT("renderer_type"), Renderer->GetClass()->GetName());
				FeedbackObj->SetNumberField(TEXT("renderer_index"), RendererIdx);
				FeedbackObj->SetStringField(TEXT("description"), Feedback.GetDescriptionText().ToString());
				FeedbackObj->SetBoolField(TEXT("has_fix"), Feedback.IsFixable());
				if (Feedback.IsFixable())
				{
					FeedbackObj->SetStringField(TEXT("fix_description"), Feedback.GetFixDescriptionText().ToString());
				}
				IssuesArray.Add(MakeShared<FJsonValueObject>(FeedbackObj));
			}

			// Process info
			for (const FNiagaraRendererFeedback& Feedback : RendererInfo)
			{
				if (!SeverityMatchesFilter(TEXT("info"), SeverityFilter))
				{
					continue;
				}

				TSharedPtr<FJsonObject> FeedbackObj = MakeShared<FJsonObject>();
				FeedbackObj->SetStringField(TEXT("emitter_name"), EmitterName);
				FeedbackObj->SetStringField(TEXT("severity"), TEXT("info"));
				FeedbackObj->SetStringField(TEXT("source"), TEXT("renderer"));
				FeedbackObj->SetStringField(TEXT("renderer_type"), Renderer->GetClass()->GetName());
				FeedbackObj->SetNumberField(TEXT("renderer_index"), RendererIdx);
				FeedbackObj->SetStringField(TEXT("description"), Feedback.GetDescriptionText().ToString());
				FeedbackObj->SetBoolField(TEXT("has_fix"), false);
				IssuesArray.Add(MakeShared<FJsonValueObject>(FeedbackObj));
			}
		}
	}

	// Check system-level message store
	FNiagaraMessageStore& MessageStore = System->GetMessageStore();
	const TMap<FGuid, TObjectPtr<UNiagaraMessageDataBase>>& Messages = MessageStore.GetMessages();

	for (const auto& Pair : Messages)
	{
		UNiagaraMessageDataBase* MessageData = Pair.Value;
		if (!MessageData)
		{
			continue;
		}

		// Message store entries are system-level issues
		FString Severity = TEXT("info");
		if (!SeverityMatchesFilter(Severity, SeverityFilter))
		{
			continue;
		}

		// If filtering by emitter, skip system-level messages
		if (!EmitterFilter.IsEmpty())
		{
			continue;
		}

		TSharedPtr<FJsonObject> IssueObj = MakeShared<FJsonObject>();
		IssueObj->SetStringField(TEXT("emitter_name"), TEXT("System"));
		IssueObj->SetStringField(TEXT("severity"), Severity);
		IssueObj->SetStringField(TEXT("description"),
			FString::Printf(TEXT("System message: %s"), *MessageData->GetClass()->GetName()));
		IssueObj->SetStringField(TEXT("message_key"), Pair.Key.ToString());
		IssueObj->SetBoolField(TEXT("has_fix"), MessageData->GetAllowDismissal());

		IssuesArray.Add(MakeShared<FJsonValueObject>(IssueObj));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("system_path"), SystemPath);
	Result->SetNumberField(TEXT("issue_count"), IssuesArray.Num());
	Result->SetArrayField(TEXT("issues"), IssuesArray);

	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
#else
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
		TEXT("get_niagara_system_errors requires editor builds (WITH_EDITORONLY_DATA)"));
#endif
}

// ---------------------------------------------------------------------------
// HandleGetNiagaraParticleStats
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleGetNiagaraParticleStats(
	const TSharedPtr<FJsonObject>& Params)
{
	FString SystemPath;
	if (!Params->TryGetStringField(TEXT("system_path"), SystemPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Missing required 'system_path' parameter"));
	}

	FString LoadError;
	UNiagaraSystem* System = NiagaraHelpers::LoadNiagaraSystem(SystemPath, LoadError);
	if (!System)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(LoadError);
	}

	FString EmitterFilter;
	Params->TryGetStringField(TEXT("emitter_name"), EmitterFilter);

	// Search all worlds for UNiagaraComponent instances running this system
	TArray<TSharedPtr<FJsonValue>> EmittersArray;
	bool bFoundInstance = false;

	UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!EditorWorld)
	{
		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetStringField(TEXT("system_path"), SystemPath);
		Result->SetStringField(TEXT("message"), TEXT("No editor world available"));
		Result->SetArrayField(TEXT("emitters"), EmittersArray);
		return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
	}

	for (TActorIterator<AActor> ActorIt(EditorWorld); ActorIt; ++ActorIt)
	{
		TArray<UNiagaraComponent*> NiagaraComponents;
		ActorIt->GetComponents<UNiagaraComponent>(NiagaraComponents);

		for (UNiagaraComponent* NiagaraComp : NiagaraComponents)
		{
			if (!NiagaraComp || NiagaraComp->GetAsset() != System)
			{
				continue;
			}

			FNiagaraSystemInstanceControllerPtr Controller =
				NiagaraComp->GetSystemInstanceController();
			if (!Controller.IsValid() || !Controller->IsValid())
			{
				continue;
			}

			FNiagaraSystemInstance* SysInstance = Controller->GetSystemInstance_Unsafe();
			if (!SysInstance)
			{
				continue;
			}

			bFoundInstance = true;

			TConstArrayView<FNiagaraEmitterInstanceRef> EmitterInstances =
				SysInstance->GetEmitters();

			for (const FNiagaraEmitterInstanceRef& EmitterInst : EmitterInstances)
			{
				const FNiagaraEmitterHandle& Handle = EmitterInst->GetEmitterHandle();
				FString EmitterName = Handle.GetName().ToString();

				// Filter by emitter name if specified
				if (!EmitterFilter.IsEmpty()
					&& !EmitterName.Equals(EmitterFilter, ESearchCase::IgnoreCase))
				{
					continue;
				}

				ENiagaraExecutionState ExecState = EmitterInst->GetExecutionState();
				int32 NumParticles = EmitterInst->GetNumParticles();
				int32 TotalSpawned = EmitterInst->GetTotalSpawnedParticles();
				bool bIsEnabled = Handle.GetIsEnabled();

				TSharedPtr<FJsonObject> EmitterObj = MakeShared<FJsonObject>();
				EmitterObj->SetStringField(TEXT("name"), EmitterName);
				EmitterObj->SetNumberField(TEXT("num_particles"), NumParticles);
				EmitterObj->SetNumberField(TEXT("total_spawned"), TotalSpawned);
				EmitterObj->SetStringField(TEXT("execution_state"),
					ExecutionStateToString(ExecState));
				EmitterObj->SetBoolField(TEXT("is_enabled"), bIsEnabled);
				EmitterObj->SetBoolField(TEXT("is_active"),
					ExecState == ENiagaraExecutionState::Active);

				// Get bounds if available
				FBox Bounds = NiagaraComp->Bounds.GetBox();
				if (Bounds.IsValid)
				{
					TSharedPtr<FJsonObject> BoundsObj = MakeShared<FJsonObject>();
					FVector Center = Bounds.GetCenter();
					FVector Extent = Bounds.GetExtent();
					BoundsObj->SetNumberField(TEXT("center_x"), Center.X);
					BoundsObj->SetNumberField(TEXT("center_y"), Center.Y);
					BoundsObj->SetNumberField(TEXT("center_z"), Center.Z);
					BoundsObj->SetNumberField(TEXT("extent_x"), Extent.X);
					BoundsObj->SetNumberField(TEXT("extent_y"), Extent.Y);
					BoundsObj->SetNumberField(TEXT("extent_z"), Extent.Z);
					EmitterObj->SetObjectField(TEXT("bounds"), BoundsObj);
				}

				EmittersArray.Add(MakeShared<FJsonValueObject>(EmitterObj));
			}

			// Only use the first matching component instance
			break;
		}

		if (bFoundInstance)
		{
			break;
		}
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("system_path"), SystemPath);

	if (!bFoundInstance)
	{
		Result->SetStringField(TEXT("message"),
			TEXT("No active preview instance found. Spawn or preview the system first."));
	}

	Result->SetNumberField(TEXT("emitter_count"), EmittersArray.Num());
	Result->SetArrayField(TEXT("emitters"), EmittersArray);

	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

// ---------------------------------------------------------------------------
// HandleSetNiagaraPlaybackRange
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleSetNiagaraPlaybackRange(
	const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
	FString SystemPath;
	if (!Params->TryGetStringField(TEXT("system_path"), SystemPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Missing required 'system_path' parameter"));
	}

	double RangeEnd = 0.0;
	if (!Params->TryGetNumberField(TEXT("range_end"), RangeEnd))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Missing required 'range_end' parameter"));
	}

	double RangeStart = 0.0;
	Params->TryGetNumberField(TEXT("range_start"), RangeStart);

	int32 FrameRateValue = 60;
	if (Params->HasField(TEXT("frame_rate")))
	{
		FrameRateValue = static_cast<int32>(Params->GetNumberField(TEXT("frame_rate")));
	}

	if (RangeEnd <= RangeStart)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("range_end must be greater than range_start"));
	}

	if (FrameRateValue <= 0)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("frame_rate must be positive"));
	}

	FString LoadError;
	UNiagaraSystem* System = NiagaraHelpers::LoadNiagaraSystem(SystemPath, LoadError);
	if (!System)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(LoadError);
	}

	UNiagaraEditorDataBase* EditorDataBase = System->GetEditorData();
	if (!EditorDataBase)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("System has no editor data"));
	}

	UNiagaraSystemEditorData* EditorData = Cast<UNiagaraSystemEditorData>(EditorDataBase);
	if (!EditorData)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Failed to cast editor data to UNiagaraSystemEditorData"));
	}

	float StartF = static_cast<float>(RangeStart);
	float EndF = static_cast<float>(RangeEnd);
	EditorData->SetPlaybackRange(TRange<float>(StartF, EndF));

	// SetPlaybackFrameRate not exported from NiagaraEditor — set via reflection
	FStructProperty* FRProp = CastField<FStructProperty>(
		UNiagaraSystemEditorData::StaticClass()->FindPropertyByName(TEXT("PlaybackFrameRate")));
	if (FRProp)
	{
		FFrameRate NewRate(FrameRateValue, 1);
		FRProp->CopyCompleteValue(
			FRProp->ContainerPtrToValuePtr<void>(EditorData), &NewRate);
	}

	System->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("system_path"), SystemPath);
	Result->SetNumberField(TEXT("range_start"), StartF);
	Result->SetNumberField(TEXT("range_end"), EndF);
	Result->SetNumberField(TEXT("frame_rate"), FrameRateValue);

	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
#else
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
		TEXT("set_niagara_playback_range requires editor builds (WITH_EDITORONLY_DATA)"));
#endif
}

// ---------------------------------------------------------------------------
// HandleGetNiagaraPlaybackRange
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleGetNiagaraPlaybackRange(
	const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
	FString SystemPath;
	if (!Params->TryGetStringField(TEXT("system_path"), SystemPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Missing required 'system_path' parameter"));
	}

	FString LoadError;
	UNiagaraSystem* System = NiagaraHelpers::LoadNiagaraSystem(SystemPath, LoadError);
	if (!System)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(LoadError);
	}

	UNiagaraEditorDataBase* EditorDataBase = System->GetEditorData();
	if (!EditorDataBase)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("System has no editor data"));
	}

	UNiagaraSystemEditorData* EditorData = Cast<UNiagaraSystemEditorData>(EditorDataBase);
	if (!EditorData)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Failed to cast editor data to UNiagaraSystemEditorData"));
	}

	TRange<float> PlaybackRange = EditorData->GetPlaybackRange();
	FFrameRate FrameRate = EditorData->GetPlaybackFrameRate();
	bool bLocked = EditorData->GetLockPlaybackFrameRate();

	float RangeMin = PlaybackRange.HasLowerBound()
		? PlaybackRange.GetLowerBoundValue()
		: 0.0f;
	float RangeMax = PlaybackRange.HasUpperBound()
		? PlaybackRange.GetUpperBoundValue()
		: 0.0f;

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("system_path"), SystemPath);
	Result->SetNumberField(TEXT("range_start"), RangeMin);
	Result->SetNumberField(TEXT("range_end"), RangeMax);
	Result->SetNumberField(TEXT("frame_rate"), FrameRate.Numerator);
	Result->SetNumberField(TEXT("frame_rate_denominator"), FrameRate.Denominator);
	Result->SetBoolField(TEXT("is_locked"), bLocked);

	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
#else
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
		TEXT("get_niagara_playback_range requires editor builds (WITH_EDITORONLY_DATA)"));
#endif
}

// ---------------------------------------------------------------------------
// HandleGetNiagaraModuleVersions
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleGetNiagaraModuleVersions(
	const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
	FString SystemPath;
	if (!Params->TryGetStringField(TEXT("system_path"), SystemPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Missing required 'system_path' parameter"));
	}

	FString EmitterName;
	if (!Params->TryGetStringField(TEXT("emitter_name"), EmitterName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Missing required 'emitter_name' parameter"));
	}

	FString Filter;
	Params->TryGetStringField(TEXT("filter"), Filter);

	FString LoadError;
	UNiagaraSystem* System = NiagaraHelpers::LoadNiagaraSystem(SystemPath, LoadError);
	if (!System)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(LoadError);
	}

	int32 EmitterIndex = INDEX_NONE;
	FString HandleError;
	FNiagaraEmitterHandle* Handle = NiagaraHelpers::FindEmitterHandle(
		System, EmitterName, EmitterIndex, HandleError);
	if (!Handle)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(HandleError);
	}

	FVersionedNiagaraEmitterData* EmitterData = NiagaraHelpers::GetEmitterData(Handle);
	if (!EmitterData)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Failed to get emitter data"));
	}

	// Collect module nodes from all script usage graphs
	TArray<TSharedPtr<FJsonValue>> ModulesArray;

	// Script usages to scan
	static const ENiagaraScriptUsage Usages[] = {
		ENiagaraScriptUsage::EmitterSpawnScript,
		ENiagaraScriptUsage::EmitterUpdateScript,
		ENiagaraScriptUsage::ParticleSpawnScript,
		ENiagaraScriptUsage::ParticleUpdateScript,
	};

	for (ENiagaraScriptUsage Usage : Usages)
	{
		UNiagaraGraph* Graph = NiagaraHelpers::GetGraphForUsage(EmitterData, Usage);
		if (!Graph)
		{
			continue;
		}

		FString UsageStr = NiagaraHelpers::ScriptUsageToString(Usage);

		// Find all function call nodes in the graph
		TArray<UNiagaraNodeFunctionCall*> FunctionNodes;
		Graph->GetNodesOfClass<UNiagaraNodeFunctionCall>(FunctionNodes);

		for (UNiagaraNodeFunctionCall* FuncNode : FunctionNodes)
		{
			if (!FuncNode)
			{
				continue;
			}

			FString ModuleName = FuncNode->GetFunctionName();

			// Apply optional filter (case-insensitive)
			if (!Filter.IsEmpty()
				&& !ModuleName.Contains(Filter, ESearchCase::IgnoreCase))
			{
				continue;
			}

			UNiagaraScript* FunctionScript = FuncNode->FunctionScript;

			TSharedPtr<FJsonObject> ModuleObj = MakeShared<FJsonObject>();
			ModuleObj->SetStringField(TEXT("name"), ModuleName);
			ModuleObj->SetStringField(TEXT("script_usage"), UsageStr);

			if (FunctionScript)
			{
				bool bVersioningEnabled = FunctionScript->IsVersioningEnabled();
				ModuleObj->SetBoolField(TEXT("versioning_enabled"), bVersioningEnabled);

				if (bVersioningEnabled)
				{
					FNiagaraAssetVersion ExposedVersion =
						FunctionScript->GetExposedVersion();
					TArray<FNiagaraAssetVersion> AllVersions =
						FunctionScript->GetAllAvailableVersions();

					// Format current version
					FString CurrentVersionStr = FString::Printf(
						TEXT("%d.%d"),
						ExposedVersion.MajorVersion,
						ExposedVersion.MinorVersion);
					ModuleObj->SetStringField(TEXT("current_version"), CurrentVersionStr);

					// Find the latest version (highest major, then highest minor)
					FNiagaraAssetVersion LatestVersion = ExposedVersion;
					for (const FNiagaraAssetVersion& Ver : AllVersions)
					{
						if (Ver.MajorVersion > LatestVersion.MajorVersion
							|| (Ver.MajorVersion == LatestVersion.MajorVersion
								&& Ver.MinorVersion > LatestVersion.MinorVersion))
						{
							LatestVersion = Ver;
						}
					}

					FString LatestVersionStr = FString::Printf(
						TEXT("%d.%d"),
						LatestVersion.MajorVersion,
						LatestVersion.MinorVersion);
					ModuleObj->SetStringField(TEXT("latest_version"), LatestVersionStr);

					bool bIsOutdated =
						(ExposedVersion.MajorVersion != LatestVersion.MajorVersion
						|| ExposedVersion.MinorVersion != LatestVersion.MinorVersion);
					ModuleObj->SetBoolField(TEXT("is_outdated"), bIsOutdated);

					// Serialize all available versions
					TArray<TSharedPtr<FJsonValue>> VersionsArray;
					for (const FNiagaraAssetVersion& Ver : AllVersions)
					{
						FString VerStr = FString::Printf(
							TEXT("%d.%d"), Ver.MajorVersion, Ver.MinorVersion);
						VersionsArray.Add(MakeShared<FJsonValueString>(VerStr));
					}
					ModuleObj->SetArrayField(TEXT("all_versions"), VersionsArray);
				}
				else
				{
					ModuleObj->SetBoolField(TEXT("is_outdated"), false);
					ModuleObj->SetStringField(TEXT("current_version"), TEXT("N/A"));
					ModuleObj->SetStringField(TEXT("latest_version"), TEXT("N/A"));
				}
			}
			else
			{
				ModuleObj->SetBoolField(TEXT("versioning_enabled"), false);
				ModuleObj->SetBoolField(TEXT("is_outdated"), false);
				ModuleObj->SetStringField(TEXT("current_version"), TEXT("N/A"));
				ModuleObj->SetStringField(TEXT("latest_version"), TEXT("N/A"));
			}

			ModulesArray.Add(MakeShared<FJsonValueObject>(ModuleObj));
		}
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("system_path"), SystemPath);
	Result->SetStringField(TEXT("emitter_name"), EmitterName);
	Result->SetNumberField(TEXT("module_count"), ModulesArray.Num());
	Result->SetArrayField(TEXT("modules"), ModulesArray);

	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
#else
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
		TEXT("get_niagara_module_versions requires editor builds (WITH_EDITORONLY_DATA)"));
#endif
}
