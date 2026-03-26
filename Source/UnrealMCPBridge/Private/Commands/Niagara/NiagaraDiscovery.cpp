#include "Commands/EpicUnrealMCPNiagaraCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "NiagaraHelpers.h"

#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraGraph.h"
#include "NiagaraDataInterface.h"
#include "NiagaraTypes.h"
#include "NiagaraCommon.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "UObject/UObjectIterator.h"

// ---------------------------------------------------------------------------
// HandleListNiagaraModules
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleListNiagaraModules(
	const TSharedPtr<FJsonObject>& Params)
{
	FString Category = TEXT("all");
	Params->TryGetStringField(TEXT("category"), Category);

	FString Search;
	Params->TryGetStringField(TEXT("search"), Search);

	// Hardcoded catalog of common built-in Niagara modules
	struct FModuleEntry
	{
		const TCHAR* Path;
		const TCHAR* Category;
		const TCHAR* Name;
		const TCHAR* Description;
	};

	static const FModuleEntry BuiltInModules[] =
	{
		// Emitter lifecycle
		{ TEXT("/Niagara/Modules/Emitter/EmitterState.EmitterState"), TEXT("emitter"), TEXT("Emitter State"), TEXT("Controls emitter lifecycle state") },
		{ TEXT("/Niagara/Modules/Emitter/SpawnRate.SpawnRate"), TEXT("emitter"), TEXT("Spawn Rate"), TEXT("Sets particle spawn rate") },
		{ TEXT("/Niagara/Modules/Emitter/SpawnBurstInstantaneous.SpawnBurstInstantaneous"), TEXT("emitter"), TEXT("Spawn Burst Instantaneous"), TEXT("Spawns particles in a single burst") },
		{ TEXT("/Niagara/Modules/Emitter/SpawnPerUnit.SpawnPerUnit"), TEXT("emitter"), TEXT("Spawn Per Unit"), TEXT("Spawns particles based on distance traveled") },

		// Spawn - Location
		{ TEXT("/Niagara/Modules/Spawn/Location/SystemLocation.SystemLocation"), TEXT("spawn"), TEXT("System Location"), TEXT("Sets particle spawn location to system location") },
		{ TEXT("/Niagara/Modules/Spawn/Location/SphereLocation.SphereLocation"), TEXT("spawn"), TEXT("Sphere Location"), TEXT("Random location within sphere") },
		{ TEXT("/Niagara/Modules/Spawn/Location/BoxLocation.BoxLocation"), TEXT("spawn"), TEXT("Box Location"), TEXT("Random location within box") },
		{ TEXT("/Niagara/Modules/Spawn/Location/CylinderLocation.CylinderLocation"), TEXT("spawn"), TEXT("Cylinder Location"), TEXT("Random location within cylinder") },
		{ TEXT("/Niagara/Modules/Spawn/Location/TorusLocation.TorusLocation"), TEXT("spawn"), TEXT("Torus Location"), TEXT("Random location on torus surface") },
		{ TEXT("/Niagara/Modules/Spawn/Location/ConeLocation.ConeLocation"), TEXT("spawn"), TEXT("Cone Location"), TEXT("Random location within cone") },
		{ TEXT("/Niagara/Modules/Spawn/Location/RingLocation.RingLocation"), TEXT("spawn"), TEXT("Ring Location"), TEXT("Random location on ring") },
		{ TEXT("/Niagara/Modules/Spawn/Location/DiscLocation.DiscLocation"), TEXT("spawn"), TEXT("Disc Location"), TEXT("Random location on disc") },
		{ TEXT("/Niagara/Modules/Spawn/Location/GridLocation.GridLocation"), TEXT("spawn"), TEXT("Grid Location"), TEXT("Location on grid") },

		// Spawn - Velocity
		{ TEXT("/Niagara/Modules/Spawn/Velocity/AddVelocity.AddVelocity"), TEXT("spawn"), TEXT("Add Velocity"), TEXT("Adds velocity to particles") },
		{ TEXT("/Niagara/Modules/Spawn/Velocity/AddVelocityInCone.AddVelocityInCone"), TEXT("spawn"), TEXT("Add Velocity In Cone"), TEXT("Random velocity within cone") },
		{ TEXT("/Niagara/Modules/Spawn/Velocity/AddVelocityFromPoint.AddVelocityFromPoint"), TEXT("spawn"), TEXT("Add Velocity From Point"), TEXT("Velocity away from point") },
		{ TEXT("/Niagara/Modules/Spawn/Velocity/InheritVelocity.InheritVelocity"), TEXT("spawn"), TEXT("Inherit Velocity"), TEXT("Inherit velocity from parent") },

		// Spawn - Initialize
		{ TEXT("/Niagara/Modules/Spawn/Initialize/InitializeParticle.InitializeParticle"), TEXT("spawn"), TEXT("Initialize Particle"), TEXT("Sets initial particle properties") },
		{ TEXT("/Niagara/Modules/Spawn/Size/InitialSize.InitialSize"), TEXT("spawn"), TEXT("Initial Size"), TEXT("Sets initial particle size") },
		{ TEXT("/Niagara/Modules/Spawn/Mass/InitialMass.InitialMass"), TEXT("spawn"), TEXT("Initial Mass"), TEXT("Sets initial particle mass") },
		{ TEXT("/Niagara/Modules/Spawn/Lifetime/SetLifetime.SetLifetime"), TEXT("spawn"), TEXT("Set Lifetime"), TEXT("Sets particle lifetime") },

		// Spawn - Rotation
		{ TEXT("/Niagara/Modules/Spawn/Rotation/InitialMeshOrientation.InitialMeshOrientation"), TEXT("spawn"), TEXT("Initial Mesh Orientation"), TEXT("Sets initial mesh rotation") },

		// Update
		{ TEXT("/Niagara/Modules/Update/Lifetime/UpdateAge.UpdateAge"), TEXT("update"), TEXT("Update Age"), TEXT("Updates particle age") },
		{ TEXT("/Niagara/Modules/Update/Color/Color.Color"), TEXT("update"), TEXT("Color"), TEXT("Sets particle color over lifetime") },
		{ TEXT("/Niagara/Modules/Update/Color/ScaleColor.ScaleColor"), TEXT("update"), TEXT("Scale Color"), TEXT("Scales particle color") },
		{ TEXT("/Niagara/Modules/Update/Size/ScaleSize.ScaleSize"), TEXT("update"), TEXT("Scale Size"), TEXT("Scales particle size over lifetime") },
		{ TEXT("/Niagara/Modules/Update/Size/ScaleSizeBySpeed.ScaleSizeBySpeed"), TEXT("update"), TEXT("Scale Size By Speed"), TEXT("Scales size based on velocity") },
		{ TEXT("/Niagara/Modules/Update/Rotation/UpdateMeshOrientation.UpdateMeshOrientation"), TEXT("update"), TEXT("Update Mesh Orientation"), TEXT("Updates mesh rotation") },
		{ TEXT("/Niagara/Modules/Update/Orientation/SpriteFacingAndAlignment.SpriteFacingAndAlignment"), TEXT("update"), TEXT("Sprite Facing And Alignment"), TEXT("Controls sprite billboard mode") },

		// Forces
		{ TEXT("/Niagara/Modules/Forces/Gravity.Gravity"), TEXT("forces"), TEXT("Gravity"), TEXT("Applies gravity force") },
		{ TEXT("/Niagara/Modules/Forces/Drag.Drag"), TEXT("forces"), TEXT("Drag"), TEXT("Applies drag force") },
		{ TEXT("/Niagara/Modules/Forces/Wind.Wind"), TEXT("forces"), TEXT("Wind"), TEXT("Applies wind force") },
		{ TEXT("/Niagara/Modules/Forces/PointForce.PointForce"), TEXT("forces"), TEXT("Point Force"), TEXT("Force from a point") },
		{ TEXT("/Niagara/Modules/Forces/VortexForce.VortexForce"), TEXT("forces"), TEXT("Vortex Force"), TEXT("Rotating vortex force") },
		{ TEXT("/Niagara/Modules/Forces/CurlNoiseForce.CurlNoiseForce"), TEXT("forces"), TEXT("Curl Noise Force"), TEXT("Curl noise turbulence") },

		// Solver
		{ TEXT("/Niagara/Modules/Solvers/SolveForcesAndVelocity.SolveForcesAndVelocity"), TEXT("update"), TEXT("Solve Forces And Velocity"), TEXT("Integrates forces to update position") },

		// Collision
		{ TEXT("/Niagara/Modules/Collision/CollisionQuery.CollisionQuery"), TEXT("update"), TEXT("Collision Query"), TEXT("Performs collision detection") },
	};

	TArray<TSharedPtr<FJsonValue>> ModulesArr;

	FString LowerCategory = Category.ToLower();
	FString LowerSearch = Search.ToLower();

	for (const FModuleEntry& Module : BuiltInModules)
	{
		// Category filter
		if (LowerCategory != TEXT("all"))
		{
			FString ModCat = Module.Category;
			if (!ModCat.Equals(Category, ESearchCase::IgnoreCase))
			{
				continue;
			}
		}

		// Search filter
		if (!LowerSearch.IsEmpty())
		{
			FString ModName = Module.Name;
			FString ModPath = Module.Path;
			if (!ModName.Contains(Search, ESearchCase::IgnoreCase) &&
				!ModPath.Contains(Search, ESearchCase::IgnoreCase))
			{
				continue;
			}
		}

		auto ModObj = MakeShared<FJsonObject>();
		ModObj->SetStringField(TEXT("name"), Module.Name);
		ModObj->SetStringField(TEXT("path"), Module.Path);
		ModObj->SetStringField(TEXT("category"), Module.Category);
		ModObj->SetStringField(TEXT("description"), Module.Description);
		ModulesArr.Add(MakeShared<FJsonValueObject>(ModObj));
	}

	// Also search Asset Registry for any project-local NiagaraScript modules
	if (!Search.IsEmpty())
	{
		IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		FARFilter Filter;
		Filter.ClassPaths.Add(UNiagaraScript::StaticClass()->GetClassPathName());
		Filter.bRecursivePaths = true;
		Filter.PackagePaths.Add(FName(TEXT("/Game")));

		TArray<FAssetData> Assets;
		AssetRegistry.GetAssets(Filter, Assets);

		for (const FAssetData& Asset : Assets)
		{
			FString AssetName = Asset.AssetName.ToString();
			FString AssetPath = Asset.GetObjectPathString();

			if (AssetName.Contains(Search, ESearchCase::IgnoreCase))
			{
				auto ModObj = MakeShared<FJsonObject>();
				ModObj->SetStringField(TEXT("name"), AssetName);
				ModObj->SetStringField(TEXT("path"), AssetPath);
				ModObj->SetStringField(TEXT("category"), TEXT("project"));
				ModObj->SetStringField(TEXT("description"), TEXT("Project module script"));
				ModulesArr.Add(MakeShared<FJsonValueObject>(ModObj));
			}
		}
	}

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("category_filter"), Category);
	Result->SetStringField(TEXT("search_filter"), Search);
	Result->SetArrayField(TEXT("modules"), ModulesArr);
	Result->SetNumberField(TEXT("count"), ModulesArr.Num());
	return Result;
}

