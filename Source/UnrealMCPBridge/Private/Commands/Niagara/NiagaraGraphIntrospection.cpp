#include "Commands/EpicUnrealMCPNiagaraCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "NiagaraHelpers.h"
#include "NiagaraPropertyIntrospection.h"

#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraGraph.h"
#include "NiagaraNode.h"
#include "NiagaraTypes.h"

#if WITH_EDITORONLY_DATA
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraNodeInput.h"
#include "NiagaraNodeParameterMapGet.h"
#include "NiagaraNodeParameterMapSet.h"
#include "NiagaraNodeCustomHlsl.h"
#include "NiagaraNodeOp.h"
#include "EdGraphSchema_Niagara.h"
#include "EditorAssetLibrary.h"
#endif

#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphNode.h"


// ---------------------------------------------------------------------------
// Graph introspection — mirrors Material graph read tools for Niagara scratch
// pad / dynamic input / module script graphs. Verbosity-controlled listing,
// per-node deep inspect, connection tracing, and orphan validation.
//
// Accepts either:
//   - system_path + module_name : resolves scratch pad script on the system
//                                 (returns the asset graph; reads-only are
//                                 safe to run against asset or edit-copy —
//                                 we prefer the edit-copy when open)
//   - script_path               : a standalone UNiagaraScript asset (module
//                                 / dynamic_input / function) saved to disk
// ---------------------------------------------------------------------------

