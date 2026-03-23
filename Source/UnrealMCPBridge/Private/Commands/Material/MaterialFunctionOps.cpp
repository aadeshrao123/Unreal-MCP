#include "Commands/EpicUnrealMCPMaterialCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "MaterialEditorUtils.h"

#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "MaterialEditingLibrary.h"
#include "EditorAssetLibrary.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Factories/MaterialFunctionFactoryNew.h"

// ---------------------------------------------------------------------------
// Enum helpers
// ---------------------------------------------------------------------------

EFunctionInputType FEpicUnrealMCPMaterialCommands::ResolveFunctionInputType(const FString& Name)
{
	FString L = Name.ToLower();

	if (L == TEXT("scalar") || L == TEXT("float"))
	{
		return FunctionInput_Scalar;
	}
	if (L == TEXT("vector2") || L == TEXT("float2"))
	{
		return FunctionInput_Vector2;
	}
	if (L == TEXT("vector3") || L == TEXT("float3"))
	{
		return FunctionInput_Vector3;
	}
	if (L == TEXT("vector4") || L == TEXT("float4"))
	{
		return FunctionInput_Vector4;
	}
	if (L == TEXT("texture2d") || L == TEXT("texture"))
	{
		return FunctionInput_Texture2D;
	}
	if (L == TEXT("texturecube") || L == TEXT("cubemap"))
	{
		return FunctionInput_TextureCube;
	}
	if (L == TEXT("texture2darray"))
	{
		return FunctionInput_Texture2DArray;
	}
	if (L == TEXT("volumetexture") || L == TEXT("texture3d"))
	{
		return FunctionInput_VolumeTexture;
	}
	if (L == TEXT("staticbool"))
	{
		return FunctionInput_StaticBool;
	}
	if (L == TEXT("materialattributes"))
	{
		return FunctionInput_MaterialAttributes;
	}
	if (L == TEXT("textureexternal"))
	{
		return FunctionInput_TextureExternal;
	}
	if (L == TEXT("bool"))
	{
		return FunctionInput_Bool;
	}
	if (L == TEXT("substrate"))
	{
		return FunctionInput_Substrate;
	}

	// Default to Scalar for unrecognised names
	return FunctionInput_Scalar;
}

FString FEpicUnrealMCPMaterialCommands::FunctionInputTypeToString(EFunctionInputType Type)
{
	switch (Type)
	{
	case FunctionInput_Scalar:
		return TEXT("Scalar");
	case FunctionInput_Vector2:
		return TEXT("Vector2");
	case FunctionInput_Vector3:
		return TEXT("Vector3");
	case FunctionInput_Vector4:
		return TEXT("Vector4");
	case FunctionInput_Texture2D:
		return TEXT("Texture2D");
	case FunctionInput_TextureCube:
		return TEXT("TextureCube");
	case FunctionInput_Texture2DArray:
		return TEXT("Texture2DArray");
	case FunctionInput_VolumeTexture:
		return TEXT("VolumeTexture");
	case FunctionInput_StaticBool:
		return TEXT("StaticBool");
	case FunctionInput_MaterialAttributes:
		return TEXT("MaterialAttributes");
	case FunctionInput_TextureExternal:
		return TEXT("TextureExternal");
	case FunctionInput_Bool:
		return TEXT("Bool");
	case FunctionInput_Substrate:
		return TEXT("Substrate");
	default:
		return TEXT("Scalar");
	}
}

// ---------------------------------------------------------------------------
// Internal: Apply FunctionInput properties from JSON to an expression node.
// ---------------------------------------------------------------------------

static void ApplyFunctionInputProperties(
	UMaterialExpressionFunctionInput* FuncIn,
	const TSharedPtr<FJsonObject>& Source)
{
	FString InputName;
	if (Source->TryGetStringField(TEXT("input_name"), InputName))
	{
		FuncIn->InputName = FName(*InputName);
	}

	FString InputTypeStr;
	if (Source->TryGetStringField(TEXT("input_type"), InputTypeStr))
	{
		FuncIn->InputType = FEpicUnrealMCPMaterialCommands::ResolveFunctionInputType(InputTypeStr);
	}

	FString Desc;
	if (Source->TryGetStringField(TEXT("description"), Desc))
	{
		FuncIn->Description = Desc;
	}

	int32 SortP = 0;
	if (Source->TryGetNumberField(TEXT("sort_priority"), SortP))
	{
		FuncIn->SortPriority = SortP;
	}

	bool bUseDefault = false;
	if (Source->TryGetBoolField(TEXT("use_preview_as_default"), bUseDefault))
	{
		FuncIn->bUsePreviewValueAsDefault = bUseDefault ? 1 : 0;
	}

	const TArray<TSharedPtr<FJsonValue>>* PrevArr = nullptr;
	if (Source->TryGetArrayField(TEXT("preview_value"), PrevArr) && PrevArr->Num() >= 1)
	{
		FuncIn->PreviewValue.X = static_cast<float>((*PrevArr)[0]->AsNumber());
		if (PrevArr->Num() >= 2)
		{
			FuncIn->PreviewValue.Y = static_cast<float>((*PrevArr)[1]->AsNumber());
		}
		if (PrevArr->Num() >= 3)
		{
			FuncIn->PreviewValue.Z = static_cast<float>((*PrevArr)[2]->AsNumber());
		}
		if (PrevArr->Num() >= 4)
		{
			FuncIn->PreviewValue.W = static_cast<float>((*PrevArr)[3]->AsNumber());
		}
	}

	FuncIn->ConditionallyGenerateId(false);
#if WITH_EDITOR
	FuncIn->ValidateName();
#endif
}

