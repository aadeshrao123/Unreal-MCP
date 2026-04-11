#include "Commands/EpicUnrealMCPNiagaraCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "NiagaraHelpers.h"
#include "NiagaraTypeHelpers.h"

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
#include "NiagaraSystemEditorData.h"
#include "NiagaraEditorSettings.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraNodeCustomHlsl.h"
#include "NiagaraNodeParameterMapGet.h"
#include "NiagaraNodeParameterMapSet.h"
#include "NiagaraScriptVariable.h"
#include "EdGraphSchema_Niagara.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#endif

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "UObject/SavePackage.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

// ---------------------------------------------------------------------------
// Local helpers
// ---------------------------------------------------------------------------

#if WITH_EDITORONLY_DATA

namespace
{

/** Parse a scratch-pad module_type string into an ENiagaraScriptUsage. */
ENiagaraScriptUsage ParseModuleType(const FString& ModuleType)
{
	const FString Lower = ModuleType.ToLower();
	if (Lower == TEXT("dynamic_input") || Lower == TEXT("dynamicinput"))
	{
		return ENiagaraScriptUsage::DynamicInput;
	}
	if (Lower == TEXT("function"))
	{
		return ENiagaraScriptUsage::Function;
	}
	return ENiagaraScriptUsage::Module;
}

/** Try to load the editor's DefaultModuleScript / DynamicInput / Function template asset. */
UNiagaraScript* LoadDefaultTemplateScript(ENiagaraScriptUsage Usage)
{
	const UNiagaraEditorSettings* Settings = GetDefault<UNiagaraEditorSettings>();
	if (!Settings)
	{
		return nullptr;
	}

	FSoftObjectPath TemplatePath;
	switch (Usage)
	{
	case ENiagaraScriptUsage::DynamicInput:
		TemplatePath = Settings->DefaultDynamicInputScript;
		break;
	case ENiagaraScriptUsage::Function:
		TemplatePath = Settings->DefaultFunctionScript;
		break;
	case ENiagaraScriptUsage::Module:
	default:
		TemplatePath = Settings->DefaultModuleScript;
		break;
	}

	if (!TemplatePath.IsValid())
	{
		return nullptr;
	}
	return Cast<UNiagaraScript>(TemplatePath.TryLoad());
}

/**
 * Build a minimal working scratch pad graph if no template is available:
 *   Input(ParameterMap) -> ParameterMapGet -> ParameterMapSet -> Output(Module)
 * This is a fallback — the preferred path is always duplicating the editor's
 * DefaultModuleScript asset which ships with richer defaults.
 */
void BuildMinimalScratchGraph(UNiagaraScript* Script, UNiagaraGraph* Graph, ENiagaraScriptUsage Usage)
{
	if (!Script || !Graph)
	{
		return;
	}

	// Output node (rightmost)
	FGraphNodeCreator<UNiagaraNodeOutput> OutputCreator(*Graph);
	UNiagaraNodeOutput* OutputNode = OutputCreator.CreateNode(false);
	OutputNode->SetUsage(Usage);
	OutputNode->NodePosX = 800;
	OutputNode->NodePosY = 0;
	FNiagaraVariable ParamMapOutput(FNiagaraTypeDefinition::GetParameterMapDef(), TEXT("Out"));
	OutputNode->Outputs.Add(ParamMapOutput);
	OutputCreator.Finalize();

	// Parameter Map Set
	FGraphNodeCreator<UNiagaraNodeParameterMapSet> SetCreator(*Graph);
	UNiagaraNodeParameterMapSet* SetNode = SetCreator.CreateNode(false);
	SetNode->NodePosX = 500;
	SetNode->NodePosY = 0;
	SetCreator.Finalize();

	// Parameter Map Get
	FGraphNodeCreator<UNiagaraNodeParameterMapGet> GetCreator(*Graph);
	UNiagaraNodeParameterMapGet* GetNode = GetCreator.CreateNode(false);
	GetNode->NodePosX = 200;
	GetNode->NodePosY = 0;
	GetCreator.Finalize();

	// Wire MapGet.ParameterMap (output) -> MapSet.ParameterMap (input) -> Output.Out (input)
	UEdGraphPin* GetMapOut = nullptr;
	for (UEdGraphPin* Pin : GetNode->Pins)
	{
		if (Pin && Pin->Direction == EGPD_Output && !NiagaraTypeHelpers::IsAddPin(Pin))
		{
			GetMapOut = Pin;
			break;
		}
	}
	UEdGraphPin* SetMapIn = nullptr;
	UEdGraphPin* SetMapOut = nullptr;
	for (UEdGraphPin* Pin : SetNode->Pins)
	{
		if (!Pin || NiagaraTypeHelpers::IsAddPin(Pin))
		{
			continue;
		}
		if (Pin->Direction == EGPD_Input && !SetMapIn)
		{
			SetMapIn = Pin;
		}
		else if (Pin->Direction == EGPD_Output && !SetMapOut)
		{
			SetMapOut = Pin;
		}
	}
	UEdGraphPin* OutputIn = OutputNode->Pins.Num() > 0 ? OutputNode->Pins[0] : nullptr;

	if (GetMapOut && SetMapIn)
	{
		GetMapOut->MakeLinkTo(SetMapIn);
	}
	if (SetMapOut && OutputIn)
	{
		SetMapOut->MakeLinkTo(OutputIn);
	}
}

/** Find a scratch pad script on a system by case-insensitive name.
 *  Returns the asset script only — use NiagaraHelpers::GetScratchPadScriptPair
 *  for mutators that need to keep the editor's live transient copy in sync.
 */
UNiagaraScript* FindScratchPadScript(UNiagaraSystem* System, const FString& ModuleName)
{
	return NiagaraHelpers::FindScratchPadScript(System, ModuleName);
}

/** Find or lazily create a CustomHlsl node on a scratch pad script's graph. */
UNiagaraNodeCustomHlsl* FindOrCreateCustomHlslNode(UNiagaraGraph* Graph)
{
	if (!Graph)
	{
		return nullptr;
	}
	TArray<UNiagaraNodeCustomHlsl*> Existing;
	Graph->GetNodesOfClass<UNiagaraNodeCustomHlsl>(Existing);
	if (Existing.Num() > 0)
	{
		return Existing[0];
	}
	FGraphNodeCreator<UNiagaraNodeCustomHlsl> Creator(*Graph);
	UNiagaraNodeCustomHlsl* HlslNode = Creator.CreateNode(false);
	HlslNode->NodePosX = 300;
	HlslNode->NodePosY = 200;
	Creator.Finalize();
	return HlslNode;
}

/**
 * Resolve a scratch-pad module to its set of live graphs — the asset's graph
 * and (when the Niagara editor is open) the transient edit-copy's graph.
 * Callers iterate this list and apply mutations to each, so changes show in
 * the open viewport without needing close + reopen.
 */
void CollectScratchPadGraphs(
	UNiagaraSystem* System,
	const FString& ModuleName,
	TArray<UNiagaraGraph*>& OutGraphs)
{
	OutGraphs.Reset();
	TArray<UNiagaraScript*> Scripts;
	NiagaraHelpers::GetScratchPadScriptPair(System, ModuleName, Scripts);
	for (UNiagaraScript* Script : Scripts)
	{
		if (!Script) continue;
		UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(Script->GetLatestSource());
		if (Source && Source->NodeGraph)
		{
			OutGraphs.Add(Source->NodeGraph);
		}
	}
}

/**
 * Same as CollectScratchPadGraphs but returns the (first) CustomHlsl node in
 * each graph, creating one if missing. Used by add/rename/remove pin handlers
 * so the same mutation is applied to both the asset node and the editor's
 * transient copy.
 */
void CollectScratchPadHlslNodes(
	UNiagaraSystem* System,
	const FString& ModuleName,
	TArray<UNiagaraNodeCustomHlsl*>& OutNodes)
{
	OutNodes.Reset();
	TArray<UNiagaraGraph*> Graphs;
	CollectScratchPadGraphs(System, ModuleName, Graphs);
	for (UNiagaraGraph* Graph : Graphs)
	{
		if (UNiagaraNodeCustomHlsl* Node = FindOrCreateCustomHlslNode(Graph))
		{
			OutNodes.Add(Node);
		}
	}
}

/** Build JSON summary of a scratch pad script: name, usage, pin counts. */
TSharedPtr<FJsonObject> ScratchPadScriptToJson(UNiagaraScript* Script, int32 Index)
{
	auto Json = MakeShared<FJsonObject>();
	Json->SetNumberField(TEXT("index"), Index);
	if (!Script)
	{
		Json->SetBoolField(TEXT("valid"), false);
		return Json;
	}
	Json->SetBoolField(TEXT("valid"), true);
	Json->SetStringField(TEXT("name"), Script->GetName());
	Json->SetStringField(TEXT("path"), Script->GetPathName());
	Json->SetStringField(TEXT("usage"), NiagaraHelpers::ScriptUsageToString(Script->GetUsage()));

	if (UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(Script->GetLatestSource()))
	{
		if (UNiagaraGraph* Graph = Source->NodeGraph)
		{
			Json->SetNumberField(TEXT("node_count"), Graph->Nodes.Num());

			TArray<UNiagaraNodeCustomHlsl*> HlslNodes;
			Graph->GetNodesOfClass<UNiagaraNodeCustomHlsl>(HlslNodes);
			Json->SetNumberField(TEXT("custom_hlsl_nodes"), HlslNodes.Num());
		}
	}
	return Json;
}

} // namespace

