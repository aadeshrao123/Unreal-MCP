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

		// Build actionable error message with available pin lists
		FString ErrorMsg = FString::Printf(
			TEXT("Connection failed: node[%d].%s -> %s."), FromIdx, *FromPin, *ToDesc);

		// Available outputs on source node
		TArray<FExpressionOutput>& SrcOutputs = FromExpr->GetOutputs();
		TArray<FString> SrcOutNames;
		for (int32 oi = 0; oi < SrcOutputs.Num(); ++oi)
		{
			FString Name = SrcOutputs[oi].OutputName.IsNone()
				? TEXT("''")
				: FString::Printf(TEXT("'%s'"), *SrcOutputs[oi].OutputName.ToString());
			SrcOutNames.Add(Name);
		}
		FString SrcType = FromExpr->GetClass()->GetName();
		SrcType.RemoveFromStart(TEXT("MaterialExpression"));
		ErrorMsg += FString::Printf(TEXT("\nAvailable outputs on node[%d] (%s): [%s]."),
			FromIdx, *SrcType, *FString::Join(SrcOutNames, TEXT(", ")));

		// Available inputs on target node (or material properties)
		if (!bToMaterial)
		{
			UMaterialExpression* ToExpr = Collection.Expressions[ToIdx];
			TArray<FString> InNames = UMaterialEditingLibrary::GetMaterialExpressionInputNames(ToExpr);
			TArray<FString> QuotedNames;
			for (const FString& N : InNames)
			{
				QuotedNames.Add(FString::Printf(TEXT("'%s'"), *N));
			}
			FString ToType = ToExpr->GetClass()->GetName();
			ToType.RemoveFromStart(TEXT("MaterialExpression"));
			ErrorMsg += FString::Printf(TEXT("\nAvailable inputs on node[%d] (%s): [%s]."),
				ToIdx, *ToType, *FString::Join(QuotedNames, TEXT(", ")));
		}
		else
		{
			ErrorMsg += TEXT("\nValid material properties: BaseColor, Metallic, Roughness, EmissiveColor, Opacity, OpacityMask, Normal, Specular, AmbientOcclusion, WorldPositionOffset, SubsurfaceColor, Refraction, PixelDepthOffset, ShadingModel, Anisotropy, Tangent, CustomData0, CustomData1, Displacement.");
		}

		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(ErrorMsg);
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

// ---------------------------------------------------------------------------
// HandleDisconnectMaterialExpression
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleDisconnectMaterialExpression(
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

	FString InputPin;
	if (!Params->TryGetStringField(TEXT("input_pin"), InputPin))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'input_pin'"));
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
	bool bDisconnected = false;

	// Handle Custom HLSL nodes
	if (UMaterialExpressionCustom* Custom = Cast<UMaterialExpressionCustom>(Expr))
	{
		for (FCustomInput& CInp : Custom->Inputs)
		{
			if (CInp.InputName.ToString() == InputPin)
			{
				CInp.Input.Expression = nullptr;
				CInp.Input.OutputIndex = 0;
				bDisconnected = true;
				break;
			}
		}
	}
	else
	{
		// Standard nodes: iterate inputs by name
		for (int32 i = 0; ; ++i)
		{
			FExpressionInput* Input = Expr->GetInput(i);
			if (!Input)
			{
				break;
			}

			FName Name = Expr->GetInputName(i);
			FString NameStr = Name.IsNone() ? FString::Printf(TEXT("Input_%d"), i) : Name.ToString();
			if (NameStr == InputPin)
			{
				Input->Expression = nullptr;
				Input->OutputIndex = 0;
				bDisconnected = true;
				break;
			}
		}
	}

	if (!bDisconnected)
	{
		// Build list of available input names for the error message
		TArray<FString> AvailNames = UMaterialEditingLibrary::GetMaterialExpressionInputNames(Expr);
		TArray<FString> QuotedNames;
		for (const FString& N : AvailNames)
		{
			QuotedNames.Add(FString::Printf(TEXT("'%s'"), *N));
		}
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Input pin '%s' not found. Available inputs: [%s]"),
				*InputPin, *FString::Join(QuotedNames, TEXT(", "))));
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
	R->SetNumberField(TEXT("node_index"), NodeIndex);
	R->SetStringField(TEXT("disconnected_pin"), InputPin);
	return R;
}

