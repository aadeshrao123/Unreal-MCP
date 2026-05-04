#include "Commands/EpicUnrealMCPEditorCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "Commands/EpicUnrealMCPBlueprintCommands.h"
#include "Commands/EpicUnrealMCPPropertyUtils.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "UObject/UObjectIterator.h"
#include "JsonObjectConverter.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "LevelEditorViewport.h"
#include "Misc/FileHelper.h"
#include "LevelEditor.h"
#include "IAssetViewport.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Framework/Application/SlateApplication.h"


#include "GameFramework/Actor.h"
#include "Engine/Selection.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Camera/CameraActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "EditorAssetLibrary.h"
#include "EngineUtils.h"

FEpicUnrealMCPEditorCommands::FEpicUnrealMCPEditorCommands()
{
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleCommand(
	const FString& CommandType,
	const TSharedPtr<FJsonObject>& Params)
{
	if (CommandType == TEXT("get_actors_in_level"))
	{
		return HandleGetActorsInLevel(Params);
	}
	else if (CommandType == TEXT("find_actors_by_name"))
	{
		return HandleFindActorsByName(Params);
	}
	else if (CommandType == TEXT("spawn_actor"))
	{
		return HandleSpawnActor(Params);
	}
	else if (CommandType == TEXT("delete_actor"))
	{
		return HandleDeleteActor(Params);
	}
	else if (CommandType == TEXT("set_actor_transform"))
	{
		return HandleSetActorTransform(Params);
	}
	else if (CommandType == TEXT("spawn_blueprint_actor"))
	{
		return HandleSpawnBlueprintActor(Params);
	}
	else if (CommandType == TEXT("get_selected_actors"))
	{
		return HandleGetSelectedActors(Params);
	}
	else if (CommandType == TEXT("get_world_info"))
	{
		return HandleGetWorldInfo(Params);
	}
	else if (CommandType == TEXT("spawn_actor_from_class"))
	{
		return HandleSpawnActorFromClass(Params);
	}
	else if (CommandType == TEXT("get_actor_properties"))
	{
		return HandleGetActorProperties(Params);
	}
	else if (CommandType == TEXT("set_actor_property"))
	{
		return HandleSetActorProperty(Params);
	}
	else if (CommandType == TEXT("get_actor_property_metadata"))
	{
		return HandleGetActorPropertyMetadata(Params);
	}
	else if (CommandType == TEXT("spawn_actor_by_class"))
	{
		return HandleSpawnActorByClass(Params);
	}
	else if (CommandType == TEXT("find_actors"))
	{
		return HandleFindActors(Params);
	}
	else if (CommandType == TEXT("take_screenshot"))
	{
		return HandleTakeScreenshot(Params);
	}

	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
		FString::Printf(TEXT("Unknown editor command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleGetActorsInLevel(
	const TSharedPtr<FJsonObject>& Params)
{
	TArray<AActor*> AllActors;
	UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);

	TArray<TSharedPtr<FJsonValue>> ActorArray;
	for (AActor* Actor : AllActors)
	{
		if (Actor)
		{
			ActorArray.Add(FEpicUnrealMCPCommonUtils::ActorToJson(Actor));
		}
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetArrayField(TEXT("actors"), ActorArray);
	return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleFindActorsByName(
	const TSharedPtr<FJsonObject>& Params)
{
	FString Pattern;
	if (!Params->TryGetStringField(TEXT("pattern"), Pattern))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'pattern' parameter"));
	}

	TArray<AActor*> AllActors;
	UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);

	TArray<TSharedPtr<FJsonValue>> MatchingActors;
	for (AActor* Actor : AllActors)
	{
		if (Actor && Actor->GetName().Contains(Pattern))
		{
			MatchingActors.Add(FEpicUnrealMCPCommonUtils::ActorToJson(Actor));
		}
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetArrayField(TEXT("actors"), MatchingActors);
	return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleSpawnActor(
	const TSharedPtr<FJsonObject>& Params)
{
	FString ActorType;
	if (!Params->TryGetStringField(TEXT("type"), ActorType))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'type' parameter"));
	}

	FString ActorName;
	if (!Params->TryGetStringField(TEXT("name"), ActorName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
	}

	FVector Location(0.0f, 0.0f, 0.0f);
	FRotator Rotation(0.0f, 0.0f, 0.0f);
	FVector Scale(1.0f, 1.0f, 1.0f);

	if (Params->HasField(TEXT("location")))
	{
		Location = FEpicUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location"));
	}
	if (Params->HasField(TEXT("rotation")))
	{
		Rotation = FEpicUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"));
	}
	if (Params->HasField(TEXT("scale")))
	{
		Scale = FEpicUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("scale"));
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
	}

	TArray<AActor*> AllActors;
	UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);
	for (AActor* Actor : AllActors)
	{
		if (Actor && Actor->GetName() == ActorName)
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Actor with name '%s' already exists"), *ActorName));
		}
	}

	AActor* NewActor = nullptr;
	FActorSpawnParameters SpawnParams;
	SpawnParams.Name = *ActorName;

	if (ActorType == TEXT("StaticMeshActor"))
	{
		AStaticMeshActor* NewMeshActor = World->SpawnActor<AStaticMeshActor>(
			AStaticMeshActor::StaticClass(), Location, Rotation, SpawnParams);
		if (NewMeshActor)
		{
			FString MeshPath;
			if (Params->TryGetStringField(TEXT("static_mesh"), MeshPath))
			{
				UStaticMesh* Mesh = Cast<UStaticMesh>(UEditorAssetLibrary::LoadAsset(MeshPath));
				if (Mesh)
				{
					NewMeshActor->GetStaticMeshComponent()->SetStaticMesh(Mesh);
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("Could not find static mesh at path: %s"), *MeshPath);
				}
			}
		}
		NewActor = NewMeshActor;
	}
	else if (ActorType == TEXT("PointLight"))
	{
		NewActor = World->SpawnActor<APointLight>(
			APointLight::StaticClass(), Location, Rotation, SpawnParams);
	}
	else if (ActorType == TEXT("SpotLight"))
	{
		NewActor = World->SpawnActor<ASpotLight>(
			ASpotLight::StaticClass(), Location, Rotation, SpawnParams);
	}
	else if (ActorType == TEXT("DirectionalLight"))
	{
		NewActor = World->SpawnActor<ADirectionalLight>(
			ADirectionalLight::StaticClass(), Location, Rotation, SpawnParams);
	}
	else if (ActorType == TEXT("CameraActor"))
	{
		NewActor = World->SpawnActor<ACameraActor>(
			ACameraActor::StaticClass(), Location, Rotation, SpawnParams);
	}
	else
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Unknown actor type: %s"), *ActorType));
	}

	if (NewActor)
	{
		// SpawnActor only takes location and rotation, apply scale separately
		FTransform Transform = NewActor->GetTransform();
		Transform.SetScale3D(Scale);
		NewActor->SetActorTransform(Transform);

		return FEpicUnrealMCPCommonUtils::ActorToJsonObject(NewActor, true);
	}

	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create actor"));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleDeleteActor(
	const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName;
	if (!Params->TryGetStringField(TEXT("name"), ActorName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : GWorld;
	if (!World)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
	}

	AActor* Target = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (A && (A->GetActorLabel() == ActorName || A->GetName() == ActorName))
		{
			Target = A;
			break;
		}
	}

	if (!Target)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Actor not found: %s"), *ActorName));
	}

	TSharedPtr<FJsonObject> ActorInfo = FEpicUnrealMCPCommonUtils::ActorToJsonObject(Target);

	// Modify() registers with the transaction system so Ctrl+Z restores the actor.
	Target->Modify();
	if (ULevel* Level = Target->GetLevel())
	{
		Level->Modify();
	}

	if (UEditorActorSubsystem* EAS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>())
	{
		EAS->DestroyActor(Target);
	}
	else
	{
		World->EditorDestroyActor(Target, true);
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetObjectField(TEXT("deleted_actor"), ActorInfo);
	return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleSetActorTransform(
	const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName;
	if (!Params->TryGetStringField(TEXT("name"), ActorName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
	}

	AActor* TargetActor = nullptr;
	TArray<AActor*> AllActors;
	UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);

	for (AActor* Actor : AllActors)
	{
		if (Actor && Actor->GetName() == ActorName)
		{
			TargetActor = Actor;
			break;
		}
	}

	if (!TargetActor)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Actor not found: %s"), *ActorName));
	}

	FTransform NewTransform = TargetActor->GetTransform();

	if (Params->HasField(TEXT("location")))
	{
		NewTransform.SetLocation(
			FEpicUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location")));
	}
	if (Params->HasField(TEXT("rotation")))
	{
		NewTransform.SetRotation(
			FQuat(FEpicUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"))));
	}
	if (Params->HasField(TEXT("scale")))
	{
		NewTransform.SetScale3D(
			FEpicUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("scale")));
	}

	TargetActor->SetActorTransform(NewTransform);
	return FEpicUnrealMCPCommonUtils::ActorToJsonObject(TargetActor, true);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleSpawnBlueprintActor(
	const TSharedPtr<FJsonObject>& Params)
{
	FEpicUnrealMCPBlueprintCommands BlueprintCommands;
	return BlueprintCommands.HandleCommand(TEXT("spawn_blueprint_actor"), Params);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleGetSelectedActors(
	const TSharedPtr<FJsonObject>& Params)
{
	TArray<TSharedPtr<FJsonValue>> ActorArray;

	USelection* Selection = GEditor->GetSelectedActors();
	for (FSelectionIterator It(*Selection); It; ++It)
	{
		AActor* Actor = Cast<AActor>(*It);
		if (!Actor)
		{
			continue;
		}

		TSharedPtr<FJsonObject> ActorObj = MakeShared<FJsonObject>();
		ActorObj->SetStringField(TEXT("name"), Actor->GetName());
		ActorObj->SetStringField(TEXT("label"), Actor->GetActorLabel());
		ActorObj->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
		const FVector Loc = Actor->GetActorLocation();
		ActorObj->SetStringField(TEXT("location"),
			FString::Printf(TEXT("(%.1f, %.1f, %.1f)"), Loc.X, Loc.Y, Loc.Z));
		ActorArray.Add(MakeShared<FJsonValueObject>(ActorObj));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetArrayField(TEXT("actors"), ActorArray);
	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleGetWorldInfo(
	const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GEditor->GetEditorWorldContext().World();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("world"), World ? World->GetName() : TEXT("None"));

	if (World)
	{
		TArray<AActor*> AllActors;
		UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);
		Result->SetNumberField(TEXT("actor_count"), AllActors.Num());

		const int32 ShowCount = FMath::Min(AllActors.Num(), 100);
		TArray<TSharedPtr<FJsonValue>> ActorArray;
		for (int32 i = 0; i < ShowCount; i++)
		{
			AActor* Actor = AllActors[i];
			if (!Actor)
			{
				continue;
			}
			TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
			Obj->SetStringField(TEXT("name"), Actor->GetName());
			Obj->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
			ActorArray.Add(MakeShared<FJsonValueObject>(Obj));
		}
		Result->SetArrayField(TEXT("actors"), ActorArray);

		if (AllActors.Num() > 100)
		{
			Result->SetStringField(TEXT("note"),
				FString::Printf(TEXT("Showing 100 of %d actors"), AllActors.Num()));
		}
	}

	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleSpawnActorFromClass(
	const TSharedPtr<FJsonObject>& Params)
{
	FString ClassName;
	if (!Params->TryGetStringField(TEXT("class_name"), ClassName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'class_name' parameter"));
	}

	double LocationX = 0.0, LocationY = 0.0, LocationZ = 0.0;
	double RotYaw = 0.0, RotPitch = 0.0, RotRoll = 0.0;
	Params->TryGetNumberField(TEXT("location_x"), LocationX);
	Params->TryGetNumberField(TEXT("location_y"), LocationY);
	Params->TryGetNumberField(TEXT("location_z"), LocationZ);
	Params->TryGetNumberField(TEXT("rotation_yaw"), RotYaw);
	Params->TryGetNumberField(TEXT("rotation_pitch"), RotPitch);
	Params->TryGetNumberField(TEXT("rotation_roll"), RotRoll);

	const FVector Location(LocationX, LocationY, LocationZ);
	const FRotator Rotation(RotPitch, RotYaw, RotRoll);

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
	}

	FActorSpawnParameters SpawnParams;
	AActor* NewActor = nullptr;

	// Blueprint path (starts with "/")
	if (ClassName.StartsWith(TEXT("/")))
	{
		UObject* Asset = UEditorAssetLibrary::LoadAsset(ClassName);
		UBlueprint* Blueprint = Cast<UBlueprint>(Asset);
		if (Blueprint && Blueprint->GeneratedClass)
		{
			NewActor = World->SpawnActor<AActor>(
				Blueprint->GeneratedClass, Location, Rotation, SpawnParams);
		}
	}

	// Native UClass — try common module paths
	if (!NewActor)
	{
		static const TCHAR* Modules[] =
		{
			TEXT("/Script/Engine."),
			TEXT("/Script/Runtime/Engine/Classes."),
		};
		UClass* Class = nullptr;
		for (const TCHAR* Prefix : Modules)
		{
			Class = FindObject<UClass>(nullptr, *(FString(Prefix) + ClassName));
			if (Class)
			{
				break;
			}
		}
		if (Class && Class->IsChildOf(AActor::StaticClass()))
		{
			NewActor = World->SpawnActor<AActor>(Class, Location, Rotation, SpawnParams);
		}
	}

	if (!NewActor)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Failed to spawn — class not found or invalid: %s"), *ClassName));
	}

	return FEpicUnrealMCPCommonUtils::ActorToJsonObject(NewActor, true);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleGetActorProperties(
	const TSharedPtr<FJsonObject>& Params)
{
	FString ActorLabel;
	if (!Params->TryGetStringField(TEXT("actor_label"), ActorLabel))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_label' parameter"));
	}

	FString Filter;
	Params->TryGetStringField(TEXT("filter"), Filter);
	const FString FilterLower = Filter.ToLower();

	bool bIncludeComponents = false;
	Params->TryGetBoolField(TEXT("include_components"), bIncludeComponents);

	bool bFlat = false;
	Params->TryGetBoolField(TEXT("flat"), bFlat);

	bool bIncludeMetadata = false;
	Params->TryGetBoolField(TEXT("include_metadata"), bIncludeMetadata);

	bool bExpandArrays = true;
	Params->TryGetBoolField(TEXT("expand_arrays"), bExpandArrays);

	int32 MaxDepth = 0;
	double MaxDepthD = 0.0;
	if (Params->TryGetNumberField(TEXT("max_depth"), MaxDepthD)) { MaxDepth = static_cast<int32>(MaxDepthD); }
	else { MaxDepth = 3; }

	int32 ArrayElementLimit = 16;
	double ArrLimD = 0.0;
	if (Params->TryGetNumberField(TEXT("array_element_limit"), ArrLimD)) { ArrayElementLimit = static_cast<int32>(ArrLimD); }

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
	}

	AActor* Target = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (A && (A->GetActorLabel() == ActorLabel || A->GetName() == ActorLabel))
		{
			Target = A;
			break;
		}
	}

	if (!Target)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Actor not found in level: '%s'"), *ActorLabel));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("actor_name"), Target->GetName());
	Result->SetStringField(TEXT("actor_label"), Target->GetActorLabel());
	Result->SetStringField(TEXT("class"), Target->GetClass()->GetPathName());

	if (bFlat)
	{
		// Flat dotted-path mode — path-aware filtering, depth-limited, AI-friendly.
		FString Category;
		Params->TryGetStringField(TEXT("category"), Category);

		bool bIncludeInherited = true;
		Params->TryGetBoolField(TEXT("include_inherited"), bIncludeInherited);

		int32 MaxEntries = 200;
		double MaxEntriesD = 0.0;
		if (Params->TryGetNumberField(TEXT("max_entries"), MaxEntriesD)) { MaxEntries = static_cast<int32>(MaxEntriesD); }

		int32 Cursor = 0;
		double CursorD = 0.0;
		if (Params->TryGetNumberField(TEXT("cursor"), CursorD)) { Cursor = static_cast<int32>(CursorD); }

		FPropertyWalkOptions Opts;
		Opts.FilterLower = FilterLower;
		Opts.CategoryLower = Category.ToLower();
		Opts.MaxDepth = MaxDepth;
		Opts.MaxEntries = MaxEntries;
		Opts.Cursor = Cursor;
		Opts.bIncludeMetadata = bIncludeMetadata;
		Opts.bExpandArrays = bExpandArrays;
		Opts.ArrayElementLimit = ArrayElementLimit;
		Opts.bIncludeInherited = bIncludeInherited;

		Result->SetObjectField(TEXT("properties"),
			FEpicUnrealMCPPropertyUtils::SerializePropertiesFlat(Target, Opts));

		if (bIncludeComponents)
		{
			TArray<UActorComponent*> Components;
			Target->GetComponents(Components);
			TSharedPtr<FJsonObject> CompMap = MakeShared<FJsonObject>();
			for (UActorComponent* Comp : Components)
			{
				if (!Comp) { continue; }
				CompMap->SetObjectField(Comp->GetName(),
					FEpicUnrealMCPPropertyUtils::SerializePropertiesFlat(Comp, Opts));
			}
			Result->SetObjectField(TEXT("components"), CompMap);
		}
	}
	else
	{
		// Nested mode — top-level FProperty names filtered, structs returned as nested JSON.
		Result->SetObjectField(TEXT("properties"),
			FEpicUnrealMCPPropertyUtils::SerializeAllProperties(Target, FilterLower, true));

		if (bIncludeComponents)
		{
			TArray<UActorComponent*> Components;
			Target->GetComponents(Components);
			TSharedPtr<FJsonObject> CompMap = MakeShared<FJsonObject>();
			for (UActorComponent* Comp : Components)
			{
				if (!Comp) { continue; }
				const FString CompKey = Comp->GetName() + TEXT(" (") + Comp->GetClass()->GetName() + TEXT(")");
				CompMap->SetObjectField(CompKey,
					FEpicUnrealMCPPropertyUtils::SerializeAllProperties(Comp, FilterLower, true));
			}
			Result->SetObjectField(TEXT("components"), CompMap);
		}
	}

	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleSetActorProperty(
	const TSharedPtr<FJsonObject>& Params)
{
	FString ActorLabel;
	if (!Params->TryGetStringField(TEXT("actor_label"), ActorLabel))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_label' parameter"));
	}

	FString PropertyPath;
	if (!Params->TryGetStringField(TEXT("property_path"), PropertyPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'property_path' parameter"));
	}

	const TSharedPtr<FJsonValue> Value = Params->TryGetField(TEXT("property_value"));
	if (!Value.IsValid())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'property_value' parameter"));
	}

	FString ComponentName;
	Params->TryGetStringField(TEXT("component_name"), ComponentName);

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
	}

	AActor* TargetActor = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (A && (A->GetActorLabel() == ActorLabel || A->GetName() == ActorLabel))
		{
			TargetActor = A;
			break;
		}
	}

	if (!TargetActor)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Actor not found in level: '%s'"), *ActorLabel));
	}

	UObject* TargetObject = TargetActor;
	UActorComponent* TargetComponent = nullptr;
	if (!ComponentName.IsEmpty())
	{
		TArray<UActorComponent*> Components;
		TargetActor->GetComponents(Components);
		for (UActorComponent* Comp : Components)
		{
			if (Comp && (Comp->GetName() == ComponentName || Comp->GetClass()->GetName() == ComponentName))
			{
				TargetComponent = Comp;
				TargetObject = Comp;
				break;
			}
		}
		if (!TargetComponent)
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Component '%s' not found on actor '%s'"), *ComponentName, *ActorLabel));
		}
	}

	TargetActor->Modify();
	if (TargetComponent) { TargetComponent->Modify(); }

	FProperty* TopLevelProp = nullptr;
	FString WriteError;
	const bool bOk = FEpicUnrealMCPPropertyUtils::SetPropertyAtPath(
		TargetObject, PropertyPath, Value, WriteError, &TopLevelProp);

	if (!bOk)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Failed to set '%s' on '%s': %s"), *PropertyPath, *ActorLabel, *WriteError));
	}

	if (TopLevelProp)
	{
		FPropertyChangedEvent ChangeEvent(TopLevelProp, EPropertyChangeType::ValueSet);
		TargetObject->PostEditChangeProperty(ChangeEvent);
	}

	if (TargetActor->GetLevel())
	{
		TargetActor->GetLevel()->MarkPackageDirty();
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("actor_label"), TargetActor->GetActorLabel());
	Result->SetStringField(TEXT("actor_name"), TargetActor->GetName());
	Result->SetStringField(TEXT("property_path"), PropertyPath);
	if (TargetComponent) { Result->SetStringField(TEXT("component"), TargetComponent->GetName()); }
	if (TopLevelProp) { Result->SetStringField(TEXT("top_level_property"), TopLevelProp->GetName()); }
	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleGetActorPropertyMetadata(
	const TSharedPtr<FJsonObject>& Params)
{
	FString ActorLabel;
	if (!Params->TryGetStringField(TEXT("actor_label"), ActorLabel))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_label' parameter"));
	}

	FString PropertyPath;
	Params->TryGetStringField(TEXT("property_path"), PropertyPath);

	FString Filter;
	Params->TryGetStringField(TEXT("filter"), Filter);
	const FString FilterLower = Filter.ToLower();

	FString Category;
	Params->TryGetStringField(TEXT("category"), Category);

	int32 MaxDepth = 1;
	double DepthD = 0.0;
	if (Params->TryGetNumberField(TEXT("depth"), DepthD)) { MaxDepth = static_cast<int32>(DepthD); }

	bool bExpandEnums = true;
	Params->TryGetBoolField(TEXT("expand_enums"), bExpandEnums);

	bool bIncludeInherited = true;
	Params->TryGetBoolField(TEXT("include_inherited"), bIncludeInherited);

	bool bDescendIntoObjects = false;
	Params->TryGetBoolField(TEXT("descend_into_objects"), bDescendIntoObjects);

	int32 MaxEntries = 50;
	double MaxEntriesD = 0.0;
	if (Params->TryGetNumberField(TEXT("max_entries"), MaxEntriesD)) { MaxEntries = static_cast<int32>(MaxEntriesD); }

	int32 Cursor = 0;
	double CursorD = 0.0;
	if (Params->TryGetNumberField(TEXT("cursor"), CursorD)) { Cursor = static_cast<int32>(CursorD); }

	FString ComponentName;
	Params->TryGetStringField(TEXT("component_name"), ComponentName);

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
	}

	AActor* Target = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (A && (A->GetActorLabel() == ActorLabel || A->GetName() == ActorLabel))
		{
			Target = A;
			break;
		}
	}
	if (!Target)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Actor not found: '%s'"), *ActorLabel));
	}

	UObject* TargetObject = Target;
	if (!ComponentName.IsEmpty())
	{
		TArray<UActorComponent*> Components;
		Target->GetComponents(Components);
		UActorComponent* Found = nullptr;
		for (UActorComponent* C : Components)
		{
			if (C && (C->GetName() == ComponentName || C->GetClass()->GetName() == ComponentName))
			{
				Found = C;
				break;
			}
		}
		if (!Found)
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Component '%s' not found"), *ComponentName));
		}
		TargetObject = Found;
	}

	FPropertyWalkOptions Opts;
	Opts.FilterLower = FilterLower;
	Opts.CategoryLower = Category.ToLower();
	Opts.MaxDepth = MaxDepth;
	Opts.MaxEntries = MaxEntries;
	Opts.Cursor = Cursor;
	Opts.bExpandEnum = bExpandEnums;
	Opts.bIncludeInherited = bIncludeInherited;
	Opts.bDescendIntoObjects = bDescendIntoObjects;
	Opts.bIncludeMetadata = true;

	TSharedPtr<FJsonObject> Tree = FEpicUnrealMCPPropertyUtils::GetPropertyMetadataTree(
		TargetObject, PropertyPath, Opts);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("actor_name"), Target->GetName());
	Result->SetStringField(TEXT("actor_label"), Target->GetActorLabel());
	Result->SetStringField(TEXT("class"), Target->GetClass()->GetPathName());
	if (!ComponentName.IsEmpty()) { Result->SetStringField(TEXT("component"), ComponentName); }
	if (!PropertyPath.IsEmpty()) { Result->SetStringField(TEXT("property_path"), PropertyPath); }
	Result->SetObjectField(TEXT("metadata"), Tree);
	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleSpawnActorByClass(
	const TSharedPtr<FJsonObject>& Params)
{
	FString ClassPath;
	if (!Params->TryGetStringField(TEXT("class_path"), ClassPath) || ClassPath.IsEmpty())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'class_path' parameter"));
	}

	FString DesiredName;
	Params->TryGetStringField(TEXT("name"), DesiredName);

	FVector Location = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;
	FVector Scale = FVector::OneVector;
	if (Params->HasField(TEXT("location"))) { Location = FEpicUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location")); }
	if (Params->HasField(TEXT("rotation"))) { Rotation = FEpicUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation")); }
	if (Params->HasField(TEXT("scale"))) { Scale = FEpicUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("scale")); }

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
	}

	UClass* Class = nullptr;

	// 1. Full class path (contains '.' and starts with '/'): direct find/load.
	if (ClassPath.Contains(TEXT(".")))
	{
		Class = FindObject<UClass>(nullptr, *ClassPath);
		if (!Class) { Class = LoadObject<UClass>(nullptr, *ClassPath); }
	}

	// 2. Blueprint asset path (e.g. "/Game/Blueprints/BP_Foo"): load asset, get GeneratedClass.
	if (!Class && ClassPath.StartsWith(TEXT("/")))
	{
		FString BPPath = ClassPath;
		if (!BPPath.Contains(TEXT(".")))
		{
			FString PathOnly, AssetName;
			BPPath.Split(TEXT("/"), &PathOnly, &AssetName, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
			if (!AssetName.IsEmpty()) { BPPath = ClassPath + TEXT(".") + AssetName; }
		}
		UObject* Asset = StaticLoadObject(UObject::StaticClass(), nullptr, *BPPath);
		if (!Asset) { Asset = UEditorAssetLibrary::LoadAsset(ClassPath); }
		if (UBlueprint* BP = Cast<UBlueprint>(Asset))
		{
			Class = BP->GeneratedClass;
		}
		if (!Class)
		{
			// Maybe ClassPath is the GeneratedClass itself (ends with _C)
			Class = LoadObject<UClass>(nullptr, *(ClassPath + TEXT("_C")));
		}
	}

	// 3. Short name fallback — iterate UClass to find by short name.
	if (!Class)
	{
		Class = FEpicUnrealMCPPropertyUtils::ResolveAnyClass(ClassPath);
	}

	if (!Class)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Could not resolve class '%s'"), *ClassPath));
	}
	if (!Class->IsChildOf(AActor::StaticClass()))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Class '%s' is not an AActor subclass"), *Class->GetPathName()));
	}
	if (Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Class '%s' is abstract or deprecated"), *Class->GetPathName()));
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (!DesiredName.IsEmpty()) { SpawnParams.Name = FName(*DesiredName); }
	SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;

	AActor* NewActor = nullptr;
	if (UEditorActorSubsystem* EAS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>())
	{
		// Editor subsystem path — registers with transaction system.
		NewActor = EAS->SpawnActorFromClass(Class, Location, Rotation, /*bTransient*/ false);
	}
	if (!NewActor)
	{
		NewActor = World->SpawnActor<AActor>(Class, Location, Rotation, SpawnParams);
	}
	if (!NewActor)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("SpawnActor returned null for class '%s'"), *Class->GetPathName()));
	}

	if (!Scale.Equals(FVector::OneVector))
	{
		NewActor->SetActorScale3D(Scale);
	}
	if (!DesiredName.IsEmpty())
	{
		NewActor->SetActorLabel(DesiredName);
	}

	if (ULevel* Level = NewActor->GetLevel())
	{
		Level->MarkPackageDirty();
	}

	TSharedPtr<FJsonObject> Result = FEpicUnrealMCPCommonUtils::ActorToJsonObject(NewActor, true);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("class_path"), Class->GetPathName());
	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleFindActors(
	const TSharedPtr<FJsonObject>& Params)
{
	FString NamePattern, LabelPattern, ClassFilter, TagFilter;
	Params->TryGetStringField(TEXT("name_pattern"), NamePattern);
	Params->TryGetStringField(TEXT("label_pattern"), LabelPattern);
	Params->TryGetStringField(TEXT("class_filter"), ClassFilter);
	Params->TryGetStringField(TEXT("tag"), TagFilter);

	int32 MaxResults = 100;
	double MaxResultsD = 0.0;
	if (Params->TryGetNumberField(TEXT("max_results"), MaxResultsD)) { MaxResults = static_cast<int32>(MaxResultsD); }

	bool bExactClass = false;
	Params->TryGetBoolField(TEXT("exact_class"), bExactClass);

	bool bIncludeTransform = false;
	Params->TryGetBoolField(TEXT("include_transform"), bIncludeTransform);

	const FString NameLower = NamePattern.ToLower();
	const FString LabelLower = LabelPattern.ToLower();
	const FString ClassLower = ClassFilter.ToLower();

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
	}

	// Resolve class filter as a UClass if it looks like a real class (so we can do IsChildOf).
	UClass* ClassFilterClass = nullptr;
	if (!ClassFilter.IsEmpty())
	{
		ClassFilterClass = FEpicUnrealMCPPropertyUtils::ResolveAnyClass(ClassFilter);
	}

	TArray<TSharedPtr<FJsonValue>> ActorArr;
	int32 TotalScanned = 0;
	int32 TotalMatched = 0;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (!A) { continue; }
		++TotalScanned;

		if (!NameLower.IsEmpty() && !A->GetName().ToLower().Contains(NameLower)) { continue; }
		if (!LabelLower.IsEmpty() && !A->GetActorLabel().ToLower().Contains(LabelLower)) { continue; }

		if (!ClassFilter.IsEmpty())
		{
			if (ClassFilterClass)
			{
				if (bExactClass)
				{
					if (A->GetClass() != ClassFilterClass) { continue; }
				}
				else
				{
					if (!A->IsA(ClassFilterClass)) { continue; }
				}
			}
			else
			{
				const FString ClassNameLower = A->GetClass()->GetName().ToLower();
				const FString ClassPathLower = A->GetClass()->GetPathName().ToLower();
				if (!ClassNameLower.Contains(ClassLower) && !ClassPathLower.Contains(ClassLower)) { continue; }
			}
		}

		if (!TagFilter.IsEmpty() && !A->Tags.Contains(FName(*TagFilter))) { continue; }

		++TotalMatched;
		if (ActorArr.Num() >= MaxResults && MaxResults > 0) { continue; }

		TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("name"), A->GetName());
		Entry->SetStringField(TEXT("label"), A->GetActorLabel());
		Entry->SetStringField(TEXT("class"), A->GetClass()->GetPathName());
		Entry->SetStringField(TEXT("class_short"), A->GetClass()->GetName());
		if (bIncludeTransform)
		{
			const FVector L = A->GetActorLocation();
			const FRotator R = A->GetActorRotation();
			const FVector S = A->GetActorScale3D();
			TArray<TSharedPtr<FJsonValue>> Loc = { MakeShared<FJsonValueNumber>(L.X), MakeShared<FJsonValueNumber>(L.Y), MakeShared<FJsonValueNumber>(L.Z) };
			TArray<TSharedPtr<FJsonValue>> Rot = { MakeShared<FJsonValueNumber>(R.Pitch), MakeShared<FJsonValueNumber>(R.Yaw), MakeShared<FJsonValueNumber>(R.Roll) };
			TArray<TSharedPtr<FJsonValue>> Scl = { MakeShared<FJsonValueNumber>(S.X), MakeShared<FJsonValueNumber>(S.Y), MakeShared<FJsonValueNumber>(S.Z) };
			Entry->SetArrayField(TEXT("location"), Loc);
			Entry->SetArrayField(TEXT("rotation"), Rot);
			Entry->SetArrayField(TEXT("scale"), Scl);
		}
		if (A->Tags.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> Tags;
			for (const FName& T : A->Tags) { Tags.Add(MakeShared<FJsonValueString>(T.ToString())); }
			Entry->SetArrayField(TEXT("tags"), Tags);
		}
		ActorArr.Add(MakeShared<FJsonValueObject>(Entry));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetNumberField(TEXT("total_scanned"), TotalScanned);
	Result->SetNumberField(TEXT("total_matched"), TotalMatched);
	Result->SetNumberField(TEXT("returned"), ActorArr.Num());
	Result->SetBoolField(TEXT("truncated"), TotalMatched > ActorArr.Num());
	Result->SetArrayField(TEXT("actors"), ActorArr);
	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleTakeScreenshot(
	const TSharedPtr<FJsonObject>& Params)
{
	FString OutputPath;
	if (!Params->TryGetStringField(TEXT("file_path"), OutputPath) || OutputPath.IsEmpty())
	{
		OutputPath = FPaths::ProjectSavedDir() / TEXT("Screenshots") / TEXT("MCP_Screenshot.png");
	}

	// "viewport" = level viewport only, "window" = full editor window
	FString Mode = TEXT("viewport");
	Params->TryGetStringField(TEXT("mode"), Mode);

	IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutputPath), true);

	TArray<FColor> Pixels;
	int32 Width = 0;
	int32 Height = 0;

	if (Mode == TEXT("window"))
	{
		// Find the active editor window
		TSharedPtr<SWindow> Window = FSlateApplication::Get().GetActiveTopLevelWindow();

		// Fall back to the first visible window if editor lost focus
		if (!Window.IsValid())
		{
			TArray<TSharedRef<SWindow>> AllWindows;
			FSlateApplication::Get().GetAllVisibleWindowsOrdered(AllWindows);
			if (AllWindows.Num() > 0)
			{
				Window = AllWindows[0];
			}
		}

		if (!Window.IsValid())
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor window found"));
		}

		// Use Slate TakeScreenshot — cross-platform, captures the full editor window
		// including BP graphs, material editors, data tables, and all Slate UI
		FIntVector Size;
		bool bCaptureSuccess = FSlateApplication::Get().TakeScreenshot(
			Window.ToSharedRef(), Pixels, Size);

		if (!bCaptureSuccess || Pixels.Num() == 0)
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				TEXT("Failed to capture editor window"));
		}

		Width = Size.X;
		Height = Size.Y;
	}
	else
	{
		// Capture the active level viewport
		FLevelEditorModule& LevelEditor =
			FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
		TSharedPtr<IAssetViewport> ActiveViewport = LevelEditor.GetFirstActiveViewport();
		if (!ActiveViewport.IsValid())
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No active level viewport found"));
		}

		FViewport* Viewport = ActiveViewport->GetActiveViewport();
		if (!Viewport)
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				TEXT("Failed to get viewport render target"));
		}

		const FIntPoint Size = Viewport->GetSizeXY();
		if (Size.X <= 0 || Size.Y <= 0)
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				TEXT("Viewport has invalid size (may be minimized or not yet rendered)"));
		}

		if (!Viewport->ReadPixels(Pixels))
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				TEXT("Failed to read pixels from viewport"));
		}

		Width = Size.X;
		Height = Size.Y;
	}

	// Both viewport and Slate may return alpha = 0
	for (FColor& Pixel : Pixels)
	{
		Pixel.A = 255;
	}

	IImageWrapperModule& ImageWrapperModule =
		FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
	TSharedPtr<IImageWrapper> PngWriter =
		ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
	if (!PngWriter.IsValid() ||
		!PngWriter->SetRaw(Pixels.GetData(), Pixels.Num() * sizeof(FColor),
			Width, Height, ERGBFormat::BGRA, 8))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to compress image to PNG"));
	}

	const TArray64<uint8> PNGData = PngWriter->GetCompressed();
	if (!FFileHelper::SaveArrayToFile(PNGData, *OutputPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Failed to save screenshot to: %s"), *OutputPath));
	}

	// Absolute path so callers can read the file directly
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("file_path"), FPaths::ConvertRelativePathToFull(OutputPath));
	Result->SetNumberField(TEXT("width"), Width);
	Result->SetNumberField(TEXT("height"), Height);
	return Result;
}
