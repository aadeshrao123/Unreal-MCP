#include "NiagaraHelpers.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"

#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraComponent.h"
#include "NiagaraActor.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraCommon.h"
#include "EdGraphSchema_Niagara.h"

#if WITH_EDITORONLY_DATA
#include "NiagaraSystemEditorData.h"
#endif

#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

// ---------------------------------------------------------------------------
// Asset Loading
// ---------------------------------------------------------------------------

UNiagaraSystem* NiagaraHelpers::LoadNiagaraSystem(const FString& AssetPath, FString& OutError)
{
	if (AssetPath.IsEmpty())
	{
		OutError = TEXT("Asset path is empty");
		return nullptr;
	}

	UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *AssetPath);
	if (!System)
	{
		// Try with explicit class path
		FString FullPath = AssetPath;
		if (!FullPath.Contains(TEXT(".")))
		{
			// Convert "/Game/FX/NS_Fire" to "/Game/FX/NS_Fire.NS_Fire"
			FString AssetName = FPaths::GetBaseFilename(FullPath);
			FullPath = FullPath + TEXT(".") + AssetName;
		}
		System = LoadObject<UNiagaraSystem>(nullptr, *FullPath);
	}

	if (!System)
	{
		OutError = FString::Printf(TEXT("Failed to load Niagara System at '%s'"), *AssetPath);
	}
	return System;
}

UNiagaraEmitter* NiagaraHelpers::LoadNiagaraEmitter(const FString& AssetPath, FString& OutError)
{
	if (AssetPath.IsEmpty())
	{
		OutError = TEXT("Asset path is empty");
		return nullptr;
	}

	UNiagaraEmitter* Emitter = LoadObject<UNiagaraEmitter>(nullptr, *AssetPath);
	if (!Emitter)
	{
		FString FullPath = AssetPath;
		if (!FullPath.Contains(TEXT(".")))
		{
			FString AssetName = FPaths::GetBaseFilename(FullPath);
			FullPath = FullPath + TEXT(".") + AssetName;
		}
		Emitter = LoadObject<UNiagaraEmitter>(nullptr, *FullPath);
	}

	if (!Emitter)
	{
		OutError = FString::Printf(TEXT("Failed to load Niagara Emitter at '%s'"), *AssetPath);
	}
	return Emitter;
}

// ---------------------------------------------------------------------------
// Emitter Handle Lookup
// ---------------------------------------------------------------------------

FNiagaraEmitterHandle* NiagaraHelpers::FindEmitterHandle(
	UNiagaraSystem* System,
	const FString& EmitterName,
	int32& OutIndex,
	FString& OutError)
{
	if (!System)
	{
		OutError = TEXT("System is null");
		return nullptr;
	}

	TArray<FNiagaraEmitterHandle>& Handles = System->GetEmitterHandles();

	// Support "Emitter[N]" index notation
	if (EmitterName.StartsWith(TEXT("Emitter[")) && EmitterName.EndsWith(TEXT("]")))
	{
		FString IndexStr = EmitterName.Mid(8, EmitterName.Len() - 9);
		int32 Idx = FCString::Atoi(*IndexStr);
		if (Idx >= 0 && Idx < Handles.Num())
		{
			OutIndex = Idx;
			return &Handles[Idx];
		}
		OutError = FString::Printf(
			TEXT("Emitter index %d out of range (system has %d emitters)"),
			Idx, Handles.Num());
		return nullptr;
	}

	// Case-insensitive name search
	for (int32 i = 0; i < Handles.Num(); ++i)
	{
		if (Handles[i].GetName().ToString().Equals(EmitterName, ESearchCase::IgnoreCase))
		{
			OutIndex = i;
			return &Handles[i];
		}
		// Also check unique name
		if (Handles[i].GetUniqueInstanceName().Equals(EmitterName, ESearchCase::IgnoreCase))
		{
			OutIndex = i;
			return &Handles[i];
		}
	}

	OutError = FString::Printf(
		TEXT("Emitter '%s' not found in system (has %d emitters)"),
		*EmitterName, Handles.Num());
	return nullptr;
}

FVersionedNiagaraEmitterData* NiagaraHelpers::GetEmitterData(FNiagaraEmitterHandle* Handle)
{
	if (!Handle)
	{
		return nullptr;
	}
	return Handle->GetEmitterData();
}

// ---------------------------------------------------------------------------
// Script Usage Conversion
// ---------------------------------------------------------------------------

