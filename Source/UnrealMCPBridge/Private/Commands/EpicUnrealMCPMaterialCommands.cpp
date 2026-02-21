#include "Commands/EpicUnrealMCPMaterialCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"

#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionComment.h"
#include "Materials/MaterialExpression.h"
#include "MaterialEditingLibrary.h"
#include "EdGraph/EdGraphNode.h"
#include "MaterialEditor/MaterialEditorInstanceConstant.h"
#include "MaterialShared.h"
#include "RHIShaderPlatform.h"
#include "EditorAssetLibrary.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Factories/MaterialFactoryNew.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "MaterialGraph/MaterialGraph.h"
#include "MaterialGraph/MaterialGraphNode.h"
#include "IMaterialEditor.h"
#include "Subsystems/AssetEditorSubsystem.h"

// ---------------------------------------------------------------------------
// Helper: Find the material editor's preview material for a given original.
//
// The material editor works on a transient UPreviewMaterial copy, not the
// original asset. The MaterialGraph (needed for visual updates) is only set
// on this copy. We use UAssetEditorSubsystem::FindEditorForAsset() and the
// IMaterialEditor interface to locate it — matching the pattern Epic uses
// in FMaterialEditor itself (MaterialEditor.cpp line 8268).
// ---------------------------------------------------------------------------

/** Get the IMaterialEditor and its preview material for a given original asset. */
static bool GetMaterialEditorContext(UMaterial* OriginalMaterial,
                                     IMaterialEditor*& OutEditor,
                                     UMaterial*& OutPreviewMaterial)
{
	OutEditor = nullptr;
	OutPreviewMaterial = nullptr;

	if (!OriginalMaterial || !GEditor) return false;

	UAssetEditorSubsystem* Sub = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (!Sub) return false;

	IAssetEditorInstance* EditorInstance = Sub->FindEditorForAsset(OriginalMaterial, /*bFocusIfOpen=*/false);
	if (!EditorInstance) return false;
	if (EditorInstance->GetEditorName() != FName("MaterialEditor")) return false;

	// Safe cast — same pattern as MaterialEditor.cpp line 8268:
	// "We've ensured that this is a valid cast by checking GetEditorName()"
	OutEditor = static_cast<IMaterialEditor*>(EditorInstance);
	OutPreviewMaterial = Cast<UMaterial>(OutEditor->GetMaterialInterface());

	return OutPreviewMaterial != nullptr && OutPreviewMaterial->MaterialGraph != nullptr;
}

/** Resolve a material to its editor preview copy (if the editor is open). */
static UMaterial* ResolveWorkingMaterial(UMaterial* OriginalMaterial)
{
	IMaterialEditor* Editor = nullptr;
	UMaterial* PreviewMat = nullptr;
	if (GetMaterialEditorContext(OriginalMaterial, Editor, PreviewMat))
		return PreviewMat;
	return OriginalMaterial;
}

// ---------------------------------------------------------------------------
// Helper: Refresh the material editor graph UI so changes appear in real time.
//
// Finds the preview material via GetMaterialEditorContext, then:
//  1. For each expression without a graph node, call AddExpression() to
//     create the visual UMaterialGraphNode wrapper.
//  2. Call LinkGraphNodesFromMaterial() to sync all pin connections.
//  3. Call IMaterialEditor::UpdateMaterialAfterGraphChange() which handles
//     recompilation, expression previews, dirty flag, and visual redraw.
// ---------------------------------------------------------------------------

static void NotifyMaterialEditorRefresh(UMaterial* OriginalMaterial)
{
	IMaterialEditor* Editor = nullptr;
	UMaterial* PreviewMat = nullptr;
	if (!GetMaterialEditorContext(OriginalMaterial, Editor, PreviewMat))
		return; // Editor not open — nothing to refresh

	UMaterialGraph* Graph = PreviewMat->MaterialGraph;

	// Create graph nodes for any expressions that lack one (newly added via library)
	for (UMaterialExpression* Expr : PreviewMat->GetExpressions())
	{
		if (Expr && !Expr->GraphNode)
		{
			Graph->AddExpression(Expr, /*bUserInvoked=*/false);
		}
	}

	// Sync all pin connections from expression data
	Graph->LinkGraphNodesFromMaterial();

	// Recompile preview, refresh expression previews, mark dirty, update visuals
	Editor->UpdateMaterialAfterGraphChange();
}

// Full rebuild variant — used after deletions or bulk graph builds where
// the entire node set may have changed.
static void RebuildMaterialEditorGraph(UMaterial* OriginalMaterial)
{
	IMaterialEditor* Editor = nullptr;
	UMaterial* PreviewMat = nullptr;
	if (!GetMaterialEditorContext(OriginalMaterial, Editor, PreviewMat))
		return;

	PreviewMat->MaterialGraph->RebuildGraph();
	Editor->UpdateMaterialAfterGraphChange();
}

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

FEpicUnrealMCPMaterialCommands::FEpicUnrealMCPMaterialCommands()
{
}

// ---------------------------------------------------------------------------
// Command Dispatch
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleCommand(
	const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
	// ---- Creation & Bulk ----
	if      (CommandType == TEXT("create_material"))              return HandleCreateMaterial(Params);
	else if (CommandType == TEXT("create_material_instance"))     return HandleCreateMaterialInstance(Params);
	else if (CommandType == TEXT("build_material_graph"))         return HandleBuildMaterialGraph(Params);
	else if (CommandType == TEXT("set_material_properties"))      return HandleSetMaterialProperties(Params);
	else if (CommandType == TEXT("add_material_comments"))        return HandleAddMaterialComments(Params);
	else if (CommandType == TEXT("recompile_material"))           return HandleRecompileMaterial(Params);
	// ---- Read ----
	else if (CommandType == TEXT("get_material_info"))            return HandleGetMaterialInfo(Params);
	else if (CommandType == TEXT("get_material_graph_nodes"))     return HandleGetMaterialGraphNodes(Params);
	else if (CommandType == TEXT("get_material_expression_info")) return HandleGetMaterialExpressionInfo(Params);
	else if (CommandType == TEXT("get_material_property_connections")) return HandleGetMaterialPropertyConnections(Params);
	else if (CommandType == TEXT("get_material_errors"))             return HandleGetMaterialErrors(Params);
	// ---- Node Mutations ----
	else if (CommandType == TEXT("add_material_expression"))      return HandleAddMaterialExpression(Params);
	else if (CommandType == TEXT("set_material_expression_property")) return HandleSetMaterialExpressionProperty(Params);
	else if (CommandType == TEXT("move_material_expression"))     return HandleMoveMaterialExpression(Params);
	else if (CommandType == TEXT("duplicate_material_expression")) return HandleDuplicateMaterialExpression(Params);
	else if (CommandType == TEXT("delete_material_expression"))   return HandleDeleteMaterialExpression(Params);
	else if (CommandType == TEXT("connect_material_expressions")) return HandleConnectMaterialExpressions(Params);
	else if (CommandType == TEXT("layout_material_expressions"))  return HandleLayoutMaterialExpressions(Params);
	// ---- Material Instance ----
	else if (CommandType == TEXT("get_material_instance_parameters")) return HandleGetMaterialInstanceParameters(Params);
	else if (CommandType == TEXT("set_material_instance_parameter"))  return HandleSetMaterialInstanceParameter(Params);
	// ---- Discovery ----
	else if (CommandType == TEXT("list_material_expression_types"))   return HandleListMaterialExpressionTypes(Params);

	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
		FString::Printf(TEXT("Unknown material command: %s"), *CommandType));
}

// ---------------------------------------------------------------------------
// Enum Resolution
// ---------------------------------------------------------------------------

EBlendMode FEpicUnrealMCPMaterialCommands::ResolveBlendMode(const FString& Name)
{
	FString L = Name.ToLower();
	if (L == TEXT("masked"))                              return BLEND_Masked;
	if (L == TEXT("translucent"))                        return BLEND_Translucent;
	if (L == TEXT("additive"))                           return BLEND_Additive;
	if (L == TEXT("modulate"))                           return BLEND_Modulate;
	if (L == TEXT("alpha_composite") || L == TEXT("alphacomposite")) return BLEND_AlphaComposite;
	if (L == TEXT("alpha_holdout")   || L == TEXT("alphaholdout"))   return BLEND_AlphaHoldout;
	return BLEND_Opaque;
}

EMaterialShadingModel FEpicUnrealMCPMaterialCommands::ResolveShadingModel(const FString& Name)
{
	FString L = Name.ToLower();
	if (L == TEXT("unlit"))                                              return MSM_Unlit;
	if (L == TEXT("subsurface"))                                         return MSM_Subsurface;
	if (L == TEXT("clear_coat")        || L == TEXT("clearcoat"))        return MSM_ClearCoat;
	if (L == TEXT("subsurface_profile") || L == TEXT("subsurfaceprofile")) return MSM_SubsurfaceProfile;
	if (L == TEXT("two_sided_foliage") || L == TEXT("twosidedfoliage")) return MSM_TwoSidedFoliage;
	if (L == TEXT("cloth"))                                              return MSM_Cloth;
	if (L == TEXT("eye"))                                                return MSM_Eye;
	if (L == TEXT("thin_translucent") || L == TEXT("thintranslucent"))  return MSM_ThinTranslucent;
	return MSM_DefaultLit;
}

EMaterialProperty FEpicUnrealMCPMaterialCommands::ResolveMaterialProperty(const FString& Name)
{
	if (Name == TEXT("BaseColor"))           return MP_BaseColor;
	if (Name == TEXT("Metallic"))            return MP_Metallic;
	if (Name == TEXT("Roughness"))           return MP_Roughness;
	if (Name == TEXT("EmissiveColor"))       return MP_EmissiveColor;
	if (Name == TEXT("Opacity"))             return MP_Opacity;
	if (Name == TEXT("OpacityMask"))         return MP_OpacityMask;
	if (Name == TEXT("Normal"))              return MP_Normal;
	if (Name == TEXT("Specular"))            return MP_Specular;
	if (Name == TEXT("AmbientOcclusion"))    return MP_AmbientOcclusion;
	if (Name == TEXT("WorldPositionOffset")) return MP_WorldPositionOffset;
	if (Name == TEXT("SubsurfaceColor"))     return MP_SubsurfaceColor;
	if (Name == TEXT("Refraction"))          return MP_Refraction;
	if (Name == TEXT("PixelDepthOffset"))    return MP_PixelDepthOffset;
	if (Name == TEXT("ShadingModel"))        return MP_ShadingModel;
	return MP_MAX;
}

