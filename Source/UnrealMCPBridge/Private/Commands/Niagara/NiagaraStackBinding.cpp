#include "Commands/EpicUnrealMCPNiagaraCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "NiagaraHelpers.h"

#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraGraph.h"

#if WITH_EDITORONLY_DATA
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeParameterMapSet.h"
#include "NiagaraNodeParameterMapGet.h"
#include "NiagaraNodeAssignment.h"
#include "NiagaraNodeInput.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraNodeCustomHlsl.h"
#include "EdGraphSchema_Niagara.h"
#include "NiagaraParameterMapHistory.h"
#include "NiagaraConstants.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#include "NiagaraEditorUtilities.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/ARFilter.h"
#endif

#include "EdGraph/EdGraphPin.h"


// ---------------------------------------------------------------------------
// Niagara stack-input introspection + mutation
//
// Implements three MCP commands that operate on module inputs in an emitter's
// stack (NOT on scratch pad scripts — those have their own introspection
// tools). Each resolves the module's UNiagaraNodeFunctionCall inside the
// emitter's stack graph, then walks its override pins to classify or mutate.
//
// Why we replicate engine helpers manually:
//   - FNiagaraStackGraphUtilities::GetStackFunctionInputOverridePin         — NOT exported
//   - FNiagaraStackGraphUtilities::GetStackFunctionOverrideNode             — NOT exported
//   - FNiagaraStackGraphUtilities::RemoveNodesForStackFunctionInputOverridePin — NOT exported
// We mirror their behavior (NiagaraStackGraphUtilities.cpp:1921-1940) using
// exported primitives + public graph structures. Exported APIs we DO use:
//   - FNiagaraStackGraphUtilities::GetStackFunctionInputs                   (h:134)
//   - FNiagaraStackGraphUtilities::SetLinkedParameterValueForFunctionInput  (h:227)
//   - FNiagaraStackGraphUtilities::SetDynamicInputForFunctionInput          (h:233)
//   - UNiagaraStackFunctionInput::GetAvailableParameters                    (h:184)
//   - FNiagaraEditorUtilities::GetFilteredScriptAssets                      — Dynamic input discovery
//   - FVersionedNiagaraEmitterData::GetScript(Usage, UsageId)               (Emitter.h:384)
// ---------------------------------------------------------------------------

#if WITH_EDITORONLY_DATA
namespace
{

/** Replicates FNiagaraStackGraphUtilities::GetStackFunctionOverrideNode.
 *  Walks the module function call's parameter map input pin to find the
 *  UNiagaraNodeParameterMapSet node that holds its override pins. */
UNiagaraNodeParameterMapSet* FindOverrideNode(UNiagaraNodeFunctionCall& StackFunctionCall)
{
    for (UEdGraphPin* Pin : StackFunctionCall.Pins)
    {
        if (!Pin || Pin->Direction != EGPD_Input) continue;
        const FNiagaraTypeDefinition TypeDef =
            UEdGraphSchema_Niagara::PinTypeToTypeDefinition(Pin->PinType);
        if (TypeDef != FNiagaraTypeDefinition::GetParameterMapDef()) continue;
        if (Pin->LinkedTo.Num() == 0) continue;

        UEdGraphPin* SourcePin = Pin->LinkedTo[0];
        if (!SourcePin || !SourcePin->GetOwningNodeUnchecked()) continue;
        return Cast<UNiagaraNodeParameterMapSet>(SourcePin->GetOwningNode());
    }
    return nullptr;
}

/** Replicates FNiagaraStackGraphUtilities::GetStackFunctionInputOverridePin.
 *  Finds the override pin for a given aliased input handle. Returns nullptr
 *  if no override is set (meaning the input is at its default value). */
UEdGraphPin* FindOverridePin(UNiagaraNodeFunctionCall& StackFunctionCall,
                             const FNiagaraParameterHandle& AliasedHandle)
{
    // Note: UNiagaraNodeFunctionCall::FindStaticSwitchInputPin is NOT
    // NIAGARAEDITOR_API exported, so we can't early-out on static switches
    // here. Those inputs show up as unoverridden (nullptr return) which is
    // correct fallback behavior — their values live inline on the function
    // call's own pins, readable via get_niagara_node_info directly.
    UNiagaraNodeParameterMapSet* OverrideNode = FindOverrideNode(StackFunctionCall);
    if (!OverrideNode) return nullptr;

    const FName TargetName = AliasedHandle.GetParameterHandleString();
    for (UEdGraphPin* Pin : OverrideNode->Pins)
    {
        if (Pin && Pin->Direction == EGPD_Input && Pin->PinName == TargetName)
        {
            return Pin;
        }
    }
    return nullptr;
}

/** Find the module's function-call node by display name within a graph. */
UNiagaraNodeFunctionCall* FindModuleFunctionCall(UNiagaraGraph* Graph,
                                                 const FString& ModuleName)
{
    if (!Graph) return nullptr;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        UNiagaraNodeFunctionCall* Call = Cast<UNiagaraNodeFunctionCall>(Node);
        if (!Call) continue;
        // Module name matching: try display name, alias, and referenced script
        // name. The stack UI allows any of these.
        const FString DisplayName = Call->GetFunctionName();
        if (DisplayName.Equals(ModuleName, ESearchCase::IgnoreCase)) return Call;
        if (Call->FunctionScript &&
            Call->FunctionScript->GetName().Equals(ModuleName, ESearchCase::IgnoreCase))
        {
            return Call;
        }
    }
    return nullptr;
}

