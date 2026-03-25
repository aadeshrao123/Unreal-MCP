#include "Commands/EpicUnrealMCPNiagaraCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "NiagaraHelpers.h"

#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeInput.h"
#include "NiagaraCommon.h"
#include "NiagaraTypes.h"
#include "NiagaraDataInterfaceCurve.h"
#include "NiagaraDataInterfaceVectorCurve.h"
#include "NiagaraDataInterfaceColorCurve.h"

#include "EdGraph/EdGraphPin.h"
#include "Curves/RichCurve.h"
#include "ScopedTransaction.h"

#if WITH_EDITORONLY_DATA
#include "NiagaraSystemEditorData.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#include "ViewModels/Stack/NiagaraParameterHandle.h"
#include "Stateless/NiagaraStatelessDistribution.h"
#endif

// ---------------------------------------------------------------------------
// Local helpers
// ---------------------------------------------------------------------------

/**
 * Remove nodes wired to an override pin and break the links.
 * Duplicated here because NiagaraCurveOps is a separate translation unit.
 */
static void RemoveOverridePinConnectionsForCurve(UEdGraphPin& OverridePin, UNiagaraGraph* Graph)
{
	if (OverridePin.LinkedTo.Num() == 0)
	{
		return;
	}

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
		if (!NodeToRemove || !Graph)
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

/**
 * Populate a float curve data interface from JSON keys.
 * Keys format: [{time, value}, ...]
 * Returns number of keys added.
 */
static int32 PopulateFloatCurve(
	UNiagaraDataInterfaceCurve* CurveDI,
	const TArray<TSharedPtr<FJsonValue>>& Keys)
{
	if (!CurveDI)
	{
		return 0;
	}

	CurveDI->CurveAsset = nullptr;
	CurveDI->Curve.Reset();

	int32 Count = 0;
	for (const TSharedPtr<FJsonValue>& KeyVal : Keys)
	{
		TSharedPtr<FJsonObject> KeyObj = KeyVal->AsObject();
		if (!KeyObj)
		{
			continue;
		}

		float Time = static_cast<float>(KeyObj->GetNumberField(TEXT("time")));
		float Value = static_cast<float>(KeyObj->GetNumberField(TEXT("value")));
		CurveDI->Curve.AddKey(Time, Value);
		++Count;
	}

	CurveDI->UpdateTimeRanges();
	CurveDI->UpdateLUT();
	CurveDI->MarkRenderDataDirty();
	// OnChanged not exported from Niagara DLL — compilation will refresh data

	return Count;
}

/**
 * Populate a vector curve data interface from JSON keys.
 * Keys format: [{time, x, y, z}, ...]
 * Returns number of keys added.
 */
static int32 PopulateVectorCurve(
	UNiagaraDataInterfaceVectorCurve* CurveDI,
	const TArray<TSharedPtr<FJsonValue>>& Keys)
{
	if (!CurveDI)
	{
		return 0;
	}

	CurveDI->CurveAsset = nullptr;
	CurveDI->XCurve.Reset();
	CurveDI->YCurve.Reset();
	CurveDI->ZCurve.Reset();

	int32 Count = 0;
	for (const TSharedPtr<FJsonValue>& KeyVal : Keys)
	{
		TSharedPtr<FJsonObject> KeyObj = KeyVal->AsObject();
		if (!KeyObj)
		{
			continue;
		}

		float Time = static_cast<float>(KeyObj->GetNumberField(TEXT("time")));
		float X = static_cast<float>(KeyObj->GetNumberField(TEXT("x")));
		float Y = static_cast<float>(KeyObj->GetNumberField(TEXT("y")));
		float Z = static_cast<float>(KeyObj->GetNumberField(TEXT("z")));

		CurveDI->XCurve.AddKey(Time, X);
		CurveDI->YCurve.AddKey(Time, Y);
		CurveDI->ZCurve.AddKey(Time, Z);
		++Count;
	}

	CurveDI->UpdateTimeRanges();
	CurveDI->UpdateLUT();
	CurveDI->MarkRenderDataDirty();
	// OnChanged not exported from Niagara DLL — compilation will refresh data

	return Count;
}

/**
 * Populate a color curve data interface from JSON keys.
 * Keys format: [{time, r, g, b, a?}, ...]
 * Returns number of keys added.
 */
static int32 PopulateColorCurve(
	UNiagaraDataInterfaceColorCurve* CurveDI,
	const TArray<TSharedPtr<FJsonValue>>& Keys)
{
	if (!CurveDI)
	{
		return 0;
	}

	CurveDI->CurveAsset = nullptr;
	CurveDI->RedCurve.Reset();
	CurveDI->GreenCurve.Reset();
	CurveDI->BlueCurve.Reset();
	CurveDI->AlphaCurve.Reset();

	int32 Count = 0;
	for (const TSharedPtr<FJsonValue>& KeyVal : Keys)
	{
		TSharedPtr<FJsonObject> KeyObj = KeyVal->AsObject();
		if (!KeyObj)
		{
			continue;
		}

		float Time = static_cast<float>(KeyObj->GetNumberField(TEXT("time")));
		float R = static_cast<float>(KeyObj->GetNumberField(TEXT("r")));
		float G = static_cast<float>(KeyObj->GetNumberField(TEXT("g")));
		float B = static_cast<float>(KeyObj->GetNumberField(TEXT("b")));

		double A = 1.0;
		KeyObj->TryGetNumberField(TEXT("a"), A);

		CurveDI->RedCurve.AddKey(Time, R);
		CurveDI->GreenCurve.AddKey(Time, G);
		CurveDI->BlueCurve.AddKey(Time, B);
		CurveDI->AlphaCurve.AddKey(Time, static_cast<float>(A));
		++Count;
	}

	CurveDI->UpdateTimeRanges();
	CurveDI->UpdateLUT();
	CurveDI->MarkRenderDataDirty();
	// OnChanged not exported from Niagara DLL — compilation will refresh data

	return Count;
}

#if WITH_EDITORONLY_DATA

/**
 * Detect whether an emitter is running in Stateless mode.
 * Stateless emitters use Distribution structs instead of data interface curves.
 */
static bool IsStatelessEmitter(FNiagaraEmitterHandle* Handle)
{
	if (!Handle)
	{
		return false;
	}

	UObject* StatelessEmitter = reinterpret_cast<UObject*>(Handle->GetStatelessEmitter());
	ENiagaraEmitterMode EmitterMode = Handle->GetEmitterMode();

	return (StatelessEmitter != nullptr && EmitterMode == ENiagaraEmitterMode::Stateless);
}

/**
 * Handle curve assignment on a Stateless emitter via Distribution reflection.
 * Stateless modules use FNiagaraDistributionColor (and similar) instead of graph-based DIs.
 */
static TSharedPtr<FJsonObject> HandleStatelessCurve(
	FNiagaraEmitterHandle* Handle,
	UNiagaraSystem* System,
	const FString& ModuleName,
	const FString& InputName,
	const FString& CurveType,
	const TArray<TSharedPtr<FJsonValue>>& Keys)
{
	UObject* StatelessEmitter = reinterpret_cast<UObject*>(Handle->GetStatelessEmitter());
	if (!StatelessEmitter)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Stateless emitter pointer is null"));
	}

	// Find the Modules array on the stateless emitter
	FArrayProperty* ModulesArrayProp = CastField<FArrayProperty>(
		StatelessEmitter->GetClass()->FindPropertyByName(TEXT("Modules")));
	if (!ModulesArrayProp)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Could not find Modules property on Stateless emitter"));
	}

	FScriptArrayHelper ModulesArray(
		ModulesArrayProp, ModulesArrayProp->ContainerPtrToValuePtr<void>(StatelessEmitter));

	UObject* TargetModule = nullptr;
	for (int32 i = 0; i < ModulesArray.Num(); ++i)
	{
		UObject** ModulePtr = reinterpret_cast<UObject**>(ModulesArray.GetRawPtr(i));
		if (ModulePtr && *ModulePtr)
		{
			UObject* Module = *ModulePtr;
			if (Module->GetClass()->GetName().Contains(ModuleName, ESearchCase::IgnoreCase))
			{
				TargetModule = Module;
				break;
			}
		}
	}

	if (!TargetModule)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Stateless module not found: %s"), *ModuleName));
	}

	// Find a Distribution property matching the input name
	FProperty* DistributionProp = nullptr;
	FString FoundPropName;
	for (TFieldIterator<FProperty> PropIt(TargetModule->GetClass()); PropIt; ++PropIt)
	{
		FProperty* Prop = *PropIt;
		FString PropName = Prop->GetName();

		FStructProperty* StructProp = CastField<FStructProperty>(Prop);
		if (!StructProp || !PropName.Contains(TEXT("Distribution"), ESearchCase::IgnoreCase))
		{
			continue;
		}

		FString PropNameWithoutDist = PropName.Replace(TEXT("Distribution"), TEXT(""));
		if (InputName.Contains(PropNameWithoutDist, ESearchCase::IgnoreCase) ||
			PropNameWithoutDist.Contains(InputName, ESearchCase::IgnoreCase))
		{
			DistributionProp = Prop;
			FoundPropName = PropName;
			break;
		}
	}

	if (!DistributionProp)
	{
		// List available properties for the error message
		FString AvailableProps;
		for (TFieldIterator<FProperty> PropIt(TargetModule->GetClass()); PropIt; ++PropIt)
		{
			if (!AvailableProps.IsEmpty())
			{
				AvailableProps += TEXT(", ");
			}
			AvailableProps += (*PropIt)->GetName();
		}
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(
				TEXT("Distribution property not found for input '%s'. Available: %s"),
				*InputName, *AvailableProps));
	}

	// Handle color distributions
	if (CurveType.Equals(TEXT("color"), ESearchCase::IgnoreCase))
	{
		FStructProperty* StructProp = CastField<FStructProperty>(DistributionProp);
		if (StructProp && StructProp->Struct->GetFName() == TEXT("NiagaraDistributionColor"))
		{
			FNiagaraDistributionColor* ColorDist =
				StructProp->ContainerPtrToValuePtr<FNiagaraDistributionColor>(TargetModule);
			if (!ColorDist)
			{
				return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
					TEXT("Failed to get Distribution color pointer"));
			}

			ColorDist->Mode = ENiagaraDistributionMode::UniformCurve;

			// Access the base class ChannelCurves (R, G, B, A)
			FNiagaraDistributionBase* BaseDistribution =
				static_cast<FNiagaraDistributionBase*>(ColorDist);
			BaseDistribution->ChannelCurves.SetNum(4);
			for (int32 i = 0; i < 4; ++i)
			{
				BaseDistribution->ChannelCurves[i].Reset();
			}

			int32 KeyCount = 0;
			for (const TSharedPtr<FJsonValue>& KeyVal : Keys)
			{
				TSharedPtr<FJsonObject> KeyObj = KeyVal->AsObject();
				if (!KeyObj)
				{
					continue;
				}

				float Time = static_cast<float>(KeyObj->GetNumberField(TEXT("time")));
				float R = static_cast<float>(KeyObj->GetNumberField(TEXT("r")));
				float G = static_cast<float>(KeyObj->GetNumberField(TEXT("g")));
				float B = static_cast<float>(KeyObj->GetNumberField(TEXT("b")));

				double A = 1.0;
				KeyObj->TryGetNumberField(TEXT("a"), A);

				BaseDistribution->ChannelCurves[0].AddKey(Time, R);
				BaseDistribution->ChannelCurves[1].AddKey(Time, G);
				BaseDistribution->ChannelCurves[2].AddKey(Time, B);
				BaseDistribution->ChannelCurves[3].AddKey(Time, static_cast<float>(A));
				++KeyCount;
			}

			ColorDist->ValuesTimeRange = FVector2f(0.0f, 1.0f);
			ColorDist->UpdateValuesFromDistribution();

			TargetModule->Modify();
			StatelessEmitter->Modify();
			System->MarkPackageDirty();
			System->PostEditChange();
			System->RequestCompile(true);
			System->WaitForCompilationComplete();

			TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
			Data->SetStringField(TEXT("module_name"), ModuleName);
			Data->SetStringField(TEXT("input_name"), InputName);
			Data->SetStringField(TEXT("property_name"), FoundPropName);
			Data->SetStringField(TEXT("curve_type"), CurveType);
			Data->SetNumberField(TEXT("key_count"), KeyCount);
			Data->SetBoolField(TEXT("stateless_emitter"), true);

			return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Data);
		}
	}

	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
		FString::Printf(TEXT("Unsupported curve type for Stateless emitter: %s"), *CurveType));
}

