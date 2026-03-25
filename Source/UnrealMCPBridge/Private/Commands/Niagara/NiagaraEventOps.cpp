#include "Commands/EpicUnrealMCPNiagaraCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "NiagaraHelpers.h"

#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraCommon.h"
#include "NiagaraTypes.h"

#if WITH_EDITORONLY_DATA
#include "NiagaraSimulationStageBase.h"
#include "NiagaraEditorUtilities.h"
#endif

// ---------------------------------------------------------------------------
// HandleAddNiagaraEventHandler
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleAddNiagaraEventHandler(
	const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
	FString SystemPath;
	if (!Params->TryGetStringField(TEXT("system_path"), SystemPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'system_path' parameter"));
	}

	FString EmitterName;
	if (!Params->TryGetStringField(TEXT("emitter_name"), EmitterName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'emitter_name' parameter"));
	}

	FString SourceEmitterName;
	Params->TryGetStringField(TEXT("source_emitter"), SourceEmitterName);

	FString EventName = TEXT("CollisionEvent");
	Params->TryGetStringField(TEXT("event_name"), EventName);

	FString ExecutionMode = TEXT("spawned_particles");
	Params->TryGetStringField(TEXT("execution_mode"), ExecutionMode);

	int32 MaxEventsPerFrame = 0;
	Params->TryGetNumberField(TEXT("max_events_per_frame"), MaxEventsPerFrame);

	bool bRandomSpawnNumber = false;
	Params->TryGetBoolField(TEXT("random_spawn_number"), bRandomSpawnNumber);

	int32 SpawnNumber = 1;
	Params->TryGetNumberField(TEXT("spawn_number"), SpawnNumber);

	FString Error;
	UNiagaraSystem* System = NiagaraHelpers::LoadNiagaraSystem(SystemPath, Error);
	if (!System)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	int32 EmitterIdx = INDEX_NONE;
	FNiagaraEmitterHandle* Handle = NiagaraHelpers::FindEmitterHandle(System, EmitterName, EmitterIdx, Error);
	if (!Handle)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	FVersionedNiagaraEmitterData* EmitterData = NiagaraHelpers::GetEmitterData(Handle);
	if (!EmitterData)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Emitter has no data"));
	}

	// Build the event handler properties
	FNiagaraEventScriptProperties EventProps;
	EventProps.Script = NewObject<UNiagaraScript>(Handle->GetInstance().Emitter, FName(TEXT("EventScript")), RF_Transactional);
	EventProps.Script->SetUsage(ENiagaraScriptUsage::ParticleEventScript);

	// Set the source event ID by name
	EventProps.SourceEventName = FName(*EventName);

	// Resolve execution mode
	FString LowerMode = ExecutionMode.ToLower();
	if (LowerMode == TEXT("spawned_particles") || LowerMode == TEXT("spawn"))
	{
		EventProps.ExecutionMode = EScriptExecutionMode::SpawnedParticles;
	}
	else if (LowerMode == TEXT("every_particle") || LowerMode == TEXT("all"))
	{
		EventProps.ExecutionMode = EScriptExecutionMode::EveryParticle;
	}
	else if (LowerMode == TEXT("single_particle") || LowerMode == TEXT("single"))
	{
		EventProps.ExecutionMode = EScriptExecutionMode::SingleParticle;
	}
	else
	{
		EventProps.ExecutionMode = EScriptExecutionMode::SpawnedParticles;
	}

	EventProps.MaxEventsPerFrame = static_cast<uint32>(MaxEventsPerFrame);
	EventProps.bRandomSpawnNumber = bRandomSpawnNumber;
	EventProps.SpawnNumber = static_cast<uint32>(SpawnNumber);

	// Resolve source emitter ID if provided
	if (!SourceEmitterName.IsEmpty())
	{
		int32 SourceIdx = INDEX_NONE;
		FNiagaraEmitterHandle* SourceHandle = NiagaraHelpers::FindEmitterHandle(
			System, SourceEmitterName, SourceIdx, Error);
		if (SourceHandle)
		{
			EventProps.SourceEmitterID = SourceHandle->GetId();
		}
	}

	// Add the event handler to the emitter
	EmitterData->EventHandlerScriptProps.Add(EventProps);

	NiagaraHelpers::CompileAndSync(System);

	int32 NewIndex = EmitterData->EventHandlerScriptProps.Num() - 1;

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("emitter_name"), EmitterName);
	Result->SetStringField(TEXT("event_name"), EventName);
	Result->SetStringField(TEXT("execution_mode"), ExecutionMode);
	Result->SetNumberField(TEXT("event_handler_index"), NewIndex);
	Result->SetNumberField(TEXT("total_handlers"), EmitterData->EventHandlerScriptProps.Num());
	return Result;
#else
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only operation"));
#endif
}

