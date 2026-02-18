#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for Editor-related MCP commands
 * Handles viewport control, actor manipulation, and level management
 */
class UNREALMCPBRIDGE_API FEpicUnrealMCPEditorCommands
{
public:
    	FEpicUnrealMCPEditorCommands();

    // Handle editor commands
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    // Actor manipulation commands
    TSharedPtr<FJsonObject> HandleGetActorsInLevel(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleFindActorsByName(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSpawnActor(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleDeleteActor(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetActorTransform(const TSharedPtr<FJsonObject>& Params);

    // Blueprint actor spawning
    TSharedPtr<FJsonObject> HandleSpawnBlueprintActor(const TSharedPtr<FJsonObject>& Params);

    // Level / world queries
    TSharedPtr<FJsonObject> HandleGetSelectedActors(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetWorldInfo(const TSharedPtr<FJsonObject>& Params);

    // Flexible actor spawn from class name or blueprint path
    TSharedPtr<FJsonObject> HandleSpawnActorFromClass(const TSharedPtr<FJsonObject>& Params);
};