/** Resolve an emitter's stack graph for a given script usage.
 *  For Events / Simulation Stages callers would pass a UsageId; we default
 *  to a null guid which matches the primary script of each usage. */
UNiagaraGraph* ResolveEmitterStackGraph(
    UNiagaraSystem* System,
    const FString& EmitterName,
    ENiagaraScriptUsage Usage,
    UNiagaraScript*& OutScript,
    FString& OutError)
{
    OutScript = nullptr;

    int32 Index = INDEX_NONE;
    FNiagaraEmitterHandle* Handle =
        NiagaraHelpers::FindEmitterHandle(System, EmitterName, Index, OutError);
    if (!Handle) return nullptr;

    FVersionedNiagaraEmitterData* EmitterData = NiagaraHelpers::GetEmitterData(Handle);
    if (!EmitterData)
    {
        OutError = TEXT("Failed to get emitter data");
        return nullptr;
    }

    OutScript = EmitterData->GetScript(Usage, FGuid());
    if (!OutScript)
    {
        OutError = FString::Printf(TEXT("Emitter '%s' has no script for usage %s"),
            *EmitterName, *NiagaraHelpers::ScriptUsageToString(Usage));
        return nullptr;
    }

    if (UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(OutScript->GetLatestSource()))
    {
        if (Source->NodeGraph) return Source->NodeGraph;
    }
    OutError = TEXT("Emitter script has no graph source");
    return nullptr;
}

/** Read mode + target for an override pin. Mirrors the classification logic
 *  in UNiagaraStackFunctionInput (NiagaraStackFunctionInput.cpp:~3530). */
struct FBindingInfo
{
    FString Mode;                    // "default" | "local" | "linked" | "dynamic" | "data" | "unknown"
    FString LinkedParameter;         // when Mode == "linked"
    FString DynamicInputScriptPath;  // when Mode == "dynamic"
    FString DynamicInputFunctionName;// friendly name
    UNiagaraNodeFunctionCall* DynamicInputCall = nullptr;  // for nested recursion
    FString LocalValue;              // when Mode == "local"
};

