#include "Commands/EpicUnrealMCPMaterialCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "MaterialEditorUtils.h"

#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionCustom.h"
#include "MaterialEditingLibrary.h"
#include "EditorAssetLibrary.h"

// ---------------------------------------------------------------------------
// HandleAddMaterialExpression
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleAddMaterialExpression(
	const TSharedPtr<FJsonObject>& Params)
{
	FString MaterialPath;
	if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path'"));
	}

	const TSharedPtr<FJsonObject>* NodeDefPtr;
	if (!Params->TryGetObjectField(TEXT("node"), NodeDefPtr))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'node' object"));
	}
	const TSharedPtr<FJsonObject>& NodeDef = *NodeDefPtr;

	UMaterial* OriginalMaterial = Cast<UMaterial>(UEditorAssetLibrary::LoadAsset(MaterialPath));
	UMaterial* Material = OriginalMaterial ? ResolveWorkingMaterial(OriginalMaterial) : nullptr;
	if (!Material)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Material not found: %s"), *MaterialPath));
	}

	FString TypeName = TEXT("Constant");
	NodeDef->TryGetStringField(TEXT("type"), TypeName);
	int32 PosX = -300, PosY = 0;
	NodeDef->TryGetNumberField(TEXT("pos_x"), PosX);
	NodeDef->TryGetNumberField(TEXT("pos_y"), PosY);

	UClass* ExprClass = FindExpressionClass(TypeName);
	if (!ExprClass)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Unknown expression type: '%s'"), *TypeName));
	}

	FMaterialExpressionCollection& Collection = Material->GetExpressionCollection();
	const int32 CountBefore = Collection.Expressions.Num();

	UMaterialExpression* NewExpr = UMaterialEditingLibrary::CreateMaterialExpression(
		Material, ExprClass, PosX, PosY);
	if (!NewExpr)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Failed to create expression '%s'"), *TypeName));
	}

	const bool bIsCustom = (TypeName == TEXT("Custom") || TypeName == TEXT("MaterialExpressionCustom"));
	if (bIsCustom)
	{
		HandleCustomHLSLNode(NewExpr, NodeDef);
	}

	TArray<FString> PropErrors;
	const TSharedPtr<FJsonObject>* PropsObj;
	if (NodeDef->TryGetObjectField(TEXT("properties"), PropsObj))
	{
		for (const auto& Pair : (*PropsObj)->Values)
		{
			FString PropErr;
			if (!SetExpressionProperty(NewExpr, Pair.Key, Pair.Value, PropErr))
			{
				PropErrors.Add(FString::Printf(TEXT("'%s': %s"), *Pair.Key, *PropErr));
			}
		}
	}

	// Find new node's index — UE appends to end but search to be safe
	int32 NewIndex = -1;
	for (int32 i = Collection.Expressions.Num() - 1; i >= CountBefore; --i)
	{
		if (Collection.Expressions[i] == NewExpr)
		{
			NewIndex = i;
			break;
		}
	}
	if (NewIndex == -1)
	{
		for (int32 i = 0; i < Collection.Expressions.Num(); ++i)
		{
			if (Collection.Expressions[i] == NewExpr)
			{
				NewIndex = i;
				break;
			}
		}
	}

	NotifyMaterialEditorRefresh(OriginalMaterial);
	if (Material == OriginalMaterial)
	{
		UMaterialEditingLibrary::RecompileMaterial(Material);
		UEditorAssetLibrary::SaveAsset(MaterialPath);
	}

	auto R = MakeShared<FJsonObject>();
	R->SetBoolField(TEXT("success"), true);
	R->SetStringField(TEXT("path"), MaterialPath);
	R->SetNumberField(TEXT("node_index"), NewIndex);
	R->SetStringField(TEXT("type"), TypeName);
	TArray<TSharedPtr<FJsonValue>> ErrArr;
	for (const FString& E : PropErrors)
	{
		ErrArr.Add(MakeShared<FJsonValueString>(E));
	}
	R->SetArrayField(TEXT("property_errors"), ErrArr);
	return R;
}

