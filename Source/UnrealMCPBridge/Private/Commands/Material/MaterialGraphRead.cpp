#include "Commands/EpicUnrealMCPMaterialCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "MaterialEditorUtils.h"

#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "MaterialEditingLibrary.h"
#include "MaterialShared.h"
#include "RHIShaderPlatform.h"
#include "EditorAssetLibrary.h"

// ---------------------------------------------------------------------------
// HandleGetMaterialInfo
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleGetMaterialInfo(
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

	auto Info = MakeShared<FJsonObject>();
	Info->SetStringField(TEXT("path"), MaterialPath);

	// Blend mode
	static const TCHAR* BlendNames[] = {
		TEXT("opaque"), TEXT("masked"), TEXT("translucent"),
		TEXT("additive"), TEXT("modulate"), TEXT("alpha_composite"), TEXT("alpha_holdout")
	};
	int32 BM = (int32)Material->BlendMode;
	Info->SetStringField(TEXT("blend_mode"), (BM >= 0 && BM < 7) ? BlendNames[BM] : TEXT("unknown"));

	// Shading model
	FMaterialShadingModelField SMF = Material->GetShadingModels();
	FString SMStr = TEXT("default_lit");
	if (SMF.HasShadingModel(MSM_Unlit))
	{
		SMStr = TEXT("unlit");
	}
	else if (SMF.HasShadingModel(MSM_Subsurface))
	{
		SMStr = TEXT("subsurface");
	}
	else if (SMF.HasShadingModel(MSM_ClearCoat))
	{
		SMStr = TEXT("clear_coat");
	}
	Info->SetStringField(TEXT("shading_model"), SMStr);
	Info->SetBoolField(TEXT("two_sided"), Material->TwoSided);
	Info->SetNumberField(TEXT("num_expressions"),
		UMaterialEditingLibrary::GetNumMaterialExpressions(Material));

	// Used textures
	TArray<UTexture*> Textures;
	Material->GetUsedTextures(Textures, EMaterialQualityLevel::High);
	TArray<TSharedPtr<FJsonValue>> TexArr;
	for (UTexture* T : Textures)
	{
		if (T)
		{
			TexArr.Add(MakeShared<FJsonValueString>(T->GetPathName()));
		}
	}
	Info->SetArrayField(TEXT("used_textures"), TexArr);

	// Parameters — NOTE: avoid variable name 'PI' which is a UE math macro
	TArray<FMaterialParameterInfo> ParamInfos;
	TArray<FGuid> ParamIds;

	Material->GetAllScalarParameterInfo(ParamInfos, ParamIds);
	TArray<TSharedPtr<FJsonValue>> ScalarNames;
	for (const FMaterialParameterInfo& PInfo : ParamInfos)
	{
		ScalarNames.Add(MakeShared<FJsonValueString>(PInfo.Name.ToString()));
	}
	Info->SetArrayField(TEXT("scalar_parameters"), ScalarNames);

	Material->GetAllVectorParameterInfo(ParamInfos, ParamIds);
	TArray<TSharedPtr<FJsonValue>> VecNames;
	for (const FMaterialParameterInfo& PInfo : ParamInfos)
	{
		VecNames.Add(MakeShared<FJsonValueString>(PInfo.Name.ToString()));
	}
	Info->SetArrayField(TEXT("vector_parameters"), VecNames);

	Material->GetAllTextureParameterInfo(ParamInfos, ParamIds);
	TArray<TSharedPtr<FJsonValue>> TexParamNames;
	for (const FMaterialParameterInfo& PInfo : ParamInfos)
	{
		TexParamNames.Add(MakeShared<FJsonValueString>(PInfo.Name.ToString()));
	}
	Info->SetArrayField(TEXT("texture_parameters"), TexParamNames);

	// Stats
	FMaterialStatistics Stats = UMaterialEditingLibrary::GetStatistics(Material);
	auto StatsObj = MakeShared<FJsonObject>();
	StatsObj->SetNumberField(TEXT("vertex_instructions"), Stats.NumVertexShaderInstructions);
	StatsObj->SetNumberField(TEXT("pixel_instructions"), Stats.NumPixelShaderInstructions);
	StatsObj->SetNumberField(TEXT("samplers"), Stats.NumSamplers);
	Info->SetObjectField(TEXT("statistics"), StatsObj);

	auto R = MakeShared<FJsonObject>();
	R->SetBoolField(TEXT("success"), true);
	R->SetObjectField(TEXT("info"), Info);
	return R;
}

