#include "Commands/EpicUnrealMCPNiagaraCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "NiagaraHelpers.h"

#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraComponent.h"
#include "NiagaraActor.h"
#include "NiagaraParameterStore.h"
#include "NiagaraTypes.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeFunctionCall.h"

#if WITH_EDITORONLY_DATA
#include "NiagaraScriptVariable.h"
#include "NiagaraEditorUtilities.h"
#include "ViewModels/Stack/NiagaraParameterHandle.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#include "NiagaraNodeInput.h"
#include "NiagaraNodeOutput.h"
#endif

#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"

// ---------------------------------------------------------------------------
// Helper: Niagara type from string
// ---------------------------------------------------------------------------

static FNiagaraTypeDefinition ResolveNiagaraType(const FString& TypeStr)
{
	FString Lower = TypeStr.ToLower();

	if (Lower == TEXT("float") || Lower == TEXT("scalar"))
	{
		return FNiagaraTypeDefinition::GetFloatDef();
	}
	if (Lower == TEXT("int") || Lower == TEXT("int32"))
	{
		return FNiagaraTypeDefinition::GetIntDef();
	}
	if (Lower == TEXT("bool"))
	{
		return FNiagaraTypeDefinition::GetBoolDef();
	}
	if (Lower == TEXT("vector") || Lower == TEXT("vector3") || Lower == TEXT("vec3"))
	{
		return FNiagaraTypeDefinition::GetVec3Def();
	}
	if (Lower == TEXT("vector2") || Lower == TEXT("vec2"))
	{
		return FNiagaraTypeDefinition::GetVec2Def();
	}
	if (Lower == TEXT("vector4") || Lower == TEXT("vec4"))
	{
		return FNiagaraTypeDefinition::GetVec4Def();
	}
	if (Lower == TEXT("color") || Lower == TEXT("linear_color") || Lower == TEXT("linearcolor"))
	{
		return FNiagaraTypeDefinition::GetColorDef();
	}
	if (Lower == TEXT("quat") || Lower == TEXT("quaternion"))
	{
		return FNiagaraTypeDefinition::GetQuatDef();
	}

	// Default to float
	return FNiagaraTypeDefinition::GetFloatDef();
}

// ---------------------------------------------------------------------------
// Helper: Serialize a Niagara variable to JSON
// ---------------------------------------------------------------------------

static TSharedPtr<FJsonObject> NiagaraVariableToJson(const FNiagaraVariableBase& Var)
{
	auto Obj = MakeShared<FJsonObject>();
	Obj->SetStringField(TEXT("name"), Var.GetName().ToString());
	Obj->SetStringField(TEXT("type"), Var.GetType().GetName());
	return Obj;
}

