#pragma once

#include "CoreMinimal.h"
#include "Json.h"

class UMaterialExpression;
class UMaterial;
class UMaterialInstanceConstant;

/**
 * Handler class for Material-related MCP commands.
 *
 * Implements native C++ material creation, graph building, and inspection
 * without going through the Python scripting plugin.
 *
 * All operations use UMaterialEditingLibrary and direct UE5 reflection APIs
 * (GetInput/GetInputName/GetOutputs) for correctness.
 */
class UNREALMCPBRIDGE_API FEpicUnrealMCPMaterialCommands
{
public:
	FEpicUnrealMCPMaterialCommands();

	TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
	// ---- Material Creation & Bulk Operations ----
	TSharedPtr<FJsonObject> HandleCreateMaterial(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleCreateMaterialInstance(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleBuildMaterialGraph(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetMaterialProperties(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAddMaterialComments(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleRecompileMaterial(const TSharedPtr<FJsonObject>& Params);

	// ---- Graph Read Operations ----
	TSharedPtr<FJsonObject> HandleGetMaterialInfo(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetMaterialGraphNodes(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetMaterialExpressionInfo(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetMaterialPropertyConnections(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetMaterialErrors(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetExpressionTypeInfo(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleValidateMaterialGraph(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleTraceMaterialConnection(const TSharedPtr<FJsonObject>& Params);

	// ---- Individual Node Mutations ----
	TSharedPtr<FJsonObject> HandleAddMaterialExpression(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetMaterialExpressionProperty(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleMoveMaterialExpression(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleDuplicateMaterialExpression(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleDeleteMaterialExpression(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleConnectMaterialExpressions(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleLayoutMaterialExpressions(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleDisconnectMaterialExpression(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleCleanupMaterialGraph(const TSharedPtr<FJsonObject>& Params);

	// ---- Material Instance ----
	TSharedPtr<FJsonObject> HandleGetMaterialInstanceParameters(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetMaterialInstanceParameter(const TSharedPtr<FJsonObject>& Params);

	// ---- Discovery ----
	TSharedPtr<FJsonObject> HandleListMaterialExpressionTypes(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSearchMaterialFunctions(const TSharedPtr<FJsonObject>& Params);

	// ---- Internal Helpers (MaterialHelpers.cpp) ----

	/** Find a UMaterialExpression subclass by short name (e.g. "Multiply" -> UMaterialExpressionMultiply). */
	static UClass* FindExpressionClass(const FString& TypeName);

	/**
	 * Set a property on a material expression node.
	 * Accepts snake_case names (parameter_name) and normalises them to PascalCase (ParameterName).
	 * Handles asset paths (string starting with "/"), FLinearColor arrays, FVector2D arrays,
	 * and delegates to FEpicUnrealMCPCommonUtils::SetObjectProperty for scalar/enum/bool.
	 */
	static bool SetExpressionProperty(UMaterialExpression* Expr, const FString& PropName,
	                                   const TSharedPtr<FJsonValue>& Value, FString& OutError);

	/**
	 * Configure a Custom HLSL node (UMaterialExpressionCustom) from a JSON definition.
	 * Handles: code, description, output_type, inputs[], outputs[].
	 */
	static void HandleCustomHLSLNode(UMaterialExpression* Expr, const TSharedPtr<FJsonObject>& NodeDef);

	/**
	 * Resolve snake_case or exact property name to the PascalCase name that UE reflection expects.
	 * Returns the normalised name (or the original if no PascalCase equivalent was found).
	 */
	static FString NormalizePropName(UMaterialExpression* Expr, const FString& Name);

	/** Parse an integer from a JSON value that may be EJson::Number or a numeric EJson::String. */
	static bool TryParseIntFromJson(const TSharedPtr<FJsonValue>& Val, int32& OutInt);

	// ---- Enum Resolution (MaterialHelpers.cpp) ----
	static EBlendMode           ResolveBlendMode(const FString& Name);
	static EMaterialShadingModel ResolveShadingModel(const FString& Name);
	static EMaterialProperty    ResolveMaterialProperty(const FString& Name);
	static FString              MaterialPropertyToString(EMaterialProperty Prop);

	// ---- Serialization (MaterialSerializer.cpp) ----

	/**
	 * Serialise one expression node to a JSON object.
	 * @param bIncludeAvailablePins  When true, adds "available_inputs" and "available_outputs" arrays.
	 * @param Verbosity  "summary" (index/type/pos only), "connections" (+ input connections), "full" (+ properties).
	 */
	static TSharedPtr<FJsonObject> SerializeMaterialExpression(
		UMaterialExpression* Expr, int32 Index,
		const TMap<UMaterialExpression*, int32>& ExprIndexMap,
		bool bIncludeAvailablePins = false,
		const FString& Verbosity = TEXT("full"));

	/** Serialize Custom HLSL node properties (code, inputs, outputs). */
	static void SerializeCustomHLSLProperties(
		class UMaterialExpressionCustom* Custom, TSharedPtr<FJsonObject>& OutProps);

	/** Serialize generic expression properties via UE reflection. */
	static void SerializeGenericExpressionProperties(
		UMaterialExpression* Expr, TSharedPtr<FJsonObject>& OutProps);

	/** Serialize input connections for a node. */
	static void SerializeInputConnections(
		UMaterialExpression* Expr,
		const TMap<UMaterialExpression*, int32>& ExprIndexMap,
		TSharedPtr<FJsonObject>& OutNode);

	/** Serialize available input/output pins for a node. */
	static void SerializeAvailablePins(
		UMaterialExpression* Expr, TSharedPtr<FJsonObject>& OutNode);
};
