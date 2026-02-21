#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class FEpicUnrealMCPBlueprintGraphCommands
{
public:
	FEpicUnrealMCPBlueprintGraphCommands();
	~FEpicUnrealMCPBlueprintGraphCommands();

	TSharedPtr<FJsonObject> HandleCommand(
		const FString& CommandType,
		const TSharedPtr<FJsonObject>& Params);

private:
	TSharedPtr<FJsonObject> HandleAddBlueprintNode(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleConnectNodes(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleCreateVariable(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetVariableProperties(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAddEventNode(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleDeleteNode(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetNodeProperty(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleCreateFunction(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAddFunctionInput(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAddFunctionOutput(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleDeleteFunction(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleRenameFunction(const TSharedPtr<FJsonObject>& Params);
};
