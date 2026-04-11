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
#include "NiagaraNode.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraTypes.h"
#include "NiagaraTypeRegistry.h"

#if WITH_EDITORONLY_DATA
#include "NiagaraNodeParameterMapGet.h"
#include "NiagaraNodeParameterMapSet.h"
#include "NiagaraNodeCustomHlsl.h"
#include "NiagaraScriptVariable.h"
#include "EdGraphSchema_Niagara.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#endif

#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphNode.h"


// ---------------------------------------------------------------------------
// Local helpers
// ---------------------------------------------------------------------------

#if WITH_EDITORONLY_DATA
namespace
{

/** Load a system + scratch pad module's graphs (asset + transient edit-copy).
 *
 *  Callers receive the set of graphs that need to be mutated in lockstep so
 *  live editor views update without a close/reopen. Also returns the module
 *  name via OutModuleName so the subsequent NotifyScratchPadScriptChanged call
 *  can refresh the correct view model entry.
 */
bool ResolveScratchPadGraphs(
	const TSharedPtr<FJsonObject>& Params,
	UNiagaraSystem*& OutSystem,
	FString& OutModuleName,
	TArray<UNiagaraGraph*>& OutGraphs,
	FString& OutError)
{
	OutSystem = nullptr;
	OutGraphs.Reset();

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

	TArray<UNiagaraScript*> Scripts;
	NiagaraHelpers::GetScratchPadScriptPair(OutSystem, OutModuleName, Scripts);
	for (UNiagaraScript* Script : Scripts)
	{
		if (!Script) continue;
		UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(Script->GetLatestSource());
		if (Source && Source->NodeGraph)
		{
			OutGraphs.Add(Source->NodeGraph);
		}
	}
	if (OutGraphs.Num() == 0)
	{
		OutError = FString::Printf(TEXT("Scratch pad module '%s' not found or has no valid graph"), *OutModuleName);
		return false;
	}
	return true;
}

/** Single-graph convenience for callers that don't (yet) handle the pair. */
UNiagaraGraph* ResolveScratchPadGraph(
	const TSharedPtr<FJsonObject>& Params,
	UNiagaraSystem*& OutSystem,
	FString& OutError)
{
	FString ModuleName;
	TArray<UNiagaraGraph*> Graphs;
	if (!ResolveScratchPadGraphs(Params, OutSystem, ModuleName, Graphs, OutError))
	{
		return nullptr;
	}
	return Graphs.Num() > 0 ? Graphs[0] : nullptr;
}

/**
 * Register a parameter in the graph's script variable metadata map. Without
 * this, parameter pins on Map Get / Map Set are recognized as graph variables
 * but the stack system (FNiagaraStackGraphUtilities::GetStackFunctionInputs)
 * won't expose them as configurable module inputs. UNiagaraGraph::AddParameter
 * is not NIAGARAEDITOR_API exported, so we replicate it via the exported
 * GetAllMetaData() accessor + UNiagaraScriptVariable::Init.
 */
void EnsureScriptVariableMetadata(
	UNiagaraGraph* Graph,
	const FNiagaraVariable& Var,
	ENiagaraDefaultMode DefaultMode = ENiagaraDefaultMode::Value)
{
	if (!Graph || !Var.GetType().IsValid())
	{
		return;
	}
	UNiagaraGraph::FScriptVariableMap& MetaMap = Graph->GetAllMetaData();
	if (MetaMap.Contains(Var))
	{
		return; // Already registered
	}

	UNiagaraScriptVariable* ScriptVar = NewObject<UNiagaraScriptVariable>(
		Graph, NAME_None, RF_Transactional);
	if (!ScriptVar) return;

	FNiagaraVariableMetaData Meta;
	ScriptVar->Init(Var, Meta);
	ScriptVar->DefaultMode = DefaultMode;
	MetaMap.Add(Var, ScriptVar);
}

/** Heuristic: choose default mode for a parameter based on its namespace. */
ENiagaraDefaultMode DefaultModeForNamespace(const FString& ParamName)
{
	if (ParamName.StartsWith(TEXT("Module."), ESearchCase::IgnoreCase) ||
		ParamName.StartsWith(TEXT("Input."), ESearchCase::IgnoreCase))
	{
		return ENiagaraDefaultMode::Value;
	}
	// Particles.* / Engine.* / etc. — read from existing parameter map binding
	return ENiagaraDefaultMode::Binding;
}

/**
 * Locate a specific node within a graph. Resolution precedence:
 *  - node_index  (raw Graph->Nodes index)
 *  - node_class  (case-insensitive class name match — first instance)
 *  - node_id     (FGuid string, matches UEdGraphNode::NodeGuid)
 */
UNiagaraNode* ResolveGraphNode(
	UNiagaraGraph* Graph,
	const TSharedPtr<FJsonObject>& Params,
	FString& OutError)
{
	if (!Graph)
	{
		OutError = TEXT("Null graph");
		return nullptr;
	}

	int32 NodeIndex = INDEX_NONE;
	if (Params->TryGetNumberField(TEXT("node_index"), NodeIndex))
	{
		if (Graph->Nodes.IsValidIndex(NodeIndex))
		{
			UNiagaraNode* Node = Cast<UNiagaraNode>(Graph->Nodes[NodeIndex]);
			if (Node) return Node;
			OutError = FString::Printf(TEXT("Node at index %d is not a Niagara node"), NodeIndex);
			return nullptr;
		}
		OutError = FString::Printf(TEXT("node_index %d out of range (have %d nodes)"), NodeIndex, Graph->Nodes.Num());
		return nullptr;
	}

	FString NodeClassName;
	if (Params->TryGetStringField(TEXT("node_class"), NodeClassName))
	{
		const FString ClassLower = NodeClassName.ToLower();
		for (UEdGraphNode* N : Graph->Nodes)
		{
			if (!N) continue;
			FString Name = N->GetClass()->GetName();
			if (Name.Equals(NodeClassName, ESearchCase::IgnoreCase) ||
				Name.Equals(FString::Printf(TEXT("NiagaraNode%s"), *NodeClassName), ESearchCase::IgnoreCase))
			{
				if (UNiagaraNode* Node = Cast<UNiagaraNode>(N))
				{
					return Node;
				}
			}
		}
		OutError = FString::Printf(TEXT("No node of class '%s' found in graph"), *NodeClassName);
		return nullptr;
	}

	FString NodeIdStr;
	if (Params->TryGetStringField(TEXT("node_id"), NodeIdStr))
	{
		FGuid TargetGuid;
		if (FGuid::Parse(NodeIdStr, TargetGuid))
		{
			for (UEdGraphNode* N : Graph->Nodes)
			{
				if (N && N->NodeGuid == TargetGuid)
				{
					if (UNiagaraNode* Node = Cast<UNiagaraNode>(N))
					{
						return Node;
					}
				}
			}
		}
		OutError = FString::Printf(TEXT("Node GUID '%s' not found"), *NodeIdStr);
		return nullptr;
	}

	OutError = TEXT("Must provide one of: node_index, node_class, node_id");
	return nullptr;
}

} // namespace
#endif // WITH_EDITORONLY_DATA

