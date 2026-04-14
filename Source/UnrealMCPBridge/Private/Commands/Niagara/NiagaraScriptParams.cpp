#include "Commands/EpicUnrealMCPNiagaraCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "NiagaraHelpers.h"
#include "NiagaraPropertyIntrospection.h"

#include "NiagaraSystem.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraGraph.h"
#include "NiagaraTypes.h"
#include "NiagaraTypeRegistry.h"

#if WITH_EDITORONLY_DATA
#include "NiagaraNode.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraNodeInput.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeOp.h"
#include "NiagaraNodeParameterMapGet.h"
#include "NiagaraNodeParameterMapSet.h"
#include "NiagaraNodeReroute.h"
#include "NiagaraScriptVariable.h"
#include "NiagaraDataInterface.h"
#include "EdGraphSchema_Niagara.h"
#include "EditorAssetLibrary.h"
#endif

#include "EdGraph/EdGraphPin.h"


// ---------------------------------------------------------------------------
// Phase 4 — Script input / output parameter management + type registry
// Phase 5 — Arbitrary node creation / deletion in a scratch pad or standalone
//           script graph
// ---------------------------------------------------------------------------

#if WITH_EDITORONLY_DATA
namespace
{

/** Resolve a graph + script (both scratch pad edit copy and asset if open).
 *  Returns the edit-copy graph when an editor is open, else the asset graph. */
bool ResolveScriptAndGraph(
    const TSharedPtr<FJsonObject>& Params,
    UNiagaraSystem*& OutSystem,
    FString& OutModuleName,
    UNiagaraScript*& OutScript,
    UNiagaraGraph*& OutGraph,
    FString& OutError)
{
    OutSystem = nullptr;
    OutScript = nullptr;
    OutGraph = nullptr;
    OutModuleName.Reset();

    FString SystemPath, ModuleName;
    Params->TryGetStringField(TEXT("system_path"), SystemPath);
    Params->TryGetStringField(TEXT("module_name"), ModuleName);

    if (!SystemPath.IsEmpty() && !ModuleName.IsEmpty())
    {
        OutSystem = NiagaraHelpers::LoadNiagaraSystem(SystemPath, OutError);
        if (!OutSystem) return false;
        OutModuleName = ModuleName;

        TArray<UNiagaraScript*> Scripts;
        NiagaraHelpers::GetScratchPadScriptPair(OutSystem, ModuleName, Scripts);
        if (Scripts.Num() == 0)
        {
            OutError = FString::Printf(TEXT("Scratch pad module '%s' not found"), *ModuleName);
            return false;
        }
        // Prefer edit-copy (when editor open): last element. Also keep original.
        OutScript = Scripts.Last();
        if (UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(OutScript->GetLatestSource()))
        {
            OutGraph = Source->NodeGraph;
        }
        if (!OutGraph)
        {
            OutError = TEXT("Script has no graph source");
            return false;
        }
        return true;
    }

    FString ScriptPath;
    if (Params->TryGetStringField(TEXT("script_path"), ScriptPath))
    {
        OutScript = Cast<UNiagaraScript>(UEditorAssetLibrary::LoadAsset(ScriptPath));
        if (!OutScript)
        {
            OutError = FString::Printf(TEXT("Script asset not found: %s"), *ScriptPath);
            return false;
        }
        if (UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(OutScript->GetLatestSource()))
        {
            OutGraph = Source->NodeGraph;
        }
        if (!OutGraph)
        {
            OutError = FString::Printf(TEXT("Script has no graph: %s"), *ScriptPath);
            return false;
        }
        return true;
    }

    OutError = TEXT("Provide either (system_path + module_name) or script_path");
    return false;
}

/** Collect ALL graphs we need to mutate to keep asset + edit-copy in sync. */
TArray<UNiagaraGraph*> CollectMutationGraphs(
    UNiagaraSystem* System,
    const FString& ModuleName,
    UNiagaraScript* DirectScript)
{
    TArray<UNiagaraGraph*> Graphs;
    if (System && !ModuleName.IsEmpty())
    {
        TArray<UNiagaraScript*> Scripts;
        NiagaraHelpers::GetScratchPadScriptPair(System, ModuleName, Scripts);
        for (UNiagaraScript* S : Scripts)
        {
            if (!S) continue;
            if (UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(S->GetLatestSource()))
            {
                if (Source->NodeGraph) Graphs.AddUnique(Source->NodeGraph);
            }
        }
    }
    else if (DirectScript)
    {
        if (UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(DirectScript->GetLatestSource()))
        {
            if (Source->NodeGraph) Graphs.Add(Source->NodeGraph);
        }
    }
    return Graphs;
}

/** Find the output node whose script usage matches the script's own usage
 *  (for standalone scripts there's typically exactly one). */
UNiagaraNodeOutput* FindOutputNode(UNiagaraGraph* Graph)
{
    if (!Graph) return nullptr;
    TArray<UNiagaraNodeOutput*> Outputs;
    Graph->GetNodesOfClass<UNiagaraNodeOutput>(Outputs);
    return Outputs.Num() > 0 ? Outputs[0] : nullptr;
}

/** Serialize FNiagaraVariable for JSON output. */
TSharedPtr<FJsonObject> VariableToJson(const FNiagaraVariable& Var, int32 Index)
{
    auto Obj = MakeShared<FJsonObject>();
    Obj->SetNumberField(TEXT("index"), Index);
    Obj->SetStringField(TEXT("name"), Var.GetName().ToString());
    Obj->SetStringField(TEXT("type"), Var.GetType().GetName());
    Obj->SetBoolField(TEXT("data_interface"), Var.IsDataInterface());
    return Obj;
}

} // namespace
#endif


