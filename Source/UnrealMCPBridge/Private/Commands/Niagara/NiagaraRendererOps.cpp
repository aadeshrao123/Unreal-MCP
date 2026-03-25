#include "Commands/EpicUnrealMCPNiagaraCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "NiagaraHelpers.h"

#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraRendererProperties.h"
#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraMeshRendererProperties.h"
#include "NiagaraRibbonRendererProperties.h"
#include "NiagaraLightRendererProperties.h"
#include "NiagaraComponentRendererProperties.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"

// ---------------------------------------------------------------------------
// Helper: Serialize a renderer to JSON
// ---------------------------------------------------------------------------

static TSharedPtr<FJsonObject> RendererToJson(UNiagaraRendererProperties* Renderer, int32 Index)
{
	auto Obj = MakeShared<FJsonObject>();
	if (!Renderer)
	{
		return Obj;
	}

	Obj->SetNumberField(TEXT("index"), Index);
	Obj->SetStringField(TEXT("name"), Renderer->GetName());
	Obj->SetBoolField(TEXT("enabled"), Renderer->GetIsEnabled());

	if (UNiagaraSpriteRendererProperties* Sprite = Cast<UNiagaraSpriteRendererProperties>(Renderer))
	{
		Obj->SetStringField(TEXT("type"), TEXT("sprite"));
		if (Sprite->Material)
		{
			Obj->SetStringField(TEXT("material"), Sprite->Material->GetPathName());
		}
		Obj->SetStringField(TEXT("facing_mode"), StaticEnum<ENiagaraSpriteFacingMode>()->GetNameStringByValue(static_cast<int64>(Sprite->FacingMode)));
		Obj->SetStringField(TEXT("alignment"), StaticEnum<ENiagaraSpriteAlignment>()->GetNameStringByValue(static_cast<int64>(Sprite->Alignment)));
		Obj->SetNumberField(TEXT("sort_order"), Sprite->SortOrderHint);
	}
	else if (UNiagaraMeshRendererProperties* Mesh = Cast<UNiagaraMeshRendererProperties>(Renderer))
	{
		Obj->SetStringField(TEXT("type"), TEXT("mesh"));
		TArray<TSharedPtr<FJsonValue>> MeshesArr;
		for (const FNiagaraMeshRendererMeshProperties& MeshProp : Mesh->Meshes)
		{
			auto MeshObj = MakeShared<FJsonObject>();
			if (MeshProp.Mesh)
			{
				MeshObj->SetStringField(TEXT("mesh_path"), MeshProp.Mesh->GetPathName());
			}
			MeshesArr.Add(MakeShared<FJsonValueObject>(MeshObj));
		}
		Obj->SetArrayField(TEXT("meshes"), MeshesArr);

		TArray<TSharedPtr<FJsonValue>> OverridesArr;
		for (const FNiagaraMeshMaterialOverride& Override : Mesh->OverrideMaterials)
		{
			auto MatObj = MakeShared<FJsonObject>();
			if (Override.ExplicitMat)
			{
				MatObj->SetStringField(TEXT("material"), Override.ExplicitMat->GetPathName());
			}
			OverridesArr.Add(MakeShared<FJsonValueObject>(MatObj));
		}
		Obj->SetArrayField(TEXT("material_overrides"), OverridesArr);
		Obj->SetNumberField(TEXT("sort_order"), Mesh->SortOrderHint);
	}
	else if (UNiagaraRibbonRendererProperties* Ribbon = Cast<UNiagaraRibbonRendererProperties>(Renderer))
	{
		Obj->SetStringField(TEXT("type"), TEXT("ribbon"));
		if (Ribbon->Material)
		{
			Obj->SetStringField(TEXT("material"), Ribbon->Material->GetPathName());
		}
		Obj->SetNumberField(TEXT("sort_order"), Ribbon->SortOrderHint);
	}
	else if (Cast<UNiagaraLightRendererProperties>(Renderer))
	{
		Obj->SetStringField(TEXT("type"), TEXT("light"));
	}
	else if (Cast<UNiagaraComponentRendererProperties>(Renderer))
	{
		Obj->SetStringField(TEXT("type"), TEXT("component"));
	}
	else
	{
		Obj->SetStringField(TEXT("type"), Renderer->GetClass()->GetName());
	}

	// Serialize attribute bindings (common ones)
#if WITH_EDITORONLY_DATA
	TArray<TSharedPtr<FJsonValue>> BindingsArr;
	const TArray<const FNiagaraVariableAttributeBinding*>& AllBindings = Renderer->GetAttributeBindings();
	for (const FNiagaraVariableAttributeBinding* Binding : AllBindings)
	{
		if (!Binding)
		{
			continue;
		}
		auto BindObj = MakeShared<FJsonObject>();
		BindObj->SetStringField(TEXT("bound_variable"), Binding->GetParamMapBindableVariable().GetName().ToString());
		BindingsArr.Add(MakeShared<FJsonValueObject>(BindObj));
	}
	Obj->SetArrayField(TEXT("bindings"), BindingsArr);
#endif

	return Obj;
}

