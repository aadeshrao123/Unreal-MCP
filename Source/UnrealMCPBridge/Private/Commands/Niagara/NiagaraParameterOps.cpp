#include "Commands/EpicUnrealMCPNiagaraCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "NiagaraHelpers.h"
#include "NiagaraPropertyIntrospection.h"

#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraComponent.h"
#include "NiagaraActor.h"
#include "NiagaraParameterStore.h"
#include "NiagaraTypes.h"
#include "NiagaraTypeRegistry.h"
#include "NiagaraDataInterface.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeFunctionCall.h"

#if WITH_EDITORONLY_DATA
#include "NiagaraScriptVariable.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraCommon.h"
#include "EdGraphSchema_Niagara.h"
#include "ScopedTransaction.h"
#include "ViewModels/Stack/NiagaraParameterHandle.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#include "NiagaraNodeInput.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraStackPathResolver.h"
#endif

#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"

// ---------------------------------------------------------------------------
// Helper: Niagara type from string
// ---------------------------------------------------------------------------
//
// Delegates to NiagaraIntrospection::ResolveTypeName which walks the shared
// NiagaraTypeHelpers::ParseTypeDef table first (primitives, Position, Matrix,
// Quat, ID, RandInfo, ParameterMap, etc.), then tries UEnum / UScriptStruct /
// UClass lookups for enums, structs, and Data Interfaces. This means anything
// FNiagaraTypeRegistry knows about is resolvable from MCP.
//
// Returns an invalid type definition on failure so callers can surface a
// meaningful error to the user instead of silently falling back to float.