// ---------------------------------------------------------------------------
// HandleListNiagaraScriptParameters
// Lists BOTH input parameters (graph script variables — Input.*, Module.*) and
// output parameters (UNiagaraNodeOutput::Outputs on the script's output node).
//
// Mirrors the "Input Parameters" and "Output Parameters" sections of the
// details panel shown in screenshots 4/5.
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleListNiagaraScriptParameters(
    const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
    FString Error;
    UNiagaraSystem* System = nullptr;
    FString ModuleName;
    UNiagaraScript* Script = nullptr;
    UNiagaraGraph* Graph = nullptr;
    if (!ResolveScriptAndGraph(Params, System, ModuleName, Script, Graph, Error))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    // ---- Outputs: walk the graph's output node. ----
    TArray<TSharedPtr<FJsonValue>> OutputParams;
    UNiagaraNodeOutput* OutputNode = FindOutputNode(Graph);
    if (OutputNode)
    {
        for (int32 i = 0; i < OutputNode->Outputs.Num(); ++i)
        {
            OutputParams.Add(MakeShared<FJsonValueObject>(
                VariableToJson(OutputNode->Outputs[i], i)));
        }
    }

    // ---- Inputs: walk the graph's script variable metadata. ----
    TArray<TSharedPtr<FJsonValue>> InputParams;
    int32 InputIndex = 0;
    const UNiagaraGraph::FScriptVariableMap& MetaMap = Graph->GetAllMetaData();
    for (const TPair<FNiagaraVariable, UNiagaraScriptVariable*>& Pair : MetaMap)
    {
        const FString VarName = Pair.Key.GetName().ToString();
        // Only expose entries that look like script-side inputs (exclude
        // Particles.*, Engine.* etc. — those come from upstream scope).
        const bool bIsInputLike =
            VarName.StartsWith(TEXT("Module."), ESearchCase::IgnoreCase) ||
            VarName.StartsWith(TEXT("Input."), ESearchCase::IgnoreCase) ||
            VarName.StartsWith(TEXT("User."), ESearchCase::IgnoreCase);
        if (!bIsInputLike) continue;

        auto J = VariableToJson(Pair.Key, InputIndex++);
        if (Pair.Value)
        {
            J->SetStringField(TEXT("default_mode"),
                StaticEnum<ENiagaraDefaultMode>()
                    ->GetNameStringByValue((int64)Pair.Value->DefaultMode));
        }
        InputParams.Add(MakeShared<FJsonValueObject>(J));
    }

    auto R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetStringField(TEXT("script_path"), Script->GetPathName());
    R->SetArrayField(TEXT("outputs"), OutputParams);
    R->SetArrayField(TEXT("inputs"), InputParams);
    return R;
#else
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only command"));
#endif
}


