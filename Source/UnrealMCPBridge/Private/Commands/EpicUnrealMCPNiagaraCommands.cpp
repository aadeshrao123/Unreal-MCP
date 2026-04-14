#include "Commands/EpicUnrealMCPNiagaraCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"

FEpicUnrealMCPNiagaraCommands::FEpicUnrealMCPNiagaraCommands()
{
}

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleCommand(
	const FString& CommandType,
	const TSharedPtr<FJsonObject>& Params)
{
	// ---- System Management ----
	if (CommandType == TEXT("create_niagara_system"))
	{
		return HandleCreateNiagaraSystem(Params);
	}
	else if (CommandType == TEXT("get_niagara_system_info"))
	{
		return HandleGetNiagaraSystemInfo(Params);
	}
	else if (CommandType == TEXT("list_niagara_systems"))
	{
		return HandleListNiagaraSystems(Params);
	}
	else if (CommandType == TEXT("delete_niagara_system"))
	{
		return HandleDeleteNiagaraSystem(Params);
	}
	else if (CommandType == TEXT("compile_niagara_system"))
	{
		return HandleCompileNiagaraSystem(Params);
	}

	// ---- Emitter Management ----
	else if (CommandType == TEXT("get_niagara_emitters"))
	{
		return HandleGetNiagaraEmitters(Params);
	}
	else if (CommandType == TEXT("add_niagara_emitter"))
	{
		return HandleAddNiagaraEmitter(Params);
	}
	else if (CommandType == TEXT("remove_niagara_emitter"))
	{
		return HandleRemoveNiagaraEmitter(Params);
	}
	else if (CommandType == TEXT("set_niagara_emitter_property"))
	{
		return HandleSetNiagaraEmitterProperty(Params);
	}
	else if (CommandType == TEXT("duplicate_niagara_emitter"))
	{
		return HandleDuplicateNiagaraEmitter(Params);
	}
	else if (CommandType == TEXT("reorder_niagara_emitter"))
	{
		return HandleReorderNiagaraEmitter(Params);
	}

	// ---- Module Stack ----
	else if (CommandType == TEXT("get_niagara_modules"))
	{
		return HandleGetNiagaraModules(Params);
	}
	else if (CommandType == TEXT("add_niagara_module"))
	{
		return HandleAddNiagaraModule(Params);
	}
	else if (CommandType == TEXT("remove_niagara_module"))
	{
		return HandleRemoveNiagaraModule(Params);
	}
	else if (CommandType == TEXT("set_niagara_module_enabled"))
	{
		return HandleSetNiagaraModuleEnabled(Params);
	}
	else if (CommandType == TEXT("reorder_niagara_module"))
	{
		return HandleReorderNiagaraModule(Params);
	}
	else if (CommandType == TEXT("get_niagara_module_inputs"))
	{
		return HandleGetNiagaraModuleInputs(Params);
	}

	// ---- Module Inputs ----
	else if (CommandType == TEXT("set_niagara_module_input"))
	{
		return HandleSetNiagaraModuleInput(Params);
	}
	else if (CommandType == TEXT("set_niagara_dynamic_input"))
	{
		return HandleSetNiagaraDynamicInput(Params);
	}
	else if (CommandType == TEXT("set_niagara_curve"))
	{
		return HandleSetNiagaraCurve(Params);
	}

	// ---- Rapid Iteration Parameters ----
	else if (CommandType == TEXT("get_niagara_rapid_iteration_parameters"))
	{
		return HandleGetNiagaraRapidIterationParameters(Params);
	}
	else if (CommandType == TEXT("set_niagara_rapid_iteration_parameter"))
	{
		return HandleSetNiagaraRapidIterationParameter(Params);
	}

	// ---- User Parameters ----
	else if (CommandType == TEXT("get_niagara_user_parameters"))
	{
		return HandleGetNiagaraUserParameters(Params);
	}
	else if (CommandType == TEXT("add_niagara_user_parameter"))
	{
		return HandleAddNiagaraUserParameter(Params);
	}
	else if (CommandType == TEXT("set_niagara_user_parameter"))
	{
		return HandleSetNiagaraUserParameter(Params);
	}
	else if (CommandType == TEXT("remove_niagara_user_parameter"))
	{
		return HandleRemoveNiagaraUserParameter(Params);
	}
	else if (CommandType == TEXT("link_niagara_parameter"))
	{
		return HandleLinkNiagaraParameter(Params);
	}

	// ---- Renderers ----
	else if (CommandType == TEXT("add_niagara_renderer"))
	{
		return HandleAddNiagaraRenderer(Params);
	}
	else if (CommandType == TEXT("remove_niagara_renderer"))
	{
		return HandleRemoveNiagaraRenderer(Params);
	}
	else if (CommandType == TEXT("get_niagara_renderer_info"))
	{
		return HandleGetNiagaraRendererInfo(Params);
	}
	else if (CommandType == TEXT("set_niagara_renderer_property"))
	{
		return HandleSetNiagaraRendererProperty(Params);
	}
	else if (CommandType == TEXT("get_niagara_renderer_properties"))
	{
		return HandleGetNiagaraRendererProperties(Params);
	}
	else if (CommandType == TEXT("set_niagara_system_property"))
	{
		return HandleSetNiagaraSystemProperty(Params);
	}
	else if (CommandType == TEXT("set_niagara_renderer_binding"))
	{
		return HandleSetNiagaraRendererBinding(Params);
	}

	// ---- Diagnostics & Timeline ----
	else if (CommandType == TEXT("get_niagara_system_errors"))
	{
		return HandleGetNiagaraSystemErrors(Params);
	}
	else if (CommandType == TEXT("get_niagara_particle_stats"))
	{
		return HandleGetNiagaraParticleStats(Params);
	}
	else if (CommandType == TEXT("set_niagara_playback_range"))
	{
		return HandleSetNiagaraPlaybackRange(Params);
	}
	else if (CommandType == TEXT("get_niagara_playback_range"))
	{
		return HandleGetNiagaraPlaybackRange(Params);
	}
	else if (CommandType == TEXT("get_niagara_module_versions"))
	{
		return HandleGetNiagaraModuleVersions(Params);
	}

	// ---- Scratch Pad & Custom Modules ----
	else if (CommandType == TEXT("create_niagara_scratch_pad_module"))
	{
		return HandleCreateNiagaraScratchPadModule(Params);
	}
	else if (CommandType == TEXT("duplicate_niagara_scratch_pad_module"))
	{
		return HandleDuplicateNiagaraScratchPadModule(Params);
	}
	else if (CommandType == TEXT("delete_niagara_scratch_pad_module"))
	{
		return HandleDeleteNiagaraScratchPadModule(Params);
	}
	else if (CommandType == TEXT("rename_niagara_scratch_pad_module"))
	{
		return HandleRenameNiagaraScratchPadModule(Params);
	}
	else if (CommandType == TEXT("list_niagara_scratch_pad_modules"))
	{
		return HandleListNiagaraScratchPadModules(Params);
	}
	else if (CommandType == TEXT("set_niagara_scratch_pad_hlsl"))
	{
		return HandleSetNiagaraScratchPadHlsl(Params);
	}
	else if (CommandType == TEXT("create_niagara_module_asset"))
	{
		return HandleCreateNiagaraModuleAsset(Params);
	}

	// ---- Custom HLSL pin management ----
	else if (CommandType == TEXT("add_niagara_custom_hlsl_input"))
	{
		return HandleAddNiagaraCustomHlslInput(Params);
	}
	else if (CommandType == TEXT("add_niagara_custom_hlsl_output"))
	{
		return HandleAddNiagaraCustomHlslOutput(Params);
	}
	else if (CommandType == TEXT("rename_niagara_custom_hlsl_pin"))
	{
		return HandleRenameNiagaraCustomHlslPin(Params);
	}
	else if (CommandType == TEXT("remove_niagara_custom_hlsl_pin"))
	{
		return HandleRemoveNiagaraCustomHlslPin(Params);
	}

	// ---- Graph introspection (scratch pad / script graphs) ----
	else if (CommandType == TEXT("get_niagara_graph_nodes"))
	{
		return HandleGetNiagaraGraphNodes(Params);
	}
	else if (CommandType == TEXT("get_niagara_node_info"))
	{
		return HandleGetNiagaraNodeInfo(Params);
	}
	else if (CommandType == TEXT("trace_niagara_connection"))
	{
		return HandleTraceNiagaraConnection(Params);
	}
	else if (CommandType == TEXT("validate_niagara_graph"))
	{
		return HandleValidateNiagaraGraph(Params);
	}
	else if (CommandType == TEXT("apply_niagara_scratch_pad"))
	{
		return HandleApplyNiagaraScratchPad(Params);
	}
	else if (CommandType == TEXT("apply_and_save_niagara_scratch_pad"))
	{
		return HandleApplyAndSaveNiagaraScratchPad(Params);
	}
	else if (CommandType == TEXT("get_niagara_script_properties"))
	{
		return HandleGetNiagaraScriptProperties(Params);
	}
	else if (CommandType == TEXT("set_niagara_script_properties"))
	{
		return HandleSetNiagaraScriptProperties(Params);
	}
	else if (CommandType == TEXT("list_niagara_script_parameters"))
	{
		return HandleListNiagaraScriptParameters(Params);
	}
	else if (CommandType == TEXT("add_niagara_script_parameter"))
	{
		return HandleAddNiagaraScriptParameter(Params);
	}
	else if (CommandType == TEXT("remove_niagara_script_parameter"))
	{
		return HandleRemoveNiagaraScriptParameter(Params);
	}
	else if (CommandType == TEXT("rename_niagara_script_parameter"))
	{
		return HandleRenameNiagaraScriptParameter(Params);
	}
	else if (CommandType == TEXT("add_niagara_graph_node"))
	{
		return HandleAddNiagaraGraphNode(Params);
	}
	else if (CommandType == TEXT("delete_niagara_graph_node"))
	{
		return HandleDeleteNiagaraGraphNode(Params);
	}
	else if (CommandType == TEXT("get_niagara_module_input_binding"))
	{
		return HandleGetNiagaraModuleInputBinding(Params);
	}
	else if (CommandType == TEXT("clear_niagara_module_input"))
	{
		return HandleClearNiagaraModuleInput(Params);
	}
	else if (CommandType == TEXT("list_niagara_input_source_menu"))
	{
		return HandleListNiagaraInputSourceMenu(Params);
	}
	else if (CommandType == TEXT("find_niagara_scratch_pad_usage"))
	{
		return HandleFindNiagaraScratchPadUsage(Params);
	}
	else if (CommandType == TEXT("resolve_niagara_built_in_dynamic_input"))
	{
		return HandleResolveNiagaraBuiltInDynamicInput(Params);
	}
	else if (CommandType == TEXT("list_niagara_data_interface_functions"))
	{
		return HandleListNiagaraDataInterfaceFunctions(Params);
	}

	// ---- Node discovery & schema introspection ----
	else if (CommandType == TEXT("list_niagara_node_types"))
	{
		return HandleListNiagaraNodeTypes(Params);
	}
	else if (CommandType == TEXT("get_niagara_node_type_info"))
	{
		return HandleGetNiagaraNodeTypeInfo(Params);
	}
	else if (CommandType == TEXT("search_niagara_functions"))
	{
		return HandleSearchNiagaraFunctions(Params);
	}
	else if (CommandType == TEXT("get_niagara_schema_actions"))
	{
		return HandleGetNiagaraSchemaActions(Params);
	}
	else if (CommandType == TEXT("describe_niagara_type"))
	{
		return HandleDescribeNiagaraType(Params);
	}
	else if (CommandType == TEXT("get_niagara_data_interface_schema"))
	{
		return HandleGetNiagaraDataInterfaceSchema(Params);
	}

	// ---- Pin operations & parameter enumeration ----
	else if (CommandType == TEXT("list_niagara_available_parameters"))
	{
		return HandleListNiagaraAvailableParameters(Params);
	}
	else if (CommandType == TEXT("add_niagara_map_get_pin"))
	{
		return HandleAddNiagaraMapGetPin(Params);
	}
	else if (CommandType == TEXT("add_niagara_map_set_pin"))
	{
		return HandleAddNiagaraMapSetPin(Params);
	}
	else if (CommandType == TEXT("add_niagara_node_pin"))
	{
		return HandleAddNiagaraNodePin(Params);
	}
	else if (CommandType == TEXT("rename_niagara_node_pin"))
	{
		return HandleRenameNiagaraNodePin(Params);
	}
	else if (CommandType == TEXT("remove_niagara_node_pin"))
	{
		return HandleRemoveNiagaraNodePin(Params);
	}
	else if (CommandType == TEXT("connect_niagara_pins"))
	{
		return HandleConnectNiagaraPins(Params);
	}
	else if (CommandType == TEXT("disconnect_niagara_pins"))
	{
		return HandleDisconnectNiagaraPins(Params);
	}

	// ---- Events & Simulation Stages ----
	else if (CommandType == TEXT("add_niagara_event_handler"))
	{
		return HandleAddNiagaraEventHandler(Params);
	}
	else if (CommandType == TEXT("add_niagara_simulation_stage"))
	{
		return HandleAddNiagaraSimulationStage(Params);
	}
	else if (CommandType == TEXT("get_niagara_event_handlers"))
	{
		return HandleGetNiagaraEventHandlers(Params);
	}

	// ---- Runtime & Level ----
	else if (CommandType == TEXT("spawn_niagara_effect"))
	{
		return HandleSpawnNiagaraEffect(Params);
	}
	else if (CommandType == TEXT("control_niagara_effect"))
	{
		return HandleControlNiagaraEffect(Params);
	}
	else if (CommandType == TEXT("add_niagara_component"))
	{
		return HandleAddNiagaraComponent(Params);
	}
	else if (CommandType == TEXT("get_niagara_actors"))
	{
		return HandleGetNiagaraActors(Params);
	}

	// ---- Discovery ----
	else if (CommandType == TEXT("list_niagara_modules"))
	{
		return HandleListNiagaraModules(Params);
	}
	else if (CommandType == TEXT("list_niagara_emitter_templates"))
	{
		return HandleListNiagaraEmitterTemplates(Params);
	}
	else if (CommandType == TEXT("list_niagara_data_interfaces"))
	{
		return HandleListNiagaraDataInterfaces(Params);
	}
	else if (CommandType == TEXT("list_niagara_parameter_types"))
	{
		return HandleListNiagaraParameterTypes(Params);
	}
	else if (CommandType == TEXT("get_niagara_emitter_attributes"))
	{
		return HandleGetNiagaraEmitterAttributes(Params);
	}

	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
		FString::Printf(TEXT("Unknown Niagara command: %s"), *CommandType));
}