ENiagaraScriptUsage NiagaraHelpers::ParseScriptUsage(const FString& Usage, bool& bOutSuccess)
{
	bOutSuccess = true;

	if (Usage.Equals(TEXT("emitter_spawn"), ESearchCase::IgnoreCase))
	{
		return ENiagaraScriptUsage::EmitterSpawnScript;
	}
	if (Usage.Equals(TEXT("emitter_update"), ESearchCase::IgnoreCase))
	{
		return ENiagaraScriptUsage::EmitterUpdateScript;
	}
	if (Usage.Equals(TEXT("particle_spawn"), ESearchCase::IgnoreCase))
	{
		return ENiagaraScriptUsage::ParticleSpawnScript;
	}
	if (Usage.Equals(TEXT("particle_update"), ESearchCase::IgnoreCase))
	{
		return ENiagaraScriptUsage::ParticleUpdateScript;
	}
	if (Usage.Equals(TEXT("system_spawn"), ESearchCase::IgnoreCase))
	{
		return ENiagaraScriptUsage::SystemSpawnScript;
	}
	if (Usage.Equals(TEXT("system_update"), ESearchCase::IgnoreCase))
	{
		return ENiagaraScriptUsage::SystemUpdateScript;
	}

	bOutSuccess = false;
	return ENiagaraScriptUsage::ParticleUpdateScript;
}

FString NiagaraHelpers::ScriptUsageToString(ENiagaraScriptUsage Usage)
{
	switch (Usage)
	{
	case ENiagaraScriptUsage::EmitterSpawnScript:
		return TEXT("emitter_spawn");
	case ENiagaraScriptUsage::EmitterUpdateScript:
		return TEXT("emitter_update");
	case ENiagaraScriptUsage::ParticleSpawnScript:
		return TEXT("particle_spawn");
	case ENiagaraScriptUsage::ParticleUpdateScript:
		return TEXT("particle_update");
	case ENiagaraScriptUsage::SystemSpawnScript:
		return TEXT("system_spawn");
	case ENiagaraScriptUsage::SystemUpdateScript:
		return TEXT("system_update");
	default:
		return TEXT("unknown");
	}
}

// ---------------------------------------------------------------------------
// Graph Access
// ---------------------------------------------------------------------------

UNiagaraGraph* NiagaraHelpers::GetGraphForUsage(
	FVersionedNiagaraEmitterData* EmitterData,
	ENiagaraScriptUsage Usage)
{
	if (!EmitterData)
	{
		return nullptr;
	}

	UNiagaraScript* Script = nullptr;

	switch (Usage)
	{
	case ENiagaraScriptUsage::EmitterSpawnScript:
		Script = EmitterData->EmitterSpawnScriptProps.Script;
		break;
	case ENiagaraScriptUsage::EmitterUpdateScript:
		Script = EmitterData->EmitterUpdateScriptProps.Script;
		break;
	case ENiagaraScriptUsage::ParticleSpawnScript:
		Script = EmitterData->SpawnScriptProps.Script;
		break;
	case ENiagaraScriptUsage::ParticleUpdateScript:
		Script = EmitterData->UpdateScriptProps.Script;
		break;
	default:
		return nullptr;
	}

	if (!Script)
	{
		return nullptr;
	}

#if WITH_EDITORONLY_DATA
	UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(Script->GetLatestSource());
	if (Source)
	{
		return Source->NodeGraph;
	}
#endif

	return nullptr;
}

UNiagaraNodeOutput* NiagaraHelpers::GetOutputNodeForUsage(
	UNiagaraGraph* Graph,
	ENiagaraScriptUsage Usage)
{
	if (!Graph)
	{
		return nullptr;
	}

#if WITH_EDITORONLY_DATA
	TArray<UNiagaraNodeOutput*> OutputNodes;
	Graph->GetNodesOfClass<UNiagaraNodeOutput>(OutputNodes);

	for (UNiagaraNodeOutput* OutputNode : OutputNodes)
	{
		if (OutputNode->GetUsage() == Usage)
		{
			return OutputNode;
		}
	}
#endif

	return nullptr;
}

// ---------------------------------------------------------------------------
// Module Node Lookup
// ---------------------------------------------------------------------------

UNiagaraNodeFunctionCall* NiagaraHelpers::FindModuleNode(
	UNiagaraGraph* Graph,
	ENiagaraScriptUsage Usage,
	const FString& ModuleName,
	FString& OutError)
{
	if (!Graph)
	{
		OutError = TEXT("Graph is null");
		return nullptr;
	}

#if WITH_EDITORONLY_DATA
	TArray<UNiagaraNodeFunctionCall*> FunctionNodes;
	Graph->GetNodesOfClass<UNiagaraNodeFunctionCall>(FunctionNodes);

	for (UNiagaraNodeFunctionCall* Node : FunctionNodes)
	{
		FString NodeName = Node->GetFunctionName();
		if (NodeName.Equals(ModuleName, ESearchCase::IgnoreCase))
		{
			return Node;
		}

		// Also match display name
		FText DisplayName = Node->GetNodeTitle(ENodeTitleType::ListView);
		if (DisplayName.ToString().Equals(ModuleName, ESearchCase::IgnoreCase))
		{
			return Node;
		}
	}

	OutError = FString::Printf(TEXT("Module '%s' not found in graph"), *ModuleName);
#else
	OutError = TEXT("Module lookup requires editor data");
#endif

	return nullptr;
}