// ---------------------------------------------------------------------------
// HandleAddNiagaraScriptParameter
// Params:
//   (graph resolver) + direction ("output" | "input")
//                    + name
//                    + type   (from FNiagaraTypeRegistry — "Vector", "float", "int32", etc.)
// For direction="output" — appends to UNiagaraNodeOutput::Outputs + notifies.
// For direction="input"  — registers a Module.<Name> script variable on graph.
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleAddNiagaraScriptParameter(
    const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
    FString Error;
    UNiagaraSystem* System = nullptr;
    FString ModuleName;
    UNiagaraScript* Script = nullptr;
    UNiagaraGraph* PrimaryGraph = nullptr;
    if (!ResolveScriptAndGraph(Params, System, ModuleName, Script, PrimaryGraph, Error))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    FString Direction = TEXT("output");
    Params->TryGetStringField(TEXT("direction"), Direction);

    FString ParamName, TypeName;
    if (!Params->TryGetStringField(TEXT("name"), ParamName) ||
        !Params->TryGetStringField(TEXT("type"), TypeName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            TEXT("Missing 'name' or 'type'"));
    }

    FNiagaraTypeDefinition TypeDef;
    if (!NiagaraIntrospection::ResolveTypeName(TypeName, TypeDef) || !TypeDef.IsValid())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Unknown Niagara type '%s'"), *TypeName));
    }

    TArray<UNiagaraGraph*> MutGraphs = CollectMutationGraphs(System, ModuleName, Script);

    for (UNiagaraGraph* G : MutGraphs)
    {
        G->Modify();
        if (Direction.Equals(TEXT("output"), ESearchCase::IgnoreCase))
        {
            if (UNiagaraNodeOutput* OutNode = FindOutputNode(G))
            {
                OutNode->Modify();
                FNiagaraVariable NewVar(TypeDef, FName(*ParamName));
                // Avoid duplicates.
                bool bExists = false;
                for (const FNiagaraVariable& V : OutNode->Outputs)
                {
                    if (V.GetName() == NewVar.GetName()) { bExists = true; break; }
                }
                if (!bExists)
                {
                    OutNode->Outputs.Add(NewVar);
                    // Dispatch PostEditChangeProperty with a synthetic event
                    // pointing at the Outputs property — same path the
                    // details panel takes when the user edits the array.
                    // UNiagaraNodeOutput::PostEditChangeProperty calls
                    // ReallocatePins when Property != null, then Super
                    // broadcasts the change so the graph editor repaints
                    // without needing the user to tickle a name field.
                    if (FProperty* OutputsProp = UNiagaraNodeOutput::StaticClass()
                            ->FindPropertyByName(TEXT("Outputs")))
                    {
                        FPropertyChangedEvent Evt(OutputsProp, EPropertyChangeType::ArrayAdd);
                        OutNode->PostEditChangeProperty(Evt);
                    }
                }
            }
        }
        else
        {
            // Input: register as Module.<Name> script variable.
            FString FullName = ParamName;
            if (!FullName.Contains(TEXT(".")))
            {
                FullName = FString::Printf(TEXT("Module.%s"), *ParamName);
            }
            FNiagaraVariable NewVar(TypeDef, FName(*FullName));
            UNiagaraGraph::FScriptVariableMap& MetaMap = G->GetAllMetaData();
            if (!MetaMap.Contains(NewVar))
            {
                UNiagaraScriptVariable* SV =
                    NewObject<UNiagaraScriptVariable>(G, NAME_None, RF_Transactional);
                FNiagaraVariableMetaData Meta;
                SV->Init(NewVar, Meta);
                SV->DefaultMode = FullName.StartsWith(TEXT("Module."))
                    ? ENiagaraDefaultMode::Value
                    : ENiagaraDefaultMode::Binding;
                MetaMap.Add(NewVar, SV);
            }
        }
        G->NotifyGraphChanged();
    }

    if (System && !ModuleName.IsEmpty())
    {
        NiagaraHelpers::NotifyScratchPadScriptChanged(System, ModuleName);
    }
    Script->MarkPackageDirty();

    auto R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetStringField(TEXT("direction"), Direction);
    R->SetStringField(TEXT("name"), ParamName);
    R->SetStringField(TEXT("type"), TypeDef.GetName());
    return R;
#else
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only command"));
#endif
}