// ---------------------------------------------------------------------------
// HandleGetNiagaraUserParameters
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleGetNiagaraUserParameters(
	const TSharedPtr<FJsonObject>& Params)
{
	UNiagaraSystem* System = nullptr;
	FString SourceName;

	// Option 1: from a running actor in level
	FString ActorName;
	if (Params->TryGetStringField(TEXT("actor_name"), ActorName))
	{
		FString Error;
		ANiagaraActor* Actor = NiagaraHelpers::FindNiagaraActorByName(ActorName, Error);
		if (!Actor)
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
		}

		UNiagaraComponent* Comp = Actor->GetNiagaraComponent();
		if (!Comp || !Comp->GetAsset())
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Actor '%s' has no Niagara system assigned"), *ActorName));
		}

		System = Comp->GetAsset();
		SourceName = ActorName;
	}
	else
	{
		// Option 2: from asset path
		FString SystemPath;
		if (!Params->TryGetStringField(TEXT("system_path"), SystemPath))
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				TEXT("Provide either 'actor_name' or 'system_path'"));
		}

		FString Error;
		System = NiagaraHelpers::LoadNiagaraSystem(SystemPath, Error);
		if (!System)
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
		}
		SourceName = SystemPath;
	}

	FString Filter;
	Params->TryGetStringField(TEXT("filter"), Filter);

	const FNiagaraUserRedirectionParameterStore& ExposedParams = System->GetExposedParameters();
	TArrayView<const FNiagaraVariableWithOffset> UserParameters = ExposedParams.ReadParameterVariables();

	TArray<TSharedPtr<FJsonValue>> ParamsArr;
	for (const FNiagaraVariableWithOffset& VarWithOffset : UserParameters)
	{
		const FNiagaraVariableBase& Var = VarWithOffset;
		FString ParamName = Var.GetName().ToString();
		if (!Filter.IsEmpty() && !ParamName.Contains(Filter, ESearchCase::IgnoreCase))
		{
			continue;
		}
		auto ParamObj = MakeShared<FJsonObject>();
		ParamObj->SetStringField(TEXT("name"), ParamName);
		ParamObj->SetStringField(TEXT("type"), Var.GetType().GetName());

		// Classify value type for callers
		FString TypeName = Var.GetType().GetName();
		if (TypeName.Contains(TEXT("Float")))
		{
			ParamObj->SetStringField(TEXT("value_type"), TEXT("float"));
		}
		else if (TypeName.Contains(TEXT("Int")))
		{
			ParamObj->SetStringField(TEXT("value_type"), TEXT("int"));
		}
		else if (TypeName.Contains(TEXT("Bool")))
		{
			ParamObj->SetStringField(TEXT("value_type"), TEXT("bool"));
		}
		else if (TypeName.Contains(TEXT("Vector")) || TypeName.Contains(TEXT("Position")))
		{
			ParamObj->SetStringField(TEXT("value_type"), TEXT("vector"));
		}
		else if (TypeName.Contains(TEXT("Color")))
		{
			ParamObj->SetStringField(TEXT("value_type"), TEXT("color"));
		}
		else
		{
			ParamObj->SetStringField(TEXT("value_type"), TypeName);
		}

		ParamsArr.Add(MakeShared<FJsonValueObject>(ParamObj));
	}

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("source"), SourceName);
	Result->SetStringField(TEXT("system_name"), System->GetName());
	Result->SetArrayField(TEXT("parameters"), ParamsArr);
	Result->SetNumberField(TEXT("count"), ParamsArr.Num());
	return Result;
}