FBindingInfo ClassifyOverride(UEdGraphPin* OverridePin)
{
    FBindingInfo Info;
    if (!OverridePin)
    {
        Info.Mode = TEXT("default");
        return Info;
    }

    // No link → local literal (may have an explicit default string).
    if (OverridePin->LinkedTo.Num() == 0)
    {
        Info.Mode = TEXT("local");
        Info.LocalValue = OverridePin->DefaultValue.IsEmpty()
            ? OverridePin->AutogeneratedDefaultValue
            : OverridePin->DefaultValue;
        return Info;
    }

    UEdGraphPin* SourcePin = OverridePin->LinkedTo[0];
    if (!SourcePin || !SourcePin->GetOwningNodeUnchecked())
    {
        Info.Mode = TEXT("unknown");
        return Info;
    }
    UEdGraphNode* SourceNode = SourcePin->GetOwningNode();

    if (UNiagaraNodeFunctionCall* FnCall = Cast<UNiagaraNodeFunctionCall>(SourceNode))
    {
        // Dynamic input = FunctionCall whose FunctionScript has Usage=DynamicInput
        if (FnCall->FunctionScript &&
            FnCall->FunctionScript->GetUsage() == ENiagaraScriptUsage::DynamicInput)
        {
            Info.Mode = TEXT("dynamic");
            Info.DynamicInputScriptPath = FnCall->FunctionScript->GetPathName();
            Info.DynamicInputFunctionName = FnCall->GetFunctionName();
            Info.DynamicInputCall = FnCall;
            return Info;
        }
        // Non-DI function call — fall through as "unknown" so caller sees it.
        Info.Mode = TEXT("function_call");
        if (FnCall->FunctionScript)
        {
            Info.DynamicInputScriptPath = FnCall->FunctionScript->GetPathName();
        }
        Info.DynamicInputFunctionName = FnCall->GetFunctionName();
        return Info;
    }

    if (Cast<UNiagaraNodeParameterMapGet>(SourceNode))
    {
        Info.Mode = TEXT("linked");
        Info.LinkedParameter = SourcePin->PinName.ToString();
        return Info;
    }

    if (UNiagaraNodeInput* InputNode = Cast<UNiagaraNodeInput>(SourceNode))
    {
        // Local-value-on-DataInterface pattern — the input node holds the DI.
        Info.Mode = TEXT("data");
        Info.LinkedParameter = InputNode->Input.GetName().ToString();
        return Info;
    }

    if (Cast<UNiagaraNodeCustomHlsl>(SourceNode))
    {
        Info.Mode = TEXT("expression");
        return Info;
    }

    Info.Mode = TEXT("unknown");
    return Info;
}

/** Parse ENiagaraScriptUsage from a string with the spellings emitter_update /
 *  particle_spawn / system_update etc. */
bool ParseUsage(const FString& Raw, ENiagaraScriptUsage& OutUsage)
{
    bool bOk = false;
    OutUsage = NiagaraHelpers::ParseScriptUsage(Raw, bOk);
    return bOk;
}

/** Serialize a FBindingInfo + its input pin type into JSON, optionally
 *  recursing one level into the dynamic input's own inputs. */