// ---------------------------------------------------------------------------
// HandleSetMaterialExpressionProperty
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleSetMaterialExpressionProperty(
	const TSharedPtr<FJsonObject>& Params)
{
	FString MaterialPath;
	if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path'"));
	}

	int32 NodeIndex = -1;
	{
		TSharedPtr<FJsonValue> V = Params->TryGetField(TEXT("node_index"));
		if (!V.IsValid() || !TryParseIntFromJson(V, NodeIndex))
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'node_index'"));
		}
	}

	UMaterial* OriginalMaterial = Cast<UMaterial>(UEditorAssetLibrary::LoadAsset(MaterialPath));
	UMaterial* Material = OriginalMaterial ? ResolveWorkingMaterial(OriginalMaterial) : nullptr;
	if (!Material)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Material not found: %s"), *MaterialPath));
	}

	FMaterialExpressionCollection& Collection = Material->GetExpressionCollection();
	if (NodeIndex < 0 || NodeIndex >= Collection.Expressions.Num() || !Collection.Expressions[NodeIndex])
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Invalid node_index %d"), NodeIndex));
	}

	UMaterialExpression* Expr = Collection.Expressions[NodeIndex];

	// Allow passing Custom HLSL fields directly on the params object or via a "node" sub-object
	if (Cast<UMaterialExpressionCustom>(Expr))
	{
		// When using property_name/property_value mode with a Custom-specific field,
		// promote it to a top-level field so HandleCustomHLSLNode can find it.
		FString PropName;
		TSharedPtr<FJsonValue> PropVal = Params->TryGetField(TEXT("property_value"));
		if (Params->TryGetStringField(TEXT("property_name"), PropName) && PropVal.IsValid())
		{
			static const TSet<FString> CustomFields = {
				TEXT("code"), TEXT("description"), TEXT("output_type"),
				TEXT("inputs"), TEXT("add_inputs"), TEXT("outputs")
			};
			if (CustomFields.Contains(PropName))
			{
				auto NodeDef = MakeShared<FJsonObject>();
				NodeDef->SetField(PropName, PropVal);
				HandleCustomHLSLNode(Expr, NodeDef);
				// Mark as handled so SetExpressionProperty doesn't also try it
				Params->RemoveField(TEXT("property_name"));
			}
		}

		// Also handle direct top-level fields or "node" sub-object
		const TSharedPtr<FJsonObject>* NodeDefPtr;
		if (Params->TryGetObjectField(TEXT("node"), NodeDefPtr))
		{
			HandleCustomHLSLNode(Expr, *NodeDefPtr);
		}
		else
		{
			HandleCustomHLSLNode(Expr, Params);
		}
	}

	TArray<FString> PropErrors;

	// Single-property mode: property_name + property_value
	FString SinglePropName;
	TSharedPtr<FJsonValue> SinglePropValue = Params->TryGetField(TEXT("property_value"));
	if (Params->TryGetStringField(TEXT("property_name"), SinglePropName) && SinglePropValue.IsValid())
	{
		FString PropErr;
		if (!SetExpressionProperty(Expr, SinglePropName, SinglePropValue, PropErr))
		{
			PropErrors.Add(FString::Printf(TEXT("'%s': %s"), *SinglePropName, *PropErr));
		}
	}

	// Batch-property mode: properties object { "PropName": value, ... }
	const TSharedPtr<FJsonObject>* PropsObj;
	if (Params->TryGetObjectField(TEXT("properties"), PropsObj))
	{
		for (const auto& Pair : (*PropsObj)->Values)
		{
			FString PropErr;
			if (!SetExpressionProperty(Expr, Pair.Key, Pair.Value, PropErr))
			{
				PropErrors.Add(FString::Printf(TEXT("'%s': %s"), *Pair.Key, *PropErr));
			}
		}
	}

	NotifyMaterialEditorRefresh(OriginalMaterial);
	if (Material == OriginalMaterial)
	{
		UMaterialEditingLibrary::RecompileMaterial(Material);
		UEditorAssetLibrary::SaveAsset(MaterialPath);
	}

	auto R = MakeShared<FJsonObject>();
	R->SetBoolField(TEXT("success"), PropErrors.IsEmpty());
	R->SetStringField(TEXT("path"), MaterialPath);
	R->SetNumberField(TEXT("node_index"), NodeIndex);
	TArray<TSharedPtr<FJsonValue>> ErrArr;
	for (const FString& E : PropErrors)
	{
		ErrArr.Add(MakeShared<FJsonValueString>(E));
	}
	R->SetArrayField(TEXT("property_errors"), ErrArr);
	return R;
}