// ---------------------------------------------------------------------------
// HandleAddNiagaraRenderer
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleAddNiagaraRenderer(
	const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
	FString SystemPath;
	if (!Params->TryGetStringField(TEXT("system_path"), SystemPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'system_path' parameter"));
	}

	FString EmitterName;
	if (!Params->TryGetStringField(TEXT("emitter_name"), EmitterName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'emitter_name' parameter"));
	}

	FString RendererType;
	if (!Params->TryGetStringField(TEXT("renderer_type"), RendererType))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'renderer_type' parameter"));
	}

	FString MaterialPath;
	Params->TryGetStringField(TEXT("material_path"), MaterialPath);

	FString MeshPath;
	Params->TryGetStringField(TEXT("mesh_path"), MeshPath);

	FString Error;
	UNiagaraSystem* System = NiagaraHelpers::LoadNiagaraSystem(SystemPath, Error);
	if (!System)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	int32 EmitterIndex = INDEX_NONE;
	FNiagaraEmitterHandle* Handle = NiagaraHelpers::FindEmitterHandle(System, EmitterName, EmitterIndex, Error);
	if (!Handle)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	FVersionedNiagaraEmitterData* EmitterData = NiagaraHelpers::GetEmitterData(Handle);
	if (!EmitterData)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Emitter has no data"));
	}

	UNiagaraEmitter* Emitter = Handle->GetInstance().Emitter;
	if (!Emitter)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get emitter instance"));
	}

	// Load optional material
	UMaterialInterface* Material = nullptr;
	if (!MaterialPath.IsEmpty())
	{
		Material = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
		if (!Material)
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Failed to load material: %s"), *MaterialPath));
		}
	}

	// Create the renderer
	UNiagaraRendererProperties* NewRenderer = nullptr;
	FString NormalizedType = RendererType.ToLower();

	if (NormalizedType == TEXT("sprite"))
	{
		UNiagaraSpriteRendererProperties* SpriteRenderer = NewObject<UNiagaraSpriteRendererProperties>(Emitter);
		if (Material)
		{
			SpriteRenderer->Material = Material;
		}
		NewRenderer = SpriteRenderer;
	}
	else if (NormalizedType == TEXT("mesh"))
	{
		UNiagaraMeshRendererProperties* MeshRenderer = NewObject<UNiagaraMeshRendererProperties>(Emitter);
		if (!MeshPath.IsEmpty())
		{
			UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
			if (Mesh)
			{
				FNiagaraMeshRendererMeshProperties MeshProps;
				MeshProps.Mesh = Mesh;
				MeshRenderer->Meshes.Add(MeshProps);
			}
		}
		if (Material)
		{
			FNiagaraMeshMaterialOverride Override;
			Override.ExplicitMat = Material;
			MeshRenderer->OverrideMaterials.Add(Override);
		}
		NewRenderer = MeshRenderer;
	}
	else if (NormalizedType == TEXT("ribbon"))
	{
		UNiagaraRibbonRendererProperties* RibbonRenderer = NewObject<UNiagaraRibbonRendererProperties>(Emitter);
		if (Material)
		{
			RibbonRenderer->Material = Material;
		}
		NewRenderer = RibbonRenderer;
	}
	else if (NormalizedType == TEXT("light"))
	{
		NewRenderer = NewObject<UNiagaraLightRendererProperties>(Emitter);
	}
	else if (NormalizedType == TEXT("component"))
	{
		NewRenderer = NewObject<UNiagaraComponentRendererProperties>(Emitter);
	}
	else
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Unknown renderer type '%s'. Use: sprite, mesh, ribbon, light, component"), *RendererType));
	}

	Emitter->AddRenderer(NewRenderer, EmitterData->Version.VersionGuid);
	NiagaraHelpers::CompileAndSync(System);

	int32 NewIndex = EmitterData->GetRenderers().Num() - 1;

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("renderer_type"), NormalizedType);
	Result->SetNumberField(TEXT("renderer_index"), NewIndex);
	Result->SetStringField(TEXT("emitter_name"), EmitterName);
	Result->SetNumberField(TEXT("renderer_count"), EmitterData->GetRenderers().Num());
	return Result;
