#include "Commands/EpicUnrealMCPMaterialCommands.h"
#include "Commands/EpicUnrealMCPPropertyUtils.h"

#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionCustom.h"
#include "MaterialEditingLibrary.h"

// ---------------------------------------------------------------------------
// SerializeMaterialExpression
//
// Serialises one expression node to a JSON object for graph inspection commands.
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::SerializeMaterialExpression(
	UMaterialExpression* Expr, int32 Index,
	const TMap<UMaterialExpression*, int32>& ExprIndexMap,
	bool bIncludeAvailablePins,
	const FString& Verbosity)
{
	TSharedPtr<FJsonObject> Node = MakeShared<FJsonObject>();
	Node->SetNumberField(TEXT("index"), Index);

	FString FullClass = Expr->GetClass()->GetName();
	FString ShortType = FullClass;
	ShortType.RemoveFromStart(TEXT("MaterialExpression"));
	Node->SetStringField(TEXT("type"),  ShortType);
	Node->SetStringField(TEXT("class"), FullClass);
	Node->SetNumberField(TEXT("pos_x"), Expr->MaterialExpressionEditorX);
	Node->SetNumberField(TEXT("pos_y"), Expr->MaterialExpressionEditorY);

	// Properties — skip in summary and connections modes
	if (Verbosity != TEXT("summary") && Verbosity != TEXT("connections"))
	{
		TSharedPtr<FJsonObject> Props = MakeShared<FJsonObject>();
		if (UMaterialExpressionCustom* Custom = Cast<UMaterialExpressionCustom>(Expr))
		{
			SerializeCustomHLSLProperties(Custom, Props);
		}
		else
		{
			SerializeGenericExpressionProperties(Expr, Props);
		}
		Node->SetObjectField(TEXT("properties"), Props);
	}

	// Input connections — skip in summary mode only
	if (Verbosity != TEXT("summary"))
	{
		SerializeInputConnections(Expr, ExprIndexMap, Node);
	}

	// Available pins (for get_material_expression_info)
	if (bIncludeAvailablePins)
	{
		SerializeAvailablePins(Expr, Node);
	}

	return Node;
}

// ---------------------------------------------------------------------------
// Custom HLSL node serialization
// ---------------------------------------------------------------------------

void FEpicUnrealMCPMaterialCommands::SerializeCustomHLSLProperties(
	UMaterialExpressionCustom* Custom, TSharedPtr<FJsonObject>& OutProps)
{
	OutProps->SetStringField(TEXT("code"),        Custom->Code);
	OutProps->SetStringField(TEXT("description"), Custom->Description);

	FString OutTypeStr;
	switch (Custom->OutputType)
	{
	case CMOT_Float1:             OutTypeStr = TEXT("float");              break;
	case CMOT_Float2:             OutTypeStr = TEXT("float2");             break;
	case CMOT_Float3:             OutTypeStr = TEXT("float3");             break;
	case CMOT_Float4:             OutTypeStr = TEXT("float4");             break;
	case CMOT_MaterialAttributes: OutTypeStr = TEXT("material_attributes"); break;
	default:                      OutTypeStr = TEXT("float");              break;
	}
	OutProps->SetStringField(TEXT("output_type"), OutTypeStr);

	TArray<TSharedPtr<FJsonValue>> InputNames;
	for (const FCustomInput& Inp : Custom->Inputs)
	{
		InputNames.Add(MakeShared<FJsonValueString>(Inp.InputName.ToString()));
	}
	OutProps->SetArrayField(TEXT("inputs"), InputNames);

	TArray<TSharedPtr<FJsonValue>> OutputDefs;
	for (const FCustomOutput& Out : Custom->AdditionalOutputs)
	{
		auto OutObj = MakeShared<FJsonObject>();
		OutObj->SetStringField(TEXT("name"), Out.OutputName.ToString());
		FString OT;
		switch (Out.OutputType)
		{
		case CMOT_Float1:             OT = TEXT("float");              break;
		case CMOT_Float2:             OT = TEXT("float2");             break;
		case CMOT_Float3:             OT = TEXT("float3");             break;
		case CMOT_Float4:             OT = TEXT("float4");             break;
		case CMOT_MaterialAttributes: OT = TEXT("material_attributes"); break;
		default:                      OT = TEXT("float");              break;
		}
		OutObj->SetStringField(TEXT("type"), OT);
		OutputDefs.Add(MakeShared<FJsonValueObject>(OutObj));
	}
	OutProps->SetArrayField(TEXT("outputs"), OutputDefs);
}

