#include "Commands/EpicUnrealMCPNiagaraCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "NiagaraHelpers.h"

#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraRendererProperties.h"
#include "NiagaraCommon.h"
#include "EditorAssetLibrary.h"

#if WITH_EDITORONLY_DATA
#include "NiagaraSystemEditorData.h"
#endif

// ---------------------------------------------------------------------------
// HandleGetNiagaraEmitters
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleGetNiagaraEmitters(
	const TSharedPtr<FJsonObject>& Params)
{
	FString SystemPath;
	if (!Params->TryGetStringField(TEXT("system_path"), SystemPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'system_path' parameter"));
	}

	FString Error;
	UNiagaraSystem* System = NiagaraHelpers::LoadNiagaraSystem(SystemPath, Error);
	if (!System)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	const TArray<FNiagaraEmitterHandle>& Handles = System->GetEmitterHandles();
	TArray<TSharedPtr<FJsonValue>> EmitterArr;
	for (int32 i = 0; i < Handles.Num(); ++i)
	{
		EmitterArr.Add(MakeShared<FJsonValueObject>(
			NiagaraHelpers::EmitterHandleToJson(Handles[i], i)));
	}

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetArrayField(TEXT("emitters"), EmitterArr);
	Result->SetNumberField(TEXT("count"), EmitterArr.Num());
	return Result;
}

// ---------------------------------------------------------------------------
// HandleAddNiagaraEmitter
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleAddNiagaraEmitter(
	const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
	FString SystemPath;
	if (!Params->TryGetStringField(TEXT("system_path"), SystemPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'system_path' parameter"));
	}

	FString Error;
	UNiagaraSystem* System = NiagaraHelpers::LoadNiagaraSystem(SystemPath, Error);
	if (!System)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	FString EmitterPath;
	FString EmitterName;
	FString TemplateName;
	Params->TryGetStringField(TEXT("emitter_path"), EmitterPath);
	Params->TryGetStringField(TEXT("emitter_name"), EmitterName);
	Params->TryGetStringField(TEXT("template"), TemplateName);

	UNiagaraEmitter* SourceEmitter = nullptr;

	// Resolve emitter source
	if (!EmitterPath.IsEmpty())
	{
		SourceEmitter = Cast<UNiagaraEmitter>(UEditorAssetLibrary::LoadAsset(EmitterPath));
		if (!SourceEmitter)
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Emitter not found: %s"), *EmitterPath));
		}
	}
	else if (!TemplateName.IsEmpty())
	{
		SourceEmitter = Cast<UNiagaraEmitter>(UEditorAssetLibrary::LoadAsset(TemplateName));
		if (!SourceEmitter)
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Template emitter not found: %s"), *TemplateName));
		}
	}
	else
	{
		// Default: try engine's Simple Sprite Burst template
		FString DefaultPath = TEXT("/Niagara/DefaultAssets/Templates/Emitters/Simple Sprite Burst.Simple Sprite Burst");
		SourceEmitter = Cast<UNiagaraEmitter>(UEditorAssetLibrary::LoadAsset(DefaultPath));
		if (!SourceEmitter)
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				TEXT("No emitter_path/template provided and default emitter not found. "
				     "Provide an 'emitter_path' to a UNiagaraEmitter asset."));
		}
	}

	// Generate unique name
	FName HandleName = EmitterName.IsEmpty()
		? SourceEmitter->GetFName()
		: FName(*EmitterName);

	TSet<FName> ExistingNames;
	for (const FNiagaraEmitterHandle& H : System->GetEmitterHandles())
	{
		ExistingNames.Add(H.GetName());
	}
	HandleName = FNiagaraUtilities::GetUniqueName(HandleName, ExistingNames);

	System->Modify();
	FGuid Version = SourceEmitter->GetExposedVersion().VersionGuid;
	FNiagaraEmitterHandle NewHandle = System->AddEmitterHandle(*SourceEmitter, HandleName, Version);

	// Critical: sync editor data so Niagara editor UI sees the new emitter
	NiagaraHelpers::CompileAndSync(System);
	UEditorAssetLibrary::SaveAsset(SystemPath);

	// Find the new handle to report its info
	const TArray<FNiagaraEmitterHandle>& Handles = System->GetEmitterHandles();
	for (int32 i = 0; i < Handles.Num(); ++i)
	{
		if (Handles[i].GetId() == NewHandle.GetId())
		{
			auto Result = MakeShared<FJsonObject>();
			Result->SetBoolField(TEXT("success"), true);
			Result->SetStringField(TEXT("emitter_name"), Handles[i].GetName().ToString());
			Result->SetStringField(TEXT("emitter_id"), Handles[i].GetId().ToString());
			Result->SetNumberField(TEXT("emitter_index"), i);
			Result->SetNumberField(TEXT("total_emitters"), Handles.Num());
			return Result;
		}
	}

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("emitter_id"), NewHandle.GetId().ToString());
	return Result;