// ---------------------------------------------------------------------------
// HandleListNiagaraAvailableParameters
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleListNiagaraAvailableParameters(
	const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
	FString Filter;
	Params->TryGetStringField(TEXT("filter"), Filter);

	FString Namespace = TEXT("all");
	Params->TryGetStringField(TEXT("namespace"), Namespace);
	const FString NamespaceLower = Namespace.ToLower();

	int32 MaxResults = 500;
	Params->TryGetNumberField(TEXT("max_results"), MaxResults);

	// Optional: filter to the scope of a specific scratch pad script (adds Module.* + Input.* etc.)
	FString SystemPath;
	FString ModuleName;
	Params->TryGetStringField(TEXT("system_path"), SystemPath);
	Params->TryGetStringField(TEXT("module_name"), ModuleName);

	TArray<TSharedPtr<FJsonValue>> ParamsJson;

	auto TryAdd = [&](const FString& Name, const FString& Type, const FString& Scope, const FString& Source)
	{
		if (ParamsJson.Num() >= MaxResults) return;
		if (!Filter.IsEmpty() && !Name.Contains(Filter, ESearchCase::IgnoreCase)) return;

		if (NamespaceLower != TEXT("all"))
		{
			const FString Prefix = NamespaceLower + TEXT(".");
			if (!Name.StartsWith(Prefix, ESearchCase::IgnoreCase) &&
				!Scope.Equals(NamespaceLower, ESearchCase::IgnoreCase))
			{
				return;
			}
		}

		auto J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("name"), Name);
		J->SetStringField(TEXT("type"), Type);
		J->SetStringField(TEXT("scope"), Scope);
		J->SetStringField(TEXT("source"), Source);
		ParamsJson.Add(MakeShared<FJsonValueObject>(J));
	};

	// ---- Engine namespace (always available) ----
	static const TPair<const TCHAR*, const TCHAR*> EngineParams[] = {
		{ TEXT("Engine.DeltaTime"), TEXT("float") },
		{ TEXT("Engine.InverseDeltaTime"), TEXT("float") },
		{ TEXT("Engine.Time"), TEXT("float") },
		{ TEXT("Engine.RealTime"), TEXT("float") },
		{ TEXT("Engine.Owner.Position"), TEXT("Position") },
		{ TEXT("Engine.Owner.Velocity"), TEXT("Vector") },
		{ TEXT("Engine.Owner.Rotation"), TEXT("Quat") },
		{ TEXT("Engine.Owner.Scale"), TEXT("Vector") },
		{ TEXT("Engine.Owner.SystemLocalToWorld"), TEXT("Matrix") },
		{ TEXT("Engine.Owner.SystemWorldToLocal"), TEXT("Matrix") },
		{ TEXT("Engine.Owner.LODDistance"), TEXT("float") },
		{ TEXT("Engine.Emitter.NumParticles"), TEXT("int") },
		{ TEXT("Engine.Emitter.SpawnCountScale"), TEXT("float") },
		{ TEXT("Engine.System.Age"), TEXT("float") },
		{ TEXT("Engine.System.TickCount"), TEXT("int") },
	};
	for (const auto& E : EngineParams) { TryAdd(E.Key, E.Value, TEXT("engine"), TEXT("well_known")); }

	// ---- Well-known particle attributes ----
	static const TPair<const TCHAR*, const TCHAR*> ParticleAttrs[] = {
		{ TEXT("Particles.Position"), TEXT("Position") },
		{ TEXT("Particles.Velocity"), TEXT("Vector") },
		{ TEXT("Particles.Color"), TEXT("LinearColor") },
		{ TEXT("Particles.SpriteSize"), TEXT("Vector2D") },
		{ TEXT("Particles.SpriteRotation"), TEXT("float") },
		{ TEXT("Particles.SpriteFacing"), TEXT("Vector") },
		{ TEXT("Particles.SpriteAlignment"), TEXT("Vector") },
		{ TEXT("Particles.Scale"), TEXT("Vector") },
		{ TEXT("Particles.Lifetime"), TEXT("float") },
		{ TEXT("Particles.Age"), TEXT("float") },
		{ TEXT("Particles.NormalizedAge"), TEXT("float") },
		{ TEXT("Particles.Mass"), TEXT("float") },
		{ TEXT("Particles.MeshOrientation"), TEXT("Quat") },
		{ TEXT("Particles.UniqueID"), TEXT("int") },
		{ TEXT("Particles.ID"), TEXT("NiagaraID") },
		{ TEXT("Particles.RibbonID"), TEXT("NiagaraID") },
		{ TEXT("Particles.RibbonWidth"), TEXT("float") },
		{ TEXT("Particles.RibbonFacing"), TEXT("Vector") },
		{ TEXT("Particles.RibbonTwist"), TEXT("float") },
		{ TEXT("Particles.MaterialRandom"), TEXT("float") },
		{ TEXT("Particles.DynamicMaterialParameter"), TEXT("Vector4") },
		{ TEXT("Particles.SubImageIndex"), TEXT("float") },
		{ TEXT("Particles.CameraOffset"), TEXT("float") },
		{ TEXT("Particles.LightRadius"), TEXT("float") },
	};
	for (const auto& P : ParticleAttrs) { TryAdd(P.Key, P.Value, TEXT("particles"), TEXT("well_known")); }

	// ---- System, Emitter, Transient placeholder namespaces (defined by user or modules) ----
	// If the caller scoped to a specific scratch pad module, pull actual variables from its graph.
	if (!SystemPath.IsEmpty() && !ModuleName.IsEmpty())
	{
		FString LoadError;
		UNiagaraSystem* System = NiagaraHelpers::LoadNiagaraSystem(SystemPath, LoadError);
		if (System)
		{
			// Collect user parameters from the exposed parameters collection
			TArray<FNiagaraVariable> UserVars;
			System->GetExposedParameters().GetParameters(UserVars);
			for (const FNiagaraVariable& V : UserVars)
			{
				TryAdd(V.GetName().ToString(), V.GetType().GetName(), TEXT("user"), TEXT("exposed"));
			}

			// Walk emitter attribute scripts for rapid-iter / variable-map params
			for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
			{
				FVersionedNiagaraEmitterData* Data = Handle.GetEmitterData();
				if (!Data) continue;
				UNiagaraScript* ScriptsToWalk[] = {
					Data->SpawnScriptProps.Script,
					Data->UpdateScriptProps.Script,
					Data->EmitterSpawnScriptProps.Script,
					Data->EmitterUpdateScriptProps.Script,
				};
				for (UNiagaraScript* S : ScriptsToWalk)
				{
					if (!S) continue;
					TArrayView<const FNiagaraVariableWithOffset> Rapid = S->RapidIterationParameters.ReadParameterVariables();
					for (const FNiagaraVariableWithOffset& V : Rapid)
					{
						TryAdd(V.GetName().ToString(), V.GetType().GetName(), TEXT("rapid_iteration"), TEXT("emitter"));
					}
				}
			}

			// Scratch pad module's own graph parameters
			for (UNiagaraScript* S : System->ScratchPadScripts)
			{
				if (!S || !S->GetName().Equals(ModuleName, ESearchCase::IgnoreCase)) continue;
				UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(S->GetLatestSource());
				if (!Source || !Source->NodeGraph) continue;

				const UNiagaraGraph::FScriptVariableMap& MetaMap = Source->NodeGraph->GetAllMetaData();
				for (const TPair<FNiagaraVariable, TObjectPtr<UNiagaraScriptVariable>>& Pair : MetaMap)
				{
					const FNiagaraVariable& V = Pair.Key;
					const FString Name = V.GetName().ToString();
					FString Scope = TEXT("module");
					if (Name.StartsWith(TEXT("Module."))) Scope = TEXT("module");
					else if (Name.StartsWith(TEXT("Input."))) Scope = TEXT("input");
					else if (Name.StartsWith(TEXT("Output."))) Scope = TEXT("output");
					else if (Name.StartsWith(TEXT("Transient."))) Scope = TEXT("transient");
					else if (Name.StartsWith(TEXT("User."))) Scope = TEXT("user");
					TryAdd(Name, V.GetType().GetName(), Scope, TEXT("script_graph"));
				}
				break;
			}
		}
	}

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("filter"), Filter);
	Result->SetStringField(TEXT("namespace"), Namespace);
	Result->SetNumberField(TEXT("count"), ParamsJson.Num());
	Result->SetArrayField(TEXT("parameters"), ParamsJson);
	return Result;
