#include "NiagaraPropertyIntrospection.h"
#include "NiagaraTypeHelpers.h"

#include "NiagaraTypes.h"
#include "NiagaraTypeRegistry.h"
#include "NiagaraDataInterface.h"

#include "UObject/Class.h"
#include "UObject/Object.h"
#include "UObject/UnrealType.h"
#include "UObject/EnumProperty.h"
#include "UObject/TextProperty.h"
#include "UObject/MetaData.h"
#include "Templates/SharedPointer.h"

namespace NiagaraIntrospection
{

// ---------------------------------------------------------------------------
// Local helpers
// ---------------------------------------------------------------------------

static void StampMetadata(const FProperty* Property, FJsonObject& Out)
{
	if (!Property) return;
	const FString DisplayName = Property->GetDisplayNameText().ToString();
	if (!DisplayName.IsEmpty() && DisplayName != Property->GetName())
	{
		Out.SetStringField(TEXT("display_name"), DisplayName);
	}
#if WITH_EDITORONLY_DATA
	if (Property->HasMetaData(TEXT("ToolTip")))
	{
		Out.SetStringField(TEXT("tooltip"), Property->GetMetaData(TEXT("ToolTip")));
	}
	if (Property->HasMetaData(TEXT("Category")))
	{
		Out.SetStringField(TEXT("category"), Property->GetMetaData(TEXT("Category")));
	}
	if (Property->HasMetaData(TEXT("ClampMin")))
	{
		Out.SetStringField(TEXT("clamp_min"), Property->GetMetaData(TEXT("ClampMin")));
	}
	if (Property->HasMetaData(TEXT("ClampMax")))
	{
		Out.SetStringField(TEXT("clamp_max"), Property->GetMetaData(TEXT("ClampMax")));
	}
	if (Property->HasMetaData(TEXT("UIMin")))
	{
		Out.SetStringField(TEXT("ui_min"), Property->GetMetaData(TEXT("UIMin")));
	}
	if (Property->HasMetaData(TEXT("UIMax")))
	{
		Out.SetStringField(TEXT("ui_max"), Property->GetMetaData(TEXT("UIMax")));
	}
	if (Property->HasMetaData(TEXT("Units")))
	{
		Out.SetStringField(TEXT("units"), Property->GetMetaData(TEXT("Units")));
	}
#endif
}

static TSharedPtr<FJsonValue> SerializePrimitiveValue(const FProperty* Property, const void* ValuePtr)
{
	if (!Property || !ValuePtr) return MakeShared<FJsonValueNull>();

	if (const FBoolProperty* P = CastField<FBoolProperty>(Property))
	{
		return MakeShared<FJsonValueBoolean>(P->GetPropertyValue(ValuePtr));
	}
	if (const FByteProperty* P = CastField<FByteProperty>(Property))
	{
		const uint8 Val = P->GetPropertyValue(ValuePtr);
		if (P->Enum)
		{
			return MakeShared<FJsonValueString>(P->Enum->GetNameStringByValue(Val));
		}
		return MakeShared<FJsonValueNumber>((double)Val);
	}
	if (const FInt8Property* P = CastField<FInt8Property>(Property))
		return MakeShared<FJsonValueNumber>((double)P->GetPropertyValue(ValuePtr));
	if (const FInt16Property* P = CastField<FInt16Property>(Property))
		return MakeShared<FJsonValueNumber>((double)P->GetPropertyValue(ValuePtr));
	if (const FIntProperty* P = CastField<FIntProperty>(Property))
		return MakeShared<FJsonValueNumber>((double)P->GetPropertyValue(ValuePtr));
	if (const FInt64Property* P = CastField<FInt64Property>(Property))
		return MakeShared<FJsonValueNumber>((double)P->GetPropertyValue(ValuePtr));
	if (const FUInt16Property* P = CastField<FUInt16Property>(Property))
		return MakeShared<FJsonValueNumber>((double)P->GetPropertyValue(ValuePtr));
	if (const FUInt32Property* P = CastField<FUInt32Property>(Property))
		return MakeShared<FJsonValueNumber>((double)P->GetPropertyValue(ValuePtr));
	if (const FUInt64Property* P = CastField<FUInt64Property>(Property))
		return MakeShared<FJsonValueNumber>((double)P->GetPropertyValue(ValuePtr));
	if (const FFloatProperty* P = CastField<FFloatProperty>(Property))
		return MakeShared<FJsonValueNumber>((double)P->GetPropertyValue(ValuePtr));
	if (const FDoubleProperty* P = CastField<FDoubleProperty>(Property))
		return MakeShared<FJsonValueNumber>(P->GetPropertyValue(ValuePtr));
	if (const FStrProperty* P = CastField<FStrProperty>(Property))
		return MakeShared<FJsonValueString>(P->GetPropertyValue(ValuePtr));
	if (const FNameProperty* P = CastField<FNameProperty>(Property))
		return MakeShared<FJsonValueString>(P->GetPropertyValue(ValuePtr).ToString());
	if (const FTextProperty* P = CastField<FTextProperty>(Property))
		return MakeShared<FJsonValueString>(P->GetPropertyValue(ValuePtr).ToString());
	if (const FEnumProperty* P = CastField<FEnumProperty>(Property))
	{
		const UEnum* Enum = P->GetEnum();
		const int64 Val = P->GetUnderlyingProperty()->GetSignedIntPropertyValue(ValuePtr);
		return MakeShared<FJsonValueString>(Enum ? Enum->GetNameStringByValue(Val) : FString::Printf(TEXT("%lld"), Val));
	}
	return MakeShared<FJsonValueNull>();
}

// ---------------------------------------------------------------------------
// SerializeEnum
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> SerializeEnum(const UEnum* Enum)
{
	auto Out = MakeShared<FJsonObject>();
	if (!Enum)
	{
		Out->SetStringField(TEXT("kind"), TEXT("invalid_enum"));
		return Out;
	}
	Out->SetStringField(TEXT("kind"), TEXT("enum"));
	Out->SetStringField(TEXT("name"), Enum->GetName());
	Out->SetStringField(TEXT("path"), Enum->GetPathName());

	TArray<TSharedPtr<FJsonValue>> Entries;
	const int32 NumEntries = Enum->NumEnums();
	for (int32 i = 0; i < NumEntries; ++i)
	{
		const FString FullName = Enum->GetNameByIndex(i).ToString();
		const FString ShortName = Enum->GetNameStringByIndex(i);
		if (ShortName.EndsWith(TEXT("_MAX"))) continue;
#if WITH_EDITORONLY_DATA
		if (Enum->HasMetaData(TEXT("Hidden"), i)) continue;
#endif
		auto Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("name"), ShortName);
		Entry->SetStringField(TEXT("full_name"), FullName);
		Entry->SetStringField(TEXT("display_name"),
			Enum->GetDisplayNameTextByIndex(i).ToString());
		Entry->SetNumberField(TEXT("value"), (double)Enum->GetValueByIndex(i));
#if WITH_EDITORONLY_DATA
		if (Enum->HasMetaData(TEXT("ToolTip"), i))
		{
			Entry->SetStringField(TEXT("tooltip"), Enum->GetMetaData(TEXT("ToolTip"), i));
		}
#endif
		Entries.Add(MakeShared<FJsonValueObject>(Entry));
	}
	Out->SetArrayField(TEXT("values"), Entries);
	return Out;
}