#endif // WITH_EDITORONLY_DATA


// ---------------------------------------------------------------------------
// HandleCreateNiagaraScratchPadModule
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleCreateNiagaraScratchPadModule(
	const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
	FString SystemPath;
	if (!Params->TryGetStringField(TEXT("system_path"), SystemPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'system_path' parameter"));
	}

	FString ModuleName = TEXT("ScratchPadModule");
	Params->TryGetStringField(TEXT("module_name"), ModuleName);

	FString ModuleTypeStr = TEXT("module");
	Params->TryGetStringField(TEXT("module_type"), ModuleTypeStr);
	const ENiagaraScriptUsage Usage = ParseModuleType(ModuleTypeStr);

	FString Error;
	UNiagaraSystem* System = NiagaraHelpers::LoadNiagaraSystem(SystemPath, Error);
	if (!System)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	// Ensure unique name within the system's scratch pad collection
	FName UniqueName = MakeUniqueObjectName(System, UNiagaraScript::StaticClass(), FName(*ModuleName));

	System->Modify();

	UNiagaraScript* NewScript = nullptr;
	bool bUsedTemplate = false;

	// Preferred path: duplicate the editor's default template so the graph
	// matches what "Create New Module" in the Scratch Script Manager would
	// produce (pre-wired Input/MapGet/MapSet/Output).
	if (UNiagaraScript* Template = LoadDefaultTemplateScript(Usage))
	{
		NewScript = Cast<UNiagaraScript>(StaticDuplicateObject(Template, System, UniqueName));
		if (NewScript)
		{
			// Scratch pad scripts are system-scoped, not standalone assets
			NewScript->ClearFlags(RF_Public | RF_Standalone);
			NewScript->SetFlags(RF_Transactional);
			NewScript->SetUsage(Usage);
			bUsedTemplate = true;
		}
	}

	// Fallback path: construct a minimal working graph ourselves
	if (!NewScript)
	{
		NewScript = NewObject<UNiagaraScript>(System, UniqueName, RF_Transactional);
		if (!NewScript)
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create scratch pad script"));
		}
		NewScript->SetUsage(Usage);

		UNiagaraScriptSource* ScriptSource = NewObject<UNiagaraScriptSource>(
			NewScript, TEXT("ScriptSource"), RF_Transactional);
		UNiagaraGraph* Graph = NewObject<UNiagaraGraph>(
			ScriptSource, TEXT("NiagaraGraph"), RF_Transactional);
		ScriptSource->NodeGraph = Graph;
		NewScript->SetLatestSource(ScriptSource);

		BuildMinimalScratchGraph(NewScript, Graph, Usage);
	}

	System->ScratchPadScripts.Add(NewScript);
	System->MarkPackageDirty();
	NiagaraHelpers::CompileAndSync(System);
	NiagaraHelpers::NotifyScratchPadScriptChanged(System, NewScript->GetName());

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("module_name"), NewScript->GetName());
	Result->SetStringField(TEXT("system_path"), SystemPath);
	Result->SetStringField(TEXT("module_type"), ModuleTypeStr);
	Result->SetBoolField(TEXT("used_template"), bUsedTemplate);
	Result->SetNumberField(TEXT("scratch_pad_count"), System->ScratchPadScripts.Num());
	return Result;
