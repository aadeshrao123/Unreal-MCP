#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/** Walker options shared by metadata + flat-property tree enumeration.
 *  Controls filtering, depth, pagination, and inherited/object behavior so a single
 *  AI tool call can scope itself precisely without flooding the response. */
struct UNREALMCPBRIDGE_API FPropertyWalkOptions
{
	/** Lower-case substring matched against the full dotted path. Empty = no filter. */
	FString FilterLower;

	/** Lower-case exact match against UPROPERTY(Category="X") metadata. Empty = no filter.
	 *  Sub-categories use "|" — e.g. "Lens|Bloom" matches that exact string. */
	FString CategoryLower;

	/** How many struct nesting levels to expand below the start point.
	 *  0 = top-level only, 1 = one level into structs, etc. */
	int32 MaxDepth = 1;

	/** Hard cap on emitted leaf entries. The walker still counts everything matching
	 *  for the _summary, but stops emitting once this many are returned. */
	int32 MaxEntries = 50;

	/** Pagination offset: how many matching entries to skip before emitting. */
	int32 Cursor = 0;

	/** When walker hits an enum, include valid_values array in metadata. */
	bool bExpandEnum = true;

	/** Walk the full inheritance chain via TFieldIterator IncludeSuper.
	 *  When false, only properties declared on the most-derived class are emitted. */
	bool bIncludeInherited = true;

	/** When the entry point's property_path lands on an FObjectProperty, descend into
	 *  the target object's class and enumerate ITS properties. False = treat as leaf
	 *  and return single metadata blob for the reference (recommended default —
	 *  callers should use `component_name` to switch to the target instead). */
	bool bDescendIntoObjects = false;

	/** Metadata mode (true) emits full per-property metadata blobs as values.
	 *  Raw mode (false) emits just the current value. Used by SerializePropertiesFlat. */
	bool bIncludeMetadata = true;

	/** Emit array elements as Field[N] entries (flat mode). */
	bool bExpandArrays = true;

	/** Cap on array elements emitted per array (flat mode). */
	int32 ArrayElementLimit = 16;
};

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

	/** Universal property writer when caller already resolved Prop + memory address.
	 *  Used by SetProperty, SetPropertyAtPath, and SetStructFieldsFromJson — single
	 *  source of truth for "write a JSON value into any FProperty" with strict
	 *  validation for nested structs and proper handling of every UE5 reflection type.
	 *  @param Prop          The FProperty descriptor.
	 *  @param PropAddr      Pointer to the property's value memory.
	 *  @param OwnerObject   Used as outer for any NewObject() and for context in errors.
	 *                       May be null for pure struct memory writes.
	 *  @param PropertyName  Used in error messages only. */
	static bool SetPropertyValueAtAddr(
		FProperty* Prop, void* PropAddr, UObject* OwnerObject,
		const FString& PropertyName, const TSharedPtr<FJsonValue>& Value, FString& OutError);

	/** Set a property at a dotted path on a UObject (e.g. "Settings.BloomIntensity",
	 *  "Settings.ColorGradingHighlights.Gain", "Tags[0]").
	 *  Walks struct properties and array indices; descends into UObject sub-properties.
	 *  Single-segment paths delegate to SetProperty for full type coverage including
	 *  instanced subobjects. Multi-segment paths use a leaf-write helper that handles
	 *  primitives, enums, structs (object literal or text format), and object refs.
	 *  @param OutTopLevelProperty  If non-null, set to the FProperty of the first
	 *                              path segment so callers can drive PostEditChangeProperty. */
	static bool SetPropertyAtPath(
		UObject* Root, const FString& PropertyPath,
		const TSharedPtr<FJsonValue>& Value, FString& OutError,
		FProperty** OutTopLevelProperty = nullptr);

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

	/** Return a human-readable UE type description for an FProperty:
	 *  "float", "FLinearColor", "TArray<FVector>", "TSubclassOf<APawn>",
	 *  "TEnumAsByte<EAutoExposureMethod>", "EBloomMethod", "TMap<FName,int32>". */
	static FString GetPropertyTypeDescription(FProperty* Prop);

	/** Build a JSON metadata blob for a single FProperty. Includes:
	 *    path, name, cpp_type, ue_type, category, display_name, tooltip,
	 *    is_struct/is_array/is_enum/is_object/is_bool flags,
	 *    array_dim (for static C arrays), clamp_min/max, ui_min/max,
	 *    edit_condition, editable/transient/readonly flags,
	 *    struct_type (for FStructProperty), enum_type (for enums),
	 *    object_class / meta_class (for object/class refs),
	 *    inner_type (for arrays/sets/maps).
	 *  @param ValueAddr      If non-null, current value is included as 'current_value'.
	 *  @param bExpandEnum    If true and Prop is enum-typed, includes 'valid_values' array. */
	static TSharedPtr<FJsonObject> GetPropertyMetadata(
		FProperty* Prop, const FString& FullPath, const void* ValueAddr,
		bool bIncludeValue, bool bExpandEnum);

	/** Walk all properties on Object recursively, return flat dict (path -> value or metadata)
	 *  PLUS a "_summary" header with total_available/total_returned/truncated/next_cursor,
	 *  own_count/inherited_count, class_chain, and categories breakdown. */
	static TSharedPtr<FJsonObject> SerializePropertiesFlat(
		UObject* Object, const FPropertyWalkOptions& Options);

	/** Enumerate property metadata anchored at a given dotted path on Object.
	 *  - PropertyPath empty:        enumerate Object's top-level properties.
	 *  - PropertyPath = struct path: enumerate that struct's fields.
	 *  - PropertyPath = object ref:  if Options.bDescendIntoObjects, descend; else
	 *                                return single metadata blob for the reference + _hint.
	 *  - PropertyPath = leaf:        return single metadata for that property.
	 *  Always emits a "_summary" header with total/cursor/categories info. */
	static TSharedPtr<FJsonObject> GetPropertyMetadataTree(
		UObject* Object, const FString& PropertyPath, const FPropertyWalkOptions& Options);
};