/**
 * Set a curve on a DataInterface pin (the pin type is "Class").
 * Creates the DI and populates it via FNiagaraStackGraphUtilities::SetDataInterfaceValueForFunctionInput.
 */
static TSharedPtr<FJsonObject> HandleDirectDICurve(
	UNiagaraNodeFunctionCall* ModuleNode,
	UNiagaraGraph* Graph,
	UNiagaraSystem* System,
	const FString& ModuleName,
	const FString& InputName,
	const FString& CurveType,
	const TArray<TSharedPtr<FJsonValue>>& Keys)
{
	// Determine the DI class based on curve type
	UClass* CurveClass = nullptr;
	if (CurveType.Equals(TEXT("float"), ESearchCase::IgnoreCase))
	{
		CurveClass = UNiagaraDataInterfaceCurve::StaticClass();
	}
	else if (CurveType.Equals(TEXT("vector"), ESearchCase::IgnoreCase))
	{
		CurveClass = UNiagaraDataInterfaceVectorCurve::StaticClass();
	}
	else if (CurveType.Equals(TEXT("color"), ESearchCase::IgnoreCase))
	{
		CurveClass = UNiagaraDataInterfaceColorCurve::StaticClass();
	}
	else
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Unknown curve type: '%s'"), *CurveType));
	}

	// Create parameter handles for the override pin system
	FNiagaraParameterHandle InputHandle =
		FNiagaraParameterHandle::CreateModuleParameterHandle(FName(*InputName));
	FNiagaraParameterHandle AliasedHandle =
		FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(InputHandle, ModuleNode);

	FNiagaraTypeDefinition CurveTypeDef(CurveClass);

	// Get or create the override pin
	UEdGraphPin& OverridePin = FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(
		*ModuleNode, AliasedHandle, CurveTypeDef, FGuid(), FGuid());

	// Remove any existing override
	if (OverridePin.LinkedTo.Num() > 0)
	{
		RemoveOverridePinConnectionsForCurve(OverridePin, Graph);
	}

	// Create the curve DI via the stack graph utilities
	UNiagaraDataInterface* CurveDataInterface = nullptr;
	FString AliasedInputName = AliasedHandle.GetParameterHandleString().ToString();
	FNiagaraStackGraphUtilities::SetDataInterfaceValueForFunctionInput(
		OverridePin, CurveClass, AliasedInputName, CurveDataInterface, FGuid());

	if (!CurveDataInterface)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create curve data interface"));
	}

	FScopedTransaction Transaction(
		NSLOCTEXT("UnrealMCPBridge", "SetNiagaraCurveDirect", "Set Niagara Curve Direct"));
	CurveDataInterface->Modify();

	// Populate the DI
	int32 KeyCount = 0;
	if (CurveType.Equals(TEXT("float"), ESearchCase::IgnoreCase))
	{
		KeyCount = PopulateFloatCurve(
			Cast<UNiagaraDataInterfaceCurve>(CurveDataInterface), Keys);
	}
	else if (CurveType.Equals(TEXT("vector"), ESearchCase::IgnoreCase))
	{
		KeyCount = PopulateVectorCurve(
			Cast<UNiagaraDataInterfaceVectorCurve>(CurveDataInterface), Keys);
	}
	else if (CurveType.Equals(TEXT("color"), ESearchCase::IgnoreCase))
	{
		KeyCount = PopulateColorCurve(
			Cast<UNiagaraDataInterfaceColorCurve>(CurveDataInterface), Keys);
	}

	// Mark the InputNode that owns this DI for synchronization
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		UNiagaraNodeInput* InputNode = Cast<UNiagaraNodeInput>(Node);
		if (!InputNode)
		{
			continue;
		}

		// Use reflection to check if this input node owns our DI
		FObjectProperty* DIProp = CastField<FObjectProperty>(
			InputNode->GetClass()->FindPropertyByName(TEXT("DataInterface")));
		if (!DIProp)
		{
			continue;
		}

		UNiagaraDataInterface* NodeDI = Cast<UNiagaraDataInterface>(
			DIProp->GetObjectPropertyValue_InContainer(InputNode));
		if (NodeDI == CurveDataInterface)
		{
			InputNode->MarkNodeRequiresSynchronization(
				TEXT("Curve data interface modified"), true);
			break;
		}
	}

	Graph->NotifyGraphChanged();
	ModuleNode->MarkNodeRequiresSynchronization(TEXT("Curve set"), true);

	System->MarkPackageDirty();
	System->PostEditChange();
	System->RequestCompile(true);
	System->WaitForCompilationComplete();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("module_name"), ModuleName);
	Data->SetStringField(TEXT("input_name"), InputName);
	Data->SetStringField(TEXT("curve_type"), CurveType);
	Data->SetNumberField(TEXT("key_count"), KeyCount);
	Data->SetBoolField(TEXT("direct_data_interface"), true);
	Data->SetBoolField(TEXT("stateless_emitter"), false);

	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Data);
}