#else
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only operation"));
#endif
}

// ---------------------------------------------------------------------------
// HandleDuplicateNiagaraScratchPadModule
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleDuplicateNiagaraScratchPadModule(
	const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
	FString SystemPath;
	if (!Params->TryGetStringField(TEXT("system_path"), SystemPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'system_path' parameter"));
	}
	FString SourceName;
	if (!Params->TryGetStringField(TEXT("module_name"), SourceName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'module_name' parameter"));
	}

	FString NewName;
	Params->TryGetStringField(TEXT("new_name"), NewName);

	FString Error;
	UNiagaraSystem* System = NiagaraHelpers::LoadNiagaraSystem(SystemPath, Error);
	if (!System)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	UNiagaraScript* Source = FindScratchPadScript(System, SourceName);
	if (!Source)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Scratch pad module '%s' not found"), *SourceName));
	}

	if (NewName.IsEmpty())
	{
		NewName = SourceName + TEXT("_Copy");
	}

	System->Modify();
	const FName UniqueName = MakeUniqueObjectName(System, UNiagaraScript::StaticClass(), FName(*NewName));
	UNiagaraScript* Duplicate = Cast<UNiagaraScript>(StaticDuplicateObject(Source, System, UniqueName));
	if (!Duplicate)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Duplication failed"));
	}
	Duplicate->ClearFlags(RF_Public | RF_Standalone);
	Duplicate->SetFlags(RF_Transactional);

	System->ScratchPadScripts.Add(Duplicate);
	System->MarkPackageDirty();
	NiagaraHelpers::CompileAndSync(System);
	NiagaraHelpers::NotifyScratchPadScriptChanged(System, Duplicate->GetName());

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("source_name"), SourceName);
	Result->SetStringField(TEXT("new_name"), Duplicate->GetName());
	Result->SetNumberField(TEXT("scratch_pad_count"), System->ScratchPadScripts.Num());
	return Result;
