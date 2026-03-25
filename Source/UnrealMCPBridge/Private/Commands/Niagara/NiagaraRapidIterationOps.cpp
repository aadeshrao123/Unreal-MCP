#include "Commands/EpicUnrealMCPNiagaraCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "NiagaraHelpers.h"

#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraScript.h"
#include "NiagaraCommon.h"
#include "NiagaraTypes.h"
#include "NiagaraParameterStore.h"
#include "EditorAssetLibrary.h"

// ---------------------------------------------------------------------------
// Helper: Get UNiagaraScript for a given script usage from emitter data
// ---------------------------------------------------------------------------

static UNiagaraScript* GetScriptForUsageRI(
	FVersionedNiagaraEmitterData* EmitterData,
	ENiagaraScriptUsage Usage)
{
	if (!EmitterData)
	{
		return nullptr;
	}

	switch (Usage)
	{
	case ENiagaraScriptUsage::EmitterSpawnScript:
		return EmitterData->EmitterSpawnScriptProps.Script;
	case ENiagaraScriptUsage::EmitterUpdateScript:
		return EmitterData->EmitterUpdateScriptProps.Script;
	case ENiagaraScriptUsage::ParticleSpawnScript:
		return EmitterData->SpawnScriptProps.Script;
	case ENiagaraScriptUsage::ParticleUpdateScript:
		return EmitterData->UpdateScriptProps.Script;
	default:
		return nullptr;
	}
}

// ---------------------------------------------------------------------------
// Helper: Serialize a rapid iteration parameter value to JSON
// ---------------------------------------------------------------------------

static void SerializeRIValue(
	const FNiagaraParameterStore& Store,
	const FNiagaraVariableWithOffset& Var,
	const TSharedPtr<FJsonObject>& OutObj)
{
	FString TypeName = Var.GetType().GetName();
	int32 Offset = Store.IndexOf(Var);

	if (Offset == INDEX_NONE)
	{
		OutObj->SetStringField(TEXT("value"), TEXT("(not found)"));
		return;
	}

	const uint8* DataPtr = Store.GetParameterData(Offset, Var.GetType().GetSize());
	if (!DataPtr)
	{
		OutObj->SetStringField(TEXT("value"), TEXT("(null)"));
		return;
	}

	if (TypeName.Contains(TEXT("Float")) || TypeName == TEXT("float"))
	{
		float Val = *reinterpret_cast<const float*>(DataPtr);
		OutObj->SetNumberField(TEXT("value"), Val);
		OutObj->SetStringField(TEXT("value_type"), TEXT("float"));
	}
	else if (TypeName.Contains(TEXT("Int32")) || TypeName == TEXT("int32"))
	{
		int32 Val = *reinterpret_cast<const int32*>(DataPtr);
		OutObj->SetNumberField(TEXT("value"), Val);
		OutObj->SetStringField(TEXT("value_type"), TEXT("int"));
	}
	else if (TypeName.Contains(TEXT("Bool")) || TypeName == TEXT("bool"))
	{
		int32 BoolVal = *reinterpret_cast<const int32*>(DataPtr);
		OutObj->SetBoolField(TEXT("value"), BoolVal != 0);
		OutObj->SetStringField(TEXT("value_type"), TEXT("bool"));
	}
	else if (TypeName.Contains(TEXT("Vector3f")) || TypeName.Contains(TEXT("Position")))
	{
		const FVector3f* Vec = reinterpret_cast<const FVector3f*>(DataPtr);
		auto VObj = MakeShared<FJsonObject>();
		VObj->SetNumberField(TEXT("x"), Vec->X);
		VObj->SetNumberField(TEXT("y"), Vec->Y);
		VObj->SetNumberField(TEXT("z"), Vec->Z);
		OutObj->SetObjectField(TEXT("value"), VObj);
		OutObj->SetStringField(TEXT("value_type"), TEXT("vector"));
	}
	else if (TypeName.Contains(TEXT("Vector2f")))
	{
		const FVector2f* Vec = reinterpret_cast<const FVector2f*>(DataPtr);
		auto VObj = MakeShared<FJsonObject>();
		VObj->SetNumberField(TEXT("x"), Vec->X);
		VObj->SetNumberField(TEXT("y"), Vec->Y);
		OutObj->SetObjectField(TEXT("value"), VObj);
		OutObj->SetStringField(TEXT("value_type"), TEXT("vector2"));
	}
	else if (TypeName.Contains(TEXT("Vector4f")))
	{
		const FVector4f* Vec = reinterpret_cast<const FVector4f*>(DataPtr);
		auto VObj = MakeShared<FJsonObject>();
		VObj->SetNumberField(TEXT("x"), Vec->X);
		VObj->SetNumberField(TEXT("y"), Vec->Y);
		VObj->SetNumberField(TEXT("z"), Vec->Z);
		VObj->SetNumberField(TEXT("w"), Vec->W);
		OutObj->SetObjectField(TEXT("value"), VObj);
		OutObj->SetStringField(TEXT("value_type"), TEXT("vector4"));
	}
	else if (TypeName.Contains(TEXT("LinearColor")))
	{
		const FLinearColor* Color = reinterpret_cast<const FLinearColor*>(DataPtr);
		auto CObj = MakeShared<FJsonObject>();
		CObj->SetNumberField(TEXT("r"), Color->R);
		CObj->SetNumberField(TEXT("g"), Color->G);
		CObj->SetNumberField(TEXT("b"), Color->B);
		CObj->SetNumberField(TEXT("a"), Color->A);
		OutObj->SetObjectField(TEXT("value"), CObj);
		OutObj->SetStringField(TEXT("value_type"), TEXT("color"));
	}
	else if (TypeName.Contains(TEXT("Quat")))
	{
		const FQuat4f* Quat = reinterpret_cast<const FQuat4f*>(DataPtr);
		auto QObj = MakeShared<FJsonObject>();
		QObj->SetNumberField(TEXT("x"), Quat->X);
		QObj->SetNumberField(TEXT("y"), Quat->Y);
		QObj->SetNumberField(TEXT("z"), Quat->Z);
		QObj->SetNumberField(TEXT("w"), Quat->W);
		OutObj->SetObjectField(TEXT("value"), QObj);
		OutObj->SetStringField(TEXT("value_type"), TEXT("quat"));
	}
	else
	{
		OutObj->SetStringField(TEXT("value"), TEXT("(unsupported type)"));
		OutObj->SetStringField(TEXT("value_type"), TypeName);
	}
}