FString FEpicUnrealMCPMaterialCommands::MaterialPropertyToString(EMaterialProperty Prop)
{
	switch (Prop)
	{
	case MP_BaseColor:           return TEXT("BaseColor");
	case MP_Metallic:            return TEXT("Metallic");
	case MP_Roughness:           return TEXT("Roughness");
	case MP_EmissiveColor:       return TEXT("EmissiveColor");
	case MP_Opacity:             return TEXT("Opacity");
	case MP_OpacityMask:         return TEXT("OpacityMask");
	case MP_Normal:              return TEXT("Normal");
	case MP_Specular:            return TEXT("Specular");
	case MP_AmbientOcclusion:    return TEXT("AmbientOcclusion");
	case MP_WorldPositionOffset: return TEXT("WorldPositionOffset");
	case MP_SubsurfaceColor:     return TEXT("SubsurfaceColor");
	case MP_Refraction:          return TEXT("Refraction");
	case MP_PixelDepthOffset:    return TEXT("PixelDepthOffset");
	default:                     return TEXT("Unknown");
	}
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

UClass* FEpicUnrealMCPMaterialCommands::FindExpressionClass(const FString& TypeName)
{
	FString ClassName = TypeName;
	if (!ClassName.StartsWith(TEXT("MaterialExpression")))
		ClassName = TEXT("MaterialExpression") + ClassName;

	static const TCHAR* Modules[] = {
		TEXT("Engine"), TEXT("Landscape"), TEXT("HairStrands")
	};
	for (const TCHAR* Mod : Modules)
	{
		FString Path = FString::Printf(TEXT("/Script/%s.%s"), Mod, *ClassName);
		if (UClass* C = FindObject<UClass>(nullptr, *Path))
			return C;
	}
	UE_LOG(LogTemp, Warning, TEXT("MCPMaterial: Cannot find expression class: %s"), *ClassName);
	return nullptr;
}

FString FEpicUnrealMCPMaterialCommands::NormalizePropName(UMaterialExpression* Expr, const FString& Name)
{
	if (!Expr) return Name;
	if (Expr->GetClass()->FindPropertyByName(*Name)) return Name;

	// snake_case → PascalCase
	FString Pascal;
	bool bCapNext = true;
	for (TCHAR Ch : Name)
	{
		if (Ch == TEXT('_'))
			bCapNext = true;
		else if (bCapNext) { Pascal += FChar::ToUpper(Ch); bCapNext = false; }
		else Pascal += Ch;
	}
	if (!Pascal.IsEmpty() && Expr->GetClass()->FindPropertyByName(*Pascal))
		return Pascal;
	return Name; // Return original; failure handled downstream
}

bool FEpicUnrealMCPMaterialCommands::TryParseIntFromJson(const TSharedPtr<FJsonValue>& Val, int32& OutInt)
{
	if (!Val.IsValid()) return false;
	if (Val->Type == EJson::Number)
	{
		OutInt = (int32)Val->AsNumber();
		return true;
	}
	if (Val->Type == EJson::String)
	{
		FString S = Val->AsString();
		if (S.IsNumeric()) { OutInt = FCString::Atoi(*S); return true; }
	}
	return false;
}

bool FEpicUnrealMCPMaterialCommands::SetExpressionProperty(
	UMaterialExpression* Expr, const FString& PropName,
	const TSharedPtr<FJsonValue>& Value, FString& OutError)
{
	if (!Expr || !Value.IsValid())
	{
		OutError = TEXT("Invalid expression or value");
		return false;
	}

	// Normalise to PascalCase so users can pass "parameter_name" OR "ParameterName"
	const FString NormName = NormalizePropName(Expr, PropName);

	// Asset path: string starting with "/" — load and set via object property
	if (Value->Type == EJson::String)
	{
		FString StrVal = Value->AsString();
		if (StrVal.StartsWith(TEXT("/")))
		{
			UObject* Asset = UEditorAssetLibrary::LoadAsset(StrVal);
			if (!Asset)
			{
				OutError = FString::Printf(TEXT("Asset not found: %s"), *StrVal);
				return false;
			}
			FProperty* Prop = Expr->GetClass()->FindPropertyByName(*NormName);
			if (!Prop)
			{
				OutError = FString::Printf(TEXT("Property not found: %s"), *NormName);
				return false;
			}
			if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Prop))
			{
				ObjProp->SetObjectPropertyValue(ObjProp->ContainerPtrToValuePtr<void>(Expr), Asset);
				return true;
			}
			OutError = FString::Printf(TEXT("Property '%s' is not an object property"), *NormName);
			return false;
		}
	}

	// Array value: [r,g,b,a] → FLinearColor, [x,y] → FVector2D
	if (Value->Type == EJson::Array)
	{
		const TArray<TSharedPtr<FJsonValue>>& Arr = Value->AsArray();
		FProperty* Prop = Expr->GetClass()->FindPropertyByName(*NormName);
		if (Prop)
		{
			FStructProperty* SP = CastField<FStructProperty>(Prop);
			if (SP)
			{
				if (SP->Struct == TBaseStructure<FLinearColor>::Get() && Arr.Num() >= 3)
				{
					FLinearColor C(
						(float)Arr[0]->AsNumber(),
						(float)Arr[1]->AsNumber(),
						(float)Arr[2]->AsNumber(),
						Arr.Num() > 3 ? (float)Arr[3]->AsNumber() : 1.0f);
					SP->CopyCompleteValue(SP->ContainerPtrToValuePtr<void>(Expr), &C);
					return true;
				}
				if (SP->Struct == TBaseStructure<FVector2D>::Get() && Arr.Num() >= 2)
				{
					FVector2D V((float)Arr[0]->AsNumber(), (float)Arr[1]->AsNumber());
					SP->CopyCompleteValue(SP->ContainerPtrToValuePtr<void>(Expr), &V);
					return true;
				}
				if (SP->Struct == TBaseStructure<FVector>::Get() && Arr.Num() >= 3)
				{
					FVector V(Arr[0]->AsNumber(), Arr[1]->AsNumber(), Arr[2]->AsNumber());
					SP->CopyCompleteValue(SP->ContainerPtrToValuePtr<void>(Expr), &V);
					return true;
				}
			}
		}
	}

	// Delegate to the generic utility (bool, int, float, string, enum)
	return FEpicUnrealMCPCommonUtils::SetObjectProperty(Expr, NormName, Value, OutError);
}

void FEpicUnrealMCPMaterialCommands::HandleCustomHLSLNode(
	UMaterialExpression* Expr, const TSharedPtr<FJsonObject>& NodeDef)
{
	UMaterialExpressionCustom* Custom = Cast<UMaterialExpressionCustom>(Expr);
	if (!Custom) return;

	FString Code;
	if (NodeDef->TryGetStringField(TEXT("code"), Code)) Custom->Code = Code;

	FString Desc;
	if (NodeDef->TryGetStringField(TEXT("description"), Desc)) Custom->Description = Desc;

	FString OutputTypeStr;
	if (NodeDef->TryGetStringField(TEXT("output_type"), OutputTypeStr))
	{
		FString L = OutputTypeStr.ToLower();
		if      (L == TEXT("float") || L == TEXT("float1")) Custom->OutputType = CMOT_Float1;
		else if (L == TEXT("float2"))                        Custom->OutputType = CMOT_Float2;
		else if (L == TEXT("float3"))                        Custom->OutputType = CMOT_Float3;
		else if (L == TEXT("float4"))                        Custom->OutputType = CMOT_Float4;
		else if (L == TEXT("material_attributes") || L == TEXT("materialattributes"))
			Custom->OutputType = CMOT_MaterialAttributes;
	}

	bool bInputsChanged = false;

	// "inputs" — replace all inputs (breaks existing connections)
	const TArray<TSharedPtr<FJsonValue>>* InputsArr;
	if (NodeDef->TryGetArrayField(TEXT("inputs"), InputsArr))
	{
		Custom->Inputs.Empty();
		for (const TSharedPtr<FJsonValue>& V : *InputsArr)
		{
			FCustomInput NewInput;
			NewInput.InputName = FName(*V->AsString());
			Custom->Inputs.Add(NewInput);
		}
		bInputsChanged = true;
	}

	// "add_inputs" — append new inputs without disturbing existing ones or their connections
	const TArray<TSharedPtr<FJsonValue>>* AddInputsArr;
	if (NodeDef->TryGetArrayField(TEXT("add_inputs"), AddInputsArr))
	{
		for (const TSharedPtr<FJsonValue>& V : *AddInputsArr)
		{
			FName NewName(*V->AsString());
			// Skip if an input with this name already exists
			bool bExists = false;
			for (const FCustomInput& Existing : Custom->Inputs)
			{
				if (Existing.InputName == NewName) { bExists = true; break; }
			}
			if (!bExists)
			{
				FCustomInput NewInput;
				NewInput.InputName = NewName;
				Custom->Inputs.Add(NewInput);
				bInputsChanged = true;
			}
		}
	}

	bool bOutputsChanged = false;

	const TArray<TSharedPtr<FJsonValue>>* OutputsArr;
	if (NodeDef->TryGetArrayField(TEXT("outputs"), OutputsArr))
	{
		Custom->AdditionalOutputs.Empty();
		for (const TSharedPtr<FJsonValue>& V : *OutputsArr)
		{
			const TSharedPtr<FJsonObject>& OutObj = V->AsObject();
			if (!OutObj.IsValid()) continue;
			FCustomOutput NewOut;
			FString OutName;
			if (OutObj->TryGetStringField(TEXT("name"), OutName)) NewOut.OutputName = FName(*OutName);
			FString OutType;
			if (OutObj->TryGetStringField(TEXT("type"), OutType))
			{
				FString L = OutType.ToLower();
				if      (L == TEXT("float") || L == TEXT("float1")) NewOut.OutputType = CMOT_Float1;
				else if (L == TEXT("float2"))                        NewOut.OutputType = CMOT_Float2;
				else if (L == TEXT("float3"))                        NewOut.OutputType = CMOT_Float3;
				else if (L == TEXT("float4"))                        NewOut.OutputType = CMOT_Float4;
				else if (L == TEXT("material_attributes") || L == TEXT("materialattributes"))
					NewOut.OutputType = CMOT_MaterialAttributes;
			}
			Custom->AdditionalOutputs.Add(NewOut);
		}
		bOutputsChanged = true;
	}

	// Rebuild outputs and reconstruct the graph node pins after input/output changes
	if (bInputsChanged || bOutputsChanged)
	{
		Custom->RebuildOutputs();
#if WITH_EDITOR
		if (Custom->GraphNode)
		{
			Custom->GraphNode->ReconstructNode();
		}
#endif
	}
}

