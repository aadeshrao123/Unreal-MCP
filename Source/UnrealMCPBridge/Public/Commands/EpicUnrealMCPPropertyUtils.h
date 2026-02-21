#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Shared property read/write utilities used by any MCP command handler.
 *
 * Covers every UE5 FProperty type including:
 *   - Primitives (int, float, bool, string, name, text, enum)
 *   - Structs (FGameplayTag, FVector, FTransform, custom structs)
 *   - Object refs, soft refs, soft class refs
 *   - TArray / TMap / TSet (via FJsonObjectConverter fallback)
 *   - TArray<UObject* Instanced> — NewObject + recursive SetPropertiesFromJson
 *   - TArray<FInstancedStruct> — FInstancedStruct::InitializeAs per element
 *   - FStructProperty containing any of the above (recursive SetStructFieldsFromJson)
 */
class UNREALMCPBRIDGE_API FEpicUnrealMCPPropertyUtils
{
public:
	/** Find ANY loaded UClass by short name, "U"-prefixed name, or full path. */
	static UClass* ResolveAnyClass(const FString& ClassName);

	/** Set one FProperty on a UObject from a JSON value. */
	static bool SetProperty(
		UObject* Object, const FString& PropertyName,
		const TSharedPtr<FJsonValue>& Value, FString& OutError);

	/** Apply every key in Json to the matching FProperty on Target,
	 *  skipping the "_ClassName" discriminator key. */
	static void SetPropertiesFromJson(
		UObject* Target, const TSharedPtr<FJsonObject>& Json, FString& OutErrors);

	/** Apply every key in Json to raw struct memory (UScriptStruct + void*).
	 *  Handles nested instanced-object arrays and FInstancedStruct arrays
	 *  that FJsonObjectConverter cannot deserialise on its own.
	 *  @param Outer  Used as outer for any NewObject calls. */
	static void SetStructFieldsFromJson(
		UScriptStruct* Struct, void* StructData,
		const TSharedPtr<FJsonObject>& Json,
		UObject* Outer, FString& OutErrors);

	/** Serialise all FProperties on Object to a JSON object.
	 *  @param FilterLower  Lower-case substring; empty = include all.
	 *  @param bIncludeInherited  Walk the full class hierarchy. */
	static TSharedPtr<FJsonObject> SerializeAllProperties(
		UObject* Object, const FString& FilterLower, bool bIncludeInherited);

	/** Safely serialize a single property to JSON, handling recursion and
	 *  skipping unsafe/internal types that might crash. */
	static TSharedPtr<FJsonValue> SafePropertyToJsonValue(FProperty* Property, const void* Value);
};