// ---------------------------------------------------------------------------
// Generic expression property serialization via reflection
// ---------------------------------------------------------------------------

void FEpicUnrealMCPMaterialCommands::SerializeGenericExpressionProperties(
	UMaterialExpression* Expr, TSharedPtr<FJsonObject>& OutProps)
{
	if (!Expr)
	{
		return;
	}

	// Get the base class to skip its properties (they are common to ALL expressions)
	UClass* BaseClass = UMaterialExpression::StaticClass();
	UClass* ObjectClass = UObject::StaticClass();

	for (TFieldIterator<FProperty> PropIt(Expr->GetClass()); PropIt; ++PropIt)
	{
		FProperty* Prop = *PropIt;

		// Skip properties owned by UMaterialExpression base or UObject
		UClass* OwnerClass = Prop->GetOwnerClass();
		if (OwnerClass == BaseClass || OwnerClass == ObjectClass)
		{
			continue;
		}

		// Skip FExpressionInput struct properties — those are pin connections, not data
		if (FStructProperty* SP = CastField<FStructProperty>(Prop))
		{
			FName StructName = SP->Struct->GetFName();
			static const TSet<FName> InputStructNames = {
				FName(TEXT("ExpressionInput")),
				FName(TEXT("ExpressionOutput")),
				FName(TEXT("ColorMaterialInput")),
				FName(TEXT("ScalarMaterialInput")),
				FName(TEXT("VectorMaterialInput")),
				FName(TEXT("Vector2MaterialInput")),
				FName(TEXT("MaterialAttributesInput")),
			};
			if (InputStructNames.Contains(StructName))
			{
				continue;
			}
		}

		// Skip transient and deprecated properties
		if (Prop->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated))
		{
			continue;
		}

		// Skip delegate, multicast delegate, and weak object properties
		if (CastField<FDelegateProperty>(Prop) || CastField<FMulticastDelegateProperty>(Prop))
		{
			continue;
		}
		if (CastField<FWeakObjectProperty>(Prop))
		{
			continue;
		}

		// Use the safe serializer from PropertyUtils
		const void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Expr);
		TSharedPtr<FJsonValue> JsonVal = FEpicUnrealMCPPropertyUtils::SafePropertyToJsonValue(Prop, ValuePtr);
		if (JsonVal.IsValid() && JsonVal->Type != EJson::Null)
		{
			OutProps->SetField(Prop->GetName(), JsonVal);
		}
	}
}

// ---------------------------------------------------------------------------
// Input connections serialization
// ---------------------------------------------------------------------------

