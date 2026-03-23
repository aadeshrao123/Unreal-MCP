#include "Commands/EpicUnrealMCPMaterialCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "Commands/EpicUnrealMCPPropertyUtils.h"
#include "MaterialEditorUtils.h"

#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionSubstrate.h"
#include "Materials/MaterialFunction.h"
#include "MaterialEditingLibrary.h"
#include "MaterialShared.h"
#include "RHIShaderPlatform.h"
#include "RenderUtils.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"

// ---------------------------------------------------------------------------
// Shared helper: serialize editable properties for an expression via reflection
// Used by HandleGetExpressionTypeInfo and the serializer.
// ---------------------------------------------------------------------------
static void SerializeExpressionReflectionProperties(
	UMaterialExpression* Expr, UClass* ExprClass,
	TArray<TSharedPtr<FJsonValue>>& OutPropsArr)
{
	UClass* BaseClass = UMaterialExpression::StaticClass();
	UClass* ObjectClass = UObject::StaticClass();

	static const TSet<FName> InputStructNames = {
		FName(TEXT("ExpressionInput")),
		FName(TEXT("ExpressionOutput")),
		FName(TEXT("ColorMaterialInput")),
		FName(TEXT("ScalarMaterialInput")),
		FName(TEXT("VectorMaterialInput")),
		FName(TEXT("Vector2MaterialInput")),
		FName(TEXT("MaterialAttributesInput")),
		FName(TEXT("ShadingModelMaterialInput")),
		FName(TEXT("SubstrateMaterialInput")),
	};

	for (TFieldIterator<FProperty> PropIt(ExprClass); PropIt; ++PropIt)
	{
		FProperty* Prop = *PropIt;

		UClass* OwnerClass = Prop->GetOwnerClass();
		if (OwnerClass == BaseClass || OwnerClass == ObjectClass)
		{
			continue;
		}

		if (FStructProperty* SP = CastField<FStructProperty>(Prop))
		{
			if (InputStructNames.Contains(SP->Struct->GetFName()))
			{
				continue;
			}
		}

		if (Prop->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated))
		{
			continue;
		}
		if (CastField<FDelegateProperty>(Prop) || CastField<FMulticastDelegateProperty>(Prop))
		{
			continue;
		}
		if (CastField<FWeakObjectProperty>(Prop))
		{
			continue;
		}

		auto PropObj = MakeShared<FJsonObject>();
		PropObj->SetStringField(TEXT("name"), Prop->GetName());
		PropObj->SetStringField(TEXT("type"), Prop->GetCPPType());

		const void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Expr);
		TSharedPtr<FJsonValue> DefaultVal = FEpicUnrealMCPPropertyUtils::SafePropertyToJsonValue(Prop, ValuePtr);
		if (DefaultVal.IsValid() && DefaultVal->Type != EJson::Null)
		{
			PropObj->SetField(TEXT("default_value"), DefaultVal);
		}

		OutPropsArr.Add(MakeShared<FJsonValueObject>(PropObj));
	}
}

// ---------------------------------------------------------------------------
// Shared helper: serialize input/output pin info for an expression
// ---------------------------------------------------------------------------
static void SerializeExpressionPinInfo(
	UMaterialExpression* Expr,
	TArray<TSharedPtr<FJsonValue>>& OutInputsArr,
	TArray<TSharedPtr<FJsonValue>>& OutOutputsArr)
{
	// Inputs
	TArray<FString> InputNames = UMaterialEditingLibrary::GetMaterialExpressionInputNames(Expr);
	for (const FString& Name : InputNames)
	{
		auto InObj = MakeShared<FJsonObject>();
		InObj->SetStringField(TEXT("name"), Name);
		OutInputsArr.Add(MakeShared<FJsonValueObject>(InObj));
	}

	// Outputs
	TArray<FExpressionOutput>& Outputs = Expr->GetOutputs();
	for (int32 i = 0; i < Outputs.Num(); ++i)
	{
		auto OutObj = MakeShared<FJsonObject>();
		OutObj->SetNumberField(TEXT("index"), i);
		FString OutName = Outputs[i].OutputName.IsNone()
			? (i == 0 ? TEXT("") : FString::Printf(TEXT("Output_%d"), i))
			: Outputs[i].OutputName.ToString();
		OutObj->SetStringField(TEXT("name"), OutName);
		OutOutputsArr.Add(MakeShared<FJsonValueObject>(OutObj));
	}
}

// ---------------------------------------------------------------------------
// Shared helper: build material property pin info for a given material
// ---------------------------------------------------------------------------
struct FMaterialPinInfo
{
	EMaterialProperty Property;
	FString Name;
	FString ExpectedType;
	bool bConnected;
};