// ---------------------------------------------------------------------------
// Internal: Apply FunctionOutput properties from JSON to an expression node.
// ---------------------------------------------------------------------------

static void ApplyFunctionOutputProperties(
	UMaterialExpressionFunctionOutput* FuncOut,
	const TSharedPtr<FJsonObject>& Source)
{
	FString OutputName;
	if (Source->TryGetStringField(TEXT("output_name"), OutputName))
	{
		FuncOut->OutputName = FName(*OutputName);
	}

	FString Desc;
	if (Source->TryGetStringField(TEXT("description"), Desc))
	{
		FuncOut->Description = Desc;
	}

	int32 SortP = 0;
	if (Source->TryGetNumberField(TEXT("sort_priority"), SortP))
	{
		FuncOut->SortPriority = SortP;
	}

	FuncOut->ConditionallyGenerateId(false);
#if WITH_EDITOR
	FuncOut->ValidateName();
#endif
}

// ---------------------------------------------------------------------------
// HandleCreateMaterialFunction
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleCreateMaterialFunction(
	const TSharedPtr<FJsonObject>& Params)
{
	FString Name;
	if (!Params->TryGetStringField(TEXT("name"), Name))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name'"));
	}

	FString Path = TEXT("/Game/Materials/Functions");
	Params->TryGetStringField(TEXT("path"), Path);

	FString Description;
	Params->TryGetStringField(TEXT("description"), Description);

	bool bExposeToLibrary = true;
	Params->TryGetBoolField(TEXT("expose_to_library"), bExposeToLibrary);

	// Check if asset already exists to avoid editor overwrite popup
	FString FullAssetPath = Path / Name;
	UMaterialFunction* MF = nullptr;

	if (UEditorAssetLibrary::DoesAssetExist(FullAssetPath))
	{
		bool bForce = false;
		Params->TryGetBoolField(TEXT("force"), bForce);
		if (!bForce)
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Material function already exists: %s. Pass force=true to overwrite."), *FullAssetPath));
		}

		// Force mode: reuse existing asset (avoids GC crash from DeleteAsset during tick)
		MF = Cast<UMaterialFunction>(UEditorAssetLibrary::LoadAsset(FullAssetPath));
		if (MF)
		{
			FMaterialExpressionCollection& Collection = MF->GetExpressionCollection();
			Collection.Empty();
		}
	}

	if (!MF)
	{
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		UMaterialFunctionFactoryNew* Factory = NewObject<UMaterialFunctionFactoryNew>();
		UObject* NewAsset = AssetTools.CreateAsset(Name, Path, UMaterialFunction::StaticClass(), Factory);
		MF = Cast<UMaterialFunction>(NewAsset);
	}

	if (!MF)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Failed to create material function: %s/%s"), *Path, *Name));
	}

	if (!Description.IsEmpty())
	{
		MF->Description = Description;
	}
	MF->bExposeToLibrary = bExposeToLibrary;

	UEditorAssetLibrary::SaveAsset(FullAssetPath);

	auto R = MakeShared<FJsonObject>();
	R->SetBoolField(TEXT("success"), true);
	R->SetStringField(TEXT("path"), FullAssetPath);
	R->SetStringField(TEXT("name"), Name);
	return R;
}

