#pragma once

#include "CoreMinimal.h"
#include "Json.h"

class UNiagaraSystem;
class UNiagaraEmitter;
class UNiagaraComponent;
class ANiagaraActor;
class UNiagaraNodeFunctionCall;
class UNiagaraGraph;
struct FNiagaraEmitterHandle;
struct FVersionedNiagaraEmitterData;

/**
 * Handler class for Niagara-related MCP commands.
 *
 * Provides complete programmatic control over Niagara particle systems:
 * system creation, emitter management, module stacks, parameters,
 * renderers, curves, scratch pads, events, and runtime spawning.
 *
 * All operations use direct Niagara C++ APIs and NiagaraEditor stack utilities.
 */
class UNREALMCPBRIDGE_API FEpicUnrealMCPNiagaraCommands
{
public:
	FEpicUnrealMCPNiagaraCommands();

	TSharedPtr<FJsonObject> HandleCommand(
		const FString& CommandType,
		const TSharedPtr<FJsonObject>& Params);

private:
	// ---- System Management (NiagaraSystemOps.cpp) ----
	TSharedPtr<FJsonObject> HandleCreateNiagaraSystem(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetNiagaraSystemInfo(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleListNiagaraSystems(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleDeleteNiagaraSystem(const TSharedPtr<FJsonObject>& Params);

	// ---- Emitter Management (NiagaraEmitterOps.cpp) ----
	TSharedPtr<FJsonObject> HandleGetNiagaraEmitters(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAddNiagaraEmitter(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleRemoveNiagaraEmitter(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetNiagaraEmitterProperty(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleDuplicateNiagaraEmitter(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleReorderNiagaraEmitter(const TSharedPtr<FJsonObject>& Params);

	// ---- Module Stack (NiagaraModuleOps.cpp) ----
	TSharedPtr<FJsonObject> HandleGetNiagaraModules(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAddNiagaraModule(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleRemoveNiagaraModule(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetNiagaraModuleEnabled(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleReorderNiagaraModule(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetNiagaraModuleInputs(const TSharedPtr<FJsonObject>& Params);

	// ---- Module Inputs (NiagaraModuleOps.cpp) ----
	TSharedPtr<FJsonObject> HandleSetNiagaraModuleInput(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetNiagaraDynamicInput(const TSharedPtr<FJsonObject>& Params);

	// ---- Rapid Iteration Parameters (NiagaraModuleOps.cpp) ----
	TSharedPtr<FJsonObject> HandleGetNiagaraRapidIterationParameters(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetNiagaraRapidIterationParameter(const TSharedPtr<FJsonObject>& Params);

	// ---- Curves (NiagaraCurveOps.cpp) ----
	TSharedPtr<FJsonObject> HandleSetNiagaraCurve(const TSharedPtr<FJsonObject>& Params);

	// ---- User Parameters (NiagaraParameterOps.cpp) ----
	TSharedPtr<FJsonObject> HandleGetNiagaraUserParameters(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAddNiagaraUserParameter(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetNiagaraUserParameter(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleRemoveNiagaraUserParameter(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleLinkNiagaraParameter(const TSharedPtr<FJsonObject>& Params);

	// ---- Renderers (NiagaraRendererOps.cpp) ----
	TSharedPtr<FJsonObject> HandleAddNiagaraRenderer(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleRemoveNiagaraRenderer(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetNiagaraRendererInfo(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetNiagaraRendererProperty(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetNiagaraRendererBinding(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetNiagaraRendererProperties(const TSharedPtr<FJsonObject>& Params);

	// ---- System Properties (NiagaraSystemOps.cpp) ----
	TSharedPtr<FJsonObject> HandleSetNiagaraSystemProperty(const TSharedPtr<FJsonObject>& Params);

	// ---- Diagnostics & Timeline (NiagaraDiagnosticsOps.cpp) ----
	TSharedPtr<FJsonObject> HandleGetNiagaraSystemErrors(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetNiagaraParticleStats(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetNiagaraPlaybackRange(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetNiagaraPlaybackRange(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetNiagaraModuleVersions(const TSharedPtr<FJsonObject>& Params);

	// ---- Scratch Pad & Custom Modules (NiagaraScratchPadOps.cpp) ----
	TSharedPtr<FJsonObject> HandleCreateNiagaraScratchPadModule(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleDuplicateNiagaraScratchPadModule(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleDeleteNiagaraScratchPadModule(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleRenameNiagaraScratchPadModule(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleListNiagaraScratchPadModules(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetNiagaraScratchPadHlsl(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleCreateNiagaraModuleAsset(const TSharedPtr<FJsonObject>& Params);

	// ---- Custom HLSL pin management (NiagaraScratchPadOps.cpp) ----
	TSharedPtr<FJsonObject> HandleAddNiagaraCustomHlslInput(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAddNiagaraCustomHlslOutput(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleRenameNiagaraCustomHlslPin(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleRemoveNiagaraCustomHlslPin(const TSharedPtr<FJsonObject>& Params);

	// ---- Node discovery & schema introspection (NiagaraNodeDiscovery.cpp) ----
	TSharedPtr<FJsonObject> HandleListNiagaraNodeTypes(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetNiagaraNodeTypeInfo(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSearchNiagaraFunctions(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetNiagaraSchemaActions(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleDescribeNiagaraType(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetNiagaraDataInterfaceSchema(const TSharedPtr<FJsonObject>& Params);

	// ---- Pin operations & parameter enumeration (NiagaraPinOps.cpp) ----
	TSharedPtr<FJsonObject> HandleListNiagaraAvailableParameters(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAddNiagaraMapGetPin(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAddNiagaraMapSetPin(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAddNiagaraNodePin(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleRenameNiagaraNodePin(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleRemoveNiagaraNodePin(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleConnectNiagaraPins(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleDisconnectNiagaraPins(const TSharedPtr<FJsonObject>& Params);

	// ---- Events & Simulation Stages (NiagaraEventOps.cpp) ----
	TSharedPtr<FJsonObject> HandleAddNiagaraEventHandler(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAddNiagaraSimulationStage(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetNiagaraEventHandlers(const TSharedPtr<FJsonObject>& Params);

	// ---- Runtime & Level (NiagaraRuntimeOps.cpp) ----
	TSharedPtr<FJsonObject> HandleSpawnNiagaraEffect(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleControlNiagaraEffect(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAddNiagaraComponent(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetNiagaraActors(const TSharedPtr<FJsonObject>& Params);

	// ---- Discovery (NiagaraDiscovery.cpp) ----
	TSharedPtr<FJsonObject> HandleListNiagaraModules(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleListNiagaraEmitterTemplates(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleListNiagaraDataInterfaces(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleListNiagaraParameterTypes(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetNiagaraEmitterAttributes(const TSharedPtr<FJsonObject>& Params);

	// ---- Compilation (NiagaraSystemOps.cpp) ----
	TSharedPtr<FJsonObject> HandleCompileNiagaraSystem(const TSharedPtr<FJsonObject>& Params);
};