// ---------------------------------------------------------------------------
// HandleRemoveNiagaraScriptParameter
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleRemoveNiagaraScriptParameter(
    const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
    FString Error;
    UNiagaraSystem* System = nullptr;
    FString ModuleName;
    UNiagaraScript* Script = nullptr;
    UNiagaraGraph* PrimaryGraph = nullptr;
    if (!ResolveScriptAndGraph(Params, System, ModuleName, Script, PrimaryGraph, Error))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    FString Direction = TEXT("output");
    Params->TryGetStringField(TEXT("direction"), Direction);
    FString ParamName;
    if (!Params->TryGetStringField(TEXT("name"), ParamName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name'"));
    }

    TArray<UNiagaraGraph*> MutGraphs = CollectMutationGraphs(System, ModuleName, Script);
    int32 Removed = 0;
    for (UNiagaraGraph* G : MutGraphs)
    {
        G->Modify();
        if (Direction.Equals(TEXT("output"), ESearchCase::IgnoreCase))
        {
            if (UNiagaraNodeOutput* OutNode = FindOutputNode(G))
            {
                OutNode->Modify();
                const int32 Before = OutNode->Outputs.Num();
                OutNode->Outputs.RemoveAll([&](const FNiagaraVariable& V)
                {
                    return V.GetName() == FName(*ParamName);
                });
                if (OutNode->Outputs.Num() != Before)
                {
                    if (FProperty* OutputsProp = UNiagaraNodeOutput::StaticClass()
                            ->FindPropertyByName(TEXT("Outputs")))
                    {
                        FPropertyChangedEvent Evt(OutputsProp, EPropertyChangeType::ArrayRemove);
                        OutNode->PostEditChangeProperty(Evt);
                    }
                    ++Removed;
                }
            }
        }
        else
        {
            UNiagaraGraph::FScriptVariableMap& MetaMap = G->GetAllMetaData();
            FNiagaraVariable Key;
            for (const TPair<FNiagaraVariable, UNiagaraScriptVariable*>& Pair : MetaMap)
            {
                if (Pair.Key.GetName() == FName(*ParamName))
                {
                    Key = Pair.Key;
                    break;
                }
            }
            if (Key.GetType().IsValid())
            {
                MetaMap.Remove(Key);
                ++Removed;

                // Cascade cleanup: remove any Map Get / Map Set pins that
                // reference this variable name across the graph, so the
                // graph doesn't keep orphan output/input pins pointing at
                // a deleted variable. This mirrors what the editor's
                // "Delete Parameter" action does behind the scenes.
                const FName ParamFName(*ParamName);
                for (UEdGraphNode* N : G->Nodes)
                {
                    if (!N) continue;
                    const bool bIsMapGet = N->IsA(UNiagaraNodeParameterMapGet::StaticClass());
                    const bool bIsMapSet = N->IsA(UNiagaraNodeParameterMapSet::StaticClass());
                    if (!bIsMapGet && !bIsMapSet) continue;

                    TArray<UEdGraphPin*> PinsCopy = N->Pins;
                    for (UEdGraphPin* Pin : PinsCopy)
                    {
                        if (!Pin || Pin->PinName != ParamFName) continue;
                        // Break links + destroy the pin.
                        N->Modify();
                        Pin->BreakAllPinLinks();
                        N->RemovePin(Pin);
                    }
                }
            }
        }
        G->NotifyGraphChanged();
    }

    if (System && !ModuleName.IsEmpty())
    {
        NiagaraHelpers::NotifyScratchPadScriptChanged(System, ModuleName);
    }
    Script->MarkPackageDirty();

    auto R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), Removed > 0);
    R->SetNumberField(TEXT("removed_graph_count"), Removed);
    R->SetStringField(TEXT("name"), ParamName);
    return R;
#else
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only command"));
#endif
}


// ---------------------------------------------------------------------------
// HandleRenameNiagaraScriptParameter
// Params: (graph resolver) + direction + old_name + new_name
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleRenameNiagaraScriptParameter(
    const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
    FString Error;
    UNiagaraSystem* System = nullptr;
    FString ModuleName;
    UNiagaraScript* Script = nullptr;
    UNiagaraGraph* PrimaryGraph = nullptr;
    if (!ResolveScriptAndGraph(Params, System, ModuleName, Script, PrimaryGraph, Error))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    FString Direction = TEXT("output");
    Params->TryGetStringField(TEXT("direction"), Direction);
    FString OldName, NewName;
    if (!Params->TryGetStringField(TEXT("old_name"), OldName) ||
        !Params->TryGetStringField(TEXT("new_name"), NewName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            TEXT("Missing 'old_name' or 'new_name'"));
    }

    TArray<UNiagaraGraph*> MutGraphs = CollectMutationGraphs(System, ModuleName, Script);
    int32 Renamed = 0;
    for (UNiagaraGraph* G : MutGraphs)
    {
        G->Modify();
        if (Direction.Equals(TEXT("output"), ESearchCase::IgnoreCase))
        {
            if (UNiagaraNodeOutput* OutNode = FindOutputNode(G))
            {
                OutNode->Modify();
                for (FNiagaraVariable& V : OutNode->Outputs)
                {
                    if (V.GetName() == FName(*OldName))
                    {
                        V.SetName(FName(*NewName));
                        ++Renamed;
                    }
                }
                if (FProperty* OutputsProp = UNiagaraNodeOutput::StaticClass()
                        ->FindPropertyByName(TEXT("Outputs")))
                {
                    FPropertyChangedEvent Evt(OutputsProp, EPropertyChangeType::ValueSet);
                    OutNode->PostEditChangeProperty(Evt);
                }
            }
        }
        else
        {
            UNiagaraGraph::FScriptVariableMap& MetaMap = G->GetAllMetaData();
            FNiagaraVariable OldKey;
            UNiagaraScriptVariable* SV = nullptr;
            for (const TPair<FNiagaraVariable, UNiagaraScriptVariable*>& Pair : MetaMap)
            {
                if (Pair.Key.GetName() == FName(*OldName))
                {
                    OldKey = Pair.Key;
                    SV = Pair.Value;
                    break;
                }
            }
            if (SV && OldKey.GetType().IsValid())
            {
                FNiagaraVariable NewKey(OldKey.GetType(), FName(*NewName));
                MetaMap.Remove(OldKey);
                SV->Variable = NewKey;
                MetaMap.Add(NewKey, SV);
                ++Renamed;
            }
        }
        G->NotifyGraphChanged();
    }

    if (System && !ModuleName.IsEmpty())
    {
        NiagaraHelpers::NotifyScratchPadScriptChanged(System, ModuleName);
    }
    Script->MarkPackageDirty();

    auto R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), Renamed > 0);
    R->SetStringField(TEXT("old_name"), OldName);
    R->SetStringField(TEXT("new_name"), NewName);
    R->SetNumberField(TEXT("renamed_graph_count"), Renamed);
    return R;