static void GetMaterialPropertyPins(UMaterial* Material, TArray<FMaterialPinInfo>& OutPins)
{
	struct FPinDef
	{
		EMaterialProperty Property;
		const TCHAR* Name;
		const TCHAR* ExpectedType;
	};

	static const FPinDef AllPins[] = {
		{ MP_BaseColor,           TEXT("BaseColor"),           TEXT("float3") },
		{ MP_Metallic,            TEXT("Metallic"),            TEXT("float")  },
		{ MP_Specular,            TEXT("Specular"),            TEXT("float")  },
		{ MP_Roughness,           TEXT("Roughness"),           TEXT("float")  },
		{ MP_Anisotropy,          TEXT("Anisotropy"),          TEXT("float")  },
		{ MP_EmissiveColor,       TEXT("EmissiveColor"),       TEXT("float3") },
		{ MP_Opacity,             TEXT("Opacity"),             TEXT("float")  },
		{ MP_OpacityMask,         TEXT("OpacityMask"),         TEXT("float")  },
		{ MP_Normal,              TEXT("Normal"),              TEXT("float3") },
		{ MP_Tangent,             TEXT("Tangent"),             TEXT("float3") },
		{ MP_WorldPositionOffset, TEXT("WorldPositionOffset"), TEXT("float3") },
		{ MP_Displacement,        TEXT("Displacement"),        TEXT("float")  },
		{ MP_SubsurfaceColor,     TEXT("SubsurfaceColor"),     TEXT("float3") },
		{ MP_CustomData0,         TEXT("CustomData0"),         TEXT("float")  },
		{ MP_CustomData1,         TEXT("CustomData1"),         TEXT("float")  },
		{ MP_AmbientOcclusion,    TEXT("AmbientOcclusion"),    TEXT("float")  },
		{ MP_Refraction,          TEXT("Refraction"),          TEXT("float")  },
		{ MP_PixelDepthOffset,    TEXT("PixelDepthOffset"),    TEXT("float")  },
		{ MP_ShadingModel,        TEXT("ShadingModel"),        TEXT("float")  },
		{ MP_SurfaceThickness,    TEXT("SurfaceThickness"),    TEXT("float")  },
		{ MP_FrontMaterial,       TEXT("FrontMaterial"),        TEXT("Substrate") },
	};

	for (const FPinDef& Def : AllPins)
	{
		FExpressionInput* Input = Material->GetExpressionInputForProperty(Def.Property);
		if (!Input)
		{
			continue; // Pin is hidden for this material configuration
		}

		FMaterialPinInfo Info;
		Info.Property = Def.Property;
		Info.Name = Def.Name;
		Info.ExpectedType = Def.ExpectedType;
		Info.bConnected = Input->IsConnected();
		OutPins.Add(Info);
	}
}

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

	// Parse include filter — comma-separated list of sections to include
	FString IncludeFilter;
	Params->TryGetStringField(TEXT("include"), IncludeFilter);

	bool bIncludeParameters = IncludeFilter.IsEmpty() || IncludeFilter.Contains(TEXT("parameters"));
	bool bIncludeTextures = IncludeFilter.IsEmpty() || IncludeFilter.Contains(TEXT("textures"));
	bool bIncludeStatistics = IncludeFilter.IsEmpty() || IncludeFilter.Contains(TEXT("statistics"));

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
	if (bIncludeTextures)
	{
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
	}

	// Parameters — NOTE: avoid variable name 'PI' which is a UE math macro
	if (bIncludeParameters)
	{
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
	}

	// Stats
	if (bIncludeStatistics)
	{
		FMaterialStatistics Stats = UMaterialEditingLibrary::GetStatistics(Material);
		auto StatsObj = MakeShared<FJsonObject>();
		StatsObj->SetNumberField(TEXT("vertex_instructions"), Stats.NumVertexShaderInstructions);
		StatsObj->SetNumberField(TEXT("pixel_instructions"), Stats.NumPixelShaderInstructions);
		StatsObj->SetNumberField(TEXT("samplers"), Stats.NumSamplers);
		Info->SetObjectField(TEXT("statistics"), StatsObj);
	}

	// Substrate status — always report (low cost, high value for AI decision-making)
	Info->SetBoolField(TEXT("substrate_enabled"), Substrate::IsSubstrateEnabled());
	Info->SetBoolField(TEXT("has_front_material_connected"), Material->HasSubstrateFrontMaterialConnected());

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

	FString Verbosity = TEXT("connections");
	Params->TryGetStringField(TEXT("verbosity"), Verbosity);

	FString TypeFilter;
	Params->TryGetStringField(TEXT("type_filter"), TypeFilter);

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

		// Apply type filter if specified
		if (!TypeFilter.IsEmpty())
		{
			FString ShortType = Expr->GetClass()->GetName();
			ShortType.RemoveFromStart(TEXT("MaterialExpression"));
			if (!ShortType.Contains(TypeFilter, ESearchCase::IgnoreCase))
			{
				continue;
			}
		}

		NodesArray.Add(MakeShared<FJsonValueObject>(
			SerializeMaterialExpression(Expr, i, ExprIndexMap, bIncludePins, Verbosity)));
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
		MP_AmbientOcclusion, MP_WorldPositionOffset, MP_SubsurfaceColor, MP_Refraction,
		MP_PixelDepthOffset, MP_ShadingModel, MP_Anisotropy, MP_Tangent,
		MP_CustomData0, MP_CustomData1, MP_SurfaceThickness, MP_Displacement, MP_FrontMaterial
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

