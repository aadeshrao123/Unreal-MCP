#include "Commands/EpicUnrealMCPNiagaraCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "NiagaraHelpers.h"

#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraGraph.h"

#if WITH_EDITORONLY_DATA
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraEditorUtilities.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/ARFilter.h"
#endif


// ---------------------------------------------------------------------------
// HandleFindNiagaraScratchPadUsage — Gap #2 reverse lookup
//
// "Which stack module(s) reference this scratch pad script?" — scans every
// emitter's emitter/particle script graphs looking for UNiagaraNodeFunctionCall
// nodes whose FunctionScript points to the named scratch pad. Reports each
// site as {emitter, script_usage, module_name, is_dynamic_input}.
//
// Params:
//   system_path   — required
//   module_name   — required, scratch pad module name to hunt for
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleFindNiagaraScratchPadUsage(
    const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
    FString SystemPath, ModuleName;
    if (!Params->TryGetStringField(TEXT("system_path"), SystemPath) ||
        !Params->TryGetStringField(TEXT("module_name"), ModuleName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            TEXT("Missing 'system_path' or 'module_name'"));
    }

    FString Error;
    UNiagaraSystem* System = NiagaraHelpers::LoadNiagaraSystem(SystemPath, Error);
    if (!System) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);

    UNiagaraScript* TargetScript = NiagaraHelpers::FindScratchPadScript(System, ModuleName);
    if (!TargetScript)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Scratch pad module '%s' not found on system"), *ModuleName));
    }
    const bool bTargetIsDynamicInput =
        TargetScript->GetUsage() == ENiagaraScriptUsage::DynamicInput;

    TArray<TSharedPtr<FJsonValue>> Sites;

    auto ScanGraph = [&](UNiagaraGraph* G, const FString& EmitterName, const FString& UsageLabel)
    {
        if (!G) return;
        for (UEdGraphNode* Node : G->Nodes)
        {
            UNiagaraNodeFunctionCall* Call = Cast<UNiagaraNodeFunctionCall>(Node);
            if (!Call || Call->FunctionScript != TargetScript) continue;

            auto Site = MakeShared<FJsonObject>();
            Site->SetStringField(TEXT("emitter"), EmitterName);
            Site->SetStringField(TEXT("script_usage"), UsageLabel);
            Site->SetStringField(TEXT("function_name"), Call->GetFunctionName());
            Site->SetStringField(TEXT("node_id"),
                Call->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));
            Site->SetBoolField(TEXT("is_dynamic_input"), bTargetIsDynamicInput);
            Sites.Add(MakeShared<FJsonValueObject>(Site));
        }
    };

    // System scripts
    if (UNiagaraScript* SysSpawn = System->GetSystemSpawnScript())
    {
        if (UNiagaraScriptSource* S = Cast<UNiagaraScriptSource>(SysSpawn->GetLatestSource()))
        {
            ScanGraph(S->NodeGraph, TEXT("<system>"), TEXT("system_spawn"));
        }
    }
    if (UNiagaraScript* SysUpdate = System->GetSystemUpdateScript())
    {
        if (UNiagaraScriptSource* S = Cast<UNiagaraScriptSource>(SysUpdate->GetLatestSource()))
        {
            ScanGraph(S->NodeGraph, TEXT("<system>"), TEXT("system_update"));
        }
    }

    // Every emitter's scripts
    static const ENiagaraScriptUsage Usages[] = {
        ENiagaraScriptUsage::EmitterSpawnScript,
        ENiagaraScriptUsage::EmitterUpdateScript,
        ENiagaraScriptUsage::ParticleSpawnScript,
        ENiagaraScriptUsage::ParticleUpdateScript,
    };

    for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
    {
        FVersionedNiagaraEmitterData* Data =
            const_cast<FNiagaraEmitterHandle&>(Handle).GetEmitterData();
        if (!Data) continue;
        for (ENiagaraScriptUsage U : Usages)
        {
            if (UNiagaraScript* S = Data->GetScript(U, FGuid()))
            {
                if (UNiagaraScriptSource* Src = Cast<UNiagaraScriptSource>(S->GetLatestSource()))
                {
                    ScanGraph(Src->NodeGraph,
                        Handle.GetName().ToString(),
                        NiagaraHelpers::ScriptUsageToString(U));
                }
            }
        }
    }

    auto R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetStringField(TEXT("target_module"), ModuleName);
    R->SetBoolField(TEXT("target_is_dynamic_input"), bTargetIsDynamicInput);
    R->SetNumberField(TEXT("usage_count"), Sites.Num());
    R->SetArrayField(TEXT("sites"), Sites);
    return R;
#else
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only command"));
#endif
}


// ---------------------------------------------------------------------------
// HandleResolveNiagaraBuiltInDynamicInput — Gap #4 helper
//
// Dynamically finds a built-in dynamic-input script's asset path via
// AssetRegistry filtered on Usage=DynamicInput. Replaces hardcoded engine
// paths (/Niagara/Modules/DynamicInputs/UniformRangedFloat.*) that aren't
// stable across UE versions or plugin layouts.
//
// Params:
//   name_filter   — substring to match against asset name (e.g. "UniformRanged")
//   type_filter   — optional substring to match against package path context
//   exact_name    — optional, return only asset whose AssetName equals this
// Returns top matches, best-first.
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleResolveNiagaraBuiltInDynamicInput(
    const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
    FString NameFilter, ExactName;
    Params->TryGetStringField(TEXT("name_filter"), NameFilter);
    Params->TryGetStringField(TEXT("exact_name"), ExactName);
    int32 MaxResults = 20;
    Params->TryGetNumberField(TEXT("max_results"), MaxResults);

    FNiagaraEditorUtilities::FGetFilteredScriptAssetsOptions Opts;
    Opts.ScriptUsageToInclude = ENiagaraScriptUsage::DynamicInput;
    Opts.bIncludeDeprecatedScripts = false;

    TArray<FAssetData> DIAssets;
    FNiagaraEditorUtilities::GetFilteredScriptAssets(Opts, DIAssets);

    TArray<TSharedPtr<FJsonValue>> Matches;
    for (const FAssetData& Asset : DIAssets)
    {
        if (Matches.Num() >= MaxResults) break;
        const FString AssetName = Asset.AssetName.ToString();
        if (!ExactName.IsEmpty())
        {
            if (!AssetName.Equals(ExactName, ESearchCase::IgnoreCase)) continue;
        }
        else if (!NameFilter.IsEmpty())
        {
            if (!AssetName.Contains(NameFilter, ESearchCase::IgnoreCase)) continue;
        }

        auto Entry = MakeShared<FJsonObject>();
        Entry->SetStringField(TEXT("name"), AssetName);
        Entry->SetStringField(TEXT("path"), Asset.GetObjectPathString());
        Entry->SetStringField(TEXT("package"), Asset.PackageName.ToString());
        Matches.Add(MakeShared<FJsonValueObject>(Entry));
    }

    auto R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetNumberField(TEXT("total_scanned"), DIAssets.Num());
    R->SetNumberField(TEXT("match_count"), Matches.Num());
    R->SetArrayField(TEXT("matches"), Matches);
    return R;
#else
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only command"));
#endif
}