// ---------------------------------------------------------------------------
// SerializeMaterialExpression
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::SerializeMaterialExpression(
	UMaterialExpression* Expr, int32 Index,
	const TMap<UMaterialExpression*, int32>& ExprIndexMap,
	bool bIncludeAvailablePins)
{
	auto Node = MakeShared<FJsonObject>();
	Node->SetNumberField(TEXT("index"), Index);

	FString FullClass = Expr->GetClass()->GetName();
	FString ShortType = FullClass;
	ShortType.RemoveFromStart(TEXT("MaterialExpression"));
	Node->SetStringField(TEXT("type"),  ShortType);
	Node->SetStringField(TEXT("class"), FullClass);
	Node->SetNumberField(TEXT("pos_x"), Expr->MaterialExpressionEditorX);
	Node->SetNumberField(TEXT("pos_y"), Expr->MaterialExpressionEditorY);

	// ---- Type-specific properties ----------------------------------------
	auto Props = MakeShared<FJsonObject>();

	if (UMaterialExpressionCustom* Custom = Cast<UMaterialExpressionCustom>(Expr))
	{
		Props->SetStringField(TEXT("code"),        Custom->Code);
		Props->SetStringField(TEXT("description"), Custom->Description);

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
		Props->SetStringField(TEXT("output_type"), OutTypeStr);

		TArray<TSharedPtr<FJsonValue>> InputNames;
		for (const FCustomInput& Inp : Custom->Inputs)
			InputNames.Add(MakeShared<FJsonValueString>(Inp.InputName.ToString()));
		Props->SetArrayField(TEXT("inputs"), InputNames);

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
		Props->SetArrayField(TEXT("outputs"), OutputDefs);
	}
	else
	{
		// Generic reflection: read commonly useful properties

		// ParameterName (ScalarParameter, VectorParameter, TextureParameter, etc.)
		if (FProperty* P = Expr->GetClass()->FindPropertyByName(TEXT("ParameterName")))
			if (FNameProperty* NP = CastField<FNameProperty>(P))
				Props->SetStringField(TEXT("ParameterName"),
					NP->GetPropertyValue_InContainer(Expr).ToString());

		// Group (parameter group in material instances)
		if (FProperty* P = Expr->GetClass()->FindPropertyByName(TEXT("Group")))
			if (FNameProperty* NP = CastField<FNameProperty>(P))
				Props->SetStringField(TEXT("Group"),
					NP->GetPropertyValue_InContainer(Expr).ToString());

		// SortPriority
		if (FProperty* P = Expr->GetClass()->FindPropertyByName(TEXT("SortPriority")))
			if (FIntProperty* IP = CastField<FIntProperty>(P))
				Props->SetNumberField(TEXT("SortPriority"), IP->GetPropertyValue_InContainer(Expr));

		// DefaultValue (float → ScalarParameter, FLinearColor → VectorParameter)
		if (FProperty* P = Expr->GetClass()->FindPropertyByName(TEXT("DefaultValue")))
		{
			if (FFloatProperty* FP = CastField<FFloatProperty>(P))
				Props->SetNumberField(TEXT("DefaultValue"), FP->GetPropertyValue_InContainer(Expr));
			else if (FStructProperty* SP = CastField<FStructProperty>(P))
			{
				if (SP->Struct == TBaseStructure<FLinearColor>::Get())
				{
					const FLinearColor* C = SP->ContainerPtrToValuePtr<FLinearColor>(Expr);
					TArray<TSharedPtr<FJsonValue>> Arr;
					Arr.Add(MakeShared<FJsonValueNumber>(C->R));
					Arr.Add(MakeShared<FJsonValueNumber>(C->G));
					Arr.Add(MakeShared<FJsonValueNumber>(C->B));
					Arr.Add(MakeShared<FJsonValueNumber>(C->A));
					Props->SetArrayField(TEXT("DefaultValue"), Arr);
				}
			}
		}

		// SliderMin / SliderMax (ScalarParameter)
		if (FProperty* P = Expr->GetClass()->FindPropertyByName(TEXT("SliderMin")))
			if (FFloatProperty* FP = CastField<FFloatProperty>(P))
				Props->SetNumberField(TEXT("SliderMin"), FP->GetPropertyValue_InContainer(Expr));
		if (FProperty* P = Expr->GetClass()->FindPropertyByName(TEXT("SliderMax")))
			if (FFloatProperty* FP = CastField<FFloatProperty>(P))
				Props->SetNumberField(TEXT("SliderMax"), FP->GetPropertyValue_InContainer(Expr));

		// R / G (Constant, Constant2Vector)
		if (FProperty* P = Expr->GetClass()->FindPropertyByName(TEXT("R")))
			if (FFloatProperty* FP = CastField<FFloatProperty>(P))
				Props->SetNumberField(TEXT("R"), FP->GetPropertyValue_InContainer(Expr));
		if (FProperty* P = Expr->GetClass()->FindPropertyByName(TEXT("G")))
			if (FFloatProperty* FP = CastField<FFloatProperty>(P))
				Props->SetNumberField(TEXT("G"), FP->GetPropertyValue_InContainer(Expr));

		// Constant (Constant3Vector/4Vector → FLinearColor, Constant2Vector → FVector2D, Constant → float)
		if (FProperty* P = Expr->GetClass()->FindPropertyByName(TEXT("Constant")))
		{
			if (FFloatProperty* FP = CastField<FFloatProperty>(P))
				Props->SetNumberField(TEXT("Constant"), FP->GetPropertyValue_InContainer(Expr));
			else if (FStructProperty* SP = CastField<FStructProperty>(P))
			{
				if (SP->Struct == TBaseStructure<FLinearColor>::Get())
				{
					const FLinearColor* C = SP->ContainerPtrToValuePtr<FLinearColor>(Expr);
					TArray<TSharedPtr<FJsonValue>> Arr;
					Arr.Add(MakeShared<FJsonValueNumber>(C->R));
					Arr.Add(MakeShared<FJsonValueNumber>(C->G));
					Arr.Add(MakeShared<FJsonValueNumber>(C->B));
					Arr.Add(MakeShared<FJsonValueNumber>(C->A));
					Props->SetArrayField(TEXT("Constant"), Arr);
				}
				else if (SP->Struct == TBaseStructure<FVector2D>::Get())
				{
					const FVector2D* V = SP->ContainerPtrToValuePtr<FVector2D>(Expr);
					TArray<TSharedPtr<FJsonValue>> Arr;
					Arr.Add(MakeShared<FJsonValueNumber>(V->X));
					Arr.Add(MakeShared<FJsonValueNumber>(V->Y));
					Props->SetArrayField(TEXT("Constant"), Arr);
				}
			}
		}

		// CoordinateIndex (TextureCoordinate)
		if (FProperty* P = Expr->GetClass()->FindPropertyByName(TEXT("CoordinateIndex")))
			if (FIntProperty* IP = CastField<FIntProperty>(P))
				Props->SetNumberField(TEXT("CoordinateIndex"), IP->GetPropertyValue_InContainer(Expr));

		// ConstA / ConstB / ConstAlpha (Multiply, Add, Lerp fallback constants)
		for (const TCHAR* FallbackName : { TEXT("ConstA"), TEXT("ConstB"), TEXT("ConstAlpha") })
		{
			if (FProperty* P = Expr->GetClass()->FindPropertyByName(FallbackName))
				if (FFloatProperty* FP = CastField<FFloatProperty>(P))
					Props->SetNumberField(FallbackName, FP->GetPropertyValue_InContainer(Expr));
		}
	}

	Node->SetObjectField(TEXT("properties"), Props);

	// ---- Input connections (which node/output-index feeds each input pin) ----
	// Uses GetInput(i)/GetInputName(i) — the modern UE5 API, not reflection iteration.
	{
		auto Connections = MakeShared<FJsonObject>();

		if (UMaterialExpressionCustom* Custom = Cast<UMaterialExpressionCustom>(Expr))
		{
			// Custom nodes store connections in Custom->Inputs[i].Input (FExpressionInput)
			for (const FCustomInput& CInp : Custom->Inputs)
			{
				if (!CInp.Input.Expression) continue;
				const int32* SrcIdx = ExprIndexMap.Find(CInp.Input.Expression);
				auto ConnObj = MakeShared<FJsonObject>();
				ConnObj->SetNumberField(TEXT("from_node"), SrcIdx ? *SrcIdx : -1);
				ConnObj->SetNumberField(TEXT("from_output_index"), CInp.Input.OutputIndex);
				// Resolve output pin name from connected node's Outputs array
				TArray<FExpressionOutput>& SrcOuts = CInp.Input.Expression->GetOutputs();
				FString FromPin;
				if (SrcOuts.IsValidIndex(CInp.Input.OutputIndex) &&
				    !SrcOuts[CInp.Input.OutputIndex].OutputName.IsNone())
					FromPin = SrcOuts[CInp.Input.OutputIndex].OutputName.ToString();
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
				if (!Input) break;
				if (!Input->Expression) continue;

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
					FromPin = SrcOuts[Input->OutputIndex].OutputName.ToString();
				ConnObj->SetStringField(TEXT("from_pin"), FromPin);
				FString Key = InputName.IsNone()
					? FString::Printf(TEXT("Input_%d"), i)
					: InputName.ToString();
				Connections->SetObjectField(Key, ConnObj);
			}
		}

		Node->SetObjectField(TEXT("input_connections"), Connections);
	}

	// ---- Available pins (for get_material_expression_info) ----
	if (bIncludeAvailablePins)
	{
		// Available input pin names via UMaterialEditingLibrary
		TArray<FString> InNames = UMaterialEditingLibrary::GetMaterialExpressionInputNames(Expr);
		TArray<TSharedPtr<FJsonValue>> AvailIn;
		for (const FString& N : InNames)
			AvailIn.Add(MakeShared<FJsonValueString>(N));
		Node->SetArrayField(TEXT("available_inputs"), AvailIn);

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
		Node->SetArrayField(TEXT("available_outputs"), AvailOut);
	}

	return Node;
}