// ---------------------------------------------------------------------------
// HandleGetMaterialFunctionInfo
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleGetMaterialFunctionInfo(
	const TSharedPtr<FJsonObject>& Params)
{
	FString FunctionPath;
	if (!Params->TryGetStringField(TEXT("function_path"), FunctionPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'function_path'"));
	}

	UMaterialFunction* OriginalMF = Cast<UMaterialFunction>(UEditorAssetLibrary::LoadAsset(FunctionPath));
	UMaterialFunction* MF = OriginalMF ? ResolveWorkingMaterialFunction(OriginalMF) : nullptr;
	if (!MF)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Material function not found: %s"), *FunctionPath));
	}

	auto Info = MakeShared<FJsonObject>();
	Info->SetStringField(TEXT("path"), FunctionPath);
	Info->SetStringField(TEXT("description"), MF->Description);
	Info->SetBoolField(TEXT("expose_to_library"), MF->bExposeToLibrary);

	FMaterialExpressionCollection& Collection = MF->GetExpressionCollection();
	Info->SetNumberField(TEXT("num_expressions"), Collection.Expressions.Num());

	// Build expression index map for connection tracing
	TMap<UMaterialExpression*, int32> ExprIndexMap;
	for (int32 i = 0; i < Collection.Expressions.Num(); ++i)
	{
		if (UMaterialExpression* E = Collection.Expressions[i])
		{
			ExprIndexMap.Add(E, i);
		}
	}

	// Enumerate inputs, outputs, and other nodes
	TArray<TSharedPtr<FJsonValue>> InputsArr;
	TArray<TSharedPtr<FJsonValue>> OutputsArr;
	TArray<TSharedPtr<FJsonValue>> NodesArr;

	for (int32 i = 0; i < Collection.Expressions.Num(); ++i)
	{
		UMaterialExpression* Expr = Collection.Expressions[i];
		if (!Expr)
		{
			continue;
		}

		if (UMaterialExpressionFunctionInput* FuncInput = Cast<UMaterialExpressionFunctionInput>(Expr))
		{
			auto InObj = MakeShared<FJsonObject>();
			InObj->SetNumberField(TEXT("node_index"), i);
			InObj->SetStringField(TEXT("name"), FuncInput->InputName.ToString());
			InObj->SetStringField(TEXT("type"), FunctionInputTypeToString(FuncInput->InputType));
			InObj->SetStringField(TEXT("description"), FuncInput->Description);
			InObj->SetNumberField(TEXT("sort_priority"), FuncInput->SortPriority);
			InObj->SetBoolField(TEXT("use_preview_as_default"), FuncInput->bUsePreviewValueAsDefault != 0);
			InObj->SetNumberField(TEXT("pos_x"), FuncInput->MaterialExpressionEditorX);
			InObj->SetNumberField(TEXT("pos_y"), FuncInput->MaterialExpressionEditorY);

			TArray<TSharedPtr<FJsonValue>> PrevArr;
			PrevArr.Add(MakeShared<FJsonValueNumber>(FuncInput->PreviewValue.X));
			PrevArr.Add(MakeShared<FJsonValueNumber>(FuncInput->PreviewValue.Y));
			PrevArr.Add(MakeShared<FJsonValueNumber>(FuncInput->PreviewValue.Z));
			PrevArr.Add(MakeShared<FJsonValueNumber>(FuncInput->PreviewValue.W));
			InObj->SetArrayField(TEXT("preview_value"), PrevArr);

			InputsArr.Add(MakeShared<FJsonValueObject>(InObj));
		}
		else if (UMaterialExpressionFunctionOutput* FuncOutput = Cast<UMaterialExpressionFunctionOutput>(Expr))
		{
			auto OutObj = MakeShared<FJsonObject>();
			OutObj->SetNumberField(TEXT("node_index"), i);
			OutObj->SetStringField(TEXT("name"), FuncOutput->OutputName.ToString());
			OutObj->SetStringField(TEXT("description"), FuncOutput->Description);
			OutObj->SetNumberField(TEXT("sort_priority"), FuncOutput->SortPriority);
			OutObj->SetNumberField(TEXT("pos_x"), FuncOutput->MaterialExpressionEditorX);
			OutObj->SetNumberField(TEXT("pos_y"), FuncOutput->MaterialExpressionEditorY);

			// Check what's connected to this output's A input
			if (FuncOutput->A.Expression)
			{
				const int32* SrcIdx = ExprIndexMap.Find(FuncOutput->A.Expression);

				auto ConnObj = MakeShared<FJsonObject>();
				ConnObj->SetNumberField(TEXT("from_node"), SrcIdx ? *SrcIdx : -1);
				ConnObj->SetNumberField(TEXT("from_output_index"), FuncOutput->A.OutputIndex);

				FString SrcType = FuncOutput->A.Expression->GetClass()->GetName();
				SrcType.RemoveFromStart(TEXT("MaterialExpression"));
				ConnObj->SetStringField(TEXT("from_type"), SrcType);

				OutObj->SetObjectField(TEXT("connected_from"), ConnObj);
			}

			OutputsArr.Add(MakeShared<FJsonValueObject>(OutObj));
		}
		else
		{
			// Regular expression node
			FString ShortType = Expr->GetClass()->GetName();
			ShortType.RemoveFromStart(TEXT("MaterialExpression"));

			auto NodeObj = MakeShared<FJsonObject>();
			NodeObj->SetNumberField(TEXT("index"), i);
			NodeObj->SetStringField(TEXT("type"), ShortType);
			NodeObj->SetNumberField(TEXT("pos_x"), Expr->MaterialExpressionEditorX);
			NodeObj->SetNumberField(TEXT("pos_y"), Expr->MaterialExpressionEditorY);
			NodesArr.Add(MakeShared<FJsonValueObject>(NodeObj));
		}
	}

	// Sort inputs and outputs by SortPriority for consistent display order
	InputsArr.Sort([](const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B)
	{
		return A->AsObject()->GetNumberField(TEXT("sort_priority")) <
		       B->AsObject()->GetNumberField(TEXT("sort_priority"));
	});
	OutputsArr.Sort([](const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B)
	{
		return A->AsObject()->GetNumberField(TEXT("sort_priority")) <
		       B->AsObject()->GetNumberField(TEXT("sort_priority"));
	});

	Info->SetArrayField(TEXT("inputs"), InputsArr);
	Info->SetArrayField(TEXT("outputs"), OutputsArr);
	Info->SetArrayField(TEXT("other_nodes"), NodesArr);

	auto R = MakeShared<FJsonObject>();
	R->SetBoolField(TEXT("success"), true);
	R->SetObjectField(TEXT("info"), Info);
	return R;
}

