#include "Commands/EpicUnrealMCPEditorCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "JsonObjectConverter.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "LevelEditorViewport.h"
#include "ImageUtils.h"
#include "HighResScreenshot.h"
#include "Engine/GameViewportClient.h"
#include "Misc/FileHelper.h"
#include "LevelEditor.h"
#include "IAssetViewport.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Framework/Application/SlateApplication.h"

#include "Windows/AllowWindowsPlatformTypes.h"
#include <Windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#include "GameFramework/Actor.h"
#include "Engine/Selection.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Camera/CameraActor.h"
#include "Components/StaticMeshComponent.h"
#include "EditorSubsystem.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "EditorAssetLibrary.h"
#include "Commands/EpicUnrealMCPBlueprintCommands.h"
#include "EngineUtils.h"

FEpicUnrealMCPEditorCommands::FEpicUnrealMCPEditorCommands()
{
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    // Actor manipulation commands
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
    // Blueprint actor spawning
    else if (CommandType == TEXT("spawn_blueprint_actor"))
    {
        return HandleSpawnBlueprintActor(Params);
    }
    // Level / world queries
    else if (CommandType == TEXT("get_selected_actors"))
    {
        return HandleGetSelectedActors(Params);
    }
    else if (CommandType == TEXT("get_world_info"))
    {
        return HandleGetWorldInfo(Params);
    }
    // Flexible class-based spawn
    else if (CommandType == TEXT("spawn_actor_from_class"))
    {
        return HandleSpawnActorFromClass(Params);
    }
    else if (CommandType == TEXT("get_actor_properties"))
    {
        return HandleGetActorProperties(Params);
    }
    else if (CommandType == TEXT("take_screenshot"))
    {
        return HandleTakeScreenshot(Params);
    }

    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown editor command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleGetActorsInLevel(const TSharedPtr<FJsonObject>& Params)
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

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleFindActorsByName(const TSharedPtr<FJsonObject>& Params)
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

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleSpawnActor(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString ActorType;
    if (!Params->TryGetStringField(TEXT("type"), ActorType))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'type' parameter"));
    }

    // Get actor name (required parameter)
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // Get optional transform parameters
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

    // Create the actor based on type
    AActor* NewActor = nullptr;
    UWorld* World = GEditor->GetEditorWorldContext().World();

    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    // Check if an actor with this name already exists
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor with name '%s' already exists"), *ActorName));
        }
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = *ActorName;

    if (ActorType == TEXT("StaticMeshActor"))
    {
        AStaticMeshActor* NewMeshActor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Location, Rotation, SpawnParams);
        if (NewMeshActor)
        {
            // Check for an optional static_mesh parameter to assign a mesh
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
        NewActor = World->SpawnActor<APointLight>(APointLight::StaticClass(), Location, Rotation, SpawnParams);
    }
    else if (ActorType == TEXT("SpotLight"))
    {
        NewActor = World->SpawnActor<ASpotLight>(ASpotLight::StaticClass(), Location, Rotation, SpawnParams);
    }
    else if (ActorType == TEXT("DirectionalLight"))
    {
        NewActor = World->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(), Location, Rotation, SpawnParams);
    }
    else if (ActorType == TEXT("CameraActor"))
    {
        NewActor = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), Location, Rotation, SpawnParams);
    }
    else
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown actor type: %s"), *ActorType));
    }

    if (NewActor)
    {
        // Set scale (since SpawnActor only takes location and rotation)
        FTransform Transform = NewActor->GetTransform();
        Transform.SetScale3D(Scale);
        NewActor->SetActorTransform(Transform);

        // Return the created actor's details
        return FEpicUnrealMCPCommonUtils::ActorToJsonObject(NewActor, true);
    }

    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create actor"));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleDeleteActor(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            // Store actor info before deletion for the response
            TSharedPtr<FJsonObject> ActorInfo = FEpicUnrealMCPCommonUtils::ActorToJsonObject(Actor);
            
            // Delete the actor
            Actor->Destroy();
            
            TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
            ResultObj->SetObjectField(TEXT("deleted_actor"), ActorInfo);
            return ResultObj;
        }
    }
    
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleSetActorTransform(const TSharedPtr<FJsonObject>& Params)
{
    // Get actor name
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // Find the actor
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
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    // Get transform parameters
    FTransform NewTransform = TargetActor->GetTransform();

    if (Params->HasField(TEXT("location")))
    {
        NewTransform.SetLocation(FEpicUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location")));
    }
    if (Params->HasField(TEXT("rotation")))
    {
        NewTransform.SetRotation(FQuat(FEpicUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"))));
    }
    if (Params->HasField(TEXT("scale")))
    {
        NewTransform.SetScale3D(FEpicUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("scale")));
    }

    // Set the new transform
    TargetActor->SetActorTransform(NewTransform);

    // Return updated actor info
    return FEpicUnrealMCPCommonUtils::ActorToJsonObject(TargetActor, true);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleSpawnBlueprintActor(const TSharedPtr<FJsonObject>& Params)
{
    // This function will now correctly call the implementation in BlueprintCommands
    FEpicUnrealMCPBlueprintCommands BlueprintCommands;
    return BlueprintCommands.HandleCommand(TEXT("spawn_blueprint_actor"), Params);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleGetSelectedActors(const TSharedPtr<FJsonObject>& Params)
{
    TArray<TSharedPtr<FJsonValue>> ActorArray;

    USelection* Selection = GEditor->GetSelectedActors();
    for (FSelectionIterator It(*Selection); It; ++It)
    {
        AActor* Actor = Cast<AActor>(*It);
        if (!Actor) continue;

        TSharedPtr<FJsonObject> ActorObj = MakeShared<FJsonObject>();
        ActorObj->SetStringField(TEXT("name"),  Actor->GetName());
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

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleGetWorldInfo(const TSharedPtr<FJsonObject>& Params)
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
            if (!Actor) continue;
            TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
            Obj->SetStringField(TEXT("name"),  Actor->GetName());
            Obj->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
            ActorArray.Add(MakeShared<FJsonValueObject>(Obj));
        }
        Result->SetArrayField(TEXT("actors"), ActorArray);

        if (AllActors.Num() > 100)
            Result->SetStringField(TEXT("note"),
                FString::Printf(TEXT("Showing 100 of %d actors"), AllActors.Num()));
    }

    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleSpawnActorFromClass(const TSharedPtr<FJsonObject>& Params)
{
    FString ClassName;
    if (!Params->TryGetStringField(TEXT("class_name"), ClassName))
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'class_name' parameter"));

    double LocationX = 0.0, LocationY = 0.0, LocationZ = 0.0;
    double RotYaw = 0.0, RotPitch = 0.0, RotRoll = 0.0;
    Params->TryGetNumberField(TEXT("location_x"),    LocationX);
    Params->TryGetNumberField(TEXT("location_y"),    LocationY);
    Params->TryGetNumberField(TEXT("location_z"),    LocationZ);
    Params->TryGetNumberField(TEXT("rotation_yaw"),  RotYaw);
    Params->TryGetNumberField(TEXT("rotation_pitch"), RotPitch);
    Params->TryGetNumberField(TEXT("rotation_roll"), RotRoll);

    const FVector   Location(LocationX, LocationY, LocationZ);
    const FRotator  Rotation(RotPitch, RotYaw, RotRoll);

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));

    FActorSpawnParameters SpawnParams;
    AActor* NewActor = nullptr;

    // --- Blueprint path (starts with "/") ---
    if (ClassName.StartsWith(TEXT("/")))
    {
        UObject* Asset = UEditorAssetLibrary::LoadAsset(ClassName);
        UBlueprint* Blueprint = Cast<UBlueprint>(Asset);
        if (Blueprint && Blueprint->GeneratedClass)
            NewActor = World->SpawnActor<AActor>(Blueprint->GeneratedClass, Location, Rotation, SpawnParams);
    }

    // --- Native UClass (try common module paths) ---
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
            if (Class) break;
        }
        if (Class && Class->IsChildOf(AActor::StaticClass()))
            NewActor = World->SpawnActor<AActor>(Class, Location, Rotation, SpawnParams);
    }

    if (!NewActor)
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Failed to spawn — class not found or invalid: %s"), *ClassName));

    return FEpicUnrealMCPCommonUtils::ActorToJsonObject(NewActor, true);
}