/**
 * Set a curve on a value-type pin (e.g., Color, Float, Vector) via Dynamic Input.
 * Uses FloatFromCurve / VectorFromCurve / ColorFromCurve dynamic inputs
 * to sample from a curve DI and produce the value.
 */
static TSharedPtr<FJsonObject> HandleDynamicInputCurve(
	UNiagaraNodeFunctionCall* ModuleNode,
	UNiagaraGraph* Graph,
	UNiagaraSystem* System,
	const FString& ModuleName,
	const FString& InputName,
	const FString& CurveType,
	const TArray<TSharedPtr<FJsonValue>>& Keys)
{
	// Determine the DI script path and types
	FString DynamicInputPath;
	FNiagaraTypeDefinition InputType;
	UClass* CurveClass = nullptr;

	if (CurveType.Equals(TEXT("float"), ESearchCase::IgnoreCase))
	{
		DynamicInputPath = TEXT("/Niagara/DynamicInputs/ValueFromCurve/FloatFromCurve.FloatFromCurve");
		InputType = FNiagaraTypeDefinition::GetFloatDef();
		CurveClass = UNiagaraDataInterfaceCurve::StaticClass();
	}
	else if (CurveType.Equals(TEXT("vector"), ESearchCase::IgnoreCase))
	{
		DynamicInputPath = TEXT("/Niagara/DynamicInputs/ValueFromCurve/VectorFromCurve.VectorFromCurve");
		InputType = FNiagaraTypeDefinition::GetVec3Def();
		CurveClass = UNiagaraDataInterfaceVectorCurve::StaticClass();
	}
	else if (CurveType.Equals(TEXT("color"), ESearchCase::IgnoreCase))
	{
		DynamicInputPath = TEXT("/Niagara/DynamicInputs/ValueFromCurve/ColorFromCurve.ColorFromCurve");
		InputType = FNiagaraTypeDefinition::GetColorDef();
		CurveClass = UNiagaraDataInterfaceColorCurve::StaticClass();
	}
	else
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(
				TEXT("Unknown curve type: '%s'. Supported: float, vector, color"), *CurveType));
	}

	// Load the Dynamic Input script
	UNiagaraScript* DynamicInputScript = LoadObject<UNiagaraScript>(nullptr, *DynamicInputPath);
	if (!DynamicInputScript)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Failed to load Dynamic Input script: %s"), *DynamicInputPath));
	}

	// Create parameter handles
	FNiagaraParameterHandle InputHandle =
		FNiagaraParameterHandle::CreateModuleParameterHandle(FName(*InputName));
	FNiagaraParameterHandle AliasedHandle =
		FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(InputHandle, ModuleNode);

	// Get or create the override pin for the module input
	UEdGraphPin& OverridePin = FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(
		*ModuleNode, AliasedHandle, InputType, FGuid(), FGuid());

	// Remove any existing override
	if (OverridePin.LinkedTo.Num() > 0)
	{
		RemoveOverridePinConnectionsForCurve(OverridePin, Graph);
	}

	// Set the Dynamic Input wrapper function on the module's input
	UNiagaraNodeFunctionCall* DynamicInputNode = nullptr;
	FNiagaraStackGraphUtilities::SetDynamicInputForFunctionInput(
		OverridePin, DynamicInputScript, DynamicInputNode, FGuid(),
		CurveType + TEXT("FromCurve"), FGuid());

	if (!DynamicInputNode)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Failed to create Dynamic Input node"));
	}

	// Now find the curve DI input on the dynamic input node ("DefaultCurve")
	FString CurveInputName = TEXT("DefaultCurve");
	FNiagaraParameterHandle CurveInputHandle =
		FNiagaraParameterHandle::CreateModuleParameterHandle(FName(*CurveInputName));
	FNiagaraParameterHandle AliasedCurveHandle =
		FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(CurveInputHandle, DynamicInputNode);

	FNiagaraTypeDefinition CurveInputType(CurveClass);

	// Get or create the curve input override pin on the dynamic input node
	UEdGraphPin& CurveOverridePin = FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(
		*DynamicInputNode, AliasedCurveHandle, CurveInputType, FGuid(), FGuid());

	if (CurveOverridePin.LinkedTo.Num() > 0)
	{
		RemoveOverridePinConnectionsForCurve(CurveOverridePin, Graph);
	}

	// Create the actual curve DI
	UNiagaraDataInterface* CurveDataInterface = nullptr;
	FString AliasedCurveInputName = AliasedCurveHandle.GetParameterHandleString().ToString();
	FNiagaraStackGraphUtilities::SetDataInterfaceValueForFunctionInput(
		CurveOverridePin, CurveClass, AliasedCurveInputName, CurveDataInterface, FGuid());

	if (!CurveDataInterface)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Failed to create curve data interface on dynamic input"));
	}

	FScopedTransaction Transaction(
		NSLOCTEXT("UnrealMCPBridge", "SetNiagaraCurveDI", "Set Niagara Curve"));
	CurveDataInterface->Modify();

	// Populate the curve
	int32 KeyCount = 0;
	if (CurveType.Equals(TEXT("float"), ESearchCase::IgnoreCase))
	{
		KeyCount = PopulateFloatCurve(
			Cast<UNiagaraDataInterfaceCurve>(CurveDataInterface), Keys);
	}
	else if (CurveType.Equals(TEXT("vector"), ESearchCase::IgnoreCase))
	{
		KeyCount = PopulateVectorCurve(
			Cast<UNiagaraDataInterfaceVectorCurve>(CurveDataInterface), Keys);
	}
	else if (CurveType.Equals(TEXT("color"), ESearchCase::IgnoreCase))
	{
		KeyCount = PopulateColorCurve(
			Cast<UNiagaraDataInterfaceColorCurve>(CurveDataInterface), Keys);
	}

	// Notify graph and compile
	Graph->NotifyGraphChanged();
	DynamicInputNode->MarkNodeRequiresSynchronization(TEXT("Curve set"), true);
	ModuleNode->MarkNodeRequiresSynchronization(TEXT("Input overridden"), true);

	System->MarkPackageDirty();
	System->PostEditChange();
	System->RequestCompile(true);
	System->WaitForCompilationComplete();

	// Auto-set Scale Mode enum to curve mode if such a pin exists on the module
	for (UEdGraphPin* Pin : ModuleNode->Pins)
	{
		if (Pin->Direction != EGPD_Input)
		{
			continue;
		}
		if (!Pin->PinName.ToString().Contains(TEXT("Scale Mode")))
		{
			continue;
		}
		if (!Pin->PinType.PinSubCategoryObject.IsValid())
		{
			continue;
		}

		UEnum* EnumType = Cast<UEnum>(Pin->PinType.PinSubCategoryObject.Get());
		if (!EnumType)
		{
			continue;
		}

		// Find the curve mode enum value
		FString BestModeValue;
		for (int32 i = 0; i < EnumType->NumEnums() - 1; ++i)
		{
			FString DisplayName = EnumType->GetDisplayNameTextByIndex(i).ToString();
			if (DisplayName.Contains(TEXT("Curve"), ESearchCase::IgnoreCase))
			{
				BestModeValue = EnumType->GetNameStringByIndex(i);
				break;
			}
		}

		if (!BestModeValue.IsEmpty() && !Pin->DefaultValue.Equals(BestModeValue))
		{
			Pin->DefaultValue = BestModeValue;

			// Recompile after changing the scale mode
			System->RequestCompile(true);
			System->WaitForCompilationComplete();
		}
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("module_name"), ModuleName);
	Data->SetStringField(TEXT("input_name"), InputName);
	Data->SetStringField(TEXT("curve_type"), CurveType);
	Data->SetNumberField(TEXT("key_count"), KeyCount);
	Data->SetBoolField(TEXT("dynamic_input_used"), true);
	Data->SetBoolField(TEXT("stateless_emitter"), false);

	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Data);
}