#if WITH_EDITORONLY_DATA
namespace
{

/**
 * Resolve a UNiagaraGraph from incoming JSON params.
 *
 * Precedence:
 *   1. system_path + module_name — scratch pad inside a system. Returns the
 *      live edit-copy graph when the editor is open, else the asset graph.
 *   2. script_path               — a UNiagaraScript asset on disk.
 */
UNiagaraGraph* ResolveReadableGraph(
    const TSharedPtr<FJsonObject>& Params,
    UNiagaraScript*& OutScript,
    FString& OutSourceDesc,
    FString& OutError)
{
    OutScript = nullptr;
    OutSourceDesc.Reset();

    FString SystemPath, ModuleName;
    Params->TryGetStringField(TEXT("system_path"), SystemPath);
    Params->TryGetStringField(TEXT("module_name"), ModuleName);

    if (!SystemPath.IsEmpty() && !ModuleName.IsEmpty())
    {
        UNiagaraSystem* System = NiagaraHelpers::LoadNiagaraSystem(SystemPath, OutError);
        if (!System)
        {
            return nullptr;
        }

        TArray<UNiagaraScript*> Scripts;
        NiagaraHelpers::GetScratchPadScriptPair(System, ModuleName, Scripts);

        // Prefer edit-copy (last element when pair is returned) so reads reflect
        // what the user sees in the open editor.
        for (int32 i = Scripts.Num() - 1; i >= 0; --i)
        {
            if (UNiagaraScript* S = Scripts[i])
            {
                if (UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(S->GetLatestSource()))
                {
                    if (Source->NodeGraph)
                    {
                        OutScript = S;
                        OutSourceDesc = FString::Printf(TEXT("%s::%s"), *SystemPath, *ModuleName);
                        return Source->NodeGraph;
                    }
                }
            }
        }

        OutError = FString::Printf(
            TEXT("Scratch pad module '%s' not found on system '%s' or has no graph"),
            *ModuleName, *SystemPath);
        return nullptr;
    }

    // Alternative: emitter stack graph resolver (Gap #12 fix).
    // Lets callers read the actual EmitterUpdateScript / ParticleSpawnScript
    // / etc. graphs that contain the spliced module function-call nodes.
    FString EmitterName, ScriptUsageStr;
    if (!SystemPath.IsEmpty() &&
        Params->TryGetStringField(TEXT("emitter_name"), EmitterName) &&
        Params->TryGetStringField(TEXT("script_usage"), ScriptUsageStr))
    {
        UNiagaraSystem* System = NiagaraHelpers::LoadNiagaraSystem(SystemPath, OutError);
        if (!System) return nullptr;

        bool bParsed = false;
        ENiagaraScriptUsage Usage = NiagaraHelpers::ParseScriptUsage(ScriptUsageStr, bParsed);
        if (!bParsed)
        {
            OutError = FString::Printf(TEXT("Unknown script_usage '%s'"), *ScriptUsageStr);
            return nullptr;
        }

        int32 Index = INDEX_NONE;
        FNiagaraEmitterHandle* Handle =
            NiagaraHelpers::FindEmitterHandle(System, EmitterName, Index, OutError);
        if (!Handle) return nullptr;

        FVersionedNiagaraEmitterData* Data = NiagaraHelpers::GetEmitterData(Handle);
        if (!Data)
        {
            OutError = TEXT("Failed to get emitter data");
            return nullptr;
        }

        UNiagaraScript* S = Data->GetScript(Usage, FGuid());
        if (!S)
        {
            OutError = FString::Printf(
                TEXT("Emitter '%s' has no script for usage '%s'"),
                *EmitterName, *ScriptUsageStr);
            return nullptr;
        }
        if (UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(S->GetLatestSource()))
        {
            if (Source->NodeGraph)
            {
                OutScript = S;
                OutSourceDesc = FString::Printf(
                    TEXT("%s::%s::%s"), *SystemPath, *EmitterName, *ScriptUsageStr);
                return Source->NodeGraph;
            }
        }
        OutError = TEXT("Emitter script has no graph");
        return nullptr;
    }

    FString ScriptPath;
    if (Params->TryGetStringField(TEXT("script_path"), ScriptPath))
    {
        UNiagaraScript* Script = Cast<UNiagaraScript>(UEditorAssetLibrary::LoadAsset(ScriptPath));
        if (!Script)
        {
            OutError = FString::Printf(TEXT("Script asset not found: %s"), *ScriptPath);
            return nullptr;
        }
        UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(Script->GetLatestSource());
        if (!Source || !Source->NodeGraph)
        {
            OutError = FString::Printf(TEXT("Script has no graph source: %s"), *ScriptPath);
            return nullptr;
        }
        OutScript = Script;
        OutSourceDesc = ScriptPath;
        return Source->NodeGraph;
    }

    OutError = TEXT("Provide (system_path + module_name), (system_path + emitter_name + script_usage), or script_path");
    return nullptr;
}

/** Short Niagara node class name: "NiagaraNodeMapGet" -> "MapGet". */
FString ShortNodeClassName(const UEdGraphNode* Node)
{
    if (!Node) return TEXT("");
    FString Name = Node->GetClass()->GetName();
    Name.RemoveFromStart(TEXT("NiagaraNode"));
    return Name;
}

/** Friendly display title for a node (matches editor graph title). */
FString NodeDisplayTitle(UEdGraphNode* Node)
{
    if (!Node) return TEXT("");
    return Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
}

/** Niagara-aware pin type description. Falls back to raw PinCategory. */
FString PinTypeDescription(const UEdGraphPin* Pin)
{
    if (!Pin) return TEXT("");

    // Try to resolve a FNiagaraTypeDefinition from the pin (most accurate).
    const FNiagaraTypeDefinition TypeDef =
        UEdGraphSchema_Niagara::PinTypeToTypeDefinition(Pin->PinType);
    if (TypeDef.IsValid())
    {
        return TypeDef.GetName();
    }

    // Fallback: raw category (e.g. "NiagaraParameterMap", "NiagaraEnumBase").
    if (!Pin->PinType.PinCategory.IsNone())
    {
        return Pin->PinType.PinCategory.ToString();
    }
    return TEXT("");
}

/** True if this pin is the "Add Pin" placeholder on a dynamic-pins node. */
bool IsAddPinPlaceholder(const UEdGraphPin* Pin)
{
    if (!Pin) return true;
    if (Pin->bOrphanedPin) return true;
    // Niagara uses a MiscCategory + AddPin name — but the bOrphanedPin + empty
    // PinName combination is the safer cross-version check.
    if (Pin->PinName == NAME_None) return true;
    return false;
}

/** Build a connection entry: {to_node, to_pin, to_pin_direction}. */
TSharedPtr<FJsonObject> SerializeLink(
    const UEdGraphPin* OtherPin,
    const TMap<UEdGraphNode*, int32>& NodeIndexMap)
{
    auto Obj = MakeShared<FJsonObject>();
    if (!OtherPin || !OtherPin->GetOwningNodeUnchecked())
    {
        Obj->SetNumberField(TEXT("to_node"), -1);
        return Obj;
    }
    UEdGraphNode* OwningNode = OtherPin->GetOwningNode();
    const int32* IdxPtr = NodeIndexMap.Find(OwningNode);
    Obj->SetNumberField(TEXT("to_node"), IdxPtr ? *IdxPtr : -1);
    Obj->SetStringField(TEXT("to_node_title"), NodeDisplayTitle(OwningNode));
    Obj->SetStringField(TEXT("to_node_class"), ShortNodeClassName(OwningNode));
    Obj->SetStringField(TEXT("to_pin"), OtherPin->PinName.ToString());
    Obj->SetStringField(TEXT("to_pin_direction"),
        OtherPin->Direction == EGPD_Input ? TEXT("input") : TEXT("output"));
    return Obj;
}

/** Serialize a single pin with optional links. */
TSharedPtr<FJsonObject> SerializePin(
    UEdGraphPin* Pin,
    const TMap<UEdGraphNode*, int32>& NodeIndexMap,
    bool bIncludeLinks,
    bool bIncludeDefault)
{
    auto Obj = MakeShared<FJsonObject>();
    Obj->SetStringField(TEXT("name"), Pin->PinName.ToString());
    Obj->SetStringField(TEXT("type"), PinTypeDescription(Pin));
    Obj->SetStringField(TEXT("direction"),
        Pin->Direction == EGPD_Input ? TEXT("input") : TEXT("output"));
    Obj->SetBoolField(TEXT("connected"), Pin->LinkedTo.Num() > 0);
    Obj->SetNumberField(TEXT("link_count"), Pin->LinkedTo.Num());
    if (Pin->bHidden) Obj->SetBoolField(TEXT("hidden"), true);
    if (Pin->bOrphanedPin) Obj->SetBoolField(TEXT("orphaned"), true);

    if (bIncludeDefault && Pin->LinkedTo.Num() == 0 && !Pin->DefaultValue.IsEmpty())
    {
        Obj->SetStringField(TEXT("default_value"), Pin->DefaultValue);
    }

    if (bIncludeLinks && Pin->LinkedTo.Num() > 0)
    {
        TArray<TSharedPtr<FJsonValue>> Links;
        for (UEdGraphPin* Other : Pin->LinkedTo)
        {
            Links.Add(MakeShared<FJsonValueObject>(SerializeLink(Other, NodeIndexMap)));
        }
        Obj->SetArrayField(TEXT("links"), Links);
    }
    return Obj;
}

/** Verbosity-aware node serializer. */
TSharedPtr<FJsonObject> SerializeNode(
    UEdGraphNode* Node,
    int32 NodeIndex,
    const TMap<UEdGraphNode*, int32>& NodeIndexMap,
    const FString& Verbosity)
{
    auto Obj = MakeShared<FJsonObject>();
    Obj->SetNumberField(TEXT("index"), NodeIndex);
    Obj->SetStringField(TEXT("class"), Node->GetClass()->GetName());
    Obj->SetStringField(TEXT("type"), ShortNodeClassName(Node));
    Obj->SetStringField(TEXT("title"), NodeDisplayTitle(Node));
    Obj->SetStringField(TEXT("guid"), Node->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));

