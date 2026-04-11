#include "EditorAssetLibrary.h"
#include "Commands/EpicUnrealMCPNiagaraCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "NiagaraHelpers.h"
#include "NiagaraPropertyIntrospection.h"
#include "NiagaraTypeHelpers.h"

#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeInput.h"
#include "NiagaraNodeCustomHlsl.h"
#include "NiagaraCommon.h"
#include "NiagaraTypes.h"
#include "NiagaraScriptVariable.h"

#include "EdGraph/EdGraphPin.h"
#include "ScopedTransaction.h"

#if WITH_EDITORONLY_DATA
#include "NiagaraSystemEditorData.h"
#include "NiagaraEditorModule.h"
#include "EdGraphSchema_Niagara.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#include "ViewModels/Stack/NiagaraStackFunctionInput.h"
#include "ViewModels/Stack/NiagaraParameterHandle.h"
#include "ViewModels/NiagaraSystemViewModel.h"
#include "ViewModels/NiagaraEmitterHandleViewModel.h"
#include "ViewModels/Stack/NiagaraStackViewModel.h"
#include "ViewModels/Stack/NiagaraStackModuleItem.h"
#endif

// ---------------------------------------------------------------------------
// Local helpers
// ---------------------------------------------------------------------------

/**
 * Remove nodes wired to an override pin and break the links.
 * Replacement for the unexported RemoveNodesForStackFunctionInputOverridePin.
 */
static void RemoveOverridePinConnections(UEdGraphPin& OverridePin, UNiagaraGraph* Graph)
{
	if (OverridePin.LinkedTo.Num() == 0)
	{
		return;
	}

	TArray<UEdGraphNode*> NodesToRemove;
	for (UEdGraphPin* LinkedPin : OverridePin.LinkedTo)
	{
		if (LinkedPin && LinkedPin->GetOwningNode())
		{
			NodesToRemove.AddUnique(LinkedPin->GetOwningNode());
		}
	}

	OverridePin.BreakAllPinLinks();

	for (UEdGraphNode* NodeToRemove : NodesToRemove)
	{
		if (!NodeToRemove || !Graph)
		{
			continue;
		}

		for (UEdGraphPin* Pin : NodeToRemove->Pins)
		{
			if (Pin)
			{
				Pin->BreakAllPinLinks();
			}
		}
		Graph->RemoveNode(NodeToRemove);
	}
}

/**
 * Walk the module chain connected to an output node and collect
 * UNiagaraNodeFunctionCall nodes in execution order.
 * Index 0 = first module in execution order (furthest from output node).
 * Last index = closest to output node (executes last).
 */
static void CollectModuleChain(
	UNiagaraNodeOutput* OutputNode,
	TArray<UNiagaraNodeFunctionCall*>& OutChain)
{
	if (!OutputNode)
	{
		return;
	}

	// The output node has one input pin wired to the module chain
	UEdGraphPin* CurrentInput = nullptr;
	for (UEdGraphPin* Pin : OutputNode->Pins)
	{
		if (Pin->Direction == EGPD_Input)
		{
			CurrentInput = Pin;
			break;
		}
	}

	// Walk upstream through the chain
	while (CurrentInput && CurrentInput->LinkedTo.Num() > 0)
	{
		UEdGraphPin* UpstreamPin = CurrentInput->LinkedTo[0];
		if (!UpstreamPin)
		{
			break;
		}

		UEdGraphNode* UpstreamNode = UpstreamPin->GetOwningNode();
		UNiagaraNodeFunctionCall* FuncNode = Cast<UNiagaraNodeFunctionCall>(UpstreamNode);
		if (FuncNode)
		{
			OutChain.Add(FuncNode);
		}

		// Walk further upstream via the function node's input pin
		CurrentInput = nullptr;
		if (UpstreamNode)
		{
			for (UEdGraphPin* Pin : UpstreamNode->Pins)
			{
				if (Pin->Direction == EGPD_Input)
				{
					CurrentInput = Pin;
					break;
				}
			}
		}
	}

	// Reverse so index 0 = first in execution order
	Algo::Reverse(OutChain);
}

/**
 * Find the first pin on a node with the given direction.
 */
static UEdGraphPin* FindFirstPin(UEdGraphNode* Node, EEdGraphPinDirection Direction)
{
	if (!Node)
	{
		return nullptr;
	}
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin->Direction == Direction)
		{
			return Pin;
		}
	}
	return nullptr;
}

// ---------------------------------------------------------------------------
// HandleGetNiagaraModules
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleGetNiagaraModules(
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

	FString ScriptUsageFilter = TEXT("all");
	Params->TryGetStringField(TEXT("script_usage"), ScriptUsageFilter);

	FString Filter;
	Params->TryGetStringField(TEXT("filter"), Filter);

	bool bIncludeInputs = true;
	Params->TryGetBoolField(TEXT("include_inputs"), bIncludeInputs);

	// Load system
	FString LoadError;
	UNiagaraSystem* System = NiagaraHelpers::LoadNiagaraSystem(SystemPath, LoadError);
	if (!System)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(LoadError);
	}

	// Find emitter
	int32 EmitterIdx = INDEX_NONE;
	FString EmitterError;
	FNiagaraEmitterHandle* Handle = NiagaraHelpers::FindEmitterHandle(
		System, EmitterName, EmitterIdx, EmitterError);
	if (!Handle)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(EmitterError);
	}

	FVersionedNiagaraEmitterData* EmitterData = NiagaraHelpers::GetEmitterData(Handle);
	if (!EmitterData)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Emitter has no data"));
	}

	// Determine which script usages to query
	TArray<ENiagaraScriptUsage> Usages;
	if (ScriptUsageFilter.Equals(TEXT("all"), ESearchCase::IgnoreCase))
	{
		Usages.Add(ENiagaraScriptUsage::EmitterSpawnScript);
		Usages.Add(ENiagaraScriptUsage::EmitterUpdateScript);
		Usages.Add(ENiagaraScriptUsage::ParticleSpawnScript);
		Usages.Add(ENiagaraScriptUsage::ParticleUpdateScript);
	}
	else
	{
		bool bOk = false;
		ENiagaraScriptUsage Usage = NiagaraHelpers::ParseScriptUsage(ScriptUsageFilter, bOk);
		if (!bOk)
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Invalid script_usage: '%s'"), *ScriptUsageFilter));
		}
		Usages.Add(Usage);
	}

	TArray<TSharedPtr<FJsonValue>> ModulesArray;

	for (ENiagaraScriptUsage Usage : Usages)
	{
		UNiagaraGraph* Graph = NiagaraHelpers::GetGraphForUsage(EmitterData, Usage);
		if (!Graph)
		{
			continue;
		}

		UNiagaraNodeOutput* OutputNode = NiagaraHelpers::GetOutputNodeForUsage(Graph, Usage);
		if (!OutputNode)
		{
			continue;
		}

		// Walk the chain to get modules in execution order
		TArray<UNiagaraNodeFunctionCall*> Chain;
		CollectModuleChain(OutputNode, Chain);

		for (int32 i = 0; i < Chain.Num(); ++i)
		{
			if (!Filter.IsEmpty() &&
				!Chain[i]->GetFunctionName().Contains(Filter, ESearchCase::IgnoreCase))
			{
				continue;
			}
			TSharedPtr<FJsonObject> ModuleJson = NiagaraHelpers::ModuleNodeToJson(
				Chain[i], i, Usage, bIncludeInputs);
			ModulesArray.Add(MakeShared<FJsonValueObject>(ModuleJson));
		}
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("system_path"), SystemPath);
	Data->SetStringField(TEXT("emitter_name"), EmitterName);
	Data->SetArrayField(TEXT("modules"), ModulesArray);
	Data->SetNumberField(TEXT("count"), ModulesArray.Num());

	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Data);