#else
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only operation"));
#endif
}

// ---------------------------------------------------------------------------
// HandleDeleteNiagaraScratchPadModule
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleDeleteNiagaraScratchPadModule(
	const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
	FString SystemPath;
	if (!Params->TryGetStringField(TEXT("system_path"), SystemPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'system_path' parameter"));
	}
	FString ModuleName;
	if (!Params->TryGetStringField(TEXT("module_name"), ModuleName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'module_name' parameter"));
	}

	FString Error;
	UNiagaraSystem* System = NiagaraHelpers::LoadNiagaraSystem(SystemPath, Error);
	if (!System)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	int32 RemovedIndex = INDEX_NONE;
	System->Modify();
	for (int32 i = 0; i < System->ScratchPadScripts.Num(); ++i)
	{
		UNiagaraScript* Script = System->ScratchPadScripts[i];
		if (Script && Script->GetName().Equals(ModuleName, ESearchCase::IgnoreCase))
		{
			System->ScratchPadScripts.RemoveAt(i);
			RemovedIndex = i;
			break;
		}
	}

	if (RemovedIndex == INDEX_NONE)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Scratch pad module '%s' not found"), *ModuleName));
	}

	System->MarkPackageDirty();
	NiagaraHelpers::CompileAndSync(System);
	NiagaraHelpers::NotifyScratchPadScriptChanged(System, FString());

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("module_name"), ModuleName);
	Result->SetNumberField(TEXT("removed_index"), RemovedIndex);
	Result->SetNumberField(TEXT("scratch_pad_count"), System->ScratchPadScripts.Num());
	return Result;
#else
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only operation"));
#endif
}

// ---------------------------------------------------------------------------
// HandleRenameNiagaraScratchPadModule
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleRenameNiagaraScratchPadModule(
	const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
	FString SystemPath;
	if (!Params->TryGetStringField(TEXT("system_path"), SystemPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'system_path' parameter"));
	}
	FString OldName;
	if (!Params->TryGetStringField(TEXT("module_name"), OldName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'module_name' parameter"));
	}
	FString NewName;
	if (!Params->TryGetStringField(TEXT("new_name"), NewName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'new_name' parameter"));
	}

	FString Error;
	UNiagaraSystem* System = NiagaraHelpers::LoadNiagaraSystem(SystemPath, Error);
	if (!System)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}
	UNiagaraScript* Script = FindScratchPadScript(System, OldName);
	if (!Script)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Scratch pad module '%s' not found"), *OldName));
	}

	const FName UniqueName = MakeUniqueObjectName(System, UNiagaraScript::StaticClass(), FName(*NewName));
	Script->Modify();
	if (!Script->Rename(*UniqueName.ToString(), System, REN_DontCreateRedirectors))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Rename failed"));
	}
	System->MarkPackageDirty();
	NiagaraHelpers::CompileAndSync(System);
	NiagaraHelpers::NotifyScratchPadScriptChanged(System, Script->GetName());

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("old_name"), OldName);
	Result->SetStringField(TEXT("new_name"), Script->GetName());
	return Result;