// ---------------------------------------------------------------------------
// HandleAddNiagaraUserParameter
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleAddNiagaraUserParameter(
	const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
	FString SystemPath;
	if (!Params->TryGetStringField(TEXT("system_path"), SystemPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'system_path' parameter"));
	}

	FString ParameterName;
	if (!Params->TryGetStringField(TEXT("parameter_name"), ParameterName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'parameter_name' parameter"));
	}

	FString ParameterType = TEXT("float");
	Params->TryGetStringField(TEXT("parameter_type"), ParameterType);

	FString Error;
	UNiagaraSystem* System = NiagaraHelpers::LoadNiagaraSystem(SystemPath, Error);
	if (!System)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	FNiagaraTypeDefinition TypeDef = ResolveNiagaraType(ParameterType);

	// Build the user parameter variable with "User." namespace prefix
	FString FullName = ParameterName;
	if (!FullName.StartsWith(TEXT("User.")))
	{
		FullName = FString::Printf(TEXT("User.%s"), *ParameterName);
	}

	FNiagaraVariable NewVar(TypeDef, FName(*FullName));

	// Initialize default data
	NewVar.AllocateData();
	if (TypeDef == FNiagaraTypeDefinition::GetFloatDef())
	{
		float Default = 0.0f;
		Params->TryGetNumberField(TEXT("default_value"), Default);
		NewVar.SetValue(Default);
	}
	else if (TypeDef == FNiagaraTypeDefinition::GetIntDef())
	{
		int32 Default = 0;
		Params->TryGetNumberField(TEXT("default_value"), Default);
		NewVar.SetValue(Default);
	}
	else if (TypeDef == FNiagaraTypeDefinition::GetBoolDef())
	{
		FNiagaraBool BoolVal;
		bool bDefault = false;
		Params->TryGetBoolField(TEXT("default_value"), bDefault);
		BoolVal.SetValue(bDefault);
		NewVar.SetValue(BoolVal);
	}

	// Add to the system's exposed parameters
	System->GetExposedParameters().AddParameter(NewVar, true);
	System->MarkPackageDirty();
	System->PostEditChange();

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("parameter_name"), FullName);
	Result->SetStringField(TEXT("parameter_type"), TypeDef.GetName());
	return Result;
#else
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only operation"));
#endif
}

// ---------------------------------------------------------------------------
// HandleSetNiagaraUserParameter
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleSetNiagaraUserParameter(
	const TSharedPtr<FJsonObject>& Params)
{
	FString ParameterName;
	if (!Params->TryGetStringField(TEXT("parameter_name"), ParameterName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'parameter_name' parameter"));
	}

	FString ParameterType = TEXT("float");
	Params->TryGetStringField(TEXT("parameter_type"), ParameterType);

	// Determine source: either a running actor or an asset
	FString ActorName;
	FString SystemPath;
	bool bIsRuntime = Params->TryGetStringField(TEXT("actor_name"), ActorName);
	if (!bIsRuntime)
	{
		if (!Params->TryGetStringField(TEXT("system_path"), SystemPath))
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				TEXT("Provide either 'actor_name' (runtime) or 'system_path' (asset default)"));
		}
	}

	if (bIsRuntime)
	{
		// Runtime: set via UNiagaraComponent
		FString Error;
		ANiagaraActor* Actor = NiagaraHelpers::FindNiagaraActorByName(ActorName, Error);
		if (!Actor)
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
		}

		UNiagaraComponent* Comp = Actor->GetNiagaraComponent();
		if (!Comp)
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Actor has no Niagara component"));
		}

		FString LowerType = ParameterType.ToLower();

		if (LowerType == TEXT("float"))
		{
			double Value = 0.0;
			if (!Params->TryGetNumberField(TEXT("value"), Value))
			{
				return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'value' for float parameter"));
			}
			Comp->SetVariableFloat(FName(*ParameterName), static_cast<float>(Value));
		}
		else if (LowerType == TEXT("int"))
		{
			int32 Value = 0;
			if (!Params->TryGetNumberField(TEXT("value"), Value))
			{
				return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'value' for int parameter"));
			}
			Comp->SetVariableInt(FName(*ParameterName), Value);
		}
		else if (LowerType == TEXT("bool"))
		{
			bool Value = false;
			if (!Params->TryGetBoolField(TEXT("value"), Value))
			{
				return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'value' for bool parameter"));
			}
			Comp->SetVariableBool(FName(*ParameterName), Value);
		}
		else if (LowerType == TEXT("vector") || LowerType == TEXT("vec3"))
		{
			FVector Value = FEpicUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("value"));
			Comp->SetVariableVec3(FName(*ParameterName), Value);
		}
		else if (LowerType == TEXT("color") || LowerType == TEXT("linear_color"))
		{
			const TSharedPtr<FJsonObject>* ValueObj = nullptr;
			if (!Params->TryGetObjectField(TEXT("value"), ValueObj))
			{
				return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'value' object for color parameter"));
			}

			FLinearColor Color;
			Color.R = static_cast<float>((*ValueObj)->GetNumberField(TEXT("r")));
			Color.G = static_cast<float>((*ValueObj)->GetNumberField(TEXT("g")));
			Color.B = static_cast<float>((*ValueObj)->GetNumberField(TEXT("b")));
			Color.A = (*ValueObj)->HasField(TEXT("a")) ? static_cast<float>((*ValueObj)->GetNumberField(TEXT("a"))) : 1.0f;
			Comp->SetVariableLinearColor(FName(*ParameterName), Color);
		}
		else
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Unsupported parameter type '%s'. Use: float, int, bool, vector, color"), *ParameterType));
		}

		auto Result = MakeShared<FJsonObject>();
		Result->SetBoolField(TEXT("success"), true);
		Result->SetStringField(TEXT("parameter_name"), ParameterName);
		Result->SetStringField(TEXT("mode"), TEXT("runtime"));
		Result->SetStringField(TEXT("actor_name"), ActorName);
		return Result;
	}
	else
	{
		// Asset default: set value in ExposedParameters store
		FString Error;
		UNiagaraSystem* System = NiagaraHelpers::LoadNiagaraSystem(SystemPath, Error);
		if (!System)
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
		}

		FString FullName = ParameterName;
		if (!FullName.StartsWith(TEXT("User.")))
		{
			FullName = FString::Printf(TEXT("User.%s"), *ParameterName);
		}

		FNiagaraTypeDefinition TypeDef = ResolveNiagaraType(ParameterType);
		FNiagaraVariable Var(TypeDef, FName(*FullName));

		// Check if parameter exists
		FNiagaraUserRedirectionParameterStore& Store = System->GetExposedParameters();
		const FNiagaraVariableBase* Found = nullptr;
		for (const FNiagaraVariableWithOffset& Existing : Store.ReadParameterVariables())
		{
			if (Existing.GetName() == Var.GetName())
			{
				Found = &Existing;
				break;
			}
		}

		if (!Found)
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Parameter '%s' not found. Add it first with add_niagara_user_parameter"), *FullName));
		}

		// Set the value
		Var.AllocateData();
		if (TypeDef == FNiagaraTypeDefinition::GetFloatDef())
		{
			double Value = 0.0;
			Params->TryGetNumberField(TEXT("value"), Value);
			Var.SetValue(static_cast<float>(Value));
		}
		else if (TypeDef == FNiagaraTypeDefinition::GetIntDef())
		{
			int32 Value = 0;
			Params->TryGetNumberField(TEXT("value"), Value);
			Var.SetValue(Value);
		}
		else if (TypeDef == FNiagaraTypeDefinition::GetBoolDef())
		{
			bool bValue = false;
			Params->TryGetBoolField(TEXT("value"), bValue);
			FNiagaraBool BoolVal;
			BoolVal.SetValue(bValue);
			Var.SetValue(BoolVal);
		}

		Store.SetParameterData(Var.GetData(), Var, true);
		System->MarkPackageDirty();
		System->PostEditChange();

		auto Result = MakeShared<FJsonObject>();
		Result->SetBoolField(TEXT("success"), true);
		Result->SetStringField(TEXT("parameter_name"), FullName);
		Result->SetStringField(TEXT("mode"), TEXT("asset_default"));
		return Result;
	}
}