// ---------------------------------------------------------------------------
// HandleAddNiagaraSimulationStage
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleAddNiagaraSimulationStage(
	const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
	FString SystemPath;
	if (!Params->TryGetStringField(TEXT("system_path"), SystemPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'system_path' parameter"));
	}

	FString EmitterName;
	if (!Params->TryGetStringField(TEXT("emitter_name"), EmitterName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'emitter_name' parameter"));
	}

	FString StageName = TEXT("SimulationStage");
	Params->TryGetStringField(TEXT("stage_name"), StageName);

	FString IterationSource = TEXT("particles");
	Params->TryGetStringField(TEXT("iteration_source"), IterationSource);

	int32 NumIterations = 1;
	Params->TryGetNumberField(TEXT("num_iterations"), NumIterations);

	bool bEnabled = true;
	Params->TryGetBoolField(TEXT("enabled"), bEnabled);

	FString Error;
	UNiagaraSystem* System = NiagaraHelpers::LoadNiagaraSystem(SystemPath, Error);
	if (!System)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	int32 EmitterIdx = INDEX_NONE;
	FNiagaraEmitterHandle* Handle = NiagaraHelpers::FindEmitterHandle(System, EmitterName, EmitterIdx, Error);
	if (!Handle)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	FVersionedNiagaraEmitterData* EmitterData = NiagaraHelpers::GetEmitterData(Handle);
	if (!EmitterData)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Emitter has no data"));
	}

	UNiagaraEmitter* Emitter = Handle->GetInstance().Emitter;
	if (!Emitter)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get emitter instance"));
	}

	// Create the simulation stage
	UNiagaraSimulationStageGeneric* NewStage = NewObject<UNiagaraSimulationStageGeneric>(
		Emitter, FName(*StageName));

	if (!NewStage)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create simulation stage"));
	}

	NewStage->SimulationStageName = FName(*StageName);
	NewStage->bEnabled = bEnabled;
	// NumIterations is FNiagaraParameterBindingWithValue — set via raw bytes
	{
		FNiagaraTypeDefinition IntDef = FNiagaraTypeDefinition::GetIntDef();
		FNiagaraVariableBase IntVar(IntDef, FName(TEXT("NumIterations")));
		NewStage->NumIterations.SetDefaultParameter(IntVar, NumIterations);
	}

	// Set iteration source
	FString LowerSource = IterationSource.ToLower();
	if (LowerSource == TEXT("particles") || LowerSource == TEXT("particle"))
	{
		NewStage->IterationSource = ENiagaraIterationSource::Particles;
	}
	else if (LowerSource == TEXT("data_interface") || LowerSource == TEXT("datainterface"))
	{
		NewStage->IterationSource = ENiagaraIterationSource::DataInterface;
	}
	else
	{
		NewStage->IterationSource = ENiagaraIterationSource::Particles;
	}

	// Add to emitter — AddSimulationStage is on UNiagaraEmitter, not the data struct
	Emitter->AddSimulationStage(NewStage, EmitterData->Version.VersionGuid);

	NiagaraHelpers::CompileAndSync(System);

	int32 NewIndex = EmitterData->GetSimulationStages().Num() - 1;

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("emitter_name"), EmitterName);
	Result->SetStringField(TEXT("stage_name"), StageName);
	Result->SetStringField(TEXT("iteration_source"), IterationSource);
	Result->SetNumberField(TEXT("num_iterations"), NumIterations);
	Result->SetNumberField(TEXT("stage_index"), NewIndex);
	Result->SetNumberField(TEXT("total_stages"), EmitterData->GetSimulationStages().Num());
	return Result;
#else
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only operation"));
#endif
}

