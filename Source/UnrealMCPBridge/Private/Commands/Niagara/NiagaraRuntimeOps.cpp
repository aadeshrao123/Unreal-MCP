#include "Commands/EpicUnrealMCPNiagaraCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "NiagaraHelpers.h"

#include "NiagaraSystem.h"
#include "NiagaraActor.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/SceneComponent.h"

// ---------------------------------------------------------------------------
// Helper: Get editor world
// ---------------------------------------------------------------------------

static UWorld* GetEditorWorld()
{
	if (GEditor)
	{
		return GEditor->GetEditorWorldContext().World();
	}
	return nullptr;
}

// ---------------------------------------------------------------------------
// Helper: Find any actor in the level by label or internal name
// ---------------------------------------------------------------------------

static AActor* FindActorByLabel(UWorld* World, const FString& Name)
{
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor->GetActorLabel().Equals(Name, ESearchCase::IgnoreCase) ||
			Actor->GetName().Equals(Name, ESearchCase::IgnoreCase))
		{
			return Actor;
		}
	}
	return nullptr;
}

// ---------------------------------------------------------------------------
// Helper: Serialize transform to JSON
// ---------------------------------------------------------------------------

static TSharedPtr<FJsonObject> VectorToJsonObj(const FVector& V)
{
	auto Obj = MakeShared<FJsonObject>();
	Obj->SetNumberField(TEXT("x"), V.X);
	Obj->SetNumberField(TEXT("y"), V.Y);
	Obj->SetNumberField(TEXT("z"), V.Z);
	return Obj;
}

static TSharedPtr<FJsonObject> RotatorToJsonObj(const FRotator& R)
{
	auto Obj = MakeShared<FJsonObject>();
	Obj->SetNumberField(TEXT("pitch"), R.Pitch);
	Obj->SetNumberField(TEXT("yaw"), R.Yaw);
	Obj->SetNumberField(TEXT("roll"), R.Roll);
	return Obj;
}

// ---------------------------------------------------------------------------
// HandleSpawnNiagaraEffect
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleSpawnNiagaraEffect(
	const TSharedPtr<FJsonObject>& Params)
{
	FString SystemPath;
	if (!Params->TryGetStringField(TEXT("system_path"), SystemPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'system_path' parameter"));
	}

	FString Error;
	UNiagaraSystem* NiagaraSystem = NiagaraHelpers::LoadNiagaraSystem(SystemPath, Error);
	if (!NiagaraSystem)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor world available"));
	}

	// Parse transform
	FVector Location = FEpicUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location"));
	FRotator Rotation = FEpicUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"));
	FVector Scale = FVector::OneVector;
	if (Params->HasField(TEXT("scale")))
	{
		Scale = FEpicUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("scale"));
	}

	bool bAutoActivate = true;
	Params->TryGetBoolField(TEXT("auto_activate"), bAutoActivate);

	// Spawn the actor
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ANiagaraActor* NiagaraActor = World->SpawnActor<ANiagaraActor>(Location, Rotation, SpawnParams);
	if (!NiagaraActor)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to spawn Niagara actor"));
	}

	// Configure the component
	UNiagaraComponent* NiagaraComp = NiagaraActor->GetNiagaraComponent();
	if (NiagaraComp)
	{
		NiagaraComp->SetAsset(NiagaraSystem);
		NiagaraComp->SetAutoActivate(bAutoActivate);
		NiagaraActor->SetActorScale3D(Scale);

		if (bAutoActivate)
		{
			NiagaraComp->Activate(true);
		}
	}

	// Set display name
	FString ActorName;
	if (Params->TryGetStringField(TEXT("name"), ActorName))
	{
		NiagaraActor->SetActorLabel(*ActorName);
	}

	// Set folder
	FString FolderPath;
	if (Params->TryGetStringField(TEXT("folder"), FolderPath))
	{
		NiagaraActor->SetFolderPath(FName(*FolderPath));
	}

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("actor_name"), NiagaraActor->GetActorLabel());
	Result->SetStringField(TEXT("internal_name"), NiagaraActor->GetName());
	Result->SetStringField(TEXT("system_path"), SystemPath);
	Result->SetObjectField(TEXT("location"), VectorToJsonObj(NiagaraActor->GetActorLocation()));
	Result->SetObjectField(TEXT("rotation"), RotatorToJsonObj(NiagaraActor->GetActorRotation()));
	Result->SetObjectField(TEXT("scale"), VectorToJsonObj(NiagaraActor->GetActorScale3D()));
	Result->SetBoolField(TEXT("auto_activate"), bAutoActivate);
	return Result;
}

// ---------------------------------------------------------------------------
// HandleControlNiagaraEffect
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleControlNiagaraEffect(
	const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName;
	if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
	}

	FString Action;
	if (!Params->TryGetStringField(TEXT("action"), Action))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'action' parameter"));
	}

	FString Error;
	ANiagaraActor* NiagaraActor = NiagaraHelpers::FindNiagaraActorByName(ActorName, Error);
	if (!NiagaraActor)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	UNiagaraComponent* NiagaraComp = NiagaraActor->GetNiagaraComponent();
	if (!NiagaraComp)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Actor has no Niagara component"));
	}

	FString LowerAction = Action.ToLower();

	if (LowerAction == TEXT("activate"))
	{
		NiagaraComp->Activate(true);
	}
	else if (LowerAction == TEXT("deactivate"))
	{
		NiagaraComp->Deactivate();
	}
	else if (LowerAction == TEXT("reset"))
	{
		NiagaraComp->ResetSystem();
	}
	else if (LowerAction == TEXT("reinitialize") || LowerAction == TEXT("reset_system"))
	{
		NiagaraComp->ReinitializeSystem();
	}
	else
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Unknown action '%s'. Use: activate, deactivate, reset, reinitialize"), *Action));
	}

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("actor_name"), ActorName);
	Result->SetStringField(TEXT("action"), Action);
	Result->SetBoolField(TEXT("is_active"), NiagaraComp->IsActive());
	return Result;
}

