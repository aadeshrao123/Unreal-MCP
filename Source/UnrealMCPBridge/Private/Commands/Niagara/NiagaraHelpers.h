#pragma once

#include "CoreMinimal.h"
#include "Json.h"
#include "NiagaraCommon.h"

class UNiagaraSystem;
class UNiagaraEmitter;
class UNiagaraComponent;
class ANiagaraActor;
class UNiagaraGraph;
class UNiagaraNodeOutput;
class UNiagaraNodeFunctionCall;
struct FNiagaraEmitterHandle;
struct FVersionedNiagaraEmitterData;

/**
 * Shared helper functions for all Niagara MCP command files.
 * Provides asset loading, emitter/module lookup, enum conversion,
 * graph access, and compilation utilities.
 */
namespace NiagaraHelpers
{
	// ---- Asset Loading ----

	/** Load a UNiagaraSystem from a content path. Returns null and sets OutError on failure. */
	UNiagaraSystem* LoadNiagaraSystem(const FString& AssetPath, FString& OutError);

	/** Load a UNiagaraEmitter from a content path. */
	UNiagaraEmitter* LoadNiagaraEmitter(const FString& AssetPath, FString& OutError);

	// ---- Emitter Handle Lookup ----

	/**
	 * Find an emitter handle by display name or index notation ("Emitter[0]").
	 * Name matching is case-insensitive.
	 * @param OutIndex  The index of the found handle in the system's array.
	 */
	FNiagaraEmitterHandle* FindEmitterHandle(
		UNiagaraSystem* System,
		const FString& EmitterName,
		int32& OutIndex,
		FString& OutError);

	/** Get versioned emitter data from a handle. */
	FVersionedNiagaraEmitterData* GetEmitterData(FNiagaraEmitterHandle* Handle);

	// ---- Script Usage Enum <-> String ----

	ENiagaraScriptUsage ParseScriptUsage(const FString& Usage, bool& bOutSuccess);
	FString ScriptUsageToString(ENiagaraScriptUsage Usage);

	// ---- Graph Access ----

	/** Get the Niagara graph for a given script usage on an emitter. */
	UNiagaraGraph* GetGraphForUsage(
		FVersionedNiagaraEmitterData* EmitterData,
		ENiagaraScriptUsage Usage);

	/** Get the output node for a specific script usage within a graph. */
	UNiagaraNodeOutput* GetOutputNodeForUsage(
		UNiagaraGraph* Graph,
		ENiagaraScriptUsage Usage);

	// ---- Module Node Lookup ----

	/** Find a module function call node by name in a specific script usage graph. */
	UNiagaraNodeFunctionCall* FindModuleNode(
		UNiagaraGraph* Graph,
		ENiagaraScriptUsage Usage,
		const FString& ModuleName,
		FString& OutError);

	// ---- Actor Lookup ----

	/** Find a Niagara actor in the current editor level by name (case-insensitive). */
	ANiagaraActor* FindNiagaraActorByName(const FString& Name, FString& OutError);

	// ---- Compilation & Editor Sync ----

	/**
	 * Synchronize overview graph, mark dirty, request compile, and post edit change.
	 * Call after any structural modification to a system.
	 */
	void CompileAndSync(UNiagaraSystem* System, bool bForce = false);

	// ---- Scratch Pad transient-proxy helpers ----

	/**
	 * Locate a scratch pad script on a system by case-insensitive name.
	 * Returns the asset-owned UNiagaraScript in System->ScratchPadScripts.
	 */
	UNiagaraScript* FindScratchPadScript(UNiagaraSystem* System, const FString& ModuleName);

	/**
	 * Collect BOTH the asset script and (if the Niagara editor is open) the
	 * transient edit-copy script that the user actually sees in the scratch pad.
	 * Mirrors how the material MCP resolves UMaterial/PreviewMaterial pairs.
	 *
	 * When the system is not open in an editor, returns a single-element list
	 * with just the asset script. When it IS open, returns [original, edit_copy]
	 * so callers can apply their mutation to both and keep them in sync without
	 * requiring the user to close/reopen the effect.
	 */
	void GetScratchPadScriptPair(
		UNiagaraSystem* System,
		const FString& ModuleName,
		TArray<UNiagaraScript*>& OutScripts);

	/**
	 * Notify the Niagara editor (if open) that a scratch pad script graph changed
	 * — refreshes the view model so the change appears live. Safe to call even
	 * when no editor is open.
	 */
	void NotifyScratchPadScriptChanged(UNiagaraSystem* System, const FString& ModuleName);

	// ---- JSON Serialization ----

	/** Serialize an emitter handle to a JSON object for API responses. */
	TSharedPtr<FJsonObject> EmitterHandleToJson(
		const FNiagaraEmitterHandle& Handle,
		int32 Index);

	/** Serialize a module function call node to JSON. */
	TSharedPtr<FJsonObject> ModuleNodeToJson(
		UNiagaraNodeFunctionCall* Node,
		int32 Index,
		ENiagaraScriptUsage Usage,
		bool bIncludeInputs);
}