#else
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only operation"));
#endif
}

// ---------------------------------------------------------------------------
// HandleAddNiagaraMapGetPin  /  HandleAddNiagaraMapSetPin
// ---------------------------------------------------------------------------

static TSharedPtr<FJsonObject> AddMapPinImpl(
	FEpicUnrealMCPNiagaraCommands* Self,
	const TSharedPtr<FJsonObject>& Params,
	EEdGraphPinDirection Direction,
	UClass* MapNodeClass,
	const FString& OperationLabel)
{
#if WITH_EDITORONLY_DATA
	FString ParamName;
	if (!Params->TryGetStringField(TEXT("parameter_name"), ParamName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'parameter_name' parameter"));
	}
	FString TypeStr = TEXT("float");
	Params->TryGetStringField(TEXT("parameter_type"), TypeStr);

	UNiagaraSystem* System = nullptr;
	FString ModuleName;
	TArray<UNiagaraGraph*> Graphs;
	FString Error;
	if (!ResolveScratchPadGraphs(Params, System, ModuleName, Graphs, Error))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	FNiagaraTypeDefinition TypeDef;
	if (!NiagaraTypeHelpers::ParseTypeDef(TypeStr, TypeDef))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Unknown parameter type '%s'"), *TypeStr));
	}

	int32 Applied = 0;
	for (UNiagaraGraph* Graph : Graphs)
	{
		if (!Graph) continue;

		// Locate target map node on THIS graph (asset or transient edit copy)
		UNiagaraNode* TargetNode = nullptr;
		for (UEdGraphNode* N : Graph->Nodes)
		{
			if (N && N->IsA(MapNodeClass))
			{
				TargetNode = Cast<UNiagaraNode>(N);
				break;
			}
		}
		if (!TargetNode) continue;

		if (NiagaraTypeHelpers::AddTypedPin(TargetNode, Direction, TypeDef, FName(*ParamName)))
		{
			// Register the parameter in the graph's script variable metadata map.
			// Without this, the stack input system won't expose Module.* pins as
			// configurable inputs (they'd just be unreachable graph variables).
			FNiagaraVariable Var(TypeDef, FName(*ParamName));
			EnsureScriptVariableMetadata(Graph, Var, DefaultModeForNamespace(ParamName));

			++Applied;
			Graph->NotifyGraphChanged();
		}
	}
	if (Applied == 0)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("No %s node in scratch pad graph"), *OperationLabel));
	}

	NiagaraHelpers::CompileAndSync(System);
	NiagaraHelpers::NotifyScratchPadScriptChanged(System, ModuleName);

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("operation"), OperationLabel);
	Result->SetStringField(TEXT("parameter_name"), ParamName);
	Result->SetStringField(TEXT("parameter_type"), NiagaraTypeHelpers::TypeDefToString(TypeDef));
	Result->SetNumberField(TEXT("updated_graph_count"), Applied);
	return Result;
