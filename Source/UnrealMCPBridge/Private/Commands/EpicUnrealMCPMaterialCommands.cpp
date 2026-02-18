#include "Commands/EpicUnrealMCPMaterialCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"

#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionComment.h"
#include "Materials/MaterialExpression.h"
#include "MaterialEditingLibrary.h"
#include "EditorAssetLibrary.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Factories/MaterialFactoryNew.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

// ---------------------------------------------------------------------------
// Construction
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
	if (CommandType == TEXT("create_material"))
	{
		return HandleCreateMaterial(Params);
	}
	else if (CommandType == TEXT("create_material_instance"))
	{
		return HandleCreateMaterialInstance(Params);
	}
	else if (CommandType == TEXT("build_material_graph"))
	{
		return HandleBuildMaterialGraph(Params);
	}
	else if (CommandType == TEXT("get_material_info"))
	{
		return HandleGetMaterialInfo(Params);
	}
	else if (CommandType == TEXT("recompile_material"))
	{
		return HandleRecompileMaterial(Params);
	}
	else if (CommandType == TEXT("set_material_properties"))
	{
		return HandleSetMaterialProperties(Params);
	}
	else if (CommandType == TEXT("add_material_comments"))
	{
		return HandleAddMaterialComments(Params);
	}

	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
		FString::Printf(TEXT("Unknown material command: %s"), *CommandType));
}

// ---------------------------------------------------------------------------
// Enum Resolution Helpers
// ---------------------------------------------------------------------------

EBlendMode FEpicUnrealMCPMaterialCommands::ResolveBlendMode(const FString& Name)
{
	FString Lower = Name.ToLower();
	if (Lower == TEXT("opaque"))             return BLEND_Opaque;
	if (Lower == TEXT("masked"))             return BLEND_Masked;
	if (Lower == TEXT("translucent"))        return BLEND_Translucent;
	if (Lower == TEXT("additive"))           return BLEND_Additive;
	if (Lower == TEXT("modulate"))           return BLEND_Modulate;
	if (Lower == TEXT("alpha_composite") ||
	    Lower == TEXT("alphacomposite"))      return BLEND_AlphaComposite;
	if (Lower == TEXT("alpha_holdout") ||
	    Lower == TEXT("alphaholdout"))        return BLEND_AlphaHoldout;
	return BLEND_Opaque;
}

EMaterialShadingModel FEpicUnrealMCPMaterialCommands::ResolveShadingModel(const FString& Name)
{
	FString Lower = Name.ToLower();
	if (Lower == TEXT("default_lit") ||
	    Lower == TEXT("defaultlit"))          return MSM_DefaultLit;
	if (Lower == TEXT("unlit"))              return MSM_Unlit;
	if (Lower == TEXT("subsurface"))         return MSM_Subsurface;
	if (Lower == TEXT("clear_coat") ||
	    Lower == TEXT("clearcoat"))           return MSM_ClearCoat;
	if (Lower == TEXT("subsurface_profile") ||
	    Lower == TEXT("subsurfaceprofile"))   return MSM_SubsurfaceProfile;
	if (Lower == TEXT("two_sided_foliage") ||
	    Lower == TEXT("twosidedfoliage"))     return MSM_TwoSidedFoliage;
	if (Lower == TEXT("cloth"))              return MSM_Cloth;
	if (Lower == TEXT("eye"))                return MSM_Eye;
	if (Lower == TEXT("thin_translucent") ||
	    Lower == TEXT("thintranslucent"))     return MSM_ThinTranslucent;
	return MSM_DefaultLit;
}

EMaterialProperty FEpicUnrealMCPMaterialCommands::ResolveMaterialProperty(const FString& Name)
{
	if (Name == TEXT("BaseColor"))               return MP_BaseColor;
	if (Name == TEXT("Metallic"))                return MP_Metallic;
	if (Name == TEXT("Roughness"))               return MP_Roughness;
	if (Name == TEXT("EmissiveColor"))           return MP_EmissiveColor;
	if (Name == TEXT("Opacity"))                 return MP_Opacity;
	if (Name == TEXT("OpacityMask"))             return MP_OpacityMask;
	if (Name == TEXT("Normal"))                  return MP_Normal;
	if (Name == TEXT("Specular"))                return MP_Specular;
	if (Name == TEXT("AmbientOcclusion"))        return MP_AmbientOcclusion;
	if (Name == TEXT("WorldPositionOffset"))     return MP_WorldPositionOffset;
	if (Name == TEXT("SubsurfaceColor"))         return MP_SubsurfaceColor;
	if (Name == TEXT("Refraction"))              return MP_Refraction;
	return MP_MAX; // invalid sentinel
}