#else
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only operation"));
#endif
}

// ---------------------------------------------------------------------------
// HandleAddNiagaraModule
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleAddNiagaraModule(
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

	FString ModulePath;
	if (!Params->TryGetStringField(TEXT("module_path"), ModulePath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'module_path' parameter"));
	}

	FString ScriptUsageStr;
	if (!Params->TryGetStringField(TEXT("script_usage"), ScriptUsageStr))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'script_usage' parameter"));
	}

	int32 TargetIndex = INDEX_NONE;
	{
		double IndexD = -1;
		if (Params->TryGetNumberField(TEXT("index"), IndexD))
		{
			TargetIndex = static_cast<int32>(IndexD);
		}
	}

	// Parse usage
	bool bUsageOk = false;
	ENiagaraScriptUsage Usage = NiagaraHelpers::ParseScriptUsage(ScriptUsageStr, bUsageOk);
	if (!bUsageOk)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Invalid script_usage: '%s'"), *ScriptUsageStr));
	}

	// Load system
	FString LoadError;
	UNiagaraSystem* System = NiagaraHelpers::LoadNiagaraSystem(SystemPath, LoadError);
	if (!System)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(LoadError);
	}

	// Find emitter
	int32 EmitterIdx = INDEX_NONE;
	FString EmitterError;
	FNiagaraEmitterHandle* Handle = NiagaraHelpers::FindEmitterHandle(
		System, EmitterName, EmitterIdx, EmitterError);
	if (!Handle)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(EmitterError);
	}

	FVersionedNiagaraEmitterData* EmitterData = NiagaraHelpers::GetEmitterData(Handle);
	if (!EmitterData)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Emitter has no data"));
	}

	// Load module script
	UNiagaraScript* ModuleScript = LoadObject<UNiagaraScript>(nullptr, *ModulePath);
	if (!ModuleScript)
	{
		// Try appending the asset name
		FString FullPath = ModulePath;
		if (!FullPath.Contains(TEXT(".")))
		{
			FString AssetName = FPaths::GetBaseFilename(FullPath);
			FullPath = FullPath + TEXT(".") + AssetName;
		}
		ModuleScript = LoadObject<UNiagaraScript>(nullptr, *FullPath);
	}
	if (!ModuleScript)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Failed to load module script: %s"), *ModulePath));
	}

	// Get graph and output node
	UNiagaraGraph* Graph = NiagaraHelpers::GetGraphForUsage(EmitterData, Usage);
	if (!Graph)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No graph for the given script usage"));
	}

	UNiagaraNodeOutput* OutputNode = NiagaraHelpers::GetOutputNodeForUsage(Graph, Usage);
	if (!OutputNode)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("No output node for usage: %s"), *ScriptUsageStr));
	}

	FScopedTransaction Transaction(
		NSLOCTEXT("UnrealMCPBridge", "AddNiagaraModule", "Add Niagara Module"));

	// Use the canonical Niagara stack-graph helper instead of manually
	// allocating the function call node. AddScriptModuleToStack handles:
	//   * function call node creation with the right outer
	//   * full signature resolution (so per-Module.* input pins appear)
	//   * stack chain insertion / append at the requested index
	//   * suggested name + version handling
	// This is the same path the editor takes when you drag a module onto an
	// emitter stack — without it, AllocateDefaultPins gives you only the
	// generic InputMap pin and Module.* inputs never become configurable.
	FString SuggestedName;
	Params->TryGetStringField(TEXT("suggested_name"), SuggestedName);
	UNiagaraNodeFunctionCall* NewNode = FNiagaraStackGraphUtilities::AddScriptModuleToStack(
		ModuleScript,
		*OutputNode,
		TargetIndex,
		SuggestedName,
		FGuid());

	if (!NewNode)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("FNiagaraStackGraphUtilities::AddScriptModuleToStack returned null"));
	}

	Graph->NotifyGraphChanged();
	NiagaraHelpers::CompileAndSync(System);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("system_path"), SystemPath);
	Data->SetStringField(TEXT("emitter_name"), EmitterName);
	Data->SetStringField(TEXT("module_name"), NewNode->GetFunctionName());
	Data->SetStringField(TEXT("module_path"), ModulePath);
	Data->SetStringField(TEXT("script_usage"), ScriptUsageStr);

	// Report what inputs the new function call node exposes — useful for
	// confirming Module.* parameters are visible to set_niagara_module_input.
	TArray<TSharedPtr<FJsonValue>> InputPinNames;
	for (UEdGraphPin* Pin : NewNode->Pins)
	{
		if (!Pin || Pin->Direction != EGPD_Input) continue;
		if (Pin->PinType.PinCategory == UEdGraphSchema_Niagara::PinCategoryMisc) continue;
		InputPinNames.Add(MakeShared<FJsonValueString>(Pin->PinName.ToString()));
	}
	Data->SetArrayField(TEXT("input_pins"), InputPinNames);

	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Data);
#else
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only operation"));
#endif
}

// ---------------------------------------------------------------------------
// HandleRemoveNiagaraModule
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleRemoveNiagaraModule(
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

	FString ModuleName;
	if (!Params->TryGetStringField(TEXT("module_name"), ModuleName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'module_name' parameter"));
	}

	FString ScriptUsageStr;
	if (!Params->TryGetStringField(TEXT("script_usage"), ScriptUsageStr))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'script_usage' parameter"));
	}

	bool bUsageOk = false;
	ENiagaraScriptUsage Usage = NiagaraHelpers::ParseScriptUsage(ScriptUsageStr, bUsageOk);
	if (!bUsageOk)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Invalid script_usage: '%s'"), *ScriptUsageStr));
	}

	// Load system
	FString LoadError;
	UNiagaraSystem* System = NiagaraHelpers::LoadNiagaraSystem(SystemPath, LoadError);
	if (!System)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(LoadError);
	}

	// Find emitter
	int32 EmitterIdx = INDEX_NONE;
	FString EmitterError;
	FNiagaraEmitterHandle* Handle = NiagaraHelpers::FindEmitterHandle(
		System, EmitterName, EmitterIdx, EmitterError);
	if (!Handle)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(EmitterError);
	}

	FVersionedNiagaraEmitterData* EmitterData = NiagaraHelpers::GetEmitterData(Handle);
	if (!EmitterData)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Emitter has no data"));
	}

	UNiagaraGraph* Graph = NiagaraHelpers::GetGraphForUsage(EmitterData, Usage);
	if (!Graph)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No graph for the given script usage"));
	}

	// Find the module node
	FString FindError;
	UNiagaraNodeFunctionCall* ModuleNode = NiagaraHelpers::FindModuleNode(
		Graph, Usage, ModuleName, FindError);
	if (!ModuleNode)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FindError);
	}

	FScopedTransaction Transaction(
		NSLOCTEXT("UnrealMCPBridge", "RemoveNiagaraModule", "Remove Niagara Module"));

	// Splice the chain around the removed node to keep the pipeline intact
	UEdGraphPin* NodeInput = FindFirstPin(ModuleNode, EGPD_Input);
	UEdGraphPin* NodeOutput = FindFirstPin(ModuleNode, EGPD_Output);

	UEdGraphPin* UpstreamOutput =
		(NodeInput && NodeInput->LinkedTo.Num() > 0) ? NodeInput->LinkedTo[0] : nullptr;
	UEdGraphPin* DownstreamInput =
		(NodeOutput && NodeOutput->LinkedTo.Num() > 0) ? NodeOutput->LinkedTo[0] : nullptr;

	// Break all links on the node
	for (UEdGraphPin* Pin : ModuleNode->Pins)
	{
		if (Pin)
		{
			Pin->BreakAllPinLinks();
		}
	}

	// Reconnect the chain around the removed node
	if (UpstreamOutput && DownstreamInput)
	{
		UpstreamOutput->MakeLinkTo(DownstreamInput);
	}

	Graph->RemoveNode(ModuleNode);
	Graph->NotifyGraphChanged();
	NiagaraHelpers::CompileAndSync(System);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("removed_module"), ModuleName);
	Data->SetStringField(TEXT("emitter_name"), EmitterName);
	Data->SetStringField(TEXT("script_usage"), ScriptUsageStr);

	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Data);