// ---------------------------------------------------------------------------
// HandleMoveMaterialExpression
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleMoveMaterialExpression(
	const TSharedPtr<FJsonObject>& Params)
{
	FString MaterialPath;
	if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path'"));
	}

	int32 NodeIndex = -1;
	{
		TSharedPtr<FJsonValue> V = Params->TryGetField(TEXT("node_index"));
		if (!V.IsValid() || !TryParseIntFromJson(V, NodeIndex))
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'node_index'"));
		}
	}

	int32 PosX = 0, PosY = 0;
	bool bHasX = Params->TryGetNumberField(TEXT("pos_x"), PosX);
	bool bHasY = Params->TryGetNumberField(TEXT("pos_y"), PosY);
	if (!bHasX && !bHasY)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Provide at least pos_x or pos_y"));
	}

	UMaterial* OriginalMaterial = Cast<UMaterial>(UEditorAssetLibrary::LoadAsset(MaterialPath));
	UMaterial* Material = OriginalMaterial ? ResolveWorkingMaterial(OriginalMaterial) : nullptr;
	if (!Material)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Material not found: %s"), *MaterialPath));
	}

	FMaterialExpressionCollection& Collection = Material->GetExpressionCollection();
	if (NodeIndex < 0 || NodeIndex >= Collection.Expressions.Num() || !Collection.Expressions[NodeIndex])
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Invalid node_index %d"), NodeIndex));
	}

	UMaterialExpression* Expr = Collection.Expressions[NodeIndex];
	if (bHasX)
	{
		Expr->MaterialExpressionEditorX = PosX;
	}
	if (bHasY)
	{
		Expr->MaterialExpressionEditorY = PosY;
	}

	NotifyMaterialEditorRefresh(OriginalMaterial);
	if (Material == OriginalMaterial)
	{
		UMaterialEditingLibrary::RecompileMaterial(Material);
		UEditorAssetLibrary::SaveAsset(MaterialPath);
	}

	auto R = MakeShared<FJsonObject>();
	R->SetBoolField(TEXT("success"), true);
	R->SetNumberField(TEXT("node_index"), NodeIndex);
	R->SetNumberField(TEXT("pos_x"), Expr->MaterialExpressionEditorX);
	R->SetNumberField(TEXT("pos_y"), Expr->MaterialExpressionEditorY);
	return R;
}

// ---------------------------------------------------------------------------
// HandleDuplicateMaterialExpression
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleDuplicateMaterialExpression(
	const TSharedPtr<FJsonObject>& Params)
{
	FString MaterialPath;
	if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path'"));
	}

	int32 NodeIndex = -1;
	{
		TSharedPtr<FJsonValue> V = Params->TryGetField(TEXT("node_index"));
		if (!V.IsValid() || !TryParseIntFromJson(V, NodeIndex))
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'node_index'"));
		}
	}

	int32 OffsetX = 150, OffsetY = 0;
	Params->TryGetNumberField(TEXT("offset_x"), OffsetX);
	Params->TryGetNumberField(TEXT("offset_y"), OffsetY);

	UMaterial* OriginalMaterial = Cast<UMaterial>(UEditorAssetLibrary::LoadAsset(MaterialPath));
	UMaterial* Material = OriginalMaterial ? ResolveWorkingMaterial(OriginalMaterial) : nullptr;
	if (!Material)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Material not found: %s"), *MaterialPath));
	}

	FMaterialExpressionCollection& Collection = Material->GetExpressionCollection();
	if (NodeIndex < 0 || NodeIndex >= Collection.Expressions.Num() || !Collection.Expressions[NodeIndex])
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Invalid node_index %d"), NodeIndex));
	}

	UMaterialExpression* Source = Collection.Expressions[NodeIndex];
	const int32 CountBefore = Collection.Expressions.Num();

	UMaterialExpression* NewExpr = UMaterialEditingLibrary::DuplicateMaterialExpression(
		Material, nullptr, Source);
	if (!NewExpr)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to duplicate expression"));
	}

	NewExpr->MaterialExpressionEditorX = Source->MaterialExpressionEditorX + OffsetX;
	NewExpr->MaterialExpressionEditorY = Source->MaterialExpressionEditorY + OffsetY;

	int32 NewIndex = -1;
	for (int32 i = Collection.Expressions.Num() - 1; i >= CountBefore; --i)
	{
		if (Collection.Expressions[i] == NewExpr)
		{
			NewIndex = i;
			break;
		}
	}
	if (NewIndex == -1)
	{
		for (int32 i = 0; i < Collection.Expressions.Num(); ++i)
		{
			if (Collection.Expressions[i] == NewExpr)
			{
				NewIndex = i;
				break;
			}
		}
	}

	NotifyMaterialEditorRefresh(OriginalMaterial);
	if (Material == OriginalMaterial)
	{
		UMaterialEditingLibrary::RecompileMaterial(Material);
		UEditorAssetLibrary::SaveAsset(MaterialPath);
	}

	FString TypeStr = Source->GetClass()->GetName();
	TypeStr.RemoveFromStart(TEXT("MaterialExpression"));

	auto R = MakeShared<FJsonObject>();
	R->SetBoolField(TEXT("success"), true);
	R->SetNumberField(TEXT("source_index"), NodeIndex);
	R->SetNumberField(TEXT("new_index"), NewIndex);
	R->SetStringField(TEXT("type"), TypeStr);
	R->SetNumberField(TEXT("pos_x"), NewExpr->MaterialExpressionEditorX);
	R->SetNumberField(TEXT("pos_y"), NewExpr->MaterialExpressionEditorY);
	return R;
}