#else
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only operation"));
#endif
}

// ---------------------------------------------------------------------------
// HandleRemoveNiagaraRenderer
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleRemoveNiagaraRenderer(
	const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
	FString SystemPath;
	if (!Params->TryGetStringField(TEXT("system_path"), SystemPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'system_path' parameter"));
	}

	FString EmitterName;
	if (!Params->TryGetStringField(TEXT("emitter_name"), EmitterName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'emitter_name' parameter"));
	}

	int32 RendererIndex = 0;
	Params->TryGetNumberField(TEXT("renderer_index"), RendererIndex);

	FString Error;
	UNiagaraSystem* System = NiagaraHelpers::LoadNiagaraSystem(SystemPath, Error);
	if (!System)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	int32 EmitterIdx = INDEX_NONE;
	FNiagaraEmitterHandle* Handle = NiagaraHelpers::FindEmitterHandle(System, EmitterName, EmitterIdx, Error);
	if (!Handle)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	FVersionedNiagaraEmitterData* EmitterData = NiagaraHelpers::GetEmitterData(Handle);
	if (!EmitterData)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Emitter has no data"));
	}

	UNiagaraEmitter* Emitter = Handle->GetInstance().Emitter;
	if (!Emitter)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get emitter instance"));
	}

	const TArray<UNiagaraRendererProperties*>& Renderers = EmitterData->GetRenderers();
	if (RendererIndex < 0 || RendererIndex >= Renderers.Num())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Renderer index %d out of range (emitter has %d renderers)"),
				RendererIndex, Renderers.Num()));
	}

	UNiagaraRendererProperties* ToRemove = Renderers[RendererIndex];
	FString RemovedName = ToRemove ? ToRemove->GetName() : TEXT("unknown");

	Emitter->RemoveRenderer(ToRemove, EmitterData->Version.VersionGuid);
	NiagaraHelpers::CompileAndSync(System);

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("removed_renderer"), RemovedName);
	Result->SetNumberField(TEXT("removed_index"), RendererIndex);
	Result->SetNumberField(TEXT("remaining_count"), EmitterData->GetRenderers().Num());
	return Result;
#else
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only operation"));
#endif
}

// ---------------------------------------------------------------------------
// HandleGetNiagaraRendererInfo
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleGetNiagaraRendererInfo(
	const TSharedPtr<FJsonObject>& Params)
{
	FString SystemPath;
	if (!Params->TryGetStringField(TEXT("system_path"), SystemPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'system_path' parameter"));
	}

	FString EmitterName;
	if (!Params->TryGetStringField(TEXT("emitter_name"), EmitterName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'emitter_name' parameter"));
	}

	FString Error;
	UNiagaraSystem* System = NiagaraHelpers::LoadNiagaraSystem(SystemPath, Error);
	if (!System)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	int32 EmitterIdx = INDEX_NONE;
	FNiagaraEmitterHandle* Handle = NiagaraHelpers::FindEmitterHandle(System, EmitterName, EmitterIdx, Error);
	if (!Handle)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	FVersionedNiagaraEmitterData* EmitterData = NiagaraHelpers::GetEmitterData(Handle);
	if (!EmitterData)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Emitter has no data"));
	}

	// Optional: query specific renderer index
	int32 RendererIndex = -1;
	Params->TryGetNumberField(TEXT("renderer_index"), RendererIndex);

	const TArray<UNiagaraRendererProperties*>& Renderers = EmitterData->GetRenderers();

	TArray<TSharedPtr<FJsonValue>> RenderersArr;
	for (int32 i = 0; i < Renderers.Num(); ++i)
	{
		if (RendererIndex >= 0 && i != RendererIndex)
		{
			continue;
		}
		RenderersArr.Add(MakeShared<FJsonValueObject>(RendererToJson(Renderers[i], i)));
	}

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("emitter_name"), EmitterName);
	Result->SetArrayField(TEXT("renderers"), RenderersArr);
	Result->SetNumberField(TEXT("count"), RenderersArr.Num());
	return Result;
}