#else
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only command"));
#endif
}


// ---------------------------------------------------------------------------
// Phase 5 — Generic node creation
//
// Supported node_type values (case-insensitive, prefix "NiagaraNode" optional):
//   Op                 — requires op_name (e.g. "Numeric::Add", "Vector::Length")
//   FunctionCall       — requires function_script asset path
//   ParameterMapGet
//   ParameterMapSet
//   Reroute
//   Input              — requires input_name, input_type
//   CustomHlsl         — empty by default; use set_niagara_scratch_pad_hlsl after
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Node spawning notes (FGraphNodeCreator contract — EdGraph.h:297-313):
//   - CreateNode(bSelectNewNode=false) allocates the node + adds to graph
//   - Finalize() must be called BEFORE the creator destructs — it calls
//     CreateNewGuid + PostPlacedNewNode + AllocateDefaultPins (only if no
//     pins exist yet) and sets bPlaced = true
//   - The destructor asserts checkf(bPlaced) — missing Finalize = crash
//
// Implication: the creator cannot live inside a helper that returns — the
// caller must hold the creator on its stack across the configuration
// window. Each node type below inlines the create → configure → finalize
// sequence so pin-layout-affecting properties (OpName, FunctionScript,
// Input variable) are set before AllocateDefaultPins runs.
// ---------------------------------------------------------------------------