// ---------------------------------------------------------------------------
// SerializeProperty (recursive workhorse)
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> SerializeProperty(const FProperty* Property, const void* Container, int32 Depth)
{
	auto Out = MakeShared<FJsonObject>();
	if (!Property)
	{
		Out->SetStringField(TEXT("kind"), TEXT("null"));
		return Out;
	}
	if (Depth > MaxIntrospectionDepth)
	{
		Out->SetStringField(TEXT("kind"), TEXT("max_depth"));
		Out->SetStringField(TEXT("name"), Property->GetName());
		return Out;
	}

	Out->SetStringField(TEXT("name"), Property->GetName());
	Out->SetStringField(TEXT("cpp_type"), Property->GetCPPType());
	StampMetadata(Property, *Out);

	const void* ValuePtr = Container ? Property->ContainerPtrToValuePtr<void>(Container) : nullptr;

	// ---- Primitive scalars ----
	if (const FBoolProperty* P = CastField<FBoolProperty>(Property))
	{
		Out->SetStringField(TEXT("kind"), TEXT("bool"));
		if (ValuePtr) Out->SetBoolField(TEXT("value"), P->GetPropertyValue(ValuePtr));
		return Out;
	}

	// Byte property: distinguishes plain uint8 from byte-backed enum
	if (const FByteProperty* P = CastField<FByteProperty>(Property))
	{
		if (P->Enum)
		{
			Out->SetStringField(TEXT("kind"), TEXT("enum"));
			Out->SetObjectField(TEXT("enum"), SerializeEnum(P->Enum));
			if (ValuePtr)
			{
				const uint8 Val = P->GetPropertyValue(ValuePtr);
				Out->SetStringField(TEXT("value"), P->Enum->GetNameStringByValue(Val));
				Out->SetStringField(TEXT("value_display"),
					P->Enum->GetDisplayNameTextByValue(Val).ToString());
			}
			return Out;
		}
		Out->SetStringField(TEXT("kind"), TEXT("int"));
		Out->SetStringField(TEXT("sub"), TEXT("uint8"));
		if (ValuePtr) Out->SetNumberField(TEXT("value"), (double)P->GetPropertyValue(ValuePtr));
		return Out;
	}

	if (const FEnumProperty* P = CastField<FEnumProperty>(Property))
	{
		Out->SetStringField(TEXT("kind"), TEXT("enum"));
		Out->SetObjectField(TEXT("enum"), SerializeEnum(P->GetEnum()));
		if (ValuePtr && P->GetEnum())
		{
			const int64 Val = P->GetUnderlyingProperty()->GetSignedIntPropertyValue(ValuePtr);
			Out->SetStringField(TEXT("value"), P->GetEnum()->GetNameStringByValue(Val));
			Out->SetStringField(TEXT("value_display"),
				P->GetEnum()->GetDisplayNameTextByValue(Val).ToString());
		}
		return Out;
	}

	auto IntKind = [&](const TCHAR* Sub, int64 Value)
	{
		Out->SetStringField(TEXT("kind"), TEXT("int"));
		Out->SetStringField(TEXT("sub"), Sub);
		if (ValuePtr) Out->SetNumberField(TEXT("value"), (double)Value);
	};
	if (const FInt8Property* P = CastField<FInt8Property>(Property))
	{
		IntKind(TEXT("int8"), ValuePtr ? P->GetPropertyValue(ValuePtr) : 0);
		return Out;
	}
	if (const FInt16Property* P = CastField<FInt16Property>(Property))
	{
		IntKind(TEXT("int16"), ValuePtr ? P->GetPropertyValue(ValuePtr) : 0);
		return Out;
	}
	if (const FIntProperty* P = CastField<FIntProperty>(Property))
	{
		IntKind(TEXT("int32"), ValuePtr ? P->GetPropertyValue(ValuePtr) : 0);
		return Out;
	}
	if (const FInt64Property* P = CastField<FInt64Property>(Property))
	{
		IntKind(TEXT("int64"), ValuePtr ? P->GetPropertyValue(ValuePtr) : 0);
		return Out;
	}
	if (const FUInt16Property* P = CastField<FUInt16Property>(Property))
	{
		IntKind(TEXT("uint16"), ValuePtr ? (int64)P->GetPropertyValue(ValuePtr) : 0);
		return Out;
	}
	if (const FUInt32Property* P = CastField<FUInt32Property>(Property))
	{
		IntKind(TEXT("uint32"), ValuePtr ? (int64)P->GetPropertyValue(ValuePtr) : 0);
		return Out;
	}
	if (const FUInt64Property* P = CastField<FUInt64Property>(Property))
	{
		IntKind(TEXT("uint64"), ValuePtr ? (int64)P->GetPropertyValue(ValuePtr) : 0);
		return Out;
	}

	if (const FFloatProperty* P = CastField<FFloatProperty>(Property))
	{
		Out->SetStringField(TEXT("kind"), TEXT("float"));
		if (ValuePtr) Out->SetNumberField(TEXT("value"), (double)P->GetPropertyValue(ValuePtr));
		return Out;
	}
	if (const FDoubleProperty* P = CastField<FDoubleProperty>(Property))
	{
		Out->SetStringField(TEXT("kind"), TEXT("double"));
		if (ValuePtr) Out->SetNumberField(TEXT("value"), P->GetPropertyValue(ValuePtr));
		return Out;
	}
	if (const FStrProperty* P = CastField<FStrProperty>(Property))
	{
		Out->SetStringField(TEXT("kind"), TEXT("string"));
		if (ValuePtr) Out->SetStringField(TEXT("value"), P->GetPropertyValue(ValuePtr));
		return Out;
	}
	if (const FNameProperty* P = CastField<FNameProperty>(Property))
	{
		Out->SetStringField(TEXT("kind"), TEXT("name"));
		if (ValuePtr) Out->SetStringField(TEXT("value"), P->GetPropertyValue(ValuePtr).ToString());
		return Out;
	}
	if (const FTextProperty* P = CastField<FTextProperty>(Property))
	{
		Out->SetStringField(TEXT("kind"), TEXT("text"));
		if (ValuePtr) Out->SetStringField(TEXT("value"), P->GetPropertyValue(ValuePtr).ToString());
		return Out;
	}

	// ---- Struct ----
	if (const FStructProperty* P = CastField<FStructProperty>(Property))
	{
		Out->SetStringField(TEXT("kind"), TEXT("struct"));
		if (P->Struct)
		{
			Out->SetStringField(TEXT("struct"), P->Struct->GetName());
			Out->SetStringField(TEXT("struct_path"), P->Struct->GetPathName());
			Out->SetObjectField(TEXT("schema"),
				SerializeStructFields(P->Struct, ValuePtr, FString(), true, Depth + 1));
		}
		return Out;
	}

	// ---- Array ----
	if (const FArrayProperty* P = CastField<FArrayProperty>(Property))
	{
		Out->SetStringField(TEXT("kind"), TEXT("array"));
		Out->SetObjectField(TEXT("inner"), SerializeProperty(P->Inner, nullptr, Depth + 1));
		if (ValuePtr)
		{
			FScriptArrayHelper Helper(P, ValuePtr);
			const int32 Num = Helper.Num();
			Out->SetNumberField(TEXT("count"), Num);

			TArray<TSharedPtr<FJsonValue>> Items;
			const int32 MaxItems = FMath::Min(Num, 64); // cap to keep responses small
			for (int32 i = 0; i < MaxItems; ++i)
			{
				Items.Add(SerializePrimitiveValue(P->Inner, Helper.GetRawPtr(i)));
			}
			Out->SetArrayField(TEXT("items"), Items);
			if (Num > MaxItems)
			{
				Out->SetBoolField(TEXT("truncated"), true);
			}
		}
		return Out;
	}

	// ---- Map ----
	if (const FMapProperty* P = CastField<FMapProperty>(Property))
	{
		Out->SetStringField(TEXT("kind"), TEXT("map"));
		Out->SetObjectField(TEXT("key"), SerializeProperty(P->KeyProp, nullptr, Depth + 1));
		Out->SetObjectField(TEXT("value"), SerializeProperty(P->ValueProp, nullptr, Depth + 1));
		if (ValuePtr)
		{
			FScriptMapHelper Helper(P, ValuePtr);
			Out->SetNumberField(TEXT("count"), Helper.Num());
		}
		return Out;
	}

	// ---- Set ----
	if (const FSetProperty* P = CastField<FSetProperty>(Property))
	{
		Out->SetStringField(TEXT("kind"), TEXT("set"));
		Out->SetObjectField(TEXT("element"), SerializeProperty(P->ElementProp, nullptr, Depth + 1));
		if (ValuePtr)
		{
			FScriptSetHelper Helper(P, ValuePtr);
			Out->SetNumberField(TEXT("count"), Helper.Num());
		}
		return Out;
	}

	// ---- Object refs ----
	if (const FObjectProperty* P = CastField<FObjectProperty>(Property))
	{
		Out->SetStringField(TEXT("kind"), TEXT("object"));
		if (P->PropertyClass)
		{
			Out->SetStringField(TEXT("class"), P->PropertyClass->GetName());
			Out->SetStringField(TEXT("class_path"), P->PropertyClass->GetPathName());
		}
		if (ValuePtr)
		{
			UObject* Obj = P->GetObjectPropertyValue(ValuePtr);
			Out->SetStringField(TEXT("value"), Obj ? Obj->GetPathName() : FString(TEXT("None")));
		}
		return Out;
	}
	if (const FClassProperty* P = CastField<FClassProperty>(Property))
	{
		Out->SetStringField(TEXT("kind"), TEXT("class"));
		if (P->MetaClass)
		{
			Out->SetStringField(TEXT("base_class"), P->MetaClass->GetName());
			Out->SetStringField(TEXT("base_class_path"), P->MetaClass->GetPathName());
		}
		if (ValuePtr)
		{
			UObject* Obj = P->GetObjectPropertyValue(ValuePtr);
			Out->SetStringField(TEXT("value"), Obj ? Obj->GetPathName() : FString(TEXT("None")));
		}
		return Out;
	}
	if (const FSoftObjectProperty* P = CastField<FSoftObjectProperty>(Property))
	{
		Out->SetStringField(TEXT("kind"), TEXT("soft_object"));
		if (P->PropertyClass)
		{
			Out->SetStringField(TEXT("class"), P->PropertyClass->GetName());
		}
		if (ValuePtr)
		{
			Out->SetStringField(TEXT("value"), P->GetPropertyValue(ValuePtr).ToString());
		}
		return Out;
	}
	if (const FSoftClassProperty* P = CastField<FSoftClassProperty>(Property))
	{
		Out->SetStringField(TEXT("kind"), TEXT("soft_class"));
		if (P->MetaClass)
		{
			Out->SetStringField(TEXT("base_class"), P->MetaClass->GetName());
		}
		if (ValuePtr)
		{
			Out->SetStringField(TEXT("value"), P->GetPropertyValue(ValuePtr).ToString());
		}
		return Out;
	}
	if (const FInterfaceProperty* P = CastField<FInterfaceProperty>(Property))
	{
		Out->SetStringField(TEXT("kind"), TEXT("interface"));
		if (P->InterfaceClass)
		{
			Out->SetStringField(TEXT("interface"), P->InterfaceClass->GetName());
		}
		return Out;
	}
	if (CastField<FDelegateProperty>(Property) || CastField<FMulticastDelegateProperty>(Property))
	{
		Out->SetStringField(TEXT("kind"), TEXT("delegate"));
		return Out;
	}

	Out->SetStringField(TEXT("kind"), TEXT("unknown"));
	return Out;
}