// ---------------------------------------------------------------------------
// HandleCleanupMaterialGraph
//
// Deletes orphaned and optionally dead-end nodes from the material graph.
// mode: "orphaned" (default) — only truly disconnected nodes
//       "dead_ends" — also delete nodes whose output goes nowhere
//       "all" — both orphaned and dead-ends
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleCleanupMaterialGraph(
	const TSharedPtr<FJsonObject>& Params)
{
	FString MaterialPath;
	if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path'"));
	}

	FString Mode = TEXT("orphaned");
	Params->TryGetStringField(TEXT("mode"), Mode);

	bool bDryRun = false;
	Params->TryGetBoolField(TEXT("dry_run"), bDryRun);

	UMaterial* OriginalMaterial = Cast<UMaterial>(UEditorAssetLibrary::LoadAsset(MaterialPath));
	UMaterial* Material = OriginalMaterial ? ResolveWorkingMaterial(OriginalMaterial) : nullptr;
	if (!Material)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Material not found: %s"), *MaterialPath));
	}

	FMaterialExpressionCollection& Collection = Material->GetExpressionCollection();
	const int32 NumExprs = Collection.Expressions.Num();

	// Build expression-to-index map
	TMap<UMaterialExpression*, int32> ExprIndexMap;
	ExprIndexMap.Reserve(NumExprs);
	for (int32 i = 0; i < NumExprs; ++i)
	{
		if (UMaterialExpression* E = Collection.Expressions[i])
		{
			ExprIndexMap.Add(E, i);
		}
	}

	// Build set of expressions whose output is consumed
	TSet<int32> OutputUsed;

	// Scan all node inputs
	for (int32 i = 0; i < NumExprs; ++i)
	{
		UMaterialExpression* Expr = Collection.Expressions[i];
		if (!Expr)
		{
			continue;
		}

		if (UMaterialExpressionCustom* Custom = Cast<UMaterialExpressionCustom>(Expr))
		{
			for (const FCustomInput& CInp : Custom->Inputs)
			{
				if (CInp.Input.Expression)
				{
					const int32* Idx = ExprIndexMap.Find(CInp.Input.Expression);
					if (Idx)
					{
						OutputUsed.Add(*Idx);
					}
				}
			}
		}
		else
		{
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
	}

	// Check material output connections
	static const EMaterialProperty AllProps[] = {
		MP_BaseColor, MP_Metallic, MP_Roughness, MP_EmissiveColor,
		MP_Opacity, MP_OpacityMask, MP_Normal, MP_Specular,
		MP_AmbientOcclusion, MP_WorldPositionOffset, MP_SubsurfaceColor, MP_Refraction,
		MP_PixelDepthOffset, MP_ShadingModel, MP_Anisotropy, MP_Tangent,
		MP_CustomData0, MP_CustomData1, MP_SurfaceThickness, MP_Displacement, MP_FrontMaterial
	};
	for (EMaterialProperty Prop : AllProps)
	{
		UMaterialExpression* Node = UMaterialEditingLibrary::GetMaterialPropertyInputNode(Material, Prop);
		if (Node)
		{
			const int32* Idx = ExprIndexMap.Find(Node);
			if (Idx)
			{
				OutputUsed.Add(*Idx);
			}
		}
	}

	// Determine which nodes to delete
	bool bDeleteOrphaned = (Mode == TEXT("orphaned") || Mode == TEXT("all"));
	bool bDeleteDeadEnds = (Mode == TEXT("dead_ends") || Mode == TEXT("all"));

	TArray<int32> ToDelete;
	TArray<TSharedPtr<FJsonValue>> DeletedArr;

	for (int32 i = 0; i < NumExprs; ++i)
	{
		UMaterialExpression* Expr = Collection.Expressions[i];
		if (!Expr)
		{
			continue;
		}

		bool bOutputConsumed = OutputUsed.Contains(i);

		// Count connected inputs
		int32 ConnectedInputs = 0;
		if (UMaterialExpressionCustom* Custom = Cast<UMaterialExpressionCustom>(Expr))
		{
			for (const FCustomInput& CInp : Custom->Inputs)
			{
				if (CInp.Input.Expression)
				{
					ConnectedInputs++;
				}
			}
		}
		else
		{
			for (int32 j = 0; ; ++j)
			{
				FExpressionInput* Input = Expr->GetInput(j);
				if (!Input)
				{
					break;
				}
				if (Input->Expression)
				{
					ConnectedInputs++;
				}
			}
		}

		bool bIsOrphaned = (ConnectedInputs == 0 && !bOutputConsumed);
		bool bIsDeadEnd = (ConnectedInputs > 0 && !bOutputConsumed);

		bool bShouldDelete = false;
		FString Reason;

		if (bIsOrphaned && bDeleteOrphaned)
		{
			bShouldDelete = true;
			Reason = TEXT("orphaned");
		}
		else if (bIsDeadEnd && bDeleteDeadEnds)
		{
			bShouldDelete = true;
			Reason = TEXT("dead_end");
		}

		if (bShouldDelete)
		{
			FString ShortType = Expr->GetClass()->GetName();
			ShortType.RemoveFromStart(TEXT("MaterialExpression"));

			auto Entry = MakeShared<FJsonObject>();
			Entry->SetNumberField(TEXT("index"), i);
			Entry->SetStringField(TEXT("type"), ShortType);
			Entry->SetStringField(TEXT("reason"), Reason);
			DeletedArr.Add(MakeShared<FJsonValueObject>(Entry));
			ToDelete.Add(i);
		}
	}

	// Actually delete (in reverse order to preserve indices for earlier deletions)
	int32 DeletedCount = 0;
	if (!bDryRun)
	{
		for (int32 k = ToDelete.Num() - 1; k >= 0; --k)
		{
			int32 Idx = ToDelete[k];
			if (Idx < Collection.Expressions.Num() && Collection.Expressions[Idx])
			{
				UMaterialEditingLibrary::DeleteMaterialExpression(Material, Collection.Expressions[Idx]);
				DeletedCount++;
			}
		}

		if (DeletedCount > 0)
		{
			RebuildMaterialEditorGraph(OriginalMaterial);
			if (Material == OriginalMaterial)
			{
				UMaterialEditingLibrary::RecompileMaterial(Material);
				UEditorAssetLibrary::SaveAsset(MaterialPath);
			}
		}
	}
	else
	{
		DeletedCount = ToDelete.Num();
	}

	auto R = MakeShared<FJsonObject>();
	R->SetBoolField(TEXT("success"), true);
	R->SetStringField(TEXT("path"), MaterialPath);
	R->SetBoolField(TEXT("dry_run"), bDryRun);
	R->SetStringField(TEXT("mode"), Mode);
	R->SetNumberField(TEXT("deleted_count"), DeletedCount);
	R->SetArrayField(TEXT("deleted_nodes"), DeletedArr);
	if (DeletedCount > 0 && !bDryRun)
	{
		R->SetStringField(TEXT("note"),
			TEXT("Node indices have shifted — re-query with get_material_graph_nodes"));
	}
	return R;
}