// ---------------------------------------------------------------------------
// HandleBuildMaterialFunctionGraph
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleBuildMaterialFunctionGraph(
	const TSharedPtr<FJsonObject>& Params)
{
	FString FunctionPath;
	if (!Params->TryGetStringField(TEXT("function_path"), FunctionPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'function_path'"));
	}

	UMaterialFunction* OriginalMF = Cast<UMaterialFunction>(UEditorAssetLibrary::LoadAsset(FunctionPath));
	UMaterialFunction* MF = OriginalMF ? ResolveWorkingMaterialFunction(OriginalMF) : nullptr;
	if (!MF)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Material function not found: %s"), *FunctionPath));
	}

	bool bClearExisting = true;
	Params->TryGetBoolField(TEXT("clear_existing"), bClearExisting);

	const TArray<TSharedPtr<FJsonValue>>* NodesArray = nullptr;
	if (!Params->TryGetArrayField(TEXT("nodes"), NodesArray))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'nodes' array"));
	}

	const TArray<TSharedPtr<FJsonValue>>* ConnectionsArray = nullptr;
	Params->TryGetArrayField(TEXT("connections"), ConnectionsArray);

	if (bClearExisting)
	{
		// Delete all existing expressions and verify the collection is empty
		UMaterialEditingLibrary::DeleteAllMaterialExpressionsInFunction(MF);

		// Force-clear the collection in case DeleteAll didn't fully clean up
		FMaterialExpressionCollection& PreClearCollection = MF->GetExpressionCollection();
		if (PreClearCollection.Expressions.Num() > 0)
		{
			PreClearCollection.Expressions.Empty();
		}
		if (PreClearCollection.ExpressionExecBegin)
		{
			PreClearCollection.ExpressionExecBegin = nullptr;
		}
		if (PreClearCollection.ExpressionExecEnd)
		{
			PreClearCollection.ExpressionExecEnd = nullptr;
		}
		PreClearCollection.EditorComments.Empty();

		MF->MarkPackageDirty();
	}

	// Phase 1: Create all nodes
	TArray<UMaterialExpression*> CreatedNodes;
	CreatedNodes.SetNum(NodesArray->Num());
	TArray<FString> Errors;

	for (int32 Idx = 0; Idx < NodesArray->Num(); ++Idx)
	{
		const TSharedPtr<FJsonObject>& NodeDef = (*NodesArray)[Idx]->AsObject();
		if (!NodeDef.IsValid())
		{
			CreatedNodes[Idx] = nullptr;
			continue;
		}

		FString TypeName = TEXT("Constant");
		NodeDef->TryGetStringField(TEXT("type"), TypeName);

		int32 PosX = -300;
		int32 PosY = 0;
		NodeDef->TryGetNumberField(TEXT("pos_x"), PosX);
		NodeDef->TryGetNumberField(TEXT("pos_y"), PosY);

		UClass* ExprClass = FindExpressionClass(TypeName);
		if (!ExprClass)
		{
			Errors.Add(FString::Printf(TEXT("Node %d: Unknown type '%s'"), Idx, *TypeName));
			CreatedNodes[Idx] = nullptr;
			continue;
		}

		UMaterialExpression* Expr = UMaterialEditingLibrary::CreateMaterialExpressionInFunction(
			MF, ExprClass, PosX, PosY);
		if (!Expr)
		{
			Errors.Add(FString::Printf(TEXT("Node %d: Failed to create '%s'"), Idx, *TypeName));
			CreatedNodes[Idx] = nullptr;
			continue;
		}

		// Handle Custom HLSL node special properties
		bool bIsCustom = (TypeName == TEXT("Custom") || TypeName == TEXT("MaterialExpressionCustom"));
		if (bIsCustom)
		{
			HandleCustomHLSLNode(Expr, NodeDef);
		}

		// Handle FunctionInput special properties
		if (UMaterialExpressionFunctionInput* FuncIn = Cast<UMaterialExpressionFunctionInput>(Expr))
		{
			ApplyFunctionInputProperties(FuncIn, NodeDef);
		}

		// Handle FunctionOutput special properties
		if (UMaterialExpressionFunctionOutput* FuncOut = Cast<UMaterialExpressionFunctionOutput>(Expr))
		{
			ApplyFunctionOutputProperties(FuncOut, NodeDef);
		}

		// Set generic properties via reflection
		const TSharedPtr<FJsonObject>* PropsObj = nullptr;
		if (NodeDef->TryGetObjectField(TEXT("properties"), PropsObj))
		{
			for (const auto& Pair : (*PropsObj)->Values)
			{
				FString PropErr;
				if (!SetExpressionProperty(Expr, Pair.Key, Pair.Value, PropErr))
				{
					Errors.Add(FString::Printf(TEXT("Node %d prop '%s': %s"), Idx, *Pair.Key, *PropErr));
				}
			}
		}

		CreatedNodes[Idx] = Expr;
	}

	// Phase 2: Wire connections
	int32 ConnectionsMade = 0;
	if (ConnectionsArray)
	{
		for (const TSharedPtr<FJsonValue>& ConnVal : *ConnectionsArray)
		{
			const TSharedPtr<FJsonObject>& Conn = ConnVal->AsObject();
			if (!Conn.IsValid())
			{
				continue;
			}

			int32 FromIdx = 0;
			{
				TSharedPtr<FJsonValue> FV = Conn->TryGetField(TEXT("from_node"));
				if (!FV.IsValid() || !TryParseIntFromJson(FV, FromIdx))
				{
					Errors.Add(TEXT("Connection: missing or invalid 'from_node'"));
					continue;
				}
			}

			FString FromPin;
			Conn->TryGetStringField(TEXT("from_pin"), FromPin);

			int32 ToIdx = 0;
			{
				TSharedPtr<FJsonValue> TV = Conn->TryGetField(TEXT("to_node"));
				if (!TV.IsValid() || !TryParseIntFromJson(TV, ToIdx))
				{
					Errors.Add(TEXT("Connection: missing or invalid 'to_node'"));
					continue;
				}
			}

			FString ToPin;
			Conn->TryGetStringField(TEXT("to_pin"), ToPin);

			if (FromIdx < 0 || FromIdx >= CreatedNodes.Num() || !CreatedNodes[FromIdx])
			{
				Errors.Add(FString::Printf(TEXT("Connection: invalid from_node %d"), FromIdx));
				continue;
			}
			if (ToIdx < 0 || ToIdx >= CreatedNodes.Num() || !CreatedNodes[ToIdx])
			{
				Errors.Add(FString::Printf(TEXT("Connection: invalid to_node %d"), ToIdx));
				continue;
			}

			bool bOk = UMaterialEditingLibrary::ConnectMaterialExpressions(
				CreatedNodes[FromIdx], FromPin, CreatedNodes[ToIdx], ToPin);
			if (bOk)
			{
				ConnectionsMade++;
			}
			else
			{
				Errors.Add(FString::Printf(TEXT("Connection failed: node[%d].%s -> node[%d].%s"),
					FromIdx, *FromPin, ToIdx, *ToPin));
			}
		}
	}

	// Phase 3: Update and save
	UMaterialEditingLibrary::UpdateMaterialFunction(MF, nullptr);
	NotifyMaterialFunctionEditorRefresh(OriginalMF);
	UEditorAssetLibrary::SaveAsset(FunctionPath);

	auto R = MakeShared<FJsonObject>();
	R->SetBoolField(TEXT("success"), true);
	R->SetStringField(TEXT("path"), FunctionPath);
	R->SetNumberField(TEXT("nodes_created"), CreatedNodes.Num());
	R->SetNumberField(TEXT("connections_made"), ConnectionsMade);

	if (Errors.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> ErrArr;
		for (const FString& E : Errors)
		{
			ErrArr.Add(MakeShared<FJsonValueString>(E));
		}
		R->SetArrayField(TEXT("errors"), ErrArr);
	}

	return R;
}