// ---------------------------------------------------------------------------
// HandleListNiagaraEmitterTemplates
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleListNiagaraEmitterTemplates(
	const TSharedPtr<FJsonObject>& Params)
{
	FString Category = TEXT("all");
	Params->TryGetStringField(TEXT("category"), Category);

	struct FTemplateEntry
	{
		const TCHAR* Path;
		const TCHAR* Name;
		const TCHAR* Category;
		const TCHAR* Description;
	};

	static const FTemplateEntry Templates[] =
	{
		// Sprites
		{ TEXT("/Niagara/DefaultAssets/Templates/Emitters/SimpleSpriteBurst"), TEXT("Simple Sprite Burst"), TEXT("sprites"), TEXT("A simple burst of sprites") },
		{ TEXT("/Niagara/DefaultAssets/Templates/Emitters/Fountain"), TEXT("Fountain"), TEXT("sprites"), TEXT("A continuous fountain of particles") },
		{ TEXT("/Niagara/DefaultAssets/Templates/Emitters/OmnidirectionalBurst"), TEXT("Omnidirectional Burst"), TEXT("sprites"), TEXT("Particles burst in all directions") },
		{ TEXT("/Niagara/DefaultAssets/Templates/Emitters/DirectionalBurst"), TEXT("Directional Burst"), TEXT("sprites"), TEXT("Particles burst in a specific direction") },
		{ TEXT("/Niagara/DefaultAssets/Templates/Emitters/ConfettiBurst"), TEXT("Confetti Burst"), TEXT("sprites"), TEXT("Confetti-style particle burst") },
		{ TEXT("/Niagara/DefaultAssets/Templates/Emitters/HangingParticulates"), TEXT("Hanging Particulates"), TEXT("sprites"), TEXT("Floating dust-like particles") },
		{ TEXT("/Niagara/DefaultAssets/Templates/Emitters/BlowingParticles"), TEXT("Blowing Particles"), TEXT("sprites"), TEXT("Particles blown by wind") },
		{ TEXT("/Niagara/DefaultAssets/Templates/Emitters/SingleLoopingParticle"), TEXT("Single Looping Particle"), TEXT("sprites"), TEXT("A single particle that loops") },
		{ TEXT("/Niagara/DefaultAssets/Templates/Emitters/Minimal"), TEXT("Minimal"), TEXT("sprites"), TEXT("Minimal emitter setup for customization") },
		{ TEXT("/Niagara/DefaultAssets/Templates/Emitters/RecycleParticlesInView"), TEXT("Recycle Particles In View"), TEXT("sprites"), TEXT("Particles recycled when in camera view") },

		// Meshes
		{ TEXT("/Niagara/DefaultAssets/Templates/Emitters/UpwardMeshBurst"), TEXT("Upward Mesh Burst"), TEXT("meshes"), TEXT("Mesh particles burst upward") },

		// Ribbons
		{ TEXT("/Niagara/DefaultAssets/Templates/Emitters/LocationBasedRibbon"), TEXT("Location Based Ribbon"), TEXT("ribbons"), TEXT("Ribbon that follows a path") },

		// Beams
		{ TEXT("/Niagara/DefaultAssets/Templates/Emitters/StaticBeam"), TEXT("Static Beam"), TEXT("beams"), TEXT("A static beam effect") },
		{ TEXT("/Niagara/DefaultAssets/Templates/Emitters/DynamicBeam"), TEXT("Dynamic Beam"), TEXT("beams"), TEXT("A dynamic, animated beam") },

		// Behavior examples
		{ TEXT("/Niagara/DefaultAssets/Templates/BehaviorExamples/GridLocation"), TEXT("Grid Location"), TEXT("behaviors"), TEXT("Particles spawned in a grid pattern") },
		{ TEXT("/Niagara/DefaultAssets/Templates/BehaviorExamples/MeshOrientation"), TEXT("Mesh Orientation"), TEXT("behaviors"), TEXT("Example of mesh particle orientation") },
		{ TEXT("/Niagara/DefaultAssets/Templates/BehaviorExamples/MeshRotationForce"), TEXT("Mesh Rotation Force"), TEXT("behaviors"), TEXT("Mesh particles with rotation") },
		{ TEXT("/Niagara/DefaultAssets/Templates/BehaviorExamples/SpriteFacingAndAlignment"), TEXT("Sprite Facing And Alignment"), TEXT("behaviors"), TEXT("Sprite billboard and alignment options") },
		{ TEXT("/Niagara/DefaultAssets/Templates/BehaviorExamples/SubUVAnimation"), TEXT("SubUV Animation"), TEXT("behaviors"), TEXT("Sprite sheet animation") },
		{ TEXT("/Niagara/DefaultAssets/Templates/BehaviorExamples/RibbonID"), TEXT("Ribbon ID"), TEXT("behaviors"), TEXT("Multiple ribbon trails example") },
		{ TEXT("/Niagara/DefaultAssets/Templates/BehaviorExamples/RibbonShapes"), TEXT("Ribbon Shapes"), TEXT("behaviors"), TEXT("Different ribbon shape modes") },
		{ TEXT("/Niagara/DefaultAssets/Templates/BehaviorExamples/PlayAudio"), TEXT("Play Audio"), TEXT("behaviors"), TEXT("Audio playback with particles") },
		{ TEXT("/Niagara/DefaultAssets/Templates/BehaviorExamples/KillParticles"), TEXT("Kill Particles"), TEXT("behaviors"), TEXT("Particle death conditions") },
		{ TEXT("/Niagara/DefaultAssets/Templates/BehaviorExamples/SpawnGroups"), TEXT("Spawn Groups"), TEXT("behaviors"), TEXT("Grouped particle spawning") },
	};

	TArray<TSharedPtr<FJsonValue>> TemplatesArr;

	for (const FTemplateEntry& Entry : Templates)
	{
		if (!Category.Equals(TEXT("all"), ESearchCase::IgnoreCase) &&
			!FString(Entry.Category).Equals(Category, ESearchCase::IgnoreCase))
		{
			continue;
		}

		auto Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("path"), Entry.Path);
		Obj->SetStringField(TEXT("name"), Entry.Name);
		Obj->SetStringField(TEXT("category"), Entry.Category);
		Obj->SetStringField(TEXT("description"), Entry.Description);
		TemplatesArr.Add(MakeShared<FJsonValueObject>(Obj));
	}

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("category_filter"), Category);
	Result->SetArrayField(TEXT("templates"), TemplatesArr);
	Result->SetNumberField(TEXT("count"), TemplatesArr.Num());
	return Result;
}