// ---------------------------------------------------------------------------
// SerializeStructFields
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> SerializeStructFields(
	const UStruct* Struct,
	const void* Container,
	const FString& Filter,
	bool bIncludeInherited,
	int32 Depth)
{
	auto Out = MakeShared<FJsonObject>();
	if (!Struct)
	{
		Out->SetStringField(TEXT("error"), TEXT("null struct"));
		return Out;
	}
	Out->SetStringField(TEXT("name"), Struct->GetName());
	Out->SetStringField(TEXT("path"), Struct->GetPathName());

	TArray<TSharedPtr<FJsonValue>> Fields;
	for (TFieldIterator<FProperty> PropIt(
			Struct,
			bIncludeInherited ? EFieldIterationFlags::IncludeSuper : EFieldIterationFlags::None);
		 PropIt; ++PropIt)
	{
		FProperty* Prop = *PropIt;
		if (!Prop) continue;

		// Only include user-editable / blueprint-visible properties — skip
		// transient, internal, and hidden state.
		const bool bIsEditable = Prop->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible);
		if (!bIsEditable) continue;
		if (Prop->HasAnyPropertyFlags(CPF_Transient | CPF_DisableEditOnInstance)) continue;

		if (!Filter.IsEmpty() && !Prop->GetName().Contains(Filter, ESearchCase::IgnoreCase))
		{
			continue;
		}

		Fields.Add(MakeShared<FJsonValueObject>(SerializeProperty(Prop, Container, Depth + 1)));
	}
	Out->SetArrayField(TEXT("fields"), Fields);
	Out->SetNumberField(TEXT("field_count"), Fields.Num());
	return Out;
}

