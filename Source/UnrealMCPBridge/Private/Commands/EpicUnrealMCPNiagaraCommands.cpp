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
	else if (CommandType == TEXT("set_niagara_scratch_pad_hlsl"))
	{
		return HandleSetNiagaraScratchPadHlsl(Params);
	}
	else if (CommandType == TEXT("create_niagara_module_asset"))
	{
		return HandleCreateNiagaraModuleAsset(Params);
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