// ---------------------------------------------------------------------------
// HandleAddNiagaraComponent
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleAddNiagaraComponent(
	const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName;
	if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
	}

	FString SystemPath;
	if (!Params->TryGetStringField(TEXT("system_path"), SystemPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'system_path' parameter"));
	}

	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor world available"));
	}

	AActor* Actor = FindActorByLabel(World, ActorName);
	if (!Actor)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Actor '%s' not found in level"), *ActorName));
	}

	FString Error;
	UNiagaraSystem* NiagaraSystem = NiagaraHelpers::LoadNiagaraSystem(SystemPath, Error);
	if (!NiagaraSystem)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	// Create the component
	FString ComponentName = TEXT("NiagaraComponent");
	Params->TryGetStringField(TEXT("component_name"), ComponentName);

	UNiagaraComponent* NiagaraComp = NewObject<UNiagaraComponent>(Actor, FName(*ComponentName));
	if (!NiagaraComp)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create Niagara component"));
	}

	NiagaraComp->SetAsset(NiagaraSystem);

	// Relative transform
	if (Params->HasField(TEXT("relative_location")))
	{
		FVector RelLoc = FEpicUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("relative_location"));
		NiagaraComp->SetRelativeLocation(RelLoc);
	}

	if (Params->HasField(TEXT("relative_rotation")))
	{
		FRotator RelRot = FEpicUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("relative_rotation"));
		NiagaraComp->SetRelativeRotation(RelRot);
	}

	bool bAutoActivate = true;
	Params->TryGetBoolField(TEXT("auto_activate"), bAutoActivate);
	NiagaraComp->SetAutoActivate(bAutoActivate);

	// Attach to root
	USceneComponent* Root = Actor->GetRootComponent();
	if (Root)
	{
		NiagaraComp->AttachToComponent(Root, FAttachmentTransformRules::KeepRelativeTransform);
	}

	NiagaraComp->RegisterComponent();

	if (bAutoActivate)
	{
		NiagaraComp->Activate(true);
	}

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("actor_name"), ActorName);
	Result->SetStringField(TEXT("component_name"), ComponentName);
	Result->SetStringField(TEXT("system_path"), SystemPath);
	Result->SetBoolField(TEXT("auto_activate"), bAutoActivate);
	return Result;
}

// ---------------------------------------------------------------------------
// HandleGetNiagaraActors
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleGetNiagaraActors(
	const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor world available"));
	}

	FString NameFilter;
	Params->TryGetStringField(TEXT("name_filter"), NameFilter);

	FString SystemFilter;
	Params->TryGetStringField(TEXT("system_filter"), SystemFilter);

	TArray<TSharedPtr<FJsonValue>> ActorsArr;

	for (TActorIterator<ANiagaraActor> It(World); It; ++It)
	{
		ANiagaraActor* Actor = *It;

		// Apply name filter
		if (!NameFilter.IsEmpty())
		{
			FString Label = Actor->GetActorLabel();
			FString InternalName = Actor->GetName();
			if (!Label.Contains(NameFilter, ESearchCase::IgnoreCase) &&
				!InternalName.Contains(NameFilter, ESearchCase::IgnoreCase))
			{
				continue;
			}
		}

		UNiagaraComponent* NiagaraComp = Actor->GetNiagaraComponent();
		FString SystemName;
		FString SystemAssetPath;

		if (NiagaraComp && NiagaraComp->GetAsset())
		{
			SystemName = NiagaraComp->GetAsset()->GetName();
			SystemAssetPath = NiagaraComp->GetAsset()->GetPathName();

			// Apply system filter
			if (!SystemFilter.IsEmpty() &&
				!SystemName.Contains(SystemFilter, ESearchCase::IgnoreCase))
			{
				continue;
			}
		}
		else if (!SystemFilter.IsEmpty())
		{
			continue;
		}

		auto ActorObj = MakeShared<FJsonObject>();
		ActorObj->SetStringField(TEXT("name"), Actor->GetActorLabel());
		ActorObj->SetStringField(TEXT("internal_name"), Actor->GetName());
		ActorObj->SetStringField(TEXT("system_name"), SystemName);
		ActorObj->SetStringField(TEXT("system_path"), SystemAssetPath);
		ActorObj->SetObjectField(TEXT("location"), VectorToJsonObj(Actor->GetActorLocation()));
		ActorObj->SetObjectField(TEXT("rotation"), RotatorToJsonObj(Actor->GetActorRotation()));
		ActorObj->SetObjectField(TEXT("scale"), VectorToJsonObj(Actor->GetActorScale3D()));

		if (NiagaraComp)
		{
			ActorObj->SetBoolField(TEXT("is_active"), NiagaraComp->IsActive());
			ActorObj->SetBoolField(TEXT("auto_activate"), NiagaraComp->bAutoActivate);
		}

		FName FolderPath = Actor->GetFolderPath();
		if (!FolderPath.IsNone())
		{
			ActorObj->SetStringField(TEXT("folder"), FolderPath.ToString());
		}

		ActorsArr.Add(MakeShared<FJsonValueObject>(ActorObj));
	}

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetArrayField(TEXT("actors"), ActorsArr);
	Result->SetNumberField(TEXT("count"), ActorsArr.Num());
	return Result;
}