#else
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only operation"));
#endif
}

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleAddNiagaraMapGetPin(
	const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
	return AddMapPinImpl(this, Params, EGPD_Output, UNiagaraNodeParameterMapGet::StaticClass(), TEXT("map_get"));
#else
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only operation"));
#endif
}

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleAddNiagaraMapSetPin(
	const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
	return AddMapPinImpl(this, Params, EGPD_Input, UNiagaraNodeParameterMapSet::StaticClass(), TEXT("map_set"));
#else
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only operation"));
#endif
}

// ---------------------------------------------------------------------------
// HandleAddNiagaraNodePin  (generic — any node that supports dynamic pins)
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleAddNiagaraNodePin(
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
	FString DirectionStr = TEXT("input");
	Params->TryGetStringField(TEXT("direction"), DirectionStr);
	const EEdGraphPinDirection Direction = DirectionStr.Equals(TEXT("output"), ESearchCase::IgnoreCase)
		? EGPD_Output : EGPD_Input;

	UNiagaraSystem* System = nullptr;
	FString ModuleName;
	TArray<UNiagaraGraph*> Graphs;
	FString GraphError;
	if (!ResolveScratchPadGraphs(Params, System, ModuleName, Graphs, GraphError))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(GraphError);
	}

	FNiagaraTypeDefinition TypeDef;
	if (!NiagaraTypeHelpers::ParseTypeDef(PinType, TypeDef))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Unknown pin type '%s'"), *PinType));
	}

	int32 Applied = 0;
	FString LastNodeClass;
	for (UNiagaraGraph* Graph : Graphs)
	{
		FString NodeError;
		UNiagaraNode* Node = ResolveGraphNode(Graph, Params, NodeError);
		if (!Node) continue;
		LastNodeClass = Node->GetClass()->GetName();
		if (NiagaraTypeHelpers::AddTypedPin(Node, Direction, TypeDef, FName(*PinName)))
		{
			++Applied;
			Graph->NotifyGraphChanged();
		}
	}
	if (Applied == 0)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to add pin on any graph"));
	}
	NiagaraHelpers::CompileAndSync(System);
	NiagaraHelpers::NotifyScratchPadScriptChanged(System, ModuleName);

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("pin_name"), PinName);
	Result->SetStringField(TEXT("pin_type"), NiagaraTypeHelpers::TypeDefToString(TypeDef));
	Result->SetStringField(TEXT("direction"), DirectionStr);
	Result->SetStringField(TEXT("node_class"), LastNodeClass);
	Result->SetNumberField(TEXT("updated_graph_count"), Applied);
	return Result;