    {
        auto Pos = MakeShared<FJsonObject>();
        Pos->SetNumberField(TEXT("x"), Node->NodePosX);
        Pos->SetNumberField(TEXT("y"), Node->NodePosY);
        Obj->SetObjectField(TEXT("position"), Pos);
    }

    const bool bFull = Verbosity.Equals(TEXT("full"), ESearchCase::IgnoreCase);
    const bool bConnections =
        bFull || Verbosity.Equals(TEXT("connections"), ESearchCase::IgnoreCase);

    // Summary verbosity: just name + pin counts.
    int32 NumInputs = 0;
    int32 NumOutputs = 0;
    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (IsAddPinPlaceholder(Pin)) continue;
        if (Pin->Direction == EGPD_Input) ++NumInputs;
        else ++NumOutputs;
    }
    Obj->SetNumberField(TEXT("num_inputs"), NumInputs);
    Obj->SetNumberField(TEXT("num_outputs"), NumOutputs);

    if (Verbosity.Equals(TEXT("summary"), ESearchCase::IgnoreCase))
    {
        return Obj;
    }

    // Connections verbosity (and above): inputs + outputs with link detail.
    TArray<TSharedPtr<FJsonValue>> InputPins, OutputPins;
    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (IsAddPinPlaceholder(Pin)) continue;
        TSharedPtr<FJsonObject> PinObj =
            SerializePin(Pin, NodeIndexMap, /*bIncludeLinks=*/true, /*bIncludeDefault=*/bFull);
        if (Pin->Direction == EGPD_Input)
        {
            InputPins.Add(MakeShared<FJsonValueObject>(PinObj));
        }
        else
        {
            OutputPins.Add(MakeShared<FJsonValueObject>(PinObj));
        }
    }
    Obj->SetArrayField(TEXT("inputs"), InputPins);
    Obj->SetArrayField(TEXT("outputs"), OutputPins);

    // Full verbosity: node-type-specific extra fields.
    if (bFull)
    {
        // Function call modules: expose function name / referenced script path.
        if (UNiagaraNodeFunctionCall* FnCall = Cast<UNiagaraNodeFunctionCall>(Node))
        {
            Obj->SetStringField(TEXT("function_display_name"), FnCall->GetFunctionName());
            if (UNiagaraScript* Referenced = FnCall->FunctionScript)
            {
                Obj->SetStringField(TEXT("function_script"), Referenced->GetPathName());
            }
        }
        // Output node: expose usage enum.
        if (UNiagaraNodeOutput* OutputNode = Cast<UNiagaraNodeOutput>(Node))
        {
            Obj->SetStringField(TEXT("script_usage"),
                NiagaraHelpers::ScriptUsageToString(OutputNode->GetUsage()));
        }
        // Input node: expose input variable name / type.
        if (UNiagaraNodeInput* InputNode = Cast<UNiagaraNodeInput>(Node))
        {
            const FNiagaraVariable& V = InputNode->Input;
            Obj->SetStringField(TEXT("input_name"), V.GetName().ToString());
            Obj->SetStringField(TEXT("input_type"), V.GetType().GetName());
        }
        // Custom HLSL: expose the HLSL body (first 500 chars) + length.
        // CustomHlsl field is private + accessor isn't API-exported, so read
        // via FProperty reflection (same pattern as NiagaraTypeHelpers uses
        // for writing).
        if (UNiagaraNodeCustomHlsl* Hlsl = Cast<UNiagaraNodeCustomHlsl>(Node))
        {
            if (FProperty* HlslProp = UNiagaraNodeCustomHlsl::StaticClass()
                    ->FindPropertyByName(TEXT("CustomHlsl")))
            {
                if (FStrProperty* StrProp = CastField<FStrProperty>(HlslProp))
                {
                    const FString Body = StrProp->GetPropertyValue_InContainer(Hlsl);
                    Obj->SetNumberField(TEXT("hlsl_length"), Body.Len());
                    Obj->SetStringField(TEXT("hlsl_preview"),
                        Body.Len() <= 500 ? Body : (Body.Left(497) + TEXT("...")));
                }
            }
        }
        // Op node: expose opname.
        if (UNiagaraNodeOp* OpNode = Cast<UNiagaraNodeOp>(Node))
        {
            Obj->SetStringField(TEXT("op_name"), OpNode->OpName.ToString());
        }
    }

    return Obj;
}