// ---------------------------------------------------------------------------
// HandleCreateMaterial
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleCreateMaterial(
	const TSharedPtr<FJsonObject>& Params)
{
	FString Name;
	if (!Params->TryGetStringField(TEXT("name"), Name))
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));

	FString Path = TEXT("/Game/Materials");
	Params->TryGetStringField(TEXT("path"), Path);
	FString BlendModeStr = TEXT("opaque");
	Params->TryGetStringField(TEXT("blend_mode"), BlendModeStr);
	FString ShadingStr = TEXT("default_lit");
	Params->TryGetStringField(TEXT("shading_model"), ShadingStr);
	bool bTwoSided = false;
	Params->TryGetBoolField(TEXT("two_sided"), bTwoSided);

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UObject* NewAsset = AssetTools.CreateAsset(Name, Path, UMaterial::StaticClass(),
	                                           NewObject<UMaterialFactoryNew>());
	UMaterial* Mat = Cast<UMaterial>(NewAsset);
	if (!Mat)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create material (may already exist)"));

	Mat->BlendMode = ResolveBlendMode(BlendModeStr);
	Mat->SetShadingModel(ResolveShadingModel(ShadingStr));
	Mat->TwoSided = bTwoSided;

	double OpacityClip;
	if (Params->TryGetNumberField(TEXT("opacity_mask_clip_value"), OpacityClip))
		Mat->OpacityMaskClipValue = (float)OpacityClip;

	FString FullPath = Path / Name;
	UEditorAssetLibrary::SaveAsset(FullPath);

	auto R = MakeShared<FJsonObject>();
	R->SetBoolField(TEXT("success"), true);
	R->SetStringField(TEXT("path"), FullPath);
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
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'parent_path' parameter"));
	FString Name;
	if (!Params->TryGetStringField(TEXT("name"), Name))
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
	FString Path = TEXT("/Game/Materials");
	Params->TryGetStringField(TEXT("path"), Path);

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UObject* NewAsset = AssetTools.CreateAsset(Name, Path, UMaterialInstanceConstant::StaticClass(),
	                                           NewObject<UMaterialInstanceConstantFactoryNew>());
	UMaterialInstanceConstant* MI = Cast<UMaterialInstanceConstant>(NewAsset);
	if (!MI)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create material instance"));

	UMaterialInterface* Parent = Cast<UMaterialInterface>(UEditorAssetLibrary::LoadAsset(ParentPath));
	if (Parent)
		UMaterialEditingLibrary::SetMaterialInstanceParent(MI, Parent);
	else
		UE_LOG(LogTemp, Warning, TEXT("MCPMaterial: Parent not found: %s"), *ParentPath);

	const TSharedPtr<FJsonObject>* ScalarObj;
	if (Params->TryGetObjectField(TEXT("scalar_params"), ScalarObj))
		for (const auto& Pair : (*ScalarObj)->Values)
			UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(MI, FName(*Pair.Key), (float)Pair.Value->AsNumber());

	const TSharedPtr<FJsonObject>* VectorObj;
	if (Params->TryGetObjectField(TEXT("vector_params"), VectorObj))
		for (const auto& Pair : (*VectorObj)->Values)
		{
			const TArray<TSharedPtr<FJsonValue>>& A = Pair.Value->AsArray();
			FLinearColor C(A.Num() > 0 ? (float)A[0]->AsNumber() : 0,
			               A.Num() > 1 ? (float)A[1]->AsNumber() : 0,
			               A.Num() > 2 ? (float)A[2]->AsNumber() : 0,
			               A.Num() > 3 ? (float)A[3]->AsNumber() : 1);
			UMaterialEditingLibrary::SetMaterialInstanceVectorParameterValue(MI, FName(*Pair.Key), C);
		}

	const TSharedPtr<FJsonObject>* TextureObj;
	if (Params->TryGetObjectField(TEXT("texture_params"), TextureObj))
		for (const auto& Pair : (*TextureObj)->Values)
		{
			UTexture* Tex = Cast<UTexture>(UEditorAssetLibrary::LoadAsset(Pair.Value->AsString()));
			if (Tex)
				UMaterialEditingLibrary::SetMaterialInstanceTextureParameterValue(MI, FName(*Pair.Key), Tex);
		}

	UMaterialEditingLibrary::UpdateMaterialInstance(MI);
	FString FullPath = Path / Name;
	UEditorAssetLibrary::SaveAsset(FullPath);

	auto R = MakeShared<FJsonObject>();
	R->SetBoolField(TEXT("success"), true);
	R->SetStringField(TEXT("path"), FullPath);
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
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path'"));

	const TArray<TSharedPtr<FJsonValue>>* NodesArray;
	if (!Params->TryGetArrayField(TEXT("nodes"), NodesArray))
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'nodes' array"));

	const TArray<TSharedPtr<FJsonValue>>* ConnectionsArray;
	if (!Params->TryGetArrayField(TEXT("connections"), ConnectionsArray))
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'connections' array"));

	bool bClearExisting = true;
	Params->TryGetBoolField(TEXT("clear_existing"), bClearExisting);

	UMaterial* OriginalMaterial = Cast<UMaterial>(UEditorAssetLibrary::LoadAsset(MaterialPath));
	UMaterial* Material = OriginalMaterial ? ResolveWorkingMaterial(OriginalMaterial) : nullptr;
	if (!Material)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Material not found: %s"), *MaterialPath));

	TArray<FString> Errors;

	if (bClearExisting)
	{
		UMaterialEditingLibrary::DeleteAllMaterialExpressions(Material);
		UMaterialEditingLibrary::DeleteAllMaterialExpressions(Material); // double-pass
	}

	TArray<UMaterialExpression*> CreatedNodes;
	CreatedNodes.SetNum(NodesArray->Num());

	for (int32 Idx = 0; Idx < NodesArray->Num(); ++Idx)
	{
		const TSharedPtr<FJsonObject>& NodeDef = (*NodesArray)[Idx]->AsObject();
		if (!NodeDef.IsValid()) { Errors.Add(FString::Printf(TEXT("Node %d: invalid JSON"), Idx)); CreatedNodes[Idx] = nullptr; continue; }

		FString TypeName = TEXT("Constant");
		NodeDef->TryGetStringField(TEXT("type"), TypeName);
		int32 PosX = -300, PosY = 0;
		NodeDef->TryGetNumberField(TEXT("pos_x"), PosX);
		NodeDef->TryGetNumberField(TEXT("pos_y"), PosY);

		UClass* ExprClass = FindExpressionClass(TypeName);
		if (!ExprClass)
		{
			Errors.Add(FString::Printf(TEXT("Node %d: unknown type '%s'"), Idx, *TypeName));
			CreatedNodes[Idx] = nullptr; continue;
		}

		UMaterialExpression* Expr = UMaterialEditingLibrary::CreateMaterialExpression(Material, ExprClass, PosX, PosY);
		if (!Expr)
		{
			Errors.Add(FString::Printf(TEXT("Node %d: failed to create '%s'"), Idx, *TypeName));
			CreatedNodes[Idx] = nullptr; continue;
		}
		CreatedNodes[Idx] = Expr;

		const bool bIsCustom = (TypeName == TEXT("Custom") || TypeName == TEXT("MaterialExpressionCustom"));
		if (bIsCustom) HandleCustomHLSLNode(Expr, NodeDef);

		const TSharedPtr<FJsonObject>* PropsObj;
		if (NodeDef->TryGetObjectField(TEXT("properties"), PropsObj))
			for (const auto& Pair : (*PropsObj)->Values)
			{
				FString PropErr;
				if (!SetExpressionProperty(Expr, Pair.Key, Pair.Value, PropErr))
					Errors.Add(FString::Printf(TEXT("Node %d prop '%s': %s"), Idx, *Pair.Key, *PropErr));
			}
	}

	int32 ConnectionsMade = 0;
	for (const TSharedPtr<FJsonValue>& ConnVal : *ConnectionsArray)
	{
		const TSharedPtr<FJsonObject>& Conn = ConnVal->AsObject();
		if (!Conn.IsValid()) continue;

		int32 FromIdx = 0;
		{
			TSharedPtr<FJsonValue> FV = Conn->TryGetField(TEXT("from_node"));
			if (FV.IsValid()) TryParseIntFromJson(FV, FromIdx);
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
				if (S.Equals(TEXT("material"), ESearchCase::IgnoreCase)) bToMaterial = true;
				else if (S.IsNumeric()) ToIdx = FCString::Atoi(*S);
			}
			else TryParseIntFromJson(ToNodeVal, ToIdx);
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
			if (MatProp == MP_MAX) { Errors.Add(FString::Printf(TEXT("Connection: unknown material property '%s'"), *ToPin)); continue; }
			bConnected = UMaterialEditingLibrary::ConnectMaterialProperty(CreatedNodes[FromIdx], FromPin, MatProp);
		}
		else
		{
			if (ToIdx < 0 || ToIdx >= CreatedNodes.Num() || !CreatedNodes[ToIdx])
			{ Errors.Add(FString::Printf(TEXT("Connection: invalid to_node %d"), ToIdx)); continue; }
			bConnected = UMaterialEditingLibrary::ConnectMaterialExpressions(
				CreatedNodes[FromIdx], FromPin, CreatedNodes[ToIdx], ToPin);
		}

		if (bConnected) ConnectionsMade++;
		else
		{
			FString ToDesc = bToMaterial
				? FString::Printf(TEXT("material.%s"), *ToPin)
				: FString::Printf(TEXT("node[%d].%s"), ToIdx, *ToPin);
			Errors.Add(FString::Printf(TEXT("Connection failed: node[%d].%s → %s"), FromIdx, *FromPin, *ToDesc));
		}
	}

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
	for (const FString& E : Errors) ErrArr.Add(MakeShared<FJsonValueString>(E));
	R->SetArrayField(TEXT("errors"), ErrArr);
	return R;
}