// ---------------------------------------------------------------------------
// HandleListNiagaraDataInterfaces
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleListNiagaraDataInterfaces(
	const TSharedPtr<FJsonObject>& Params)
{
	FString Filter;
	Params->TryGetStringField(TEXT("filter"), Filter);

	TArray<UClass*> DerivedClasses;
	GetDerivedClasses(UNiagaraDataInterface::StaticClass(), DerivedClasses, true);

	TArray<TSharedPtr<FJsonValue>> InterfacesArr;

	for (UClass* Class : DerivedClasses)
	{
		if (!Class || Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated))
		{
			continue;
		}

		FString ClassName = Class->GetName();

		// Apply filter
		if (!Filter.IsEmpty() && !ClassName.Contains(Filter, ESearchCase::IgnoreCase))
		{
			continue;
		}

		auto Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("class_name"), ClassName);
		Obj->SetStringField(TEXT("class_path"), Class->GetPathName());

		// Extract a readable name by removing common prefixes
		FString DisplayName = ClassName;
		DisplayName.RemoveFromStart(TEXT("UNiagaraDataInterface"));
		DisplayName.RemoveFromStart(TEXT("NiagaraDataInterface"));
		if (DisplayName.IsEmpty())
		{
			DisplayName = ClassName;
		}
		Obj->SetStringField(TEXT("display_name"), DisplayName);

		InterfacesArr.Add(MakeShared<FJsonValueObject>(Obj));
	}

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("filter"), Filter);
	Result->SetArrayField(TEXT("data_interfaces"), InterfacesArr);
	Result->SetNumberField(TEXT("count"), InterfacesArr.Num());
	return Result;
}