// ---------------------------------------------------------------------------
// HandleAddMaterialFunctionInput
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleAddMaterialFunctionInput(
	const TSharedPtr<FJsonObject>& Params)
{
	FString FunctionPath;
	if (!Params->TryGetStringField(TEXT("function_path"), FunctionPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'function_path'"));
	}

	FString InputName;
	if (!Params->TryGetStringField(TEXT("input_name"), InputName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'input_name'"));
	}

	UMaterialFunction* OriginalMF = Cast<UMaterialFunction>(UEditorAssetLibrary::LoadAsset(FunctionPath));
	UMaterialFunction* MF = OriginalMF ? ResolveWorkingMaterialFunction(OriginalMF) : nullptr;
	if (!MF)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Material function not found: %s"), *FunctionPath));
	}

	int32 PosX = -600;
	int32 PosY = 0;
	Params->TryGetNumberField(TEXT("pos_x"), PosX);
	Params->TryGetNumberField(TEXT("pos_y"), PosY);

	UMaterialExpression* Expr = UMaterialEditingLibrary::CreateMaterialExpressionInFunction(
		MF, UMaterialExpressionFunctionInput::StaticClass(), PosX, PosY);
	UMaterialExpressionFunctionInput* FuncIn = Cast<UMaterialExpressionFunctionInput>(Expr);
	if (!FuncIn)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create FunctionInput node"));
	}

	// Apply all properties from Params (input_name, input_type, etc.)
	ApplyFunctionInputProperties(FuncIn, Params);

	// Find the node index in the expression collection
	FMaterialExpressionCollection& Collection = MF->GetExpressionCollection();
	int32 NodeIndex = -1;
	for (int32 i = 0; i < Collection.Expressions.Num(); ++i)
	{
		if (Collection.Expressions[i] == FuncIn)
		{
			NodeIndex = i;
			break;
		}
	}

	UMaterialEditingLibrary::UpdateMaterialFunction(MF, nullptr);
	NotifyMaterialFunctionEditorRefresh(OriginalMF);
	UEditorAssetLibrary::SaveAsset(FunctionPath);

	auto R = MakeShared<FJsonObject>();
	R->SetBoolField(TEXT("success"), true);
	R->SetStringField(TEXT("path"), FunctionPath);
	R->SetNumberField(TEXT("node_index"), NodeIndex);
	R->SetStringField(TEXT("input_name"), FuncIn->InputName.ToString());
	R->SetStringField(TEXT("input_type"), FunctionInputTypeToString(FuncIn->InputType));
	return R;
}

// ---------------------------------------------------------------------------
// HandleAddMaterialFunctionOutput
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleAddMaterialFunctionOutput(
	const TSharedPtr<FJsonObject>& Params)
{
	FString FunctionPath;
	if (!Params->TryGetStringField(TEXT("function_path"), FunctionPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'function_path'"));
	}

	FString OutputName;
	if (!Params->TryGetStringField(TEXT("output_name"), OutputName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'output_name'"));
	}

	UMaterialFunction* OriginalMF = Cast<UMaterialFunction>(UEditorAssetLibrary::LoadAsset(FunctionPath));
	UMaterialFunction* MF = OriginalMF ? ResolveWorkingMaterialFunction(OriginalMF) : nullptr;
	if (!MF)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Material function not found: %s"), *FunctionPath));
	}

	int32 PosX = 200;
	int32 PosY = 0;
	Params->TryGetNumberField(TEXT("pos_x"), PosX);
	Params->TryGetNumberField(TEXT("pos_y"), PosY);

	UMaterialExpression* Expr = UMaterialEditingLibrary::CreateMaterialExpressionInFunction(
		MF, UMaterialExpressionFunctionOutput::StaticClass(), PosX, PosY);
	UMaterialExpressionFunctionOutput* FuncOut = Cast<UMaterialExpressionFunctionOutput>(Expr);
	if (!FuncOut)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create FunctionOutput node"));
	}

	// Apply all properties from Params (output_name, description, etc.)
	ApplyFunctionOutputProperties(FuncOut, Params);

	// Find the node index in the expression collection
	FMaterialExpressionCollection& Collection = MF->GetExpressionCollection();
	int32 NodeIndex = -1;
	for (int32 i = 0; i < Collection.Expressions.Num(); ++i)
	{
		if (Collection.Expressions[i] == FuncOut)
		{
			NodeIndex = i;
			break;
		}
	}

	UMaterialEditingLibrary::UpdateMaterialFunction(MF, nullptr);
	NotifyMaterialFunctionEditorRefresh(OriginalMF);
	UEditorAssetLibrary::SaveAsset(FunctionPath);

	auto R = MakeShared<FJsonObject>();
	R->SetBoolField(TEXT("success"), true);
	R->SetStringField(TEXT("path"), FunctionPath);
	R->SetNumberField(TEXT("node_index"), NodeIndex);
	R->SetStringField(TEXT("output_name"), FuncOut->OutputName.ToString());
	return R;
}