// ---------------------------------------------------------------------------
// HandleGetMaterialGraphNodes
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleGetMaterialGraphNodes(
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

	bool bIncludePins = false;
	Params->TryGetBoolField(TEXT("include_pins"), bIncludePins);

	FMaterialExpressionCollection& Collection = Material->GetExpressionCollection();

	TMap<UMaterialExpression*, int32> ExprIndexMap;
	ExprIndexMap.Reserve(Collection.Expressions.Num());
	for (int32 i = 0; i < Collection.Expressions.Num(); ++i)
	{
		if (UMaterialExpression* E = Collection.Expressions[i])
		{
			ExprIndexMap.Add(E, i);
		}
	}

	TArray<TSharedPtr<FJsonValue>> NodesArray;
	for (int32 i = 0; i < Collection.Expressions.Num(); ++i)
	{
		UMaterialExpression* Expr = Collection.Expressions[i];
		if (!Expr)
		{
			continue;
		}
		NodesArray.Add(MakeShared<FJsonValueObject>(
			SerializeMaterialExpression(Expr, i, ExprIndexMap, bIncludePins)));
	}

	auto R = MakeShared<FJsonObject>();
	R->SetBoolField(TEXT("success"), true);
	R->SetStringField(TEXT("path"), MaterialPath);
	R->SetNumberField(TEXT("count"), NodesArray.Num());
	R->SetArrayField(TEXT("nodes"), NodesArray);
	return R;
}

// ---------------------------------------------------------------------------
// HandleGetMaterialExpressionInfo
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleGetMaterialExpressionInfo(
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

	TMap<UMaterialExpression*, int32> ExprIndexMap;
	for (int32 i = 0; i < Collection.Expressions.Num(); ++i)
	{
		if (UMaterialExpression* E = Collection.Expressions[i])
		{
			ExprIndexMap.Add(E, i);
		}
	}

	// Always include available pins — that's the point of this command
	TSharedPtr<FJsonObject> NodeJson = SerializeMaterialExpression(
		Collection.Expressions[NodeIndex], NodeIndex, ExprIndexMap, /*bIncludeAvailablePins=*/true);

	auto R = MakeShared<FJsonObject>();
	R->SetBoolField(TEXT("success"), true);
	R->SetStringField(TEXT("path"), MaterialPath);
	R->SetObjectField(TEXT("node"), NodeJson);
	return R;
}

// ---------------------------------------------------------------------------
// HandleGetMaterialPropertyConnections
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleGetMaterialPropertyConnections(
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

	FMaterialExpressionCollection& Collection = Material->GetExpressionCollection();
	TMap<UMaterialExpression*, int32> ExprIndexMap;
	for (int32 i = 0; i < Collection.Expressions.Num(); ++i)
	{
		if (UMaterialExpression* E = Collection.Expressions[i])
		{
			ExprIndexMap.Add(E, i);
		}
	}

	static const EMaterialProperty Props[] = {
		MP_BaseColor, MP_Metallic, MP_Roughness, MP_EmissiveColor,
		MP_Opacity, MP_OpacityMask, MP_Normal, MP_Specular,
		MP_AmbientOcclusion, MP_WorldPositionOffset, MP_SubsurfaceColor, MP_Refraction
	};

	auto ConnMap = MakeShared<FJsonObject>();
	for (EMaterialProperty Prop : Props)
	{
		UMaterialExpression* InputNode = UMaterialEditingLibrary::GetMaterialPropertyInputNode(Material, Prop);
		if (!InputNode)
		{
			continue;
		}

		const int32* IdxPtr = ExprIndexMap.Find(InputNode);
		FString OutputName = UMaterialEditingLibrary::GetMaterialPropertyInputNodeOutputName(Material, Prop);
		FString TypeStr = InputNode->GetClass()->GetName();
		TypeStr.RemoveFromStart(TEXT("MaterialExpression"));

		auto Entry = MakeShared<FJsonObject>();
		Entry->SetNumberField(TEXT("node_index"), IdxPtr ? *IdxPtr : -1);
		Entry->SetStringField(TEXT("node_type"), TypeStr);
		Entry->SetStringField(TEXT("output_pin"), OutputName);
		ConnMap->SetObjectField(MaterialPropertyToString(Prop), Entry);
	}

	auto R = MakeShared<FJsonObject>();
	R->SetBoolField(TEXT("success"), true);
	R->SetStringField(TEXT("path"), MaterialPath);
	R->SetObjectField(TEXT("connections"), ConnMap);
	return R;
}