// ---------------------------------------------------------------------------
// HandleGetMaterialInfo
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleGetMaterialInfo(
	const TSharedPtr<FJsonObject>& Params)
{
	FString MaterialPath;
	if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path'"));

	UMaterial* OriginalMaterial = Cast<UMaterial>(UEditorAssetLibrary::LoadAsset(MaterialPath));
	UMaterial* Material = OriginalMaterial ? ResolveWorkingMaterial(OriginalMaterial) : nullptr;
	if (!Material)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Material not found: %s"), *MaterialPath));

	auto Info = MakeShared<FJsonObject>();
	Info->SetStringField(TEXT("path"), MaterialPath);

	// Blend mode
	static const TCHAR* BlendNames[] = { TEXT("opaque"), TEXT("masked"), TEXT("translucent"), TEXT("additive"), TEXT("modulate"), TEXT("alpha_composite"), TEXT("alpha_holdout") };
	int32 BM = (int32)Material->BlendMode;
	Info->SetStringField(TEXT("blend_mode"), (BM >= 0 && BM < 7) ? BlendNames[BM] : TEXT("unknown"));

	// Shading model
	FMaterialShadingModelField SMF = Material->GetShadingModels();
	FString SMStr = TEXT("default_lit");
	if (SMF.HasShadingModel(MSM_Unlit))              SMStr = TEXT("unlit");
	else if (SMF.HasShadingModel(MSM_Subsurface))    SMStr = TEXT("subsurface");
	else if (SMF.HasShadingModel(MSM_ClearCoat))     SMStr = TEXT("clear_coat");
	Info->SetStringField(TEXT("shading_model"), SMStr);
	Info->SetBoolField(TEXT("two_sided"), Material->TwoSided);
	Info->SetNumberField(TEXT("num_expressions"), UMaterialEditingLibrary::GetNumMaterialExpressions(Material));

	// Used textures
	TArray<UTexture*> Textures;
	Material->GetUsedTextures(Textures, EMaterialQualityLevel::High);
	TArray<TSharedPtr<FJsonValue>> TexArr;
	for (UTexture* T : Textures) if (T) TexArr.Add(MakeShared<FJsonValueString>(T->GetPathName()));
	Info->SetArrayField(TEXT("used_textures"), TexArr);

	// Parameters — NOTE: avoid variable name 'PI' which is a UE math macro (#define PI 3.14159...)
	TArray<FMaterialParameterInfo> ParamInfos;
	TArray<FGuid> ParamIds;
	Material->GetAllScalarParameterInfo(ParamInfos, ParamIds);
	TArray<TSharedPtr<FJsonValue>> ScalarNames;
	for (const FMaterialParameterInfo& PInfo : ParamInfos)
		ScalarNames.Add(MakeShared<FJsonValueString>(PInfo.Name.ToString()));
	Info->SetArrayField(TEXT("scalar_parameters"), ScalarNames);

	Material->GetAllVectorParameterInfo(ParamInfos, ParamIds);
	TArray<TSharedPtr<FJsonValue>> VecNames;
	for (const FMaterialParameterInfo& PInfo : ParamInfos)
		VecNames.Add(MakeShared<FJsonValueString>(PInfo.Name.ToString()));
	Info->SetArrayField(TEXT("vector_parameters"), VecNames);

	Material->GetAllTextureParameterInfo(ParamInfos, ParamIds);
	TArray<TSharedPtr<FJsonValue>> TexParamNames;
	for (const FMaterialParameterInfo& PInfo : ParamInfos)
		TexParamNames.Add(MakeShared<FJsonValueString>(PInfo.Name.ToString()));
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
// HandleRecompileMaterial
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleRecompileMaterial(
	const TSharedPtr<FJsonObject>& Params)
{
	FString MaterialPath;
	if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path'"));

	UMaterial* OriginalMaterial = Cast<UMaterial>(UEditorAssetLibrary::LoadAsset(MaterialPath));
	UMaterial* Material = OriginalMaterial ? ResolveWorkingMaterial(OriginalMaterial) : nullptr;
	if (!Material)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Material not found: %s"), *MaterialPath));

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

// ---------------------------------------------------------------------------
// HandleGetMaterialErrors
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleGetMaterialErrors(
	const TSharedPtr<FJsonObject>& Params)
{
	FString MaterialPath;
	if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path'"));

	UMaterial* OriginalMaterial = Cast<UMaterial>(UEditorAssetLibrary::LoadAsset(MaterialPath));
	UMaterial* Material = OriginalMaterial ? ResolveWorkingMaterial(OriginalMaterial) : nullptr;
	if (!Material)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Material not found: %s"), *MaterialPath));

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
			continue;

		const TArray<FString>& CompileErrors = MatResource->GetCompileErrors();
		for (const FString& Error : CompileErrors)
		{
			if (SeenErrors.Contains(Error))
				continue;
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
			// Attach error node info to the last error entry for this quality level
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
				// Attach to last error in this batch
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
// HandleSetMaterialProperties
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleSetMaterialProperties(
	const TSharedPtr<FJsonObject>& Params)
{
	FString MaterialPath;
	if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path'"));

	UMaterial* OriginalMaterial = Cast<UMaterial>(UEditorAssetLibrary::LoadAsset(MaterialPath));
	UMaterial* Material = OriginalMaterial ? ResolveWorkingMaterial(OriginalMaterial) : nullptr;
	if (!Material)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Material not found: %s"), *MaterialPath));

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
		Changed.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("two_sided=%s"), bBool ? TEXT("true") : TEXT("false"))));
	}
	double Num;
	if (Params->TryGetNumberField(TEXT("opacity_mask_clip_value"), Num))
	{
		Material->OpacityMaskClipValue = (float)Num;
		Changed.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("opacity_mask_clip_value=%f"), Num)));
	}
	if (Params->TryGetBoolField(TEXT("dithered_lof_transition"), bBool))
	{
		Material->DitheredLODTransition = bBool;
		Changed.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("dithered_lod_transition=%s"), bBool ? TEXT("true") : TEXT("false"))));
	}
	if (Params->TryGetBoolField(TEXT("allow_negative_emissive_color"), bBool))
	{
		Material->bAllowNegativeEmissiveColor = bBool;
		Changed.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("allow_negative_emissive_color=%s"), bBool ? TEXT("true") : TEXT("false"))));
	}

	bool bRecompile = true;
	Params->TryGetBoolField(TEXT("recompile"), bRecompile);
	if (Material == OriginalMaterial)
	{
		if (bRecompile) UMaterialEditingLibrary::RecompileMaterial(Material);
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
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path'"));

	const TArray<TSharedPtr<FJsonValue>>* CommentsArray;
	if (!Params->TryGetArrayField(TEXT("comments"), CommentsArray))
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'comments' array"));

	UMaterial* OriginalMaterial = Cast<UMaterial>(UEditorAssetLibrary::LoadAsset(MaterialPath));
	UMaterial* Material = OriginalMaterial ? ResolveWorkingMaterial(OriginalMaterial) : nullptr;
	if (!Material)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Material not found: %s"), *MaterialPath));

	FMaterialExpressionCollection& Collection = Material->GetExpressionCollection();
	int32 CommentsCreated = 0;
	TArray<FString> Errors;

	for (int32 Idx = 0; Idx < CommentsArray->Num(); ++Idx)
	{
		const TSharedPtr<FJsonObject>& Def = (*CommentsArray)[Idx]->AsObject();
		if (!Def.IsValid()) { Errors.Add(FString::Printf(TEXT("Comment %d: invalid JSON"), Idx)); continue; }

		FString Text;
		if (!Def->TryGetStringField(TEXT("text"), Text))
		{ Errors.Add(FString::Printf(TEXT("Comment %d: missing 'text'"), Idx)); continue; }

		UMaterialExpressionComment* Comment = NewObject<UMaterialExpressionComment>(Material);
		if (!Comment) { Errors.Add(FString::Printf(TEXT("Comment %d: failed to create"), Idx)); continue; }

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
			Comment->CommentColor = FLinearColor(
				(float)(*ColorArr)[0]->AsNumber(),
				(float)(*ColorArr)[1]->AsNumber(),
				(float)(*ColorArr)[2]->AsNumber(),
				ColorArr->Num() > 3 ? (float)(*ColorArr)[3]->AsNumber() : 1.f);

		bool bBool;
		if (Def->TryGetBoolField(TEXT("show_bubble"), bBool)) Comment->bCommentBubbleVisible_InDetailsPanel = bBool;
		if (Def->TryGetBoolField(TEXT("color_bubble"), bBool)) Comment->bColorCommentBubble = bBool;
		bool bGroup = true;
		if (Def->TryGetBoolField(TEXT("group_mode"), bGroup)) Comment->bGroupMode = bGroup;

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
	for (const FString& E : Errors) ErrArr.Add(MakeShared<FJsonValueString>(E));
	R->SetArrayField(TEXT("errors"), ErrArr);
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
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path'"));

	UMaterial* OriginalMaterial = Cast<UMaterial>(UEditorAssetLibrary::LoadAsset(MaterialPath));
	UMaterial* Material = OriginalMaterial ? ResolveWorkingMaterial(OriginalMaterial) : nullptr;
	if (!Material)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Material not found: %s"), *MaterialPath));

	// Optional: include available_inputs / available_outputs per node (larger but more useful)
	bool bIncludePins = false;
	Params->TryGetBoolField(TEXT("include_pins"), bIncludePins);

	FMaterialExpressionCollection& Collection = Material->GetExpressionCollection();

	TMap<UMaterialExpression*, int32> ExprIndexMap;
	ExprIndexMap.Reserve(Collection.Expressions.Num());
	for (int32 i = 0; i < Collection.Expressions.Num(); ++i)
		if (UMaterialExpression* E = Collection.Expressions[i])
			ExprIndexMap.Add(E, i);

	TArray<TSharedPtr<FJsonValue>> NodesArray;
	for (int32 i = 0; i < Collection.Expressions.Num(); ++i)
	{
		UMaterialExpression* Expr = Collection.Expressions[i];
		if (!Expr) continue;
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
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path'"));

	int32 NodeIndex = -1;
	{
		TSharedPtr<FJsonValue> V = Params->TryGetField(TEXT("node_index"));
		if (!V.IsValid() || !TryParseIntFromJson(V, NodeIndex))
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'node_index'"));
	}

	UMaterial* OriginalMaterial = Cast<UMaterial>(UEditorAssetLibrary::LoadAsset(MaterialPath));
	UMaterial* Material = OriginalMaterial ? ResolveWorkingMaterial(OriginalMaterial) : nullptr;
	if (!Material)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Material not found: %s"), *MaterialPath));

	FMaterialExpressionCollection& Collection = Material->GetExpressionCollection();
	if (NodeIndex < 0 || NodeIndex >= Collection.Expressions.Num() || !Collection.Expressions[NodeIndex])
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Invalid node_index %d (material has %d nodes)"),
				NodeIndex, Collection.Expressions.Num()));

	TMap<UMaterialExpression*, int32> ExprIndexMap;
	for (int32 i = 0; i < Collection.Expressions.Num(); ++i)
		if (UMaterialExpression* E = Collection.Expressions[i])
			ExprIndexMap.Add(E, i);

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
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path'"));

	UMaterial* OriginalMaterial = Cast<UMaterial>(UEditorAssetLibrary::LoadAsset(MaterialPath));
	UMaterial* Material = OriginalMaterial ? ResolveWorkingMaterial(OriginalMaterial) : nullptr;
	if (!Material)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Material not found: %s"), *MaterialPath));

	FMaterialExpressionCollection& Collection = Material->GetExpressionCollection();
	TMap<UMaterialExpression*, int32> ExprIndexMap;
	for (int32 i = 0; i < Collection.Expressions.Num(); ++i)
		if (UMaterialExpression* E = Collection.Expressions[i])
			ExprIndexMap.Add(E, i);

	static const EMaterialProperty Props[] = {
		MP_BaseColor, MP_Metallic, MP_Roughness, MP_EmissiveColor, MP_Opacity, MP_OpacityMask,
		MP_Normal, MP_Specular, MP_AmbientOcclusion, MP_WorldPositionOffset, MP_SubsurfaceColor, MP_Refraction
	};

	auto ConnMap = MakeShared<FJsonObject>();
	for (EMaterialProperty Prop : Props)
	{
		UMaterialExpression* InputNode = UMaterialEditingLibrary::GetMaterialPropertyInputNode(Material, Prop);
		if (!InputNode) continue;

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
// HandleAddMaterialExpression
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleAddMaterialExpression(
	const TSharedPtr<FJsonObject>& Params)
{
	FString MaterialPath;
	if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path'"));

	const TSharedPtr<FJsonObject>* NodeDefPtr;
	if (!Params->TryGetObjectField(TEXT("node"), NodeDefPtr))
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'node' object"));
	const TSharedPtr<FJsonObject>& NodeDef = *NodeDefPtr;

	UMaterial* OriginalMaterial = Cast<UMaterial>(UEditorAssetLibrary::LoadAsset(MaterialPath));
	UMaterial* Material = OriginalMaterial ? ResolveWorkingMaterial(OriginalMaterial) : nullptr;
	if (!Material)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Material not found: %s"), *MaterialPath));

	FString TypeName = TEXT("Constant");
	NodeDef->TryGetStringField(TEXT("type"), TypeName);
	int32 PosX = -300, PosY = 0;
	NodeDef->TryGetNumberField(TEXT("pos_x"), PosX);
	NodeDef->TryGetNumberField(TEXT("pos_y"), PosY);

	UClass* ExprClass = FindExpressionClass(TypeName);
	if (!ExprClass)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Unknown expression type: '%s'"), *TypeName));

	FMaterialExpressionCollection& Collection = Material->GetExpressionCollection();
	const int32 CountBefore = Collection.Expressions.Num();

	UMaterialExpression* NewExpr = UMaterialEditingLibrary::CreateMaterialExpression(Material, ExprClass, PosX, PosY);
	if (!NewExpr)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Failed to create expression '%s'"), *TypeName));

	const bool bIsCustom = (TypeName == TEXT("Custom") || TypeName == TEXT("MaterialExpressionCustom"));
	if (bIsCustom) HandleCustomHLSLNode(NewExpr, NodeDef);

	TArray<FString> PropErrors;
	const TSharedPtr<FJsonObject>* PropsObj;
	if (NodeDef->TryGetObjectField(TEXT("properties"), PropsObj))
		for (const auto& Pair : (*PropsObj)->Values)
		{
			FString PropErr;
			if (!SetExpressionProperty(NewExpr, Pair.Key, Pair.Value, PropErr))
				PropErrors.Add(FString::Printf(TEXT("'%s': %s"), *Pair.Key, *PropErr));
		}

	// Find new node's index — UE appends to end but search to be safe
	int32 NewIndex = -1;
	for (int32 i = Collection.Expressions.Num() - 1; i >= CountBefore; --i)
		if (Collection.Expressions[i] == NewExpr) { NewIndex = i; break; }
	if (NewIndex == -1)
		for (int32 i = 0; i < Collection.Expressions.Num(); ++i)
			if (Collection.Expressions[i] == NewExpr) { NewIndex = i; break; }

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
	for (const FString& E : PropErrors) ErrArr.Add(MakeShared<FJsonValueString>(E));
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
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path'"));

	int32 NodeIndex = -1;
	{
		TSharedPtr<FJsonValue> V = Params->TryGetField(TEXT("node_index"));
		if (!V.IsValid() || !TryParseIntFromJson(V, NodeIndex))
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'node_index'"));
	}

	UMaterial* OriginalMaterial = Cast<UMaterial>(UEditorAssetLibrary::LoadAsset(MaterialPath));
	UMaterial* Material = OriginalMaterial ? ResolveWorkingMaterial(OriginalMaterial) : nullptr;
	if (!Material)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Material not found: %s"), *MaterialPath));

	FMaterialExpressionCollection& Collection = Material->GetExpressionCollection();
	if (NodeIndex < 0 || NodeIndex >= Collection.Expressions.Num() || !Collection.Expressions[NodeIndex])
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Invalid node_index %d"), NodeIndex));

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
				// Build a node-def object with the field under its proper name
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
			HandleCustomHLSLNode(Expr, *NodeDefPtr);
		else
			HandleCustomHLSLNode(Expr, Params);
	}

	TArray<FString> PropErrors;

	// Single-property mode: property_name + property_value (used by the Python MCP tool)
	FString SinglePropName;
	TSharedPtr<FJsonValue> SinglePropValue = Params->TryGetField(TEXT("property_value"));
	if (Params->TryGetStringField(TEXT("property_name"), SinglePropName) && SinglePropValue.IsValid())
	{
		FString PropErr;
		if (!SetExpressionProperty(Expr, SinglePropName, SinglePropValue, PropErr))
			PropErrors.Add(FString::Printf(TEXT("'%s': %s"), *SinglePropName, *PropErr));
	}

	// Batch-property mode: properties object { "PropName": value, ... }
	const TSharedPtr<FJsonObject>* PropsObj;
	if (Params->TryGetObjectField(TEXT("properties"), PropsObj))
		for (const auto& Pair : (*PropsObj)->Values)
		{
			FString PropErr;
			if (!SetExpressionProperty(Expr, Pair.Key, Pair.Value, PropErr))
				PropErrors.Add(FString::Printf(TEXT("'%s': %s"), *Pair.Key, *PropErr));
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
	for (const FString& E : PropErrors) ErrArr.Add(MakeShared<FJsonValueString>(E));
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
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path'"));

	int32 NodeIndex = -1;
	{
		TSharedPtr<FJsonValue> V = Params->TryGetField(TEXT("node_index"));
		if (!V.IsValid() || !TryParseIntFromJson(V, NodeIndex))
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'node_index'"));
	}

	int32 PosX = 0, PosY = 0;
	bool bHasX = Params->TryGetNumberField(TEXT("pos_x"), PosX);
	bool bHasY = Params->TryGetNumberField(TEXT("pos_y"), PosY);
	if (!bHasX && !bHasY)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Provide at least pos_x or pos_y"));

	UMaterial* OriginalMaterial = Cast<UMaterial>(UEditorAssetLibrary::LoadAsset(MaterialPath));
	UMaterial* Material = OriginalMaterial ? ResolveWorkingMaterial(OriginalMaterial) : nullptr;
	if (!Material)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Material not found: %s"), *MaterialPath));

	FMaterialExpressionCollection& Collection = Material->GetExpressionCollection();
	if (NodeIndex < 0 || NodeIndex >= Collection.Expressions.Num() || !Collection.Expressions[NodeIndex])
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Invalid node_index %d"), NodeIndex));

	UMaterialExpression* Expr = Collection.Expressions[NodeIndex];
	if (bHasX) Expr->MaterialExpressionEditorX = PosX;
	if (bHasY) Expr->MaterialExpressionEditorY = PosY;
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
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path'"));

	int32 NodeIndex = -1;
	{
		TSharedPtr<FJsonValue> V = Params->TryGetField(TEXT("node_index"));
		if (!V.IsValid() || !TryParseIntFromJson(V, NodeIndex))
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'node_index'"));
	}

	int32 OffsetX = 150, OffsetY = 0;
	Params->TryGetNumberField(TEXT("offset_x"), OffsetX);
	Params->TryGetNumberField(TEXT("offset_y"), OffsetY);

	UMaterial* OriginalMaterial = Cast<UMaterial>(UEditorAssetLibrary::LoadAsset(MaterialPath));
	UMaterial* Material = OriginalMaterial ? ResolveWorkingMaterial(OriginalMaterial) : nullptr;
	if (!Material)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Material not found: %s"), *MaterialPath));

	FMaterialExpressionCollection& Collection = Material->GetExpressionCollection();
	if (NodeIndex < 0 || NodeIndex >= Collection.Expressions.Num() || !Collection.Expressions[NodeIndex])
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Invalid node_index %d"), NodeIndex));

	UMaterialExpression* Source = Collection.Expressions[NodeIndex];
	const int32 CountBefore = Collection.Expressions.Num();

	// DuplicateMaterialExpression copies all properties and adds to the material
	UMaterialExpression* NewExpr = UMaterialEditingLibrary::DuplicateMaterialExpression(
		Material, nullptr, Source);
	if (!NewExpr)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to duplicate expression"));

	NewExpr->MaterialExpressionEditorX = Source->MaterialExpressionEditorX + OffsetX;
	NewExpr->MaterialExpressionEditorY = Source->MaterialExpressionEditorY + OffsetY;

	int32 NewIndex = -1;
	for (int32 i = Collection.Expressions.Num() - 1; i >= CountBefore; --i)
		if (Collection.Expressions[i] == NewExpr) { NewIndex = i; break; }
	if (NewIndex == -1)
		for (int32 i = 0; i < Collection.Expressions.Num(); ++i)
			if (Collection.Expressions[i] == NewExpr) { NewIndex = i; break; }

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
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path'"));

	int32 NodeIndex = -1;
	{
		TSharedPtr<FJsonValue> V = Params->TryGetField(TEXT("node_index"));
		if (!V.IsValid() || !TryParseIntFromJson(V, NodeIndex))
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'node_index'"));
	}

	UMaterial* OriginalMaterial = Cast<UMaterial>(UEditorAssetLibrary::LoadAsset(MaterialPath));
	UMaterial* Material = OriginalMaterial ? ResolveWorkingMaterial(OriginalMaterial) : nullptr;
	if (!Material)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Material not found: %s"), *MaterialPath));

	FMaterialExpressionCollection& Collection = Material->GetExpressionCollection();
	if (NodeIndex < 0 || NodeIndex >= Collection.Expressions.Num() || !Collection.Expressions[NodeIndex])
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Invalid node_index %d (material has %d nodes)"),
				NodeIndex, Collection.Expressions.Num()));

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
	R->SetStringField(TEXT("note"), TEXT("Indices of remaining nodes may have shifted — re-query with get_material_graph_nodes"));
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
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path'"));

	// from_node: int (also accept numeric strings for robustness)
	int32 FromIdx = 0;
	{
		TSharedPtr<FJsonValue> FV = Params->TryGetField(TEXT("from_node"));
		if (!FV.IsValid() || !TryParseIntFromJson(FV, FromIdx))
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'from_node'"));
	}

	FString FromPin;
	Params->TryGetStringField(TEXT("from_pin"), FromPin);

	FString ToPin;
	if (!Params->TryGetStringField(TEXT("to_pin"), ToPin))
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'to_pin'"));

	// to_node: str "material" → material output; numeric string or int → node index
	bool bToMaterial = false;
	int32 ToIdx = 0;
	{
		TSharedPtr<FJsonValue> TV = Params->TryGetField(TEXT("to_node"));
		if (!TV.IsValid())
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'to_node'"));

		if (TV->Type == EJson::String)
		{
			FString S = TV->AsString();
			if (S.Equals(TEXT("material"), ESearchCase::IgnoreCase))
				bToMaterial = true;
			else if (S.IsNumeric())
				ToIdx = FCString::Atoi(*S);
			else
				return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
					FString::Printf(TEXT("Invalid to_node '%s': use a node index or \"material\""), *S));
		}
		else if (!TryParseIntFromJson(TV, ToIdx))
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("to_node must be a node index or \"material\""));
		}
	}

	UMaterial* OriginalMaterial = Cast<UMaterial>(UEditorAssetLibrary::LoadAsset(MaterialPath));
	UMaterial* Material = OriginalMaterial ? ResolveWorkingMaterial(OriginalMaterial) : nullptr;
	if (!Material)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Material not found: %s"), *MaterialPath));

	FMaterialExpressionCollection& Collection = Material->GetExpressionCollection();

	if (FromIdx < 0 || FromIdx >= Collection.Expressions.Num() || !Collection.Expressions[FromIdx])
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Invalid from_node %d (material has %d nodes)"),
				FromIdx, Collection.Expressions.Num()));
	UMaterialExpression* FromExpr = Collection.Expressions[FromIdx];

	bool bConnected = false;
	if (bToMaterial)
	{
		EMaterialProperty MatProp = ResolveMaterialProperty(ToPin);
		if (MatProp == MP_MAX)
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Unknown material property '%s'"), *ToPin));
		bConnected = UMaterialEditingLibrary::ConnectMaterialProperty(FromExpr, FromPin, MatProp);
	}
	else
	{
		if (ToIdx < 0 || ToIdx >= Collection.Expressions.Num() || !Collection.Expressions[ToIdx])
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Invalid to_node %d (material has %d nodes)"),
					ToIdx, Collection.Expressions.Num()));
		bConnected = UMaterialEditingLibrary::ConnectMaterialExpressions(
			FromExpr, FromPin, Collection.Expressions[ToIdx], ToPin);
	}

	if (!bConnected)
	{
		FString ToDesc = bToMaterial
			? FString::Printf(TEXT("material.%s"), *ToPin)
			: FString::Printf(TEXT("node[%d].%s"), ToIdx, *ToPin);
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Connection failed: node[%d].%s → %s — check pin names with get_material_expression_info"),
				FromIdx, *FromPin, *ToDesc));
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
	FString ToDesc = bToMaterial
		? FString::Printf(TEXT("material.%s"), *ToPin)
		: FString::Printf(TEXT("node[%d].%s"), ToIdx, *ToPin);
	R->SetStringField(TEXT("connection"), FString::Printf(TEXT("node[%d].%s → %s"), FromIdx, *FromPin, *ToDesc));
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
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path'"));

	UMaterial* OriginalMaterial = Cast<UMaterial>(UEditorAssetLibrary::LoadAsset(MaterialPath));
	UMaterial* Material = OriginalMaterial ? ResolveWorkingMaterial(OriginalMaterial) : nullptr;
	if (!Material)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Material not found: %s"), *MaterialPath));

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
// HandleGetMaterialInstanceParameters
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleGetMaterialInstanceParameters(
	const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("material_path"), AssetPath) &&
	    !Params->TryGetStringField(TEXT("asset_path"), AssetPath))
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path'"));

	UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
	UMaterialInstanceConstant* MI = Cast<UMaterialInstanceConstant>(Asset);
	if (!MI)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Material instance not found: %s"), *AssetPath));

	// Scalar
	TArray<FName> Names;
	UMaterialEditingLibrary::GetScalarParameterNames(MI, Names);
	auto ScalarObj = MakeShared<FJsonObject>();
	for (const FName& N : Names)
		ScalarObj->SetNumberField(N.ToString(),
			UMaterialEditingLibrary::GetMaterialInstanceScalarParameterValue(MI, N));

	// Vector
	UMaterialEditingLibrary::GetVectorParameterNames(MI, Names);
	auto VectorObj = MakeShared<FJsonObject>();
	for (const FName& N : Names)
	{
		FLinearColor C = UMaterialEditingLibrary::GetMaterialInstanceVectorParameterValue(MI, N);
		TArray<TSharedPtr<FJsonValue>> Arr;
		Arr.Add(MakeShared<FJsonValueNumber>(C.R));
		Arr.Add(MakeShared<FJsonValueNumber>(C.G));
		Arr.Add(MakeShared<FJsonValueNumber>(C.B));
		Arr.Add(MakeShared<FJsonValueNumber>(C.A));
		VectorObj->SetArrayField(N.ToString(), Arr);
	}

	// Texture
	UMaterialEditingLibrary::GetTextureParameterNames(MI, Names);
	auto TexObj = MakeShared<FJsonObject>();
	for (const FName& N : Names)
	{
		UTexture* T = UMaterialEditingLibrary::GetMaterialInstanceTextureParameterValue(MI, N);
		TexObj->SetStringField(N.ToString(), T ? T->GetPathName() : TEXT("None"));
	}

	// Static switch
	UMaterialEditingLibrary::GetStaticSwitchParameterNames(MI, Names);
	auto SwitchObj = MakeShared<FJsonObject>();
	for (const FName& N : Names)
		SwitchObj->SetBoolField(N.ToString(),
			UMaterialEditingLibrary::GetMaterialInstanceStaticSwitchParameterValue(MI, N));

	auto R = MakeShared<FJsonObject>();
	R->SetBoolField(TEXT("success"), true);
	R->SetStringField(TEXT("path"), AssetPath);
	R->SetObjectField(TEXT("scalar"),        ScalarObj);
	R->SetObjectField(TEXT("vector"),        VectorObj);
	R->SetObjectField(TEXT("texture"),       TexObj);
	R->SetObjectField(TEXT("static_switch"), SwitchObj);
	return R;
}

