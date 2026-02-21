#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler for asset management MCP commands.
 * Covers: find, list, open, inspect, modify, delete, save, import, duplicate, rename, sync.
 */
class UNREALMCPBRIDGE_API FEpicUnrealMCPAssetCommands
{
public:
	FEpicUnrealMCPAssetCommands();

	TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
	TSharedPtr<FJsonObject> HandleFindAssets(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleListAssets(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleOpenAsset(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetAssetInfo(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetAssetProperties(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetAssetProperty(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleFindReferences(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleDuplicateAsset(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleRenameAsset(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleDeleteAsset(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSaveAsset(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSaveAll(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleImportAsset(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleImportAssetsBatch(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetSelectedAssets(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSyncBrowser(const TSharedPtr<FJsonObject>& Params);

	static bool ResolveClassPath(const FString& ClassType, FString& OutPackagePath, FString& OutClassName);
};