// ---------------------------------------------------------------------------
// HandleGetExpressionTypeInfo
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleGetExpressionTypeInfo(
	const TSharedPtr<FJsonObject>& Params)
{
	FString TypeName;
	if (!Params->TryGetStringField(TEXT("type_name"), TypeName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'type_name'"));
	}

	UClass* ExprClass = FindExpressionClass(TypeName);
	if (!ExprClass)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Unknown expression type: '%s'"), *TypeName));
	}

	// Create a temporary instance to inspect pins and properties
	UMaterialExpression* Temp = NewObject<UMaterialExpression>(
		GetTransientPackage(), ExprClass);
	if (!Temp)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Failed to create temp instance of '%s'"), *TypeName));
	}

	auto Result = MakeShared<FJsonObject>();

	FString ShortType = ExprClass->GetName();
	ShortType.RemoveFromStart(TEXT("MaterialExpression"));
	Result->SetStringField(TEXT("type"), ShortType);

	// ---- Special handling: MaterialFunctionCall with function_path ----
	const bool bIsFunctionCall = ExprClass->IsChildOf(UMaterialExpressionMaterialFunctionCall::StaticClass());
	if (bIsFunctionCall)
	{
		FString FunctionPath;
		Params->TryGetStringField(TEXT("function_path"), FunctionPath);

		if (!FunctionPath.IsEmpty())
		{
			// Load the function and populate the call node with real inputs/outputs
			UMaterialFunctionInterface* Func = LoadObject<UMaterialFunctionInterface>(nullptr, *FunctionPath);
			if (Func)
			{
				UMaterialExpressionMaterialFunctionCall* FuncCall =
					Cast<UMaterialExpressionMaterialFunctionCall>(Temp);
				if (FuncCall)
				{
					FuncCall->SetMaterialFunction(Func);
				}
				Result->SetStringField(TEXT("function_path"), FunctionPath);

				// Also include the function's own info for convenience
				UMaterialFunction* MF = Cast<UMaterialFunction>(Func);
				if (MF)
				{
					Result->SetStringField(TEXT("function_description"), MF->Description);
				}
			}
			else
			{
				Result->SetStringField(TEXT("function_error"),
					FString::Printf(TEXT("Function not found: %s"), *FunctionPath));
			}
		}
		else
		{
			// No function_path provided — inputs/outputs will be empty.
			// Tell the AI how to use this properly.
			Result->SetStringField(TEXT("note"),
				TEXT("MaterialFunctionCall inputs/outputs depend on which function is loaded. ")
				TEXT("Pass function_path to see actual pins. ")
				TEXT("Use search_material_functions to discover functions, then ")
				TEXT("get_material_function_info to inspect a function's pins. ")
				TEXT("In build_material_graph, set properties.function_path to the function asset path."));
		}
	}

	// ---- Collect pins (populated by SetMaterialFunction for function calls) ----
	TArray<TSharedPtr<FJsonValue>> InputsArr;
	TArray<TSharedPtr<FJsonValue>> OutputsArr;
	SerializeExpressionPinInfo(Temp, InputsArr, OutputsArr);
	Result->SetArrayField(TEXT("inputs"), InputsArr);
	Result->SetArrayField(TEXT("outputs"), OutputsArr);

	// ---- Editable properties via reflection ----
	TArray<TSharedPtr<FJsonValue>> PropsArr;
	SerializeExpressionReflectionProperties(Temp, ExprClass, PropsArr);
	Result->SetArrayField(TEXT("properties"), PropsArr);

	// ---- Special handling: Custom HLSL node schema ----
	const bool bIsCustom = ExprClass->IsChildOf(UMaterialExpressionCustom::StaticClass());
	if (bIsCustom)
	{
		auto Schema = MakeShared<FJsonObject>();
		Schema->SetStringField(TEXT("description"),
			TEXT("Custom HLSL nodes support additional top-level fields in build_material_graph "
			     "and add_material_expression (NOT inside 'properties'):"));

		// code field
		auto CodeField = MakeShared<FJsonObject>();
		CodeField->SetStringField(TEXT("type"), TEXT("string"));
		CodeField->SetStringField(TEXT("description"), TEXT("HLSL source code string"));
		CodeField->SetStringField(TEXT("example"), TEXT("return UV.x * Speed;"));
		Schema->SetObjectField(TEXT("code"), CodeField);

		// description field
		auto DescField = MakeShared<FJsonObject>();
		DescField->SetStringField(TEXT("type"), TEXT("string"));
		DescField->SetStringField(TEXT("description"), TEXT("Node title shown in the graph"));
		Schema->SetObjectField(TEXT("description"), DescField);

		// output_type field
		auto OutputTypeField = MakeShared<FJsonObject>();
		OutputTypeField->SetStringField(TEXT("type"), TEXT("string"));
		OutputTypeField->SetStringField(TEXT("description"), TEXT("Primary output type"));
		TArray<TSharedPtr<FJsonValue>> OutputTypeValues;
		OutputTypeValues.Add(MakeShared<FJsonValueString>(TEXT("float")));
		OutputTypeValues.Add(MakeShared<FJsonValueString>(TEXT("float2")));
		OutputTypeValues.Add(MakeShared<FJsonValueString>(TEXT("float3")));
		OutputTypeValues.Add(MakeShared<FJsonValueString>(TEXT("float4")));
		OutputTypeValues.Add(MakeShared<FJsonValueString>(TEXT("material_attributes")));
		OutputTypeField->SetArrayField(TEXT("valid_values"), OutputTypeValues);
		Schema->SetObjectField(TEXT("output_type"), OutputTypeField);

		// inputs field
		auto InputsField = MakeShared<FJsonObject>();
		InputsField->SetStringField(TEXT("type"), TEXT("array of string"));
		InputsField->SetStringField(TEXT("description"),
			TEXT("Input pin names. Replaces all existing inputs. Use add_inputs to append instead."));
		InputsField->SetStringField(TEXT("example"), TEXT("[\"UV\", \"Speed\", \"Color\"]"));
		Schema->SetObjectField(TEXT("inputs"), InputsField);

		// add_inputs field
		auto AddInputsField = MakeShared<FJsonObject>();
		AddInputsField->SetStringField(TEXT("type"), TEXT("array of string"));
		AddInputsField->SetStringField(TEXT("description"),
			TEXT("Append new input pins without disturbing existing ones or their connections."));
		Schema->SetObjectField(TEXT("add_inputs"), AddInputsField);

		// outputs field
		auto OutputsField = MakeShared<FJsonObject>();
		OutputsField->SetStringField(TEXT("type"), TEXT("array of {name: string, type: string}"));
		OutputsField->SetStringField(TEXT("description"),
			TEXT("Additional output pins beyond the primary output. Same type values as output_type."));
		OutputsField->SetStringField(TEXT("example"),
			TEXT("[{\"name\": \"Mask\", \"type\": \"float\"}, {\"name\": \"UV\", \"type\": \"float2\"}]"));
		Schema->SetObjectField(TEXT("outputs"), OutputsField);

		// Full node example
		Schema->SetStringField(TEXT("full_example"),
			TEXT("{\"type\": \"Custom\", \"code\": \"return UV.x * Speed;\", ")
			TEXT("\"description\": \"UV Scroller\", \"output_type\": \"float2\", ")
			TEXT("\"inputs\": [\"UV\", \"Speed\"], \"pos_x\": -400, \"pos_y\": 0}"));

		Result->SetObjectField(TEXT("custom_hlsl_schema"), Schema);
	}

	// ---- Special handling: Substrate BSDF nodes ----
	const bool bIsSubstrate = ExprClass->IsChildOf(UMaterialExpressionSubstrateBSDF::StaticClass());
	if (bIsSubstrate)
	{
		Result->SetStringField(TEXT("substrate_note"),
			TEXT("This is a Substrate BSDF node. Its output type is 'Substrate' (not float). ")
			TEXT("Connect it to the material's FrontMaterial pin: ")
			TEXT("{\"from_node\": N, \"to_node\": \"material\", \"to_pin\": \"FrontMaterial\"}. ")
			TEXT("Substrate must be enabled in Project Settings > Rendering > Substrate."));
	}

	// ---- MaterialFunctionCall usage note ----
	if (bIsFunctionCall)
	{
		auto UsageObj = MakeShared<FJsonObject>();
		UsageObj->SetStringField(TEXT("description"),
			TEXT("To use a Material Function in build_material_graph, set the function_path property:"));
		UsageObj->SetStringField(TEXT("example"),
			TEXT("{\"type\": \"MaterialFunctionCall\", \"properties\": {\"function_path\": \"/Engine/Functions/...\"}}"));
		UsageObj->SetStringField(TEXT("discover_functions"),
			TEXT("Use search_material_functions(filter, include_engine=true) to find functions, ")
			TEXT("then get_material_function_info(function_path) to see inputs/outputs."));
		Result->SetObjectField(TEXT("usage"), UsageObj);
	}

	// Clean up temp object
	Temp->MarkAsGarbage();

	auto R = MakeShared<FJsonObject>();
	R->SetBoolField(TEXT("success"), true);
	R->SetObjectField(TEXT("info"), Result);
	return R;
}