static FNiagaraTypeDefinition ResolveNiagaraType(const FString& TypeStr)
{
	FNiagaraTypeDefinition Resolved;
	if (NiagaraIntrospection::ResolveTypeName(TypeStr, Resolved))
	{
		return Resolved;
	}
	return FNiagaraTypeDefinition();
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
	if (!TypeDef.IsValid())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(
				TEXT("Unknown parameter_type '%s'. Use list_niagara_parameter_types to discover valid names "
					 "(primitives, structs, enums, and data interfaces)."),
				*ParameterType));
	}

	// Build the user parameter variable with "User." namespace prefix
	FString FullName = ParameterName;
	if (!FullName.StartsWith(TEXT("User.")))
	{
		FullName = FString::Printf(TEXT("User.%s"), *ParameterName);
	}

	FNiagaraVariable NewVar(TypeDef, FName(*FullName));

	// Allocate the default storage for primitive/struct types. Data interfaces
	// don't need AllocateData — their storage is the UNiagaraDataInterface object
	// pointer which AddParameter creates automatically when bInitInterfaces=true.
	if (!TypeDef.IsDataInterface())
	{
		NewVar.AllocateData();
	}

	// Parse default value for the common primitive types. For structs/DIs we
	// leave the default at the CDO/zero — callers can use set_niagara_user_parameter
	// afterwards for richer initialization.
	if (TypeDef == FNiagaraTypeDefinition::GetFloatDef())
	{
		double Default = 0.0;
		Params->TryGetNumberField(TEXT("default_value"), Default);
		NewVar.SetValue(static_cast<float>(Default));
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
	else if (TypeDef == FNiagaraTypeDefinition::GetVec2Def()
		|| TypeDef == FNiagaraTypeDefinition::GetVec3Def()
		|| TypeDef == FNiagaraTypeDefinition::GetVec4Def()
		|| TypeDef == FNiagaraTypeDefinition::GetPositionDef()
		|| TypeDef == FNiagaraTypeDefinition::GetColorDef()
		|| TypeDef == FNiagaraTypeDefinition::GetQuatDef())
	{
		// Accept both "(X=1,Y=2,Z=3)" style and comma-separated "1,2,3"
		FString DefaultStr;
		if (Params->TryGetStringField(TEXT("default_value"), DefaultStr) && !DefaultStr.IsEmpty())
		{
			// Use ImportText on the underlying struct so any UE-standard form works.
			if (UScriptStruct* Struct = TypeDef.GetScriptStruct())
			{
				Struct->ImportText(*DefaultStr, NewVar.GetData(), nullptr, PPF_None, nullptr,
					Struct->GetName());
			}
		}
	}

	// AddParameter does the right thing for every kind:
	//   - primitive/struct: copies the default bytes
	//   - data interface: with bInitInterfaces=true (default) it creates a new
	//     instance of the DI class and stores it in the internal DI array
	//   - uobject: tracked via the internal UObject map
	System->GetExposedParameters().AddParameter(NewVar, /*bInitInterfaces=*/true, /*bTriggerRebind=*/true);
	System->MarkPackageDirty();
	System->PostEditChange();

	// Classify for the response so callers can confirm how the parameter was handled.
	FString Kind = TEXT("primitive");
	FString ClassPath;
	if (TypeDef.IsDataInterface())
	{
		Kind = TEXT("data_interface");
		if (UClass* Cls = TypeDef.GetClass())
		{
			ClassPath = Cls->GetPathName();
		}
	}
	else if (TypeDef.IsEnum())
	{
		Kind = TEXT("enum");
	}
	else if (TypeDef.IsUObject())
	{
		Kind = TEXT("object");
	}
	else if (TypeDef.GetScriptStruct())
	{
		Kind = TEXT("struct");
	}

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("parameter_name"), FullName);
	Result->SetStringField(TEXT("parameter_type"), TypeDef.GetName());
	Result->SetStringField(TEXT("kind"), Kind);
	if (!ClassPath.IsEmpty())
	{
		Result->SetStringField(TEXT("class_path"), ClassPath);
	}
	Result->SetNumberField(TEXT("size_bytes"), TypeDef.GetSize());
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

	// Nested-path resolution (Gap #10): "Spawn Count.Position Array" descends
	// through dynamic-input chains so the link lands on the leaf input.
	{
		FString LeafName, DescentError;
		UNiagaraNodeFunctionCall* TargetCall =
			NiagaraStackPath::DescendNestedPath(*ModuleNode, InputName, LeafName, DescentError);
		if (!TargetCall)
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(DescentError);
		}
		ModuleNode = TargetCall;
		InputName = LeafName; // remainder of handler resolves the leaf name
	}

	// Resolve the module input's FNiagaraVariable and type.
	// Strategy: first try direct function-call pins, then fall back to
	// parameter-map stack inputs (Module.*) surfaced via GetStackFunctionInputs.
	// This is the same two-pass used by HandleGetNiagaraModuleInputs so any input
	// listable via the query tool is writable via link.
	const UEdGraphSchema_Niagara* NiagaraSchema = GetDefault<UEdGraphSchema_Niagara>();
	FNiagaraTypeDefinition InputType;
	FString ResolvedInputName;
	FString InputSource;

	for (UEdGraphPin* Pin : ModuleNode->Pins)
	{
		if (Pin->Direction != EGPD_Input)
		{
			continue;
		}
		const FString PinNameStr = Pin->PinName.ToString();
		if (PinNameStr.Equals(InputName, ESearchCase::IgnoreCase))
		{
			InputType = NiagaraSchema->PinToTypeDefinition(Pin);
			ResolvedInputName = PinNameStr;
			InputSource = TEXT("function_call_pin");
			break;
		}
	}

	if (!InputType.IsValid())
	{
		TArray<FNiagaraVariable> StackInputs;
		FCompileConstantResolver ConstantResolver(System, Usage);
		FNiagaraStackGraphUtilities::GetStackFunctionInputs(
			*ModuleNode, StackInputs, ConstantResolver,
			FNiagaraStackGraphUtilities::ENiagaraGetStackFunctionInputPinsOptions::ModuleInputsOnly);

		const FString ModulePrefix(TEXT("Module."));
		for (const FNiagaraVariable& Var : StackInputs)
		{
			FString DisplayName = Var.GetName().ToString();
			if (DisplayName.StartsWith(ModulePrefix))
			{
				DisplayName.RightChopInline(ModulePrefix.Len());
			}
			if (DisplayName.Equals(InputName, ESearchCase::IgnoreCase))
			{
				InputType = Var.GetType();
				ResolvedInputName = DisplayName;
				InputSource = TEXT("parameter_map_input");
				break;
			}
		}
	}

	if (!InputType.IsValid())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Input '%s' not found on module '%s'"), *InputName, *ModuleName));
	}

	// Build the aliased module parameter handle so the override is scoped to
	// this emitter/particle context (e.g. Particles.Module.Boost, not just Module.Boost).
	FNiagaraParameterHandle InputHandle =
		FNiagaraParameterHandle::CreateModuleParameterHandle(FName(*ResolvedInputName));
	FNiagaraParameterHandle AliasedHandle =
		FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(InputHandle, ModuleNode);

	// Wrap the whole link operation in a transaction so undo/redo is clean.
	FScopedTransaction Transaction(
		NSLOCTEXT("UnrealMCPBridge", "LinkNiagaraParameter", "Link Niagara Parameter"));
	ModuleNode->Modify();
	Graph->Modify();

	// GetOrCreateStackFunctionInputOverridePin walks the module's internal override
	// ParameterMapSet (creating it if needed) and returns the pin that represents
	// this specific input's override. Same path used by the editor's stack UI.
	UEdGraphPin& OverridePin = FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(
		*ModuleNode, AliasedHandle, InputType, FGuid(), FGuid());

	// Clean any existing override (inline value, dynamic input, previous link).
	if (OverridePin.LinkedTo.Num() > 0)
	{
		TArray<UEdGraphNode*> NodesToRemove;
		for (UEdGraphPin* LinkedPin : OverridePin.LinkedTo)
		{
			if (LinkedPin && LinkedPin->GetOwningNode())
			{
				NodesToRemove.AddUnique(LinkedPin->GetOwningNode());
			}
		}
		OverridePin.BreakAllPinLinks();
		for (UEdGraphNode* NodeToRemove : NodesToRemove)
		{
			if (!NodeToRemove)
			{
				continue;
			}
			for (UEdGraphPin* Pin : NodeToRemove->Pins)
			{
				if (Pin)
				{
					Pin->BreakAllPinLinks();
				}
			}
			Graph->RemoveNode(NodeToRemove);
		}
	}

	// Known-parameters set lets SetLinkedParameterValueForFunctionInput reuse
	// existing script-variable definitions instead of inventing new ones.
	// Pull: (a) system exposed User.* params, (b) emitter-scope params, (c) engine-provided constants.
	TSet<FNiagaraVariableBase> KnownParameters;
	{
		TArray<FNiagaraVariable> UserVars;
		System->GetExposedParameters().GetParameters(UserVars);
		for (const FNiagaraVariable& Var : UserVars)
		{
			KnownParameters.Add(Var);
		}

		if (Graph)
		{
			for (const auto& It : Graph->GetAllMetaData())
			{
				KnownParameters.Add(It.Key);
			}
		}
	}

	// Build the linked parameter variable using the resolved input type so the
	// override pin matches. If user passed "User.ColorA" and input type is Color,
	// the linked variable is FNiagaraVariable(Color, "User.ColorA").
	FNiagaraVariableBase LinkedParameter(InputType, FName(*LinkedParameterName));

	FNiagaraStackGraphUtilities::SetLinkedParameterValueForFunctionInput(
		OverridePin,
		LinkedParameter,
		KnownParameters,
		ENiagaraDefaultMode::FailIfPreviouslyNotSet,
		FGuid());

	Graph->NotifyGraphChanged();
	NiagaraHelpers::CompileAndSync(System);

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("module_name"), ModuleName);
	Result->SetStringField(TEXT("input_name"), ResolvedInputName);
	Result->SetStringField(TEXT("input_source"), InputSource);
	Result->SetStringField(TEXT("input_type"), InputType.GetName());
	Result->SetStringField(TEXT("linked_parameter"), LinkedParameterName);
	Result->SetStringField(TEXT("script_usage"), ScriptUsageStr);
	return Result;
#else
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only operation"));
#endif
}