void FEpicUnrealMCPMaterialCommands::SerializeInputConnections(
	UMaterialExpression* Expr,
	const TMap<UMaterialExpression*, int32>& ExprIndexMap,
	TSharedPtr<FJsonObject>& OutNode)
{
	auto Connections = MakeShared<FJsonObject>();

	if (UMaterialExpressionCustom* Custom = Cast<UMaterialExpressionCustom>(Expr))
	{
		// Custom nodes store connections in Custom->Inputs[i].Input (FExpressionInput)
		for (const FCustomInput& CInp : Custom->Inputs)
		{
			if (!CInp.Input.Expression)
			{
				continue;
			}
			const int32* SrcIdx = ExprIndexMap.Find(CInp.Input.Expression);
			auto ConnObj = MakeShared<FJsonObject>();
			ConnObj->SetNumberField(TEXT("from_node"), SrcIdx ? *SrcIdx : -1);
			ConnObj->SetNumberField(TEXT("from_output_index"), CInp.Input.OutputIndex);

			// Resolve output pin name from connected node's Outputs array
			TArray<FExpressionOutput>& SrcOuts = CInp.Input.Expression->GetOutputs();
			FString FromPin;
			if (SrcOuts.IsValidIndex(CInp.Input.OutputIndex) &&
			    !SrcOuts[CInp.Input.OutputIndex].OutputName.IsNone())
			{
				FromPin = SrcOuts[CInp.Input.OutputIndex].OutputName.ToString();
			}
			ConnObj->SetStringField(TEXT("from_pin"), FromPin);
			Connections->SetObjectField(CInp.InputName.ToString(), ConnObj);
		}
	}
	else
	{
		// Standard nodes: GetInput(i) returns nullptr at the end
		for (int32 i = 0; ; ++i)
		{
			FExpressionInput* Input = Expr->GetInput(i);
			if (!Input)
			{
				break;
			}
			if (!Input->Expression)
			{
				continue;
			}

			FName InputName = Expr->GetInputName(i);
			const int32* SrcIdx = ExprIndexMap.Find(Input->Expression);
			auto ConnObj = MakeShared<FJsonObject>();
			ConnObj->SetNumberField(TEXT("from_node"), SrcIdx ? *SrcIdx : -1);
			ConnObj->SetNumberField(TEXT("from_output_index"), Input->OutputIndex);

			// Resolve output pin name
			TArray<FExpressionOutput>& SrcOuts = Input->Expression->GetOutputs();
			FString FromPin;
			if (SrcOuts.IsValidIndex(Input->OutputIndex) &&
			    !SrcOuts[Input->OutputIndex].OutputName.IsNone())
			{
				FromPin = SrcOuts[Input->OutputIndex].OutputName.ToString();
			}
			ConnObj->SetStringField(TEXT("from_pin"), FromPin);

			FString Key = InputName.IsNone()
				? FString::Printf(TEXT("Input_%d"), i)
				: InputName.ToString();
			Connections->SetObjectField(Key, ConnObj);
		}
	}

	OutNode->SetObjectField(TEXT("input_connections"), Connections);
}

// ---------------------------------------------------------------------------
// Available pins serialization
// ---------------------------------------------------------------------------

void FEpicUnrealMCPMaterialCommands::SerializeAvailablePins(
	UMaterialExpression* Expr, TSharedPtr<FJsonObject>& OutNode)
{
	// Available input pin names via UMaterialEditingLibrary
	TArray<FString> InNames = UMaterialEditingLibrary::GetMaterialExpressionInputNames(Expr);
	TArray<TSharedPtr<FJsonValue>> AvailIn;
	for (const FString& N : InNames)
	{
		AvailIn.Add(MakeShared<FJsonValueString>(N));
	}
	OutNode->SetArrayField(TEXT("available_inputs"), AvailIn);

	// Available output pins from Expr->GetOutputs()
	TArray<FExpressionOutput>& OutPuts = Expr->GetOutputs();
	TArray<TSharedPtr<FJsonValue>> AvailOut;
	for (int32 i = 0; i < OutPuts.Num(); ++i)
	{
		auto OutObj = MakeShared<FJsonObject>();
		OutObj->SetNumberField(TEXT("index"), i);
		FString OutName = OutPuts[i].OutputName.IsNone()
			? (i == 0 ? TEXT("") : FString::Printf(TEXT("Output_%d"), i))
			: OutPuts[i].OutputName.ToString();
		OutObj->SetStringField(TEXT("name"), OutName);
		AvailOut.Add(MakeShared<FJsonValueObject>(OutObj));
	}
	OutNode->SetArrayField(TEXT("available_outputs"), AvailOut);
}
