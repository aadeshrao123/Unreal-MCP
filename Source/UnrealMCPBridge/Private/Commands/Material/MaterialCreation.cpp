#include "Commands/EpicUnrealMCPMaterialCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "MaterialEditorUtils.h"

#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionComment.h"
#include "MaterialEditingLibrary.h"
#include "EditorAssetLibrary.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Factories/MaterialFactoryNew.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"

// ---------------------------------------------------------------------------
// HandleCreateMaterial
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleCreateMaterial(
	const TSharedPtr<FJsonObject>& Params)
{
	FString Name;
	if (!Params->TryGetStringField(TEXT("name"), Name))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
	}

	FString Path = TEXT("/Game/Materials");
	Params->TryGetStringField(TEXT("path"), Path);
	FString BlendModeStr = TEXT("opaque");
	Params->TryGetStringField(TEXT("blend_mode"), BlendModeStr);
	FString ShadingStr = TEXT("default_lit");
	Params->TryGetStringField(TEXT("shading_model"), ShadingStr);
	bool bTwoSided = false;
	Params->TryGetBoolField(TEXT("two_sided"), bTwoSided);

	// Check if asset already exists to avoid editor overwrite popup that blocks automation
	FString FullAssetPath = Path / Name;
	UMaterial* Mat = nullptr;

	if (UEditorAssetLibrary::DoesAssetExist(FullAssetPath))
	{
		bool bForce = false;
		Params->TryGetBoolField(TEXT("force"), bForce);
		if (!bForce)
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Material already exists: %s. Pass force=true to overwrite."), *FullAssetPath));
		}

		// Force mode: reuse existing asset (avoids GC crash from DeleteAsset during tick)
		Mat = Cast<UMaterial>(UEditorAssetLibrary::LoadAsset(FullAssetPath));
		if (Mat)
		{
			UMaterialEditingLibrary::DeleteAllMaterialExpressions(Mat);
		}
	}

	if (!Mat)
	{
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		UObject* NewAsset = AssetTools.CreateAsset(Name, Path, UMaterial::StaticClass(),
		                                           NewObject<UMaterialFactoryNew>());
		Mat = Cast<UMaterial>(NewAsset);
	}

	if (!Mat)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create material"));
	}

	Mat->BlendMode = ResolveBlendMode(BlendModeStr);
	Mat->SetShadingModel(ResolveShadingModel(ShadingStr));
	Mat->TwoSided = bTwoSided;

	double OpacityClip;
	if (Params->TryGetNumberField(TEXT("opacity_mask_clip_value"), OpacityClip))
	{
		Mat->OpacityMaskClipValue = (float)OpacityClip;
	}

	UEditorAssetLibrary::SaveAsset(FullAssetPath);

	auto R = MakeShared<FJsonObject>();
	R->SetBoolField(TEXT("success"), true);
	R->SetStringField(TEXT("path"), FullAssetPath);
	return R;
}