// ---------------------------------------------------------------------------
// HandleAddNiagaraGraphNode
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleAddNiagaraGraphNode(
    const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
    FString Error;
    UNiagaraSystem* System = nullptr;
    FString ModuleName;
    UNiagaraScript* Script = nullptr;
    UNiagaraGraph* PrimaryGraph = nullptr;
    if (!ResolveScriptAndGraph(Params, System, ModuleName, Script, PrimaryGraph, Error))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    FString NodeType;
    if (!Params->TryGetStringField(TEXT("node_type"), NodeType))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            TEXT("Missing 'node_type'"));
    }
    NodeType.RemoveFromStart(TEXT("NiagaraNode"));

    int32 PosX = 0, PosY = 0;
    Params->TryGetNumberField(TEXT("pos_x"), PosX);
    Params->TryGetNumberField(TEXT("pos_y"), PosY);

    TArray<UNiagaraGraph*> MutGraphs = CollectMutationGraphs(System, ModuleName, Script);
    if (MutGraphs.Num() == 0)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No graph to mutate"));
    }

    // Spawn on every mirrored graph so asset + edit-copy stay in lockstep.
    UEdGraphNode* FirstSpawned = nullptr;
    int32 FirstSpawnedIndex = INDEX_NONE;
    for (UNiagaraGraph* G : MutGraphs)
    {
        G->Modify();
        UEdGraphNode* Spawned = nullptr;

        if (NodeType.Equals(TEXT("Op"), ESearchCase::IgnoreCase))
        {
            FString OpName;
            if (!Params->TryGetStringField(TEXT("op_name"), OpName))
            {
                return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
                    TEXT("node_type=Op requires 'op_name' (e.g. 'Numeric::Add')"));
            }
            FGraphNodeCreator<UNiagaraNodeOp> Creator(*G);
            UNiagaraNodeOp* N = Creator.CreateNode(false);
            N->NodePosX = PosX;
            N->NodePosY = PosY;
            N->OpName = FName(*OpName);
            Creator.Finalize();
            G->NotifyGraphChanged();
            Spawned = N;
        }
        else if (NodeType.Equals(TEXT("FunctionCall"), ESearchCase::IgnoreCase))
        {
            FString FuncPath;
            if (!Params->TryGetStringField(TEXT("function_script"), FuncPath))
            {
                return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
                    TEXT("node_type=FunctionCall requires 'function_script' asset path"));
            }
            UNiagaraScript* FuncScript = Cast<UNiagaraScript>(UEditorAssetLibrary::LoadAsset(FuncPath));
            if (!FuncScript)
            {
                return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
                    FString::Printf(TEXT("function_script not found: %s"), *FuncPath));
            }
            FGraphNodeCreator<UNiagaraNodeFunctionCall> Creator(*G);
            UNiagaraNodeFunctionCall* N = Creator.CreateNode(false);
            N->NodePosX = PosX;
            N->NodePosY = PosY;
            N->FunctionScript = FuncScript;
            Creator.Finalize();
            G->NotifyGraphChanged();
            Spawned = N;
        }
        else if (NodeType.Equals(TEXT("ParameterMapGet"), ESearchCase::IgnoreCase))
        {
            FGraphNodeCreator<UNiagaraNodeParameterMapGet> Creator(*G);
            UNiagaraNodeParameterMapGet* N = Creator.CreateNode(false);
            N->NodePosX = PosX;
            N->NodePosY = PosY;
            Creator.Finalize();
            G->NotifyGraphChanged();
            Spawned = N;
        }
        else if (NodeType.Equals(TEXT("ParameterMapSet"), ESearchCase::IgnoreCase))
        {
            FGraphNodeCreator<UNiagaraNodeParameterMapSet> Creator(*G);
            UNiagaraNodeParameterMapSet* N = Creator.CreateNode(false);
            N->NodePosX = PosX;
            N->NodePosY = PosY;
            Creator.Finalize();
            G->NotifyGraphChanged();
            Spawned = N;
        }
        else if (NodeType.Equals(TEXT("Reroute"), ESearchCase::IgnoreCase))
        {
            FGraphNodeCreator<UNiagaraNodeReroute> Creator(*G);
            UNiagaraNodeReroute* N = Creator.CreateNode(false);
            N->NodePosX = PosX;
            N->NodePosY = PosY;
            Creator.Finalize();
            G->NotifyGraphChanged();
            Spawned = N;
        }
        else if (NodeType.Equals(TEXT("Input"), ESearchCase::IgnoreCase))
        {
            FString InName, InTypeName;
            if (!Params->TryGetStringField(TEXT("input_name"), InName) ||
                !Params->TryGetStringField(TEXT("input_type"), InTypeName))
            {
                return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
                    TEXT("node_type=Input requires 'input_name' and 'input_type'"));
            }
            FNiagaraTypeDefinition TypeDef;
            if (!NiagaraIntrospection::ResolveTypeName(InTypeName, TypeDef) || !TypeDef.IsValid())
            {
                return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
                    FString::Printf(TEXT("Unknown Niagara type '%s'"), *InTypeName));
            }
            FGraphNodeCreator<UNiagaraNodeInput> Creator(*G);
            UNiagaraNodeInput* N = Creator.CreateNode(false);
            N->NodePosX = PosX;
            N->NodePosY = PosY;
            N->Input = FNiagaraVariable(TypeDef, FName(*InName));
            Creator.Finalize();
            G->NotifyGraphChanged();
            Spawned = N;
        }
        else if (NodeType.Equals(TEXT("DataInterfaceFunction"), ESearchCase::IgnoreCase))
        {
            // Gap #15 fix: spawn a UNiagaraNodeFunctionCall whose Signature is
            // populated from a data-interface class's GetFunctionSignatures.
            // FunctionScript stays null because these are built-in DI member
            // functions, not script-backed function calls. Required params:
            //   di_class       — short or full class name of the DI
            //                    (e.g. "NiagaraDataInterfaceArrayPosition")
            //   function_name  — the FName of the member function to wrap
            //                    (e.g. "Length", "Get")
            FString DIClassName, FunctionName;
            if (!Params->TryGetStringField(TEXT("di_class"), DIClassName) ||
                !Params->TryGetStringField(TEXT("function_name"), FunctionName))
            {
                return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
                    TEXT("node_type=DataInterfaceFunction requires 'di_class' and 'function_name'"));
            }

            // Resolve the DI class — accept short or full name.
            UClass* DIClass = FindFirstObjectSafe<UClass>(*DIClassName, EFindFirstObjectOptions::ExactClass);
            if (!DIClass)
            {
                FString FullName = DIClassName.StartsWith(TEXT("/Script/Niagara."))
                    ? DIClassName
                    : (FString(TEXT("/Script/Niagara.")) + DIClassName);
                DIClass = LoadClass<UObject>(nullptr, *FullName);
            }
            if (!DIClass || !DIClass->IsChildOf(UNiagaraDataInterface::StaticClass()))
            {
                return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
                    FString::Printf(TEXT("DI class '%s' not found or not a UNiagaraDataInterface"),
                        *DIClassName));
            }

            UNiagaraDataInterface* DICDO =
                Cast<UNiagaraDataInterface>(DIClass->GetDefaultObject());
            if (!DICDO)
            {
                return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
                    TEXT("DI class has no default object"));
            }

            TArray<FNiagaraFunctionSignature> Signatures;
            DICDO->GetFunctionSignatures(Signatures);
            const FNiagaraFunctionSignature* MatchedSig = nullptr;
            for (const FNiagaraFunctionSignature& S : Signatures)
            {
                if (S.Name.ToString().Equals(FunctionName, ESearchCase::IgnoreCase))
                {
                    MatchedSig = &S;
                    break;
                }
            }
            if (!MatchedSig)
            {
                return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
                    FString::Printf(
                        TEXT("Function '%s' not found on DI '%s'. Use list_niagara_data_interface_functions to discover."),
                        *FunctionName, *DIClassName));
            }

            FGraphNodeCreator<UNiagaraNodeFunctionCall> Creator(*G);
            UNiagaraNodeFunctionCall* N = Creator.CreateNode(false);
            N->NodePosX = PosX;
            N->NodePosY = PosY;
            N->Signature = *MatchedSig;
            // FunctionScript intentionally null — the editor identifies these
            // as DI member functions by Signature presence + null script.
            Creator.Finalize();
            G->NotifyGraphChanged();
            Spawned = N;
        }
        else
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
                FString::Printf(
                    TEXT("Unsupported node_type '%s'. Supported: Op, FunctionCall, DataInterfaceFunction, ParameterMapGet, ParameterMapSet, Reroute, Input"),
                    *NodeType));
        }

        if (!FirstSpawned)
        {
            FirstSpawned = Spawned;
            FirstSpawnedIndex = G->Nodes.IndexOfByKey(Spawned);
        }
    }

    if (System && !ModuleName.IsEmpty())
    {
        NiagaraHelpers::NotifyScratchPadScriptChanged(System, ModuleName);
    }
    Script->MarkPackageDirty();

    auto R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), FirstSpawned != nullptr);
    R->SetStringField(TEXT("node_type"), NodeType);
    R->SetNumberField(TEXT("node_index"), FirstSpawnedIndex);
    if (FirstSpawned)
    {
        R->SetStringField(TEXT("node_id"),
            FirstSpawned->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));
    }
    return R;