#else
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only operation"));
#endif
}

// ---------------------------------------------------------------------------
// HandleSetNiagaraModuleEnabled
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleSetNiagaraModuleEnabled(
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

	FString ModuleName;
	if (!Params->TryGetStringField(TEXT("module_name"), ModuleName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'module_name' parameter"));
	}

	FString ScriptUsageStr;
	if (!Params->TryGetStringField(TEXT("script_usage"), ScriptUsageStr))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'script_usage' parameter"));
	}

	bool bEnabled = true;
	Params->TryGetBoolField(TEXT("enabled"), bEnabled);

	bool bUsageOk = false;
	ENiagaraScriptUsage Usage = NiagaraHelpers::ParseScriptUsage(ScriptUsageStr, bUsageOk);
	if (!bUsageOk)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Invalid script_usage: '%s'"), *ScriptUsageStr));
	}

	// Load system
	FString LoadError;
	UNiagaraSystem* System = NiagaraHelpers::LoadNiagaraSystem(SystemPath, LoadError);
	if (!System)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(LoadError);
	}

	// Find emitter
	int32 EmitterIdx = INDEX_NONE;
	FString EmitterError;
	FNiagaraEmitterHandle* Handle = NiagaraHelpers::FindEmitterHandle(
		System, EmitterName, EmitterIdx, EmitterError);
	if (!Handle)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(EmitterError);
	}

	FVersionedNiagaraEmitterData* EmitterData = NiagaraHelpers::GetEmitterData(Handle);
	if (!EmitterData)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Emitter has no data"));
	}

	UNiagaraGraph* Graph = NiagaraHelpers::GetGraphForUsage(EmitterData, Usage);
	if (!Graph)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No graph for the given script usage"));
	}

	FString FindError;
	UNiagaraNodeFunctionCall* ModuleNode = NiagaraHelpers::FindModuleNode(
		Graph, Usage, ModuleName, FindError);
	if (!ModuleNode)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FindError);
	}

	FScopedTransaction Transaction(
		NSLOCTEXT("UnrealMCPBridge", "SetNiagaraModuleEnabled", "Set Module Enabled"));

	ModuleNode->SetEnabledState(
		bEnabled ? ENodeEnabledState::Enabled : ENodeEnabledState::Disabled,
		false);
	ModuleNode->MarkNodeRequiresSynchronization(TEXT("Module enabled state changed"), true);

	Graph->NotifyGraphChanged();
	NiagaraHelpers::CompileAndSync(System);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("module_name"), ModuleName);
	Data->SetBoolField(TEXT("enabled"), bEnabled);

	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Data);
#else
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only operation"));
#endif
}

// ---------------------------------------------------------------------------
// HandleReorderNiagaraModule
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleReorderNiagaraModule(
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

	FString ModuleName;
	if (!Params->TryGetStringField(TEXT("module_name"), ModuleName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'module_name' parameter"));
	}

	FString ScriptUsageStr;
	if (!Params->TryGetStringField(TEXT("script_usage"), ScriptUsageStr))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'script_usage' parameter"));
	}

	double NewIndexD = 0;
	if (!Params->TryGetNumberField(TEXT("new_index"), NewIndexD))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'new_index' parameter"));
	}
	int32 NewIndex = static_cast<int32>(NewIndexD);

	bool bUsageOk = false;
	ENiagaraScriptUsage Usage = NiagaraHelpers::ParseScriptUsage(ScriptUsageStr, bUsageOk);
	if (!bUsageOk)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Invalid script_usage: '%s'"), *ScriptUsageStr));
	}

	// Load system
	FString LoadError;
	UNiagaraSystem* System = NiagaraHelpers::LoadNiagaraSystem(SystemPath, LoadError);
	if (!System)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(LoadError);
	}

	// Find emitter
	int32 EmitterIdx = INDEX_NONE;
	FString EmitterError;
	FNiagaraEmitterHandle* Handle = NiagaraHelpers::FindEmitterHandle(
		System, EmitterName, EmitterIdx, EmitterError);
	if (!Handle)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(EmitterError);
	}

	FVersionedNiagaraEmitterData* EmitterData = NiagaraHelpers::GetEmitterData(Handle);
	if (!EmitterData)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Emitter has no data"));
	}

	UNiagaraGraph* Graph = NiagaraHelpers::GetGraphForUsage(EmitterData, Usage);
	if (!Graph)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No graph for the given script usage"));
	}

	UNiagaraNodeOutput* OutputNode = NiagaraHelpers::GetOutputNodeForUsage(Graph, Usage);
	if (!OutputNode)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No output node found"));
	}

	// Get the current chain
	TArray<UNiagaraNodeFunctionCall*> Chain;
	CollectModuleChain(OutputNode, Chain);

	if (Chain.Num() < 2)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Not enough modules in chain to reorder"));
	}

	// Find the node to move
	int32 OldIndex = INDEX_NONE;
	UNiagaraNodeFunctionCall* TargetNode = nullptr;
	for (int32 i = 0; i < Chain.Num(); ++i)
	{
		FString NodeName = Chain[i]->GetFunctionName();
		FText DisplayName = Chain[i]->GetNodeTitle(ENodeTitleType::ListView);

		if (NodeName.Equals(ModuleName, ESearchCase::IgnoreCase) ||
			DisplayName.ToString().Equals(ModuleName, ESearchCase::IgnoreCase))
		{
			OldIndex = i;
			TargetNode = Chain[i];
			break;
		}
	}

	if (!TargetNode)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Module '%s' not found in chain"), *ModuleName));
	}

	int32 ClampedIndex = FMath::Clamp(NewIndex, 0, Chain.Num() - 1);
	if (ClampedIndex == OldIndex)
	{
		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("module_name"), ModuleName);
		Data->SetNumberField(TEXT("index"), OldIndex);
		Data->SetStringField(TEXT("note"), TEXT("Module already at requested index"));
		return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Data);
	}

	FScopedTransaction Transaction(
		NSLOCTEXT("UnrealMCPBridge", "ReorderNiagaraModule", "Reorder Niagara Module"));

	// Disconnect ALL chain links -- we rebuild the whole chain
	for (UNiagaraNodeFunctionCall* Node : Chain)
	{
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin->Direction == EGPD_Input || Pin->Direction == EGPD_Output)
			{
				Pin->BreakAllPinLinks();
			}
		}
	}

	// Disconnect output node input pin
	UEdGraphPin* OutputNodeInputPin = FindFirstPin(OutputNode, EGPD_Input);
	if (OutputNodeInputPin)
	{
		OutputNodeInputPin->BreakAllPinLinks();
	}

	// Rearrange array
	Chain.RemoveAt(OldIndex);
	Chain.Insert(TargetNode, ClampedIndex);

	// Rebuild the chain: Chain[0] -> Chain[1] -> ... -> OutputNode
	for (int32 i = 1; i < Chain.Num(); ++i)
	{
		UEdGraphPin* PrevOutput = FindFirstPin(Chain[i - 1], EGPD_Output);
		UEdGraphPin* CurrInput = FindFirstPin(Chain[i], EGPD_Input);
		if (PrevOutput && CurrInput)
		{
			PrevOutput->MakeLinkTo(CurrInput);
		}
	}

	// Connect last node to the output node
	if (Chain.Num() > 0 && OutputNodeInputPin)
	{
		UEdGraphPin* LastOutput = FindFirstPin(Chain.Last(), EGPD_Output);
		if (LastOutput)
		{
			LastOutput->MakeLinkTo(OutputNodeInputPin);
		}
	}

	Graph->NotifyGraphChanged();
	NiagaraHelpers::CompileAndSync(System);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("module_name"), ModuleName);
	Data->SetNumberField(TEXT("old_index"), OldIndex);
	Data->SetNumberField(TEXT("new_index"), ClampedIndex);

	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Data);