/** Build a node→index map for a graph's Nodes array (nulls dropped). */
void BuildNodeIndexMap(UNiagaraGraph* Graph, TMap<UEdGraphNode*, int32>& OutMap)
{
    OutMap.Reset();
    if (!Graph) return;
    OutMap.Reserve(Graph->Nodes.Num());
    for (int32 i = 0; i < Graph->Nodes.Num(); ++i)
    {
        if (UEdGraphNode* N = Graph->Nodes[i])
        {
            OutMap.Add(N, i);
        }
    }
}

} // namespace
#endif // WITH_EDITORONLY_DATA


// ---------------------------------------------------------------------------
// HandleGetNiagaraGraphNodes — list every node in a scratch pad / script graph
// Params:
//   system_path + module_name   OR   script_path
//   verbosity       : "summary" | "connections" (default) | "full"
//   type_filter     : case-insensitive substring (matches short class name)
//   name_filter     : case-insensitive substring (matches node title)
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleGetNiagaraGraphNodes(
    const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
    FString Error;
    UNiagaraScript* Script = nullptr;
    FString SourceDesc;
    UNiagaraGraph* Graph = ResolveReadableGraph(Params, Script, SourceDesc, Error);
    if (!Graph)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    FString Verbosity = TEXT("connections");
    Params->TryGetStringField(TEXT("verbosity"), Verbosity);

    FString TypeFilter;
    Params->TryGetStringField(TEXT("type_filter"), TypeFilter);

    FString NameFilter;
    Params->TryGetStringField(TEXT("name_filter"), NameFilter);

    TMap<UEdGraphNode*, int32> NodeIndexMap;
    BuildNodeIndexMap(Graph, NodeIndexMap);

    TArray<TSharedPtr<FJsonValue>> NodesArr;
    int32 SkippedByFilter = 0;
    for (int32 i = 0; i < Graph->Nodes.Num(); ++i)
    {
        UEdGraphNode* Node = Graph->Nodes[i];
        if (!Node) continue;

        if (!TypeFilter.IsEmpty())
        {
            const FString Short = ShortNodeClassName(Node);
            if (!Short.Contains(TypeFilter, ESearchCase::IgnoreCase))
            {
                ++SkippedByFilter;
                continue;
            }
        }
        if (!NameFilter.IsEmpty())
        {
            const FString Title = NodeDisplayTitle(Node);
            if (!Title.Contains(NameFilter, ESearchCase::IgnoreCase))
            {
                ++SkippedByFilter;
                continue;
            }
        }

        NodesArr.Add(MakeShared<FJsonValueObject>(
            SerializeNode(Node, i, NodeIndexMap, Verbosity)));
    }

    auto R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetStringField(TEXT("source"), SourceDesc);
    R->SetNumberField(TEXT("total_nodes"), Graph->Nodes.Num());
    R->SetNumberField(TEXT("returned"), NodesArr.Num());
    R->SetNumberField(TEXT("filtered_out"), SkippedByFilter);
    R->SetStringField(TEXT("verbosity"), Verbosity);
    R->SetArrayField(TEXT("nodes"), NodesArr);
    return R;
#else
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only command"));
#endif
}