// ---------------------------------------------------------------------------
// HandleSetMaterialFunctionInput
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleSetMaterialFunctionInput(
	const TSharedPtr<FJsonObject>& Params)
{
	FString FunctionPath;
	if (!Params->TryGetStringField(TEXT("function_path"), FunctionPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'function_path'"));
	}

	int32 NodeIndex = -1;
	{
		TSharedPtr<FJsonValue> V = Params->TryGetField(TEXT("node_index"));
		if (!V.IsValid() || !TryParseIntFromJson(V, NodeIndex))
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'node_index'"));
		}
	}

	UMaterialFunction* OriginalMF = Cast<UMaterialFunction>(UEditorAssetLibrary::LoadAsset(FunctionPath));
	UMaterialFunction* MF = OriginalMF ? ResolveWorkingMaterialFunction(OriginalMF) : nullptr;
	if (!MF)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Material function not found: %s"), *FunctionPath));
	}

	FMaterialExpressionCollection& Collection = MF->GetExpressionCollection();
	if (NodeIndex < 0 || NodeIndex >= Collection.Expressions.Num() || !Collection.Expressions[NodeIndex])
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Invalid node_index %d"), NodeIndex));
	}

	UMaterialExpressionFunctionInput* FuncIn = Cast<UMaterialExpressionFunctionInput>(
		Collection.Expressions[NodeIndex]);
	if (!FuncIn)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Node %d is not a FunctionInput"), NodeIndex));
	}

	// Apply updated properties
	FString InputName;
	if (Params->TryGetStringField(TEXT("input_name"), InputName))
	{
		FuncIn->InputName = FName(*InputName);
#if WITH_EDITOR
		FuncIn->ValidateName();
#endif
	}

	FString InputTypeStr;
	if (Params->TryGetStringField(TEXT("input_type"), InputTypeStr))
	{
		FuncIn->InputType = ResolveFunctionInputType(InputTypeStr);
	}

	FString Desc;
	if (Params->TryGetStringField(TEXT("description"), Desc))
	{
		FuncIn->Description = Desc;
	}

	int32 SortP = 0;
	if (Params->TryGetNumberField(TEXT("sort_priority"), SortP))
	{
		FuncIn->SortPriority = SortP;
	}

	bool bUseDefault = false;
	if (Params->TryGetBoolField(TEXT("use_preview_as_default"), bUseDefault))
	{
		FuncIn->bUsePreviewValueAsDefault = bUseDefault ? 1 : 0;
	}

	const TArray<TSharedPtr<FJsonValue>>* PrevArr = nullptr;
	if (Params->TryGetArrayField(TEXT("preview_value"), PrevArr) && PrevArr->Num() >= 1)
	{
		FuncIn->PreviewValue.X = static_cast<float>((*PrevArr)[0]->AsNumber());
		if (PrevArr->Num() >= 2)
		{
			FuncIn->PreviewValue.Y = static_cast<float>((*PrevArr)[1]->AsNumber());
		}
		if (PrevArr->Num() >= 3)
		{
			FuncIn->PreviewValue.Z = static_cast<float>((*PrevArr)[2]->AsNumber());
		}
		if (PrevArr->Num() >= 4)
		{
			FuncIn->PreviewValue.W = static_cast<float>((*PrevArr)[3]->AsNumber());
		}
	}

	UMaterialEditingLibrary::UpdateMaterialFunction(MF, nullptr);
	NotifyMaterialFunctionEditorRefresh(OriginalMF);
	UEditorAssetLibrary::SaveAsset(FunctionPath);

	auto R = MakeShared<FJsonObject>();
	R->SetBoolField(TEXT("success"), true);
	R->SetStringField(TEXT("path"), FunctionPath);
	R->SetNumberField(TEXT("node_index"), NodeIndex);
	R->SetStringField(TEXT("input_name"), FuncIn->InputName.ToString());
	R->SetStringField(TEXT("input_type"), FunctionInputTypeToString(FuncIn->InputType));
	return R;
}