// ---------------------------------------------------------------------------
// SerializeDataInterfaceClass
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> SerializeDataInterfaceClass(UClass* DIClass)
{
	auto Out = MakeShared<FJsonObject>();
	if (!DIClass)
	{
		Out->SetStringField(TEXT("error"), TEXT("null class"));
		return Out;
	}
	if (!DIClass->IsChildOf(UNiagaraDataInterface::StaticClass()))
	{
		Out->SetStringField(TEXT("error"), TEXT("class is not a UNiagaraDataInterface subclass"));
		Out->SetStringField(TEXT("class"), DIClass->GetName());
		return Out;
	}

	Out->SetStringField(TEXT("kind"), TEXT("data_interface"));
	Out->SetStringField(TEXT("class"), DIClass->GetName());
	Out->SetStringField(TEXT("class_path"), DIClass->GetPathName());
	Out->SetStringField(TEXT("display_name"), DIClass->GetDisplayNameText().ToString());
#if WITH_EDITORONLY_DATA
	if (DIClass->HasMetaData(TEXT("ToolTip")))
	{
		Out->SetStringField(TEXT("tooltip"), DIClass->GetMetaData(TEXT("ToolTip")));
	}
	if (DIClass->HasMetaData(TEXT("Category")))
	{
		Out->SetStringField(TEXT("category"), DIClass->GetMetaData(TEXT("Category")));
	}
#endif

	UObject* CDO = DIClass->GetDefaultObject();
	Out->SetObjectField(TEXT("schema"),
		SerializeStructFields(DIClass, CDO, FString(), true, 0));
	return Out;
}