// ---------------------------------------------------------------------------
// HandleGetNiagaraNodeInfo — deep inspect a single node
// Params:
//   (graph resolver) +
//   node_index | node_class | node_id
// Returns: full node serialization incl. inputs, outputs, both link sides.
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleGetNiagaraNodeInfo(
    const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
    FString Error;
    UNiagaraScript* Script = nullptr;
    FString SourceDesc;
    UNiagaraGraph* Graph = ResolveReadableGraph(Params, Script, SourceDesc, Error);
    if (!Graph)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    TMap<UEdGraphNode*, int32> NodeIndexMap;
    BuildNodeIndexMap(Graph, NodeIndexMap);

    // Resolve by one of: node_index, node_class (first match), node_id (GUID).
    UEdGraphNode* Target = nullptr;
    int32 TargetIndex = INDEX_NONE;

    int32 NodeIndex = INDEX_NONE;
    if (Params->TryGetNumberField(TEXT("node_index"), NodeIndex))
    {
        if (!Graph->Nodes.IsValidIndex(NodeIndex) || !Graph->Nodes[NodeIndex])
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("node_index %d out of range (have %d nodes)"),
                    NodeIndex, Graph->Nodes.Num()));
        }
        Target = Graph->Nodes[NodeIndex];
        TargetIndex = NodeIndex;
    }
    else
    {
        FString NodeClassStr;
        FString NodeIdStr;
        const bool bHaveClass = Params->TryGetStringField(TEXT("node_class"), NodeClassStr);
        const bool bHaveGuid = Params->TryGetStringField(TEXT("node_id"), NodeIdStr);

        FGuid TargetGuid;
        const bool bGuidValid = bHaveGuid && FGuid::Parse(NodeIdStr, TargetGuid);

        for (int32 i = 0; i < Graph->Nodes.Num(); ++i)
        {
            UEdGraphNode* N = Graph->Nodes[i];
            if (!N) continue;

            if (bGuidValid && N->NodeGuid == TargetGuid)
            {
                Target = N;
                TargetIndex = i;
                break;
            }
            if (bHaveClass)
            {
                const FString Short = ShortNodeClassName(N);
                const FString Full = N->GetClass()->GetName();
                if (Short.Equals(NodeClassStr, ESearchCase::IgnoreCase) ||
                    Full.Equals(NodeClassStr, ESearchCase::IgnoreCase))
                {
                    Target = N;
                    TargetIndex = i;
                    break;
                }
            }
        }

        if (!Target)
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
                TEXT("Node not found — provide node_index, node_class (short or full), or node_id"));
        }
    }

    TSharedPtr<FJsonObject> NodeJson =
        SerializeNode(Target, TargetIndex, NodeIndexMap, TEXT("full"));

    auto R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetStringField(TEXT("source"), SourceDesc);
    R->SetObjectField(TEXT("node"), NodeJson);
    return R;