// ---------------------------------------------------------------------------
// HandleGetNiagaraRapidIterationParameters
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleGetNiagaraRapidIterationParameters(
	const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
	FString SystemPath, EmitterName;
	if (!Params->TryGetStringField(TEXT("system_path"), SystemPath) ||
		!Params->TryGetStringField(TEXT("emitter_name"), EmitterName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Missing required: system_path, emitter_name"));
	}

	FString ScriptUsageStr = TEXT("all");
	Params->TryGetStringField(TEXT("script_usage"), ScriptUsageStr);

	FString Filter;
	Params->TryGetStringField(TEXT("filter"), Filter);

	FString Error;
	UNiagaraSystem* System = NiagaraHelpers::LoadNiagaraSystem(SystemPath, Error);
	if (!System)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	int32 EmitterIdx;
	FNiagaraEmitterHandle* Handle = NiagaraHelpers::FindEmitterHandle(
		System, EmitterName, EmitterIdx, Error);
	if (!Handle)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	FVersionedNiagaraEmitterData* Data = NiagaraHelpers::GetEmitterData(Handle);
	if (!Data)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No emitter data"));
	}

	// Determine which usages to query
	TArray<ENiagaraScriptUsage> Usages;
	if (ScriptUsageStr.Equals(TEXT("all"), ESearchCase::IgnoreCase))
	{
		Usages.Add(ENiagaraScriptUsage::EmitterSpawnScript);
		Usages.Add(ENiagaraScriptUsage::EmitterUpdateScript);
		Usages.Add(ENiagaraScriptUsage::ParticleSpawnScript);
		Usages.Add(ENiagaraScriptUsage::ParticleUpdateScript);
	}
	else
	{
		bool bOk = false;
		ENiagaraScriptUsage Usage = NiagaraHelpers::ParseScriptUsage(ScriptUsageStr, bOk);
		if (!bOk)
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Invalid script_usage: '%s'"), *ScriptUsageStr));
		}
		Usages.Add(Usage);
	}

	TArray<TSharedPtr<FJsonValue>> ParamsArr;

	for (ENiagaraScriptUsage Usage : Usages)
	{
		UNiagaraScript* Script = GetScriptForUsageRI(Data, Usage);
		if (!Script)
		{
			continue;
		}

		const FNiagaraParameterStore& Store = Script->RapidIterationParameters;
		TArrayView<const FNiagaraVariableWithOffset> Vars = Store.ReadParameterVariables();

		for (const FNiagaraVariableWithOffset& Var : Vars)
		{
			FString ParamName = Var.GetName().ToString();

			// Apply filter
			if (!Filter.IsEmpty() &&
				!ParamName.Contains(Filter, ESearchCase::IgnoreCase))
			{
				continue;
			}

			auto ParamObj = MakeShared<FJsonObject>();
			ParamObj->SetStringField(TEXT("name"), ParamName);
			ParamObj->SetStringField(TEXT("type"), Var.GetType().GetName());
			ParamObj->SetStringField(TEXT("script_usage"),
				NiagaraHelpers::ScriptUsageToString(Usage));

			// Parse module_name and input_name from RI param name
			// Format: "Constants.EmitterName.ModuleName.InputName"
			TArray<FString> Parts;
			ParamName.ParseIntoArray(Parts, TEXT("."));
			if (Parts.Num() >= 4)
			{
				ParamObj->SetStringField(TEXT("module_name"), Parts[2]);
				FString InputName;
				for (int32 i = 3; i < Parts.Num(); ++i)
				{
					if (i > 3)
					{
						InputName += TEXT(".");
					}
					InputName += Parts[i];
				}
				ParamObj->SetStringField(TEXT("input_name"), InputName);
			}

			SerializeRIValue(Store, Var, ParamObj);
			ParamsArr.Add(MakeShared<FJsonValueObject>(ParamObj));
		}
	}

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("emitter_name"), EmitterName);
	Result->SetArrayField(TEXT("parameters"), ParamsArr);
	Result->SetNumberField(TEXT("count"), ParamsArr.Num());
	return Result;