// ---------------------------------------------------------------------------
// HandleDeleteMaterialExpression
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleDeleteMaterialExpression(
	const TSharedPtr<FJsonObject>& Params)
{
	FString MaterialPath;
	if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path'"));
	}

	int32 NodeIndex = -1;
	{
		TSharedPtr<FJsonValue> V = Params->TryGetField(TEXT("node_index"));
		if (!V.IsValid() || !TryParseIntFromJson(V, NodeIndex))
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'node_index'"));
		}
	}

	UMaterial* OriginalMaterial = Cast<UMaterial>(UEditorAssetLibrary::LoadAsset(MaterialPath));
	UMaterial* Material = OriginalMaterial ? ResolveWorkingMaterial(OriginalMaterial) : nullptr;
	if (!Material)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Material not found: %s"), *MaterialPath));
	}

	FMaterialExpressionCollection& Collection = Material->GetExpressionCollection();
	if (NodeIndex < 0 || NodeIndex >= Collection.Expressions.Num() || !Collection.Expressions[NodeIndex])
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Invalid node_index %d (material has %d nodes)"),
				NodeIndex, Collection.Expressions.Num()));
	}

	UMaterialExpression* Expr = Collection.Expressions[NodeIndex];
	FString DeletedType = Expr->GetClass()->GetName();
	DeletedType.RemoveFromStart(TEXT("MaterialExpression"));

	UMaterialEditingLibrary::DeleteMaterialExpression(Material, Expr);
	RebuildMaterialEditorGraph(OriginalMaterial);
	if (Material == OriginalMaterial)
	{
		UMaterialEditingLibrary::RecompileMaterial(Material);
		UEditorAssetLibrary::SaveAsset(MaterialPath);
	}

	auto R = MakeShared<FJsonObject>();
	R->SetBoolField(TEXT("success"), true);
	R->SetNumberField(TEXT("deleted_index"), NodeIndex);
	R->SetStringField(TEXT("deleted_type"), DeletedType);
	R->SetStringField(TEXT("note"),
		TEXT("Indices of remaining nodes may have shifted — re-query with get_material_graph_nodes"));
	return R;
}