// ---------------------------------------------------------------------------
// SerializeNiagaraType
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> SerializeNiagaraType(const FNiagaraTypeDefinition& TypeDef)
{
	auto Out = MakeShared<FJsonObject>();
	if (!TypeDef.IsValid())
	{
		Out->SetStringField(TEXT("kind"), TEXT("invalid"));
		return Out;
	}
	Out->SetStringField(TEXT("name"), TypeDef.GetName());
	Out->SetNumberField(TEXT("size_bytes"), TypeDef.GetSize());

	if (TypeDef.IsDataInterface())
	{
		Out->SetStringField(TEXT("kind"), TEXT("data_interface"));
		Out->SetObjectField(TEXT("schema"), SerializeDataInterfaceClass(TypeDef.GetClass()));
		return Out;
	}
	if (TypeDef.IsEnum())
	{
		Out->SetStringField(TEXT("kind"), TEXT("enum"));
		Out->SetObjectField(TEXT("enum"), SerializeEnum(TypeDef.GetEnum()));
		return Out;
	}
	if (UScriptStruct* Struct = TypeDef.GetScriptStruct())
	{
		Out->SetStringField(TEXT("kind"), TEXT("struct"));
		Out->SetObjectField(TEXT("schema"),
			SerializeStructFields(Struct, nullptr, FString(), true, 0));
		return Out;
	}
	Out->SetStringField(TEXT("kind"), TEXT("primitive"));
	return Out;
}