#endif // WITH_EDITORONLY_DATA

// ---------------------------------------------------------------------------
// HandleSetNiagaraCurve
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleSetNiagaraCurve(
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

	FString ScriptUsageStr;
	if (!Params->TryGetStringField(TEXT("script_usage"), ScriptUsageStr))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'script_usage' parameter"));
	}

	FString CurveType;
	if (!Params->TryGetStringField(TEXT("curve_type"), CurveType))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'curve_type' parameter"));
	}

	const TArray<TSharedPtr<FJsonValue>>* KeysArrayPtr = nullptr;
	if (!Params->TryGetArrayField(TEXT("keys"), KeysArrayPtr) ||
		!KeysArrayPtr ||
		KeysArrayPtr->Num() == 0)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'keys' array"));
	}

	bool bUsageOk = false;
	ENiagaraScriptUsage Usage = NiagaraHelpers::ParseScriptUsage(ScriptUsageStr, bUsageOk);
	if (!bUsageOk)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Invalid script_usage: '%s'"), *ScriptUsageStr));
	}

	// Load system
	FString LoadError;
	UNiagaraSystem* System = NiagaraHelpers::LoadNiagaraSystem(SystemPath, LoadError);
	if (!System)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(LoadError);
	}

	// Find emitter
	int32 EmitterIdx = INDEX_NONE;
	FString EmitterError;
	FNiagaraEmitterHandle* Handle = NiagaraHelpers::FindEmitterHandle(
		System, EmitterName, EmitterIdx, EmitterError);
	if (!Handle)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(EmitterError);
	}

	// ---- Stateless emitter path ----
	if (IsStatelessEmitter(Handle))
	{
		return HandleStatelessCurve(
			Handle, System, ModuleName, InputName, CurveType, *KeysArrayPtr);
	}

	// ---- Standard emitter path ----
	FVersionedNiagaraEmitterData* EmitterData = NiagaraHelpers::GetEmitterData(Handle);
	if (!EmitterData)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Emitter has no data"));
	}

	UNiagaraGraph* Graph = NiagaraHelpers::GetGraphForUsage(EmitterData, Usage);
	if (!Graph)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No graph for the given script usage"));
	}

	FString FindError;
	UNiagaraNodeFunctionCall* ModuleNode = NiagaraHelpers::FindModuleNode(
		Graph, Usage, ModuleName, FindError);
	if (!ModuleNode)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FindError);
	}

	// Determine whether the target input is a DataInterface pin or a value pin.
	// DI pins have pin category "Class" -- they accept curve DIs directly.
	// Value pins (float, vector, color) need a FromCurve dynamic input wrapper.
	bool bIsDataInterfaceInput = false;
	for (UEdGraphPin* Pin : ModuleNode->Pins)
	{
		if (Pin->Direction == EGPD_Input &&
			Pin->PinName.ToString().Equals(InputName, ESearchCase::IgnoreCase))
		{
			FString PinCategory = Pin->PinType.PinCategory.ToString();
			bIsDataInterfaceInput = PinCategory.Equals(TEXT("Class"), ESearchCase::IgnoreCase);
			break;
		}
	}

	if (bIsDataInterfaceInput)
	{
		return HandleDirectDICurve(
			ModuleNode, Graph, System, ModuleName, InputName, CurveType, *KeysArrayPtr);
	}
	else
	{
		return HandleDynamicInputCurve(
			ModuleNode, Graph, System, ModuleName, InputName, CurveType, *KeysArrayPtr);
	}
#else
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only operation"));
#endif
}
