#pragma once

#include "CoreMinimal.h"
#include "Json.h"
#include "Commands/EpicUnrealMCPPropertyUtils.h"

/**
 * Handler for Data Asset MCP commands.
 * Supports any UDataAsset / UPrimaryDataAsset subclass.
 */
class UNREALMCPBRIDGE_API FEpicUnrealMCPDataAssetCommands
{
public:
	FEpicUnrealMCPDataAssetCommands();

	TSharedPtr<FJsonObject> HandleCommand(
		const FString& CommandType,
		const TSharedPtr<FJsonObject>& Params);

private:
	TSharedPtr<FJsonObject> HandleListDataAssetClasses(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleCreateDataAsset(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetDataAssetProperties(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetDataAssetProperty(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetDataAssetProperties(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleListDataAssets(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetPropertyValidTypes(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSearchClassPaths(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetMassConfigTraits(const TSharedPtr<FJsonObject>& Params);

	// Limited to UDataAsset subclasses. For any class use PU::ResolveAnyClass.
	static UClass* ResolveDataAssetClass(const FString& ClassName);
};