// ---------------------------------------------------------------------------
// FindExpressionClass — dynamic UClass lookup for material expression types
// ---------------------------------------------------------------------------

UClass* FEpicUnrealMCPMaterialCommands::FindExpressionClass(const FString& TypeName)
{
	FString ClassName = TypeName;
	if (!ClassName.StartsWith(TEXT("MaterialExpression")))
	{
		ClassName = TEXT("MaterialExpression") + ClassName;
	}

	// Most expressions live in /Script/Engine
	FString Path = FString::Printf(TEXT("/Script/Engine.%s"), *ClassName);
	UClass* Result = FindObject<UClass>(nullptr, *Path);
	if (Result) return Result;

	// Try other modules for specialized expressions
	static const TCHAR* OtherModules[] = { TEXT("Landscape"), TEXT("HairStrands") };
	for (const TCHAR* Module : OtherModules)
	{
		Path = FString::Printf(TEXT("/Script/%s.%s"), Module, *ClassName);
		Result = FindObject<UClass>(nullptr, *Path);
		if (Result) return Result;
	}

	UE_LOG(LogTemp, Warning, TEXT("FEpicUnrealMCPMaterialCommands: Could not find expression class: %s"), *ClassName);
	return nullptr;
}

// ---------------------------------------------------------------------------
// SetExpressionProperty — set a property on a material expression node
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

	// Asset path: string starting with "/" — load the asset and set via reflection
	if (Value->Type == EJson::String)
	{
		FString StrVal = Value->AsString();
		if (StrVal.StartsWith(TEXT("/")))
		{
			UObject* Asset = UEditorAssetLibrary::LoadAsset(StrVal);
			if (!Asset)
			{
				OutError = FString::Printf(TEXT("Failed to load asset: %s"), *StrVal);
				return false;
			}

			FProperty* Prop = Expr->GetClass()->FindPropertyByName(*PropName);
			if (!Prop)
			{
				OutError = FString::Printf(TEXT("Property not found: %s"), *PropName);
				return false;
			}

			FObjectProperty* ObjProp = CastField<FObjectProperty>(Prop);
			if (ObjProp)
			{
				ObjProp->SetObjectPropertyValue(ObjProp->ContainerPtrToValuePtr<void>(Expr), Asset);
				return true;
			}

			OutError = FString::Printf(TEXT("Property %s is not an object property"), *PropName);
			return false;
		}
	}

	// Array values: [r,g,b,a] → FLinearColor, [x,y] → FVector2D
	if (Value->Type == EJson::Array)
	{
		const TArray<TSharedPtr<FJsonValue>>& Arr = Value->AsArray();

		if (Arr.Num() == 2)
		{
			// Try FVector2D first, then FLinearColor
			FProperty* Prop = Expr->GetClass()->FindPropertyByName(*PropName);
			if (Prop)
			{
				FStructProperty* StructProp = CastField<FStructProperty>(Prop);
				if (StructProp && StructProp->Struct == TBaseStructure<FVector2D>::Get())
				{
					FVector2D Vec(Arr[0]->AsNumber(), Arr[1]->AsNumber());
					StructProp->CopyCompleteValue(StructProp->ContainerPtrToValuePtr<void>(Expr), &Vec);
					return true;
				}
			}
		}

		if (Arr.Num() >= 3)
		{
			FProperty* Prop = Expr->GetClass()->FindPropertyByName(*PropName);
			if (Prop)
			{
				FStructProperty* StructProp = CastField<FStructProperty>(Prop);
				if (StructProp && StructProp->Struct == TBaseStructure<FLinearColor>::Get())
				{
					float R = (float)Arr[0]->AsNumber();
					float G = (float)Arr[1]->AsNumber();
					float B = (float)Arr[2]->AsNumber();
					float A = Arr.Num() > 3 ? (float)Arr[3]->AsNumber() : 1.0f;
					FLinearColor Color(R, G, B, A);
					StructProp->CopyCompleteValue(StructProp->ContainerPtrToValuePtr<void>(Expr), &Color);
					return true;
				}
				// Also try FVector for 3-component
				if (StructProp && StructProp->Struct == TBaseStructure<FVector>::Get() && Arr.Num() >= 3)
				{
					FVector Vec(Arr[0]->AsNumber(), Arr[1]->AsNumber(), Arr[2]->AsNumber());
					StructProp->CopyCompleteValue(StructProp->ContainerPtrToValuePtr<void>(Expr), &Vec);
					return true;
				}
			}
		}
	}

	// Delegate to the generic utility for basic types (bool, int, float, string, enum)
	return FEpicUnrealMCPCommonUtils::SetObjectProperty(Expr, PropName, Value, OutError);
}

