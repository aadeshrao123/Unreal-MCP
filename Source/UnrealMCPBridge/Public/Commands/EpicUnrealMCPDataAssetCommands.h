#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for Data Asset MCP commands.
 *
 * Supports any UDataAsset / UPrimaryDataAsset subclass:
 *   - Enumerate available classes
 *   - Create new data assets
 *   - Read ALL FProperty values (including structs, arrays, object refs)
 *   - Write single or bulk properties with full type coverage:
 *       primitives, enums, structs (FGameplayTag, FVector…),
 *       TArray, TMap, UObject* refs, FSoftObjectPath/TSoftObjectPtr
 *   - List data assets in a content path
 */
class UNREALMCPBRIDGE_API FEpicUnrealMCPDataAssetCommands
{
public:
    FEpicUnrealMCPDataAssetCommands();

    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    // list all non-abstract UDataAsset subclasses that are currently loaded
    TSharedPtr<FJsonObject> HandleListDataAssetClasses(const TSharedPtr<FJsonObject>& Params);

    // create a new data asset of any UDataAsset subclass
    TSharedPtr<FJsonObject> HandleCreateDataAsset(const TSharedPtr<FJsonObject>& Params);

    // read ALL FProperties from a data asset (no CPF filter, full hierarchy)
    TSharedPtr<FJsonObject> HandleGetDataAssetProperties(const TSharedPtr<FJsonObject>& Params);

    // set a single named property on a data asset
    TSharedPtr<FJsonObject> HandleSetDataAssetProperty(const TSharedPtr<FJsonObject>& Params);

    // set multiple properties at once  { "properties": { "Name": value, ... } }
    TSharedPtr<FJsonObject> HandleSetDataAssetProperties(const TSharedPtr<FJsonObject>& Params);

    // list data assets in a content path, optionally filtered by class name
    TSharedPtr<FJsonObject> HandleListDataAssets(const TSharedPtr<FJsonObject>& Params);

    // -----------------------------------------------------------------------
    // Internal helpers
    // -----------------------------------------------------------------------

    // Find a loaded UClass by short name, full path, or "/Script/Module.ClassName"
    static UClass* ResolveDataAssetClass(const FString& ClassName);

    // Set one FProperty on an object from a FJsonValue.
    // Covers: primitives, enums, FStructProperty (via FJsonObjectConverter),
    // FArrayProperty, FMapProperty, FSetProperty,
    // FObjectProperty (load by path), FSoftObjectProperty, FSoftClassProperty,
    // FNameProperty, FTextProperty, FDoubleProperty, FInt64Property, etc.
    static bool SetProperty(UObject* Object, const FString& PropertyName,
                            const TSharedPtr<FJsonValue>& Value, FString& OutError);

    // Serialize all FProperties of an object to a JSON object
    static TSharedPtr<FJsonObject> SerializeAllProperties(UObject* Object, const FString& FilterLower,
                                                           bool bIncludeInherited);

    // Apply all JSON fields to a UObject, skipping "_ClassName".
    // Used recursively when deserializing instanced subobjects (e.g. Mass traits).
    static void SetPropertiesFromJson(UObject* Target, const TSharedPtr<FJsonObject>& Json,
                                      FString& OutErrors);
};