// ---------------------------------------------------------------------------
// HandleSetMaterialInstanceParameter
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleSetMaterialInstanceParameter(
	const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("material_path"), AssetPath) &&
	    !Params->TryGetStringField(TEXT("asset_path"), AssetPath))
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path'"));
	FString ParamName;
	if (!Params->TryGetStringField(TEXT("param_name"), ParamName))
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'param_name'"));
	FString ParamType;
	if (!Params->TryGetStringField(TEXT("param_type"), ParamType))
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Missing 'param_type': scalar | vector | texture | static_switch"));

	UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
	UMaterialInstanceConstant* MI = Cast<UMaterialInstanceConstant>(Asset);
	if (!MI)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Material instance not found: %s"), *AssetPath));

	FName ParamFName(*ParamName);
	FString Lower = ParamType.ToLower();
	bool bSuccess = false;
	FString ErrMsg;

	if (Lower == TEXT("scalar") || Lower == TEXT("float"))
	{
		// UE5 bug: SetMaterialInstanceScalarParameterValue never sets bResult=true.
		// Call SetScalarParameterValueEditorOnly directly.
		double Val;
		if (Params->TryGetNumberField(TEXT("value"), Val))
		{
			MI->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(ParamFName), (float)Val);
			bSuccess = true;
		}
		else
		{
			// value may arrive as a string (e.g. "0.5") — parse it
			FString ValStr;
			if (Params->TryGetStringField(TEXT("value"), ValStr) && FCString::IsNumeric(*ValStr))
			{
				MI->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(ParamFName), FCString::Atof(*ValStr));
				bSuccess = true;
			}
			else
				ErrMsg = TEXT("Missing 'value' number");
		}
	}
	else if (Lower == TEXT("vector") || Lower == TEXT("color"))
	{
		// Try direct JSON array first, then fall back to parsing a string-encoded array
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		TSharedPtr<FJsonValue> ParsedValue;
		TArray<TSharedPtr<FJsonValue>> ParsedArray;

		if (Params->TryGetArrayField(TEXT("value"), Arr) && Arr && Arr->Num() >= 3)
		{
			// Direct JSON array — use as-is
		}
		else
		{
			// value is likely a string like "[1.0, 0.1, 0.05, 1.0]" — parse it
			FString ValStr;
			if (Params->TryGetStringField(TEXT("value"), ValStr))
			{
				TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ValStr);
				if (FJsonSerializer::Deserialize(Reader, ParsedValue) && ParsedValue.IsValid()
					&& ParsedValue->Type == EJson::Array)
				{
					ParsedArray = ParsedValue->AsArray();
					Arr = &ParsedArray;
				}
			}
		}

		if (!Arr || Arr->Num() < 3)
		{
			ErrMsg = TEXT("Missing 'value' [r,g,b] or [r,g,b,a] array");
		}
		else
		{
			FLinearColor C(
				(float)(*Arr)[0]->AsNumber(), (float)(*Arr)[1]->AsNumber(), (float)(*Arr)[2]->AsNumber(),
				Arr->Num() > 3 ? (float)(*Arr)[3]->AsNumber() : 1.0f);
			// UE5 bug: UMaterialEditingLibrary::SetMaterialInstanceVectorParameterValue
			// never sets bResult=true, always returns false even on success.
			// Call SetVectorParameterValueEditorOnly directly to work around it.
			MI->SetVectorParameterValueEditorOnly(FMaterialParameterInfo(ParamFName), C);
			bSuccess = true;
		}
	}
	else if (Lower == TEXT("texture"))
	{
		FString TexPath;
		if (!Params->TryGetStringField(TEXT("value"), TexPath))
			ErrMsg = TEXT("Missing 'value' texture path");
		else
		{
			UTexture* Tex = Cast<UTexture>(UEditorAssetLibrary::LoadAsset(TexPath));
			if (!Tex) ErrMsg = FString::Printf(TEXT("Texture not found: %s"), *TexPath);
			else
			{
				// UE5 bug: same bResult issue as scalar/vector — call directly.
				MI->SetTextureParameterValueEditorOnly(FMaterialParameterInfo(ParamFName), Tex);
				bSuccess = true;
			}
		}
	}
	else if (Lower == TEXT("static_switch") || Lower == TEXT("switch") || Lower == TEXT("bool"))
	{
		bool Val = false;
		Params->TryGetBoolField(TEXT("value"), Val);
		bSuccess = UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(MI, ParamFName, Val);
	}
	else
	{
		ErrMsg = FString::Printf(TEXT("Unknown param_type '%s'. Use: scalar|vector|texture|static_switch"), *ParamType);
	}

	if (!ErrMsg.IsEmpty())
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(ErrMsg);

	if (bSuccess)
	{
		// Mirror what the static switch setter does (MaterialEditingLibrary.cpp:1285-1286):
		// Create a transient UMaterialEditorInstanceConstant and call SetSourceInstance().
		// This triggers RegenerateArrays() which properly registers parameter overrides
		// so the MI editor doesn't wipe values on first open.
		UMaterialEditorInstanceConstant* EditorInst = NewObject<UMaterialEditorInstanceConstant>(
			GetTransientPackage(), NAME_None, RF_Transactional);
		EditorInst->SetSourceInstance(MI);

		UMaterialEditingLibrary::UpdateMaterialInstance(MI);

		// Rebuild any already-open MI editors so parameter checkboxes refresh immediately.
		// UpdateMaterialInstance only calls NotifyExternalMaterialChange->Refresh which
		// does NOT call RegenerateArrays(). RebuildMaterialInstanceEditors does.
		UMaterial* BaseMaterial = MI->GetMaterial();
		if (BaseMaterial)
		{
			UMaterialEditingLibrary::RebuildMaterialInstanceEditors(BaseMaterial);
		}

		UEditorAssetLibrary::SaveAsset(AssetPath);
	}

	auto R = MakeShared<FJsonObject>();
	R->SetBoolField(TEXT("success"), bSuccess);
	R->SetStringField(TEXT("path"), AssetPath);
	R->SetStringField(TEXT("param_name"), ParamName);
	R->SetStringField(TEXT("param_type"), ParamType);
	if (!bSuccess)
		R->SetStringField(TEXT("error"), TEXT("SetParameter returned false — parameter may not exist in this instance's parent"));
	return R;
}