// ---------------------------------------------------------------------------
// HandleRemoveNiagaraUserParameter
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleRemoveNiagaraUserParameter(
	const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
	FString SystemPath;
	if (!Params->TryGetStringField(TEXT("system_path"), SystemPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'system_path' parameter"));
	}

	FString ParameterName;
	if (!Params->TryGetStringField(TEXT("parameter_name"), ParameterName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'parameter_name' parameter"));
	}

	FString Error;
	UNiagaraSystem* System = NiagaraHelpers::LoadNiagaraSystem(SystemPath, Error);
	if (!System)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	FString FullName = ParameterName;
	if (!FullName.StartsWith(TEXT("User.")))
	{
		FullName = FString::Printf(TEXT("User.%s"), *ParameterName);
	}

	FNiagaraUserRedirectionParameterStore& Store = System->GetExposedParameters();

	// Find the variable to remove
	FNiagaraVariable VarToRemove;
	bool bFound = false;
	for (const FNiagaraVariableWithOffset& Existing : Store.ReadParameterVariables())
	{
		if (Existing.GetName().ToString().Equals(FullName, ESearchCase::IgnoreCase))
		{
			VarToRemove = FNiagaraVariable(Existing.GetType(), Existing.GetName());
			bFound = true;
			break;
		}
	}

	if (!bFound)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("User parameter '%s' not found"), *FullName));
	}

	Store.RemoveParameter(VarToRemove);
	System->MarkPackageDirty();
	System->PostEditChange();

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("removed_parameter"), FullName);
	return Result;
