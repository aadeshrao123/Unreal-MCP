#include "Commands/EpicUnrealMCPStateTreeCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"

FEpicUnrealMCPStateTreeCommands::FEpicUnrealMCPStateTreeCommands()
{
}

TSharedPtr<FJsonObject> FEpicUnrealMCPStateTreeCommands::HandleCommand(
	const FString& CommandType,
	const TSharedPtr<FJsonObject>& Params)
{
	// ---- Read/Query ----
	if (CommandType == TEXT("get_statetree_info"))
	{
		return HandleGetStateTreeInfo(Params);
	}
	else if (CommandType == TEXT("get_statetree_states"))
	{
		return HandleGetStateTreeStates(Params);
	}
	else if (CommandType == TEXT("get_statetree_state"))
	{
		return HandleGetStateTreeState(Params);
	}
	else if (CommandType == TEXT("get_statetree_evaluators"))
	{
		return HandleGetStateTreeEvaluators(Params);
	}
	else if (CommandType == TEXT("get_statetree_global_tasks"))
	{
		return HandleGetStateTreeGlobalTasks(Params);
	}
	else if (CommandType == TEXT("get_statetree_parameters"))
	{
		return HandleGetStateTreeParameters(Params);
	}
	else if (CommandType == TEXT("get_statetree_node"))
	{
		return HandleGetStateTreeNode(Params);
	}
	else if (CommandType == TEXT("get_statetree_bindings"))
	{
		return HandleGetStateTreeBindings(Params);
	}
	else if (CommandType == TEXT("get_statetree_transition_targets"))
	{
		return HandleGetStateTreeTransitionTargets(Params);
	}

	// ---- Create/Add ----
	else if (CommandType == TEXT("create_statetree"))
	{
		return HandleCreateStateTree(Params);
	}
	else if (CommandType == TEXT("add_statetree_state"))
	{
		return HandleAddStateTreeState(Params);
	}
	else if (CommandType == TEXT("add_statetree_task"))
	{
		return HandleAddStateTreeTask(Params);
	}
	else if (CommandType == TEXT("add_statetree_evaluator"))
	{
		return HandleAddStateTreeEvaluator(Params);
	}
	else if (CommandType == TEXT("add_statetree_global_task"))
	{
		return HandleAddStateTreeGlobalTask(Params);
	}
	else if (CommandType == TEXT("add_statetree_condition"))
	{
		return HandleAddStateTreeCondition(Params);
	}
	else if (CommandType == TEXT("add_statetree_transition"))
	{
		return HandleAddStateTreeTransition(Params);
	}
	else if (CommandType == TEXT("add_statetree_parameter"))
	{
		return HandleAddStateTreeParameter(Params);
	}

	// ---- Modify ----
	else if (CommandType == TEXT("set_statetree_state_property"))
	{
		return HandleSetStateTreeStateProperty(Params);
	}
	else if (CommandType == TEXT("set_statetree_node_property"))
	{
		return HandleSetStateTreeNodeProperty(Params);
	}
	else if (CommandType == TEXT("set_statetree_transition_property"))
	{
		return HandleSetStateTreeTransitionProperty(Params);
	}
	else if (CommandType == TEXT("set_statetree_schema"))
	{
		return HandleSetStateTreeSchema(Params);
	}
	else if (CommandType == TEXT("add_statetree_binding"))
	{
		return HandleAddStateTreeBinding(Params);
	}
	else if (CommandType == TEXT("set_statetree_color"))
	{
		return HandleSetStateTreeColor(Params);
	}

	// ---- Remove ----
	else if (CommandType == TEXT("remove_statetree_state"))
	{
		return HandleRemoveStateTreeState(Params);
	}
	else if (CommandType == TEXT("remove_statetree_node"))
	{
		return HandleRemoveStateTreeNode(Params);
	}
	else if (CommandType == TEXT("remove_statetree_transition"))
	{
		return HandleRemoveStateTreeTransition(Params);
	}
	else if (CommandType == TEXT("remove_statetree_binding"))
	{
		return HandleRemoveStateTreeBinding(Params);
	}
	else if (CommandType == TEXT("remove_statetree_parameter"))
	{
		return HandleRemoveStateTreeParameter(Params);
	}

	// ---- Compile/Discovery ----
	else if (CommandType == TEXT("compile_statetree"))
	{
		return HandleCompileStateTree(Params);
	}
	else if (CommandType == TEXT("list_statetree_node_types"))
	{
		return HandleListStateTreeNodeTypes(Params);
	}
	else if (CommandType == TEXT("list_statetree_enum_values"))
	{
		return HandleListStateTreeEnumValues(Params);
	}

	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
		FString::Printf(TEXT("Unknown StateTree command: %s"), *CommandType));
}