// ---------------------------------------------------------------------------
// HandleGetAvailableMaterialPins
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleGetAvailableMaterialPins(
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

	// Gather all visible pins
	TArray<FMaterialPinInfo> Pins;
	GetMaterialPropertyPins(Material, Pins);

	// Also build a map of what's connected (reuse existing helper)
	FMaterialExpressionCollection& Collection = Material->GetExpressionCollection();
	TMap<UMaterialExpression*, int32> ExprIndexMap;
	for (int32 i = 0; i < Collection.Expressions.Num(); ++i)
	{
		if (UMaterialExpression* E = Collection.Expressions[i])
		{
			ExprIndexMap.Add(E, i);
		}
	}

	TArray<TSharedPtr<FJsonValue>> PinsArr;
	for (const FMaterialPinInfo& Pin : Pins)
	{
		auto PinObj = MakeShared<FJsonObject>();
		PinObj->SetStringField(TEXT("name"), Pin.Name);
		PinObj->SetStringField(TEXT("expected_type"), Pin.ExpectedType);
		PinObj->SetBoolField(TEXT("connected"), Pin.bConnected);

		// If connected, report which node feeds it
		if (Pin.bConnected)
		{
			UMaterialExpression* InputNode = UMaterialEditingLibrary::GetMaterialPropertyInputNode(Material, Pin.Property);
			if (InputNode)
			{
				const int32* IdxPtr = ExprIndexMap.Find(InputNode);
				PinObj->SetNumberField(TEXT("connected_node_index"), IdxPtr ? *IdxPtr : -1);
				FString NodeType = InputNode->GetClass()->GetName();
				NodeType.RemoveFromStart(TEXT("MaterialExpression"));
				PinObj->SetStringField(TEXT("connected_node_type"), NodeType);
			}
		}

		PinsArr.Add(MakeShared<FJsonValueObject>(PinObj));
	}

	// Report Substrate status
	bool bSubstrateEnabled = Substrate::IsSubstrateEnabled();
	bool bHasFrontMaterial = Material->HasSubstrateFrontMaterialConnected();

	auto R = MakeShared<FJsonObject>();
	R->SetBoolField(TEXT("success"), true);
	R->SetStringField(TEXT("path"), MaterialPath);
	R->SetNumberField(TEXT("pin_count"), PinsArr.Num());
	R->SetArrayField(TEXT("pins"), PinsArr);
	R->SetBoolField(TEXT("substrate_enabled"), bSubstrateEnabled);
	R->SetBoolField(TEXT("has_front_material_connected"), bHasFrontMaterial);

	if (bSubstrateEnabled)
	{
		R->SetStringField(TEXT("substrate_note"),
			TEXT("Substrate is enabled. Use FrontMaterial pin with Substrate BSDF nodes "
			     "(SubstrateSlabBSDF, SubstrateShadingModels, SubstrateSimpleClearCoatBSDF, etc.). "
			     "Traditional pins (BaseColor, Metallic, etc.) are auto-converted when Substrate is active."));
	}

	return R;
}

