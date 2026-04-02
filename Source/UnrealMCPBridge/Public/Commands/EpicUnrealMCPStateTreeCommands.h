#pragma once

#include "CoreMinimal.h"
#include "Json.h"

class UStateTree;
class UStateTreeEditorData;
class UStateTreeState;

/**
 * Handler class for StateTree-related MCP commands.
 *
 * Provides complete programmatic control over StateTree AI assets:
 * state hierarchy, tasks, evaluators, conditions, transitions,
 * property bindings, parameters, schema, theme, and compilation.
 *
 * All operations use the StateTree editor API (UStateTreeEditorData,
 * UStateTreeState) and Unreal's reflection system for maximum
 * type compatibility.
 */
class UNREALMCPBRIDGE_API FEpicUnrealMCPStateTreeCommands
{
public:
	FEpicUnrealMCPStateTreeCommands();

	TSharedPtr<FJsonObject> HandleCommand(
		const FString& CommandType,
		const TSharedPtr<FJsonObject>& Params);

private:
	// ---- Read/Query (StateTreeReadOps.cpp) ----
	TSharedPtr<FJsonObject> HandleGetStateTreeInfo(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetStateTreeStates(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetStateTreeState(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetStateTreeEvaluators(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetStateTreeGlobalTasks(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetStateTreeParameters(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetStateTreeNode(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetStateTreeBindings(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetStateTreeTransitionTargets(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetStateTreeFullInfo(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSearchStateTreeNodes(const TSharedPtr<FJsonObject>& Params);

	// ---- Create/Add (StateTreeCreateOps.cpp) ----
	TSharedPtr<FJsonObject> HandleCreateStateTree(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAddStateTreeState(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAddStateTreeTask(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAddStateTreeEvaluator(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAddStateTreeGlobalTask(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAddStateTreeCondition(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAddStateTreeTransition(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAddStateTreeParameter(const TSharedPtr<FJsonObject>& Params);

	// ---- Modify (StateTreeModifyOps.cpp) ----
	TSharedPtr<FJsonObject> HandleSetStateTreeStateProperty(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetStateTreeNodeProperty(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetStateTreeTransitionProperty(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetStateTreeSchema(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAddStateTreeBinding(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetStateTreeColor(const TSharedPtr<FJsonObject>& Params);

	// ---- Remove (StateTreeRemoveOps.cpp) ----
	TSharedPtr<FJsonObject> HandleRemoveStateTreeState(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleRemoveStateTreeNode(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleRemoveStateTreeTransition(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleRemoveStateTreeBinding(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleRemoveStateTreeParameter(const TSharedPtr<FJsonObject>& Params);

	// ---- Compile/Discovery (StateTreeCompileOps.cpp) ----
	TSharedPtr<FJsonObject> HandleCompileStateTree(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleListStateTreeNodeTypes(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleListStateTreeEnumValues(const TSharedPtr<FJsonObject>& Params);
};