// ---------------------------------------------------------------------------
// HandleCreateMaterialInstance
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleCreateMaterialInstance(
	const TSharedPtr<FJsonObject>& Params)
{
	FString ParentPath;
	if (!Params->TryGetStringField(TEXT("parent_path"), ParentPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'parent_path' parameter"));
	}
	FString Name;
	if (!Params->TryGetStringField(TEXT("name"), Name))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
	}
	FString Path = TEXT("/Game/Materials");
	Params->TryGetStringField(TEXT("path"), Path);

	// Check if asset already exists to avoid editor overwrite popup
	FString FullAssetPath = Path / Name;
	UMaterialInstanceConstant* MI = nullptr;

	if (UEditorAssetLibrary::DoesAssetExist(FullAssetPath))
	{
		bool bForce = false;
		Params->TryGetBoolField(TEXT("force"), bForce);
		if (!bForce)
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Material instance already exists: %s. Pass force=true to overwrite."), *FullAssetPath));
		}

		// Force mode: reuse existing asset (avoids GC crash from DeleteAsset during tick)
		MI = Cast<UMaterialInstanceConstant>(UEditorAssetLibrary::LoadAsset(FullAssetPath));
	}

	if (!MI)
	{
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		UObject* NewAsset = AssetTools.CreateAsset(Name, Path, UMaterialInstanceConstant::StaticClass(),
		                                           NewObject<UMaterialInstanceConstantFactoryNew>());
		MI = Cast<UMaterialInstanceConstant>(NewAsset);
	}

	if (!MI)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create material instance"));
	}

	UMaterialInterface* Parent = Cast<UMaterialInterface>(UEditorAssetLibrary::LoadAsset(ParentPath));
	if (Parent)
	{
		UMaterialEditingLibrary::SetMaterialInstanceParent(MI, Parent);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("MCPMaterial: Parent not found: %s"), *ParentPath);
	}

	const TSharedPtr<FJsonObject>* ScalarObj;
	if (Params->TryGetObjectField(TEXT("scalar_params"), ScalarObj))
	{
		for (const auto& Pair : (*ScalarObj)->Values)
		{
			UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(
				MI, FName(*Pair.Key), (float)Pair.Value->AsNumber());
		}
	}

	const TSharedPtr<FJsonObject>* VectorObj;
	if (Params->TryGetObjectField(TEXT("vector_params"), VectorObj))
	{
		for (const auto& Pair : (*VectorObj)->Values)
		{
			const TArray<TSharedPtr<FJsonValue>>& A = Pair.Value->AsArray();
			FLinearColor C(
				A.Num() > 0 ? (float)A[0]->AsNumber() : 0,
				A.Num() > 1 ? (float)A[1]->AsNumber() : 0,
				A.Num() > 2 ? (float)A[2]->AsNumber() : 0,
				A.Num() > 3 ? (float)A[3]->AsNumber() : 1);
			UMaterialEditingLibrary::SetMaterialInstanceVectorParameterValue(MI, FName(*Pair.Key), C);
		}
	}

	const TSharedPtr<FJsonObject>* TextureObj;
	if (Params->TryGetObjectField(TEXT("texture_params"), TextureObj))
	{
		for (const auto& Pair : (*TextureObj)->Values)
		{
			UTexture* Tex = Cast<UTexture>(UEditorAssetLibrary::LoadAsset(Pair.Value->AsString()));
			if (Tex)
			{
				UMaterialEditingLibrary::SetMaterialInstanceTextureParameterValue(MI, FName(*Pair.Key), Tex);
			}
		}
	}

	UMaterialEditingLibrary::UpdateMaterialInstance(MI);
	UEditorAssetLibrary::SaveAsset(FullAssetPath);

	auto R = MakeShared<FJsonObject>();
	R->SetBoolField(TEXT("success"), true);
	R->SetStringField(TEXT("path"), FullAssetPath);
	return R;
}