// ---------------------------------------------------------------------------
// ResolveTypeName
// ---------------------------------------------------------------------------

bool ResolveTypeName(const FString& TypeName, FNiagaraTypeDefinition& OutType)
{
	// Built-ins via shared helper first
	if (NiagaraTypeHelpers::ParseTypeDef(TypeName, OutType))
	{
		return true;
	}

	// Lookup as enum
	if (UEnum* Enum = FindObject<UEnum>(nullptr, *TypeName))
	{
		OutType = FNiagaraTypeDefinition(Enum);
		return true;
	}
	// Lookup as struct
	if (UScriptStruct* Struct = FindObject<UScriptStruct>(nullptr, *TypeName))
	{
		OutType = FNiagaraTypeDefinition(Struct);
		return true;
	}
	// Lookup as class (data interface) — normalize the name first since
	// UClass::GetName() returns UObject short names WITHOUT the leading "U"
	FString Normalized = TypeName;
	if (Normalized.StartsWith(TEXT("U"), ESearchCase::CaseSensitive) &&
		Normalized.Len() > 1 &&
		FChar::IsUpper(Normalized[1]))
	{
		Normalized.RightChopInline(1);
	}

	if (UClass* Cls = FindObject<UClass>(nullptr, *TypeName))
	{
		if (Cls->IsChildOf(UNiagaraDataInterface::StaticClass()))
		{
			OutType = FNiagaraTypeDefinition(Cls);
			return true;
		}
	}
	if (UClass* Cls = FindObject<UClass>(nullptr, *Normalized))
	{
		if (Cls->IsChildOf(UNiagaraDataInterface::StaticClass()))
		{
			OutType = FNiagaraTypeDefinition(Cls);
			return true;
		}
	}
	const FString WithPrefix = FString::Printf(TEXT("NiagaraDataInterface%s"), *Normalized);
	for (TObjectIterator<UClass> It; It; ++It)
	{
		if (!It->IsChildOf(UNiagaraDataInterface::StaticClass())) continue;
		const FString ClsName = It->GetName();
		if (ClsName.Equals(Normalized, ESearchCase::IgnoreCase) ||
			ClsName.Equals(WithPrefix, ESearchCase::IgnoreCase))
		{
			OutType = FNiagaraTypeDefinition(*It);
			return true;
		}
	}
	return false;
}