#else
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only operation"));
#endif
}

// ---------------------------------------------------------------------------
// HandleListNiagaraScratchPadModules
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleListNiagaraScratchPadModules(
	const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
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

	TArray<TSharedPtr<FJsonValue>> Scripts;
	for (int32 i = 0; i < System->ScratchPadScripts.Num(); ++i)
	{
		Scripts.Add(MakeShared<FJsonValueObject>(ScratchPadScriptToJson(System->ScratchPadScripts[i], i)));
	}

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("system_path"), SystemPath);
	Result->SetArrayField(TEXT("modules"), Scripts);
	Result->SetNumberField(TEXT("count"), Scripts.Num());
	return Result;
#else
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only operation"));
#endif
}

// ---------------------------------------------------------------------------
// HandleSetNiagaraScratchPadHlsl
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleSetNiagaraScratchPadHlsl(
	const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
	FString SystemPath;
	if (!Params->TryGetStringField(TEXT("system_path"), SystemPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'system_path' parameter"));
	}
	FString ModuleName;
	if (!Params->TryGetStringField(TEXT("module_name"), ModuleName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'module_name' parameter"));
	}
	FString HlslCode;
	if (!Params->TryGetStringField(TEXT("hlsl_code"), HlslCode))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'hlsl_code' parameter"));
	}
	bool bClearExistingPins = false;
	Params->TryGetBoolField(TEXT("clear_existing_pins"), bClearExistingPins);

	FString Error;
	UNiagaraSystem* System = NiagaraHelpers::LoadNiagaraSystem(SystemPath, Error);
	if (!System)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}
	// Collect every CustomHlsl node we need to update — always the asset node,
	// plus the Niagara editor's transient edit-copy node if the system is open.
	// Writing to both keeps the viewport in sync without a close+reopen cycle.
	TArray<UNiagaraNodeCustomHlsl*> HlslNodes;
	CollectScratchPadHlslNodes(System, ModuleName, HlslNodes);
	if (HlslNodes.Num() == 0)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Scratch pad module '%s' not found or has no valid graph"), *ModuleName));
	}

	// Parse the JSON input/output arrays ONCE so we can apply the identical
	// (name, type) set to every node in the pair.
	struct FPinSpec
	{
		FName Name;
		FNiagaraTypeDefinition Type;
	};
	TArray<FPinSpec> InputSpecs;
	TArray<FPinSpec> OutputSpecs;

	auto CollectSpecs = [&](const TCHAR* FieldName, TArray<FPinSpec>& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (!Params->TryGetArrayField(FieldName, Arr)) return;
		for (const TSharedPtr<FJsonValue>& Val : *Arr)
		{
			const TSharedPtr<FJsonObject>* Obj = nullptr;
			if (!Val->TryGetObject(Obj)) continue;
			FString PinName;
			FString PinType = TEXT("float");
			(*Obj)->TryGetStringField(TEXT("name"), PinName);
			(*Obj)->TryGetStringField(TEXT("type"), PinType);
			if (PinName.IsEmpty()) continue;
			FNiagaraTypeDefinition TypeDef;
			if (!NiagaraTypeHelpers::ParseTypeDef(PinType, TypeDef)) continue;
			Out.Add({FName(*PinName), TypeDef});
		}
	};
	CollectSpecs(TEXT("inputs"), InputSpecs);
	CollectSpecs(TEXT("outputs"), OutputSpecs);

	int32 InputsAdded = 0;
	int32 OutputsAdded = 0;

	for (UNiagaraNodeCustomHlsl* HlslNode : HlslNodes)
	{
		if (!HlslNode) continue;

		if (bClearExistingPins)
		{
			TArray<UEdGraphPin*> PinsToRemove;
			for (UEdGraphPin* Pin : HlslNode->Pins)
			{
				if (Pin && !NiagaraTypeHelpers::IsAddPin(Pin))
				{
					PinsToRemove.Add(Pin);
				}
			}
			HlslNode->Modify();
			for (UEdGraphPin* Pin : PinsToRemove)
			{
				Pin->BreakAllPinLinks();
				HlslNode->RemovePin(Pin);
			}
		}

		for (const FPinSpec& Spec : InputSpecs)
		{
			if (NiagaraTypeHelpers::AddTypedPin(HlslNode, EGPD_Input, Spec.Type, Spec.Name))
			{
				++InputsAdded;
			}
		}
		for (const FPinSpec& Spec : OutputSpecs)
		{
			if (NiagaraTypeHelpers::AddTypedPin(HlslNode, EGPD_Output, Spec.Type, Spec.Name))
			{
				++OutputsAdded;
			}
		}

		NiagaraTypeHelpers::SetCustomHlslViaReflection(HlslNode, HlslCode);

		if (UEdGraph* Graph = HlslNode->GetGraph())
		{
			Graph->NotifyGraphChanged();
		}
	}

	// Normalize the counts — we doubled them up when iterating the pair
	InputsAdded /= HlslNodes.Num();
	OutputsAdded /= HlslNodes.Num();

	NiagaraHelpers::CompileAndSync(System);
	NiagaraHelpers::NotifyScratchPadScriptChanged(System, ModuleName);

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("module_name"), ModuleName);
	Result->SetNumberField(TEXT("hlsl_length"), HlslCode.Len());
	Result->SetNumberField(TEXT("inputs_added"), InputsAdded);
	Result->SetNumberField(TEXT("outputs_added"), OutputsAdded);
	Result->SetBoolField(TEXT("cleared_existing_pins"), bClearExistingPins);
	return Result;
