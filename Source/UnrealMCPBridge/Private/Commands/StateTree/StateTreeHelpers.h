#pragma once

#include "CoreMinimal.h"
#include "Json.h"
#include "StateTreeTypes.h"
#include "StructUtils/InstancedStruct.h"

class UStateTree;
class UStateTreeEditorData;
class UStateTreeState;
struct FStateTreeEditorNode;
struct FStateTreeTransition;
struct FStateTreePropertyPathBinding;

/**
 * Shared helper functions for all StateTree MCP command files.
 * Provides asset loading, state resolution (tri-modal: index path / name / GUID),
 * node resolution, JSON serialization, and compilation utilities.
 */
namespace StateTreeHelpers
{
	// ---- Asset Loading ----

	/** Load a UStateTree from a content path. Returns null and sets OutError on failure. */
	UStateTree* LoadStateTree(const FString& AssetPath, FString& OutError);

	/** Get the UStateTreeEditorData from a loaded UStateTree. Returns null and sets OutError if missing. */
	UStateTreeEditorData* GetEditorData(UStateTree* Tree, FString& OutError);

	// ---- State Resolution (tri-modal) ----

	/**
	 * Resolve a state from JSON params. Checks for:
	 *   "state" (index path like "0/1/2"),
	 *   "state_name" (case-insensitive depth-first search),
	 *   "state_guid" (GUID string).
	 * Returns null and sets OutError if not found.
	 */
	UStateTreeState* ResolveState(
		UStateTreeEditorData* EditorData,
		const TSharedPtr<FJsonObject>& Params,
		FString& OutError);

	/** Resolve a state by index path (e.g. "0/1/2" = SubTrees[0]->Children[1]->Children[2]). */
	UStateTreeState* ResolveStateByPath(UStateTreeEditorData* EditorData, const FString& IndexPath, FString& OutError);

	/** Resolve a state by name (case-insensitive, depth-first). Returns error if ambiguous. */
	UStateTreeState* ResolveStateByName(UStateTreeEditorData* EditorData, const FString& Name, FString& OutError);

	/** Resolve a state by GUID string. */
	UStateTreeState* ResolveStateByGuid(UStateTreeEditorData* EditorData, const FGuid& Guid, FString& OutError);

	// ---- Index Path Computation ----

	/** Compute the index path string for a given state (e.g. "0/1/2"). */
	FString ComputeIndexPath(UStateTreeEditorData* EditorData, const UStateTreeState* State);

	// ---- Node Resolution ----

	/**
	 * Find an FStateTreeEditorNode by GUID across all states, evaluators, and global tasks.
	 * OutContext describes where it was found ("evaluator", "global_task", "task:StateName",
	 * "enter_condition:StateName", "transition_condition:StateName").
	 * OutArray and OutIndex point to the containing array and position.
	 */
	FStateTreeEditorNode* ResolveNodeByGuid(
		UStateTreeEditorData* EditorData,
		const FGuid& NodeGuid,
		FString& OutContext,
		TArray<FStateTreeEditorNode>*& OutArray,
		int32& OutIndex,
		FString& OutError);

	// ---- JSON Serialization ----

	/** Serialize a state to a compact summary (name, type, ID, path, counts). */
	TSharedPtr<FJsonObject> StateToJsonSummary(
		UStateTreeEditorData* EditorData,
		UStateTreeState* State);

	/** Serialize a state with full details (properties, tasks, conditions, transitions, parameters, bindings). */
	TSharedPtr<FJsonObject> StateToJsonDetailed(
		UStateTreeEditorData* EditorData,
		UStateTreeState* State,
		bool bIncludeBindings = false);

	/** Serialize an FStateTreeEditorNode (task/evaluator/condition) to JSON. */
	TSharedPtr<FJsonObject> EditorNodeToJson(const FStateTreeEditorNode& Node, int32 Index);

	/**
	 * Serialize an FStateTreeEditorNode with its bindings inlined.
	 * Looks up all bindings where the node's ID matches source or target struct ID.
	 */
	TSharedPtr<FJsonObject> EditorNodeToJsonWithBindings(
		const FStateTreeEditorNode& Node,
		int32 Index,
		UStateTreeEditorData* EditorData);

	/** Serialize an FStateTreeTransition to JSON. */
	TSharedPtr<FJsonObject> TransitionToJson(
		const FStateTreeTransition& Transition,
		int32 Index,
		UStateTreeEditorData* EditorData = nullptr,
		bool bIncludeBindings = false);

	/** Serialize all properties of an FInstancedStruct using reflection. */
	TSharedPtr<FJsonObject> SerializeInstancedStructProperties(const FInstancedStruct& Struct);

	// ---- Binding Serialization ----

	/**
	 * Serialize a single property binding to JSON with full property path details.
	 * Resolves struct IDs to human-readable names via GetBindableStructByID.
	 */
	TSharedPtr<FJsonObject> BindingToJson(
		const FStateTreePropertyPathBinding& Binding,
		int32 Index,
		UStateTreeEditorData* EditorData);

	/**
	 * Collect all bindings that reference the given struct ID (as source or target).
	 * Returns a JSON array of binding objects.
	 */
	TArray<TSharedPtr<FJsonValue>> CollectBindingsForNode(
		const FGuid& NodeStructID,
		UStateTreeEditorData* EditorData);

	/**
	 * Resolve a binding struct ID to a human-readable name.
	 * Uses EditorData->GetBindableStructByID and falls back to node search.
	 */
	FString ResolveStructIDToName(const FGuid& StructID, UStateTreeEditorData* EditorData);

	// ---- Enum Conversion ----

	FString StateTypeToString(uint8 Type);
	FString SelectionBehaviorToString(uint8 Behavior);
	FString TransitionTriggerToString(uint8 Trigger);
	FString TransitionPriorityToString(uint8 Priority);
	FString ExpressionOperandToString(uint8 Operand);
	FString TaskCompletionTypeToString(uint8 CompletionType);

	// ---- Enum Parsing / Class Resolution ----

	/** Parses an MCP input string to a StateTree state type. */
	bool ParseStateType(const FString& Str, EStateTreeStateType& OutType);

	/** Parses MCP input string into a StateTree child state selection behavior. */
	bool ParseSelectionBehavior(const FString& Str, EStateTreeStateSelectionBehavior& OutBehavior);

	/** Parses MCP input string into a StateTree transition trigger type. */
	bool ParseTransitionTrigger(const FString& Str, EStateTreeTransitionTrigger& OutTrigger);

	/** Parses MCP input string into a StateTree transition priority. */
	bool ParseTransitionPriority(const FString& Str, EStateTreeTransitionPriority& OutPriority);

	/** Resolves a UStateTreeSchema subclass by short name or full path. */
	UClass* ResolveSchemaClass(const FString& SchemaName, FString& OutError);

	// ---- Compilation ----

	/**
	 * Compile a StateTree and return success/failure.
	 * OutMessages contains compiler errors/warnings.
	 */
	bool CompileTree(UStateTree* Tree, TArray<FString>& OutMessages);
}