#else
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Requires editor mode"));
#endif
}

// ---------------------------------------------------------------------------
// HandleRemoveNiagaraEmitter
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleRemoveNiagaraEmitter(
	const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
	FString SystemPath;
	if (!Params->TryGetStringField(TEXT("system_path"), SystemPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'system_path' parameter"));
	}

	FString EmitterName;
	if (!Params->TryGetStringField(TEXT("emitter_name"), EmitterName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'emitter_name' parameter"));
	}

	FString Error;
	UNiagaraSystem* System = NiagaraHelpers::LoadNiagaraSystem(SystemPath, Error);
	if (!System)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	int32 FoundIdx;
	FNiagaraEmitterHandle* Handle = NiagaraHelpers::FindEmitterHandle(System, EmitterName, FoundIdx, Error);
	if (!Handle)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	FGuid RemovedId = Handle->GetId();
	TSet<FGuid> IdsToRemove;
	IdsToRemove.Add(RemovedId);

	System->Modify();
	System->RemoveEmitterHandlesById(IdsToRemove);

	NiagaraHelpers::CompileAndSync(System);
	UEditorAssetLibrary::SaveAsset(SystemPath);

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("removed_emitter"), EmitterName);
	Result->SetNumberField(TEXT("remaining_emitters"), System->GetNumEmitters());
	return Result;
#else
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Requires editor mode"));
#endif
}

// ---------------------------------------------------------------------------
// HandleSetNiagaraEmitterProperty
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleSetNiagaraEmitterProperty(
	const TSharedPtr<FJsonObject>& Params)
{
	FString SystemPath, EmitterName, Property;
	if (!Params->TryGetStringField(TEXT("system_path"), SystemPath) ||
		!Params->TryGetStringField(TEXT("emitter_name"), EmitterName) ||
		!Params->TryGetStringField(TEXT("property"), Property))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Missing required parameters: system_path, emitter_name, property"));
	}

	FString Error;
	UNiagaraSystem* System = NiagaraHelpers::LoadNiagaraSystem(SystemPath, Error);
	if (!System)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	int32 Idx;
	FNiagaraEmitterHandle* Handle = NiagaraHelpers::FindEmitterHandle(System, EmitterName, Idx, Error);
	if (!Handle)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	FVersionedNiagaraEmitterData* Data = NiagaraHelpers::GetEmitterData(Handle);
	if (!Data)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No emitter data found"));
	}

	FString LowerProp = Property.ToLower();
	FString NewValueStr;

	if (LowerProp == TEXT("enabled"))
	{
		bool bVal = false;
		if (!Params->TryGetBoolField(TEXT("value"), bVal))
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("'value' must be bool for 'enabled'"));
		}
		Handle->SetIsEnabled(bVal, *System, true);
		NewValueStr = bVal ? TEXT("true") : TEXT("false");
	}
	else if (LowerProp == TEXT("sim_target"))
	{
		FString Val;
		if (!Params->TryGetStringField(TEXT("value"), Val))
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("'value' must be string for 'sim_target'"));
		}
		FString Lower = Val.ToLower();
		if (Lower == TEXT("gpu"))
		{
			Data->SimTarget = ENiagaraSimTarget::GPUComputeSim;
		}
		else if (Lower == TEXT("cpu"))
		{
			Data->SimTarget = ENiagaraSimTarget::CPUSim;
		}
		else
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("sim_target must be 'cpu' or 'gpu'"));
		}
		NewValueStr = Lower;
	}
	else if (LowerProp == TEXT("local_space"))
	{
		bool bVal = false;
		if (!Params->TryGetBoolField(TEXT("value"), bVal))
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("'value' must be bool for 'local_space'"));
		}
		Data->bLocalSpace = bVal;
		NewValueStr = bVal ? TEXT("true") : TEXT("false");
	}
	else if (LowerProp == TEXT("determinism"))
	{
		bool bVal = false;
		if (!Params->TryGetBoolField(TEXT("value"), bVal))
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("'value' must be bool for 'determinism'"));
		}
		Data->bDeterminism = bVal;
		NewValueStr = bVal ? TEXT("true") : TEXT("false");
	}
	else if (LowerProp == TEXT("bounds_mode"))
	{
		FString Val;
		if (!Params->TryGetStringField(TEXT("value"), Val))
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("'value' must be string for 'bounds_mode'"));
		}
		FString Lower = Val.ToLower();
		if (Lower == TEXT("dynamic"))
		{
			Data->CalculateBoundsMode = ENiagaraEmitterCalculateBoundMode::Dynamic;
		}
		else if (Lower == TEXT("fixed"))
		{
			Data->CalculateBoundsMode = ENiagaraEmitterCalculateBoundMode::Fixed;
		}
		else if (Lower == TEXT("programmable"))
		{
			Data->CalculateBoundsMode = ENiagaraEmitterCalculateBoundMode::Programmable;
		}
		else
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				TEXT("bounds_mode must be 'dynamic', 'fixed', or 'programmable'"));
		}
		NewValueStr = Lower;
	}
	else if (LowerProp == TEXT("max_particles"))
	{
		double NumVal = 0;
		if (!Params->TryGetNumberField(TEXT("value"), NumVal))
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				TEXT("'value' must be a number for 'max_particles'"));
		}
		int32 MaxParticles = FMath::Max(0, static_cast<int32>(NumVal));

		// Switch allocation mode to manual and set the pre-allocation count
		Data->AllocationMode = EParticleAllocationMode::ManualEstimate;
		Data->PreAllocationCount = MaxParticles;
		NewValueStr = FString::FromInt(MaxParticles);
	}
	else
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Unsupported property: '%s'. Supported: enabled, sim_target, "
				"local_space, determinism, bounds_mode, max_particles"), *Property));
	}

	System->RequestCompile(false);
	System->PostEditChange();
	UEditorAssetLibrary::SaveAsset(SystemPath);

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("emitter_name"), EmitterName);
	Result->SetStringField(TEXT("property"), Property);
	Result->SetStringField(TEXT("value"), NewValueStr);
	return Result;
}