// ---------------------------------------------------------------------------
// HandleListNiagaraParameterTypes
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleListNiagaraParameterTypes(
	const TSharedPtr<FJsonObject>& Params)
{
	FString Scope = TEXT("all");
	Params->TryGetStringField(TEXT("scope"), Scope);

	// Common Niagara parameter types available for user parameters
	struct FTypeEntry
	{
		const TCHAR* Name;
		const TCHAR* Description;
		const TCHAR* Scope;
	};

	static const FTypeEntry Types[] =
	{
		// Primitive types (available in all scopes)
		{ TEXT("float"), TEXT("32-bit floating point"), TEXT("all") },
		{ TEXT("int32"), TEXT("32-bit signed integer"), TEXT("all") },
		{ TEXT("bool"), TEXT("Boolean value"), TEXT("all") },

		// Vector types
		{ TEXT("Vector2D"), TEXT("2D vector (X, Y)"), TEXT("all") },
		{ TEXT("Vector"), TEXT("3D vector (X, Y, Z)"), TEXT("all") },
		{ TEXT("Vector4"), TEXT("4D vector (X, Y, Z, W)"), TEXT("all") },

		// Color
		{ TEXT("LinearColor"), TEXT("Linear color (R, G, B, A)"), TEXT("all") },

		// Transform
		{ TEXT("Quat"), TEXT("Quaternion rotation"), TEXT("all") },
		{ TEXT("Matrix"), TEXT("4x4 matrix"), TEXT("particle") },
		{ TEXT("Position"), TEXT("World-space position (LWC)"), TEXT("all") },

		// Niagara-specific
		{ TEXT("NiagaraID"), TEXT("Niagara particle ID"), TEXT("particle") },
		{ TEXT("NiagaraSpawnInfo"), TEXT("Spawn info struct"), TEXT("emitter") },
		{ TEXT("NiagaraParameterMap"), TEXT("Parameter map"), TEXT("system") },

		// Enums
		{ TEXT("ENiagaraExecutionState"), TEXT("Emitter execution state enum"), TEXT("emitter") },
		{ TEXT("ENiagaraCoordinateSpace"), TEXT("Coordinate space enum"), TEXT("all") },
		{ TEXT("ENiagaraOrientationAxis"), TEXT("Orientation axis enum"), TEXT("all") },

		// Data interfaces (user parameter scope)
		{ TEXT("Texture2D"), TEXT("2D texture reference"), TEXT("user") },
		{ TEXT("TextureCube"), TEXT("Cube texture reference"), TEXT("user") },
		{ TEXT("StaticMesh"), TEXT("Static mesh reference"), TEXT("user") },
		{ TEXT("SkeletalMesh"), TEXT("Skeletal mesh reference"), TEXT("user") },
	};

	TArray<TSharedPtr<FJsonValue>> TypesArr;
	FString LowerScope = Scope.ToLower();

	for (const FTypeEntry& Entry : Types)
	{
		if (LowerScope != TEXT("all"))
		{
			FString EntryScope = Entry.Scope;
			if (!EntryScope.Equals(TEXT("all"), ESearchCase::IgnoreCase) &&
				!EntryScope.Equals(Scope, ESearchCase::IgnoreCase))
			{
				continue;
			}
		}

		auto Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("name"), Entry.Name);
		Obj->SetStringField(TEXT("description"), Entry.Description);
		Obj->SetStringField(TEXT("scope"), Entry.Scope);
		TypesArr.Add(MakeShared<FJsonValueObject>(Obj));
	}

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("scope_filter"), Scope);
	Result->SetArrayField(TEXT("types"), TypesArr);
	Result->SetNumberField(TEXT("count"), TypesArr.Num());
	return Result;
}