// ---------------------------------------------------------------------------
// Actor Lookup
// ---------------------------------------------------------------------------

ANiagaraActor* NiagaraHelpers::FindNiagaraActorByName(const FString& Name, FString& OutError)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		OutError = TEXT("No editor world available");
		return nullptr;
	}

	for (TActorIterator<ANiagaraActor> It(World); It; ++It)
	{
		ANiagaraActor* Actor = *It;
		if (Actor->GetActorLabel().Equals(Name, ESearchCase::IgnoreCase) ||
			Actor->GetName().Equals(Name, ESearchCase::IgnoreCase))
		{
			return Actor;
		}
	}

	OutError = FString::Printf(TEXT("Niagara actor '%s' not found in level"), *Name);
	return nullptr;
}

// ---------------------------------------------------------------------------
// Compilation & Editor Sync
// ---------------------------------------------------------------------------

void NiagaraHelpers::CompileAndSync(UNiagaraSystem* System, bool bForce)
{
	if (!System)
	{
		return;
	}

#if WITH_EDITORONLY_DATA
	UNiagaraSystemEditorData* EditorData = Cast<UNiagaraSystemEditorData>(System->GetEditorData());
	if (EditorData)
	{
		EditorData->SynchronizeOverviewGraphWithSystem(*System);
	}
#endif

	System->MarkPackageDirty();
	System->RequestCompile(bForce);

	if (bForce)
	{
		System->WaitForCompilationComplete();
	}

	System->PostEditChange();
}

// ---------------------------------------------------------------------------
// JSON Serialization
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> NiagaraHelpers::EmitterHandleToJson(
	const FNiagaraEmitterHandle& Handle,
	int32 Index)
{
	auto Obj = MakeShared<FJsonObject>();
	Obj->SetStringField(TEXT("name"), Handle.GetName().ToString());
	Obj->SetStringField(TEXT("unique_name"), Handle.GetUniqueInstanceName());
	Obj->SetStringField(TEXT("id"), Handle.GetId().ToString());
	Obj->SetNumberField(TEXT("index"), Index);
	Obj->SetBoolField(TEXT("enabled"), Handle.GetIsEnabled());

	FVersionedNiagaraEmitterData* Data = const_cast<FNiagaraEmitterHandle&>(Handle).GetEmitterData();
	if (Data)
	{
		FString SimTarget = (Data->SimTarget == ENiagaraSimTarget::GPUComputeSim)
			? TEXT("gpu") : TEXT("cpu");
		Obj->SetStringField(TEXT("sim_target"), SimTarget);
		Obj->SetBoolField(TEXT("local_space"), Data->bLocalSpace);
		Obj->SetBoolField(TEXT("determinism"), Data->bDeterminism);
		Obj->SetNumberField(TEXT("renderer_count"), Data->GetRenderers().Num());
	}

	return Obj;
}

TSharedPtr<FJsonObject> NiagaraHelpers::ModuleNodeToJson(
	UNiagaraNodeFunctionCall* Node,
	int32 Index,
	ENiagaraScriptUsage Usage,
	bool bIncludeInputs)
{
	auto Obj = MakeShared<FJsonObject>();
	if (!Node)
	{
		return Obj;
	}

	Obj->SetStringField(TEXT("name"), Node->GetFunctionName());
	Obj->SetNumberField(TEXT("index"), Index);
	Obj->SetStringField(TEXT("script_usage"), ScriptUsageToString(Usage));

#if WITH_EDITORONLY_DATA
	FText DisplayName = Node->GetNodeTitle(ENodeTitleType::ListView);
	Obj->SetStringField(TEXT("display_name"), DisplayName.ToString());

	if (Node->FunctionScript)
	{
		Obj->SetStringField(TEXT("script_path"), Node->FunctionScript->GetPathName());
	}

	if (bIncludeInputs)
	{
		TArray<TSharedPtr<FJsonValue>> InputsArray;
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin->Direction != EGPD_Input)
			{
				continue;
			}

			// Skip the parameter map pins — they are internal wiring
			if (Pin->PinType.PinCategory == UEdGraphSchema_Niagara::PinCategoryMisc)
			{
				continue;
			}

			auto InputObj = MakeShared<FJsonObject>();
			InputObj->SetStringField(TEXT("name"), Pin->PinName.ToString());
			InputObj->SetStringField(TEXT("type"), Pin->PinType.PinCategory.ToString());
			InputObj->SetStringField(TEXT("default_value"), Pin->DefaultValue);
			InputObj->SetBoolField(TEXT("is_connected"), Pin->LinkedTo.Num() > 0);

			InputsArray.Add(MakeShared<FJsonValueObject>(InputObj));
		}
		Obj->SetArrayField(TEXT("inputs"), InputsArray);
	}
#endif

	return Obj;
}