#else
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only operation"));
#endif
}

// ---------------------------------------------------------------------------
// HandleRenameNiagaraNodePin  /  HandleRemoveNiagaraNodePin
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleRenameNiagaraNodePin(
	const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
	FString OldName, NewName;
	if (!Params->TryGetStringField(TEXT("old_name"), OldName) ||
		!Params->TryGetStringField(TEXT("new_name"), NewName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'old_name' or 'new_name' parameter"));
	}

	UNiagaraSystem* System = nullptr;
	FString ModuleName;
	TArray<UNiagaraGraph*> Graphs;
	FString GraphError;
	if (!ResolveScratchPadGraphs(Params, System, ModuleName, Graphs, GraphError))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(GraphError);
	}

	int32 Applied = 0;
	FString LastError;
	for (UNiagaraGraph* Graph : Graphs)
	{
		FString NodeError;
		UNiagaraNode* Node = ResolveGraphNode(Graph, Params, NodeError);
		if (!Node) continue;
		FString RenameError;
		if (NiagaraTypeHelpers::RenameDynamicPin(Node, FName(*OldName), FName(*NewName), RenameError))
		{
			++Applied;
			Graph->NotifyGraphChanged();
		}
		else
		{
			LastError = RenameError;
		}
	}
	if (Applied == 0)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(LastError.IsEmpty() ? TEXT("Rename failed") : LastError);
	}
	NiagaraHelpers::CompileAndSync(System);
	NiagaraHelpers::NotifyScratchPadScriptChanged(System, ModuleName);

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("old_name"), OldName);
	Result->SetStringField(TEXT("new_name"), NewName);
	Result->SetNumberField(TEXT("updated_graph_count"), Applied);
	return Result;