// ---------------------------------------------------------------------------
// HandleSearchMaterialFunctions
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleSearchMaterialFunctions(
	const TSharedPtr<FJsonObject>& Params)
{
	FString Filter;
	Params->TryGetStringField(TEXT("filter"), Filter);

	FString SearchPath = TEXT("/Game");
	Params->TryGetStringField(TEXT("path"), SearchPath);

	int32 MaxResults = 50;
	{
		double Tmp;
		if (Params->TryGetNumberField(TEXT("max_results"), Tmp))
		{
			MaxResults = FMath::Max(1, (int32)Tmp);
		}
	}

	bool bIncludeEngine = false;
	Params->TryGetBoolField(TEXT("include_engine"), bIncludeEngine);

	FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AR = ARM.Get();

	FARFilter ARFilter;
	ARFilter.ClassPaths.Add(UMaterialFunction::StaticClass()->GetClassPathName());
	ARFilter.bRecursiveClasses = true;
	ARFilter.bRecursivePaths = true;
	if (!bIncludeEngine)
	{
		ARFilter.PackagePaths.Add(FName(*SearchPath));
	}

	TArray<FAssetData> Assets;
	AR.GetAssets(ARFilter, Assets);

	TArray<TSharedPtr<FJsonValue>> ResultArray;
	for (const FAssetData& Asset : Assets)
	{
		FString AssetName = Asset.AssetName.ToString();

		if (!Filter.IsEmpty() && !AssetName.Contains(Filter, ESearchCase::IgnoreCase))
		{
			continue;
		}

		auto Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("name"), AssetName);
		Entry->SetStringField(TEXT("path"), Asset.GetObjectPathString());
		ResultArray.Add(MakeShared<FJsonValueObject>(Entry));

		if (ResultArray.Num() >= MaxResults)
		{
			break;
		}
	}

	auto R = MakeShared<FJsonObject>();
	R->SetBoolField(TEXT("success"), true);
	R->SetNumberField(TEXT("count"), ResultArray.Num());
	if (!Filter.IsEmpty())
	{
		R->SetStringField(TEXT("filter"), Filter);
	}
	R->SetArrayField(TEXT("functions"), ResultArray);
	return R;
}

