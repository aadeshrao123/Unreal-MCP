#pragma once

#include "CoreMinimal.h"
#include "Json.h"

class UInputAction;
class UInputMappingContext;
class UInputTrigger;
class UInputModifier;
struct FEnhancedActionKeyMapping;

/**
 * Handler for Enhanced Input MCP commands.
 * Supports creating/reading/modifying UInputAction and UInputMappingContext assets,
 * including their triggers, modifiers, and key mappings.
 */
class UNREALMCPBRIDGE_API FEpicUnrealMCPEnhancedInputCommands
{
public:
	TSharedPtr<FJsonObject> HandleCommand(
		const FString& CommandType,
		const TSharedPtr<FJsonObject>& Params);

private:
	// --- Input Action ---
	TSharedPtr<FJsonObject> HandleCreateInputAction(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetInputAction(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetInputActionProperties(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAddInputActionTrigger(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAddInputActionModifier(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleRemoveInputActionTrigger(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleRemoveInputActionModifier(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleListInputActions(const TSharedPtr<FJsonObject>& Params);

	// --- Input Mapping Context ---
	TSharedPtr<FJsonObject> HandleCreateInputMappingContext(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetInputMappingContext(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAddKeyMapping(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleRemoveKeyMapping(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetKeyMapping(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAddMappingTrigger(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAddMappingModifier(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleRemoveMappingTrigger(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleRemoveMappingModifier(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleListInputMappingContexts(const TSharedPtr<FJsonObject>& Params);

	// --- Discovery / Utility ---
	TSharedPtr<FJsonObject> HandleListTriggerTypes(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleListModifierTypes(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleListInputKeys(const TSharedPtr<FJsonObject>& Params);

	// --- Helpers ---
	static UInputAction* LoadInputAction(const FString& AssetPath);
	static UInputMappingContext* LoadInputMappingContext(const FString& AssetPath);

	static UClass* ResolveTriggerClass(const FString& TypeName);
	static UClass* ResolveModifierClass(const FString& TypeName);

	static UInputTrigger* CreateTriggerInstance(
		UObject* Outer, const FString& TriggerType,
		const TSharedPtr<FJsonObject>& Properties);

	static UInputModifier* CreateModifierInstance(
		UObject* Outer, const FString& ModifierType,
		const TSharedPtr<FJsonObject>& Properties);

	static TSharedPtr<FJsonObject> SerializeTrigger(const UInputTrigger* Trigger, int32 Index);
	static TSharedPtr<FJsonObject> SerializeModifier(const UInputModifier* Modifier, int32 Index);
	static TSharedPtr<FJsonObject> SerializeMapping(const FEnhancedActionKeyMapping& Mapping, int32 Index);
};