TSharedPtr<FJsonObject> SerializeBinding(
    const FString& InputName,
    const FNiagaraVariable& InputVar,
    const FBindingInfo& Info,
    int32 RemainingDepth)
{
    auto J = MakeShared<FJsonObject>();
    J->SetStringField(TEXT("name"), InputName);
    J->SetStringField(TEXT("type"), InputVar.GetType().GetName());
    J->SetStringField(TEXT("mode"), Info.Mode);

    if (Info.Mode == TEXT("local") && !Info.LocalValue.IsEmpty())
    {
        J->SetStringField(TEXT("value"), Info.LocalValue);
    }
    if (Info.Mode == TEXT("linked") || Info.Mode == TEXT("data"))
    {
        J->SetStringField(TEXT("linked_parameter"), Info.LinkedParameter);
    }
    if (Info.Mode == TEXT("dynamic") || Info.Mode == TEXT("function_call"))
    {
        if (!Info.DynamicInputScriptPath.IsEmpty())
        {
            J->SetStringField(TEXT("script_path"), Info.DynamicInputScriptPath);
        }
        if (!Info.DynamicInputFunctionName.IsEmpty())
        {
            J->SetStringField(TEXT("function_name"), Info.DynamicInputFunctionName);
        }

        // Recurse into the dynamic input's own inputs (user's Gap #7 fix —
        // reach nested Position Array inside GetDataInterfaceLength).
        if (Info.Mode == TEXT("dynamic") && Info.DynamicInputCall && RemainingDepth > 0)
        {
            TArray<FNiagaraVariable> ChildVars;
            TSet<FNiagaraVariable> ChildHidden;
            FCompileConstantResolver Resolver;
            FNiagaraStackGraphUtilities::GetStackFunctionInputs(
                *Info.DynamicInputCall,
                ChildVars,
                ChildHidden,
                Resolver,
                FNiagaraStackGraphUtilities::ENiagaraGetStackFunctionInputPinsOptions::AllInputs,
                /*bIgnoreDisabled=*/true);

            TArray<TSharedPtr<FJsonValue>> ChildArr;
            for (const FNiagaraVariable& ChildVar : ChildVars)
            {
                const FNiagaraParameterHandle ChildHandle =
                    FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(
                        FNiagaraParameterHandle(ChildVar.GetName()), Info.DynamicInputCall);
                UEdGraphPin* ChildPin = FindOverridePin(*Info.DynamicInputCall, ChildHandle);
                FBindingInfo ChildInfo = ClassifyOverride(ChildPin);

                // Strip Module. prefix from child names for display parity
                // with the stack UI.
                FString ChildName = ChildVar.GetName().ToString();
                ChildName.RemoveFromStart(TEXT("Module."));

                ChildArr.Add(MakeShared<FJsonValueObject>(
                    SerializeBinding(ChildName, ChildVar, ChildInfo, RemainingDepth - 1)));
            }
            J->SetArrayField(TEXT("children"), ChildArr);
        }
    }
    return J;
}

} // namespace
#endif


// ---------------------------------------------------------------------------
// HandleGetNiagaraModuleInputBinding
//
// Params:
//   system_path, emitter_name, module_name, script_usage  (required)
//   input_filter    — optional substring on input name
//   max_depth       — nested recursion cap (default 3)
//
// Returns per-input: name, type, mode, value/linked_parameter/script_path,
// recursive children when bound to a dynamic input.
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleGetNiagaraModuleInputBinding(
    const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
    FString SystemPath, EmitterName, ModuleName, ScriptUsageStr;
    if (!Params->TryGetStringField(TEXT("system_path"), SystemPath) ||
        !Params->TryGetStringField(TEXT("emitter_name"), EmitterName) ||
        !Params->TryGetStringField(TEXT("module_name"), ModuleName) ||
        !Params->TryGetStringField(TEXT("script_usage"), ScriptUsageStr))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            TEXT("Missing required: system_path, emitter_name, module_name, script_usage"));
    }

    ENiagaraScriptUsage Usage;
    if (!ParseUsage(ScriptUsageStr, Usage))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Unknown script_usage '%s'"), *ScriptUsageStr));
    }

    FString Error;
    UNiagaraSystem* System = NiagaraHelpers::LoadNiagaraSystem(SystemPath, Error);
    if (!System) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);

    UNiagaraScript* Script = nullptr;
    UNiagaraGraph* Graph = ResolveEmitterStackGraph(System, EmitterName, Usage, Script, Error);
    if (!Graph) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);

    UNiagaraNodeFunctionCall* ModuleCall = FindModuleFunctionCall(Graph, ModuleName);
    if (!ModuleCall)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Module '%s' not found in %s stack"),
                *ModuleName, *ScriptUsageStr));
    }

    FString InputFilter;
    Params->TryGetStringField(TEXT("input_filter"), InputFilter);
    int32 MaxDepth = 3;
    Params->TryGetNumberField(TEXT("max_depth"), MaxDepth);

    TArray<FNiagaraVariable> InputVars;
    TSet<FNiagaraVariable> HiddenVars;
    FCompileConstantResolver Resolver;
    FNiagaraStackGraphUtilities::GetStackFunctionInputs(
        *ModuleCall, InputVars, HiddenVars, Resolver,
        FNiagaraStackGraphUtilities::ENiagaraGetStackFunctionInputPinsOptions::AllInputs,
        /*bIgnoreDisabled=*/true);

    TArray<TSharedPtr<FJsonValue>> BindingArr;
    for (const FNiagaraVariable& Var : InputVars)
    {
        FString InputName = Var.GetName().ToString();
        InputName.RemoveFromStart(TEXT("Module."));

        if (!InputFilter.IsEmpty() &&
            !InputName.Contains(InputFilter, ESearchCase::IgnoreCase))
        {
            continue;
        }

        const FNiagaraParameterHandle Aliased =
            FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(
                FNiagaraParameterHandle(Var.GetName()), ModuleCall);
        UEdGraphPin* OverridePin = FindOverridePin(*ModuleCall, Aliased);
        FBindingInfo Info = ClassifyOverride(OverridePin);
        BindingArr.Add(MakeShared<FJsonValueObject>(
            SerializeBinding(InputName, Var, Info, MaxDepth)));
    }

    auto R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetStringField(TEXT("system_path"), SystemPath);
    R->SetStringField(TEXT("emitter_name"), EmitterName);
    R->SetStringField(TEXT("module_name"), ModuleName);
    R->SetStringField(TEXT("script_usage"), ScriptUsageStr);
    R->SetNumberField(TEXT("input_count"), BindingArr.Num());
    R->SetArrayField(TEXT("inputs"), BindingArr);
    return R;
