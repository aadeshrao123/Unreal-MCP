#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Shared property read/write utilities used by any MCP command handler.
 *
 * Covers every UE5 FProperty type including:
 *   - Primitives (int, float, bool, string, name, text, enum)
 *   - Structs (FGameplayTag, FVector, FTransform, custom structs …)
 *   - Object refs, soft refs, soft class refs
 *   - TArray / TMap / TSet (via FJsonObjectConverter fallback)
 *   - TArray<UObject* Instanced>  — NewObject + recursive SetPropertiesFromJson
 *   - TArray<FInstancedStruct>    — FInstancedStruct::InitializeAs per element
 *   - FStructProperty containing any of the above (recursive SetStructFieldsFromJson)
 */
class UNREALMCPBRIDGE_API FEpicUnrealMCPPropertyUtils
{
public:
    // ------------------------------------------------------------------
    // Class resolution
    // ------------------------------------------------------------------

    /** Find ANY loaded UClass by short name, "U"-prefixed name, or full path.
     *  Not limited to UDataAsset subclasses — works for any UClass. */
    static UClass* ResolveAnyClass(const FString& ClassName);

    // ------------------------------------------------------------------
    // Single-property write
    // ------------------------------------------------------------------

    /** Set one FProperty on a UObject from a JSON value.
     *  Returns false and populates OutError on failure. */
    static bool SetProperty(UObject* Object, const FString& PropertyName,
                            const TSharedPtr<FJsonValue>& Value, FString& OutError);

    // ------------------------------------------------------------------
    // Bulk write
    // ------------------------------------------------------------------

    /** Apply every key in Json to the matching FProperty on Target,
     *  skipping the "_ClassName" discriminator key.
     *  Used recursively when deserialising instanced subobjects. */
    static void SetPropertiesFromJson(UObject* Target,
                                      const TSharedPtr<FJsonObject>& Json,
                                      FString& OutErrors);

    /** Apply every key in Json to raw struct memory (UScriptStruct + void*).
     *  Handles nested instanced-object arrays and FInstancedStruct arrays
     *  that FJsonObjectConverter cannot deserialise on its own.
     *  Falls back to FJsonObjectConverter for all other field types.
     *  @param Outer  Used as outer for any NewObject calls when creating
     *                instanced subobjects. */
    static void SetStructFieldsFromJson(UScriptStruct* Struct, void* StructData,
                                        const TSharedPtr<FJsonObject>& Json,
                                        UObject* Outer, FString& OutErrors);

    // ------------------------------------------------------------------
    // Read
    // ------------------------------------------------------------------

    /** Serialise all FProperties on Object to a JSON object.
     *  @param FilterLower  Lower-case substring; empty = include all.
     *  @param bIncludeInherited  Walk the full class hierarchy. */
    static TSharedPtr<FJsonObject> SerializeAllProperties(UObject* Object,
                                                           const FString& FilterLower,
                                                           bool bIncludeInherited);
};