// ---------------------------------------------------------------------------
// HandleDuplicateNiagaraEmitter
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleDuplicateNiagaraEmitter(
	const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
	FString SystemPath, EmitterName, NewName;
	if (!Params->TryGetStringField(TEXT("system_path"), SystemPath) ||
		!Params->TryGetStringField(TEXT("emitter_name"), EmitterName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Missing required parameters: system_path, emitter_name"));
	}
	Params->TryGetStringField(TEXT("new_name"), NewName);

	FString Error;
	UNiagaraSystem* System = NiagaraHelpers::LoadNiagaraSystem(SystemPath, Error);
	if (!System)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	int32 SourceIdx;
	FNiagaraEmitterHandle* SourceHandle = NiagaraHelpers::FindEmitterHandle(
		System, EmitterName, SourceIdx, Error);
	if (!SourceHandle)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	FName DupName;
	if (NewName.IsEmpty())
	{
		TSet<FName> Existing;
		for (const FNiagaraEmitterHandle& H : System->GetEmitterHandles())
		{
			Existing.Add(H.GetName());
		}
		DupName = FNiagaraUtilities::GetUniqueName(SourceHandle->GetName(), Existing);
	}
	else
	{
		DupName = FName(*NewName);
	}

	System->Modify();
	FNiagaraEmitterHandle NewHandle = System->DuplicateEmitterHandle(*SourceHandle, DupName);

	NiagaraHelpers::CompileAndSync(System);
	UEditorAssetLibrary::SaveAsset(SystemPath);

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("new_emitter_name"), NewHandle.GetName().ToString());
	Result->SetStringField(TEXT("new_emitter_id"), NewHandle.GetId().ToString());
	Result->SetNumberField(TEXT("total_emitters"), System->GetNumEmitters());
	return Result;
#else
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Requires editor mode"));
#endif
}

// ---------------------------------------------------------------------------
// HandleReorderNiagaraEmitter
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleReorderNiagaraEmitter(
	const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
	FString SystemPath, EmitterName;
	double NewIdxD = 0;
	if (!Params->TryGetStringField(TEXT("system_path"), SystemPath) ||
		!Params->TryGetStringField(TEXT("emitter_name"), EmitterName) ||
		!Params->TryGetNumberField(TEXT("new_index"), NewIdxD))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Missing required parameters: system_path, emitter_name, new_index"));
	}
	int32 NewIndex = (int32)NewIdxD;

	FString Error;
	UNiagaraSystem* System = NiagaraHelpers::LoadNiagaraSystem(SystemPath, Error);
	if (!System)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	int32 CurrentIdx;
	FNiagaraEmitterHandle* Handle = NiagaraHelpers::FindEmitterHandle(
		System, EmitterName, CurrentIdx, Error);
	if (!Handle)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	TArray<FNiagaraEmitterHandle>& Handles = System->GetEmitterHandles();
	if (NewIndex < 0 || NewIndex >= Handles.Num())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("new_index %d out of range [0, %d)"), NewIndex, Handles.Num()));
	}

	if (CurrentIdx != NewIndex)
	{
		System->Modify();
		FNiagaraEmitterHandle Moved = Handles[CurrentIdx];
		Handles.RemoveAt(CurrentIdx);
		Handles.Insert(Moved, NewIndex);
		NiagaraHelpers::CompileAndSync(System);
		UEditorAssetLibrary::SaveAsset(SystemPath);
	}

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("emitter_name"), EmitterName);
	Result->SetNumberField(TEXT("old_index"), CurrentIdx);
	Result->SetNumberField(TEXT("new_index"), NewIndex);
	return Result;
#else
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Requires editor mode"));
#endif
}