#else
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only operation"));
#endif
}

// ---------------------------------------------------------------------------
// Custom HLSL pin management (input / output / rename / remove)
// ---------------------------------------------------------------------------

#if WITH_EDITORONLY_DATA
namespace
{
/**
 * Resolve a scratch pad module to the list of CustomHlsl nodes that need to
 * receive a mutation — always the asset's node, plus the Niagara editor's
 * transient edit-copy node when the system is open.
 */
bool ResolveScratchPadHlslNodes(
	const TSharedPtr<FJsonObject>& Params,
	UNiagaraSystem*& OutSystem,
	FString& OutModuleName,
	TArray<UNiagaraNodeCustomHlsl*>& OutNodes,
	FString& OutError)
{
	OutSystem = nullptr;
	OutNodes.Reset();

	FString SystemPath;
	if (!Params->TryGetStringField(TEXT("system_path"), SystemPath))
	{
		OutError = TEXT("Missing 'system_path' parameter");
		return false;
	}
	if (!Params->TryGetStringField(TEXT("module_name"), OutModuleName))
	{
		OutError = TEXT("Missing 'module_name' parameter");
		return false;
	}

	OutSystem = NiagaraHelpers::LoadNiagaraSystem(SystemPath, OutError);
	if (!OutSystem) return false;

	CollectScratchPadHlslNodes(OutSystem, OutModuleName, OutNodes);
	if (OutNodes.Num() == 0)
	{
		OutError = FString::Printf(TEXT("Scratch pad module '%s' not found or has no valid graph"), *OutModuleName);
		return false;
	}
	return true;
}
}
#endif

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleAddNiagaraCustomHlslInput(
	const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
	FString PinName;
	if (!Params->TryGetStringField(TEXT("pin_name"), PinName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'pin_name' parameter"));
	}
	FString PinType = TEXT("float");
	Params->TryGetStringField(TEXT("pin_type"), PinType);

	UNiagaraSystem* System = nullptr;
	FString ModuleName;
	TArray<UNiagaraNodeCustomHlsl*> HlslNodes;
	FString Error;
	if (!ResolveScratchPadHlslNodes(Params, System, ModuleName, HlslNodes, Error))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	FNiagaraTypeDefinition TypeDef;
	if (!NiagaraTypeHelpers::ParseTypeDef(PinType, TypeDef))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Unknown pin type '%s'"), *PinType));
	}

	bool bAny = false;
	for (UNiagaraNodeCustomHlsl* HlslNode : HlslNodes)
	{
		if (NiagaraTypeHelpers::AddTypedPin(HlslNode, EGPD_Input, TypeDef, FName(*PinName)))
		{
			bAny = true;
		}
	}
	if (!bAny)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to add pin"));
	}
	NiagaraHelpers::CompileAndSync(System);
	NiagaraHelpers::NotifyScratchPadScriptChanged(System, ModuleName);

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("pin_name"), PinName);
	Result->SetStringField(TEXT("pin_type"), NiagaraTypeHelpers::TypeDefToString(TypeDef));
	Result->SetNumberField(TEXT("updated_node_count"), HlslNodes.Num());
	return Result;