// ---------------------------------------------------------------------------
// HandleBuildMaterialGraph
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleBuildMaterialGraph(
	const TSharedPtr<FJsonObject>& Params)
{
	FString MaterialPath;
	if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path'"));
	}

	const TArray<TSharedPtr<FJsonValue>>* NodesArray;
	if (!Params->TryGetArrayField(TEXT("nodes"), NodesArray))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'nodes' array"));
	}

	const TArray<TSharedPtr<FJsonValue>>* ConnectionsArray;
	if (!Params->TryGetArrayField(TEXT("connections"), ConnectionsArray))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'connections' array"));
	}

	bool bClearExisting = true;
	Params->TryGetBoolField(TEXT("clear_existing"), bClearExisting);

	UMaterial* OriginalMaterial = Cast<UMaterial>(UEditorAssetLibrary::LoadAsset(MaterialPath));
	UMaterial* Material = OriginalMaterial ? ResolveWorkingMaterial(OriginalMaterial) : nullptr;
	if (!Material)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Material not found: %s"), *MaterialPath));
	}

	TArray<FString> Errors;

	if (bClearExisting)
	{
		UMaterialEditingLibrary::DeleteAllMaterialExpressions(Material);
		UMaterialEditingLibrary::DeleteAllMaterialExpressions(Material); // double-pass
	}

	// ---- Create nodes ----
	TArray<UMaterialExpression*> CreatedNodes;
	CreatedNodes.SetNum(NodesArray->Num());

	for (int32 Idx = 0; Idx < NodesArray->Num(); ++Idx)
	{
		const TSharedPtr<FJsonObject>& NodeDef = (*NodesArray)[Idx]->AsObject();
		if (!NodeDef.IsValid())
		{
			Errors.Add(FString::Printf(TEXT("Node %d: invalid JSON"), Idx));
			CreatedNodes[Idx] = nullptr;
			continue;
		}

		FString TypeName = TEXT("Constant");
		NodeDef->TryGetStringField(TEXT("type"), TypeName);
		int32 PosX = -300, PosY = 0;
		NodeDef->TryGetNumberField(TEXT("pos_x"), PosX);
		NodeDef->TryGetNumberField(TEXT("pos_y"), PosY);

		UClass* ExprClass = FindExpressionClass(TypeName);
		if (!ExprClass)
		{
			Errors.Add(FString::Printf(TEXT("Node %d: unknown type '%s'"), Idx, *TypeName));
			CreatedNodes[Idx] = nullptr;
			continue;
		}

		UMaterialExpression* Expr = UMaterialEditingLibrary::CreateMaterialExpression(
			Material, ExprClass, PosX, PosY);
		if (!Expr)
		{
			Errors.Add(FString::Printf(TEXT("Node %d: failed to create '%s'"), Idx, *TypeName));
			CreatedNodes[Idx] = nullptr;
			continue;
		}
		CreatedNodes[Idx] = Expr;

		const bool bIsCustom = (TypeName == TEXT("Custom") || TypeName == TEXT("MaterialExpressionCustom"));
		if (bIsCustom)
		{
			HandleCustomHLSLNode(Expr, NodeDef);
		}

		const TSharedPtr<FJsonObject>* PropsObj;
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
	}

	// ---- Wire connections ----
	int32 ConnectionsMade = 0;
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
			if (FV.IsValid())
			{
				TryParseIntFromJson(FV, FromIdx);
			}
		}
		FString FromPin;
		Conn->TryGetStringField(TEXT("from_pin"), FromPin);
		FString ToPin;
		Conn->TryGetStringField(TEXT("to_pin"), ToPin);

		bool bToMaterial = false;
		int32 ToIdx = 0;
		TSharedPtr<FJsonValue> ToNodeVal = Conn->TryGetField(TEXT("to_node"));
		if (ToNodeVal.IsValid())
		{
			if (ToNodeVal->Type == EJson::String)
			{
				FString S = ToNodeVal->AsString();
				if (S.Equals(TEXT("material"), ESearchCase::IgnoreCase))
				{
					bToMaterial = true;
				}
				else if (S.IsNumeric())
				{
					ToIdx = FCString::Atoi(*S);
				}
			}
			else
			{
				TryParseIntFromJson(ToNodeVal, ToIdx);
			}
		}

		if (FromIdx < 0 || FromIdx >= CreatedNodes.Num() || !CreatedNodes[FromIdx])
		{
			Errors.Add(FString::Printf(TEXT("Connection: invalid from_node %d"), FromIdx));
			continue;
		}

		bool bConnected = false;
		if (bToMaterial)
		{
			EMaterialProperty MatProp = ResolveMaterialProperty(ToPin);
			if (MatProp == MP_MAX)
			{
				Errors.Add(FString::Printf(TEXT("Connection: unknown material property '%s'"), *ToPin));
				continue;
			}
			bConnected = UMaterialEditingLibrary::ConnectMaterialProperty(
				CreatedNodes[FromIdx], FromPin, MatProp);
		}
		else
		{
			if (ToIdx < 0 || ToIdx >= CreatedNodes.Num() || !CreatedNodes[ToIdx])
			{
				Errors.Add(FString::Printf(TEXT("Connection: invalid to_node %d"), ToIdx));
				continue;
			}
			bConnected = UMaterialEditingLibrary::ConnectMaterialExpressions(
				CreatedNodes[FromIdx], FromPin, CreatedNodes[ToIdx], ToPin);
		}

		if (bConnected)
		{
			ConnectionsMade++;
		}
		else
		{
			FString ToDesc = bToMaterial
				? FString::Printf(TEXT("material.%s"), *ToPin)
				: FString::Printf(TEXT("node[%d].%s"), ToIdx, *ToPin);
			Errors.Add(FString::Printf(TEXT("Connection failed: node[%d].%s -> %s"),
				FromIdx, *FromPin, *ToDesc));
		}
	}

	// ---- Save ----
	RebuildMaterialEditorGraph(OriginalMaterial);
	if (Material == OriginalMaterial)
	{
		UMaterialEditingLibrary::RecompileMaterial(Material);
		UEditorAssetLibrary::SaveAsset(MaterialPath);
	}

	auto R = MakeShared<FJsonObject>();
	R->SetBoolField(TEXT("success"), true);
	R->SetStringField(TEXT("path"), MaterialPath);
	R->SetNumberField(TEXT("nodes_created"), CreatedNodes.Num());
	R->SetNumberField(TEXT("connections_made"), ConnectionsMade);
	TArray<TSharedPtr<FJsonValue>> ErrArr;
	for (const FString& E : Errors)
	{
		ErrArr.Add(MakeShared<FJsonValueString>(E));
	}
	R->SetArrayField(TEXT("errors"), ErrArr);
	return R;
}