// ---------------------------------------------------------------------------
// HandleGetNiagaraEventHandlers
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleGetNiagaraEventHandlers(
	const TSharedPtr<FJsonObject>& Params)
{
	FString SystemPath;
	if (!Params->TryGetStringField(TEXT("system_path"), SystemPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'system_path' parameter"));
	}

	FString EmitterName;
	if (!Params->TryGetStringField(TEXT("emitter_name"), EmitterName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'emitter_name' parameter"));
	}

	FString Error;
	UNiagaraSystem* System = NiagaraHelpers::LoadNiagaraSystem(SystemPath, Error);
	if (!System)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	int32 EmitterIdx = INDEX_NONE;
	FNiagaraEmitterHandle* Handle = NiagaraHelpers::FindEmitterHandle(System, EmitterName, EmitterIdx, Error);
	if (!Handle)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	FVersionedNiagaraEmitterData* EmitterData = NiagaraHelpers::GetEmitterData(Handle);
	if (!EmitterData)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Emitter has no data"));
	}

	// Serialize event handlers
	TArray<TSharedPtr<FJsonValue>> HandlersArr;
	for (int32 i = 0; i < EmitterData->EventHandlerScriptProps.Num(); ++i)
	{
		const FNiagaraEventScriptProperties& EventProps = EmitterData->EventHandlerScriptProps[i];

		auto HandlerObj = MakeShared<FJsonObject>();
		HandlerObj->SetNumberField(TEXT("index"), i);
		HandlerObj->SetStringField(TEXT("event_name"), EventProps.SourceEventName.ToString());

		// Execution mode
		FString ModeName;
		switch (EventProps.ExecutionMode)
		{
		case EScriptExecutionMode::SpawnedParticles:
			ModeName = TEXT("spawned_particles");
			break;
		case EScriptExecutionMode::EveryParticle:
			ModeName = TEXT("every_particle");
			break;
		case EScriptExecutionMode::SingleParticle:
			ModeName = TEXT("single_particle");
			break;
		default:
			ModeName = TEXT("unknown");
			break;
		}
		HandlerObj->SetStringField(TEXT("execution_mode"), ModeName);

		HandlerObj->SetNumberField(TEXT("max_events_per_frame"), static_cast<int32>(EventProps.MaxEventsPerFrame));
		HandlerObj->SetNumberField(TEXT("spawn_number"), static_cast<int32>(EventProps.SpawnNumber));
		HandlerObj->SetBoolField(TEXT("random_spawn_number"), EventProps.bRandomSpawnNumber);

		// Source emitter ID
		FString SourceEmitterStr = EventProps.SourceEmitterID.IsValid()
			? EventProps.SourceEmitterID.ToString()
			: TEXT("");

		// Try to resolve source emitter name
		if (EventProps.SourceEmitterID.IsValid())
		{
			for (const FNiagaraEmitterHandle& OtherHandle : System->GetEmitterHandles())
			{
				if (OtherHandle.GetId() == EventProps.SourceEmitterID)
				{
					SourceEmitterStr = OtherHandle.GetName().ToString();
					break;
				}
			}
		}
		HandlerObj->SetStringField(TEXT("source_emitter"), SourceEmitterStr);

		HandlersArr.Add(MakeShared<FJsonValueObject>(HandlerObj));
	}

	// Also serialize simulation stages
	TArray<TSharedPtr<FJsonValue>> StagesArr;
	const TArray<UNiagaraSimulationStageBase*>& SimStages = EmitterData->GetSimulationStages();
	for (int32 i = 0; i < SimStages.Num(); ++i)
	{
		UNiagaraSimulationStageBase* Stage = SimStages[i];
		if (!Stage)
		{
			continue;
		}

		auto StageObj = MakeShared<FJsonObject>();
		StageObj->SetNumberField(TEXT("index"), i);
		StageObj->SetStringField(TEXT("name"), Stage->GetName());
		StageObj->SetBoolField(TEXT("enabled"), Stage->bEnabled);

		UNiagaraSimulationStageGeneric* GenericStage = Cast<UNiagaraSimulationStageGeneric>(Stage);
		if (GenericStage)
		{
			StageObj->SetStringField(TEXT("simulation_stage_name"), GenericStage->SimulationStageName.ToString());
			// NumIterations is FNiagaraParameterBindingWithValue — read via GetDefaultValue<int32>
			int32 NumIter = GenericStage->NumIterations.GetDefaultValue<int32>();
			StageObj->SetNumberField(TEXT("num_iterations"), (double)NumIter);

			FString IterSourceStr;
			switch (GenericStage->IterationSource)
			{
			case ENiagaraIterationSource::Particles:
				IterSourceStr = TEXT("particles");
				break;
			case ENiagaraIterationSource::DataInterface:
				IterSourceStr = TEXT("data_interface");
				break;
			default:
				IterSourceStr = TEXT("unknown");
				break;
			}
			StageObj->SetStringField(TEXT("iteration_source"), IterSourceStr);
		}

		StagesArr.Add(MakeShared<FJsonValueObject>(StageObj));
	}

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("emitter_name"), EmitterName);
	Result->SetArrayField(TEXT("event_handlers"), HandlersArr);
	Result->SetNumberField(TEXT("event_handler_count"), HandlersArr.Num());
	Result->SetArrayField(TEXT("simulation_stages"), StagesArr);
	Result->SetNumberField(TEXT("simulation_stage_count"), StagesArr.Num());
	return Result;
}