// ---------------------------------------------------------------------------
// HandleValidateMaterialGraph
//
// Analyses every node and categorises connection health:
//   orphaned      — neither inputs nor outputs connected to anything
//   dead_ends     — inputs connected but output goes nowhere
//   missing_inputs — some input pins are empty (may be intentional for fallback constants)
//   feeds_material — node (or downstream chain) reaches a material output
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleValidateMaterialGraph(
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

	// --- Pass 1: Build reverse connection map (who consumes each node's output) ---
	// OutputConsumers[i] = list of {consumer_index, input_pin_name}
	struct FConsumerEntry
	{
		int32 ConsumerIndex;
		FString InputPinName;
	};
	TMap<int32, TArray<FConsumerEntry>> OutputConsumers;

	for (int32 i = 0; i < NumExprs; ++i)
	{
		UMaterialExpression* Expr = Collection.Expressions[i];
		if (!Expr)
		{
			continue;
		}

		// Check standard inputs
		if (UMaterialExpressionCustom* Custom = Cast<UMaterialExpressionCustom>(Expr))
		{
			for (const FCustomInput& CInp : Custom->Inputs)
			{
				if (CInp.Input.Expression)
				{
					const int32* SrcIdx = ExprIndexMap.Find(CInp.Input.Expression);
					if (SrcIdx)
					{
						OutputConsumers.FindOrAdd(*SrcIdx).Add({i, CInp.InputName.ToString()});
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
					const int32* SrcIdx = ExprIndexMap.Find(Input->Expression);
					if (SrcIdx)
					{
						FName PinName = Expr->GetInputName(j);
						FString PinStr = PinName.IsNone()
							? FString::Printf(TEXT("Input_%d"), j)
							: PinName.ToString();
						OutputConsumers.FindOrAdd(*SrcIdx).Add({i, PinStr});
					}
				}
			}
		}
	}

	// --- Pass 1b: Check material output connections ---
	TSet<int32> FeedsMaterialOutput;
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
				FeedsMaterialOutput.Add(*Idx);
				OutputConsumers.FindOrAdd(*Idx).Add({-1, MaterialPropertyToString(Prop)});
			}
		}
	}

	// --- Pass 2: Analyse each node ---
	TArray<TSharedPtr<FJsonValue>> OrphanedArr;
	TArray<TSharedPtr<FJsonValue>> DeadEndArr;
	TArray<TSharedPtr<FJsonValue>> MissingInputArr;
	TArray<TSharedPtr<FJsonValue>> UnconnectedInputArr;
	TArray<TSharedPtr<FJsonValue>> UnconnectedOutputArr;

	for (int32 i = 0; i < NumExprs; ++i)
	{
		UMaterialExpression* Expr = Collection.Expressions[i];
		if (!Expr)
		{
			continue;
		}

		FString ShortType = Expr->GetClass()->GetName();
		ShortType.RemoveFromStart(TEXT("MaterialExpression"));

		// Count connected inputs
		int32 TotalInputs = 0;
		int32 ConnectedInputs = 0;
		TArray<FString> EmptyInputNames;

		if (UMaterialExpressionCustom* Custom = Cast<UMaterialExpressionCustom>(Expr))
		{
			for (const FCustomInput& CInp : Custom->Inputs)
			{
				TotalInputs++;
				if (CInp.Input.Expression)
				{
					ConnectedInputs++;
				}
				else
				{
					EmptyInputNames.Add(CInp.InputName.ToString());
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
				TotalInputs++;
				if (Input->Expression)
				{
					ConnectedInputs++;
				}
				else
				{
					FName PinName = Expr->GetInputName(j);
					EmptyInputNames.Add(PinName.IsNone()
						? FString::Printf(TEXT("Input_%d"), j) : PinName.ToString());
				}
			}
		}

		bool bHasAnyInputConnected = (ConnectedInputs > 0);
		bool bOutputUsed = OutputConsumers.Contains(i);

		// Helper to build a node summary
		auto MakeNodeObj = [&]() -> TSharedPtr<FJsonObject>
		{
			auto Obj = MakeShared<FJsonObject>();
			Obj->SetNumberField(TEXT("index"), i);
			Obj->SetStringField(TEXT("type"), ShortType);
			Obj->SetNumberField(TEXT("pos_x"), Expr->MaterialExpressionEditorX);
			Obj->SetNumberField(TEXT("pos_y"), Expr->MaterialExpressionEditorY);
			return Obj;
		};

		// Orphaned: nothing connected at all (no inputs AND output not consumed)
		if (!bHasAnyInputConnected && !bOutputUsed)
		{
			auto Obj = MakeNodeObj();
			Obj->SetStringField(TEXT("reason"), TEXT("No inputs connected and output not consumed by anything"));
			OrphanedArr.Add(MakeShared<FJsonValueObject>(Obj));
		}
		// Dead-end: has input(s) but output goes nowhere
		else if (bHasAnyInputConnected && !bOutputUsed)
		{
			auto Obj = MakeNodeObj();
			Obj->SetStringField(TEXT("reason"), TEXT("Has inputs but output is not connected to any node or material output"));
			DeadEndArr.Add(MakeShared<FJsonValueObject>(Obj));
		}

		// Unconnected outputs: output not consumed (regardless of input state)
		if (!bOutputUsed)
		{
			auto Obj = MakeNodeObj();
			UnconnectedOutputArr.Add(MakeShared<FJsonValueObject>(Obj));
		}

		// Unconnected inputs: no inputs at all (but this node expects them)
		if (TotalInputs > 0 && ConnectedInputs == 0)
		{
			auto Obj = MakeNodeObj();
			TArray<TSharedPtr<FJsonValue>> PinArr;
			for (const FString& P : EmptyInputNames)
			{
				PinArr.Add(MakeShared<FJsonValueString>(P));
			}
			Obj->SetArrayField(TEXT("empty_pins"), PinArr);
			UnconnectedInputArr.Add(MakeShared<FJsonValueObject>(Obj));
		}
		// Missing inputs: some connected, some empty
		else if (TotalInputs > 0 && ConnectedInputs > 0 && EmptyInputNames.Num() > 0)
		{
			auto Obj = MakeNodeObj();
			Obj->SetNumberField(TEXT("connected"), ConnectedInputs);
			Obj->SetNumberField(TEXT("total"), TotalInputs);
			TArray<TSharedPtr<FJsonValue>> PinArr;
			for (const FString& P : EmptyInputNames)
			{
				PinArr.Add(MakeShared<FJsonValueString>(P));
			}
			Obj->SetArrayField(TEXT("empty_pins"), PinArr);
			MissingInputArr.Add(MakeShared<FJsonValueObject>(Obj));
		}
	}

	auto R = MakeShared<FJsonObject>();
	R->SetBoolField(TEXT("success"), true);
	R->SetStringField(TEXT("path"), MaterialPath);
	R->SetNumberField(TEXT("total_nodes"), NumExprs);

	bool bHealthy = OrphanedArr.IsEmpty() && DeadEndArr.IsEmpty();
	R->SetBoolField(TEXT("healthy"), bHealthy);

	auto Issues = MakeShared<FJsonObject>();
	Issues->SetNumberField(TEXT("orphaned_count"), OrphanedArr.Num());
	Issues->SetArrayField(TEXT("orphaned"), OrphanedArr);
	Issues->SetNumberField(TEXT("dead_end_count"), DeadEndArr.Num());
	Issues->SetArrayField(TEXT("dead_ends"), DeadEndArr);
	Issues->SetNumberField(TEXT("missing_input_count"), MissingInputArr.Num());
	Issues->SetArrayField(TEXT("missing_inputs"), MissingInputArr);
	Issues->SetNumberField(TEXT("unconnected_input_count"), UnconnectedInputArr.Num());
	Issues->SetArrayField(TEXT("unconnected_inputs"), UnconnectedInputArr);
	Issues->SetNumberField(TEXT("unconnected_output_count"), UnconnectedOutputArr.Num());
	Issues->SetArrayField(TEXT("unconnected_outputs"), UnconnectedOutputArr);
	R->SetObjectField(TEXT("issues"), Issues);

	return R;
}