#else
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only command"));
#endif
}


// ---------------------------------------------------------------------------
// HandleDeleteNiagaraGraphNode
// Params: (graph resolver) + node_index | node_id
// Deletes the matching node from asset + edit-copy graphs.
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleDeleteNiagaraGraphNode(
    const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
    FString Error;
    UNiagaraSystem* System = nullptr;
    FString ModuleName;
    UNiagaraScript* Script = nullptr;
    UNiagaraGraph* PrimaryGraph = nullptr;
    if (!ResolveScriptAndGraph(Params, System, ModuleName, Script, PrimaryGraph, Error))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    int32 NodeIndex = INDEX_NONE;
    FString NodeIdStr;
    FGuid TargetGuid;
    const bool bHaveIndex = Params->TryGetNumberField(TEXT("node_index"), NodeIndex);
    const bool bHaveGuid  = Params->TryGetStringField(TEXT("node_id"), NodeIdStr) &&
                            FGuid::Parse(NodeIdStr, TargetGuid);
    if (!bHaveIndex && !bHaveGuid)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            TEXT("Provide 'node_index' or 'node_id'"));
    }

    TArray<UNiagaraGraph*> MutGraphs = CollectMutationGraphs(System, ModuleName, Script);
    int32 DeletedGraphs = 0;
    FString DeletedClass;

    // On the primary graph, resolve by index first to look up the node GUID —
    // index is only stable per-graph, so we fall back to GUID for mirrors.
    if (bHaveIndex && PrimaryGraph && PrimaryGraph->Nodes.IsValidIndex(NodeIndex))
    {
        UEdGraphNode* N = PrimaryGraph->Nodes[NodeIndex];
        if (N)
        {
            TargetGuid = N->NodeGuid;
            DeletedClass = N->GetClass()->GetName();
        }
    }

    if (!TargetGuid.IsValid())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Could not resolve target node"));
    }

    for (UNiagaraGraph* G : MutGraphs)
    {
        for (int32 i = G->Nodes.Num() - 1; i >= 0; --i)
        {
            UEdGraphNode* N = G->Nodes[i];
            if (N && N->NodeGuid == TargetGuid)
            {
                G->Modify();
                if (DeletedClass.IsEmpty()) DeletedClass = N->GetClass()->GetName();
                N->BreakAllNodeLinks();
                G->RemoveNode(N);
                ++DeletedGraphs;
                break;
            }
        }
        G->NotifyGraphChanged();
    }

    if (System && !ModuleName.IsEmpty())
    {
        NiagaraHelpers::NotifyScratchPadScriptChanged(System, ModuleName);
    }
    Script->MarkPackageDirty();

    auto R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), DeletedGraphs > 0);
    R->SetNumberField(TEXT("deleted_graph_count"), DeletedGraphs);
    R->SetStringField(TEXT("node_class"), DeletedClass);
    return R;