// ---------------------------------------------------------------------------
// HandleSetMaterialFunctionOutput
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleSetMaterialFunctionOutput(
	const TSharedPtr<FJsonObject>& Params)
{
	FString FunctionPath;
	if (!Params->TryGetStringField(TEXT("function_path"), FunctionPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'function_path'"));
	}

	int32 NodeIndex = -1;
	{
		TSharedPtr<FJsonValue> V = Params->TryGetField(TEXT("node_index"));
		if (!V.IsValid() || !TryParseIntFromJson(V, NodeIndex))
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'node_index'"));
		}
	}

	UMaterialFunction* OriginalMF = Cast<UMaterialFunction>(UEditorAssetLibrary::LoadAsset(FunctionPath));
	UMaterialFunction* MF = OriginalMF ? ResolveWorkingMaterialFunction(OriginalMF) : nullptr;
	if (!MF)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Material function not found: %s"), *FunctionPath));
	}

	FMaterialExpressionCollection& Collection = MF->GetExpressionCollection();
	if (NodeIndex < 0 || NodeIndex >= Collection.Expressions.Num() || !Collection.Expressions[NodeIndex])
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Invalid node_index %d"), NodeIndex));
	}

	UMaterialExpressionFunctionOutput* FuncOut = Cast<UMaterialExpressionFunctionOutput>(
		Collection.Expressions[NodeIndex]);
	if (!FuncOut)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Node %d is not a FunctionOutput"), NodeIndex));
	}

	// Apply updated properties
	FString OutputName;
	if (Params->TryGetStringField(TEXT("output_name"), OutputName))
	{
		FuncOut->OutputName = FName(*OutputName);
#if WITH_EDITOR
		FuncOut->ValidateName();
#endif
	}

	FString Desc;
	if (Params->TryGetStringField(TEXT("description"), Desc))
	{
		FuncOut->Description = Desc;
	}

	int32 SortP = 0;
	if (Params->TryGetNumberField(TEXT("sort_priority"), SortP))
	{
		FuncOut->SortPriority = SortP;
	}

	UMaterialEditingLibrary::UpdateMaterialFunction(MF, nullptr);
	NotifyMaterialFunctionEditorRefresh(OriginalMF);
	UEditorAssetLibrary::SaveAsset(FunctionPath);

	auto R = MakeShared<FJsonObject>();
	R->SetBoolField(TEXT("success"), true);
	R->SetStringField(TEXT("path"), FunctionPath);
	R->SetNumberField(TEXT("node_index"), NodeIndex);
	R->SetStringField(TEXT("output_name"), FuncOut->OutputName.ToString());
	return R;
}

// ---------------------------------------------------------------------------
// HandleValidateMaterialFunction
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleValidateMaterialFunction(
	const TSharedPtr<FJsonObject>& Params)
{
	FString FunctionPath;
	if (!Params->TryGetStringField(TEXT("function_path"), FunctionPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'function_path'"));
	}

	UMaterialFunction* OriginalMF = Cast<UMaterialFunction>(UEditorAssetLibrary::LoadAsset(FunctionPath));
	UMaterialFunction* MF = OriginalMF ? ResolveWorkingMaterialFunction(OriginalMF) : nullptr;
	if (!MF)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Material function not found: %s"), *FunctionPath));
	}

	FMaterialExpressionCollection& Collection = MF->GetExpressionCollection();
	const int32 NumExprs = Collection.Expressions.Num();

	TMap<UMaterialExpression*, int32> ExprIndexMap;
	TSet<int32> OutputUsed;
	ExprIndexMap.Reserve(NumExprs);
	for (int32 i = 0; i < NumExprs; ++i)
	{
		if (UMaterialExpression* E = Collection.Expressions[i])
		{
			ExprIndexMap.Add(E, i);
		}
	}
	for (int32 i = 0; i < NumExprs; ++i)
	{
		UMaterialExpression* Expr = Collection.Expressions[i];
		if (!Expr)
		{
			continue;
		}
		for (int32 j = 0; ; ++j)
		{
			FExpressionInput* Input = Expr->GetInput(j);
			if (!Input)
			{
				break;
			}
			if (Input->Expression)
			{
				const int32* Idx = ExprIndexMap.Find(Input->Expression);
				if (Idx)
				{
					OutputUsed.Add(*Idx);
				}
			}
		}
	}

	TArray<TSharedPtr<FJsonValue>> UnconnectedOutputs;
	TArray<TSharedPtr<FJsonValue>> UnusedInputs;
	TArray<TSharedPtr<FJsonValue>> OrphanedNodes;

	for (int32 i = 0; i < NumExprs; ++i)
	{
		UMaterialExpression* Expr = Collection.Expressions[i];
		if (!Expr)
		{
			continue;
		}

		FString ShortType = Expr->GetClass()->GetName();
		ShortType.RemoveFromStart(TEXT("MaterialExpression"));

		auto MakeObj = [&]() -> TSharedPtr<FJsonObject>
		{
			auto Obj = MakeShared<FJsonObject>();
			Obj->SetNumberField(TEXT("index"), i);
			Obj->SetStringField(TEXT("type"), ShortType);
			return Obj;
		};

		if (UMaterialExpressionFunctionOutput* FuncOut2 = Cast<UMaterialExpressionFunctionOutput>(Expr))
		{
			if (!FuncOut2->A.Expression)
			{
				auto Obj = MakeObj();
				Obj->SetStringField(TEXT("name"), FuncOut2->OutputName.ToString());
				UnconnectedOutputs.Add(MakeShared<FJsonValueObject>(Obj));
			}
			continue;
		}

		if (UMaterialExpressionFunctionInput* FuncIn2 = Cast<UMaterialExpressionFunctionInput>(Expr))
		{
			if (!OutputUsed.Contains(i))
			{
				auto Obj = MakeObj();
				Obj->SetStringField(TEXT("name"), FuncIn2->InputName.ToString());
				UnusedInputs.Add(MakeShared<FJsonValueObject>(Obj));
			}
			continue;
		}

		int32 ConnInputs = 0;
		for (int32 j = 0; ; ++j)
		{
			FExpressionInput* Input = Expr->GetInput(j);
			if (!Input)
			{
				break;
			}
			if (Input->Expression)
			{
				ConnInputs++;
			}
		}
		if (ConnInputs == 0 && !OutputUsed.Contains(i))
		{
			OrphanedNodes.Add(MakeShared<FJsonValueObject>(MakeObj()));
		}
	}

	bool bHealthy = UnconnectedOutputs.IsEmpty() && UnusedInputs.IsEmpty() && OrphanedNodes.IsEmpty();

	auto VR = MakeShared<FJsonObject>();
	VR->SetBoolField(TEXT("success"), true);
	VR->SetStringField(TEXT("path"), FunctionPath);
	VR->SetNumberField(TEXT("total_nodes"), NumExprs);
	VR->SetBoolField(TEXT("healthy"), bHealthy);
	VR->SetNumberField(TEXT("unconnected_output_count"), UnconnectedOutputs.Num());
	VR->SetArrayField(TEXT("unconnected_outputs"), UnconnectedOutputs);
	VR->SetNumberField(TEXT("unused_input_count"), UnusedInputs.Num());
	VR->SetArrayField(TEXT("unused_inputs"), UnusedInputs);
	VR->SetNumberField(TEXT("orphaned_count"), OrphanedNodes.Num());
	VR->SetArrayField(TEXT("orphaned"), OrphanedNodes);
	return VR;
}