// ---------------------------------------------------------------------------
// HandleTraceMaterialConnection
//
// Traces connections upstream (what feeds into a node) and downstream
// (what consumes a node's output). Supports single-node and recursive trace.
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleTraceMaterialConnection(
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

	FString Direction = TEXT("both");
	Params->TryGetStringField(TEXT("direction"), Direction);

	int32 MaxDepth = 1;
	{
		double Tmp;
		if (Params->TryGetNumberField(TEXT("max_depth"), Tmp))
		{
			MaxDepth = FMath::Clamp((int32)Tmp, 1, 50);
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

	TMap<UMaterialExpression*, int32> ExprIndexMap;
	ExprIndexMap.Reserve(Collection.Expressions.Num());
	for (int32 i = 0; i < Collection.Expressions.Num(); ++i)
	{
		if (UMaterialExpression* E = Collection.Expressions[i])
		{
			ExprIndexMap.Add(E, i);
		}
	}

	// Helper: get short type name
	auto GetShortType = [](UMaterialExpression* Expr) -> FString
	{
		FString S = Expr->GetClass()->GetName();
		S.RemoveFromStart(TEXT("MaterialExpression"));
		return S;
	};

	// --- Build reverse map: for each expression, who consumes its output ---
	struct FDownstreamEntry
	{
		int32 ConsumerIndex; // -1 = material output
		FString ConsumerPin;
		FString SourcePin;
	};
	TMap<int32, TArray<FDownstreamEntry>> DownstreamMap;

	for (int32 i = 0; i < Collection.Expressions.Num(); ++i)
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
					const int32* SrcIdx = ExprIndexMap.Find(CInp.Input.Expression);
					if (SrcIdx)
					{
						FString SrcPin;
						TArray<FExpressionOutput>& SrcOuts = CInp.Input.Expression->GetOutputs();
						if (SrcOuts.IsValidIndex(CInp.Input.OutputIndex) &&
							!SrcOuts[CInp.Input.OutputIndex].OutputName.IsNone())
						{
							SrcPin = SrcOuts[CInp.Input.OutputIndex].OutputName.ToString();
						}
						DownstreamMap.FindOrAdd(*SrcIdx).Add({i, CInp.InputName.ToString(), SrcPin});
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
					const int32* SrcIdx = ExprIndexMap.Find(Input->Expression);
					if (SrcIdx)
					{
						FName PinName = Expr->GetInputName(j);
						FString PinStr = PinName.IsNone()
							? FString::Printf(TEXT("Input_%d"), j) : PinName.ToString();
						FString SrcPin;
						TArray<FExpressionOutput>& SrcOuts = Input->Expression->GetOutputs();
						if (SrcOuts.IsValidIndex(Input->OutputIndex) &&
							!SrcOuts[Input->OutputIndex].OutputName.IsNone())
						{
							SrcPin = SrcOuts[Input->OutputIndex].OutputName.ToString();
						}
						DownstreamMap.FindOrAdd(*SrcIdx).Add({i, PinStr, SrcPin});
					}
				}
			}
		}
	}

	// Material output connections
	static const EMaterialProperty AllMatProps[] = {
		MP_BaseColor, MP_Metallic, MP_Roughness, MP_EmissiveColor,
		MP_Opacity, MP_OpacityMask, MP_Normal, MP_Specular,
		MP_AmbientOcclusion, MP_WorldPositionOffset, MP_SubsurfaceColor, MP_Refraction,
		MP_PixelDepthOffset, MP_ShadingModel, MP_Anisotropy, MP_Tangent,
		MP_CustomData0, MP_CustomData1, MP_SurfaceThickness, MP_Displacement, MP_FrontMaterial
	};
	for (EMaterialProperty Prop : AllMatProps)
	{
		UMaterialExpression* Node = UMaterialEditingLibrary::GetMaterialPropertyInputNode(Material, Prop);
		if (Node)
		{
			const int32* Idx = ExprIndexMap.Find(Node);
			if (Idx)
			{
				FString OutName = UMaterialEditingLibrary::GetMaterialPropertyInputNodeOutputName(Material, Prop);
				DownstreamMap.FindOrAdd(*Idx).Add({-1, MaterialPropertyToString(Prop), OutName});
			}
		}
	}

	// --- Trace upstream (recursive) ---
	auto TraceUpstream = [&](int32 StartIdx, int32 Depth) -> TSharedPtr<FJsonObject>
	{
		auto TraceUpstreamImpl = [&](auto& Self, int32 Idx, int32 CurDepth, TSet<int32>& Visited) -> TSharedPtr<FJsonObject>
		{
			if (CurDepth > Depth || Visited.Contains(Idx))
			{
				return nullptr;
			}
			Visited.Add(Idx);

			UMaterialExpression* Expr = Collection.Expressions[Idx];
			if (!Expr)
			{
				return nullptr;
			}

			auto NodeObj = MakeShared<FJsonObject>();
			NodeObj->SetNumberField(TEXT("index"), Idx);
			NodeObj->SetStringField(TEXT("type"), GetShortType(Expr));

			TArray<TSharedPtr<FJsonValue>> InputsArr;

			if (UMaterialExpressionCustom* Custom = Cast<UMaterialExpressionCustom>(Expr))
			{
				for (const FCustomInput& CInp : Custom->Inputs)
				{
					auto PinObj = MakeShared<FJsonObject>();
					PinObj->SetStringField(TEXT("pin"), CInp.InputName.ToString());

					if (CInp.Input.Expression)
					{
						const int32* SrcIdx = ExprIndexMap.Find(CInp.Input.Expression);
						if (SrcIdx)
						{
							PinObj->SetNumberField(TEXT("from_node"), *SrcIdx);
							PinObj->SetStringField(TEXT("from_type"), GetShortType(CInp.Input.Expression));

							if (CurDepth < Depth)
							{
								TSharedPtr<FJsonObject> SubTrace = Self(Self, *SrcIdx, CurDepth + 1, Visited);
								if (SubTrace.IsValid())
								{
									PinObj->SetObjectField(TEXT("trace"), SubTrace);
								}
							}
						}
					}
					else
					{
						PinObj->SetStringField(TEXT("from_node"), TEXT("(empty)"));
					}
					InputsArr.Add(MakeShared<FJsonValueObject>(PinObj));
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

					FName PinName = Expr->GetInputName(j);
					FString PinStr = PinName.IsNone()
						? FString::Printf(TEXT("Input_%d"), j) : PinName.ToString();

					auto PinObj = MakeShared<FJsonObject>();
					PinObj->SetStringField(TEXT("pin"), PinStr);

					if (Input->Expression)
					{
						const int32* SrcIdx = ExprIndexMap.Find(Input->Expression);
						if (SrcIdx)
						{
							PinObj->SetNumberField(TEXT("from_node"), *SrcIdx);
							PinObj->SetStringField(TEXT("from_type"), GetShortType(Input->Expression));

							if (CurDepth < Depth)
							{
								TSharedPtr<FJsonObject> SubTrace = Self(Self, *SrcIdx, CurDepth + 1, Visited);
								if (SubTrace.IsValid())
								{
									PinObj->SetObjectField(TEXT("trace"), SubTrace);
								}
							}
						}
					}
					else
					{
						PinObj->SetStringField(TEXT("from_node"), TEXT("(empty)"));
					}
					InputsArr.Add(MakeShared<FJsonValueObject>(PinObj));
				}
			}

			NodeObj->SetArrayField(TEXT("inputs"), InputsArr);
			return NodeObj;
		};

		TSet<int32> Visited;
		return TraceUpstreamImpl(TraceUpstreamImpl, StartIdx, 1, Visited);
	};

	// --- Trace downstream ---
	auto TraceDownstream = [&](int32 StartIdx, int32 Depth) -> TSharedPtr<FJsonObject>
	{
		auto TraceDownstreamImpl = [&](auto& Self, int32 Idx, int32 CurDepth, TSet<int32>& Visited) -> TSharedPtr<FJsonObject>
		{
			if (CurDepth > Depth || Visited.Contains(Idx))
			{
				return nullptr;
			}
			Visited.Add(Idx);

			UMaterialExpression* Expr = Collection.Expressions[Idx];
			if (!Expr)
			{
				return nullptr;
			}

			auto NodeObj = MakeShared<FJsonObject>();
			NodeObj->SetNumberField(TEXT("index"), Idx);
			NodeObj->SetStringField(TEXT("type"), GetShortType(Expr));

			TArray<TSharedPtr<FJsonValue>> OutputsArr;
			const TArray<FDownstreamEntry>* Consumers = DownstreamMap.Find(Idx);
			if (Consumers)
			{
				for (const FDownstreamEntry& Entry : *Consumers)
				{
					auto ConnObj = MakeShared<FJsonObject>();
					if (!Entry.SourcePin.IsEmpty())
					{
						ConnObj->SetStringField(TEXT("from_pin"), Entry.SourcePin);
					}

					if (Entry.ConsumerIndex == -1)
					{
						ConnObj->SetStringField(TEXT("to_node"), TEXT("material"));
						ConnObj->SetStringField(TEXT("to_pin"), Entry.ConsumerPin);
					}
					else
					{
						ConnObj->SetNumberField(TEXT("to_node"), Entry.ConsumerIndex);
						ConnObj->SetStringField(TEXT("to_type"),
							GetShortType(Collection.Expressions[Entry.ConsumerIndex]));
						ConnObj->SetStringField(TEXT("to_pin"), Entry.ConsumerPin);

						if (CurDepth < Depth)
						{
							TSharedPtr<FJsonObject> SubTrace = Self(Self, Entry.ConsumerIndex, CurDepth + 1, Visited);
							if (SubTrace.IsValid())
							{
								ConnObj->SetObjectField(TEXT("trace"), SubTrace);
							}
						}
					}
					OutputsArr.Add(MakeShared<FJsonValueObject>(ConnObj));
				}
			}

			if (OutputsArr.IsEmpty())
			{
				auto DeadObj = MakeShared<FJsonObject>();
				DeadObj->SetStringField(TEXT("to_node"), TEXT("(nothing)"));
				OutputsArr.Add(MakeShared<FJsonValueObject>(DeadObj));
			}

			NodeObj->SetArrayField(TEXT("outputs"), OutputsArr);
			return NodeObj;
		};

		TSet<int32> Visited;
		return TraceDownstreamImpl(TraceDownstreamImpl, StartIdx, 1, Visited);
	};

	// Build result
	UMaterialExpression* TargetExpr = Collection.Expressions[NodeIndex];
	auto R = MakeShared<FJsonObject>();
	R->SetBoolField(TEXT("success"), true);
	R->SetStringField(TEXT("path"), MaterialPath);
	R->SetNumberField(TEXT("node_index"), NodeIndex);
	R->SetStringField(TEXT("node_type"), GetShortType(TargetExpr));

	bool bDoUpstream = (Direction == TEXT("upstream") || Direction == TEXT("both"));
	bool bDoDownstream = (Direction == TEXT("downstream") || Direction == TEXT("both"));

	if (bDoUpstream)
	{
		TSharedPtr<FJsonObject> UpTrace = TraceUpstream(NodeIndex, MaxDepth);
		if (UpTrace.IsValid())
		{
			R->SetObjectField(TEXT("upstream"), UpTrace);
		}
	}

	if (bDoDownstream)
	{
		TSharedPtr<FJsonObject> DownTrace = TraceDownstream(NodeIndex, MaxDepth);
		if (DownTrace.IsValid())
		{
			R->SetObjectField(TEXT("downstream"), DownTrace);
		}
	}

	return R;
}