// ---------------------------------------------------------------------------
// HandleCustomHLSLNode — configure a MaterialExpressionCustom
// ---------------------------------------------------------------------------

void FEpicUnrealMCPMaterialCommands::HandleCustomHLSLNode(
	UMaterialExpression* Expr, const TSharedPtr<FJsonObject>& NodeDef)
{
	UMaterialExpressionCustom* Custom = Cast<UMaterialExpressionCustom>(Expr);
	if (!Custom) return;

	// HLSL code body
	FString Code;
	if (NodeDef->TryGetStringField(TEXT("code"), Code))
	{
		Custom->Code = Code;
	}

	// Description (node title)
	FString Description;
	if (NodeDef->TryGetStringField(TEXT("description"), Description))
	{
		Custom->Description = Description;
	}

	// Output type
	FString OutputTypeStr;
	if (NodeDef->TryGetStringField(TEXT("output_type"), OutputTypeStr))
	{
		FString Lower = OutputTypeStr.ToLower();
		if (Lower == TEXT("float") || Lower == TEXT("float1"))
			Custom->OutputType = CMOT_Float1;
		else if (Lower == TEXT("float2"))
			Custom->OutputType = CMOT_Float2;
		else if (Lower == TEXT("float3"))
			Custom->OutputType = CMOT_Float3;
		else if (Lower == TEXT("float4"))
			Custom->OutputType = CMOT_Float4;
		else if (Lower == TEXT("material_attributes") || Lower == TEXT("materialattributes"))
			Custom->OutputType = CMOT_MaterialAttributes;
	}

	// Named inputs
	const TArray<TSharedPtr<FJsonValue>>* InputsArray;
	if (NodeDef->TryGetArrayField(TEXT("inputs"), InputsArray))
	{
		Custom->Inputs.Empty();
		for (const TSharedPtr<FJsonValue>& InputVal : *InputsArray)
		{
			FCustomInput NewInput;
			NewInput.InputName = FName(*InputVal->AsString());
			Custom->Inputs.Add(NewInput);
		}
	}

	// Additional outputs
	const TArray<TSharedPtr<FJsonValue>>* OutputsArray;
	if (NodeDef->TryGetArrayField(TEXT("outputs"), OutputsArray))
	{
		Custom->AdditionalOutputs.Empty();
		for (const TSharedPtr<FJsonValue>& OutputVal : *OutputsArray)
		{
			const TSharedPtr<FJsonObject>& OutObj = OutputVal->AsObject();
			if (!OutObj.IsValid()) continue;

			FCustomOutput NewOutput;
			FString OutputName;
			if (OutObj->TryGetStringField(TEXT("name"), OutputName))
			{
				NewOutput.OutputName = FName(*OutputName);
			}

			FString OutType;
			if (OutObj->TryGetStringField(TEXT("type"), OutType))
			{
				FString Lower = OutType.ToLower();
				if (Lower == TEXT("float") || Lower == TEXT("float1"))
					NewOutput.OutputType = ECustomMaterialOutputType::CMOT_Float1;
				else if (Lower == TEXT("float2"))
					NewOutput.OutputType = ECustomMaterialOutputType::CMOT_Float2;
				else if (Lower == TEXT("float3"))
					NewOutput.OutputType = ECustomMaterialOutputType::CMOT_Float3;
				else if (Lower == TEXT("float4"))
					NewOutput.OutputType = ECustomMaterialOutputType::CMOT_Float4;
				else if (Lower == TEXT("material_attributes") || Lower == TEXT("materialattributes"))
					NewOutput.OutputType = ECustomMaterialOutputType::CMOT_MaterialAttributes;
			}

			Custom->AdditionalOutputs.Add(NewOutput);
		}
	}
}

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

	FString ShadingModelStr = TEXT("default_lit");
	Params->TryGetStringField(TEXT("shading_model"), ShadingModelStr);

	bool bTwoSided = false;
	Params->TryGetBoolField(TEXT("two_sided"), bTwoSided);

	// Create material via AssetTools
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();

	UObject* NewAsset = AssetTools.CreateAsset(Name, Path, UMaterial::StaticClass(), Factory);
	UMaterial* Material = Cast<UMaterial>(NewAsset);
	if (!Material)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Failed to create material (may already exist)"));
	}

	// Set properties
	Material->BlendMode = ResolveBlendMode(BlendModeStr);
	Material->SetShadingModel(ResolveShadingModel(ShadingModelStr));
	Material->TwoSided = bTwoSided;

	// Optional opacity mask clip value
	double OpacityClip;
	if (Params->TryGetNumberField(TEXT("opacity_mask_clip_value"), OpacityClip))
	{
		Material->OpacityMaskClipValue = (float)OpacityClip;
	}

	// Save
	FString FullPath = Path / Name;
	UEditorAssetLibrary::SaveAsset(FullPath);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("path"), FullPath);
	return Result;
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

	// Create material instance
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();

	UObject* NewAsset = AssetTools.CreateAsset(Name, Path, UMaterialInstanceConstant::StaticClass(), Factory);
	UMaterialInstanceConstant* MI = Cast<UMaterialInstanceConstant>(NewAsset);
	if (!MI)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Failed to create material instance (may already exist)"));
	}

	// Set parent
	UMaterialInterface* Parent = Cast<UMaterialInterface>(UEditorAssetLibrary::LoadAsset(ParentPath));
	if (Parent)
	{
		UMaterialEditingLibrary::SetMaterialInstanceParent(MI, Parent);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("FEpicUnrealMCPMaterialCommands: Parent material not found: %s"), *ParentPath);
	}

	// Scalar parameters
	const TSharedPtr<FJsonObject>* ScalarObj;
	if (Params->TryGetObjectField(TEXT("scalar_params"), ScalarObj))
	{
		for (const auto& Pair : (*ScalarObj)->Values)
		{
			float Val = (float)Pair.Value->AsNumber();
			UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(MI, FName(*Pair.Key), Val);
		}
	}

	// Vector parameters
	const TSharedPtr<FJsonObject>* VectorObj;
	if (Params->TryGetObjectField(TEXT("vector_params"), VectorObj))
	{
		for (const auto& Pair : (*VectorObj)->Values)
		{
			const TArray<TSharedPtr<FJsonValue>>& Arr = Pair.Value->AsArray();
			float R = Arr.Num() > 0 ? (float)Arr[0]->AsNumber() : 0.0f;
			float G = Arr.Num() > 1 ? (float)Arr[1]->AsNumber() : 0.0f;
			float B = Arr.Num() > 2 ? (float)Arr[2]->AsNumber() : 0.0f;
			float A = Arr.Num() > 3 ? (float)Arr[3]->AsNumber() : 1.0f;
			UMaterialEditingLibrary::SetMaterialInstanceVectorParameterValue(MI, FName(*Pair.Key), FLinearColor(R, G, B, A));
		}
	}

	// Texture parameters
	const TSharedPtr<FJsonObject>* TextureObj;
	if (Params->TryGetObjectField(TEXT("texture_params"), TextureObj))
	{
		for (const auto& Pair : (*TextureObj)->Values)
		{
			FString TexPath = Pair.Value->AsString();
			UTexture* Texture = Cast<UTexture>(UEditorAssetLibrary::LoadAsset(TexPath));
			if (Texture)
			{
				UMaterialEditingLibrary::SetMaterialInstanceTextureParameterValue(MI, FName(*Pair.Key), Texture);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("FEpicUnrealMCPMaterialCommands: Texture not found: %s"), *TexPath);
			}
		}
	}

	// Save
	FString FullPath = Path / Name;
	UEditorAssetLibrary::SaveAsset(FullPath);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("path"), FullPath);
	return Result;
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
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path' parameter"));
	}

	const TArray<TSharedPtr<FJsonValue>>* NodesArray;
	if (!Params->TryGetArrayField(TEXT("nodes"), NodesArray))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'nodes' array parameter"));
	}

	const TArray<TSharedPtr<FJsonValue>>* ConnectionsArray;
	if (!Params->TryGetArrayField(TEXT("connections"), ConnectionsArray))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'connections' array parameter"));
	}

	bool bClearExisting = true;
	Params->TryGetBoolField(TEXT("clear_existing"), bClearExisting);

	// Load material
	UObject* Asset = UEditorAssetLibrary::LoadAsset(MaterialPath);
	UMaterial* Material = Cast<UMaterial>(Asset);
	if (!Material)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Material not found: %s"), *MaterialPath));
	}

	TArray<FString> Errors;

	// Clear existing expressions (two passes — UE doesn't always get all in one)
	if (bClearExisting)
	{
		UMaterialEditingLibrary::DeleteAllMaterialExpressions(Material);
		UMaterialEditingLibrary::DeleteAllMaterialExpressions(Material);
	}

	// Create nodes
	TArray<UMaterialExpression*> CreatedNodes;
	CreatedNodes.SetNum(NodesArray->Num());

	for (int32 Idx = 0; Idx < NodesArray->Num(); ++Idx)
	{
		const TSharedPtr<FJsonObject>& NodeDef = (*NodesArray)[Idx]->AsObject();
		if (!NodeDef.IsValid())
		{
			Errors.Add(FString::Printf(TEXT("Node %d: invalid JSON object"), Idx));
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
			Errors.Add(FString::Printf(TEXT("Node %d: unknown expression class '%s'"), Idx, *TypeName));
			CreatedNodes[Idx] = nullptr;
			continue;
		}

		UMaterialExpression* NewExpr = UMaterialEditingLibrary::CreateMaterialExpression(
			Material, ExprClass, PosX, PosY);
		if (!NewExpr)
		{
			Errors.Add(FString::Printf(TEXT("Node %d: failed to create expression '%s'"), Idx, *TypeName));
			CreatedNodes[Idx] = nullptr;
			continue;
		}

		CreatedNodes[Idx] = NewExpr;

		// Handle Custom HLSL nodes specially
		bool bIsCustom = TypeName == TEXT("Custom") || TypeName == TEXT("MaterialExpressionCustom");
		if (bIsCustom)
		{
			HandleCustomHLSLNode(NewExpr, NodeDef);
		}

		// Set generic properties
		const TSharedPtr<FJsonObject>* PropsObj;
		if (NodeDef->TryGetObjectField(TEXT("properties"), PropsObj))
		{
			for (const auto& Pair : (*PropsObj)->Values)
			{
				FString PropError;
				if (!SetExpressionProperty(NewExpr, Pair.Key, Pair.Value, PropError))
				{
					Errors.Add(FString::Printf(TEXT("Node %d property '%s': %s"), Idx, *Pair.Key, *PropError));
				}
			}
		}
	}

	// Create connections
	int32 ConnectionsMade = 0;
	for (const TSharedPtr<FJsonValue>& ConnVal : *ConnectionsArray)
	{
		const TSharedPtr<FJsonObject>& Conn = ConnVal->AsObject();
		if (!Conn.IsValid()) continue;

		// from_node is always an int index
		int32 FromIdx = 0;
		Conn->TryGetNumberField(TEXT("from_node"), FromIdx);

		FString FromPin;
		Conn->TryGetStringField(TEXT("from_pin"), FromPin);

		FString ToPin;
		Conn->TryGetStringField(TEXT("to_pin"), ToPin);

		// Check if to_node is "material" (string) or an index (number)
		FString ToNodeStr;
		int32 ToIdx = 0;
		bool bToMaterial = false;

		if (Conn->HasField(TEXT("to_node")))
		{
			TSharedPtr<FJsonValue> ToNodeJsonVal = Conn->TryGetField(TEXT("to_node"));
			if (ToNodeJsonVal.IsValid())
			{
				if (ToNodeJsonVal->Type == EJson::String)
				{
					ToNodeStr = ToNodeJsonVal->AsString();
					bToMaterial = (ToNodeStr == TEXT("material"));
				}
				else if (ToNodeJsonVal->Type == EJson::Number)
				{
					ToIdx = (int32)ToNodeJsonVal->AsNumber();
				}
			}
		}

		// Validate from_node
		if (FromIdx < 0 || FromIdx >= CreatedNodes.Num() || !CreatedNodes[FromIdx])
		{
			Errors.Add(FString::Printf(TEXT("Connection: invalid from_node %d"), FromIdx));
			continue;
		}

		if (bToMaterial)
		{
			// Connect to material output property
			EMaterialProperty MatProp = ResolveMaterialProperty(ToPin);
			if (MatProp == MP_MAX)
			{
				Errors.Add(FString::Printf(TEXT("Connection: unknown material property '%s'"), *ToPin));
				continue;
			}

			bool bConnected = UMaterialEditingLibrary::ConnectMaterialProperty(
				CreatedNodes[FromIdx], FromPin, MatProp);
			if (bConnected)
			{
				ConnectionsMade++;
			}
			else
			{
				Errors.Add(FString::Printf(TEXT("Connection: failed to connect node %d to material.%s"), FromIdx, *ToPin));
			}
		}
		else
		{
			// Node-to-node connection
			if (ToIdx < 0 || ToIdx >= CreatedNodes.Num() || !CreatedNodes[ToIdx])
			{
				Errors.Add(FString::Printf(TEXT("Connection: invalid to_node %d"), ToIdx));
				continue;
			}

			bool bConnected = UMaterialEditingLibrary::ConnectMaterialExpressions(
				CreatedNodes[FromIdx], FromPin, CreatedNodes[ToIdx], ToPin);
			if (bConnected)
			{
				ConnectionsMade++;
			}
			else
			{
				Errors.Add(FString::Printf(TEXT("Connection: failed to connect node %d.%s -> node %d.%s"),
					FromIdx, *FromPin, ToIdx, *ToPin));
			}
		}
	}

	// Recompile and save
	UMaterialEditingLibrary::RecompileMaterial(Material);
	UEditorAssetLibrary::SaveAsset(MaterialPath);

	// Build result
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("path"), MaterialPath);
	Result->SetNumberField(TEXT("nodes_created"), CreatedNodes.Num());
	Result->SetNumberField(TEXT("connections_made"), ConnectionsMade);

	TArray<TSharedPtr<FJsonValue>> ErrorArray;
	for (const FString& Err : Errors)
	{
		ErrorArray.Add(MakeShared<FJsonValueString>(Err));
	}
	Result->SetArrayField(TEXT("errors"), ErrorArray);

	return Result;
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
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path' parameter"));
	}

	UObject* Asset = UEditorAssetLibrary::LoadAsset(MaterialPath);
	UMaterial* Material = Cast<UMaterial>(Asset);
	if (!Material)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Material not found: %s"), *MaterialPath));
	}

	TSharedPtr<FJsonObject> Info = MakeShared<FJsonObject>();
	Info->SetStringField(TEXT("path"), MaterialPath);

	// Blend mode
	FString BlendModeStr;
	switch (Material->BlendMode)
	{
	case BLEND_Opaque:          BlendModeStr = TEXT("opaque"); break;
	case BLEND_Masked:          BlendModeStr = TEXT("masked"); break;
	case BLEND_Translucent:     BlendModeStr = TEXT("translucent"); break;
	case BLEND_Additive:        BlendModeStr = TEXT("additive"); break;
	case BLEND_Modulate:        BlendModeStr = TEXT("modulate"); break;
	case BLEND_AlphaComposite:  BlendModeStr = TEXT("alpha_composite"); break;
	case BLEND_AlphaHoldout:    BlendModeStr = TEXT("alpha_holdout"); break;
	default:                    BlendModeStr = TEXT("unknown"); break;
	}
	Info->SetStringField(TEXT("blend_mode"), BlendModeStr);

	// Shading model
	FMaterialShadingModelField ShadingModels = Material->GetShadingModels();
	FString ShadingModelStr = TEXT("default_lit");
	if (ShadingModels.HasShadingModel(MSM_Unlit))                ShadingModelStr = TEXT("unlit");
	else if (ShadingModels.HasShadingModel(MSM_Subsurface))      ShadingModelStr = TEXT("subsurface");
	else if (ShadingModels.HasShadingModel(MSM_ClearCoat))       ShadingModelStr = TEXT("clear_coat");
	else if (ShadingModels.HasShadingModel(MSM_SubsurfaceProfile)) ShadingModelStr = TEXT("subsurface_profile");
	else if (ShadingModels.HasShadingModel(MSM_TwoSidedFoliage)) ShadingModelStr = TEXT("two_sided_foliage");
	else if (ShadingModels.HasShadingModel(MSM_Cloth))           ShadingModelStr = TEXT("cloth");
	else if (ShadingModels.HasShadingModel(MSM_Eye))             ShadingModelStr = TEXT("eye");
	else if (ShadingModels.HasShadingModel(MSM_ThinTranslucent)) ShadingModelStr = TEXT("thin_translucent");
	Info->SetStringField(TEXT("shading_model"), ShadingModelStr);

	// Two-sided
	Info->SetBoolField(TEXT("two_sided"), Material->TwoSided);

	// Expression count
	Info->SetNumberField(TEXT("num_expressions"),
		UMaterialEditingLibrary::GetNumMaterialExpressions(Material));

	// Used textures
	TArray<UTexture*> UsedTextures;
	Material->GetUsedTextures(UsedTextures, EMaterialQualityLevel::High);

	TArray<TSharedPtr<FJsonValue>> TextureArray;
	for (UTexture* Tex : UsedTextures)
	{
		if (Tex)
		{
			TextureArray.Add(MakeShared<FJsonValueString>(Tex->GetPathName()));
		}
	}
	Info->SetArrayField(TEXT("used_textures"), TextureArray);

	// Collect parameter names via the material interface
	TArray<FMaterialParameterInfo> ScalarParams;
	TArray<FGuid> ScalarGuids;
	Material->GetAllScalarParameterInfo(ScalarParams, ScalarGuids);
	TArray<TSharedPtr<FJsonValue>> ScalarNames;
	for (const FMaterialParameterInfo& P : ScalarParams)
	{
		ScalarNames.Add(MakeShared<FJsonValueString>(P.Name.ToString()));
	}
	Info->SetArrayField(TEXT("scalar_parameters"), ScalarNames);

	TArray<FMaterialParameterInfo> VectorParams;
	TArray<FGuid> VectorGuids;
	Material->GetAllVectorParameterInfo(VectorParams, VectorGuids);
	TArray<TSharedPtr<FJsonValue>> VectorNames;
	for (const FMaterialParameterInfo& P : VectorParams)
	{
		VectorNames.Add(MakeShared<FJsonValueString>(P.Name.ToString()));
	}
	Info->SetArrayField(TEXT("vector_parameters"), VectorNames);

	TArray<FMaterialParameterInfo> TextureParams;
	TArray<FGuid> TextureGuids;
	Material->GetAllTextureParameterInfo(TextureParams, TextureGuids);
	TArray<TSharedPtr<FJsonValue>> TextureParamNames;
	for (const FMaterialParameterInfo& P : TextureParams)
	{
		TextureParamNames.Add(MakeShared<FJsonValueString>(P.Name.ToString()));
	}
	Info->SetArrayField(TEXT("texture_parameters"), TextureParamNames);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetObjectField(TEXT("info"), Info);
	return Result;
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
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path' parameter"));
	}

	UObject* Asset = UEditorAssetLibrary::LoadAsset(MaterialPath);
	UMaterial* Material = Cast<UMaterial>(Asset);
	if (!Material)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Material not found: %s"), *MaterialPath));
	}

	UMaterialEditingLibrary::RecompileMaterial(Material);
	UEditorAssetLibrary::SaveAsset(MaterialPath);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("path"), MaterialPath);
	Result->SetStringField(TEXT("message"), TEXT("Recompiled and saved"));
	return Result;
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
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path' parameter"));
	}

	UObject* Asset = UEditorAssetLibrary::LoadAsset(MaterialPath);
	UMaterial* Material = Cast<UMaterial>(Asset);
	if (!Material)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Material not found: %s"), *MaterialPath));
	}

	TArray<TSharedPtr<FJsonValue>> ChangedArray;

	// Blend mode
	FString BlendModeStr;
	if (Params->TryGetStringField(TEXT("blend_mode"), BlendModeStr))
	{
		Material->BlendMode = ResolveBlendMode(BlendModeStr);
		ChangedArray.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("blend_mode=%s"), *BlendModeStr)));
	}

	// Shading model
	FString ShadingModelStr;
	if (Params->TryGetStringField(TEXT("shading_model"), ShadingModelStr))
	{
		Material->SetShadingModel(ResolveShadingModel(ShadingModelStr));
		ChangedArray.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("shading_model=%s"), *ShadingModelStr)));
	}

	// Two-sided
	bool bTwoSided;
	if (Params->TryGetBoolField(TEXT("two_sided"), bTwoSided))
	{
		Material->TwoSided = bTwoSided;
		ChangedArray.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("two_sided=%s"), bTwoSided ? TEXT("true") : TEXT("false"))));
	}

	// Opacity mask clip value
	double OpacityClip;
	if (Params->TryGetNumberField(TEXT("opacity_mask_clip_value"), OpacityClip))
	{
		Material->OpacityMaskClipValue = (float)OpacityClip;
		ChangedArray.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("opacity_mask_clip_value=%f"), OpacityClip)));
	}

	// Dithered LOD transition
	bool bDithered;
	if (Params->TryGetBoolField(TEXT("dithered_lof_transition"), bDithered))
	{
		Material->DitheredLODTransition = bDithered;
		ChangedArray.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("dithered_lof_transition=%s"), bDithered ? TEXT("true") : TEXT("false"))));
	}

	// Allow negative emissive color
	bool bNegativeEmissive;
	if (Params->TryGetBoolField(TEXT("allow_negative_emissive_color"), bNegativeEmissive))
	{
		Material->bAllowNegativeEmissiveColor = bNegativeEmissive;
		ChangedArray.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("allow_negative_emissive_color=%s"), bNegativeEmissive ? TEXT("true") : TEXT("false"))));
	}

	// Recompile
	bool bRecompile = true;
	Params->TryGetBoolField(TEXT("recompile"), bRecompile);
	if (bRecompile)
	{
		UMaterialEditingLibrary::RecompileMaterial(Material);
	}

	// Save
	UEditorAssetLibrary::SaveAsset(MaterialPath);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("path"), MaterialPath);
	Result->SetArrayField(TEXT("changed"), ChangedArray);
	return Result;
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
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path' parameter"));
	}

	const TArray<TSharedPtr<FJsonValue>>* CommentsArray;
	if (!Params->TryGetArrayField(TEXT("comments"), CommentsArray))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'comments' array parameter"));
	}

	UObject* Asset = UEditorAssetLibrary::LoadAsset(MaterialPath);
	UMaterial* Material = Cast<UMaterial>(Asset);
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
		const TSharedPtr<FJsonObject>& CommentDef = (*CommentsArray)[Idx]->AsObject();
		if (!CommentDef.IsValid())
		{
			Errors.Add(FString::Printf(TEXT("Comment %d: invalid JSON object"), Idx));
			continue;
		}

		FString Text;
		if (!CommentDef->TryGetStringField(TEXT("text"), Text))
		{
			Errors.Add(FString::Printf(TEXT("Comment %d: missing 'text' field"), Idx));
			continue;
		}

		// Create the comment object — must be NewObject since CreateMaterialExpression
		// won't work (IsAllowedIn returns false for comments)
		UMaterialExpressionComment* Comment = NewObject<UMaterialExpressionComment>(Material);
		if (!Comment)
		{
			Errors.Add(FString::Printf(TEXT("Comment %d: failed to create object"), Idx));
			continue;
		}

		Comment->Text = Text;

		// Position
		int32 PosX = 0, PosY = 0;
		CommentDef->TryGetNumberField(TEXT("pos_x"), PosX);
		CommentDef->TryGetNumberField(TEXT("pos_y"), PosY);
		Comment->MaterialExpressionEditorX = PosX;
		Comment->MaterialExpressionEditorY = PosY;

		// Size — defaults to a reasonable box
		int32 SizeX = 400, SizeY = 200;
		CommentDef->TryGetNumberField(TEXT("size_x"), SizeX);
		CommentDef->TryGetNumberField(TEXT("size_y"), SizeY);
		Comment->SizeX = SizeX;
		Comment->SizeY = SizeY;

		// Font size
		int32 FontSize = 18;
		CommentDef->TryGetNumberField(TEXT("font_size"), FontSize);
		Comment->FontSize = FMath::Clamp(FontSize, 1, 1000);

		// Comment color [r, g, b, a]
		const TArray<TSharedPtr<FJsonValue>>* ColorArray;
		if (CommentDef->TryGetArrayField(TEXT("color"), ColorArray) && ColorArray->Num() >= 3)
		{
			float R = (float)(*ColorArray)[0]->AsNumber();
			float G = (float)(*ColorArray)[1]->AsNumber();
			float B = (float)(*ColorArray)[2]->AsNumber();
			float A = ColorArray->Num() > 3 ? (float)(*ColorArray)[3]->AsNumber() : 1.0f;
			Comment->CommentColor = FLinearColor(R, G, B, A);
		}

		// Show bubble when zoomed
		bool bBubbleVisible = false;
		if (CommentDef->TryGetBoolField(TEXT("show_bubble"), bBubbleVisible))
		{
			Comment->bCommentBubbleVisible_InDetailsPanel = bBubbleVisible;
		}

		// Color the bubble
		bool bColorBubble = false;
		if (CommentDef->TryGetBoolField(TEXT("color_bubble"), bColorBubble))
		{
			Comment->bColorCommentBubble = bColorBubble;
		}

		// Group mode (move enclosed nodes when dragging)
		bool bGroupMode = true;
		if (CommentDef->TryGetBoolField(TEXT("group_mode"), bGroupMode))
		{
			Comment->bGroupMode = bGroupMode;
		}

		// Register into the material's expression collection
		Collection.AddComment(Comment);
		Comment->Material = Material;
		CommentsCreated++;
	}

	// Mark dirty and save
	Material->MarkPackageDirty();
	UEditorAssetLibrary::SaveAsset(MaterialPath);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("path"), MaterialPath);
	Result->SetNumberField(TEXT("comments_created"), CommentsCreated);

	TArray<TSharedPtr<FJsonValue>> ErrorArray;
	for (const FString& Err : Errors)
	{
		ErrorArray.Add(MakeShared<FJsonValueString>(Err));
	}
	Result->SetArrayField(TEXT("errors"), ErrorArray);

	return Result;
}
