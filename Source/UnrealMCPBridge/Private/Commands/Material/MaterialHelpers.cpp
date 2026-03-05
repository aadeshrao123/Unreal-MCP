#include "Commands/EpicUnrealMCPMaterialCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"

#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionCustom.h"
#include "EdGraph/EdGraphNode.h"
#include "EditorAssetLibrary.h"

// ---------------------------------------------------------------------------
// Enum Resolution
// ---------------------------------------------------------------------------

EBlendMode FEpicUnrealMCPMaterialCommands::ResolveBlendMode(const FString& Name)
{
	FString L = Name.ToLower();
	if (L == TEXT("masked"))                                                return BLEND_Masked;
	if (L == TEXT("translucent"))                                           return BLEND_Translucent;
	if (L == TEXT("additive"))                                              return BLEND_Additive;
	if (L == TEXT("modulate"))                                              return BLEND_Modulate;
	if (L == TEXT("alpha_composite") || L == TEXT("alphacomposite"))         return BLEND_AlphaComposite;
	if (L == TEXT("alpha_holdout")   || L == TEXT("alphaholdout"))           return BLEND_AlphaHoldout;
	return BLEND_Opaque;
}

EMaterialShadingModel FEpicUnrealMCPMaterialCommands::ResolveShadingModel(const FString& Name)
{
	FString L = Name.ToLower();
	if (L == TEXT("unlit"))                                                  return MSM_Unlit;
	if (L == TEXT("subsurface"))                                             return MSM_Subsurface;
	if (L == TEXT("clear_coat")         || L == TEXT("clearcoat"))            return MSM_ClearCoat;
	if (L == TEXT("subsurface_profile") || L == TEXT("subsurfaceprofile"))    return MSM_SubsurfaceProfile;
	if (L == TEXT("two_sided_foliage")  || L == TEXT("twosidedfoliage"))      return MSM_TwoSidedFoliage;
	if (L == TEXT("cloth"))                                                  return MSM_Cloth;
	if (L == TEXT("eye"))                                                    return MSM_Eye;
	if (L == TEXT("thin_translucent")   || L == TEXT("thintranslucent"))      return MSM_ThinTranslucent;
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
// FindExpressionClass
// ---------------------------------------------------------------------------

UClass* FEpicUnrealMCPMaterialCommands::FindExpressionClass(const FString& TypeName)
{
	FString ClassName = TypeName;
	if (!ClassName.StartsWith(TEXT("MaterialExpression")))
	{
		ClassName = TEXT("MaterialExpression") + ClassName;
	}

	static const TCHAR* Modules[] = {
		TEXT("Engine"), TEXT("Landscape"), TEXT("HairStrands")
	};
	for (const TCHAR* Mod : Modules)
	{
		FString Path = FString::Printf(TEXT("/Script/%s.%s"), Mod, *ClassName);
		if (UClass* C = FindObject<UClass>(nullptr, *Path))
		{
			return C;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("MCPMaterial: Cannot find expression class: %s"), *ClassName);
	return nullptr;
}

// ---------------------------------------------------------------------------
// NormalizePropName
// ---------------------------------------------------------------------------

FString FEpicUnrealMCPMaterialCommands::NormalizePropName(UMaterialExpression* Expr, const FString& Name)
{
	if (!Expr)
	{
		return Name;
	}
	if (Expr->GetClass()->FindPropertyByName(*Name))
	{
		return Name;
	}

	// snake_case -> PascalCase
	FString Pascal;
	bool bCapNext = true;
	for (TCHAR Ch : Name)
	{
		if (Ch == TEXT('_'))
		{
			bCapNext = true;
		}
		else if (bCapNext)
		{
			Pascal += FChar::ToUpper(Ch);
			bCapNext = false;
		}
		else
		{
			Pascal += Ch;
		}
	}

	if (!Pascal.IsEmpty() && Expr->GetClass()->FindPropertyByName(*Pascal))
	{
		return Pascal;
	}
	return Name; // Return original; failure handled downstream
}

// ---------------------------------------------------------------------------
// TryParseIntFromJson
// ---------------------------------------------------------------------------

bool FEpicUnrealMCPMaterialCommands::TryParseIntFromJson(const TSharedPtr<FJsonValue>& Val, int32& OutInt)
{
	if (!Val.IsValid())
	{
		return false;
	}

	if (Val->Type == EJson::Number)
	{
		OutInt = (int32)Val->AsNumber();
		return true;
	}

	if (Val->Type == EJson::String)
	{
		FString S = Val->AsString();
		if (S.IsNumeric())
		{
			OutInt = FCString::Atoi(*S);
			return true;
		}
	}
	return false;
}

// ---------------------------------------------------------------------------
// SetExpressionProperty
// ---------------------------------------------------------------------------

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

	// Array value: [r,g,b,a] -> FLinearColor, [x,y] -> FVector2D
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

// ---------------------------------------------------------------------------
// HandleCustomHLSLNode
// ---------------------------------------------------------------------------

void FEpicUnrealMCPMaterialCommands::HandleCustomHLSLNode(
	UMaterialExpression* Expr, const TSharedPtr<FJsonObject>& NodeDef)
{
	UMaterialExpressionCustom* Custom = Cast<UMaterialExpressionCustom>(Expr);
	if (!Custom)
	{
		return;
	}

	FString Code;
	if (NodeDef->TryGetStringField(TEXT("code"), Code))
	{
		Custom->Code = Code;
	}

	FString Desc;
	if (NodeDef->TryGetStringField(TEXT("description"), Desc))
	{
		Custom->Description = Desc;
	}

	FString OutputTypeStr;
	if (NodeDef->TryGetStringField(TEXT("output_type"), OutputTypeStr))
	{
		FString L = OutputTypeStr.ToLower();
		if      (L == TEXT("float") || L == TEXT("float1")) Custom->OutputType = CMOT_Float1;
		else if (L == TEXT("float2"))                        Custom->OutputType = CMOT_Float2;
		else if (L == TEXT("float3"))                        Custom->OutputType = CMOT_Float3;
		else if (L == TEXT("float4"))                        Custom->OutputType = CMOT_Float4;
		else if (L == TEXT("material_attributes") || L == TEXT("materialattributes"))
		{
			Custom->OutputType = CMOT_MaterialAttributes;
		}
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
				if (Existing.InputName == NewName)
				{
					bExists = true;
					break;
				}
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
			if (!OutObj.IsValid())
			{
				continue;
			}
			FCustomOutput NewOut;
			FString OutName;
			if (OutObj->TryGetStringField(TEXT("name"), OutName))
			{
				NewOut.OutputName = FName(*OutName);
			}
			FString OutType;
			if (OutObj->TryGetStringField(TEXT("type"), OutType))
			{
				FString L = OutType.ToLower();
				if      (L == TEXT("float") || L == TEXT("float1")) NewOut.OutputType = CMOT_Float1;
				else if (L == TEXT("float2"))                        NewOut.OutputType = CMOT_Float2;
				else if (L == TEXT("float3"))                        NewOut.OutputType = CMOT_Float3;
				else if (L == TEXT("float4"))                        NewOut.OutputType = CMOT_Float4;
				else if (L == TEXT("material_attributes") || L == TEXT("materialattributes"))
				{
					NewOut.OutputType = CMOT_MaterialAttributes;
				}
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