// ---------------------------------------------------------------------------
// JsonValueToPinDefaultString
// ---------------------------------------------------------------------------

bool JsonValueToPinDefaultString(
	const TSharedPtr<FJsonValue>& JsonValue,
	const FNiagaraTypeDefinition& TypeDef,
	FString& OutDefaultValue,
	FString& OutError)
{
	if (!JsonValue.IsValid())
	{
		OutError = TEXT("Null JSON value");
		return false;
	}

	const EJson Type = JsonValue->Type;

	if (Type == EJson::Number)
	{
		OutDefaultValue = FString::SanitizeFloat(JsonValue->AsNumber());
		return true;
	}
	if (Type == EJson::Boolean)
	{
		OutDefaultValue = JsonValue->AsBool() ? TEXT("true") : TEXT("false");
		return true;
	}
	if (Type == EJson::String)
	{
		OutDefaultValue = JsonValue->AsString();
		return true;
	}
	if (Type == EJson::Object)
	{
		const TSharedPtr<FJsonObject> Obj = JsonValue->AsObject();
		if (Obj->HasField(TEXT("x")))
		{
			double X = 0, Y = 0, Z = 0, W = 0;
			Obj->TryGetNumberField(TEXT("x"), X);
			Obj->TryGetNumberField(TEXT("y"), Y);
			Obj->TryGetNumberField(TEXT("z"), Z);
			if (Obj->HasField(TEXT("w")))
			{
				Obj->TryGetNumberField(TEXT("w"), W);
				OutDefaultValue = FString::Printf(TEXT("%f,%f,%f,%f"), X, Y, Z, W);
			}
			else
			{
				OutDefaultValue = FString::Printf(TEXT("%f,%f,%f"), X, Y, Z);
			}
			return true;
		}
		if (Obj->HasField(TEXT("r")))
		{
			double R = 0, G = 0, B = 0, A = 1;
			Obj->TryGetNumberField(TEXT("r"), R);
			Obj->TryGetNumberField(TEXT("g"), G);
			Obj->TryGetNumberField(TEXT("b"), B);
			Obj->TryGetNumberField(TEXT("a"), A);
			OutDefaultValue = FString::Printf(TEXT("%f,%f,%f,%f"), R, G, B, A);
			return true;
		}
		OutError = TEXT("Object must have {x,y,z[,w]} or {r,g,b[,a]} keys");
		return false;
	}
	if (Type == EJson::Array)
	{
		const TArray<TSharedPtr<FJsonValue>>& Arr = JsonValue->AsArray();
		TArray<FString> Parts;
		for (const TSharedPtr<FJsonValue>& V : Arr)
		{
			Parts.Add(FString::SanitizeFloat(V->AsNumber()));
		}
		OutDefaultValue = FString::Join(Parts, TEXT(","));
		return true;
	}

	OutError = TEXT("Unsupported JSON value kind");
	return false;
}

} // namespace NiagaraIntrospection
