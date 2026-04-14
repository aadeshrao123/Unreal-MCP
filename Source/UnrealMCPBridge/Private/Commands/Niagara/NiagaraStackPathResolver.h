#pragma once

#include "CoreMinimal.h"

#if WITH_EDITORONLY_DATA
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeParameterMapSet.h"
#include "NiagaraTypes.h"
#include "NiagaraParameterMapHistory.h"
#include "EdGraphSchema_Niagara.h"
#include "EdGraph/EdGraphPin.h"

/**
 * Shared descent helper for nested input paths in Niagara stack mutators.
 *
 * Splits an input name like "Spawn Count.Position Array" by '.' and walks
 * through dynamic-input function-call nodes — each segment except the last
 * names an input on the current call, whose override pin must be linked to
 * a dynamic-input UNiagaraNodeFunctionCall to descend further.
 *
 * Returns the FINAL function-call node + the last segment name. Callers then
 * compute the AliasedHandle on the returned call and apply their mutation.
 *
 * Example: "Spawn Count.Position Array" on SpawnPerFrame
 *   - Segment 0 "Spawn Count" → finds override pin → follows to GetDataInterfaceLength FunctionCall
 *   - Segment 1 "Position Array" (last) → returns (GetDataInterfaceLength, "Position Array")
 */
namespace NiagaraStackPath
{
    /** Walk the override-pin chain from the initial module call to the
     *  function call that owns the leaf input. */
    inline UNiagaraNodeFunctionCall* DescendNestedPath(
        UNiagaraNodeFunctionCall& InitialCall,
        const FString& DotPath,
        FString& OutLeafInputName,
        FString& OutError)
    {
        TArray<FString> Segments;
        DotPath.ParseIntoArray(Segments, TEXT("."));
        if (Segments.Num() == 0)
        {
            OutError = TEXT("Empty input path");
            return nullptr;
        }
        if (Segments.Num() == 1)
        {
            // Top-level input — no descent needed. Most common case.
            OutLeafInputName = Segments[0];
            return &InitialCall;
        }

        UNiagaraNodeFunctionCall* CurrentCall = &InitialCall;
        for (int32 i = 0; i < Segments.Num() - 1; ++i)
        {
            // Find the parameter map set node feeding the current call.
            UNiagaraNodeParameterMapSet* OverrideNode = nullptr;
            for (UEdGraphPin* Pin : CurrentCall->Pins)
            {
                if (!Pin || Pin->Direction != EGPD_Input) continue;
                const FNiagaraTypeDefinition TypeDef =
                    UEdGraphSchema_Niagara::PinTypeToTypeDefinition(Pin->PinType);
                if (TypeDef != FNiagaraTypeDefinition::GetParameterMapDef()) continue;
                if (Pin->LinkedTo.Num() == 0) continue;
                OverrideNode = Cast<UNiagaraNodeParameterMapSet>(Pin->LinkedTo[0]->GetOwningNode());
                if (OverrideNode) break;
            }
            if (!OverrideNode)
            {
                OutError = FString::Printf(
                    TEXT("Path segment '%s' has no override node — input is not currently overridden"),
                    *Segments[i]);
                return nullptr;
            }

            // Find the override pin matching the segment.
            const FString ModulePrefixed = FString::Printf(TEXT("Module.%s"), *Segments[i]);
            const FNiagaraParameterHandle Aliased =
                FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(
                    FNiagaraParameterHandle(FName(*ModulePrefixed)), CurrentCall);
            const FName TargetPinName = Aliased.GetParameterHandleString();

            UEdGraphPin* OverridePin = nullptr;
            for (UEdGraphPin* Pin : OverrideNode->Pins)
            {
                if (Pin && Pin->Direction == EGPD_Input && Pin->PinName == TargetPinName)
                {
                    OverridePin = Pin;
                    break;
                }
            }
            if (!OverridePin || OverridePin->LinkedTo.Num() == 0)
            {
                OutError = FString::Printf(
                    TEXT("Path segment '%s' has no dynamic input attached to descend into"),
                    *Segments[i]);
                return nullptr;
            }

            UEdGraphPin* SourcePin = OverridePin->LinkedTo[0];
            if (!SourcePin || !SourcePin->GetOwningNodeUnchecked())
            {
                OutError = FString::Printf(TEXT("Path segment '%s' has invalid downstream node"), *Segments[i]);
                return nullptr;
            }

            UNiagaraNodeFunctionCall* NextCall =
                Cast<UNiagaraNodeFunctionCall>(SourcePin->GetOwningNode());
            if (!NextCall)
            {
                OutError = FString::Printf(
                    TEXT("Path segment '%s' is bound to a non-function-call node (e.g. linked parameter or expression) — cannot descend further"),
                    *Segments[i]);
                return nullptr;
            }
            CurrentCall = NextCall;
        }

        OutLeafInputName = Segments.Last();
        return CurrentCall;
    }
}
#endif // WITH_EDITORONLY_DATA