// ---------------------------------------------------------------------------
// HandleSetNiagaraRendererProperty
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleSetNiagaraRendererProperty(
	const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
	FString SystemPath;
	if (!Params->TryGetStringField(TEXT("system_path"), SystemPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'system_path' parameter"));
	}

	FString EmitterName;
	if (!Params->TryGetStringField(TEXT("emitter_name"), EmitterName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'emitter_name' parameter"));
	}

	FString PropertyName;
	if (!Params->TryGetStringField(TEXT("property"), PropertyName) &&
		!Params->TryGetStringField(TEXT("property_name"), PropertyName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'property' parameter"));
	}

	int32 RendererIndex = 0;
	Params->TryGetNumberField(TEXT("renderer_index"), RendererIndex);

	FString Error;
	UNiagaraSystem* System = NiagaraHelpers::LoadNiagaraSystem(SystemPath, Error);
	if (!System)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	int32 EmitterIdx = INDEX_NONE;
	FNiagaraEmitterHandle* Handle = NiagaraHelpers::FindEmitterHandle(System, EmitterName, EmitterIdx, Error);
	if (!Handle)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	FVersionedNiagaraEmitterData* EmitterData = NiagaraHelpers::GetEmitterData(Handle);
	if (!EmitterData)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Emitter has no data"));
	}

	const TArray<UNiagaraRendererProperties*>& Renderers = EmitterData->GetRenderers();
	if (RendererIndex < 0 || RendererIndex >= Renderers.Num())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Renderer index %d out of range (%d renderers)"),
				RendererIndex, Renderers.Num()));
	}

	UNiagaraRendererProperties* Renderer = Renderers[RendererIndex];
	FString LowerProp = PropertyName.ToLower();

	// Handle "material" property specially for sprite/ribbon/mesh
	if (LowerProp == TEXT("material") || LowerProp == TEXT("material_path"))
	{
		FString MatPath;
		if (!Params->TryGetStringField(TEXT("value"), MatPath))
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'value' (material path) for material property"));
		}

		UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *MatPath);
		if (!Material)
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Failed to load material: %s"), *MatPath));
		}

		if (UNiagaraSpriteRendererProperties* Sprite = Cast<UNiagaraSpriteRendererProperties>(Renderer))
		{
			Sprite->Material = Material;
		}
		else if (UNiagaraRibbonRendererProperties* Ribbon = Cast<UNiagaraRibbonRendererProperties>(Renderer))
		{
			Ribbon->Material = Material;
		}
		else if (UNiagaraMeshRendererProperties* Mesh = Cast<UNiagaraMeshRendererProperties>(Renderer))
		{
			FNiagaraMeshMaterialOverride Override;
			Override.ExplicitMat = Material;
			if (Mesh->OverrideMaterials.Num() > 0)
			{
				Mesh->OverrideMaterials[0] = Override;
			}
			else
			{
				Mesh->OverrideMaterials.Add(Override);
			}
		}
		else
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("This renderer type does not support materials"));
		}

		NiagaraHelpers::CompileAndSync(System);
		auto Result = MakeShared<FJsonObject>();
		Result->SetBoolField(TEXT("success"), true);
		Result->SetStringField(TEXT("property"), TEXT("material"));
		Result->SetStringField(TEXT("value"), MatPath);
		return Result;
	}

	// Handle "mesh" or "mesh_path" for mesh renderer
	if (LowerProp == TEXT("mesh") || LowerProp == TEXT("mesh_path"))
	{
		UNiagaraMeshRendererProperties* Mesh = Cast<UNiagaraMeshRendererProperties>(Renderer);
		if (!Mesh)
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("'mesh' property only applies to mesh renderers"));
		}

		FString MeshAssetPath;
		if (!Params->TryGetStringField(TEXT("value"), MeshAssetPath))
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'value' (mesh asset path)"));
		}

		UStaticMesh* StaticMesh = LoadObject<UStaticMesh>(nullptr, *MeshAssetPath);
		if (!StaticMesh)
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Failed to load mesh: %s"), *MeshAssetPath));
		}

		if (Mesh->Meshes.Num() > 0)
		{
			Mesh->Meshes[0].Mesh = StaticMesh;
		}
		else
		{
			FNiagaraMeshRendererMeshProperties MeshProps;
			MeshProps.Mesh = StaticMesh;
			Mesh->Meshes.Add(MeshProps);
		}

		NiagaraHelpers::CompileAndSync(System);
		auto Result = MakeShared<FJsonObject>();
		Result->SetBoolField(TEXT("success"), true);
		Result->SetStringField(TEXT("property"), TEXT("mesh"));
		Result->SetStringField(TEXT("value"), MeshAssetPath);
		return Result;
	}

	// Handle "sort_order"
	if (LowerProp == TEXT("sort_order") || LowerProp == TEXT("sort_order_hint"))
	{
		int32 SortOrder = 0;
		if (!Params->TryGetNumberField(TEXT("value"), SortOrder))
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'value' (integer) for sort_order"));
		}

		Renderer->SortOrderHint = SortOrder;
		NiagaraHelpers::CompileAndSync(System);

		auto Result = MakeShared<FJsonObject>();
		Result->SetBoolField(TEXT("success"), true);
		Result->SetStringField(TEXT("property"), TEXT("sort_order"));
		Result->SetNumberField(TEXT("value"), SortOrder);
		return Result;
	}

	// Handle "enabled"
	if (LowerProp == TEXT("enabled") || LowerProp == TEXT("is_enabled"))
	{
		bool bEnabled = true;
		Params->TryGetBoolField(TEXT("value"), bEnabled);
		Renderer->SetIsEnabled(bEnabled);
		NiagaraHelpers::CompileAndSync(System);

		auto Result = MakeShared<FJsonObject>();
		Result->SetBoolField(TEXT("success"), true);
		Result->SetStringField(TEXT("property"), TEXT("enabled"));
		Result->SetBoolField(TEXT("value"), bEnabled);
		return Result;
	}

	// Handle "facing_mode" for sprite renderer
	if (LowerProp == TEXT("facing_mode"))
	{
		UNiagaraSpriteRendererProperties* Sprite = Cast<UNiagaraSpriteRendererProperties>(Renderer);
		if (!Sprite)
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("'facing_mode' only applies to sprite renderers"));
		}

		FString ValueStr;
		if (!Params->TryGetStringField(TEXT("value"), ValueStr))
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'value' for facing_mode"));
		}

		int64 EnumVal = 0;
		UEnum* FacingEnum = StaticEnum<ENiagaraSpriteFacingMode>();
		if (FacingEnum)
		{
			EnumVal = FacingEnum->GetValueByNameString(ValueStr);
			if (EnumVal == INDEX_NONE)
			{
				// Try case-insensitive
				for (int32 i = 0; i < FacingEnum->NumEnums() - 1; ++i)
				{
					FString EnumName = FacingEnum->GetNameStringByIndex(i);
					if (EnumName.Equals(ValueStr, ESearchCase::IgnoreCase))
					{
						EnumVal = FacingEnum->GetValueByIndex(i);
						break;
					}
				}
			}
		}

		Sprite->FacingMode = static_cast<ENiagaraSpriteFacingMode>(EnumVal);
		NiagaraHelpers::CompileAndSync(System);

		auto Result = MakeShared<FJsonObject>();
		Result->SetBoolField(TEXT("success"), true);
		Result->SetStringField(TEXT("property"), TEXT("facing_mode"));
		Result->SetStringField(TEXT("value"), FacingEnum ? FacingEnum->GetNameStringByValue(EnumVal) : ValueStr);
		return Result;
	}

	// Fallback: try generic UPROPERTY reflection
	FString ValueStr;
	if (Params->TryGetStringField(TEXT("value"), ValueStr))
	{
		FProperty* Prop = Renderer->GetClass()->FindPropertyByName(FName(*PropertyName));
		if (Prop)
		{
			Prop->ImportText_Direct(*ValueStr, Prop->ContainerPtrToValuePtr<void>(Renderer), Renderer, PPF_None);
			NiagaraHelpers::CompileAndSync(System);

			auto Result = MakeShared<FJsonObject>();
			Result->SetBoolField(TEXT("success"), true);
			Result->SetStringField(TEXT("property"), PropertyName);
			Result->SetStringField(TEXT("value"), ValueStr);
			return Result;
		}
	}

	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
		FString::Printf(TEXT("Unknown renderer property '%s'. Supported: material, mesh, sort_order, enabled, facing_mode"), *PropertyName));
