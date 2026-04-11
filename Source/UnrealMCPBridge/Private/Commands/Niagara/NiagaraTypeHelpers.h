#pragma once

#include "CoreMinimal.h"
#include "NiagaraTypes.h"

class UEdGraphPin;
class UNiagaraNode;
class UNiagaraNodeFunctionCall;

/**
 * Shared helpers for type string <-> FNiagaraTypeDefinition conversion,
 * and for programmatic pin manipulation that reproduces the internal
 * UNiagaraNodeWithDynamicPins::RequestNewTypedPin logic using only
 * exported engine APIs (most NiagaraEditor pin helpers are not
 * NIAGARAEDITOR_API exported).
 */
namespace NiagaraTypeHelpers
{
	// ---- Type string <-> FNiagaraTypeDefinition ----

	/**
	 * Parse a user-friendly type name string (e.g. "float", "vec3", "LinearColor",
	 * "Quat", "Vector4", "ParameterMap", "bool") into a FNiagaraTypeDefinition.
	 * Falls back to FNiagaraTypeRegistry::GetRegisteredTypeByName if not a built-in.
	 * Returns true on success.
	 */
	bool ParseTypeDef(const FString& TypeString, FNiagaraTypeDefinition& OutType);

	/** Human-readable name for a FNiagaraTypeDefinition (display + serialization). */
	FString TypeDefToString(const FNiagaraTypeDefinition& TypeDef);

	// ---- Add-pin helpers (UNiagaraNodeWithDynamicPins::IsAddPin is not exported) ----

	/** True if the pin is a dynamic "+" Add pin on a Niagara node. */
	bool IsAddPin(const UEdGraphPin* Pin);

	// ---- Dynamic pin reproduction (RequestNewTypedPin equivalent) ----

	/**
	 * Reproduces UNiagaraNodeWithDynamicPins::RequestNewTypedPin using only
	 * exported APIs. Handles finding/creating the "Add" pin, assigning the
	 * correct PinType, and creating the replacement Add pin.
	 *
	 * If bRebuildSignature is true and the node is a UNiagaraNodeFunctionCall
	 * (or subclass such as UNiagaraNodeCustomHlsl), the Signature.Inputs/Outputs
	 * are rebuilt from the pin list (same logic as RebuildSignatureFromPins).
	 *
	 * Returns the newly created pin or nullptr on failure.
	 */
	UEdGraphPin* AddTypedPin(
		UNiagaraNode* Node,
		EEdGraphPinDirection Direction,
		const FNiagaraTypeDefinition& Type,
		const FName& InName,
		bool bRebuildSignature = true);

	/**
	 * Rebuild FNiagaraFunctionSignature.Inputs/Outputs from the node's current
	 * pins. Mirrors UNiagaraNodeCustomHlsl::RebuildSignatureFromPins.
	 */
	void RebuildSignatureFromPins(UNiagaraNodeFunctionCall* FunctionCallNode);

	/**
	 * Rename an existing dynamic pin. For UNiagaraNodeCustomHlsl this also
	 * token-replaces the old name in the HLSL source.
	 */
	bool RenameDynamicPin(
		UNiagaraNode* Node,
		const FName& OldName,
		const FName& NewName,
		FString& OutError);

	/**
	 * Remove a dynamic pin by name. Rebuilds signature on function-call nodes.
	 */
	bool RemoveDynamicPinByName(
		UNiagaraNode* Node,
		const FName& PinName,
		FString& OutError);

	/**
	 * Set a CustomHlsl node's HLSL source via reflection (SetCustomHlsl is
	 * not exported). Triggers PostEditChangeProperty so pins/signature
	 * refresh correctly.
	 */
	bool SetCustomHlslViaReflection(UNiagaraNode* HlslNode, const FString& HlslCode);
}