// ---------------------------------------------------------------------------
// HandleCleanupMaterialFunction
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleCleanupMaterialFunction(
	const TSharedPtr<FJsonObject>& Params)
{
	FString FunctionPath;
	if (!Params->TryGetStringField(TEXT("function_path"), FunctionPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'function_path'"));
	}

	bool bDryRun = false;
	Params->TryGetBoolField(TEXT("dry_run"), bDryRun);

	UMaterialFunction* OriginalMF = Cast<UMaterialFunction>(UEditorAssetLibrary::LoadAsset(FunctionPath));
	UMaterialFunction* MF = OriginalMF ? ResolveWorkingMaterialFunction(OriginalMF) : nullptr;
	if (!MF)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Material function not found: %s"), *FunctionPath));
	}

	FMaterialExpressionCollection& Collection = MF->GetExpressionCollection();
	const int32 NumExprs = Collection.Expressions.Num();

	TMap<UMaterialExpression*, int32> ExprIndexMap;
	TSet<int32> OutputUsed;
	ExprIndexMap.Reserve(NumExprs);
	for (int32 i = 0; i < NumExprs; ++i)
	{
		if (UMaterialExpression* E = Collection.Expressions[i])
		{
			ExprIndexMap.Add(E, i);
		}
	}
	for (int32 i = 0; i < NumExprs; ++i)
	{
		UMaterialExpression* Expr = Collection.Expressions[i];
		if (!Expr)
		{
			continue;
		}
		for (int32 j = 0; ; ++j)
		{
			FExpressionInput* Input = Expr->GetInput(j);
			if (!Input)
			{
				break;
			}
			if (Input->Expression)
			{
				const int32* Idx = ExprIndexMap.Find(Input->Expression);
				if (Idx)
				{
					OutputUsed.Add(*Idx);
				}
			}
		}
	}

	TArray<UMaterialExpression*> ToDelete;
	TArray<TSharedPtr<FJsonValue>> DeletedArr;

	for (int32 i = 0; i < NumExprs; ++i)
	{
		UMaterialExpression* Expr = Collection.Expressions[i];
		if (!Expr)
		{
			continue;
		}

		FString ShortType = Expr->GetClass()->GetName();
		ShortType.RemoveFromStart(TEXT("MaterialExpression"));
		bool bShouldDelete = false;
		FString Reason;

		if (UMaterialExpressionFunctionOutput* FO = Cast<UMaterialExpressionFunctionOutput>(Expr))
		{
			if (!FO->A.Expression)
			{
				bShouldDelete = true;
				Reason = FString::Printf(TEXT("Unconnected output '%s'"), *FO->OutputName.ToString());
			}
		}
		else if (UMaterialExpressionFunctionInput* FI = Cast<UMaterialExpressionFunctionInput>(Expr))
		{
			if (!OutputUsed.Contains(i))
			{
				bShouldDelete = true;
				Reason = FString::Printf(TEXT("Unused input '%s'"), *FI->InputName.ToString());
			}
		}
		else
		{
			int32 ConnInputs = 0;
			for (int32 j = 0; ; ++j)
			{
				FExpressionInput* Input = Expr->GetInput(j);
				if (!Input)
				{
					break;
				}
				if (Input->Expression)
				{
					ConnInputs++;
				}
			}
			if (ConnInputs == 0 && !OutputUsed.Contains(i))
			{
				bShouldDelete = true;
				Reason = TEXT("Orphaned node");
			}
		}

		if (bShouldDelete)
		{
			auto Entry = MakeShared<FJsonObject>();
			Entry->SetNumberField(TEXT("index"), i);
			Entry->SetStringField(TEXT("type"), ShortType);
			Entry->SetStringField(TEXT("reason"), Reason);
			DeletedArr.Add(MakeShared<FJsonValueObject>(Entry));
			ToDelete.Add(Expr);
		}
	}

	int32 DeletedCount = 0;
	if (!bDryRun && ToDelete.Num() > 0)
	{
		for (UMaterialExpression* Expr : ToDelete)
		{
			UMaterialEditingLibrary::DeleteMaterialExpressionInFunction(MF, Expr);
			DeletedCount++;
		}
		UMaterialEditingLibrary::UpdateMaterialFunction(MF, nullptr);
		UEditorAssetLibrary::SaveAsset(FunctionPath);
	}
	else
	{
		DeletedCount = ToDelete.Num();
	}

	auto CR = MakeShared<FJsonObject>();
	CR->SetBoolField(TEXT("success"), true);
	CR->SetStringField(TEXT("path"), FunctionPath);
	CR->SetBoolField(TEXT("dry_run"), bDryRun);
	CR->SetNumberField(TEXT("deleted_count"), DeletedCount);
	CR->SetArrayField(TEXT("deleted_nodes"), DeletedArr);
	return CR;
}