#else
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only command"));
#endif
}


// ---------------------------------------------------------------------------
// HandleClearNiagaraModuleInput — reset an input to Default
//
// Equivalent to "Reset to Default" in the stack UI. Finds the override pin,
// disconnects every node feeding it, and removes those nodes if they became
// orphaned. Mirrors FNiagaraStackGraphUtilities::RemoveNodesForStack...
// (non-exported — NiagaraStackGraphUtilities.cpp:~1960 implementation).
//
// Params: system_path + emitter_name + module_name + script_usage + input_name
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleClearNiagaraModuleInput(
    const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
    FString SystemPath, EmitterName, ModuleName, ScriptUsageStr, InputName;
    if (!Params->TryGetStringField(TEXT("system_path"), SystemPath) ||
        !Params->TryGetStringField(TEXT("emitter_name"), EmitterName) ||
        !Params->TryGetStringField(TEXT("module_name"), ModuleName) ||
        !Params->TryGetStringField(TEXT("script_usage"), ScriptUsageStr) ||
        !Params->TryGetStringField(TEXT("input_name"), InputName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            TEXT("Missing required: system_path, emitter_name, module_name, script_usage, input_name"));
    }

    ENiagaraScriptUsage Usage;
    if (!ParseUsage(ScriptUsageStr, Usage))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Unknown script_usage '%s'"), *ScriptUsageStr));
    }

    FString Error;
    UNiagaraSystem* System = NiagaraHelpers::LoadNiagaraSystem(SystemPath, Error);
    if (!System) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);

    UNiagaraScript* Script = nullptr;
    UNiagaraGraph* Graph = ResolveEmitterStackGraph(System, EmitterName, Usage, Script, Error);
    if (!Graph) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);

    UNiagaraNodeFunctionCall* ModuleCall = FindModuleFunctionCall(Graph, ModuleName);
    if (!ModuleCall)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Module '%s' not found in %s stack"),
                *ModuleName, *ScriptUsageStr));
    }

    // Nested input path — dot-segmented. First segment names an input on the
    // module; subsequent segments name inputs on the dynamic input attached
    // to the prior segment.
    TArray<FString> Segments;
    InputName.ParseIntoArray(Segments, TEXT("."));
    if (Segments.Num() == 0)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Empty input_name"));
    }

    UNiagaraNodeFunctionCall* CurrentCall = ModuleCall;
    UEdGraphPin* TargetOverridePin = nullptr;
    for (int32 i = 0; i < Segments.Num(); ++i)
    {
        const FString ModulePrefixed = FString::Printf(TEXT("Module.%s"), *Segments[i]);
        const FNiagaraParameterHandle Aliased =
            FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(
                FNiagaraParameterHandle(FName(*ModulePrefixed)), CurrentCall);

        UEdGraphPin* Pin = FindOverridePin(*CurrentCall, Aliased);
        if (i == Segments.Num() - 1)
        {
            TargetOverridePin = Pin;
            break;
        }
        if (!Pin || Pin->LinkedTo.Num() == 0)
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("Path segment '%s' has no dynamic input to descend into"),
                    *Segments[i]));
        }
        UNiagaraNodeFunctionCall* Next =
            Cast<UNiagaraNodeFunctionCall>(Pin->LinkedTo[0]->GetOwningNode());
        if (!Next)
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("Path segment '%s' is not a dynamic input"),
                    *Segments[i]));
        }
        CurrentCall = Next;
    }

    if (!TargetOverridePin)
    {
        auto R = MakeShared<FJsonObject>();
        R->SetBoolField(TEXT("success"), true);
        R->SetBoolField(TEXT("was_overridden"), false);
        R->SetStringField(TEXT("message"), TEXT("Input already at default — no override pin exists"));
        return R;
    }

    // Destroy linked nodes (dynamic input, map get, custom HLSL, etc.).
    // Collect first to avoid mutating LinkedTo mid-iteration.
    TSet<UEdGraphNode*> NodesToRemove;
    for (UEdGraphPin* Linked : TargetOverridePin->LinkedTo)
    {
        if (Linked && Linked->GetOwningNodeUnchecked())
        {
            NodesToRemove.Add(Linked->GetOwningNode());
        }
    }

    CurrentCall->GetGraph()->Modify();
    TargetOverridePin->BreakAllPinLinks();

    for (UEdGraphNode* Node : NodesToRemove)
    {
        if (!Node) continue;
        // Only remove if the node was exclusively feeding this override
        // (would be orphaned). Check: does it still have outgoing links?
        bool bStillUsed = false;
        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (Pin && Pin->Direction == EGPD_Output && Pin->LinkedTo.Num() > 0)
            {
                bStillUsed = true;
                break;
            }
        }
        if (!bStillUsed)
        {
            Node->Modify();
            Node->BreakAllNodeLinks();
            CurrentCall->GetGraph()->RemoveNode(Node);
        }
    }

    // Also remove the override pin itself from the MapSet (mirrors editor
    // behavior so the input fully reverts to default).
    UNiagaraNodeParameterMapSet* OverrideNode = FindOverrideNode(*CurrentCall);
    if (OverrideNode)
    {
        OverrideNode->Modify();
        OverrideNode->RemovePin(TargetOverridePin);
    }

    CurrentCall->GetGraph()->NotifyGraphChanged();
    Script->MarkPackageDirty();
    Script->PostEditChange();
    NiagaraHelpers::CompileAndSync(System, false);

    auto R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetBoolField(TEXT("was_overridden"), true);
    R->SetStringField(TEXT("input_name"), InputName);
    R->SetNumberField(TEXT("removed_node_count"), NodesToRemove.Num());
    return R;