// ---------------------------------------------------------------------------
// HandleGetNiagaraEmitterAttributes
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleGetNiagaraEmitterAttributes(
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

	FString Filter;
	Params->TryGetStringField(TEXT("filter"), Filter);

	FVersionedNiagaraEmitterData* EmitterData = NiagaraHelpers::GetEmitterData(Handle);
	if (!EmitterData)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Emitter has no data"));
	}

	// Gather attributes from scripts (particle, emitter, system scopes)
	struct FScopeInfo
	{
		const TCHAR* ScopeName;
		ENiagaraScriptUsage Usage;
	};

	static const FScopeInfo Scopes[] =
	{
		{ TEXT("particle_spawn"), ENiagaraScriptUsage::ParticleSpawnScript },
		{ TEXT("particle_update"), ENiagaraScriptUsage::ParticleUpdateScript },
		{ TEXT("emitter_spawn"), ENiagaraScriptUsage::EmitterSpawnScript },
		{ TEXT("emitter_update"), ENiagaraScriptUsage::EmitterUpdateScript },
	};

	TArray<TSharedPtr<FJsonValue>> AttributesArr;

#if WITH_EDITORONLY_DATA
	for (const FScopeInfo& ScopeInfo : Scopes)
	{
		UNiagaraScript* Script = nullptr;

		switch (ScopeInfo.Usage)
		{
		case ENiagaraScriptUsage::ParticleSpawnScript:
			Script = EmitterData->SpawnScriptProps.Script;
			break;
		case ENiagaraScriptUsage::ParticleUpdateScript:
			Script = EmitterData->UpdateScriptProps.Script;
			break;
		case ENiagaraScriptUsage::EmitterSpawnScript:
			Script = EmitterData->EmitterSpawnScriptProps.Script;
			break;
		case ENiagaraScriptUsage::EmitterUpdateScript:
			Script = EmitterData->EmitterUpdateScriptProps.Script;
			break;
		default:
			break;
		}

		if (!Script)
		{
			continue;
		}

		// Get attribute parameters from the compiled data if available
		TArrayView<const FNiagaraVariableWithOffset> RapidIterParams = Script->RapidIterationParameters.ReadParameterVariables();
		for (const FNiagaraVariableWithOffset& Var : RapidIterParams)
		{
			FString AttrName = Var.GetName().ToString();
			if (!Filter.IsEmpty() && !AttrName.Contains(Filter, ESearchCase::IgnoreCase))
			{
				continue;
			}
			auto AttrObj = MakeShared<FJsonObject>();
			AttrObj->SetStringField(TEXT("name"), AttrName);
			AttrObj->SetStringField(TEXT("type"), Var.GetType().GetName());
			AttrObj->SetStringField(TEXT("scope"), ScopeInfo.ScopeName);
			AttrObj->SetStringField(TEXT("source"), TEXT("rapid_iteration"));
			AttributesArr.Add(MakeShared<FJsonValueObject>(AttrObj));
		}
	}