#else
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only operation"));
#endif
}

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleRemoveNiagaraNodePin(
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
	TArray<UNiagaraGraph*> Graphs;
	FString GraphError;
	if (!ResolveScratchPadGraphs(Params, System, ModuleName, Graphs, GraphError))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(GraphError);
	}

	int32 Applied = 0;
	FString LastError;
	for (UNiagaraGraph* Graph : Graphs)
	{
		FString NodeError;
		UNiagaraNode* Node = ResolveGraphNode(Graph, Params, NodeError);
		if (!Node) continue;
		FString RemoveError;
		if (NiagaraTypeHelpers::RemoveDynamicPinByName(Node, FName(*PinName), RemoveError))
		{
			++Applied;
			Graph->NotifyGraphChanged();
		}
		else
		{
			LastError = RemoveError;
		}
	}
	if (Applied == 0)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(LastError.IsEmpty() ? TEXT("Remove failed") : LastError);
	}
	NiagaraHelpers::CompileAndSync(System);
	NiagaraHelpers::NotifyScratchPadScriptChanged(System, ModuleName);

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("pin_name"), PinName);
	Result->SetNumberField(TEXT("updated_graph_count"), Applied);
	return Result;
#else
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only operation"));
#endif
}

// ---------------------------------------------------------------------------
// HandleConnectNiagaraPins  /  HandleDisconnectNiagaraPins
// Convenience for scripted graph wiring between nodes inside a scratch pad.
// ---------------------------------------------------------------------------