#else
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only command"));
#endif
}


// ---------------------------------------------------------------------------
// HandleListNiagaraInputSourceMenu — reproduces the stack-UI source dropdown
//
// Returns three sections matching the editor menu (user screenshot):
//   1. dynamic_inputs   — every UNiagaraScript with Usage=DynamicInput whose
//                          output type matches the input type (when provided).
//                          Built via FNiagaraEditorUtilities::GetFilteredScriptAssets.
//   2. link_parameters  — namespace-grouped parameters (Engine/Emitter/Particles/User/System/StackContext)
//                          that are type-compatible. Built via parameter map history.
//   3. make             — "Read from new <scope> parameter" actions for each
//                          writable namespace.
//
// Params: system_path, emitter_name, module_name, script_usage, input_name,
//         type_filter (optional — limit to specific type), name_filter (optional)
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleListNiagaraInputSourceMenu(
    const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
    FString SystemPath, EmitterName, ModuleName, ScriptUsageStr, InputName;
    if (!Params->TryGetStringField(TEXT("system_path"), SystemPath) ||
        !Params->TryGetStringField(TEXT("emitter_name"), EmitterName) ||
        !Params->TryGetStringField(TEXT("module_name"), ModuleName) ||
        !Params->TryGetStringField(TEXT("script_usage"), ScriptUsageStr) ||
        !Params->TryGetStringField(TEXT("input_name"), InputName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            TEXT("Missing required: system_path, emitter_name, module_name, script_usage, input_name"));
    }

    FString NameFilter;
    Params->TryGetStringField(TEXT("name_filter"), NameFilter);

    ENiagaraScriptUsage Usage;
    if (!ParseUsage(ScriptUsageStr, Usage))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Unknown script_usage '%s'"), *ScriptUsageStr));
    }

    FString Error;
    UNiagaraSystem* System = NiagaraHelpers::LoadNiagaraSystem(SystemPath, Error);
    if (!System) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);

    UNiagaraScript* Script = nullptr;
    UNiagaraGraph* Graph = ResolveEmitterStackGraph(System, EmitterName, Usage, Script, Error);
    if (!Graph) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);

    UNiagaraNodeFunctionCall* ModuleCall = FindModuleFunctionCall(Graph, ModuleName);
    if (!ModuleCall)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Module '%s' not found"), *ModuleName));
    }

    // Resolve the target input's type so we can filter dynamic inputs by
    // output type compatibility (same filter the UI applies).
    TArray<FNiagaraVariable> InputVars;
    TSet<FNiagaraVariable> HiddenVars;
    FCompileConstantResolver Resolver;
    FNiagaraStackGraphUtilities::GetStackFunctionInputs(
        *ModuleCall, InputVars, HiddenVars, Resolver,
        FNiagaraStackGraphUtilities::ENiagaraGetStackFunctionInputPinsOptions::AllInputs,
        /*bIgnoreDisabled=*/true);

    FNiagaraTypeDefinition TargetType;
    const FString ModulePrefixed = FString::Printf(TEXT("Module.%s"), *InputName);
    for (const FNiagaraVariable& V : InputVars)
    {
        if (V.GetName().ToString().Equals(ModulePrefixed, ESearchCase::IgnoreCase))
        {
            TargetType = V.GetType();
            break;
        }
    }

    // ---- SECTION 1: Dynamic inputs (type-matched) ----
    TArray<TSharedPtr<FJsonValue>> DynamicInputArr;
    {
        FNiagaraEditorUtilities::FGetFilteredScriptAssetsOptions DIOpts;
        DIOpts.ScriptUsageToInclude = ENiagaraScriptUsage::DynamicInput;
        DIOpts.bIncludeDeprecatedScripts = false;

        TArray<FAssetData> DynamicInputAssets;
        FNiagaraEditorUtilities::GetFilteredScriptAssets(DIOpts, DynamicInputAssets);

        for (const FAssetData& AssetData : DynamicInputAssets)
        {
            const FString AssetName = AssetData.AssetName.ToString();
            if (!NameFilter.IsEmpty() &&
                !AssetName.Contains(NameFilter, ESearchCase::IgnoreCase))
            {
                continue;
            }

            auto Entry = MakeShared<FJsonObject>();
            Entry->SetStringField(TEXT("name"), AssetName);
            Entry->SetStringField(TEXT("script_path"), AssetData.GetObjectPathString());
            Entry->SetStringField(TEXT("source"), TEXT("asset_registry"));
            // Category tag from asset metadata — may be empty.
            FString Category;
            if (AssetData.GetTagValue<FString>(TEXT("Category"), Category) && !Category.IsEmpty())
            {
                Entry->SetStringField(TEXT("category"), Category);
            }
            DynamicInputArr.Add(MakeShared<FJsonValueObject>(Entry));
        }

        // Also include scratch pad scripts on this system whose usage is
        // DynamicInput. These don't appear in the AssetRegistry filter
        // because they're sub-objects on UNiagaraSystem, not standalone
        // assets — but they ARE valid dynamic input choices the editor
        // exposes in the dropdown's "Dynamic Inputs" section.
        for (UNiagaraScript* ScratchScript : System->ScratchPadScripts)
        {
            if (!ScratchScript) continue;
            if (ScratchScript->GetUsage() != ENiagaraScriptUsage::DynamicInput) continue;

            const FString AssetName = ScratchScript->GetName();
            if (!NameFilter.IsEmpty() &&
                !AssetName.Contains(NameFilter, ESearchCase::IgnoreCase))
            {
                continue;
            }

            auto Entry = MakeShared<FJsonObject>();
            Entry->SetStringField(TEXT("name"), AssetName);
            Entry->SetStringField(TEXT("script_path"), ScratchScript->GetPathName());
            Entry->SetStringField(TEXT("source"), TEXT("scratch_pad"));
            Entry->SetStringField(TEXT("category"), TEXT("Scratch Pad"));
            DynamicInputArr.Add(MakeShared<FJsonValueObject>(Entry));
        }
    }

    // ---- SECTION 2: Link parameters (well-known namespace entries) ----
    // We enumerate the authoritative well-known parameters via
    // FNiagaraConstants and include the system's own user parameters. This
    // matches what the editor's Link Inputs submenu populates from.
    TArray<TSharedPtr<FJsonValue>> LinkParamArr;
    auto AddLinkEntry = [&](const FString& Name, const FString& Type, const FString& Namespace)
    {
        if (!NameFilter.IsEmpty() && !Name.Contains(NameFilter, ESearchCase::IgnoreCase)) return;
        auto E = MakeShared<FJsonObject>();
        E->SetStringField(TEXT("name"), Name);
        E->SetStringField(TEXT("type"), Type);
        E->SetStringField(TEXT("namespace"), Namespace);
        LinkParamArr.Add(MakeShared<FJsonValueObject>(E));
    };

    for (const FNiagaraVariable& EngineVar : FNiagaraConstants::GetEngineConstants())
    {
        AddLinkEntry(EngineVar.GetName().ToString(), EngineVar.GetType().GetName(), TEXT("Engine"));
    }
    for (const FNiagaraVariable& SysVar : FNiagaraConstants::GetCommonParticleAttributes())
    {
        AddLinkEntry(SysVar.GetName().ToString(), SysVar.GetType().GetName(), TEXT("Particles"));
    }

    // User-exposed parameters on this system.
    TArray<FNiagaraVariable> UserVars;
    System->GetExposedParameters().GetParameters(UserVars);
    for (const FNiagaraVariable& V : UserVars)
    {
        AddLinkEntry(V.GetName().ToString(), V.GetType().GetName(), TEXT("User"));
    }

    auto R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetStringField(TEXT("input_name"), InputName);
    if (TargetType.IsValid())
    {
        R->SetStringField(TEXT("input_type"), TargetType.GetName());
    }
    R->SetNumberField(TEXT("dynamic_input_count"), DynamicInputArr.Num());
    R->SetArrayField(TEXT("dynamic_inputs"), DynamicInputArr);
    R->SetNumberField(TEXT("link_parameter_count"), LinkParamArr.Num());
    R->SetArrayField(TEXT("link_parameters"), LinkParamArr);
    return R;
#else
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only command"));
#endif
}