#else
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only command"));
#endif
}


// ---------------------------------------------------------------------------
// HandleListNiagaraDataInterfaceFunctions — Gap #15 discovery
// Enumerates member functions on a Niagara data interface class so callers
// can pick the exact function_name to pass to add_niagara_graph_node with
// node_type="DataInterfaceFunction". Authoritative source: each DI's
// GetFunctionSignatures (NIAGARA_API exported, NiagaraDataInterface.h:681).
//
// Params:
//   di_class       — required, short or full class name
//   filter         — optional, case-insensitive substring on function name
//   include_pins   — true (default), include input/output pin schemas
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleListNiagaraDataInterfaceFunctions(
    const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
    FString DIClassName;
    if (!Params->TryGetStringField(TEXT("di_class"), DIClassName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'di_class'"));
    }
    FString Filter;
    Params->TryGetStringField(TEXT("filter"), Filter);
    bool bIncludePins = true;
    Params->TryGetBoolField(TEXT("include_pins"), bIncludePins);

    UClass* DIClass = FindFirstObjectSafe<UClass>(*DIClassName, EFindFirstObjectOptions::ExactClass);
    if (!DIClass)
    {
        FString FullName = DIClassName.StartsWith(TEXT("/Script/Niagara."))
            ? DIClassName
            : (FString(TEXT("/Script/Niagara.")) + DIClassName);
        DIClass = LoadClass<UObject>(nullptr, *FullName);
    }
    if (!DIClass || !DIClass->IsChildOf(UNiagaraDataInterface::StaticClass()))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("DI class '%s' not found"), *DIClassName));
    }

    UNiagaraDataInterface* CDO = Cast<UNiagaraDataInterface>(DIClass->GetDefaultObject());
    if (!CDO)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("DI has no CDO"));
    }

    TArray<FNiagaraFunctionSignature> Sigs;
    CDO->GetFunctionSignatures(Sigs);

    auto SerializeVar = [](const FNiagaraVariableBase& V)
    {
        auto J = MakeShared<FJsonObject>();
        J->SetStringField(TEXT("name"), V.GetName().ToString());
        J->SetStringField(TEXT("type"), V.GetType().GetName());
        return MakeShared<FJsonValueObject>(J);
    };

    TArray<TSharedPtr<FJsonValue>> Out;
    for (const FNiagaraFunctionSignature& S : Sigs)
    {
        const FString FnName = S.Name.ToString();
        if (!Filter.IsEmpty() && !FnName.Contains(Filter, ESearchCase::IgnoreCase))
        {
            continue;
        }
        auto Entry = MakeShared<FJsonObject>();
        Entry->SetStringField(TEXT("name"), FnName);
        Entry->SetBoolField(TEXT("supports_cpu"), S.bSupportsCPU);
        Entry->SetBoolField(TEXT("supports_gpu"), S.bSupportsGPU);
        Entry->SetBoolField(TEXT("read_function"), S.bReadFunction);
        Entry->SetBoolField(TEXT("write_function"), S.bWriteFunction);
        Entry->SetBoolField(TEXT("member_function"), S.bMemberFunction);
        if (!S.Description.IsEmpty())
        {
            Entry->SetStringField(TEXT("description"), S.Description.ToString());
        }
        if (bIncludePins)
        {
            TArray<TSharedPtr<FJsonValue>> Inputs, Outputs;
            for (const FNiagaraVariable& V : S.Inputs) Inputs.Add(SerializeVar(V));
            for (const FNiagaraVariableBase& V : S.Outputs) Outputs.Add(SerializeVar(V));
            Entry->SetArrayField(TEXT("inputs"), Inputs);
            Entry->SetArrayField(TEXT("outputs"), Outputs);
        }
        Out.Add(MakeShared<FJsonValueObject>(Entry));
    }

    auto R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetStringField(TEXT("di_class"), DIClass->GetName());
    R->SetStringField(TEXT("di_class_path"), DIClass->GetPathName());
    R->SetNumberField(TEXT("total_functions"), Sigs.Num());
    R->SetNumberField(TEXT("returned"), Out.Num());
    R->SetArrayField(TEXT("functions"), Out);
    return R;
#else
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only command"));
#endif
}