#else
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only command"));
#endif
}


// ---------------------------------------------------------------------------
// HandleTraceNiagaraConnection — BFS upstream or downstream from a node
// Params:
//   (graph resolver) + node_index|class|id
//   direction    : "upstream" (feeds this node) | "downstream" (this feeds)
//                  | "both" (default)
//   max_depth    : default 8
//   pin_name     : optional — restrict the starting pin by name
// Returns: ordered list of visited nodes with depth, verbosity=summary.
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleTraceNiagaraConnection(
    const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
    FString Error;
    UNiagaraScript* Script = nullptr;
    FString SourceDesc;
    UNiagaraGraph* Graph = ResolveReadableGraph(Params, Script, SourceDesc, Error);
    if (!Graph)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    TMap<UEdGraphNode*, int32> NodeIndexMap;
    BuildNodeIndexMap(Graph, NodeIndexMap);

    // Resolve starting node (reuse lookup by index first, else by class/id).
    UEdGraphNode* Start = nullptr;
    int32 StartIndex = INDEX_NONE;
    int32 NodeIndex = INDEX_NONE;
    if (Params->TryGetNumberField(TEXT("node_index"), NodeIndex) &&
        Graph->Nodes.IsValidIndex(NodeIndex))
    {
        Start = Graph->Nodes[NodeIndex];
        StartIndex = NodeIndex;
    }
    if (!Start)
    {
        FString ClassStr, IdStr;
        const bool bC = Params->TryGetStringField(TEXT("node_class"), ClassStr);
        const bool bI = Params->TryGetStringField(TEXT("node_id"), IdStr);
        FGuid TargetGuid;
        const bool bGuidValid = bI && FGuid::Parse(IdStr, TargetGuid);
        for (int32 i = 0; i < Graph->Nodes.Num(); ++i)
        {
            UEdGraphNode* N = Graph->Nodes[i];
            if (!N) continue;
            if (bGuidValid && N->NodeGuid == TargetGuid) { Start = N; StartIndex = i; break; }
            if (bC && (ShortNodeClassName(N).Equals(ClassStr, ESearchCase::IgnoreCase) ||
                       N->GetClass()->GetName().Equals(ClassStr, ESearchCase::IgnoreCase)))
            {
                Start = N; StartIndex = i; break;
            }
        }
    }
    if (!Start)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            TEXT("Starting node not found — provide node_index, node_class, or node_id"));
    }

    FString Direction = TEXT("both");
    Params->TryGetStringField(TEXT("direction"), Direction);
    const bool bUp = Direction.Equals(TEXT("upstream"), ESearchCase::IgnoreCase) ||
                     Direction.Equals(TEXT("both"), ESearchCase::IgnoreCase);
    const bool bDown = Direction.Equals(TEXT("downstream"), ESearchCase::IgnoreCase) ||
                       Direction.Equals(TEXT("both"), ESearchCase::IgnoreCase);

    int32 MaxDepth = 8;
    Params->TryGetNumberField(TEXT("max_depth"), MaxDepth);

    FString PinFilter;
    Params->TryGetStringField(TEXT("pin_name"), PinFilter);

    // BFS: (node, depth).
    struct FVisit { UEdGraphNode* Node; int32 Depth; EEdGraphPinDirection Flow; };
    TArray<FVisit> Frontier;
    TSet<UEdGraphNode*> Visited;
    Visited.Add(Start);

    auto EnqueueLinked = [&](UEdGraphNode* Node, EEdGraphPinDirection From, int32 Depth)
    {
        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (IsAddPinPlaceholder(Pin)) continue;
            if (Pin->Direction != From) continue;
            if (!PinFilter.IsEmpty() && Node == Start &&
                !Pin->PinName.ToString().Contains(PinFilter, ESearchCase::IgnoreCase))
            {
                continue;
            }
            for (UEdGraphPin* Other : Pin->LinkedTo)
            {
                if (!Other || !Other->GetOwningNodeUnchecked()) continue;
                UEdGraphNode* Next = Other->GetOwningNode();
                if (Visited.Contains(Next)) continue;
                Visited.Add(Next);
                Frontier.Add({ Next, Depth + 1, From });
            }
        }
    };

    if (bUp)   EnqueueLinked(Start, EGPD_Input,  0);
    if (bDown) EnqueueLinked(Start, EGPD_Output, 0);

    TArray<TSharedPtr<FJsonValue>> Visits;
    for (int32 i = 0; i < Frontier.Num(); ++i)
    {
        const FVisit V = Frontier[i];
        auto Entry = MakeShared<FJsonObject>();
        const int32* IdxPtr = NodeIndexMap.Find(V.Node);
        Entry->SetNumberField(TEXT("index"), IdxPtr ? *IdxPtr : -1);
        Entry->SetNumberField(TEXT("depth"), V.Depth);
        Entry->SetStringField(TEXT("direction"),
            V.Flow == EGPD_Input ? TEXT("upstream") : TEXT("downstream"));
        Entry->SetStringField(TEXT("type"), ShortNodeClassName(V.Node));
        Entry->SetStringField(TEXT("title"), NodeDisplayTitle(V.Node));
        Visits.Add(MakeShared<FJsonValueObject>(Entry));

        if (V.Depth < MaxDepth)
        {
            EnqueueLinked(V.Node, V.Flow, V.Depth);
        }
    }

    auto R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetStringField(TEXT("source"), SourceDesc);
    R->SetNumberField(TEXT("start_index"), StartIndex);
    R->SetStringField(TEXT("start_title"), NodeDisplayTitle(Start));
    R->SetStringField(TEXT("direction"), Direction);
    R->SetNumberField(TEXT("max_depth"), MaxDepth);
    R->SetNumberField(TEXT("visited"), Visits.Num());
    R->SetArrayField(TEXT("nodes"), Visits);
    return R;