// ---------------------------------------------------------------------------
// get_actor_properties
// Reads ALL FProperty values from a LIVE world actor instance — not the CDO.
// Captures per-instance overrides set in the editor (e.g. ResourceType on a
// specific BP_ResourceNode placed in the level).
// Also serialises each component's properties if include_components is true.
// ---------------------------------------------------------------------------
static TSharedPtr<FJsonObject> SerializeObjectProperties(UObject* Obj, const FString& FilterLower)
{
    TSharedPtr<FJsonObject> Props = MakeShared<FJsonObject>();
    for (TFieldIterator<FProperty> It(Obj->GetClass(), EFieldIteratorFlags::IncludeSuper); It; ++It)
    {
        FProperty* Prop = *It;
        const FString Name = Prop->GetName();
        if (!FilterLower.IsEmpty() && !Name.ToLower().Contains(FilterLower))
            continue;

        const void* Addr = Prop->ContainerPtrToValuePtr<void>(Obj);
        TSharedPtr<FJsonValue> Val = FJsonObjectConverter::UPropertyToJsonValue(Prop, Addr);
        if (Val.IsValid())
            Props->SetField(Name, Val);
    }
    return Props;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleGetActorProperties(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorLabel;
    if (!Params->TryGetStringField(TEXT("actor_label"), ActorLabel))
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_label' parameter"));

    // Optional substring filter on property names (case-insensitive)
    FString Filter;
    Params->TryGetStringField(TEXT("filter"), Filter);
    const FString FilterLower = Filter.ToLower();

    // Optional: also dump component properties
    bool bIncludeComponents = false;
    Params->TryGetBoolField(TEXT("include_components"), bIncludeComponents);

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));

    // Find actor by label (editor display name) or fall back to internal name
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
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Actor not found in level: '%s'"), *ActorLabel));

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"),      true);
    Result->SetStringField(TEXT("actor_name"), Target->GetName());
    Result->SetStringField(TEXT("actor_label"),Target->GetActorLabel());
    Result->SetStringField(TEXT("class"),      Target->GetClass()->GetName());

    // Actor's own properties (all FProperties, no flag filter)
    Result->SetObjectField(TEXT("properties"), SerializeObjectProperties(Target, FilterLower));

    // Optionally add per-component property maps
    if (bIncludeComponents)
    {
        TArray<UActorComponent*> Components;
        Target->GetComponents(Components);

        TSharedPtr<FJsonObject> CompMap = MakeShared<FJsonObject>();
        for (UActorComponent* Comp : Components)
        {
            if (!Comp) continue;
            const FString CompKey = Comp->GetName() + TEXT(" (") + Comp->GetClass()->GetName() + TEXT(")");
            CompMap->SetObjectField(CompKey, SerializeObjectProperties(Comp, FilterLower));
        }
        Result->SetObjectField(TEXT("components"), CompMap);
    }

    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleTakeScreenshot(const TSharedPtr<FJsonObject>& Params)
{
    // Get optional output path
    FString OutputPath;
    if (!Params->TryGetStringField(TEXT("file_path"), OutputPath) || OutputPath.IsEmpty())
    {
        OutputPath = FPaths::ProjectSavedDir() / TEXT("Screenshots") / TEXT("MCP_Screenshot.png");
    }

    // mode: "viewport" (level viewport only) or "window" (full editor window)
    FString Mode = TEXT("viewport");
    Params->TryGetStringField(TEXT("mode"), Mode);

    // Ensure output directory exists
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutputPath), true);

    TArray<FColor> Pixels;
    int32 Width = 0;
    int32 Height = 0;

    if (Mode == TEXT("window"))
    {
        // ── Window mode: capture the entire active editor window ──
        // Uses Windows API to capture the rendered window content via DWM.
        // Works for widget designer, material editor, blueprint graph, etc.
        TSharedPtr<SWindow> Window = FSlateApplication::Get().GetActiveTopLevelWindow();

        // Fall back: editor may have lost focus (e.g. user is in the terminal).
        // Use the first visible top-level window instead (usually the main editor).
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

        TSharedPtr<FGenericWindow> NativeWindow = Window->GetNativeWindow();
        if (!NativeWindow.IsValid())
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No native OS window handle"));
        }

        HWND Hwnd = reinterpret_cast<HWND>(NativeWindow->GetOSWindowHandle());
        if (!Hwnd)
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Invalid OS window handle"));
        }

        // Get client area dimensions (excludes OS title bar / borders)
        RECT ClientRect;
        ::GetClientRect(Hwnd, &ClientRect);
        Width  = ClientRect.right  - ClientRect.left;
        Height = ClientRect.bottom - ClientRect.top;

        if (Width <= 0 || Height <= 0)
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
                TEXT("Window has invalid size (may be minimized)"));
        }

        // Create a memory DC + bitmap to receive the capture
        HDC WindowDC = ::GetDC(Hwnd);
        HDC MemDC    = ::CreateCompatibleDC(WindowDC);
        HBITMAP HBitmap   = ::CreateCompatibleBitmap(WindowDC, Width, Height);
        HBITMAP OldBitmap = static_cast<HBITMAP>(::SelectObject(MemDC, HBitmap));

        // PrintWindow with PW_RENDERFULLCONTENT captures D3D/DWM content
        const UINT PW_RENDERFULLCONTENT_FLAG = 0x00000002;
        BOOL bCaptured = ::PrintWindow(Hwnd, MemDC, PW_RENDERFULLCONTENT_FLAG);

        if (!bCaptured)
        {
            // Fall back to BitBlt from window DC
            ::BitBlt(MemDC, 0, 0, Width, Height, WindowDC, 0, 0, SRCCOPY);
        }

        // Read pixel data from the bitmap (top-down BGRA)
        BITMAPINFOHEADER BMI = {};
        BMI.biSize        = sizeof(BITMAPINFOHEADER);
        BMI.biWidth       = Width;
        BMI.biHeight      = -Height;  // Negative = top-down
        BMI.biPlanes      = 1;
        BMI.biBitCount    = 32;
        BMI.biCompression = BI_RGB;

        Pixels.SetNum(Width * Height);
        ::GetDIBits(MemDC, HBitmap, 0, Height, Pixels.GetData(),
                    reinterpret_cast<BITMAPINFO*>(&BMI), DIB_RGB_COLORS);

        // Cleanup GDI objects
        ::SelectObject(MemDC, OldBitmap);
        ::DeleteObject(HBitmap);
        ::DeleteDC(MemDC);
        ::ReleaseDC(Hwnd, WindowDC);

        if (Pixels.Num() == 0)
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
                TEXT("Failed to capture editor window"));
        }
    }
    else
    {
        // ── Viewport mode (default): capture the active level viewport ──
        FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
        TSharedPtr<IAssetViewport> ActiveViewport = LevelEditor.GetFirstActiveViewport();
        if (!ActiveViewport.IsValid())
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No active level viewport found"));
        }

        FViewport* Viewport = ActiveViewport->GetActiveViewport();
        if (!Viewport)
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get viewport render target"));
        }

        const FIntPoint Size = Viewport->GetSizeXY();
        if (Size.X <= 0 || Size.Y <= 0)
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
                TEXT("Viewport has invalid size (may be minimized or not yet rendered)"));
        }

        if (!Viewport->ReadPixels(Pixels))
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to read pixels from viewport"));
        }

        Width = Size.X;
        Height = Size.Y;
    }

    // Fix alpha channel — both viewport and Slate may return alpha = 0
    for (FColor& Pixel : Pixels)
    {
        Pixel.A = 255;
    }

    // Compress to PNG via IImageWrapper
    IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
    TSharedPtr<IImageWrapper> PngWriter = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
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

    // Return result with absolute path so callers can read the file directly
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("file_path"), FPaths::ConvertRelativePathToFull(OutputPath));
    Result->SetNumberField(TEXT("width"), Width);
    Result->SetNumberField(TEXT("height"), Height);
    return Result;
}
