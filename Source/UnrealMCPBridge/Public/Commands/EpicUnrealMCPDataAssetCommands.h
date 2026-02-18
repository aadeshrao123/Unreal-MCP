#pragma once

#include "CoreMinimal.h"
#include "Json.h"
#include "Commands/EpicUnrealMCPPropertyUtils.h"

/**
 * Handler class for Data Asset MCP commands.
 *
 * Supports any UDataAsset / UPrimaryDataAsset subclass:
 *   - Enumerate available classes
 *   - Create new data assets
 *   - Read ALL FProperty values (including structs, arrays, object refs)
 *   - Write single or bulk properties via FEpicUnrealMCPPropertyUtils
 *   - List data assets in a content path
 *   - Enumerate valid types for any property slot (editor dropdown mirror)
 */
class UNREALMCPBRIDGE_API FEpicUnrealMCPDataAssetCommands
{
public:
    FEpicUnrealMCPDataAssetCommands();

    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    TSharedPtr<FJsonObject> HandleListDataAssetClasses(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCreateDataAsset(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetDataAssetProperties(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetDataAssetProperty(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetDataAssetProperties(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleListDataAssets(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetPropertyValidTypes(const TSharedPtr<FJsonObject>& Params);

    // Find a loaded UClass that is a UDataAsset subclass.
    // For other classes use FEpicUnrealMCPPropertyUtils::ResolveAnyClass.
    static UClass* ResolveDataAssetClass(const FString& ClassName);
};
