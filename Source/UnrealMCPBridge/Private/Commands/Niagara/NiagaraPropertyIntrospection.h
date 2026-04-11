#pragma once

#include "CoreMinimal.h"
#include "Json.h"
#include "NiagaraTypes.h"

class FProperty;
class UStruct;
class UScriptStruct;
class UClass;
class UObject;
class UEnum;

/**
 * Generic FProperty → JSON schema/value introspector for Niagara MCP tools.
 *
 * Replaces the hand-rolled "if FloatProperty / if EnumProperty" branches in
 * existing handlers with a single recursive walker that handles every Unreal
 * property type:
 *
 *   bool, byte, int8/16/32/64, uint8/16/32/64, float, double, string, name,
 *   text, enum (FEnumProperty + FByteProperty-with-enum), struct (recursive),
 *   array (recursive), map (key+value recursive), set (recursive),
 *   object/class/soft-object/soft-class refs, interface refs.
 *
 * Each property serializes both its SCHEMA (kind, sub-type, enum entries,
 * nested fields, metadata) and its CURRENT VALUE (when a container pointer
 * is provided). The shape is stable so callers always get the same fields.
 *
 * Used by:
 *   - get_niagara_module_inputs   (rich per-input schema)
 *   - describe_niagara_type       (standalone type query)
 *   - get_niagara_data_interface_schema (UNiagaraDataInterface CDO walk)
 */
namespace NiagaraIntrospection
{
	/** Recursion budget — guards against deeply nested struct cycles. */
	constexpr int32 MaxIntrospectionDepth = 6;

	/**
	 * Build the JSON schema for one FProperty. If Container is non-null, also
	 * embeds the property's current value under "value".
	 *
	 * Returns an object of the form:
	 *   { "kind": "...", "name": "...", "display_name": "...", "tooltip": "...",
	 *     ...kind-specific fields..., "value": <current value> }
	 *
	 * Where "kind" is one of:
	 *   "bool", "int", "float", "double", "string", "name", "text", "enum",
	 *   "struct", "array", "map", "set", "object", "class", "soft_object",
	 *   "soft_class", "interface", "delegate", "unknown".
	 */
	TSharedPtr<FJsonObject> SerializeProperty(
		const FProperty* Property,
		const void* Container = nullptr,
		int32 Depth = 0);

	/**
	 * Walk every editable UPROPERTY on a UStruct (UClass or UScriptStruct) and
	 * return the field list. When Container is non-null, embeds current values.
	 * Filter is an optional case-insensitive name substring.
	 */
	TSharedPtr<FJsonObject> SerializeStructFields(
		const UStruct* Struct,
		const void* Container = nullptr,
		const FString& Filter = FString(),
		bool bIncludeInherited = true,
		int32 Depth = 0);

	/**
	 * Build a schema description of a UEnum (every entry with display name,
	 * tooltip, value, and full path).
	 */
	TSharedPtr<FJsonObject> SerializeEnum(const UEnum* Enum);

	/**
	 * Walk a UNiagaraDataInterface class default object and return the schema
	 * of every editable property the user can configure. Use this to discover
	 * what fields a DI class supports before instantiating one.
	 */
	TSharedPtr<FJsonObject> SerializeDataInterfaceClass(UClass* DIClass);

	/**
	 * Convert a FNiagaraTypeDefinition into a schema JSON object. Returns
	 * meta-info about the type kind (primitive / struct / enum / data
	 * interface / parameter map / position / etc.) plus a nested schema for
	 * struct/enum/DI types.
	 */
	TSharedPtr<FJsonObject> SerializeNiagaraType(const FNiagaraTypeDefinition& TypeDef);

	/**
	 * Resolve a Niagara type name string into a FNiagaraTypeDefinition. Tries:
	 *   1. Built-in fast paths (float, int, bool, vec3, color, etc.)
	 *   2. FNiagaraTypeRegistry::GetRegisteredTypeByName
	 *   3. Lookup as UEnum / UScriptStruct / UClass by short or long name
	 * Returns true on success.
	 */
	bool ResolveTypeName(const FString& TypeName, FNiagaraTypeDefinition& OutType);

	/**
	 * Try to convert a JSON value into a string suitable for assignment to a
	 * UEdGraphPin's DefaultValue, given the property's expected type.
	 *
	 * Handles primitives (number, bool, string), vector ({x,y,z}), color
	 * ({r,g,b,a}), and array-of-numbers shorthand. Returns false on type
	 * mismatch.
	 */
	bool JsonValueToPinDefaultString(
		const TSharedPtr<FJsonValue>& JsonValue,
		const FNiagaraTypeDefinition& TypeDef,
		FString& OutDefaultValue,
		FString& OutError);
}