#else
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only operation"));
#endif
}

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleAddNiagaraCustomHlslOutput(
	const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
	FString PinName;
	if (!Params->TryGetStringField(TEXT("pin_name"), PinName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'pin_name' parameter"));
	}
	FString PinType = TEXT("float");
	Params->TryGetStringField(TEXT("pin_type"), PinType);

	UNiagaraSystem* System = nullptr;
	FString ModuleName;
	TArray<UNiagaraNodeCustomHlsl*> HlslNodes;
	FString Error;
	if (!ResolveScratchPadHlslNodes(Params, System, ModuleName, HlslNodes, Error))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	FNiagaraTypeDefinition TypeDef;
	if (!NiagaraTypeHelpers::ParseTypeDef(PinType, TypeDef))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Unknown pin type '%s'"), *PinType));
	}

	bool bAny = false;
	for (UNiagaraNodeCustomHlsl* HlslNode : HlslNodes)
	{
		if (NiagaraTypeHelpers::AddTypedPin(HlslNode, EGPD_Output, TypeDef, FName(*PinName)))
		{
			bAny = true;
		}
	}
	if (!bAny)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to add pin"));
	}
	NiagaraHelpers::CompileAndSync(System);
	NiagaraHelpers::NotifyScratchPadScriptChanged(System, ModuleName);

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("pin_name"), PinName);
	Result->SetStringField(TEXT("pin_type"), NiagaraTypeHelpers::TypeDefToString(TypeDef));
	Result->SetNumberField(TEXT("updated_node_count"), HlslNodes.Num());
	return Result;
#else
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only operation"));
#endif
}

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleRenameNiagaraCustomHlslPin(
	const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
	FString OldName;
	FString NewName;
	if (!Params->TryGetStringField(TEXT("old_name"), OldName) ||
		!Params->TryGetStringField(TEXT("new_name"), NewName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'old_name' or 'new_name' parameter"));
	}

	UNiagaraSystem* System = nullptr;
	FString ModuleName;
	TArray<UNiagaraNodeCustomHlsl*> HlslNodes;
	FString Error;
	if (!ResolveScratchPadHlslNodes(Params, System, ModuleName, HlslNodes, Error))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	bool bAny = false;
	FString LastError;
	for (UNiagaraNodeCustomHlsl* HlslNode : HlslNodes)
	{
		FString RenameError;
		if (NiagaraTypeHelpers::RenameDynamicPin(HlslNode, FName(*OldName), FName(*NewName), RenameError))
		{
			bAny = true;
		}
		else
		{
			LastError = RenameError;
		}
	}
	if (!bAny)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(LastError.IsEmpty() ? TEXT("Rename failed") : LastError);
	}
	NiagaraHelpers::CompileAndSync(System);
	NiagaraHelpers::NotifyScratchPadScriptChanged(System, ModuleName);

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("old_name"), OldName);
	Result->SetStringField(TEXT("new_name"), NewName);
	Result->SetNumberField(TEXT("updated_node_count"), HlslNodes.Num());
	return Result;