// ---------------------------------------------------------------------------
// HandleListMaterialExpressionTypes
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleListMaterialExpressionTypes(
	const TSharedPtr<FJsonObject>& Params)
{
	FString Filter;
	Params->TryGetStringField(TEXT("filter"), Filter);

	static const FString ExpressionPrefix = TEXT("MaterialExpression");

	// Classes to exclude (same as Epic's MaterialExpressionClasses.cpp)
	static TSet<FName> ExcludedClasses = {
		TEXT("MaterialExpressionComment"),            // Handled by add_material_comments
		TEXT("MaterialExpressionParameter"),           // Abstract base for parameter nodes
		TEXT("MaterialExpressionNamedRerouteUsage"),   // Internal reroute usage
		TEXT("MaterialExpressionMaterialLayerOutput"), // Internal layer output
	};

	TArray<TSharedPtr<FJsonValue>> ResultArray;

	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated))
			continue;
		if (!Class->IsChildOf(UMaterialExpression::StaticClass()))
			continue;
		if (Class->HasMetaData(TEXT("Private")))
			continue;
		if (ExcludedClasses.Contains(Class->GetFName()))
			continue;

		FString ClassName = Class->GetName();

		// Strip the "MaterialExpression" prefix for the short name
		FString ShortName = ClassName;
		if (ShortName.StartsWith(ExpressionPrefix, ESearchCase::CaseSensitive))
		{
			ShortName = ShortName.Mid(ExpressionPrefix.Len());
		}

		// Get display name if available
		FString DisplayName;
		if (Class->HasMetaData(TEXT("DisplayName")))
		{
			DisplayName = Class->GetDisplayNameText().ToString();
		}

		// Get CDO for keywords, categories, descriptions
		UMaterialExpression* CDO = Cast<UMaterialExpression>(Class->GetDefaultObject());

		// Get keywords (lerp, sin, cos, +, -, *, /, etc.)
		FString Keywords;
		if (CDO)
		{
			Keywords = CDO->GetKeywords().ToString();
		}

		// Get creation name (alternate search name)
		FString CreationName;
		if (CDO)
		{
			CreationName = CDO->GetCreationName().ToString();
		}

		// Apply filter — match against all searchable fields
		if (!Filter.IsEmpty())
		{
			bool bMatches = ShortName.Contains(Filter, ESearchCase::IgnoreCase)
				|| (!DisplayName.IsEmpty() && DisplayName.Contains(Filter, ESearchCase::IgnoreCase))
				|| (!Keywords.IsEmpty() && Keywords.Contains(Filter, ESearchCase::IgnoreCase))
				|| (!CreationName.IsEmpty() && CreationName.Contains(Filter, ESearchCase::IgnoreCase));

			// Also check categories
			if (!bMatches && CDO)
			{
				for (const FText& Cat : CDO->MenuCategories)
				{
					if (Cat.ToString().Contains(Filter, ESearchCase::IgnoreCase))
					{
						bMatches = true;
						break;
					}
				}
			}

			if (!bMatches)
				continue;
		}

		// Build result entry — only include fields that have values to minimize tokens
		auto Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("type"), ShortName);

		if (!DisplayName.IsEmpty() && DisplayName != ShortName)
		{
			Entry->SetStringField(TEXT("display_name"), DisplayName);
		}

		if (!Keywords.IsEmpty())
		{
			Entry->SetStringField(TEXT("keywords"), Keywords);
		}

		if (CDO)
		{
			if (CDO->MenuCategories.Num() > 0)
			{
				TArray<TSharedPtr<FJsonValue>> CatArray;
				for (const FText& Cat : CDO->MenuCategories)
				{
					CatArray.Add(MakeShared<FJsonValueString>(Cat.ToString()));
				}
				Entry->SetArrayField(TEXT("categories"), CatArray);
			}

			FString Desc = CDO->GetCreationDescription().ToString();
			if (!Desc.IsEmpty())
			{
				Entry->SetStringField(TEXT("description"), Desc);
			}
		}

		ResultArray.Add(MakeShared<FJsonValueObject>(Entry));
	}

	// Sort by short name
	ResultArray.Sort([](const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B)
	{
		return A->AsObject()->GetStringField(TEXT("type")) <
		       B->AsObject()->GetStringField(TEXT("type"));
	});

	auto R = MakeShared<FJsonObject>();
	R->SetBoolField(TEXT("success"), true);
	R->SetNumberField(TEXT("count"), ResultArray.Num());
	if (!Filter.IsEmpty())
	{
		R->SetStringField(TEXT("filter"), Filter);
	}
	R->SetArrayField(TEXT("expression_types"), ResultArray);
	return R;
}
