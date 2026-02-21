#include "Commands/EpicUnrealMCPBlueprintCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "Commands/EpicUnrealMCPPropertyUtils.h"
#include "JsonObjectConverter.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "KismetCompilerModule.h"
#include "EdGraphSchema_K2.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Engine/Engine.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "UObject/Field.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UObjectIterator.h"

FEpicUnrealMCPBlueprintCommands::FEpicUnrealMCPBlueprintCommands()
{
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleCommand(
	const FString& CommandType,
	const TSharedPtr<FJsonObject>& Params)
{
	if (CommandType == TEXT("create_blueprint"))
	{
		return HandleCreateBlueprint(Params);
	}
	else if (CommandType == TEXT("search_parent_classes"))
	{
		return HandleSearchParentClasses(Params);
	}
	else if (CommandType == TEXT("add_component_to_blueprint"))
	{
		return HandleAddComponentToBlueprint(Params);
	}
	else if (CommandType == TEXT("set_physics_properties"))
	{
		return HandleSetPhysicsProperties(Params);
	}
	else if (CommandType == TEXT("compile_blueprint"))
	{
		return HandleCompileBlueprint(Params);
	}
	else if (CommandType == TEXT("set_static_mesh_properties"))
	{
		return HandleSetStaticMeshProperties(Params);
	}
	else if (CommandType == TEXT("spawn_blueprint_actor"))
	{
		return HandleSpawnBlueprintActor(Params);
	}
	else if (CommandType == TEXT("set_mesh_material_color"))
	{
		return HandleSetMeshMaterialColor(Params);
	}
	else if (CommandType == TEXT("get_available_materials"))
	{
		return HandleGetAvailableMaterials(Params);
	}
	else if (CommandType == TEXT("apply_material_to_actor"))
	{
		return HandleApplyMaterialToActor(Params);
	}
	else if (CommandType == TEXT("apply_material_to_blueprint"))
	{
		return HandleApplyMaterialToBlueprint(Params);
	}
	else if (CommandType == TEXT("get_actor_material_info"))
	{
		return HandleGetActorMaterialInfo(Params);
	}
	else if (CommandType == TEXT("get_blueprint_material_info"))
	{
		return HandleGetBlueprintMaterialInfo(Params);
	}
	else if (CommandType == TEXT("read_blueprint_content"))
	{
		return HandleReadBlueprintContent(Params);
	}
	else if (CommandType == TEXT("analyze_blueprint_graph"))
	{
		return HandleAnalyzeBlueprintGraph(Params);
	}
	else if (CommandType == TEXT("get_blueprint_variable_details"))
	{
		return HandleGetBlueprintVariableDetails(Params);
	}
	else if (CommandType == TEXT("get_blueprint_function_details"))
	{
		return HandleGetBlueprintFunctionDetails(Params);
	}
	else if (CommandType == TEXT("get_blueprint_class_defaults"))
	{
		return HandleGetBlueprintClassDefaults(Params);
	}

	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
		FString::Printf(TEXT("Unknown blueprint command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleCreateBlueprint(
	const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("name"), BlueprintName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
	}

	FString PackagePath = TEXT("/Game/Blueprints/");
	FString CustomPath;
	if (Params->TryGetStringField(TEXT("path"), CustomPath) && !CustomPath.IsEmpty())
	{
		PackagePath = CustomPath;
		if (!PackagePath.EndsWith(TEXT("/")))
		{
			PackagePath += TEXT("/");
		}
	}

	FString AssetName = BlueprintName;
	if (UEditorAssetLibrary::DoesAssetExist(PackagePath + AssetName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Blueprint already exists: %s"), *BlueprintName));
	}

	// Resolve parent class — supports C++ classes from any module, Blueprint
	// parents, full paths, short names, A/U-prefixed names
	FString ParentClass;
	Params->TryGetStringField(TEXT("parent_class"), ParentClass);

	UClass* SelectedParentClass = AActor::StaticClass();

	if (!ParentClass.IsEmpty())
	{
		UClass* FoundClass = nullptr;

		// Content path — try as Blueprint asset first, then as class path
		if (ParentClass.StartsWith(TEXT("/")))
		{
			UBlueprint* ParentBP = Cast<UBlueprint>(
				UEditorAssetLibrary::LoadAsset(ParentClass));
			if (ParentBP && ParentBP->GeneratedClass)
			{
				FoundClass = ParentBP->GeneratedClass;
			}
			else
			{
				FoundClass = FEpicUnrealMCPPropertyUtils::ResolveAnyClass(ParentClass);
			}
		}
		else
		{
			FoundClass = FEpicUnrealMCPPropertyUtils::ResolveAnyClass(ParentClass);
		}

		if (FoundClass)
		{
			SelectedParentClass = FoundClass;
		}
		else
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Could not find parent class '%s'. Use search_parent_classes to find the correct class name."),
					*ParentClass));
		}
	}

	// Same check the engine uses in UBlueprintFactory::FactoryCreateNew
	if (!FKismetEditorUtilities::CanCreateBlueprintOfClass(SelectedParentClass))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Cannot create a Blueprint based on class '%s'. Class must be Blueprintable and not deprecated."),
				*SelectedParentClass->GetName()));
	}

	// Resolve correct Blueprint asset type (e.g. UUserWidget -> UWidgetBlueprint)
	UClass* BlueprintClass = nullptr;
	UClass* BlueprintGeneratedClass = nullptr;

	IKismetCompilerInterface& KismetCompilerModule =
		FModuleManager::LoadModuleChecked<IKismetCompilerInterface>("KismetCompiler");
	KismetCompilerModule.GetBlueprintTypesForClass(
		SelectedParentClass, BlueprintClass, BlueprintGeneratedClass);

	UPackage* Package = CreatePackage(*(PackagePath + AssetName));

	UBlueprint* NewBlueprint = FKismetEditorUtilities::CreateBlueprint(
		SelectedParentClass, Package, *AssetName, BPTYPE_Normal,
		BlueprintClass, BlueprintGeneratedClass, FName("MCP"));

	if (NewBlueprint)
	{
		FAssetRegistryModule::AssetCreated(NewBlueprint);
		Package->MarkPackageDirty();

		TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
		ResultObj->SetStringField(TEXT("name"), AssetName);
		ResultObj->SetStringField(TEXT("path"), PackagePath + AssetName);
		ResultObj->SetStringField(TEXT("parent_class"), SelectedParentClass->GetName());
		ResultObj->SetStringField(TEXT("parent_class_path"), SelectedParentClass->GetPathName());
		ResultObj->SetStringField(TEXT("blueprint_type"), BlueprintClass->GetName());
		return ResultObj;
	}

	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create blueprint"));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleSearchParentClasses(
	const TSharedPtr<FJsonObject>& Params)
{
	FString Filter;
	if (!Params->TryGetStringField(TEXT("filter"), Filter) || Filter.IsEmpty())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'filter' parameter"));
	}

	int32 MaxResults = 20;
	if (Params->HasField(TEXT("max_results")))
	{
		MaxResults = FMath::Clamp(Params->GetIntegerField(TEXT("max_results")), 1, 100);
	}

	bool bIncludeBlueprintClasses = true;
	Params->TryGetBoolField(TEXT("include_blueprint_classes"), bIncludeBlueprintClasses);

	const FString FilterLower = Filter.ToLower();

	TArray<TSharedPtr<FJsonValue>> ResultArray;

	for (TObjectIterator<UClass> It; It; ++It)
	{
		if (ResultArray.Num() >= MaxResults)
		{
			break;
		}

		UClass* C = *It;
		if (!C)
		{
			continue;
		}

		if (C->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists))
		{
			continue;
		}

		if (!FKismetEditorUtilities::CanCreateBlueprintOfClass(C))
		{
			continue;
		}

		const bool bIsBlueprintGenerated = C->HasAnyClassFlags(CLASS_CompiledFromBlueprint);
		if (bIsBlueprintGenerated && !bIncludeBlueprintClasses)
		{
			continue;
		}

		const FString ClassName = C->GetName();
		const FString ClassNameLower = ClassName.ToLower();

		// Strip UE prefix (A/U) for more flexible matching
		FString UnprefixedName = ClassName;
		if (ClassName.Len() > 1
			&& (ClassName[0] == TEXT('A') || ClassName[0] == TEXT('U'))
			&& FChar::IsUpper(ClassName[1]))
		{
			UnprefixedName = ClassName.RightChop(1);
		}
		const FString UnprefixedLower = UnprefixedName.ToLower();

		if (!ClassNameLower.Contains(FilterLower) && !UnprefixedLower.Contains(FilterLower))
		{
			continue;
		}

		FString ClassPath = C->GetPathName();
		FString ModuleName = TEXT("Unknown");
		if (ClassPath.StartsWith(TEXT("/Script/")))
		{
			FString Remainder = ClassPath.RightChop(8);
			int32 DotIndex;
			if (Remainder.FindChar(TEXT('.'), DotIndex))
			{
				ModuleName = Remainder.Left(DotIndex);
			}
		}

		TSharedPtr<FJsonObject> ClassObj = MakeShared<FJsonObject>();
		ClassObj->SetStringField(TEXT("name"), ClassName);
		ClassObj->SetStringField(TEXT("path"), ClassPath);
		ClassObj->SetStringField(TEXT("module"), ModuleName);
		ClassObj->SetBoolField(TEXT("is_blueprint"), bIsBlueprintGenerated);

		if (bIsBlueprintGenerated)
		{
			UObject* Outer = C->GetOuter();
			if (Outer)
			{
				ClassObj->SetStringField(TEXT("blueprint_path"), Outer->GetPathName());
			}
		}

		UClass* SuperClass = C->GetSuperClass();
		if (SuperClass)
		{
			ClassObj->SetStringField(TEXT("parent"), SuperClass->GetName());
		}

		ResultArray.Add(MakeShared<FJsonValueObject>(ClassObj));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("filter"), Filter);
	Result->SetNumberField(TEXT("count"), ResultArray.Num());
	Result->SetNumberField(TEXT("max_results"), MaxResults);
	Result->SetArrayField(TEXT("classes"), ResultArray);
	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleAddComponentToBlueprint(
	const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
	}

	FString ComponentType;
	if (!Params->TryGetStringField(TEXT("component_type"), ComponentType))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'type' parameter"));
	}

	FString ComponentName;
	if (!Params->TryGetStringField(TEXT("component_name"), ComponentName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
	}

	UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
	if (!Blueprint)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
	}

	// Try exact name, then with "Component" suffix, then with "U" prefix
	UClass* ComponentClass = FindObject<UClass>(nullptr, *ComponentType);

	if (!ComponentClass && !ComponentType.EndsWith(TEXT("Component")))
	{
		FString WithSuffix = ComponentType + TEXT("Component");
		ComponentClass = FindObject<UClass>(nullptr, *WithSuffix);
	}

	if (!ComponentClass && !ComponentType.StartsWith(TEXT("U")))
	{
		FString WithPrefix = TEXT("U") + ComponentType;
		ComponentClass = FindObject<UClass>(nullptr, *WithPrefix);

		if (!ComponentClass && !ComponentType.EndsWith(TEXT("Component")))
		{
			FString WithBoth = TEXT("U") + ComponentType + TEXT("Component");
			ComponentClass = FindObject<UClass>(nullptr, *WithBoth);
		}
	}

	if (!ComponentClass || !ComponentClass->IsChildOf(UActorComponent::StaticClass()))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Unknown component type: %s"), *ComponentType));
	}

	USCS_Node* NewNode = Blueprint->SimpleConstructionScript->CreateNode(
		ComponentClass, *ComponentName);
	if (NewNode)
	{
		USceneComponent* SceneComponent = Cast<USceneComponent>(NewNode->ComponentTemplate);
		if (SceneComponent)
		{
			if (Params->HasField(TEXT("location")))
			{
				SceneComponent->SetRelativeLocation(
					FEpicUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location")));
			}
			if (Params->HasField(TEXT("rotation")))
			{
				SceneComponent->SetRelativeRotation(
					FEpicUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation")));
			}
			if (Params->HasField(TEXT("scale")))
			{
				SceneComponent->SetRelativeScale3D(
					FEpicUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("scale")));
			}
		}

		Blueprint->SimpleConstructionScript->AddNode(NewNode);
		FKismetEditorUtilities::CompileBlueprint(Blueprint);

		TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
		ResultObj->SetStringField(TEXT("component_name"), ComponentName);
		ResultObj->SetStringField(TEXT("component_type"), ComponentType);
		return ResultObj;
	}

	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to add component to blueprint"));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleSetPhysicsProperties(
	const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
	}

	FString ComponentName;
	if (!Params->TryGetStringField(TEXT("component_name"), ComponentName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'component_name' parameter"));
	}

	UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
	if (!Blueprint)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
	}

	USCS_Node* ComponentNode = nullptr;
	for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
	{
		if (Node && Node->GetVariableName().ToString() == ComponentName)
		{
			ComponentNode = Node;
			break;
		}
	}

	if (!ComponentNode)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Component not found: %s"), *ComponentName));
	}

	UPrimitiveComponent* PrimComponent = Cast<UPrimitiveComponent>(ComponentNode->ComponentTemplate);
	if (!PrimComponent)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Component is not a primitive component"));
	}

	if (Params->HasField(TEXT("simulate_physics")))
	{
		PrimComponent->SetSimulatePhysics(Params->GetBoolField(TEXT("simulate_physics")));
	}

	if (Params->HasField(TEXT("mass")))
	{
		float Mass = Params->GetNumberField(TEXT("mass"));
		PrimComponent->SetMassOverrideInKg(NAME_None, Mass);
	}

	if (Params->HasField(TEXT("linear_damping")))
	{
		PrimComponent->SetLinearDamping(Params->GetNumberField(TEXT("linear_damping")));
	}

	if (Params->HasField(TEXT("angular_damping")))
	{
		PrimComponent->SetAngularDamping(Params->GetNumberField(TEXT("angular_damping")));
	}

	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("component"), ComponentName);
	return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleCompileBlueprint(
	const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
	}

	UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
	if (!Blueprint)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
	}

	FKismetEditorUtilities::CompileBlueprint(Blueprint);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("name"), BlueprintName);
	ResultObj->SetBoolField(TEXT("compiled"), true);
	return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleSpawnBlueprintActor(
	const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
	}

	FString ActorName;
	if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
	}

	UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
	if (!Blueprint)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
	}

	FVector Location(0.0f, 0.0f, 0.0f);
	FRotator Rotation(0.0f, 0.0f, 0.0f);

	if (Params->HasField(TEXT("location")))
	{
		Location = FEpicUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location"));
	}
	if (Params->HasField(TEXT("rotation")))
	{
		Rotation = FEpicUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"));
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
	}

	FTransform SpawnTransform;
	SpawnTransform.SetLocation(Location);
	SpawnTransform.SetRotation(FQuat(Rotation));

	// Brief delay so the engine can process a freshly compiled class
	FPlatformProcess::Sleep(0.2f);

	AActor* NewActor = World->SpawnActor<AActor>(Blueprint->GeneratedClass, SpawnTransform);

	if (NewActor)
	{
		NewActor->SetActorLabel(*ActorName);
		return FEpicUnrealMCPCommonUtils::ActorToJsonObject(NewActor, true);
	}

	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to spawn blueprint actor"));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleSetStaticMeshProperties(
	const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
	}

	FString ComponentName;
	if (!Params->TryGetStringField(TEXT("component_name"), ComponentName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'component_name' parameter"));
	}

	UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
	if (!Blueprint)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
	}

	USCS_Node* ComponentNode = nullptr;
	for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
	{
		if (Node && Node->GetVariableName().ToString() == ComponentName)
		{
			ComponentNode = Node;
			break;
		}
	}

	if (!ComponentNode)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Component not found: %s"), *ComponentName));
	}

	UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(ComponentNode->ComponentTemplate);
	if (!MeshComponent)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Component is not a static mesh component"));
	}

	if (Params->HasField(TEXT("static_mesh")))
	{
		FString MeshPath = Params->GetStringField(TEXT("static_mesh"));
		UStaticMesh* Mesh = Cast<UStaticMesh>(UEditorAssetLibrary::LoadAsset(MeshPath));
		if (Mesh)
		{
			MeshComponent->SetStaticMesh(Mesh);
		}
	}

	if (Params->HasField(TEXT("material")))
	{
		FString MaterialPath = Params->GetStringField(TEXT("material"));
		UMaterialInterface* Material = Cast<UMaterialInterface>(
			UEditorAssetLibrary::LoadAsset(MaterialPath));
		if (Material)
		{
			MeshComponent->SetMaterial(0, Material);
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("component"), ComponentName);
	return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleSetMeshMaterialColor(
	const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
	}

	FString ComponentName;
	if (!Params->TryGetStringField(TEXT("component_name"), ComponentName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'component_name' parameter"));
	}

	UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
	if (!Blueprint)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
	}

	USCS_Node* ComponentNode = nullptr;
	for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
	{
		if (Node && Node->GetVariableName().ToString() == ComponentName)
		{
			ComponentNode = Node;
			break;
		}
	}

	if (!ComponentNode)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Component not found: %s"), *ComponentName));
	}

	UPrimitiveComponent* PrimComponent = Cast<UPrimitiveComponent>(ComponentNode->ComponentTemplate);
	if (!PrimComponent)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Component is not a primitive component"));
	}

	TArray<float> ColorArray;
	const TArray<TSharedPtr<FJsonValue>>* ColorJsonArray;
	if (!Params->TryGetArrayField(TEXT("color"), ColorJsonArray) || ColorJsonArray->Num() != 4)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("'color' must be an array of 4 float values [R, G, B, A]"));
	}

	for (const TSharedPtr<FJsonValue>& Value : *ColorJsonArray)
	{
		ColorArray.Add(FMath::Clamp(Value->AsNumber(), 0.0f, 1.0f));
	}

	FLinearColor Color(ColorArray[0], ColorArray[1], ColorArray[2], ColorArray[3]);

	int32 MaterialSlot = 0;
	if (Params->HasField(TEXT("material_slot")))
	{
		MaterialSlot = Params->GetIntegerField(TEXT("material_slot"));
	}

	FString ParameterName = TEXT("BaseColor");
	Params->TryGetStringField(TEXT("parameter_name"), ParameterName);

	UMaterialInterface* Material = nullptr;

	FString MaterialPath;
	if (Params->TryGetStringField(TEXT("material_path"), MaterialPath))
	{
		Material = Cast<UMaterialInterface>(UEditorAssetLibrary::LoadAsset(MaterialPath));
		if (!Material)
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Failed to load material: %s"), *MaterialPath));
		}
	}
	else
	{
		Material = PrimComponent->GetMaterial(MaterialSlot);
		if (!Material)
		{
			Material = Cast<UMaterialInterface>(
				UEditorAssetLibrary::LoadAsset(TEXT("/Engine/BasicShapes/BasicShapeMaterial")));
			if (!Material)
			{
				return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
					TEXT("No material found on component and failed to load default material"));
			}
		}
	}

	UMaterialInstanceDynamic* DynMaterial = UMaterialInstanceDynamic::Create(Material, PrimComponent);
	if (!DynMaterial)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create dynamic material instance"));
	}

	DynMaterial->SetVectorParameterValue(*ParameterName, Color);
	PrimComponent->SetMaterial(MaterialSlot, DynMaterial);
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("component"), ComponentName);
	ResultObj->SetNumberField(TEXT("material_slot"), MaterialSlot);
	ResultObj->SetStringField(TEXT("parameter_name"), ParameterName);

	TArray<TSharedPtr<FJsonValue>> ColorResultArray;
	ColorResultArray.Add(MakeShared<FJsonValueNumber>(Color.R));
	ColorResultArray.Add(MakeShared<FJsonValueNumber>(Color.G));
	ColorResultArray.Add(MakeShared<FJsonValueNumber>(Color.B));
	ColorResultArray.Add(MakeShared<FJsonValueNumber>(Color.A));
	ResultObj->SetArrayField(TEXT("color"), ColorResultArray);

	ResultObj->SetBoolField(TEXT("success"), true);
	return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleGetAvailableMaterials(
	const TSharedPtr<FJsonObject>& Params)
{
	FString SearchPath;
	if (!Params->TryGetStringField(TEXT("search_path"), SearchPath))
	{
		SearchPath = TEXT("");
	}

	bool bIncludeEngineMaterials = true;
	if (Params->HasField(TEXT("include_engine_materials")))
	{
		bIncludeEngineMaterials = Params->GetBoolField(TEXT("include_engine_materials"));
	}

	FAssetRegistryModule& AssetRegistryModule =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FARFilter Filter;
	Filter.ClassPaths.Add(UMaterialInterface::StaticClass()->GetClassPathName());
	Filter.ClassPaths.Add(UMaterial::StaticClass()->GetClassPathName());
	Filter.ClassPaths.Add(UMaterialInstanceConstant::StaticClass()->GetClassPathName());
	Filter.ClassPaths.Add(UMaterialInstanceDynamic::StaticClass()->GetClassPathName());

	if (!SearchPath.IsEmpty())
	{
		if (!SearchPath.StartsWith(TEXT("/")))
		{
			SearchPath = TEXT("/") + SearchPath;
		}
		if (!SearchPath.EndsWith(TEXT("/")))
		{
			SearchPath += TEXT("/");
		}
		Filter.PackagePaths.Add(*SearchPath);
	}
	else
	{
		Filter.PackagePaths.Add(TEXT("/Game/"));
	}

	if (bIncludeEngineMaterials)
	{
		Filter.PackagePaths.Add(TEXT("/Engine/"));
	}

	Filter.bRecursivePaths = true;

	TArray<FAssetData> AssetDataArray;
	AssetRegistry.GetAssets(Filter, AssetDataArray);

	// Supplement with EditorAssetLibrary for more comprehensive results
	TArray<FString> AllAssetPaths;
	if (!SearchPath.IsEmpty())
	{
		AllAssetPaths = UEditorAssetLibrary::ListAssets(SearchPath, true, false);
	}
	else
	{
		AllAssetPaths = UEditorAssetLibrary::ListAssets(TEXT("/Game/"), true, false);
	}

	for (const FString& AssetPath : AllAssetPaths)
	{
		if (AssetPath.Contains(TEXT("Material")) && !AssetPath.Contains(TEXT(".uasset")))
		{
			UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
			if (Asset && Asset->IsA<UMaterialInterface>())
			{
				bool bAlreadyFound = false;
				for (const FAssetData& ExistingData : AssetDataArray)
				{
					if (ExistingData.GetObjectPathString() == AssetPath)
					{
						bAlreadyFound = true;
						break;
					}
				}

				if (!bAlreadyFound)
				{
					FAssetData ManualAssetData(Asset);
					AssetDataArray.Add(ManualAssetData);
				}
			}
		}
	}

	TArray<TSharedPtr<FJsonValue>> MaterialArray;
	for (const FAssetData& AssetData : AssetDataArray)
	{
		TSharedPtr<FJsonObject> MaterialObj = MakeShared<FJsonObject>();
		MaterialObj->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
		MaterialObj->SetStringField(TEXT("path"), AssetData.GetObjectPathString());
		MaterialObj->SetStringField(TEXT("package"), AssetData.PackageName.ToString());
		MaterialObj->SetStringField(TEXT("class"), AssetData.AssetClassPath.ToString());
		MaterialArray.Add(MakeShared<FJsonValueObject>(MaterialObj));
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetArrayField(TEXT("materials"), MaterialArray);
	ResultObj->SetNumberField(TEXT("count"), MaterialArray.Num());
	ResultObj->SetStringField(TEXT("search_path_used"), SearchPath.IsEmpty() ? TEXT("/Game/") : SearchPath);

	return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleApplyMaterialToActor(
	const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName;
	if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
	}

	FString MaterialPath;
	if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path' parameter"));
	}

	int32 MaterialSlot = 0;
	if (Params->HasField(TEXT("material_slot")))
	{
		MaterialSlot = Params->GetIntegerField(TEXT("material_slot"));
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
	}

	AActor* TargetActor = nullptr;
	TArray<AActor*> AllActors;
	UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);

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

	UMaterialInterface* Material = Cast<UMaterialInterface>(UEditorAssetLibrary::LoadAsset(MaterialPath));
	if (!Material)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Failed to load material: %s"), *MaterialPath));
	}

	TArray<UStaticMeshComponent*> MeshComponents;
	TargetActor->GetComponents<UStaticMeshComponent>(MeshComponents);

	bool bAppliedToAny = false;
	for (UStaticMeshComponent* MeshComp : MeshComponents)
	{
		if (MeshComp)
		{
			MeshComp->SetMaterial(MaterialSlot, Material);
			bAppliedToAny = true;
		}
	}

	if (!bAppliedToAny)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No mesh components found on actor"));
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("actor_name"), ActorName);
	ResultObj->SetStringField(TEXT("material_path"), MaterialPath);
	ResultObj->SetNumberField(TEXT("material_slot"), MaterialSlot);
	ResultObj->SetBoolField(TEXT("success"), true);

	return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleApplyMaterialToBlueprint(
	const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
	}

	FString ComponentName;
	if (!Params->TryGetStringField(TEXT("component_name"), ComponentName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'component_name' parameter"));
	}

	FString MaterialPath;
	if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path' parameter"));
	}

	int32 MaterialSlot = 0;
	if (Params->HasField(TEXT("material_slot")))
	{
		MaterialSlot = Params->GetIntegerField(TEXT("material_slot"));
	}

	UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
	if (!Blueprint)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
	}

	USCS_Node* ComponentNode = nullptr;
	for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
	{
		if (Node && Node->GetVariableName().ToString() == ComponentName)
		{
			ComponentNode = Node;
			break;
		}
	}

	if (!ComponentNode)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Component not found: %s"), *ComponentName));
	}

	UPrimitiveComponent* PrimComponent = Cast<UPrimitiveComponent>(ComponentNode->ComponentTemplate);
	if (!PrimComponent)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Component is not a primitive component"));
	}

	UMaterialInterface* Material = Cast<UMaterialInterface>(UEditorAssetLibrary::LoadAsset(MaterialPath));
	if (!Material)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Failed to load material: %s"), *MaterialPath));
	}

	PrimComponent->SetMaterial(MaterialSlot, Material);
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("blueprint_name"), BlueprintName);
	ResultObj->SetStringField(TEXT("component_name"), ComponentName);
	ResultObj->SetStringField(TEXT("material_path"), MaterialPath);
	ResultObj->SetNumberField(TEXT("material_slot"), MaterialSlot);
	ResultObj->SetBoolField(TEXT("success"), true);

	return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleGetActorMaterialInfo(
	const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName;
	if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
	}

	AActor* TargetActor = nullptr;
	TArray<AActor*> AllActors;
	UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);

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

	TArray<UStaticMeshComponent*> MeshComponents;
	TargetActor->GetComponents<UStaticMeshComponent>(MeshComponents);

	TArray<TSharedPtr<FJsonValue>> MaterialSlots;

	for (UStaticMeshComponent* MeshComp : MeshComponents)
	{
		if (!MeshComp)
		{
			continue;
		}

		for (int32 i = 0; i < MeshComp->GetNumMaterials(); i++)
		{
			TSharedPtr<FJsonObject> SlotInfo = MakeShared<FJsonObject>();
			SlotInfo->SetNumberField(TEXT("slot"), i);
			SlotInfo->SetStringField(TEXT("component"), MeshComp->GetName());

			UMaterialInterface* Material = MeshComp->GetMaterial(i);
			if (Material)
			{
				SlotInfo->SetStringField(TEXT("material_name"), Material->GetName());
				SlotInfo->SetStringField(TEXT("material_path"), Material->GetPathName());
				SlotInfo->SetStringField(TEXT("material_class"), Material->GetClass()->GetName());
			}
			else
			{
				SlotInfo->SetStringField(TEXT("material_name"), TEXT("None"));
				SlotInfo->SetStringField(TEXT("material_path"), TEXT(""));
				SlotInfo->SetStringField(TEXT("material_class"), TEXT(""));
			}

			MaterialSlots.Add(MakeShared<FJsonValueObject>(SlotInfo));
		}
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("actor_name"), ActorName);
	ResultObj->SetArrayField(TEXT("material_slots"), MaterialSlots);
	ResultObj->SetNumberField(TEXT("total_slots"), MaterialSlots.Num());

	return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleGetBlueprintMaterialInfo(
	const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
	}

	FString ComponentName;
	if (!Params->TryGetStringField(TEXT("component_name"), ComponentName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'component_name' parameter"));
	}

	UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
	if (!Blueprint)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
	}

	USCS_Node* ComponentNode = nullptr;
	for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
	{
		if (Node && Node->GetVariableName().ToString() == ComponentName)
		{
			ComponentNode = Node;
			break;
		}
	}

	if (!ComponentNode)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Component not found: %s"), *ComponentName));
	}

	UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(ComponentNode->ComponentTemplate);
	if (!MeshComponent)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Component is not a static mesh component"));
	}

	TArray<TSharedPtr<FJsonValue>> MaterialSlots;
	int32 NumMaterials = 0;

	UStaticMesh* StaticMesh = MeshComponent->GetStaticMesh();
	if (StaticMesh)
	{
		NumMaterials = StaticMesh->GetNumSections(0);

		for (int32 i = 0; i < NumMaterials; i++)
		{
			TSharedPtr<FJsonObject> SlotInfo = MakeShared<FJsonObject>();
			SlotInfo->SetNumberField(TEXT("slot"), i);
			SlotInfo->SetStringField(TEXT("component"), ComponentName);

			UMaterialInterface* Material = MeshComponent->GetMaterial(i);
			if (Material)
			{
				SlotInfo->SetStringField(TEXT("material_name"), Material->GetName());
				SlotInfo->SetStringField(TEXT("material_path"), Material->GetPathName());
				SlotInfo->SetStringField(TEXT("material_class"), Material->GetClass()->GetName());
			}
			else
			{
				SlotInfo->SetStringField(TEXT("material_name"), TEXT("None"));
				SlotInfo->SetStringField(TEXT("material_path"), TEXT(""));
				SlotInfo->SetStringField(TEXT("material_class"), TEXT(""));
			}

			MaterialSlots.Add(MakeShared<FJsonValueObject>(SlotInfo));
		}
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("blueprint_name"), BlueprintName);
	ResultObj->SetStringField(TEXT("component_name"), ComponentName);
	ResultObj->SetArrayField(TEXT("material_slots"), MaterialSlots);
	ResultObj->SetNumberField(TEXT("total_slots"), MaterialSlots.Num());
	ResultObj->SetBoolField(TEXT("has_static_mesh"), StaticMesh != nullptr);

	return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleReadBlueprintContent(
	const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintPath;
	if (!Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_path' parameter"));
	}

	bool bIncludeEventGraph = true;
	bool bIncludeFunctions = true;
	bool bIncludeVariables = true;
	bool bIncludeComponents = true;
	bool bIncludeInterfaces = true;

	Params->TryGetBoolField(TEXT("include_event_graph"), bIncludeEventGraph);
	Params->TryGetBoolField(TEXT("include_functions"), bIncludeFunctions);
	Params->TryGetBoolField(TEXT("include_variables"), bIncludeVariables);
	Params->TryGetBoolField(TEXT("include_components"), bIncludeComponents);
	Params->TryGetBoolField(TEXT("include_interfaces"), bIncludeInterfaces);

	UBlueprint* Blueprint = Cast<UBlueprint>(UEditorAssetLibrary::LoadAsset(BlueprintPath));
	if (!Blueprint)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Failed to load blueprint: %s"), *BlueprintPath));
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("blueprint_path"), BlueprintPath);
	ResultObj->SetStringField(TEXT("blueprint_name"), Blueprint->GetName());
	ResultObj->SetStringField(TEXT("parent_class"),
		Blueprint->ParentClass ? Blueprint->ParentClass->GetName() : TEXT("None"));

	if (bIncludeVariables)
	{
		TArray<TSharedPtr<FJsonValue>> VariableArray;
		for (const FBPVariableDescription& Variable : Blueprint->NewVariables)
		{
			TSharedPtr<FJsonObject> VarObj = MakeShared<FJsonObject>();
			VarObj->SetStringField(TEXT("name"), Variable.VarName.ToString());
			VarObj->SetStringField(TEXT("type"), Variable.VarType.PinCategory.ToString());
			VarObj->SetStringField(TEXT("default_value"), Variable.DefaultValue);
			VarObj->SetBoolField(TEXT("is_editable"), (Variable.PropertyFlags & CPF_Edit) != 0);
			VariableArray.Add(MakeShared<FJsonValueObject>(VarObj));
		}
		ResultObj->SetArrayField(TEXT("variables"), VariableArray);
	}

	if (bIncludeFunctions)
	{
		TArray<TSharedPtr<FJsonValue>> FunctionArray;
		for (UEdGraph* Graph : Blueprint->FunctionGraphs)
		{
			if (!Graph)
			{
				continue;
			}

			TSharedPtr<FJsonObject> FuncObj = MakeShared<FJsonObject>();
			FuncObj->SetStringField(TEXT("name"), Graph->GetName());
			FuncObj->SetStringField(TEXT("graph_type"), TEXT("Function"));
			FuncObj->SetNumberField(TEXT("node_count"), Graph->Nodes.Num());
			FunctionArray.Add(MakeShared<FJsonValueObject>(FuncObj));
		}
		ResultObj->SetArrayField(TEXT("functions"), FunctionArray);
	}

	if (bIncludeEventGraph)
	{
		TSharedPtr<FJsonObject> EventGraphObj = MakeShared<FJsonObject>();

		for (UEdGraph* Graph : Blueprint->UbergraphPages)
		{
			if (Graph && Graph->GetName() == TEXT("EventGraph"))
			{
				EventGraphObj->SetStringField(TEXT("name"), Graph->GetName());
				EventGraphObj->SetNumberField(TEXT("node_count"), Graph->Nodes.Num());

				TArray<TSharedPtr<FJsonValue>> NodeArray;
				for (UEdGraphNode* Node : Graph->Nodes)
				{
					if (!Node)
					{
						continue;
					}

					TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
					NodeObj->SetStringField(TEXT("name"), Node->GetName());
					NodeObj->SetStringField(TEXT("class"), Node->GetClass()->GetName());
					NodeObj->SetStringField(TEXT("title"),
						Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
					NodeArray.Add(MakeShared<FJsonValueObject>(NodeObj));
				}
				EventGraphObj->SetArrayField(TEXT("nodes"), NodeArray);
				break;
			}
		}

		ResultObj->SetObjectField(TEXT("event_graph"), EventGraphObj);
	}

	if (bIncludeComponents)
	{
		TArray<TSharedPtr<FJsonValue>> ComponentArray;
		if (Blueprint->SimpleConstructionScript)
		{
			for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
			{
				if (!Node || !Node->ComponentTemplate)
				{
					continue;
				}

				TSharedPtr<FJsonObject> CompObj = MakeShared<FJsonObject>();
				CompObj->SetStringField(TEXT("name"), Node->GetVariableName().ToString());
				CompObj->SetStringField(TEXT("class"),
					Node->ComponentTemplate->GetClass()->GetName());
				CompObj->SetBoolField(TEXT("is_root"),
					Node == Blueprint->SimpleConstructionScript->GetDefaultSceneRootNode());
				ComponentArray.Add(MakeShared<FJsonValueObject>(CompObj));
			}
		}
		ResultObj->SetArrayField(TEXT("components"), ComponentArray);
	}

	if (bIncludeInterfaces)
	{
		TArray<TSharedPtr<FJsonValue>> InterfaceArray;
		for (const FBPInterfaceDescription& Interface : Blueprint->ImplementedInterfaces)
		{
			TSharedPtr<FJsonObject> InterfaceObj = MakeShared<FJsonObject>();
			InterfaceObj->SetStringField(TEXT("name"),
				Interface.Interface ? Interface.Interface->GetName() : TEXT("Unknown"));
			InterfaceArray.Add(MakeShared<FJsonValueObject>(InterfaceObj));
		}
		ResultObj->SetArrayField(TEXT("interfaces"), InterfaceArray);
	}

	ResultObj->SetBoolField(TEXT("success"), true);
	return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleAnalyzeBlueprintGraph(
	const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintPath;
	if (!Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_path' parameter"));
	}

	FString GraphName = TEXT("EventGraph");
	Params->TryGetStringField(TEXT("graph_name"), GraphName);

	bool bIncludeNodeDetails = true;
	bool bIncludePinConnections = true;
	bool bTraceExecutionFlow = true;

	Params->TryGetBoolField(TEXT("include_node_details"), bIncludeNodeDetails);
	Params->TryGetBoolField(TEXT("include_pin_connections"), bIncludePinConnections);
	Params->TryGetBoolField(TEXT("trace_execution_flow"), bTraceExecutionFlow);

	UBlueprint* Blueprint = Cast<UBlueprint>(UEditorAssetLibrary::LoadAsset(BlueprintPath));
	if (!Blueprint)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Failed to load blueprint: %s"), *BlueprintPath));
	}

	UEdGraph* TargetGraph = nullptr;

	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (Graph && Graph->GetName() == GraphName)
		{
			TargetGraph = Graph;
			break;
		}
	}

	if (!TargetGraph)
	{
		for (UEdGraph* Graph : Blueprint->FunctionGraphs)
		{
			if (Graph && Graph->GetName() == GraphName)
			{
				TargetGraph = Graph;
				break;
			}
		}
	}

	if (!TargetGraph)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Graph not found: %s"), *GraphName));
	}

	TSharedPtr<FJsonObject> GraphData = MakeShared<FJsonObject>();
	GraphData->SetStringField(TEXT("graph_name"), TargetGraph->GetName());
	GraphData->SetStringField(TEXT("graph_type"), TargetGraph->GetClass()->GetName());

	TArray<TSharedPtr<FJsonValue>> NodeArray;
	TArray<TSharedPtr<FJsonValue>> ConnectionArray;

	for (UEdGraphNode* Node : TargetGraph->Nodes)
	{
		if (!Node)
		{
			continue;
		}

		TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
		NodeObj->SetStringField(TEXT("name"), Node->GetName());
		NodeObj->SetStringField(TEXT("class"), Node->GetClass()->GetName());
		NodeObj->SetStringField(TEXT("title"),
			Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());

		if (bIncludeNodeDetails)
		{
			NodeObj->SetNumberField(TEXT("pos_x"), Node->NodePosX);
			NodeObj->SetNumberField(TEXT("pos_y"), Node->NodePosY);
			NodeObj->SetBoolField(TEXT("can_rename"), Node->bCanRenameNode);
		}

		if (bIncludePinConnections)
		{
			TArray<TSharedPtr<FJsonValue>> PinArray;
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin)
				{
					continue;
				}

				TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
				PinObj->SetStringField(TEXT("name"), Pin->PinName.ToString());
				PinObj->SetStringField(TEXT("type"), Pin->PinType.PinCategory.ToString());
				PinObj->SetStringField(TEXT("direction"),
					Pin->Direction == EGPD_Input ? TEXT("Input") : TEXT("Output"));
				PinObj->SetNumberField(TEXT("connections"), Pin->LinkedTo.Num());

				for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
				{
					if (LinkedPin && LinkedPin->GetOwningNode())
					{
						TSharedPtr<FJsonObject> ConnObj = MakeShared<FJsonObject>();
						ConnObj->SetStringField(TEXT("from_node"),
							Pin->GetOwningNode()->GetName());
						ConnObj->SetStringField(TEXT("from_pin"),
							Pin->PinName.ToString());
						ConnObj->SetStringField(TEXT("to_node"),
							LinkedPin->GetOwningNode()->GetName());
						ConnObj->SetStringField(TEXT("to_pin"),
							LinkedPin->PinName.ToString());
						ConnectionArray.Add(MakeShared<FJsonValueObject>(ConnObj));
					}
				}

				PinArray.Add(MakeShared<FJsonValueObject>(PinObj));
			}
			NodeObj->SetArrayField(TEXT("pins"), PinArray);
		}

		NodeArray.Add(MakeShared<FJsonValueObject>(NodeObj));
	}

	GraphData->SetArrayField(TEXT("nodes"), NodeArray);
	GraphData->SetArrayField(TEXT("connections"), ConnectionArray);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("blueprint_path"), BlueprintPath);
	ResultObj->SetObjectField(TEXT("graph_data"), GraphData);
	ResultObj->SetBoolField(TEXT("success"), true);

	return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleGetBlueprintVariableDetails(
	const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintPath;
	if (!Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_path' parameter"));
	}

	FString VariableName;
	bool bSpecificVariable = Params->TryGetStringField(TEXT("variable_name"), VariableName);

	UBlueprint* Blueprint = Cast<UBlueprint>(UEditorAssetLibrary::LoadAsset(BlueprintPath));
	if (!Blueprint)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Failed to load blueprint: %s"), *BlueprintPath));
	}

	TArray<TSharedPtr<FJsonValue>> VariableArray;

	for (const FBPVariableDescription& Variable : Blueprint->NewVariables)
	{
		if (bSpecificVariable && Variable.VarName.ToString() != VariableName)
		{
			continue;
		}

		TSharedPtr<FJsonObject> VarObj = MakeShared<FJsonObject>();
		VarObj->SetStringField(TEXT("name"), Variable.VarName.ToString());
		VarObj->SetStringField(TEXT("type"), Variable.VarType.PinCategory.ToString());
		VarObj->SetStringField(TEXT("sub_category"), Variable.VarType.PinSubCategory.ToString());
		VarObj->SetStringField(TEXT("default_value"), Variable.DefaultValue);
		VarObj->SetStringField(TEXT("friendly_name"),
			Variable.FriendlyName.IsEmpty() ? Variable.VarName.ToString() : Variable.FriendlyName);

		FString TooltipValue;
		if (Variable.HasMetaData(FBlueprintMetadata::MD_Tooltip))
		{
			TooltipValue = Variable.GetMetaData(FBlueprintMetadata::MD_Tooltip);
		}
		VarObj->SetStringField(TEXT("tooltip"), TooltipValue);

		VarObj->SetStringField(TEXT("category"), Variable.Category.ToString());

		VarObj->SetBoolField(TEXT("is_editable"), (Variable.PropertyFlags & CPF_Edit) != 0);
		VarObj->SetBoolField(TEXT("is_blueprint_visible"), (Variable.PropertyFlags & CPF_BlueprintVisible) != 0);
		VarObj->SetBoolField(TEXT("is_editable_in_instance"), (Variable.PropertyFlags & CPF_DisableEditOnInstance) == 0);
		VarObj->SetBoolField(TEXT("is_config"), (Variable.PropertyFlags & CPF_Config) != 0);

		VarObj->SetNumberField(TEXT("replication"), (int32)Variable.ReplicationCondition);

		VariableArray.Add(MakeShared<FJsonValueObject>(VarObj));
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("blueprint_path"), BlueprintPath);

	if (bSpecificVariable)
	{
		ResultObj->SetStringField(TEXT("variable_name"), VariableName);
		if (VariableArray.Num() > 0)
		{
			ResultObj->SetObjectField(TEXT("variable"), VariableArray[0]->AsObject());
		}
		else
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Variable not found: %s"), *VariableName));
		}
	}
	else
	{
		ResultObj->SetArrayField(TEXT("variables"), VariableArray);
		ResultObj->SetNumberField(TEXT("variable_count"), VariableArray.Num());
	}

	ResultObj->SetBoolField(TEXT("success"), true);
	return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleGetBlueprintFunctionDetails(
	const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintPath;
	if (!Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_path' parameter"));
	}

	FString FunctionName;
	bool bSpecificFunction = Params->TryGetStringField(TEXT("function_name"), FunctionName);

	bool bIncludeGraph = true;
	Params->TryGetBoolField(TEXT("include_graph"), bIncludeGraph);

	UBlueprint* Blueprint = Cast<UBlueprint>(UEditorAssetLibrary::LoadAsset(BlueprintPath));
	if (!Blueprint)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Failed to load blueprint: %s"), *BlueprintPath));
	}

	TArray<TSharedPtr<FJsonValue>> FunctionArray;

	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (!Graph)
		{
			continue;
		}

		if (bSpecificFunction && Graph->GetName() != FunctionName)
		{
			continue;
		}

		TSharedPtr<FJsonObject> FuncObj = MakeShared<FJsonObject>();
		FuncObj->SetStringField(TEXT("name"), Graph->GetName());
		FuncObj->SetStringField(TEXT("graph_type"), TEXT("Function"));

		TArray<TSharedPtr<FJsonValue>> InputPins;
		TArray<TSharedPtr<FJsonValue>> OutputPins;

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node)
			{
				continue;
			}

			if (Node->GetClass()->GetName().Contains(TEXT("FunctionEntry")))
			{
				for (UEdGraphPin* Pin : Node->Pins)
				{
					if (Pin && Pin->Direction == EGPD_Output && Pin->PinName != TEXT("then"))
					{
						TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
						PinObj->SetStringField(TEXT("name"), Pin->PinName.ToString());
						PinObj->SetStringField(TEXT("type"), Pin->PinType.PinCategory.ToString());
						InputPins.Add(MakeShared<FJsonValueObject>(PinObj));
					}
				}
			}
			else if (Node->GetClass()->GetName().Contains(TEXT("FunctionResult")))
			{
				for (UEdGraphPin* Pin : Node->Pins)
				{
					if (Pin && Pin->Direction == EGPD_Input && Pin->PinName != TEXT("exec"))
					{
						TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
						PinObj->SetStringField(TEXT("name"), Pin->PinName.ToString());
						PinObj->SetStringField(TEXT("type"), Pin->PinType.PinCategory.ToString());
						OutputPins.Add(MakeShared<FJsonValueObject>(PinObj));
					}
				}
			}
		}

		FuncObj->SetArrayField(TEXT("input_parameters"), InputPins);
		FuncObj->SetArrayField(TEXT("output_parameters"), OutputPins);
		FuncObj->SetNumberField(TEXT("node_count"), Graph->Nodes.Num());

		if (bIncludeGraph)
		{
			TArray<TSharedPtr<FJsonValue>> NodeArray;
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (!Node)
				{
					continue;
				}

				TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
				NodeObj->SetStringField(TEXT("name"), Node->GetName());
				NodeObj->SetStringField(TEXT("class"), Node->GetClass()->GetName());
				NodeObj->SetStringField(TEXT("title"),
					Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
				NodeArray.Add(MakeShared<FJsonValueObject>(NodeObj));
			}
			FuncObj->SetArrayField(TEXT("graph_nodes"), NodeArray);
		}

		FunctionArray.Add(MakeShared<FJsonValueObject>(FuncObj));
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("blueprint_path"), BlueprintPath);

	if (bSpecificFunction)
	{
		ResultObj->SetStringField(TEXT("function_name"), FunctionName);
		if (FunctionArray.Num() > 0)
		{
			ResultObj->SetObjectField(TEXT("function"), FunctionArray[0]->AsObject());
		}
		else
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Function not found: %s"), *FunctionName));
		}
	}
	else
	{
		ResultObj->SetArrayField(TEXT("functions"), FunctionArray);
		ResultObj->SetNumberField(TEXT("function_count"), FunctionArray.Num());
	}

	ResultObj->SetBoolField(TEXT("success"), true);
	return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleGetBlueprintClassDefaults(
	const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintPath;
	if (!Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_path' parameter"));
	}

	FString Filter;
	Params->TryGetStringField(TEXT("filter"), Filter);
	const FString FilterLower = Filter.ToLower();

	bool bIncludeInherited = true;
	Params->TryGetBoolField(TEXT("include_inherited"), bIncludeInherited);

	UBlueprint* Blueprint = Cast<UBlueprint>(UEditorAssetLibrary::LoadAsset(BlueprintPath));
	if (!Blueprint)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Failed to load blueprint: %s"), *BlueprintPath));
	}

	UClass* GenClass = Blueprint->GeneratedClass;
	if (!GenClass)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Blueprint has no GeneratedClass — compile the blueprint first"));
	}

	UObject* CDO = GenClass->GetDefaultObject(false);
	if (!CDO)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get Class Default Object"));
	}

	TSharedPtr<FJsonObject> PropsObj = MakeShared<FJsonObject>();

	const EFieldIteratorFlags::SuperClassFlags SuperFlag =
		bIncludeInherited ? EFieldIteratorFlags::IncludeSuper : EFieldIteratorFlags::ExcludeSuper;

	for (TFieldIterator<FProperty> PropIt(GenClass, SuperFlag); PropIt; ++PropIt)
	{
		FProperty* Prop = *PropIt;

		if (!Prop->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible))
		{
			continue;
		}

		const FString PropName = Prop->GetName();

		if (!FilterLower.IsEmpty() && !PropName.ToLower().Contains(FilterLower))
		{
			continue;
		}

		const void* PropAddr = Prop->ContainerPtrToValuePtr<void>(CDO);
		TSharedPtr<FJsonValue> JsonValue = FJsonObjectConverter::UPropertyToJsonValue(Prop, PropAddr);
		if (JsonValue.IsValid())
		{
			PropsObj->SetField(PropName, JsonValue);
		}
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("blueprint_path"), BlueprintPath);
	Result->SetStringField(TEXT("class"), GenClass->GetName());
	Result->SetStringField(TEXT("parent_class"),
		GenClass->GetSuperClass() ? GenClass->GetSuperClass()->GetName() : TEXT("None"));
	Result->SetBoolField(TEXT("include_inherited"), bIncludeInherited);
	Result->SetObjectField(TEXT("defaults"), PropsObj);
	return Result;
}