#else
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only operation"));
#endif
}

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleRemoveNiagaraCustomHlslPin(
	const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
	FString PinName;
	if (!Params->TryGetStringField(TEXT("pin_name"), PinName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'pin_name' parameter"));
	}

	UNiagaraSystem* System = nullptr;
	FString ModuleName;
	TArray<UNiagaraNodeCustomHlsl*> HlslNodes;
	FString Error;
	if (!ResolveScratchPadHlslNodes(Params, System, ModuleName, HlslNodes, Error))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	bool bAny = false;
	FString LastError;
	for (UNiagaraNodeCustomHlsl* HlslNode : HlslNodes)
	{
		FString RemoveError;
		if (NiagaraTypeHelpers::RemoveDynamicPinByName(HlslNode, FName(*PinName), RemoveError))
		{
			bAny = true;
		}
		else
		{
			LastError = RemoveError;
		}
	}
	if (!bAny)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(LastError.IsEmpty() ? TEXT("Remove failed") : LastError);
	}
	NiagaraHelpers::CompileAndSync(System);
	NiagaraHelpers::NotifyScratchPadScriptChanged(System, ModuleName);

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("pin_name"), PinName);
	Result->SetNumberField(TEXT("updated_node_count"), HlslNodes.Num());
	return Result;
#else
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only operation"));
#endif
}

// ---------------------------------------------------------------------------
// HandleCreateNiagaraModuleAsset  (standalone module / dynamic input / function asset)
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleCreateNiagaraModuleAsset(
	const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));
	}

	FString ModuleTypeStr = TEXT("module");
	Params->TryGetStringField(TEXT("module_type"), ModuleTypeStr);
	const ENiagaraScriptUsage Usage = ParseModuleType(ModuleTypeStr);

	// Parse path
	FString PackagePath;
	FString AssetName;
	int32 LastSlash = INDEX_NONE;
	if (!AssetPath.FindLastChar('/', LastSlash) || LastSlash <= 0)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Invalid asset_path format. Expected '/Game/Path/AssetName'"));
	}
	PackagePath = AssetPath.Left(LastSlash);
	AssetName = AssetPath.Mid(LastSlash + 1);
	if (AssetName.IsEmpty())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Asset name is empty"));
	}

	FString FullPackagePath = PackagePath / AssetName;
	UPackage* Package = CreatePackage(*FullPackagePath);
	if (!Package)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Failed to create package at '%s'"), *FullPackagePath));
	}
	Package->FullyLoad();

	UNiagaraScript* NewScript = nullptr;
	bool bUsedTemplate = false;

	// Prefer template duplication so the asset matches what the editor "Create Niagara Module Script" wizard produces
	if (UNiagaraScript* Template = LoadDefaultTemplateScript(Usage))
	{
		NewScript = Cast<UNiagaraScript>(StaticDuplicateObject(Template, Package, FName(*AssetName)));
		if (NewScript)
		{
			NewScript->SetFlags(RF_Public | RF_Standalone | RF_Transactional);
			NewScript->SetUsage(Usage);
			bUsedTemplate = true;
		}
	}

	if (!NewScript)
	{
		NewScript = NewObject<UNiagaraScript>(Package, FName(*AssetName), RF_Public | RF_Standalone | RF_Transactional);
		if (!NewScript)
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create Niagara script"));
		}
		NewScript->SetUsage(Usage);

		UNiagaraScriptSource* ScriptSource = NewObject<UNiagaraScriptSource>(
			NewScript, TEXT("ScriptSource"), RF_Transactional);
		UNiagaraGraph* Graph = NewObject<UNiagaraGraph>(
			ScriptSource, TEXT("NiagaraGraph"), RF_Transactional);
		ScriptSource->NodeGraph = Graph;
		NewScript->SetLatestSource(ScriptSource);

		BuildMinimalScratchGraph(NewScript, Graph, Usage);
	}

	// Optional description via reflection
	FString DescriptionStr;
	if (Params->TryGetStringField(TEXT("description"), DescriptionStr))
	{
		FTextProperty* DescProp = CastField<FTextProperty>(
			UNiagaraScript::StaticClass()->FindPropertyByName(TEXT("Description")));
		if (DescProp)
		{
			DescProp->SetPropertyValue_InContainer(NewScript, FText::FromString(DescriptionStr));
		}
	}

	Package->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(NewScript);

	FString PackageFileName = FPackageName::LongPackageNameToFilename(
		FullPackagePath, FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	UPackage::SavePackage(Package, NewScript, *PackageFileName, SaveArgs);

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("asset_path"), NewScript->GetPathName());
	Result->SetStringField(TEXT("asset_name"), AssetName);
	Result->SetStringField(TEXT("module_type"), ModuleTypeStr);
	Result->SetStringField(TEXT("package_path"), PackagePath);
	Result->SetBoolField(TEXT("used_template"), bUsedTemplate);
	return Result;
#else
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only operation"));
#endif
}