#else
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only operation"));
#endif
}

// ---------------------------------------------------------------------------
// HandleGetNiagaraModuleInputs
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleGetNiagaraModuleInputs(
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

	FString ModuleName;
	if (!Params->TryGetStringField(TEXT("module_name"), ModuleName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'module_name' parameter"));
	}

	FString ScriptUsageStr;
	if (!Params->TryGetStringField(TEXT("script_usage"), ScriptUsageStr))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'script_usage' parameter"));
	}

	FString InputFilter;
	Params->TryGetStringField(TEXT("input_filter"), InputFilter);

	bool bUsageOk = false;
	ENiagaraScriptUsage Usage = NiagaraHelpers::ParseScriptUsage(ScriptUsageStr, bUsageOk);
	if (!bUsageOk)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Invalid script_usage: '%s'"), *ScriptUsageStr));
	}

	// Load system
	FString LoadError;
	UNiagaraSystem* System = NiagaraHelpers::LoadNiagaraSystem(SystemPath, LoadError);
	if (!System)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(LoadError);
	}

	// Find emitter
	int32 EmitterIdx = INDEX_NONE;
	FString EmitterError;
	FNiagaraEmitterHandle* Handle = NiagaraHelpers::FindEmitterHandle(
		System, EmitterName, EmitterIdx, EmitterError);
	if (!Handle)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(EmitterError);
	}

	FVersionedNiagaraEmitterData* EmitterData = NiagaraHelpers::GetEmitterData(Handle);
	if (!EmitterData)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Emitter has no data"));
	}

	UNiagaraGraph* Graph = NiagaraHelpers::GetGraphForUsage(EmitterData, Usage);
	if (!Graph)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No graph for the given script usage"));
	}

	FString FindError;
	UNiagaraNodeFunctionCall* ModuleNode = NiagaraHelpers::FindModuleNode(
		Graph, Usage, ModuleName, FindError);
	if (!ModuleNode)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FindError);
	}

	TArray<TSharedPtr<FJsonValue>> InputsArray;

	for (UEdGraphPin* Pin : ModuleNode->Pins)
	{
		if (Pin->Direction != EGPD_Input)
		{
			continue;
		}

		// Skip internal parameter map pins
		FString PinCategory = Pin->PinType.PinCategory.ToString();
		if (PinCategory.Equals(TEXT("Misc"), ESearchCase::IgnoreCase))
		{
			continue;
		}

		FString PinName = Pin->PinName.ToString();

		// Apply name filter
		if (!InputFilter.IsEmpty() &&
			!PinName.Contains(InputFilter, ESearchCase::IgnoreCase))
		{
			continue;
		}

		auto InputObj = MakeShared<FJsonObject>();
		InputObj->SetStringField(TEXT("name"), PinName);
		InputObj->SetStringField(TEXT("type"), PinCategory);
		InputObj->SetStringField(TEXT("default_value"), Pin->DefaultValue);
		InputObj->SetBoolField(TEXT("is_connected"), Pin->LinkedTo.Num() > 0);
		InputObj->SetStringField(TEXT("source"), TEXT("function_call_pin"));

		if (Pin->PinType.PinSubCategoryObject.IsValid())
		{
			UObject* SubObj = Pin->PinType.PinSubCategoryObject.Get();
			InputObj->SetStringField(TEXT("sub_type"), SubObj->GetName());
			InputObj->SetStringField(TEXT("sub_type_path"), SubObj->GetPathName());

			// Generic type schema via introspector — handles enum, struct, DI,
			// nested fields, every property kind. Replaces the old hand-rolled
			// "if FEnum" branch with a single recursive walker that returns
			// the full schema regardless of type.
			if (UEnum* EnumPtr = Cast<UEnum>(SubObj))
			{
				InputObj->SetObjectField(TEXT("type_schema"),
					NiagaraIntrospection::SerializeEnum(EnumPtr));
				if (!Pin->DefaultValue.IsEmpty())
				{
					const int64 CurrentVal = EnumPtr->GetValueByName(FName(*Pin->DefaultValue));
					if (CurrentVal != INDEX_NONE)
					{
						InputObj->SetStringField(TEXT("default_value_display"),
							EnumPtr->GetDisplayNameTextByValue(CurrentVal).ToString());
					}
				}
			}
			else if (UScriptStruct* StructPtr = Cast<UScriptStruct>(SubObj))
			{
				InputObj->SetObjectField(TEXT("type_schema"),
					NiagaraIntrospection::SerializeStructFields(StructPtr));
			}
			else if (UClass* ClassPtr = Cast<UClass>(SubObj))
			{
				if (ClassPtr->IsChildOf(UNiagaraDataInterface::StaticClass()))
				{
					InputObj->SetObjectField(TEXT("type_schema"),
						NiagaraIntrospection::SerializeDataInterfaceClass(ClassPtr));
				}
			}
		}

		InputsArray.Add(MakeShared<FJsonValueObject>(InputObj));
	}

	// ---- Second pass: parameter-map module inputs (scratch pad modules) ----
	// Direct function-call pins only cover legacy / direct-pin modules. Modern
	// parameter-map-style modules (any scratch pad module created by the MCP
	// flow) declare their Module.* inputs via Map Get reads inside the called
	// script. The canonical Niagara API to enumerate these is
	// FNiagaraStackGraphUtilities::GetStackFunctionInputs — same call used by
	// the editor's stack UI to populate module input fields. Walking it here
	// means scratch pad inputs (e.g. Module.Boost) finally show up via MCP.
	{
		TArray<FNiagaraVariable> StackInputs;
		FCompileConstantResolver ConstantResolver(System, Usage);
		FNiagaraStackGraphUtilities::GetStackFunctionInputs(
			*ModuleNode,
			StackInputs,
			ConstantResolver,
			FNiagaraStackGraphUtilities::ENiagaraGetStackFunctionInputPinsOptions::ModuleInputsOnly);

		const FString ModulePrefix(TEXT("Module."));
		for (const FNiagaraVariable& Var : StackInputs)
		{
			const FString FullName = Var.GetName().ToString();
			FString DisplayName = FullName;
			if (DisplayName.StartsWith(ModulePrefix))
			{
				DisplayName.RightChopInline(ModulePrefix.Len());
			}

			if (!InputFilter.IsEmpty() &&
				!DisplayName.Contains(InputFilter, ESearchCase::IgnoreCase) &&
				!FullName.Contains(InputFilter, ESearchCase::IgnoreCase))
			{
				continue;
			}

			auto InputObj = MakeShared<FJsonObject>();
			InputObj->SetStringField(TEXT("name"), DisplayName);
			InputObj->SetStringField(TEXT("aliased_name"), FullName);
			InputObj->SetStringField(TEXT("type"), Var.GetType().GetName());
			InputObj->SetStringField(TEXT("source"), TEXT("parameter_map_input"));
			InputObj->SetObjectField(TEXT("type_schema"),
				NiagaraIntrospection::SerializeNiagaraType(Var.GetType()));
			InputsArray.Add(MakeShared<FJsonValueObject>(InputObj));
		}
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("module_name"), ModuleName);
	Data->SetStringField(TEXT("script_usage"), ScriptUsageStr);
	Data->SetArrayField(TEXT("inputs"), InputsArray);
	Data->SetNumberField(TEXT("count"), InputsArray.Num());

	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Data);