// ---------------------------------------------------------------------------
// HandleSetMaterialProperties
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleSetMaterialProperties(
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

	TArray<TSharedPtr<FJsonValue>> Changed;

	FString Str;
	if (Params->TryGetStringField(TEXT("blend_mode"), Str))
	{
		Material->BlendMode = ResolveBlendMode(Str);
		Changed.Add(MakeShared<FJsonValueString>(TEXT("blend_mode=") + Str));
	}
	if (Params->TryGetStringField(TEXT("shading_model"), Str))
	{
		Material->SetShadingModel(ResolveShadingModel(Str));
		Changed.Add(MakeShared<FJsonValueString>(TEXT("shading_model=") + Str));
	}

	bool bBool;
	if (Params->TryGetBoolField(TEXT("two_sided"), bBool))
	{
		Material->TwoSided = bBool;
		Changed.Add(MakeShared<FJsonValueString>(
			FString::Printf(TEXT("two_sided=%s"), bBool ? TEXT("true") : TEXT("false"))));
	}

	double Num;
	if (Params->TryGetNumberField(TEXT("opacity_mask_clip_value"), Num))
	{
		Material->OpacityMaskClipValue = (float)Num;
		Changed.Add(MakeShared<FJsonValueString>(
			FString::Printf(TEXT("opacity_mask_clip_value=%f"), Num)));
	}
	if (Params->TryGetBoolField(TEXT("dithered_lof_transition"), bBool))
	{
		Material->DitheredLODTransition = bBool;
		Changed.Add(MakeShared<FJsonValueString>(
			FString::Printf(TEXT("dithered_lod_transition=%s"), bBool ? TEXT("true") : TEXT("false"))));
	}
	if (Params->TryGetBoolField(TEXT("allow_negative_emissive_color"), bBool))
	{
		Material->bAllowNegativeEmissiveColor = bBool;
		Changed.Add(MakeShared<FJsonValueString>(
			FString::Printf(TEXT("allow_negative_emissive_color=%s"), bBool ? TEXT("true") : TEXT("false"))));
	}

	bool bRecompile = true;
	Params->TryGetBoolField(TEXT("recompile"), bRecompile);
	if (Material == OriginalMaterial)
	{
		if (bRecompile)
		{
			UMaterialEditingLibrary::RecompileMaterial(Material);
		}
		UEditorAssetLibrary::SaveAsset(MaterialPath);
	}
	else if (bRecompile)
	{
		NotifyMaterialEditorRefresh(OriginalMaterial);
	}

	auto R = MakeShared<FJsonObject>();
	R->SetBoolField(TEXT("success"), true);
	R->SetStringField(TEXT("path"), MaterialPath);
	R->SetArrayField(TEXT("changed"), Changed);
	return R;
}