#else
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only command"));
#endif
}


// ---------------------------------------------------------------------------
// HandleValidateNiagaraGraph — find orphaned / dead-end / unreachable nodes
// Params: (graph resolver)
// Returns a classification of problem nodes.
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleValidateNiagaraGraph(
    const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
    FString Error;
    UNiagaraScript* Script = nullptr;
    FString SourceDesc;
    UNiagaraGraph* Graph = ResolveReadableGraph(Params, Script, SourceDesc, Error);
    if (!Graph)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    TArray<TSharedPtr<FJsonValue>> Orphans;     // No in + no out
    TArray<TSharedPtr<FJsonValue>> DeadEnds;    // Has inputs connected, no outputs linked
    TArray<TSharedPtr<FJsonValue>> Disconnected; // Missing required input pins

    for (int32 i = 0; i < Graph->Nodes.Num(); ++i)
    {
        UEdGraphNode* N = Graph->Nodes[i];
        if (!N) continue;

        // Skip anchor nodes that always terminate — they're supposed to have
        // no downstream consumers in the same graph.
        const bool bIsAnchor =
            N->IsA(UNiagaraNodeOutput::StaticClass()) ||
            N->IsA(UNiagaraNodeInput::StaticClass());

        int32 ConnectedIn = 0, ConnectedOut = 0, MissingRequiredIn = 0;
        for (UEdGraphPin* Pin : N->Pins)
        {
            if (IsAddPinPlaceholder(Pin)) continue;
            // Skip the dynamic "+Add" placeholder pin on nodes supporting
            // added inputs (UNiagaraNodeWithDynamicPins). Its name is
            // literally "Add" and its PinCategory is "Misc" — it's a UI
            // affordance, not a functional input, and counting it as a
            // missing input is a false positive.
            if (Pin->PinName == TEXT("Add") &&
                Pin->PinType.PinCategory == TEXT("Misc"))
            {
                continue;
            }
            const bool bHas = Pin->LinkedTo.Num() > 0;
            if (Pin->Direction == EGPD_Input)
            {
                if (bHas) ++ConnectedIn;
                else if (Pin->DefaultValue.IsEmpty() && Pin->AutogeneratedDefaultValue.IsEmpty())
                {
                    ++MissingRequiredIn;
                }
            }
            else if (bHas) ++ConnectedOut;
        }

        auto Entry = [&]()
        {
            auto E = MakeShared<FJsonObject>();
            E->SetNumberField(TEXT("index"), i);
            E->SetStringField(TEXT("type"), ShortNodeClassName(N));
            E->SetStringField(TEXT("title"), NodeDisplayTitle(N));
            return E;
        };

        if (!bIsAnchor && ConnectedIn == 0 && ConnectedOut == 0)
        {
            Orphans.Add(MakeShared<FJsonValueObject>(Entry()));
            continue;
        }
        if (!bIsAnchor && ConnectedIn > 0 && ConnectedOut == 0)
        {
            // Output node acts as sink so only non-Output classes count here.
            DeadEnds.Add(MakeShared<FJsonValueObject>(Entry()));
        }
        if (MissingRequiredIn > 0)
        {
            TSharedPtr<FJsonObject> E = Entry();
            E->SetNumberField(TEXT("missing_input_count"), MissingRequiredIn);
            Disconnected.Add(MakeShared<FJsonValueObject>(E));
        }
    }

    auto R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetStringField(TEXT("source"), SourceDesc);
    R->SetNumberField(TEXT("total_nodes"), Graph->Nodes.Num());
    R->SetArrayField(TEXT("orphaned"), Orphans);
    R->SetArrayField(TEXT("dead_ends"), DeadEnds);
    R->SetArrayField(TEXT("missing_inputs"), Disconnected);
    R->SetBoolField(TEXT("graph_clean"),
        Orphans.Num() == 0 && DeadEnds.Num() == 0 && Disconnected.Num() == 0);
    return R;
#else
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only command"));
#endif
}