#else
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only operation"));
#endif
}

// ---------------------------------------------------------------------------
// HandleSetNiagaraModuleInput
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleSetNiagaraModuleInput(
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

	FString ModuleName;
	if (!Params->TryGetStringField(TEXT("module_name"), ModuleName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'module_name' parameter"));
	}

	FString InputName;
	if (!Params->TryGetStringField(TEXT("input_name"), InputName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'input_name' parameter"));
	}

	FString ScriptUsageStr;
	if (!Params->TryGetStringField(TEXT("script_usage"), ScriptUsageStr))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'script_usage' parameter"));
	}

	if (!Params->HasField(TEXT("value")))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'value' parameter"));
	}

	bool bUsageOk = false;
	ENiagaraScriptUsage Usage = NiagaraHelpers::ParseScriptUsage(ScriptUsageStr, bUsageOk);
	if (!bUsageOk)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Invalid script_usage: '%s'"), *ScriptUsageStr));
	}

	// Load system
	FString LoadError;
	UNiagaraSystem* System = NiagaraHelpers::LoadNiagaraSystem(SystemPath, LoadError);
	if (!System)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(LoadError);
	}

	// Find emitter
	int32 EmitterIdx = INDEX_NONE;
	FString EmitterError;
	FNiagaraEmitterHandle* Handle = NiagaraHelpers::FindEmitterHandle(
		System, EmitterName, EmitterIdx, EmitterError);
	if (!Handle)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(EmitterError);
	}

	FVersionedNiagaraEmitterData* EmitterData = NiagaraHelpers::GetEmitterData(Handle);
	if (!EmitterData)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Emitter has no data"));
	}

	UNiagaraGraph* Graph = NiagaraHelpers::GetGraphForUsage(EmitterData, Usage);
	if (!Graph)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No graph for the given script usage"));
	}

	FString FindError;
	UNiagaraNodeFunctionCall* ModuleNode = NiagaraHelpers::FindModuleNode(
		Graph, Usage, ModuleName, FindError);
	if (!ModuleNode)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FindError);
	}

	// Find the input pin
	UEdGraphPin* InputPin = nullptr;
	for (UEdGraphPin* Pin : ModuleNode->Pins)
	{
		if (Pin->Direction == EGPD_Input &&
			Pin->PinName.ToString().Equals(InputName, ESearchCase::IgnoreCase))
		{
			InputPin = Pin;
			break;
		}
	}

	if (!InputPin)
	{
		// Build list of available inputs for error message
		FString AvailableInputs;
		for (UEdGraphPin* Pin : ModuleNode->Pins)
		{
			if (Pin->Direction != EGPD_Input)
			{
				continue;
			}

			FString Category = Pin->PinType.PinCategory.ToString();
			if (Category.Equals(TEXT("Misc"), ESearchCase::IgnoreCase))
			{
				continue;
			}

			if (!AvailableInputs.IsEmpty())
			{
				AvailableInputs += TEXT(", ");
			}
			AvailableInputs += Pin->PinName.ToString();
		}

		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Input '%s' not found on module '%s'. Available: %s"),
				*InputName, *ModuleName, *AvailableInputs));
	}

	FScopedTransaction Transaction(
		NSLOCTEXT("UnrealMCPBridge", "SetNiagaraModuleInput", "Set Module Input"));

	// Convert the JSON value to a string for the pin's DefaultValue
	TSharedPtr<FJsonValue> JsonValue = Params->TryGetField(TEXT("value"));
	FString ValueStr;

	if (JsonValue->Type == EJson::Number)
	{
		double NumVal = JsonValue->AsNumber();
		ValueStr = FString::SanitizeFloat(NumVal);
	}
	else if (JsonValue->Type == EJson::Boolean)
	{
		ValueStr = JsonValue->AsBool() ? TEXT("true") : TEXT("false");
	}
	else if (JsonValue->Type == EJson::String)
	{
		ValueStr = JsonValue->AsString();
	}
	else if (JsonValue->Type == EJson::Object)
	{
		TSharedPtr<FJsonObject> ValueObj = JsonValue->AsObject();

		// Vector: {x,y,z}
		if (ValueObj->HasField(TEXT("x")))
		{
			double X = ValueObj->GetNumberField(TEXT("x"));
			double Y = ValueObj->GetNumberField(TEXT("y"));
			double Z = ValueObj->GetNumberField(TEXT("z"));
			ValueStr = FString::Printf(TEXT("%f,%f,%f"), X, Y, Z);
		}
		// Color: {r,g,b,a}
		else if (ValueObj->HasField(TEXT("r")))
		{
			double R = ValueObj->GetNumberField(TEXT("r"));
			double G = ValueObj->GetNumberField(TEXT("g"));
			double B = ValueObj->GetNumberField(TEXT("b"));
			double A = 1.0;
			ValueObj->TryGetNumberField(TEXT("a"), A);
			ValueStr = FString::Printf(TEXT("%f,%f,%f,%f"), R, G, B, A);
		}
		else
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				TEXT("Value object must have {x,y,z} or {r,g,b,a} fields"));
		}
	}
	else if (JsonValue->Type == EJson::Array)
	{
		// Array of numbers for vector/color
		const TArray<TSharedPtr<FJsonValue>>& Arr = JsonValue->AsArray();
		TArray<FString> Parts;
		for (const TSharedPtr<FJsonValue>& V : Arr)
		{
			Parts.Add(FString::SanitizeFloat(V->AsNumber()));
		}
		ValueStr = FString::Join(Parts, TEXT(","));
	}
	else
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Unsupported value type. Expected: number, boolean, string, object, or array"));
	}

	// If the pin is enum-typed, validate the supplied value against the enum's
	// known entries before assigning. Without this, an unknown string is silently
	// stored as the pin's DefaultValue and Niagara ignores it (the user thinks
	// the change took effect when it didn't).
	//
	// Accepts three forms for caller convenience:
	//   - internal short name ("NewEnumerator0")
	//   - full name           ("ENiagara_EmitterStateOptions::NewEnumerator0")
	//   - display name        ("Infinite")
	if (UEnum* EnumPtr = Cast<UEnum>(InputPin->PinType.PinSubCategoryObject.Get()))
	{
		int64 ResolvedValue = EnumPtr->GetValueByNameString(ValueStr);
		if (ResolvedValue == INDEX_NONE)
		{
			// Try matching against the display name (user-friendly form)
			for (int32 i = 0; i < EnumPtr->NumEnums(); ++i)
			{
				if (EnumPtr->GetDisplayNameTextByIndex(i).ToString().Equals(ValueStr, ESearchCase::IgnoreCase))
				{
					ResolvedValue = EnumPtr->GetValueByIndex(i);
					break;
				}
			}
		}
		if (ResolvedValue == INDEX_NONE)
		{
			TArray<FString> ValidEntries;
			for (int32 i = 0; i < EnumPtr->NumEnums(); ++i)
			{
				const FString ShortName = EnumPtr->GetNameStringByIndex(i);
				if (ShortName.EndsWith(TEXT("_MAX"))) continue;
				if (EnumPtr->HasMetaData(TEXT("Hidden"), i)) continue;
				const FString DisplayName = EnumPtr->GetDisplayNameTextByIndex(i).ToString();
				ValidEntries.Add(FString::Printf(TEXT("%s ('%s')"), *ShortName, *DisplayName));
			}
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(
					TEXT("Enum value '%s' is not valid for input '%s' (enum: %s). Valid entries: %s"),
					*ValueStr, *InputName, *EnumPtr->GetName(),
					*FString::Join(ValidEntries, TEXT(", "))));
		}
		// Niagara stores the enum default value as the short entry name.
		ValueStr = EnumPtr->GetNameStringByValue(ResolvedValue);
	}

	InputPin->DefaultValue = ValueStr;
	ModuleNode->MarkNodeRequiresSynchronization(TEXT("Module input changed"), true);

	Graph->NotifyGraphChanged();
	NiagaraHelpers::CompileAndSync(System);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("module_name"), ModuleName);
	Data->SetStringField(TEXT("input_name"), InputName);
	Data->SetStringField(TEXT("value"), ValueStr);

	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Data);