#endif

	// Also add well-known default particle attributes
	struct FWellKnownAttr
	{
		const TCHAR* Name;
		const TCHAR* Type;
	};

	static const FWellKnownAttr WellKnown[] =
	{
		{ TEXT("Particles.Position"), TEXT("Vector") },
		{ TEXT("Particles.Velocity"), TEXT("Vector") },
		{ TEXT("Particles.Color"), TEXT("LinearColor") },
		{ TEXT("Particles.SpriteSize"), TEXT("Vector2D") },
		{ TEXT("Particles.SpriteRotation"), TEXT("float") },
		{ TEXT("Particles.Scale"), TEXT("Vector") },
		{ TEXT("Particles.Lifetime"), TEXT("float") },
		{ TEXT("Particles.Age"), TEXT("float") },
		{ TEXT("Particles.NormalizedAge"), TEXT("float") },
		{ TEXT("Particles.Mass"), TEXT("float") },
		{ TEXT("Particles.MeshOrientation"), TEXT("Quat") },
		{ TEXT("Particles.UniqueID"), TEXT("int32") },
		{ TEXT("Particles.RibbonID"), TEXT("NiagaraID") },
		{ TEXT("Particles.RibbonWidth"), TEXT("float") },
		{ TEXT("Particles.RibbonFacing"), TEXT("Vector") },
		{ TEXT("Particles.RibbonTwist"), TEXT("float") },
		{ TEXT("Particles.MaterialRandom"), TEXT("float") },
		{ TEXT("Particles.DynamicMaterialParameter"), TEXT("Vector4") },
		{ TEXT("Particles.DynamicMaterialParameter1"), TEXT("Vector4") },
		{ TEXT("Particles.SubImageIndex"), TEXT("float") },
		{ TEXT("Particles.CameraOffset"), TEXT("float") },
		{ TEXT("Particles.LightRadius"), TEXT("float") },
		{ TEXT("Particles.LightExponent"), TEXT("float") },
		{ TEXT("Particles.LightVolumetricScattering"), TEXT("float") },
	};

	for (const FWellKnownAttr& Attr : WellKnown)
	{
		if (!Filter.IsEmpty() && !FString(Attr.Name).Contains(Filter, ESearchCase::IgnoreCase))
		{
			continue;
		}
		auto AttrObj = MakeShared<FJsonObject>();
		AttrObj->SetStringField(TEXT("name"), Attr.Name);
		AttrObj->SetStringField(TEXT("type"), Attr.Type);
		AttrObj->SetStringField(TEXT("scope"), TEXT("particle"));
		AttrObj->SetStringField(TEXT("source"), TEXT("well_known"));
		AttributesArr.Add(MakeShared<FJsonValueObject>(AttrObj));
	}

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("emitter_name"), EmitterName);
	Result->SetArrayField(TEXT("attributes"), AttributesArr);
	Result->SetNumberField(TEXT("count"), AttributesArr.Num());
	return Result;
}