#if WITH_EDITORONLY_DATA
namespace
{
UEdGraphPin* FindPinByName(UNiagaraNode* Node, const FName& Name, EEdGraphPinDirection Direction)
{
	if (!Node) return nullptr;
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin && Pin->Direction == Direction && Pin->PinName == Name &&
			!NiagaraTypeHelpers::IsAddPin(Pin))
		{
			return Pin;
		}
	}
	return nullptr;
}
}
#endif

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleConnectNiagaraPins(
	const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
	FString FromPin, ToPin;
	if (!Params->TryGetStringField(TEXT("from_pin"), FromPin) ||
		!Params->TryGetStringField(TEXT("to_pin"), ToPin))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'from_pin' or 'to_pin'"));
	}

	UNiagaraSystem* System = nullptr;
	FString ModuleName;
	TArray<UNiagaraGraph*> Graphs;
	FString GraphError;
	if (!ResolveScratchPadGraphs(Params, System, ModuleName, Graphs, GraphError))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(GraphError);
	}

	const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();
	int32 Applied = 0;
	FString LastError;
	for (UNiagaraGraph* Graph : Graphs)
	{
		auto ResolveSide = [&](const TCHAR* Prefix, FString& OutErr) -> UNiagaraNode*
		{
			TSharedPtr<FJsonObject> Local = MakeShared<FJsonObject>();
			FString S;
			double D;
			if (Params->TryGetStringField(FString::Printf(TEXT("%s_node_class"), Prefix), S)) Local->SetStringField(TEXT("node_class"), S);
			if (Params->TryGetStringField(FString::Printf(TEXT("%s_node_id"), Prefix), S))    Local->SetStringField(TEXT("node_id"), S);
			if (Params->TryGetNumberField(FString::Printf(TEXT("%s_node_index"), Prefix), D)) Local->SetNumberField(TEXT("node_index"), D);
			return ResolveGraphNode(Graph, Local, OutErr);
		};

		FString FromErr, ToErr;
		UNiagaraNode* FromNode = ResolveSide(TEXT("from"), FromErr);
		UNiagaraNode* ToNode = ResolveSide(TEXT("to"), ToErr);
		if (!FromNode || !ToNode)
		{
			LastError = FString::Printf(TEXT("%s%s"),
				!FromNode ? *FString::Printf(TEXT("from: %s "), *FromErr) : TEXT(""),
				!ToNode   ? *FString::Printf(TEXT("to: %s"),   *ToErr)   : TEXT(""));
			continue;
		}

		UEdGraphPin* FromPinObj = FindPinByName(FromNode, FName(*FromPin), EGPD_Output);
		UEdGraphPin* ToPinObj   = FindPinByName(ToNode,   FName(*ToPin),   EGPD_Input);
		if (!FromPinObj || !ToPinObj)
		{
			LastError = TEXT("Could not resolve both pins by name/direction");
			continue;
		}

		if (!Schema->TryCreateConnection(FromPinObj, ToPinObj))
		{
			LastError = TEXT("Schema refused the connection (type mismatch?)");
			continue;
		}
		Graph->NotifyGraphChanged();
		++Applied;
	}
	if (Applied == 0)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(LastError.IsEmpty() ? TEXT("Connect failed") : LastError);
	}
	NiagaraHelpers::CompileAndSync(System);
	NiagaraHelpers::NotifyScratchPadScriptChanged(System, ModuleName);

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("from"), FromPin);
	Result->SetStringField(TEXT("to"), ToPin);
	Result->SetNumberField(TEXT("updated_graph_count"), Applied);
	return Result;
#else
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only operation"));
#endif
}

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleDisconnectNiagaraPins(
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
	TArray<UNiagaraGraph*> Graphs;
	FString GraphError;
	if (!ResolveScratchPadGraphs(Params, System, ModuleName, Graphs, GraphError))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(GraphError);
	}

	int32 Broken = 0;
	for (UNiagaraGraph* Graph : Graphs)
	{
		FString NodeError;
		UNiagaraNode* Node = ResolveGraphNode(Graph, Params, NodeError);
		if (!Node) continue;
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->PinName == FName(*PinName))
			{
				Broken += Pin->LinkedTo.Num();
				Pin->BreakAllPinLinks();
			}
		}
		Graph->NotifyGraphChanged();
	}
	if (Broken == 0)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Pin '%s' had no connections to break"), *PinName));
	}
	NiagaraHelpers::CompileAndSync(System);
	NiagaraHelpers::NotifyScratchPadScriptChanged(System, ModuleName);

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("pin_name"), PinName);
	Result->SetNumberField(TEXT("broken_links"), Broken);
	return Result;
#else
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only operation"));
#endif
}