#else
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only operation"));
#endif
}

// ---------------------------------------------------------------------------
// HandleSetNiagaraDynamicInput
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleSetNiagaraDynamicInput(
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

	FString ModuleName;
	if (!Params->TryGetStringField(TEXT("module_name"), ModuleName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'module_name' parameter"));
	}

	FString InputName;
	if (!Params->TryGetStringField(TEXT("input_name"), InputName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'input_name' parameter"));
	}

	FString ScriptUsageStr;
	if (!Params->TryGetStringField(TEXT("script_usage"), ScriptUsageStr))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'script_usage' parameter"));
	}

	FString DynamicInputType;
	if (!Params->TryGetStringField(TEXT("dynamic_input_type"), DynamicInputType))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'dynamic_input_type' parameter"));
	}

	bool bUsageOk = false;
	ENiagaraScriptUsage Usage = NiagaraHelpers::ParseScriptUsage(ScriptUsageStr, bUsageOk);
	if (!bUsageOk)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Invalid script_usage: '%s'"), *ScriptUsageStr));
	}

	// Load system
	FString LoadError;
	UNiagaraSystem* System = NiagaraHelpers::LoadNiagaraSystem(SystemPath, LoadError);
	if (!System)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(LoadError);
	}

	// Find emitter
	int32 EmitterIdx = INDEX_NONE;
	FString EmitterError;
	FNiagaraEmitterHandle* Handle = NiagaraHelpers::FindEmitterHandle(
		System, EmitterName, EmitterIdx, EmitterError);
	if (!Handle)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(EmitterError);
	}

	FVersionedNiagaraEmitterData* EmitterData = NiagaraHelpers::GetEmitterData(Handle);
	if (!EmitterData)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Emitter has no data"));
	}

	UNiagaraGraph* Graph = NiagaraHelpers::GetGraphForUsage(EmitterData, Usage);
	if (!Graph)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No graph for the given script usage"));
	}

	FString FindError;
	UNiagaraNodeFunctionCall* ModuleNode = NiagaraHelpers::FindModuleNode(
		Graph, Usage, ModuleName, FindError);
	if (!ModuleNode)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FindError);
	}

	// Create the parameter handle for the input
	FNiagaraParameterHandle InputHandle =
		FNiagaraParameterHandle::CreateModuleParameterHandle(FName(*InputName));
	FNiagaraParameterHandle AliasedHandle =
		FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(InputHandle, ModuleNode);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("module_name"), ModuleName);
	Data->SetStringField(TEXT("input_name"), InputName);
	Data->SetStringField(TEXT("dynamic_input_type"), DynamicInputType);

	FScopedTransaction Transaction(
		NSLOCTEXT("UnrealMCPBridge", "SetNiagaraDynamicInput", "Set Dynamic Input"));

	FString LowerType = DynamicInputType.ToLower();

	// ---- Random Range / Uniform Random ----
	if (LowerType == TEXT("random_range") || LowerType == TEXT("uniform_random"))
	{
		// Determine if float or vector based on value parameters
		bool bIsVector = false;
		FVector MinVec = FVector::ZeroVector;
		FVector MaxVec = FVector::OneVector;
		double MinFloat = 0.0;
		double MaxFloat = 1.0;

		TSharedPtr<FJsonValue> MinVal = Params->TryGetField(TEXT("min_value"));
		TSharedPtr<FJsonValue> MaxVal = Params->TryGetField(TEXT("max_value"));

		if (MinVal.IsValid() && MinVal->Type == EJson::Object)
		{
			bIsVector = true;
			TSharedPtr<FJsonObject> MinObj = MinVal->AsObject();
			MinVec.X = MinObj->GetNumberField(TEXT("x"));
			MinVec.Y = MinObj->GetNumberField(TEXT("y"));
			MinVec.Z = MinObj->GetNumberField(TEXT("z"));

			if (MaxVal.IsValid() && MaxVal->Type == EJson::Object)
			{
				TSharedPtr<FJsonObject> MaxObj = MaxVal->AsObject();
				MaxVec.X = MaxObj->GetNumberField(TEXT("x"));
				MaxVec.Y = MaxObj->GetNumberField(TEXT("y"));
				MaxVec.Z = MaxObj->GetNumberField(TEXT("z"));
			}
		}
		else
		{
			if (MinVal.IsValid())
			{
				MinFloat = MinVal->AsNumber();
			}
			if (MaxVal.IsValid())
			{
				MaxFloat = MaxVal->AsNumber();
			}
		}

		FString RandomScriptPath = bIsVector
			? TEXT("/Niagara/Modules/DynamicInputs/UniformRangedVector.UniformRangedVector")
			: TEXT("/Niagara/Modules/DynamicInputs/UniformRangedFloat.UniformRangedFloat");

		UNiagaraScript* DynamicInputScript = LoadObject<UNiagaraScript>(nullptr, *RandomScriptPath);
		if (!DynamicInputScript)
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Failed to load dynamic input script: %s"), *RandomScriptPath));
		}

		FNiagaraTypeDefinition InputType = bIsVector
			? FNiagaraTypeDefinition::GetVec3Def()
			: FNiagaraTypeDefinition::GetFloatDef();

		UEdGraphPin& OverridePin = FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(
			*ModuleNode, AliasedHandle, InputType, FGuid(), FGuid());

		// Remove existing override connections
		if (OverridePin.LinkedTo.Num() > 0)
		{
			RemoveOverridePinConnections(OverridePin, Graph);
		}

		UNiagaraNodeFunctionCall* DynamicInputNode = nullptr;
		FNiagaraStackGraphUtilities::SetDynamicInputForFunctionInput(
			OverridePin, DynamicInputScript, DynamicInputNode, FGuid(), TEXT("Random"), FGuid());

		if (DynamicInputNode)
		{
			// Set min/max values on the dynamic input's own pins
			for (UEdGraphPin* DIPin : DynamicInputNode->Pins)
			{
				if (DIPin->Direction != EGPD_Input)
				{
					continue;
				}

				FString PinNameLower = DIPin->PinName.ToString().ToLower();
				if (PinNameLower.Contains(TEXT("min")))
				{
					if (bIsVector)
					{
						DIPin->DefaultValue = FString::Printf(
							TEXT("%f,%f,%f"), MinVec.X, MinVec.Y, MinVec.Z);
					}
					else
					{
						DIPin->DefaultValue = FString::SanitizeFloat(MinFloat);
					}
				}
				else if (PinNameLower.Contains(TEXT("max")))
				{
					if (bIsVector)
					{
						DIPin->DefaultValue = FString::Printf(
							TEXT("%f,%f,%f"), MaxVec.X, MaxVec.Y, MaxVec.Z);
					}
					else
					{
						DIPin->DefaultValue = FString::SanitizeFloat(MaxFloat);
					}
				}
			}
		}

		Data->SetStringField(TEXT("script_path"), RandomScriptPath);
		Data->SetBoolField(TEXT("dynamic_input_created"), DynamicInputNode != nullptr);
	}
	// ---- Parameter Link ----
	else if (LowerType == TEXT("parameter_link"))
	{
		FString ParameterName;
		if (!Params->TryGetStringField(TEXT("parameter_name"), ParameterName))
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				TEXT("Missing 'parameter_name' for parameter_link dynamic input"));
		}

		const UEdGraphSchema_Niagara* NiagaraSchema = GetDefault<UEdGraphSchema_Niagara>();

		// Determine the target input type from the module pin
		FNiagaraTypeDefinition TargetType = FNiagaraTypeDefinition::GetFloatDef();
		for (UEdGraphPin* Pin : ModuleNode->Pins)
		{
			if (Pin->Direction == EGPD_Input &&
				Pin->PinName.ToString().Equals(InputName, ESearchCase::IgnoreCase))
			{
				TargetType = NiagaraSchema->PinToTypeDefinition(Pin);
				break;
			}
		}

		// Find the source parameter's actual type from the system's exposed parameters
		FNiagaraTypeDefinition SourceType;
		bool bFoundSourceParam = false;
		TArrayView<const FNiagaraVariableWithOffset> ExposedVars =
			System->GetExposedParameters().ReadParameterVariables();
		for (const FNiagaraVariableWithOffset& Var : ExposedVars)
		{
			FString VarName = Var.GetName().ToString();
			if (VarName.Equals(ParameterName, ESearchCase::IgnoreCase) ||
				VarName.Contains(ParameterName, ESearchCase::IgnoreCase))
			{
				SourceType = Var.GetType();
				bFoundSourceParam = true;
				break;
			}
		}

		if (!bFoundSourceParam)
		{
			SourceType = FNiagaraTypeDefinition::GetVec3Def();
		}

		// Debug: log the types in the response
		Data->SetStringField(TEXT("source_type"), SourceType.GetName());
		Data->SetStringField(TEXT("target_type"), TargetType.GetName());
		Data->SetBoolField(TEXT("types_differ"), SourceType != TargetType);

		// Get the EXISTING ViewModel from the Niagara editor (if the system is open)
		// This avoids creating a new ViewModel which can crash due to MessageManager GUID issues
		TSharedPtr<FNiagaraSystemViewModel> SystemViewModel =
			FNiagaraEditorModule::Get().GetExistingViewModelForSystem(System);

		if (!SystemViewModel.IsValid())
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				TEXT("No active Niagara editor found for this system. "
				     "Open the system in the Niagara editor first, then retry the parameter_link command."));
		}

		// Find the target emitter handle ViewModel
		TSharedPtr<FNiagaraEmitterHandleViewModel> EmitterHandleVM;
		for (const TSharedRef<FNiagaraEmitterHandleViewModel>& HandleVM : SystemViewModel->GetEmitterHandleViewModels())
		{
			if (HandleVM->GetName().ToString().Equals(EmitterName, ESearchCase::IgnoreCase))
			{
				EmitterHandleVM = HandleVM;
				break;
			}
		}

		if (!EmitterHandleVM.IsValid())
		{
			// ViewModel cleanup handled by shared pointer destructor
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Could not find emitter '%s' in SystemViewModel"), *EmitterName));
		}

		// Get the emitter stack ViewModel and find the target module + input
		UNiagaraStackViewModel* StackVM = EmitterHandleVM->GetEmitterStackViewModel();
		if (!StackVM)
		{
			// ViewModel cleanup handled by shared pointer destructor
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				TEXT("Failed to get emitter stack ViewModel"));
		}

		UNiagaraStackEntry* RootEntry = StackVM->GetRootEntry();
		if (!RootEntry)
		{
			// ViewModel cleanup handled by shared pointer destructor
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				TEXT("Failed to get stack root entry"));
		}

		// Recursively find the UNiagaraStackModuleItem matching our module name
		TArray<UNiagaraStackModuleItem*> ModuleItems;
		RootEntry->GetFilteredChildrenOfType<UNiagaraStackModuleItem>(ModuleItems, true);

		UNiagaraStackModuleItem* TargetModuleItem = nullptr;
		for (UNiagaraStackModuleItem* Item : ModuleItems)
		{
			FString DisplayName = Item->GetDisplayName().ToString();
			if (DisplayName.Equals(ModuleName, ESearchCase::IgnoreCase))
			{
				TargetModuleItem = Item;
				break;
			}

			// Also try matching against the function call node name
			UNiagaraNodeFunctionCall& FuncNode = Item->GetModuleNode();
			FString FuncName = FuncNode.GetFunctionName();
			if (FuncName.Equals(ModuleName, ESearchCase::IgnoreCase))
			{
				TargetModuleItem = Item;
				break;
			}
		}

		if (!TargetModuleItem)
		{
			// ViewModel cleanup handled by shared pointer destructor
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(
					TEXT("Could not find module '%s' in the stack ViewModel (found %d modules)"),
					*ModuleName, ModuleItems.Num()));
		}

		// Get ALL inputs recursively — includes nested children like Shape Origin under Transform
		TArray<UNiagaraStackFunctionInput*> ParameterInputs;
		TargetModuleItem->GetUnfilteredChildrenOfType<UNiagaraStackFunctionInput>(
			ParameterInputs, true);

		UNiagaraStackFunctionInput* TargetInput = nullptr;
		for (UNiagaraStackFunctionInput* Input : ParameterInputs)
		{
			FString InputDisplayName = Input->GetDisplayName().ToString();
			FString InputParamName = Input->GetInputParameterHandle().GetName().ToString();

			// Exact match first
			if (InputDisplayName.Equals(InputName, ESearchCase::IgnoreCase) ||
				InputParamName.Equals(InputName, ESearchCase::IgnoreCase))
			{
				TargetInput = Input;
				break;
			}

			// Partial/contains match as fallback
			if (InputDisplayName.Contains(InputName, ESearchCase::IgnoreCase) ||
				InputParamName.Contains(InputName, ESearchCase::IgnoreCase))
			{
				TargetInput = Input;
				// Don't break — keep looking for exact match
			}
		}

		if (!TargetInput)
		{
			// List available inputs to help the user find the right name
			TArray<FString> AvailableInputs;
			for (UNiagaraStackFunctionInput* Input : ParameterInputs)
			{
				AvailableInputs.Add(Input->GetDisplayName().ToString());
			}
			// Cap to avoid huge responses
			if (AvailableInputs.Num() > 20)
			{
				AvailableInputs.SetNum(20);
				AvailableInputs.Add(TEXT("..."));
			}

			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(
					TEXT("Could not find input '%s' on module '%s' (found %d inputs). Available: %s"),
					*InputName, *ModuleName, ParameterInputs.Num(),
					*FString::Join(AvailableInputs, TEXT(", "))));
		}

		// Use the ViewModel's input type — this is the CORRECT type, not from pin scanning
		FNiagaraTypeDefinition ActualTargetType = TargetInput->GetInputType();

		// Check current input state — if it has an existing linked value or dynamic input,
		// we need to reset it first. But Reset can crash if graph nodes are inconsistent
		// (CastChecked in RemoveOverridePin fails on nodes left by prior MCP operations).
		// Use the ViewModel's own reset which is safer than manual graph manipulation.
		auto CurrentMode = TargetInput->GetValueMode();
		if (CurrentMode != UNiagaraStackFunctionInput::EValueMode::Local &&
			CurrentMode != UNiagaraStackFunctionInput::EValueMode::DefaultFunction &&
			TargetInput->CanReset())
		{
			TargetInput->Reset();
			// Refresh the stack after reset to ensure clean state
			TargetInput->RefreshChildren();
		}

		// Build the linked parameter variable with the source type
		FNiagaraVariable LinkedParam(SourceType, FName(*ParameterName));

		// Update debug info with actual types
		Data->SetStringField(TEXT("actual_target_type"), ActualTargetType.GetName());

		// Check if type conversion is needed using the ACTUAL target type
		bool bTypeConversionUsed = false;
		FString ConversionScriptPath;

		if (SourceType != ActualTargetType)
		{
			TArray<UNiagaraScript*> ConversionScripts =
				UNiagaraStackFunctionInput::GetPossibleConversionScripts(SourceType, ActualTargetType);

			if (ConversionScripts.Num() > 0)
			{
				TargetInput->SetLinkedParameterValueViaConversionScript(
					LinkedParam, *ConversionScripts[0]);

				bTypeConversionUsed = true;
				ConversionScriptPath = ConversionScripts[0]->GetPathName();
			}
			else
			{
				// No conversion script available — try direct link anyway
				TargetInput->SetLinkedParameterValue(LinkedParam);
			}
		}
		else
		{
			// Types match — direct link
			TargetInput->SetLinkedParameterValue(LinkedParam);
		}

		Data->SetStringField(TEXT("linked_parameter"), ParameterName);
		Data->SetBoolField(TEXT("type_conversion_used"), bTypeConversionUsed);
		Data->SetBoolField(TEXT("dynamic_input_created"), true);
		if (bTypeConversionUsed)
		{
			Data->SetStringField(TEXT("conversion_script"), ConversionScriptPath);
		}

		// ViewModel cleanup handled by shared pointer going out of scope
	}
	// ---- Custom Expression ----
	else if (LowerType == TEXT("custom_expression"))
	{
		FString Expression;
		if (!Params->TryGetStringField(TEXT("expression"), Expression))
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				TEXT("Missing 'expression' for custom_expression dynamic input"));
		}

		// Create a custom HLSL node and wire it as a dynamic input
		UNiagaraNodeCustomHlsl* CustomNode = NewObject<UNiagaraNodeCustomHlsl>(Graph);
		CustomNode->CreateNewGuid();
		CustomNode->PostPlacedNewNode();
		CustomNode->AllocateDefaultPins();
		// SetCustomHlsl not exported from NiagaraEditor — set via reflection
		FStrProperty* HlslProp = CastField<FStrProperty>(
			UNiagaraNodeCustomHlsl::StaticClass()->FindPropertyByName(TEXT("CustomHlsl")));
		if (HlslProp)
		{
			HlslProp->SetPropertyValue_InContainer(CustomNode, Expression);
		}
		CustomNode->MarkNodeRequiresSynchronization(TEXT("HLSL set via MCP"), true);
		Graph->AddNode(CustomNode, false, false);

		// Wire the custom node output to the module's override pin
		FNiagaraTypeDefinition InputType = FNiagaraTypeDefinition::GetFloatDef();
		UEdGraphPin& OverridePin = FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(
			*ModuleNode, AliasedHandle, InputType, FGuid(), FGuid());

		if (OverridePin.LinkedTo.Num() > 0)
		{
			RemoveOverridePinConnections(OverridePin, Graph);
		}

		UEdGraphPin* CustomOutputPin = FindFirstPin(CustomNode, EGPD_Output);
		if (CustomOutputPin)
		{
			CustomOutputPin->MakeLinkTo(&OverridePin);
		}

		Data->SetStringField(TEXT("expression"), Expression);
		Data->SetBoolField(TEXT("dynamic_input_created"), true);
	}
	// ---- Generic script-backed dynamic input ----
	// Accepts an arbitrary dynamic input script asset path; works for any
	// dynamic input script not covered by the dedicated helpers above.
	else if (LowerType == TEXT("script") || LowerType == TEXT("asset"))
	{
		FString ScriptPath;
		if (!Params->TryGetStringField(TEXT("dynamic_input_script_path"), ScriptPath))
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				TEXT("Missing 'dynamic_input_script_path' for script-backed dynamic input"));
		}

		UNiagaraScript* DynamicInputScript = LoadObject<UNiagaraScript>(nullptr, *ScriptPath);
		if (!DynamicInputScript)
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Failed to load dynamic input script '%s'"), *ScriptPath));
		}
		if (DynamicInputScript->GetUsage() != ENiagaraScriptUsage::DynamicInput)
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				TEXT("Script is not a DynamicInput usage script"));
		}

		// Resolve the module input's declared type from its pin
		const UEdGraphSchema_Niagara* NiagaraSchema = GetDefault<UEdGraphSchema_Niagara>();
		FNiagaraTypeDefinition InputType = FNiagaraTypeDefinition::GetFloatDef();
		for (UEdGraphPin* Pin : ModuleNode->Pins)
		{
			if (Pin->Direction == EGPD_Input &&
				Pin->PinName.ToString().Equals(InputName, ESearchCase::IgnoreCase))
			{
				InputType = NiagaraSchema->PinToTypeDefinition(Pin);
				break;
			}
		}

		UEdGraphPin& OverridePin = FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(
			*ModuleNode, AliasedHandle, InputType, FGuid(), FGuid());
		if (OverridePin.LinkedTo.Num() > 0)
		{
			RemoveOverridePinConnections(OverridePin, Graph);
		}

		UNiagaraNodeFunctionCall* DynamicInputNode = nullptr;
		FString SuggestedName;
		Params->TryGetStringField(TEXT("suggested_name"), SuggestedName);
		FNiagaraStackGraphUtilities::SetDynamicInputForFunctionInput(
			OverridePin, DynamicInputScript, DynamicInputNode, FGuid(), SuggestedName, FGuid());

		// Optional: apply default values to the dynamic input's own pins
		// Shape: { "pin_name": "value_as_string", ... }
		const TSharedPtr<FJsonObject>* DefaultsObj = nullptr;
		if (DynamicInputNode && Params->TryGetObjectField(TEXT("pin_defaults"), DefaultsObj))
		{
			for (UEdGraphPin* DIPin : DynamicInputNode->Pins)
			{
				if (!DIPin || DIPin->Direction != EGPD_Input) continue;
				FString DefaultStr;
				if ((*DefaultsObj)->TryGetStringField(DIPin->PinName.ToString(), DefaultStr))
				{
					DIPin->DefaultValue = DefaultStr;
				}
			}
		}

		Data->SetStringField(TEXT("script_path"), DynamicInputScript->GetPathName());
		Data->SetStringField(TEXT("input_type"), InputType.GetName());
		Data->SetBoolField(TEXT("dynamic_input_created"), DynamicInputNode != nullptr);
	}
	else
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(
				TEXT("Unknown dynamic_input_type: '%s'. Supported: random_range, uniform_random, parameter_link, custom_expression, script"),
				*DynamicInputType));
	}

	Graph->NotifyGraphChanged();
	NiagaraHelpers::CompileAndSync(System);

	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Data);
#else
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only operation"));
#endif
}

// Rapid iteration parameter tools are in NiagaraRapidIterationOps.cpp