// ---------------------------------------------------------------------------
// HandleConnectMaterialExpressions
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleConnectMaterialExpressions(
	const TSharedPtr<FJsonObject>& Params)
{
	FString MaterialPath;
	if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path'"));
	}

	// from_node: int (also accept numeric strings for robustness)
	int32 FromIdx = 0;
	{
		TSharedPtr<FJsonValue> FV = Params->TryGetField(TEXT("from_node"));
		if (!FV.IsValid() || !TryParseIntFromJson(FV, FromIdx))
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'from_node'"));
		}
	}

	FString FromPin;
	Params->TryGetStringField(TEXT("from_pin"), FromPin);

	FString ToPin;
	if (!Params->TryGetStringField(TEXT("to_pin"), ToPin))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'to_pin'"));
	}

	// to_node: str "material" -> material output; numeric string or int -> node index
	bool bToMaterial = false;
	int32 ToIdx = 0;
	{
		TSharedPtr<FJsonValue> TV = Params->TryGetField(TEXT("to_node"));
		if (!TV.IsValid())
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'to_node'"));
		}

		if (TV->Type == EJson::String)
		{
			FString S = TV->AsString();
			if (S.Equals(TEXT("material"), ESearchCase::IgnoreCase))
			{
				bToMaterial = true;
			}
			else if (S.IsNumeric())
			{
				ToIdx = FCString::Atoi(*S);
			}
			else
			{
				return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
					FString::Printf(TEXT("Invalid to_node '%s': use a node index or \"material\""), *S));
			}
		}
		else if (!TryParseIntFromJson(TV, ToIdx))
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				TEXT("to_node must be a node index or \"material\""));
		}
	}

	UMaterial* OriginalMaterial = Cast<UMaterial>(UEditorAssetLibrary::LoadAsset(MaterialPath));
	UMaterial* Material = OriginalMaterial ? ResolveWorkingMaterial(OriginalMaterial) : nullptr;
	if (!Material)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Material not found: %s"), *MaterialPath));
	}

	FMaterialExpressionCollection& Collection = Material->GetExpressionCollection();

	if (FromIdx < 0 || FromIdx >= Collection.Expressions.Num() || !Collection.Expressions[FromIdx])
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Invalid from_node %d (material has %d nodes)"),
				FromIdx, Collection.Expressions.Num()));
	}
	UMaterialExpression* FromExpr = Collection.Expressions[FromIdx];

	bool bConnected = false;
	if (bToMaterial)
	{
		EMaterialProperty MatProp = ResolveMaterialProperty(ToPin);
		if (MatProp == MP_MAX)
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Unknown material property '%s'"), *ToPin));
		}
		bConnected = UMaterialEditingLibrary::ConnectMaterialProperty(FromExpr, FromPin, MatProp);
	}
	else
	{
		if (ToIdx < 0 || ToIdx >= Collection.Expressions.Num() || !Collection.Expressions[ToIdx])
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Invalid to_node %d (material has %d nodes)"),
					ToIdx, Collection.Expressions.Num()));
		}
		bConnected = UMaterialEditingLibrary::ConnectMaterialExpressions(
			FromExpr, FromPin, Collection.Expressions[ToIdx], ToPin);
	}

	if (!bConnected)
	{
		FString ToDesc = bToMaterial
			? FString::Printf(TEXT("material.%s"), *ToPin)
			: FString::Printf(TEXT("node[%d].%s"), ToIdx, *ToPin);
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(
				TEXT("Connection failed: node[%d].%s -> %s — check pin names with get_material_expression_info"),
				FromIdx, *FromPin, *ToDesc));
	}

	NotifyMaterialEditorRefresh(OriginalMaterial);
	if (Material == OriginalMaterial)
	{
		UMaterialEditingLibrary::RecompileMaterial(Material);
		UEditorAssetLibrary::SaveAsset(MaterialPath);
	}

	FString ToDesc = bToMaterial
		? FString::Printf(TEXT("material.%s"), *ToPin)
		: FString::Printf(TEXT("node[%d].%s"), ToIdx, *ToPin);

	auto R = MakeShared<FJsonObject>();
	R->SetBoolField(TEXT("success"), true);
	R->SetStringField(TEXT("path"), MaterialPath);
	R->SetStringField(TEXT("connection"),
		FString::Printf(TEXT("node[%d].%s -> %s"), FromIdx, *FromPin, *ToDesc));
	return R;
}

// ---------------------------------------------------------------------------
// HandleLayoutMaterialExpressions
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleLayoutMaterialExpressions(
	const TSharedPtr<FJsonObject>& Params)
{
	FString MaterialPath;
	if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path'"));
	}

	UMaterial* OriginalMaterial = Cast<UMaterial>(UEditorAssetLibrary::LoadAsset(MaterialPath));
	UMaterial* Material = OriginalMaterial ? ResolveWorkingMaterial(OriginalMaterial) : nullptr;
	if (!Material)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Material not found: %s"), *MaterialPath));
	}

	UMaterialEditingLibrary::LayoutMaterialExpressions(Material);
	RebuildMaterialEditorGraph(OriginalMaterial);
	if (Material == OriginalMaterial)
	{
		UMaterialEditingLibrary::RecompileMaterial(Material);
		UEditorAssetLibrary::SaveAsset(MaterialPath);
	}

	auto R = MakeShared<FJsonObject>();
	R->SetBoolField(TEXT("success"), true);
	R->SetStringField(TEXT("path"), MaterialPath);
	R->SetStringField(TEXT("message"), TEXT("Auto-laid out and saved"));
	return R;
}
