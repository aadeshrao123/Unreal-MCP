#include "Commands/EpicUnrealMCPMaterialCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"

/**
 * Material command dispatch.
 *
 * Implementation is split across files in Private/Commands/Material/:
 *   MaterialEditorUtils.cpp  — Editor context helpers (shared by all)
 *   MaterialHelpers.cpp      — Enum resolvers, property setters, HLSL node config
 *   MaterialSerializer.cpp   — Expression-to-JSON serialization
 *   MaterialCreation.cpp     — create_material, create_material_instance, build_material_graph,
 *                              set_material_properties, add_material_comments, recompile_material
 *   MaterialGraphRead.cpp    — get_material_info, get_material_graph_nodes, get_material_expression_info,
 *                              get_material_property_connections, get_material_errors
 *   MaterialNodeMutations.cpp — add/set/move/duplicate/delete/connect/layout expressions
 *   MaterialInstance.cpp     — get/set material instance parameters, list_material_expression_types
 *   MaterialFunctionOps.cpp  — create/inspect/build material functions, add/set inputs & outputs
 */

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

FEpicUnrealMCPMaterialCommands::FEpicUnrealMCPMaterialCommands()
{
}

// ---------------------------------------------------------------------------
// Command Dispatch
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleCommand(
	const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
	// ---- Creation & Bulk ----
	if      (CommandType == TEXT("create_material"))                  return HandleCreateMaterial(Params);
	else if (CommandType == TEXT("create_material_instance"))         return HandleCreateMaterialInstance(Params);
	else if (CommandType == TEXT("build_material_graph"))             return HandleBuildMaterialGraph(Params);
	else if (CommandType == TEXT("set_material_properties"))          return HandleSetMaterialProperties(Params);
	else if (CommandType == TEXT("add_material_comments"))            return HandleAddMaterialComments(Params);
	else if (CommandType == TEXT("recompile_material"))               return HandleRecompileMaterial(Params);

	// ---- Read ----
	else if (CommandType == TEXT("get_material_info"))                return HandleGetMaterialInfo(Params);
	else if (CommandType == TEXT("get_material_graph_nodes"))         return HandleGetMaterialGraphNodes(Params);
	else if (CommandType == TEXT("get_material_expression_info"))     return HandleGetMaterialExpressionInfo(Params);
	else if (CommandType == TEXT("get_material_property_connections")) return HandleGetMaterialPropertyConnections(Params);
	else if (CommandType == TEXT("get_material_errors"))              return HandleGetMaterialErrors(Params);
	else if (CommandType == TEXT("get_expression_type_info"))       return HandleGetExpressionTypeInfo(Params);
	else if (CommandType == TEXT("get_available_material_pins"))   return HandleGetAvailableMaterialPins(Params);
	else if (CommandType == TEXT("validate_material_graph"))        return HandleValidateMaterialGraph(Params);
	else if (CommandType == TEXT("trace_material_connection"))      return HandleTraceMaterialConnection(Params);

	// ---- Node Mutations ----
	else if (CommandType == TEXT("add_material_expression"))          return HandleAddMaterialExpression(Params);
	else if (CommandType == TEXT("set_material_expression_property")) return HandleSetMaterialExpressionProperty(Params);
	else if (CommandType == TEXT("move_material_expression"))         return HandleMoveMaterialExpression(Params);
	else if (CommandType == TEXT("duplicate_material_expression"))    return HandleDuplicateMaterialExpression(Params);
	else if (CommandType == TEXT("delete_material_expression"))       return HandleDeleteMaterialExpression(Params);
	else if (CommandType == TEXT("connect_material_expressions"))     return HandleConnectMaterialExpressions(Params);
	else if (CommandType == TEXT("layout_material_expressions"))      return HandleLayoutMaterialExpressions(Params);
	else if (CommandType == TEXT("disconnect_material_expression")) return HandleDisconnectMaterialExpression(Params);
	else if (CommandType == TEXT("cleanup_material_graph"))         return HandleCleanupMaterialGraph(Params);

	// ---- Material Instance ----
	else if (CommandType == TEXT("get_material_instance_parameters")) return HandleGetMaterialInstanceParameters(Params);
	else if (CommandType == TEXT("set_material_instance_parameter"))  return HandleSetMaterialInstanceParameter(Params);

	// ---- Discovery ----
	else if (CommandType == TEXT("list_material_expression_types"))   return HandleListMaterialExpressionTypes(Params);
	else if (CommandType == TEXT("search_material_functions"))      return HandleSearchMaterialFunctions(Params);

	// ---- Material Function ----
	else if (CommandType == TEXT("create_material_function"))        return HandleCreateMaterialFunction(Params);
	else if (CommandType == TEXT("get_material_function_info"))      return HandleGetMaterialFunctionInfo(Params);
	else if (CommandType == TEXT("build_material_function_graph"))   return HandleBuildMaterialFunctionGraph(Params);
	else if (CommandType == TEXT("add_material_function_input"))     return HandleAddMaterialFunctionInput(Params);
	else if (CommandType == TEXT("add_material_function_output"))    return HandleAddMaterialFunctionOutput(Params);
	else if (CommandType == TEXT("set_material_function_input"))     return HandleSetMaterialFunctionInput(Params);
	else if (CommandType == TEXT("set_material_function_output"))    return HandleSetMaterialFunctionOutput(Params);
	else if (CommandType == TEXT("validate_material_function"))     return HandleValidateMaterialFunction(Params);
	else if (CommandType == TEXT("cleanup_material_function"))      return HandleCleanupMaterialFunction(Params);

	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
		FString::Printf(TEXT("Unknown material command: %s"), *CommandType));
}