#else
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only operation"));
#endif
}

// ---------------------------------------------------------------------------
// HandleSetNiagaraRendererBinding
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleSetNiagaraRendererBinding(
	const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
	FString SystemPath;
	if (!Params->TryGetStringField(TEXT("system_path"), SystemPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'system_path' parameter"));
	}

	FString EmitterName;
	if (!Params->TryGetStringField(TEXT("emitter_name"), EmitterName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'emitter_name' parameter"));
	}

	FString BindingName;
	if (!Params->TryGetStringField(TEXT("binding_name"), BindingName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'binding_name' parameter (e.g., 'PositionBinding')"));
	}

	FString AttributeName;
	if (!Params->TryGetStringField(TEXT("attribute_name"), AttributeName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'attribute_name' parameter (e.g., 'Particles.Position')"));
	}

	int32 RendererIndex = 0;
	Params->TryGetNumberField(TEXT("renderer_index"), RendererIndex);

	FString Error;
	UNiagaraSystem* System = NiagaraHelpers::LoadNiagaraSystem(SystemPath, Error);
	if (!System)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	int32 EmitterIdx = INDEX_NONE;
	FNiagaraEmitterHandle* Handle = NiagaraHelpers::FindEmitterHandle(System, EmitterName, EmitterIdx, Error);
	if (!Handle)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	FVersionedNiagaraEmitterData* EmitterData = NiagaraHelpers::GetEmitterData(Handle);
	if (!EmitterData)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Emitter has no data"));
	}

	const TArray<UNiagaraRendererProperties*>& Renderers = EmitterData->GetRenderers();
	if (RendererIndex < 0 || RendererIndex >= Renderers.Num())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Renderer index %d out of range (%d renderers)"),
				RendererIndex, Renderers.Num()));
	}

	UNiagaraRendererProperties* Renderer = Renderers[RendererIndex];

	// Find the binding property by name via reflection
	FStructProperty* BindingProp = CastField<FStructProperty>(
		Renderer->GetClass()->FindPropertyByName(FName(*BindingName)));

	if (!BindingProp || BindingProp->Struct->GetName() != TEXT("NiagaraVariableAttributeBinding"))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("'%s' is not a valid binding property on this renderer"), *BindingName));
	}

	FNiagaraVariableAttributeBinding* Binding = BindingProp->ContainerPtrToValuePtr<FNiagaraVariableAttributeBinding>(Renderer);
	if (!Binding)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to access binding property"));
	}

	// Parse the attribute namespace and name
	FNiagaraVariableBase BoundVar;
	FString Namespace;
	FString VarName;
	if (AttributeName.Split(TEXT("."), &Namespace, &VarName))
	{
		// Full qualified: "Particles.Position" or "Emitter.Color"
		BoundVar.SetName(FName(*AttributeName));
	}
	else
	{
		// Assume particle namespace
		BoundVar.SetName(FName(*FString::Printf(TEXT("Particles.%s"), *AttributeName)));
	}

	// SetValue takes FName, FVersionedNiagaraEmitterBase, ENiagaraRendererSourceDataMode
	FVersionedNiagaraEmitterBase VersionedEmitterBase;
	VersionedEmitterBase.Emitter = Handle->GetInstance().Emitter;
	VersionedEmitterBase.Version = EmitterData->Version.VersionGuid;
	Binding->SetValue(
		BoundVar.GetName(),
		VersionedEmitterBase,
		ENiagaraRendererSourceDataMode::Particles);

	NiagaraHelpers::CompileAndSync(System);

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("binding_name"), BindingName);
	Result->SetStringField(TEXT("attribute_name"), BoundVar.GetName().ToString());
	Result->SetNumberField(TEXT("renderer_index"), RendererIndex);
	return Result;
#else
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only operation"));
#endif
}