// ---------------------------------------------------------------------------
// HandleAddMaterialComments
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleAddMaterialComments(
	const TSharedPtr<FJsonObject>& Params)
{
	FString MaterialPath;
	if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path'"));
	}

	const TArray<TSharedPtr<FJsonValue>>* CommentsArray;
	if (!Params->TryGetArrayField(TEXT("comments"), CommentsArray))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'comments' array"));
	}

	UMaterial* OriginalMaterial = Cast<UMaterial>(UEditorAssetLibrary::LoadAsset(MaterialPath));
	UMaterial* Material = OriginalMaterial ? ResolveWorkingMaterial(OriginalMaterial) : nullptr;
	if (!Material)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Material not found: %s"), *MaterialPath));
	}

	FMaterialExpressionCollection& Collection = Material->GetExpressionCollection();
	int32 CommentsCreated = 0;
	TArray<FString> Errors;

	for (int32 Idx = 0; Idx < CommentsArray->Num(); ++Idx)
	{
		const TSharedPtr<FJsonObject>& Def = (*CommentsArray)[Idx]->AsObject();
		if (!Def.IsValid())
		{
			Errors.Add(FString::Printf(TEXT("Comment %d: invalid JSON"), Idx));
			continue;
		}

		FString Text;
		if (!Def->TryGetStringField(TEXT("text"), Text))
		{
			Errors.Add(FString::Printf(TEXT("Comment %d: missing 'text'"), Idx));
			continue;
		}

		UMaterialExpressionComment* Comment = NewObject<UMaterialExpressionComment>(Material);
		if (!Comment)
		{
			Errors.Add(FString::Printf(TEXT("Comment %d: failed to create"), Idx));
			continue;
		}

		Comment->Text = Text;

		int32 PosX = 0, PosY = 0;
		Def->TryGetNumberField(TEXT("pos_x"), PosX);
		Def->TryGetNumberField(TEXT("pos_y"), PosY);
		Comment->MaterialExpressionEditorX = PosX;
		Comment->MaterialExpressionEditorY = PosY;

		int32 SzX = 400, SzY = 200;
		Def->TryGetNumberField(TEXT("size_x"), SzX);
		Def->TryGetNumberField(TEXT("size_y"), SzY);
		Comment->SizeX = SzX;
		Comment->SizeY = SzY;

		int32 FontSize = 18;
		Def->TryGetNumberField(TEXT("font_size"), FontSize);
		Comment->FontSize = FMath::Clamp(FontSize, 1, 1000);

		const TArray<TSharedPtr<FJsonValue>>* ColorArr;
		if (Def->TryGetArrayField(TEXT("color"), ColorArr) && ColorArr->Num() >= 3)
		{
			Comment->CommentColor = FLinearColor(
				(float)(*ColorArr)[0]->AsNumber(),
				(float)(*ColorArr)[1]->AsNumber(),
				(float)(*ColorArr)[2]->AsNumber(),
				ColorArr->Num() > 3 ? (float)(*ColorArr)[3]->AsNumber() : 1.f);
		}

		bool bBool;
		if (Def->TryGetBoolField(TEXT("show_bubble"), bBool))
		{
			Comment->bCommentBubbleVisible_InDetailsPanel = bBool;
		}
		if (Def->TryGetBoolField(TEXT("color_bubble"), bBool))
		{
			Comment->bColorCommentBubble = bBool;
		}
		bool bGroup = true;
		if (Def->TryGetBoolField(TEXT("group_mode"), bGroup))
		{
			Comment->bGroupMode = bGroup;
		}

		Collection.AddComment(Comment);
		Comment->Material = Material;
		CommentsCreated++;
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
	R->SetNumberField(TEXT("comments_created"), CommentsCreated);
	TArray<TSharedPtr<FJsonValue>> ErrArr;
	for (const FString& E : Errors)
	{
		ErrArr.Add(MakeShared<FJsonValueString>(E));
	}
	R->SetArrayField(TEXT("errors"), ErrArr);
	return R;
}

// ---------------------------------------------------------------------------
// HandleRecompileMaterial
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleRecompileMaterial(
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

	NotifyMaterialEditorRefresh(OriginalMaterial);
	if (Material == OriginalMaterial)
	{
		UMaterialEditingLibrary::RecompileMaterial(Material);
		UEditorAssetLibrary::SaveAsset(MaterialPath);
	}

	auto R = MakeShared<FJsonObject>();
	R->SetBoolField(TEXT("success"), true);
	R->SetStringField(TEXT("path"), MaterialPath);
	return R;
}