#else
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only operation"));
#endif
}

// ---------------------------------------------------------------------------
// HandleSetNiagaraRapidIterationParameter
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleSetNiagaraRapidIterationParameter(
	const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
	FString SystemPath, EmitterName, ModuleName, InputName, ScriptUsageStr;
	if (!Params->TryGetStringField(TEXT("system_path"), SystemPath) ||
		!Params->TryGetStringField(TEXT("emitter_name"), EmitterName) ||
		!Params->TryGetStringField(TEXT("module_name"), ModuleName) ||
		!Params->TryGetStringField(TEXT("input_name"), InputName) ||
		!Params->TryGetStringField(TEXT("script_usage"), ScriptUsageStr))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Missing required: system_path, emitter_name, module_name, input_name, script_usage"));
	}

	bool bOk = false;
	ENiagaraScriptUsage Usage = NiagaraHelpers::ParseScriptUsage(ScriptUsageStr, bOk);
	if (!bOk)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Invalid script_usage"));
	}

	FString Error;
	UNiagaraSystem* System = NiagaraHelpers::LoadNiagaraSystem(SystemPath, Error);
	if (!System)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	int32 EmitterIdx;
	FNiagaraEmitterHandle* Handle = NiagaraHelpers::FindEmitterHandle(
		System, EmitterName, EmitterIdx, Error);
	if (!Handle)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	FVersionedNiagaraEmitterData* EmitterData = NiagaraHelpers::GetEmitterData(Handle);
	if (!EmitterData)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No emitter data"));
	}

	UNiagaraScript* Script = GetScriptForUsageRI(EmitterData, Usage);
	if (!Script)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("No script for usage: %s"), *ScriptUsageStr));
	}

	// Build the RI parameter name: "Constants.EmitterName.ModuleName.InputName"
	FString RIParamName = FString::Printf(TEXT("Constants.%s.%s.%s"),
		*EmitterName, *ModuleName, *InputName);

	// Find the parameter in the store
	FNiagaraParameterStore& Store = Script->RapidIterationParameters;
	TArrayView<const FNiagaraVariableWithOffset> AllVars = Store.ReadParameterVariables();

	// Try exact match first
	const FNiagaraVariableWithOffset* FoundVar = nullptr;
	for (const FNiagaraVariableWithOffset& Var : AllVars)
	{
		if (Var.GetName().ToString().Equals(RIParamName, ESearchCase::IgnoreCase))
		{
			FoundVar = &Var;
			break;
		}
	}

	// Partial match fallback — module name + input name anywhere in the param name
	if (!FoundVar)
	{
		for (const FNiagaraVariableWithOffset& Var : AllVars)
		{
			FString VarName = Var.GetName().ToString();
			if (VarName.Contains(ModuleName, ESearchCase::IgnoreCase) &&
				VarName.Contains(InputName, ESearchCase::IgnoreCase))
			{
				FoundVar = &Var;
				break;
			}
		}
	}

	if (!FoundVar)
	{
		// Provide helpful error with available params for this module
		TArray<FString> Available;
		for (const FNiagaraVariableWithOffset& Var : AllVars)
		{
			FString VarName = Var.GetName().ToString();
			if (VarName.Contains(ModuleName, ESearchCase::IgnoreCase))
			{
				Available.Add(VarName);
			}
		}

		FString AvailableStr = Available.Num() > 0
			? FString::Join(Available, TEXT("\n  "))
			: TEXT("(none found for this module)");

		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Parameter not found: '%s'\nAvailable for '%s':\n  %s"),
				*RIParamName, *ModuleName, *AvailableStr));
	}

	// Get the JSON value
	const TSharedPtr<FJsonValue>* ValueField = Params->Values.Find(TEXT("value"));
	if (!ValueField || !ValueField->IsValid())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'value' parameter"));
	}

	FNiagaraVariable MutableVar(FoundVar->GetType(), FoundVar->GetName());
	FString TypeName = FoundVar->GetType().GetName();
	FString ValueStr;
	bool bSuccess = false;

	Script->Modify();

	// Type-specific value setting
	if (TypeName.Contains(TEXT("Float")) || TypeName == TEXT("float"))
	{
		float Val = (float)(*ValueField)->AsNumber();
		bSuccess = Store.SetParameterValue<float>(Val, MutableVar, true);
		ValueStr = FString::SanitizeFloat(Val);
	}
	else if (TypeName.Contains(TEXT("Int32")) || TypeName == TEXT("int32"))
	{
		int32 Val = (int32)(*ValueField)->AsNumber();
		bSuccess = Store.SetParameterValue<int32>(Val, MutableVar, true);
		ValueStr = FString::FromInt(Val);
	}
	else if (TypeName.Contains(TEXT("Bool")) || TypeName == TEXT("bool"))
	{
		int32 BoolVal = (*ValueField)->AsBool() ? 1 : 0;
		bSuccess = Store.SetParameterValue<int32>(BoolVal, MutableVar, true);
		ValueStr = BoolVal ? TEXT("true") : TEXT("false");
	}
	else if (TypeName.Contains(TEXT("Vector3f")) || TypeName.Contains(TEXT("Position")))
	{
		FVector3f Vec(0, 0, 0);
		if ((*ValueField)->Type == EJson::Object)
		{
			auto Obj = (*ValueField)->AsObject();
			Vec.X = (float)Obj->GetNumberField(TEXT("x"));
			Vec.Y = (float)Obj->GetNumberField(TEXT("y"));
			Vec.Z = (float)Obj->GetNumberField(TEXT("z"));
		}
		else if ((*ValueField)->Type == EJson::Array)
		{
			const auto& Arr = (*ValueField)->AsArray();
			if (Arr.Num() >= 3)
			{
				Vec.X = (float)Arr[0]->AsNumber();
				Vec.Y = (float)Arr[1]->AsNumber();
				Vec.Z = (float)Arr[2]->AsNumber();
			}
		}
		bSuccess = Store.SetParameterValue<FVector3f>(Vec, MutableVar, true);
		ValueStr = FString::Printf(TEXT("(%f, %f, %f)"), Vec.X, Vec.Y, Vec.Z);
	}
	else if (TypeName.Contains(TEXT("Vector2f")))
	{
		FVector2f Vec(0, 0);
		if ((*ValueField)->Type == EJson::Object)
		{
			auto Obj = (*ValueField)->AsObject();
			Vec.X = (float)Obj->GetNumberField(TEXT("x"));
			Vec.Y = (float)Obj->GetNumberField(TEXT("y"));
		}
		else if ((*ValueField)->Type == EJson::Array)
		{
			const auto& Arr = (*ValueField)->AsArray();
			if (Arr.Num() >= 2)
			{
				Vec.X = (float)Arr[0]->AsNumber();
				Vec.Y = (float)Arr[1]->AsNumber();
			}
		}
		bSuccess = Store.SetParameterValue<FVector2f>(Vec, MutableVar, true);
		ValueStr = FString::Printf(TEXT("(%f, %f)"), Vec.X, Vec.Y);
	}
	else if (TypeName.Contains(TEXT("Vector4f")))
	{
		FVector4f Vec(0, 0, 0, 0);
		if ((*ValueField)->Type == EJson::Object)
		{
			auto Obj = (*ValueField)->AsObject();
			Vec.X = (float)Obj->GetNumberField(TEXT("x"));
			Vec.Y = (float)Obj->GetNumberField(TEXT("y"));
			Vec.Z = (float)Obj->GetNumberField(TEXT("z"));
			Vec.W = (float)Obj->GetNumberField(TEXT("w"));
		}
		else if ((*ValueField)->Type == EJson::Array)
		{
			const auto& Arr = (*ValueField)->AsArray();
			if (Arr.Num() >= 4)
			{
				Vec.X = (float)Arr[0]->AsNumber();
				Vec.Y = (float)Arr[1]->AsNumber();
				Vec.Z = (float)Arr[2]->AsNumber();
				Vec.W = (float)Arr[3]->AsNumber();
			}
		}
		bSuccess = Store.SetParameterValue<FVector4f>(Vec, MutableVar, true);
		ValueStr = FString::Printf(TEXT("(%f, %f, %f, %f)"), Vec.X, Vec.Y, Vec.Z, Vec.W);
	}
	else if (TypeName.Contains(TEXT("LinearColor")))
	{
		FLinearColor Color(0, 0, 0, 1);
		if ((*ValueField)->Type == EJson::Object)
		{
			auto Obj = (*ValueField)->AsObject();
			Color.R = (float)Obj->GetNumberField(TEXT("r"));
			Color.G = (float)Obj->GetNumberField(TEXT("g"));
			Color.B = (float)Obj->GetNumberField(TEXT("b"));
			double A = 1.0;
			Obj->TryGetNumberField(TEXT("a"), A);
			Color.A = (float)A;
		}
		else if ((*ValueField)->Type == EJson::Array)
		{
			const auto& Arr = (*ValueField)->AsArray();
			if (Arr.Num() >= 3)
			{
				Color.R = (float)Arr[0]->AsNumber();
				Color.G = (float)Arr[1]->AsNumber();
				Color.B = (float)Arr[2]->AsNumber();
				if (Arr.Num() >= 4)
				{
					Color.A = (float)Arr[3]->AsNumber();
				}
			}
		}
		bSuccess = Store.SetParameterValue<FLinearColor>(Color, MutableVar, true);
		ValueStr = FString::Printf(TEXT("(R=%f, G=%f, B=%f, A=%f)"),
			Color.R, Color.G, Color.B, Color.A);
	}
	else if (TypeName.Contains(TEXT("Quat")))
	{
		FQuat4f Quat(0, 0, 0, 1);
		if ((*ValueField)->Type == EJson::Object)
		{
			auto Obj = (*ValueField)->AsObject();
			Quat.X = (float)Obj->GetNumberField(TEXT("x"));
			Quat.Y = (float)Obj->GetNumberField(TEXT("y"));
			Quat.Z = (float)Obj->GetNumberField(TEXT("z"));
			Quat.W = (float)Obj->GetNumberField(TEXT("w"));
		}
		bSuccess = Store.SetParameterValue<FQuat4f>(Quat, MutableVar, true);
		ValueStr = FString::Printf(TEXT("(%f, %f, %f, %f)"),
			Quat.X, Quat.Y, Quat.Z, Quat.W);
	}
	else
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Unsupported parameter type: '%s'"), *TypeName));
	}

	if (!bSuccess)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Failed to set parameter value in the store"));
	}

	// Recompile to apply changes
	NiagaraHelpers::CompileAndSync(System);
	UEditorAssetLibrary::SaveAsset(SystemPath);

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("parameter_name"), FoundVar->GetName().ToString());
	Result->SetStringField(TEXT("module_name"), ModuleName);
	Result->SetStringField(TEXT("input_name"), InputName);
	Result->SetStringField(TEXT("value"), ValueStr);
	Result->SetStringField(TEXT("type"), TypeName);
	Result->SetStringField(TEXT("script_usage"), ScriptUsageStr);
	return Result;
#else
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only operation"));
#endif
}
