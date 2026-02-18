#pragma once

#include "CoreMinimal.h"
#include "Json.h"

class UMaterialExpression;

/**
 * Handler class for Material-related MCP commands.
 * Implements native C++ material creation, graph building, and inspection
 * without going through the Python scripting plugin.
 */
class UNREALMCPBRIDGE_API FEpicUnrealMCPMaterialCommands
{
public:
	FEpicUnrealMCPMaterialCommands();

	TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
	TSharedPtr<FJsonObject> HandleCreateMaterial(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleCreateMaterialInstance(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleBuildMaterialGraph(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetMaterialInfo(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleRecompileMaterial(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetMaterialProperties(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAddMaterialComments(const TSharedPtr<FJsonObject>& Params);

	// Helpers
	static UClass* FindExpressionClass(const FString& TypeName);
	static bool SetExpressionProperty(UMaterialExpression* Expr, const FString& PropName,
	                                   const TSharedPtr<FJsonValue>& Value, FString& OutError);
	static void HandleCustomHLSLNode(UMaterialExpression* Expr, const TSharedPtr<FJsonObject>& NodeDef);

	// Enum resolution helpers
	static EBlendMode ResolveBlendMode(const FString& Name);
	static EMaterialShadingModel ResolveShadingModel(const FString& Name);
	static EMaterialProperty ResolveMaterialProperty(const FString& Name);
};