// ---------------------------------------------------------------------------
// HandleGetMaterialErrors
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleGetMaterialErrors(
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

	// Optional: recompile first to get fresh errors
	bool bRecompile = false;
	Params->TryGetBoolField(TEXT("recompile"), bRecompile);
	if (bRecompile)
	{
		UMaterialEditingLibrary::RecompileMaterial(Material);
	}

	// Build expression index map so we can report error node indices
	FMaterialExpressionCollection& Collection = Material->GetExpressionCollection();
	TMap<UMaterialExpression*, int32> ExprIndexMap;
	ExprIndexMap.Reserve(Collection.Expressions.Num());
	for (int32 i = 0; i < Collection.Expressions.Num(); ++i)
	{
		if (UMaterialExpression* E = Collection.Expressions[i])
		{
			ExprIndexMap.Add(E, i);
		}
	}

	// Gather errors across all quality levels for the current shader platform
	TArray<TSharedPtr<FJsonValue>> ErrorsArray;
	TSet<FString> SeenErrors; // Deduplicate

	const EShaderPlatform ShaderPlatform = GMaxRHIShaderPlatform;

	for (int32 QualityIdx = 0; QualityIdx < EMaterialQualityLevel::Num; ++QualityIdx)
	{
		const FMaterialResource* MatResource = Material->GetMaterialResource(
			ShaderPlatform, static_cast<EMaterialQualityLevel::Type>(QualityIdx));

		if (!MatResource)
		{
			continue;
		}

		const TArray<FString>& CompileErrors = MatResource->GetCompileErrors();
		for (const FString& Error : CompileErrors)
		{
			if (SeenErrors.Contains(Error))
			{
				continue;
			}
			SeenErrors.Add(Error);

			auto ErrObj = MakeShared<FJsonObject>();
			ErrObj->SetStringField(TEXT("message"), Error);
			ErrObj->SetNumberField(TEXT("quality_level"), QualityIdx);
			ErrorsArray.Add(MakeShared<FJsonValueObject>(ErrObj));
		}

		// Map error expressions to node indices
		const TArray<UMaterialExpression*>& ErrorExprs = MatResource->GetErrorExpressions();
		if (ErrorExprs.Num() > 0 && ErrorsArray.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> ErrorNodeIndices;
			for (UMaterialExpression* ErrExpr : ErrorExprs)
			{
				if (const int32* IdxPtr = ExprIndexMap.Find(ErrExpr))
				{
					auto NodeObj = MakeShared<FJsonObject>();
					NodeObj->SetNumberField(TEXT("node_index"), *IdxPtr);
					NodeObj->SetStringField(TEXT("node_type"),
						ErrExpr->GetClass()->GetName().Replace(TEXT("MaterialExpression"), TEXT("")));
					ErrorNodeIndices.Add(MakeShared<FJsonValueObject>(NodeObj));
				}
			}
			if (ErrorNodeIndices.Num() > 0)
			{
				ErrorsArray.Last()->AsObject()->SetArrayField(TEXT("error_nodes"), ErrorNodeIndices);
			}
		}
	}

	auto R = MakeShared<FJsonObject>();
	R->SetBoolField(TEXT("success"), true);
	R->SetStringField(TEXT("path"), MaterialPath);
	R->SetBoolField(TEXT("has_errors"), ErrorsArray.Num() > 0);
	R->SetNumberField(TEXT("error_count"), ErrorsArray.Num());
	R->SetArrayField(TEXT("errors"), ErrorsArray);
	return R;
}