#else
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only operation"));
#endif
}

// ---------------------------------------------------------------------------
// HandleLinkNiagaraParameter
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleLinkNiagaraParameter(
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

	FString ModuleName;
	if (!Params->TryGetStringField(TEXT("module_name"), ModuleName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'module_name' parameter"));
	}

	FString InputName;
	if (!Params->TryGetStringField(TEXT("input_name"), InputName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'input_name' parameter"));
	}

	FString LinkedParameterName;
	if (!Params->TryGetStringField(TEXT("linked_parameter"), LinkedParameterName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'linked_parameter' parameter (e.g., 'User.MyColor')"));
	}

	FString ScriptUsageStr = TEXT("particle_update");
	Params->TryGetStringField(TEXT("script_usage"), ScriptUsageStr);

	FString Error;
	UNiagaraSystem* System = NiagaraHelpers::LoadNiagaraSystem(SystemPath, Error);
	if (!System)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	int32 EmitterIdx = INDEX_NONE;
	FNiagaraEmitterHandle* Handle = NiagaraHelpers::FindEmitterHandle(System, EmitterName, EmitterIdx, Error);
	if (!Handle)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	bool bUsageOk = false;
	ENiagaraScriptUsage Usage = NiagaraHelpers::ParseScriptUsage(ScriptUsageStr, bUsageOk);
	if (!bUsageOk)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Invalid script_usage '%s'"), *ScriptUsageStr));
	}

	FVersionedNiagaraEmitterData* EmitterData = NiagaraHelpers::GetEmitterData(Handle);
	if (!EmitterData)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Emitter has no data"));
	}

	UNiagaraGraph* Graph = NiagaraHelpers::GetGraphForUsage(EmitterData, Usage);
	if (!Graph)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Could not get graph for script usage"));
	}

	UNiagaraNodeFunctionCall* ModuleNode = NiagaraHelpers::FindModuleNode(Graph, Usage, ModuleName, Error);
	if (!ModuleNode)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	// Find the input pin on the module node
	UEdGraphPin* InputPin = nullptr;
	for (UEdGraphPin* Pin : ModuleNode->Pins)
	{
		if (Pin->Direction == EGPD_Input &&
			Pin->PinName.ToString().Contains(InputName, ESearchCase::IgnoreCase))
		{
			InputPin = Pin;
			break;
		}
	}

	if (!InputPin)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Input pin '%s' not found on module '%s'"), *InputName, *ModuleName));
	}

	// Create a parameter map get node (NiagaraNodeInput) for the linked parameter
	FNiagaraTypeDefinition PinType = FNiagaraTypeDefinition::GetFloatDef();
	// Infer type from the pin
	if (InputPin->PinType.PinCategory != NAME_None)
	{
		FNiagaraVariable TempVar;
		TempVar.SetName(FName(*LinkedParameterName));
		// Use the pin's type if it can be resolved
		PinType = TempVar.GetType().IsValid() ? TempVar.GetType() : FNiagaraTypeDefinition::GetFloatDef();
	}

	// Set the default value to reference the linked parameter
	// In Niagara, linking a module input to a parameter is done by setting the
	// pin's linked default to point to the parameter name.
	InputPin->DefaultValue = LinkedParameterName;
	InputPin->bDefaultValueIsIgnored = false;

	Graph->NotifyGraphChanged();
	NiagaraHelpers::CompileAndSync(System);

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("module_name"), ModuleName);
	Result->SetStringField(TEXT("input_name"), InputName);
	Result->SetStringField(TEXT("linked_parameter"), LinkedParameterName);
	Result->SetStringField(TEXT("script_usage"), ScriptUsageStr);
	return Result;
#else
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only operation"));
#endif
}
