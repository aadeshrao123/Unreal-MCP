#include "Commands/EpicUnrealMCPPropertyUtils.h"

#include "EditorAssetLibrary.h"
#include "JsonObjectConverter.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/Field.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectIterator.h"

#if PLATFORM_WINDOWS
#ifndef EXCEPTION_EXECUTE_HANDLER
#define EXCEPTION_EXECUTE_HANDLER 1
#endif
#endif

UClass* FEpicUnrealMCPPropertyUtils::ResolveAnyClass(const FString& ClassName)
{
	if (ClassName.IsEmpty())
	{
		return nullptr;
	}

	// Full path (e.g. "/Script/MassCrowd.MassCrowdVisualizationTrait")
	if (ClassName.Contains(TEXT(".")))
	{
		UClass* C = FindObject<UClass>(nullptr, *ClassName);
		if (C)
		{
			return C;
		}
		C = LoadObject<UClass>(nullptr, *ClassName);
		if (C)
		{
			return C;
		}
	}

	// Short name — handles U/A prefix and raw names
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* C = *It;
		if (!C)
		{
			continue;
		}
		const FString Name = C->GetName();
		if (Name == ClassName ||
			Name == (TEXT("U") + ClassName) ||
			Name == (TEXT("A") + ClassName) ||
			(ClassName.StartsWith(TEXT("U")) && Name == ClassName.RightChop(1)) ||
			(ClassName.StartsWith(TEXT("A")) && Name == ClassName.RightChop(1)))
		{
			return C;
		}
	}

	return nullptr;
}

void FEpicUnrealMCPPropertyUtils::SetPropertiesFromJson(
	UObject* Target, const TSharedPtr<FJsonObject>& Json, FString& OutErrors)
{
	if (!Target || !Json.IsValid())
	{
		return;
	}

	for (const auto& Pair : Json->Values)
	{
		if (Pair.Key == TEXT("_ClassName"))
		{
			continue;
		}

		FString Err;
		if (!SetProperty(Target, Pair.Key, Pair.Value, Err))
		{
			OutErrors += FString::Printf(TEXT("[%s: %s] "), *Pair.Key, *Err);
		}
	}
}

void FEpicUnrealMCPPropertyUtils::SetStructFieldsFromJson(
	UScriptStruct* Struct, void* StructData,
	const TSharedPtr<FJsonObject>& Json, UObject* Outer, FString& OutErrors)
{
	if (!Struct || !StructData || !Json.IsValid())
	{
		return;
	}

	for (const auto& Pair : Json->Values)
	{
		if (Pair.Key == TEXT("_ClassName"))
		{
			continue;
		}

		FProperty* FieldProp = nullptr;
		for (UStruct* S = Struct; S && !FieldProp; S = S->GetSuperStruct())
		{
			FieldProp = S->FindPropertyByName(*Pair.Key);
		}

		if (!FieldProp)
		{
			OutErrors += FString::Printf(TEXT("[field '%s' not found on struct '%s'] "),
				*Pair.Key, *Struct->GetName());
			continue;
		}

		void* FieldAddr = FieldProp->ContainerPtrToValuePtr<void>(StructData);
		FString FieldErr;
		if (!SetPropertyValueAtAddr(FieldProp, FieldAddr, Outer, Pair.Key, Pair.Value, FieldErr))
		{
			OutErrors += FString::Printf(TEXT("[%s: %s] "), *Pair.Key, *FieldErr);
		}
	}
}

bool FEpicUnrealMCPPropertyUtils::SetProperty(
	UObject* Object, const FString& PropertyName,
	const TSharedPtr<FJsonValue>& Value, FString& OutError)
{
	if (!Object)
	{
		OutError = TEXT("Null object");
		return false;
	}

	FProperty* Prop = nullptr;
	for (UClass* C = Object->GetClass(); C && !Prop; C = C->GetSuperClass())
	{
		Prop = C->FindPropertyByName(*PropertyName);
	}

	if (!Prop)
	{
		OutError = FString::Printf(TEXT("Property '%s' not found on class '%s'"),
			*PropertyName, *Object->GetClass()->GetName());
		return false;
	}

	void* PropAddr = Prop->ContainerPtrToValuePtr<void>(Object);
	return SetPropertyValueAtAddr(Prop, PropAddr, Object, PropertyName, Value, OutError);
}

bool FEpicUnrealMCPPropertyUtils::SetPropertyValueAtAddr(
	FProperty* Prop, void* PropAddr, UObject* Object,
	const FString& PropertyName, const TSharedPtr<FJsonValue>& Value, FString& OutError)
{
	if (!Prop || !PropAddr)
	{
		OutError = TEXT("Null property or address");
		return false;
	}
	if (!Value.IsValid())
	{
		OutError = TEXT("Null JSON value");
		return false;
	}

	// FObjectProperty — instanced subobject (JSON object with _ClassName)
	if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Prop))
	{
		if (ObjProp->HasAnyPropertyFlags(CPF_PersistentInstance | CPF_InstancedReference)
			&& Value->Type == EJson::Object)
		{
			const TSharedPtr<FJsonObject>& JsonObj = Value->AsObject();
			FString ClassPath;
			JsonObj->TryGetStringField(TEXT("_ClassName"), ClassPath);

			UClass* SubClass = ObjProp->PropertyClass;
			if (!ClassPath.IsEmpty() && ClassPath != TEXT("None"))
			{
				UClass* Resolved = FindObject<UClass>(nullptr, *ClassPath);
				if (!Resolved)
				{
					Resolved = LoadObject<UClass>(nullptr, *ClassPath);
				}
				if (Resolved)
				{
					SubClass = Resolved;
				}
			}

			UObject* SubObj = NewObject<UObject>(Object, SubClass);
			FString SubErrors;
			SetPropertiesFromJson(SubObj, JsonObj, SubErrors);
			ObjProp->SetObjectPropertyValue(PropAddr, SubObj);
			if (!SubErrors.IsEmpty())
			{
				OutError = FString::Printf(TEXT("Subobject '%s' field write errors: %s"),
					*PropertyName, *SubErrors);
				return false;
			}
			return true;
		}

		if (Value->Type == EJson::Null || Value->AsString() == TEXT("None") || Value->AsString().IsEmpty())
		{
			ObjProp->SetObjectPropertyValue(PropAddr, nullptr);
			return true;
		}

		FString PathStr = Value->AsString();
		UObject* LoadedObj = StaticLoadObject(ObjProp->PropertyClass, nullptr, *PathStr);
		if (!LoadedObj)
		{
			LoadedObj = UEditorAssetLibrary::LoadAsset(PathStr);
		}
		if (!LoadedObj)
		{
			OutError = FString::Printf(TEXT("Could not load object '%s' for property '%s'"),
				*PathStr, *PropertyName);
			return false;
		}
		ObjProp->SetObjectPropertyValue(PropAddr, LoadedObj);
		return true;
	}

	// FClassProperty — TSubclassOf<T> (e.g., HighResTemplateActor, RepresentationSubsystemClass)
	if (FClassProperty* ClassProp = CastField<FClassProperty>(Prop))
	{
		if (Value->Type == EJson::Null || Value->AsString() == TEXT("None") || Value->AsString().IsEmpty())
		{
			ClassProp->SetObjectPropertyValue(PropAddr, nullptr);
			return true;
		}

		FString ClassPath = Value->AsString();

		// Try loading as a class directly
		UClass* LoadedClass = LoadObject<UClass>(nullptr, *ClassPath);

		// If that fails, try appending _C for Blueprint classes
		if (!LoadedClass && !ClassPath.EndsWith(TEXT("_C")))
		{
			// Try /Game/Path/BP_Name.BP_Name_C format
			FString BPClassPath = ClassPath + TEXT("_C");
			LoadedClass = LoadObject<UClass>(nullptr, *BPClassPath);
		}

		if (!LoadedClass)
		{
			OutError = FString::Printf(TEXT("Could not load class '%s' for TSubclassOf property '%s'"),
				*ClassPath, *PropertyName);
			return false;
		}

		// Validate the class is compatible with the expected meta class
		if (!LoadedClass->IsChildOf(ClassProp->MetaClass))
		{
			OutError = FString::Printf(TEXT("Class '%s' is not a subclass of '%s' for property '%s'"),
				*LoadedClass->GetName(), *ClassProp->MetaClass->GetName(), *PropertyName);
			return false;
		}

		ClassProp->SetObjectPropertyValue(PropAddr, LoadedClass);
		return true;
	}

	// Other FObjectPropertyBase subclasses — always path-string based
	if (FObjectPropertyBase* ObjPropBase = CastField<FObjectPropertyBase>(Prop))
	{
		if (Value->Type == EJson::Null || Value->AsString() == TEXT("None") || Value->AsString().IsEmpty())
		{
			ObjPropBase->SetObjectPropertyValue(PropAddr, nullptr);
			return true;
		}

		FString PathStr = Value->AsString();
		UObject* LoadedObj = StaticLoadObject(ObjPropBase->PropertyClass, nullptr, *PathStr);
		if (!LoadedObj)
		{
			LoadedObj = UEditorAssetLibrary::LoadAsset(PathStr);
		}
		if (!LoadedObj)
		{
			OutError = FString::Printf(TEXT("Could not load object '%s' for property '%s'"),
				*PathStr, *PropertyName);
			return false;
		}
		ObjPropBase->SetObjectPropertyValue(PropAddr, LoadedObj);
		return true;
	}

	if (FSoftObjectProperty* SoftProp = CastField<FSoftObjectProperty>(Prop))
	{
		FSoftObjectPtr SoftPtr(FSoftObjectPath(Value->AsString()));
		SoftProp->SetPropertyValue(PropAddr, SoftPtr);
		return true;
	}

	if (FSoftClassProperty* SoftClassProp = CastField<FSoftClassProperty>(Prop))
	{
		FSoftObjectPtr SoftPtr(FSoftObjectPath(Value->AsString()));
		SoftClassProp->SetPropertyValue(PropAddr, SoftPtr);
		return true;
	}

	if (FNameProperty* NameProp = CastField<FNameProperty>(Prop))
	{
		NameProp->SetPropertyValue(PropAddr, FName(*Value->AsString()));
		return true;
	}

	if (FTextProperty* TextProp = CastField<FTextProperty>(Prop))
	{
		TextProp->SetPropertyValue(PropAddr, FText::FromString(Value->AsString()));
		return true;
	}

	// Numeric types
	if (FIntProperty* IntProp = CastField<FIntProperty>(Prop))
	{
		IntProp->SetPropertyValue(PropAddr, static_cast<int32>(Value->AsNumber()));
		return true;
	}
	if (FInt64Property* I64Prop = CastField<FInt64Property>(Prop))
	{
		I64Prop->SetPropertyValue(PropAddr, static_cast<int64>(Value->AsNumber()));
		return true;
	}
	if (FDoubleProperty* DblProp = CastField<FDoubleProperty>(Prop))
	{
		DblProp->SetPropertyValue(PropAddr, Value->AsNumber());
		return true;
	}
	if (FFloatProperty* FltProp = CastField<FFloatProperty>(Prop))
	{
		FltProp->SetPropertyValue(PropAddr, static_cast<float>(Value->AsNumber()));
		return true;
	}
	if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Prop))
	{
		BoolProp->SetPropertyValue(PropAddr, Value->AsBool());
		return true;
	}
	if (FStrProperty* StrProp = CastField<FStrProperty>(Prop))
	{
		StrProp->SetPropertyValue(PropAddr, Value->AsString());
		return true;
	}

	// Enums
	if (FByteProperty* ByteProp = CastField<FByteProperty>(Prop))
	{
		UEnum* Enum = ByteProp->GetIntPropertyEnum();
		if (Enum)
		{
			int64 EnumVal = INDEX_NONE;
			if (Value->Type == EJson::Number)
			{
				EnumVal = static_cast<int64>(Value->AsNumber());
			}
			else
			{
				FString Str = Value->AsString();
				if (Str.Contains(TEXT("::")))
				{
					Str.Split(TEXT("::"), nullptr, &Str);
				}
				EnumVal = Enum->GetValueByNameString(Str);
				if (EnumVal == INDEX_NONE)
				{
					EnumVal = Enum->GetValueByNameString(Value->AsString());
				}
			}
			if (EnumVal == INDEX_NONE)
			{
				OutError = FString::Printf(TEXT("Invalid enum value for '%s'"), *PropertyName);
				return false;
			}
			ByteProp->SetPropertyValue(PropAddr, static_cast<uint8>(EnumVal));
		}
		else
		{
			ByteProp->SetPropertyValue(PropAddr, static_cast<uint8>(Value->AsNumber()));
		}
		return true;
	}
	if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Prop))
	{
		UEnum* Enum = EnumProp->GetEnum();
		FNumericProperty* Underlying = EnumProp->GetUnderlyingProperty();
		if (Enum && Underlying)
		{
			int64 EnumVal = INDEX_NONE;
			if (Value->Type == EJson::Number)
			{
				EnumVal = static_cast<int64>(Value->AsNumber());
			}
			else
			{
				FString Str = Value->AsString();
				if (Str.Contains(TEXT("::")))
				{
					Str.Split(TEXT("::"), nullptr, &Str);
				}
				EnumVal = Enum->GetValueByNameString(Str);
				if (EnumVal == INDEX_NONE)
				{
					EnumVal = Enum->GetValueByNameString(Value->AsString());
				}
			}
			if (EnumVal == INDEX_NONE)
			{
				OutError = FString::Printf(TEXT("Invalid enum value for '%s'"), *PropertyName);
				return false;
			}
			Underlying->SetIntPropertyValue(PropAddr, EnumVal);
		}
		return true;
	}

	// Structs — use SetStructFieldsFromJson for nested instanced arrays
	if (FStructProperty* StructProp = CastField<FStructProperty>(Prop))
	{
		if (Value->Type == EJson::Object)
		{
			FString SubErrors;
			SetStructFieldsFromJson(StructProp->Struct, PropAddr, Value->AsObject(), Object, SubErrors);
			if (!SubErrors.IsEmpty())
			{
				OutError = FString::Printf(TEXT("Struct '%s' field write errors: %s"),
					*PropertyName, *SubErrors);
				return false;
			}
			return true;
		}
		// String fallback: use UE's ImportText for struct properties.
		// This handles complex structs with fixed-size arrays, enums, etc.
		// e.g., "(LODRepresentation[0]=HighResSpawnedActor,LODRepresentation[1]=HighResSpawnedActor,...)"
		if (Value->Type == EJson::String)
		{
			const FString TextValue = Value->AsString();
			const TCHAR* Buffer = *TextValue;
			if (StructProp->ImportText_Direct(Buffer, PropAddr, nullptr, PPF_None))
			{
				return true;
			}
			OutError = FString::Printf(TEXT("ImportText failed for struct property '%s' with value '%s'"),
				*PropertyName, *TextValue);
			return false;
		}
		OutError = FString::Printf(TEXT("Expected JSON object or text string for struct property '%s'"), *PropertyName);
		return false;
	}

	// Arrays — instanced object and FInstancedStruct get special handling
	if (FArrayProperty* ArrProp = CastField<FArrayProperty>(Prop))
	{
		// Instanced object array
		FObjectProperty* InnerObj = CastField<FObjectProperty>(ArrProp->Inner);
		if (InnerObj
			&& InnerObj->HasAnyPropertyFlags(CPF_PersistentInstance | CPF_InstancedReference)
			&& Value->Type == EJson::Array)
		{
			const TArray<TSharedPtr<FJsonValue>>& JsonArr = Value->AsArray();
			FScriptArrayHelper ArrHelper(ArrProp, PropAddr);
			ArrHelper.EmptyValues();
			ArrHelper.AddValues(JsonArr.Num());

			for (int32 i = 0; i < JsonArr.Num(); ++i)
			{
				const TSharedPtr<FJsonValue>& Elem = JsonArr[i];
				void* ElemAddr = ArrHelper.GetRawPtr(i);

				if (Elem->Type == EJson::Object)
				{
					const TSharedPtr<FJsonObject>& ElemJson = Elem->AsObject();
					FString ClassPath;
					ElemJson->TryGetStringField(TEXT("_ClassName"), ClassPath);

					UClass* ElemClass = InnerObj->PropertyClass;
					if (!ClassPath.IsEmpty() && ClassPath != TEXT("None"))
					{
						UClass* Resolved = FindObject<UClass>(nullptr, *ClassPath);
						if (!Resolved)
						{
							Resolved = LoadObject<UClass>(nullptr, *ClassPath);
						}
						if (Resolved)
						{
							ElemClass = Resolved;
						}
					}

					UObject* SubObj = NewObject<UObject>(Object, ElemClass);
					FString SubErrors;
					SetPropertiesFromJson(SubObj, ElemJson, SubErrors);
					InnerObj->SetObjectPropertyValue(ElemAddr, SubObj);
					if (!SubErrors.IsEmpty())
					{
						OutError = FString::Printf(TEXT("Array '%s' element %d errors: %s"),
							*PropertyName, i, *SubErrors);
						return false;
					}
				}
				else if (Elem->Type == EJson::String)
				{
					FString ElemPath = Elem->AsString();
					if (!ElemPath.IsEmpty() && ElemPath != TEXT("None"))
					{
						UObject* Loaded = StaticLoadObject(InnerObj->PropertyClass, nullptr, *ElemPath);
						InnerObj->SetObjectPropertyValue(ElemAddr, Loaded);
					}
				}
			}
			return true;
		}

		// FInstancedStruct array
		FStructProperty* InnerSP = CastField<FStructProperty>(ArrProp->Inner);
		if (InnerSP && InnerSP->Struct
			&& InnerSP->Struct->GetName() == TEXT("InstancedStruct")
			&& Value->Type == EJson::Array)
		{
			const TArray<TSharedPtr<FJsonValue>>& JsonArr = Value->AsArray();
			FScriptArrayHelper ArrHelper(ArrProp, PropAddr);
			ArrHelper.EmptyValues();
			ArrHelper.AddValues(JsonArr.Num());

			for (int32 i = 0; i < JsonArr.Num(); ++i)
			{
				const TSharedPtr<FJsonValue>& Elem = JsonArr[i];
				void* ElemAddr = ArrHelper.GetRawPtr(i);
				FInstancedStruct* InstStruct = reinterpret_cast<FInstancedStruct*>(ElemAddr);

				auto ResolveScriptStruct = [](const FString& StructPath) -> UScriptStruct*
				{
					if (StructPath.IsEmpty() || StructPath == TEXT("None"))
					{
						return nullptr;
					}

					UScriptStruct* S = FindObject<UScriptStruct>(nullptr, *StructPath);
					if (S)
					{
						return S;
					}
					S = LoadObject<UScriptStruct>(nullptr, *StructPath);
					if (S)
					{
						return S;
					}

					for (TObjectIterator<UScriptStruct> It; It; ++It)
					{
						const FString N = (*It)->GetName();
						if (N == StructPath || N == (TEXT("F") + StructPath) ||
							(StructPath.StartsWith(TEXT("F")) && N == StructPath.RightChop(1)) ||
							(*It)->GetPathName() == StructPath)
						{
							return *It;
						}
					}
					return nullptr;
				};

				if (Elem->Type == EJson::Object)
				{
					const TSharedPtr<FJsonObject>& ElemJson = Elem->AsObject();
					FString StructPath;
					ElemJson->TryGetStringField(TEXT("_ClassName"), StructPath);
					UScriptStruct* TargetStruct = ResolveScriptStruct(StructPath);

					if (TargetStruct)
					{
						InstStruct->InitializeAs(TargetStruct);
						uint8* StructMem = InstStruct->GetMutableMemory();
						if (StructMem)
						{
							for (const auto& InnerPair : ElemJson->Values)
							{
								if (InnerPair.Key == TEXT("_ClassName"))
								{
									continue;
								}
								FProperty* FieldProp = TargetStruct->FindPropertyByName(*InnerPair.Key);
								if (!FieldProp)
								{
									continue;
								}
								void* FieldAddr = FieldProp->ContainerPtrToValuePtr<void>(StructMem);
								FJsonObjectConverter::JsonValueToUProperty(InnerPair.Value, FieldProp, FieldAddr);
							}
						}
					}
					else if (!StructPath.IsEmpty())
					{
						UE_LOG(LogTemp, Warning, TEXT("SetProperty '%s'[%d]: could not resolve struct '%s'"),
							*PropertyName, i, *StructPath);
					}
				}
				else if (Elem->Type == EJson::String)
				{
					UScriptStruct* TargetStruct = ResolveScriptStruct(Elem->AsString());
					if (TargetStruct)
					{
						InstStruct->InitializeAs(TargetStruct);
					}
				}
			}
			return true;
		}

		if (FJsonObjectConverter::JsonValueToUProperty(Value, Prop, PropAddr))
		{
			return true;
		}
		OutError = FString::Printf(TEXT("Failed to set array property '%s'"), *PropertyName);
		return false;
	}

	if (Prop->IsA<FSetProperty>() || Prop->IsA<FMapProperty>())
	{
		if (FJsonObjectConverter::JsonValueToUProperty(Value, Prop, PropAddr))
		{
			return true;
		}
		OutError = FString::Printf(TEXT("Failed to set container property '%s'"), *PropertyName);
		return false;
	}

	// Fallback
	if (FJsonObjectConverter::JsonValueToUProperty(Value, Prop, PropAddr))
	{
		return true;
	}

	OutError = FString::Printf(TEXT("Unsupported property type '%s' for property '%s'"),
		*Prop->GetClass()->GetName(), *PropertyName);
	return false;
}

namespace MCPPropertySafety
{
	/** Returns true if the property type is dangerous to serialize (delegates, weak refs, etc.). */
	static bool ShouldSkipPropertyType(FProperty* Prop)
	{
		if (!Prop)
		{
			return true;
		}

		if (Prop->IsA<FDelegateProperty>())
		{
			return true;
		}
		if (Prop->IsA<FMulticastDelegateProperty>())
		{
			return true;
		}
		if (Prop->IsA<FInterfaceProperty>())
		{
			return true;
		}
		if (Prop->IsA<FWeakObjectProperty>())
		{
			return true;
		}
		if (Prop->IsA<FLazyObjectProperty>())
		{
			return true;
		}
		if (Prop->IsA<FFieldPathProperty>())
		{
			return true;
		}
		if (Prop->HasAnyPropertyFlags(CPF_Deprecated))
		{
			return true;
		}

		// Skip internal UMG bookkeeping that often contains uninitialised data
		const FString PropName = Prop->GetName();
		if (PropName == TEXT("Slot") || PropName == TEXT("Navigation"))
		{
			return true;
		}

		if (const FObjectProperty* ObjProp = CastField<FObjectProperty>(Prop))
		{
			if (ObjProp->PropertyClass)
			{
				const FString ClassName = ObjProp->PropertyClass->GetName();
				if (ClassName.StartsWith(TEXT("SWidget")) || ClassName.StartsWith(TEXT("Slate")))
				{
					return true;
				}
			}
		}

		return false;
	}

#if PLATFORM_WINDOWS
	// SEH trampoline — must not have C++ objects with non-trivial destructors
	static int WindowsExecuteGuarded(void (*Func)(void*), void* Context)
	{
		__try
		{
			Func(Context);
			return 1;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return 0;
		}
	}
#endif

	struct FNameToStringCtx
	{
		const FName* Name;
		FString* OutString;
	};

	static void DoNameToString(void* Context)
	{
		FNameToStringCtx* Ctx = static_cast<FNameToStringCtx*>(Context);
		*(Ctx->OutString) = Ctx->Name->ToString();
	}
}

TSharedPtr<FJsonValue> FEpicUnrealMCPPropertyUtils::SafePropertyToJsonValue(
	FProperty* Property, const void* Value)
{
	if (!Property || MCPPropertySafety::ShouldSkipPropertyType(Property))
	{
		return MakeShared<FJsonValueNull>();
	}

	// Primitives
	if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
	{
		return MakeShared<FJsonValueBoolean>(BoolProp->GetPropertyValue(Value));
	}
	if (FIntProperty* IntProp = CastField<FIntProperty>(Property))
	{
		return MakeShared<FJsonValueNumber>(IntProp->GetPropertyValue(Value));
	}
	if (FInt64Property* Int64Prop = CastField<FInt64Property>(Property))
	{
		return MakeShared<FJsonValueNumber>(Int64Prop->GetPropertyValue(Value));
	}
	if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
	{
		return MakeShared<FJsonValueNumber>(FloatProp->GetPropertyValue(Value));
	}
	if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Property))
	{
		return MakeShared<FJsonValueNumber>(DoubleProp->GetPropertyValue(Value));
	}
	if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
	{
		return MakeShared<FJsonValueString>(StrProp->GetPropertyValue(Value));
	}

	if (FNameProperty* NameProp = CastField<FNameProperty>(Property))
	{
		FString Result;
		const FName Name = NameProp->GetPropertyValue(Value);

#if PLATFORM_WINDOWS
		MCPPropertySafety::FNameToStringCtx Ctx{ &Name, &Result };
		if (!MCPPropertySafety::WindowsExecuteGuarded(&MCPPropertySafety::DoNameToString, &Ctx))
		{
			Result = TEXT("<invalid name>");
		}
#else
		Result = Name.ToString();
#endif
		return MakeShared<FJsonValueString>(Result);
	}

	if (FTextProperty* TextProp = CastField<FTextProperty>(Property))
	{
		return MakeShared<FJsonValueString>(TextProp->GetPropertyValue(Value).ToString());
	}

	// Enums
	if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
	{
		if (UEnum* Enum = EnumProp->GetEnum())
		{
			FNumericProperty* UnderlyingProp = EnumProp->GetUnderlyingProperty();
			const int64 IntValue = UnderlyingProp->GetSignedIntPropertyValue(Value);
			if (Enum->GetIndexByValue(IntValue) != INDEX_NONE)
			{
				return MakeShared<FJsonValueString>(Enum->GetNameStringByValue(IntValue));
			}
			return MakeShared<FJsonValueString>(FString::Printf(TEXT("<invalid enum %lld>"), IntValue));
		}
	}
	if (FByteProperty* ByteProp = CastField<FByteProperty>(Property))
	{
		if (UEnum* Enum = ByteProp->GetIntPropertyEnum())
		{
			const int64 IntValue = ByteProp->GetSignedIntPropertyValue(Value);
			if (Enum->GetIndexByValue(IntValue) != INDEX_NONE)
			{
				return MakeShared<FJsonValueString>(Enum->GetNameStringByValue(IntValue));
			}
			return MakeShared<FJsonValueString>(FString::Printf(TEXT("<invalid enum %lld>"), IntValue));
		}
		return MakeShared<FJsonValueNumber>(ByteProp->GetPropertyValue(Value));
	}

	// Arrays
	if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
	{
		FScriptArrayHelper Helper(ArrayProp, Value);
		TArray<TSharedPtr<FJsonValue>> OutArray;

		for (int32 i = 0; i < Helper.Num(); ++i)
		{
			TSharedPtr<FJsonValue> Val = SafePropertyToJsonValue(ArrayProp->Inner, Helper.GetRawPtr(i));
			if (Val.IsValid() && Val->Type != EJson::Null)
			{
				OutArray.Add(Val);
			}
		}
		return MakeShared<FJsonValueArray>(OutArray);
	}

	// Sets
	if (FSetProperty* SetProp = CastField<FSetProperty>(Property))
	{
		FScriptSetHelper Helper(SetProp, Value);
		TArray<TSharedPtr<FJsonValue>> OutArray;

		for (int32 i = 0; i < Helper.Num(); ++i)
		{
			TSharedPtr<FJsonValue> Val = SafePropertyToJsonValue(SetProp->ElementProp, Helper.GetElementPtr(i));
			if (Val.IsValid() && Val->Type != EJson::Null)
			{
				OutArray.Add(Val);
			}
		}
		return MakeShared<FJsonValueArray>(OutArray);
	}

	// Maps
	if (FMapProperty* MapProp = CastField<FMapProperty>(Property))
	{
		FScriptMapHelper Helper(MapProp, Value);
		TSharedPtr<FJsonObject> OutObject = MakeShared<FJsonObject>();

		for (int32 i = 0; i < Helper.Num(); ++i)
		{
			uint8* PairPtr = Helper.GetPairPtr(i);
			const void* KeyPtr = MapProp->KeyProp->ContainerPtrToValuePtr<void>(PairPtr);
			const void* ValPtr = MapProp->ValueProp->ContainerPtrToValuePtr<void>(PairPtr);

			FString KeyStr;
			if (FStrProperty* KeyS = CastField<FStrProperty>(MapProp->KeyProp))
			{
				KeyStr = KeyS->GetPropertyValue(KeyPtr);
			}
			else if (FNameProperty* KeyN = CastField<FNameProperty>(MapProp->KeyProp))
			{
				const FName KeyName = KeyN->GetPropertyValue(KeyPtr);
#if PLATFORM_WINDOWS
				MCPPropertySafety::FNameToStringCtx KeyCtx{ &KeyName, &KeyStr };
				if (!MCPPropertySafety::WindowsExecuteGuarded(&MCPPropertySafety::DoNameToString, &KeyCtx))
				{
					KeyStr = FString::Printf(TEXT("Key_%d"), i);
				}
#else
				KeyStr = KeyName.ToString();
#endif
			}
			else
			{
				KeyStr = FString::Printf(TEXT("Key_%d"), i);
			}

			OutObject->SetField(KeyStr, SafePropertyToJsonValue(MapProp->ValueProp, ValPtr));
		}
		return MakeShared<FJsonValueObject>(OutObject);
	}

	// Structs — recursive safe serialization
	if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
	{
		UScriptStruct* Struct = StructProp->Struct;
		if (!Struct)
		{
			return MakeShared<FJsonValueNull>();
		}

		TSharedPtr<FJsonObject> OutObject = MakeShared<FJsonObject>();
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			FProperty* Prop = *It;
			if (MCPPropertySafety::ShouldSkipPropertyType(Prop))
			{
				continue;
			}

			TSharedPtr<FJsonValue> Val = SafePropertyToJsonValue(Prop, Prop->ContainerPtrToValuePtr<void>(Value));
			if (Val.IsValid() && Val->Type != EJson::Null)
			{
				OutObject->SetField(Prop->GetName(), Val);
			}
		}
		return MakeShared<FJsonValueObject>(OutObject);
	}

	// Objects
	if (FObjectPropertyBase* ObjectProp = CastField<FObjectPropertyBase>(Property))
	{
		UObject* Object = ObjectProp->GetObjectPropertyValue(Value);
		if (Object)
		{
			return MakeShared<FJsonValueString>(Object->GetPathName());
		}
		return MakeShared<FJsonValueNull>();
	}

	// Catch-all for remaining numeric types (int8, int16, uint16, uint32, uint64)
	if (FNumericProperty* NumProp = CastField<FNumericProperty>(Property))
	{
		if (NumProp->IsFloatingPoint())
		{
			return MakeShared<FJsonValueNumber>(NumProp->GetFloatingPointPropertyValue(Value));
		}
		return MakeShared<FJsonValueNumber>(static_cast<double>(NumProp->GetSignedIntPropertyValue(Value)));
	}

	return MakeShared<FJsonValueString>(TEXT("<unsupported type>"));
}

bool FEpicUnrealMCPPropertyUtils::SetPropertyAtPath(
	UObject* Root, const FString& PropertyPath,
	const TSharedPtr<FJsonValue>& Value, FString& OutError,
	FProperty** OutTopLevelProperty)
{
	if (!Root) { OutError = TEXT("Null root object"); return false; }
	if (PropertyPath.IsEmpty()) { OutError = TEXT("Empty property path"); return false; }

	TArray<FString> Segments;
	PropertyPath.ParseIntoArray(Segments, TEXT("."));
	if (Segments.Num() == 0) { OutError = TEXT("Invalid property path"); return false; }

	UStruct* CurrentStruct = Root->GetClass();
	void* CurrentAddr = Root;
	FProperty* TopLevelProp = nullptr;

	for (int32 i = 0; i < Segments.Num(); ++i)
	{
		FString Segment = Segments[i];
		int32 ArrayIndex = INDEX_NONE;
		int32 BracketPos = INDEX_NONE;
		if (Segment.FindChar(TEXT('['), BracketPos))
		{
			int32 CloseBracket = INDEX_NONE;
			if (!Segment.FindChar(TEXT(']'), CloseBracket) || CloseBracket <= BracketPos + 1)
			{
				OutError = FString::Printf(TEXT("Invalid array syntax in segment '%s'"), *Segment);
				return false;
			}
			const FString IdxStr = Segment.Mid(BracketPos + 1, CloseBracket - BracketPos - 1);
			ArrayIndex = FCString::Atoi(*IdxStr);
			Segment = Segment.Left(BracketPos);
		}

		FProperty* Prop = nullptr;
		for (UStruct* S = CurrentStruct; S && !Prop; S = S->GetSuperStruct())
		{
			Prop = S->FindPropertyByName(*Segment);
		}
		if (!Prop)
		{
			OutError = FString::Printf(TEXT("Property '%s' not found at path segment %d (in struct '%s')"),
				*Segment, i, CurrentStruct ? *CurrentStruct->GetName() : TEXT("?"));
			return false;
		}

		if (i == 0) { TopLevelProp = Prop; }

		void* PropAddr = Prop->ContainerPtrToValuePtr<void>(CurrentAddr);

		FProperty* EffectiveProp = Prop;
		void* EffectiveAddr = PropAddr;
		if (ArrayIndex != INDEX_NONE)
		{
			if (FArrayProperty* ArrProp = CastField<FArrayProperty>(Prop))
			{
				FScriptArrayHelper Helper(ArrProp, PropAddr);
				if (!Helper.IsValidIndex(ArrayIndex))
				{
					OutError = FString::Printf(TEXT("Array index %d out of bounds for '%s' (size %d)"),
						ArrayIndex, *Segment, Helper.Num());
					return false;
				}
				EffectiveProp = ArrProp->Inner;
				EffectiveAddr = Helper.GetRawPtr(ArrayIndex);
			}
			else if (Prop->ArrayDim > 1)
			{
				// C-style fixed array (e.g. FLinearColor LensFlareTints[8]).
				if (ArrayIndex < 0 || ArrayIndex >= Prop->ArrayDim)
				{
					OutError = FString::Printf(TEXT("Static array index %d out of bounds for '%s' (size %d)"),
						ArrayIndex, *Segment, Prop->ArrayDim);
					return false;
				}
				EffectiveProp = Prop;
				EffectiveAddr = Prop->ContainerPtrToValuePtr<void>(CurrentAddr, ArrayIndex);
			}
			else if (FSetProperty* SetProp = CastField<FSetProperty>(Prop))
			{
				FScriptSetHelper Helper(SetProp, PropAddr);
				if (ArrayIndex < 0 || ArrayIndex >= Helper.Num())
				{
					OutError = FString::Printf(TEXT("Set index %d out of bounds for '%s' (size %d)"),
						ArrayIndex, *Segment, Helper.Num());
					return false;
				}
				EffectiveProp = SetProp->ElementProp;
				EffectiveAddr = Helper.GetElementPtr(ArrayIndex);
			}
			else
			{
				OutError = FString::Printf(TEXT("Property '%s' is not an array, set, or static C array but index [%d] was given (type %s)"),
					*Segment, ArrayIndex, *Prop->GetClass()->GetName());
				return false;
			}
		}

		const bool bIsLeaf = (i == Segments.Num() - 1);
		if (bIsLeaf)
		{
			if (OutTopLevelProperty) { *OutTopLevelProperty = TopLevelProp; }
			return SetPropertyValueAtAddr(EffectiveProp, EffectiveAddr, Root, Segment, Value, OutError);
		}

		if (FStructProperty* SP = CastField<FStructProperty>(EffectiveProp))
		{
			if (!SP->Struct)
			{
				OutError = FString::Printf(TEXT("Struct property '%s' has no UScriptStruct"), *Segment);
				return false;
			}
			CurrentStruct = SP->Struct;
			CurrentAddr = EffectiveAddr;
			continue;
		}

		if (FObjectProperty* OP = CastField<FObjectProperty>(EffectiveProp))
		{
			UObject* SubObj = OP->GetObjectPropertyValue(EffectiveAddr);
			if (!SubObj)
			{
				OutError = FString::Printf(TEXT("Object property '%s' is null, cannot descend"), *Segment);
				return false;
			}
			CurrentStruct = SubObj->GetClass();
			CurrentAddr = SubObj;
			continue;
		}

		OutError = FString::Printf(TEXT("Cannot descend through property '%s' (type %s)"),
			*Segment, *EffectiveProp->GetClass()->GetName());
		return false;
	}

	return false;
}

FString FEpicUnrealMCPPropertyUtils::GetPropertyTypeDescription(FProperty* Prop)
{
	if (!Prop) { return TEXT("?"); }

	if (Prop->IsA<FBoolProperty>()) { return TEXT("bool"); }
	if (Prop->IsA<FFloatProperty>()) { return TEXT("float"); }
	if (Prop->IsA<FDoubleProperty>()) { return TEXT("double"); }
	if (Prop->IsA<FIntProperty>()) { return TEXT("int32"); }
	if (Prop->IsA<FInt64Property>()) { return TEXT("int64"); }
	if (Prop->IsA<FStrProperty>()) { return TEXT("FString"); }
	if (Prop->IsA<FNameProperty>()) { return TEXT("FName"); }
	if (Prop->IsA<FTextProperty>()) { return TEXT("FText"); }

	if (FByteProperty* BP = CastField<FByteProperty>(Prop))
	{
		return BP->Enum ? FString::Printf(TEXT("TEnumAsByte<%s>"), *BP->Enum->GetName()) : FString(TEXT("uint8"));
	}
	if (FEnumProperty* EP = CastField<FEnumProperty>(Prop))
	{
		return EP->GetEnum() ? EP->GetEnum()->GetName() : FString(TEXT("enum"));
	}
	if (FStructProperty* SP = CastField<FStructProperty>(Prop))
	{
		return SP->Struct ? FString::Printf(TEXT("F%s"), *SP->Struct->GetName()) : FString(TEXT("FStruct"));
	}
	if (FClassProperty* CP = CastField<FClassProperty>(Prop))
	{
		return CP->MetaClass ? FString::Printf(TEXT("TSubclassOf<%s>"), *CP->MetaClass->GetName()) : FString(TEXT("UClass*"));
	}
	if (FSoftClassProperty* SCP = CastField<FSoftClassProperty>(Prop))
	{
		return SCP->MetaClass ? FString::Printf(TEXT("TSoftClassPtr<%s>"), *SCP->MetaClass->GetName()) : FString(TEXT("FSoftClassPath"));
	}
	if (FSoftObjectProperty* SOP = CastField<FSoftObjectProperty>(Prop))
	{
		return SOP->PropertyClass ? FString::Printf(TEXT("TSoftObjectPtr<%s>"), *SOP->PropertyClass->GetName()) : FString(TEXT("FSoftObjectPath"));
	}
	if (FObjectPropertyBase* OPB = CastField<FObjectPropertyBase>(Prop))
	{
		return OPB->PropertyClass ? FString::Printf(TEXT("%s*"), *OPB->PropertyClass->GetName()) : FString(TEXT("UObject*"));
	}
	if (FArrayProperty* AP = CastField<FArrayProperty>(Prop))
	{
		return FString::Printf(TEXT("TArray<%s>"), *GetPropertyTypeDescription(AP->Inner));
	}
	if (FSetProperty* SP2 = CastField<FSetProperty>(Prop))
	{
		return FString::Printf(TEXT("TSet<%s>"), *GetPropertyTypeDescription(SP2->ElementProp));
	}
	if (FMapProperty* MP = CastField<FMapProperty>(Prop))
	{
		return FString::Printf(TEXT("TMap<%s,%s>"),
			*GetPropertyTypeDescription(MP->KeyProp), *GetPropertyTypeDescription(MP->ValueProp));
	}
	return Prop->GetClass()->GetName();
}

TSharedPtr<FJsonObject> FEpicUnrealMCPPropertyUtils::GetPropertyMetadata(
	FProperty* Prop, const FString& FullPath, const void* ValueAddr,
	bool bIncludeValue, bool bExpandEnum)
{
	TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
	if (!Prop) { return Out; }

	Out->SetStringField(TEXT("path"), FullPath);
	Out->SetStringField(TEXT("name"), Prop->GetName());
	Out->SetStringField(TEXT("cpp_type"), Prop->GetClass()->GetName());
	Out->SetStringField(TEXT("ue_type"), GetPropertyTypeDescription(Prop));

	const FString Category = Prop->GetMetaData(TEXT("Category"));
	if (!Category.IsEmpty()) { Out->SetStringField(TEXT("category"), Category); }
	const FString DisplayName = Prop->GetMetaData(TEXT("DisplayName"));
	if (!DisplayName.IsEmpty()) { Out->SetStringField(TEXT("display_name"), DisplayName); }
	const FString ToolTip = Prop->GetMetaData(TEXT("ToolTip"));
	if (!ToolTip.IsEmpty()) { Out->SetStringField(TEXT("tooltip"), ToolTip); }
	const FString EditCond = Prop->GetMetaData(TEXT("EditCondition"));
	if (!EditCond.IsEmpty()) { Out->SetStringField(TEXT("edit_condition"), EditCond); }

	const bool bIsByteEnum = Prop->IsA<FByteProperty>() && CastField<FByteProperty>(Prop)->Enum != nullptr;
	Out->SetBoolField(TEXT("is_struct"), Prop->IsA<FStructProperty>());
	Out->SetBoolField(TEXT("is_array"), Prop->IsA<FArrayProperty>() || Prop->ArrayDim > 1);
	Out->SetBoolField(TEXT("is_enum"), Prop->IsA<FEnumProperty>() || bIsByteEnum);
	Out->SetBoolField(TEXT("is_object"), Prop->IsA<FObjectPropertyBase>());
	Out->SetBoolField(TEXT("is_bool"), Prop->IsA<FBoolProperty>());

	if (Prop->ArrayDim > 1) { Out->SetNumberField(TEXT("array_dim"), Prop->ArrayDim); }

	Out->SetBoolField(TEXT("editable"), Prop->HasAnyPropertyFlags(CPF_Edit));
	Out->SetBoolField(TEXT("transient"), Prop->HasAnyPropertyFlags(CPF_Transient));
	Out->SetBoolField(TEXT("readonly"), Prop->HasAnyPropertyFlags(CPF_BlueprintReadOnly));

	if (Prop->HasMetaData(TEXT("ClampMin"))) { Out->SetStringField(TEXT("clamp_min"), Prop->GetMetaData(TEXT("ClampMin"))); }
	if (Prop->HasMetaData(TEXT("ClampMax"))) { Out->SetStringField(TEXT("clamp_max"), Prop->GetMetaData(TEXT("ClampMax"))); }
	if (Prop->HasMetaData(TEXT("UIMin"))) { Out->SetStringField(TEXT("ui_min"), Prop->GetMetaData(TEXT("UIMin"))); }
	if (Prop->HasMetaData(TEXT("UIMax"))) { Out->SetStringField(TEXT("ui_max"), Prop->GetMetaData(TEXT("UIMax"))); }

	if (FStructProperty* SP = CastField<FStructProperty>(Prop))
	{
		if (SP->Struct) { Out->SetStringField(TEXT("struct_type"), SP->Struct->GetName()); }
	}
	if (FClassProperty* CP = CastField<FClassProperty>(Prop))
	{
		if (CP->MetaClass) { Out->SetStringField(TEXT("meta_class"), CP->MetaClass->GetPathName()); }
	}
	if (FSoftClassProperty* SCP = CastField<FSoftClassProperty>(Prop))
	{
		if (SCP->MetaClass) { Out->SetStringField(TEXT("meta_class"), SCP->MetaClass->GetPathName()); }
	}
	if (FObjectPropertyBase* OPB = CastField<FObjectPropertyBase>(Prop))
	{
		if (OPB->PropertyClass) { Out->SetStringField(TEXT("object_class"), OPB->PropertyClass->GetPathName()); }
	}

	UEnum* Enum = nullptr;
	if (FEnumProperty* EP = CastField<FEnumProperty>(Prop)) { Enum = EP->GetEnum(); }
	else if (FByteProperty* BP = CastField<FByteProperty>(Prop)) { Enum = BP->Enum; }
	if (Enum)
	{
		Out->SetStringField(TEXT("enum_type"), Enum->GetName());
		if (bExpandEnum)
		{
			TArray<TSharedPtr<FJsonValue>> Values;
			const int32 Count = Enum->NumEnums();
			for (int32 i = 0; i < Count - 1; ++i)
			{
				if (Enum->HasMetaData(TEXT("Hidden"), i)) { continue; }
				Values.Add(MakeShared<FJsonValueString>(Enum->GetNameStringByIndex(i)));
			}
			Out->SetArrayField(TEXT("valid_values"), Values);
		}
	}

	if (FArrayProperty* AP = CastField<FArrayProperty>(Prop))
	{
		Out->SetStringField(TEXT("inner_type"), GetPropertyTypeDescription(AP->Inner));
	}
	if (FSetProperty* SP2 = CastField<FSetProperty>(Prop))
	{
		Out->SetStringField(TEXT("element_type"), GetPropertyTypeDescription(SP2->ElementProp));
	}
	if (FMapProperty* MP = CastField<FMapProperty>(Prop))
	{
		Out->SetStringField(TEXT("key_type"), GetPropertyTypeDescription(MP->KeyProp));
		Out->SetStringField(TEXT("value_type"), GetPropertyTypeDescription(MP->ValueProp));
	}

	if (bIncludeValue && ValueAddr)
	{
		TSharedPtr<FJsonValue> Val = SafePropertyToJsonValue(Prop, ValueAddr);
		if (Val.IsValid()) { Out->SetField(TEXT("current_value"), Val); }
	}

	return Out;
}

namespace MCPFlatWalker
{
	struct FSummaryCtx
	{
		int32 TotalAvailable = 0;   // matches filter+category, ignoring cap
		int32 TotalReturned = 0;    // actually emitted
		int32 OwnCount = 0;
		int32 InheritedCount = 0;
		TMap<FString, int32> CategoryCounts;  // path-filter scope, ignoring category filter
		UClass* MostDerivedClass = nullptr;
	};

	static bool MatchesPathFilter(const FString& FullPath, const FString& FilterLower)
	{
		return FilterLower.IsEmpty() || FullPath.ToLower().Contains(FilterLower);
	}

	static bool MatchesCategoryFilter(FProperty* P, const FString& CategoryLower)
	{
		if (CategoryLower.IsEmpty()) { return true; }
		return P->GetMetaData(TEXT("Category")).ToLower() == CategoryLower;
	}

	static bool IsOwnProp(FProperty* P, UClass* MostDerivedClass)
	{
		if (!MostDerivedClass) { return true; }
		return P->GetOwnerClass() == MostDerivedClass;
	}

	static void EmitOrSkip(
		FProperty* P, const void* Addr, const FString& FullPath,
		const FPropertyWalkOptions& Opts, FSummaryCtx& Ctx, TSharedPtr<FJsonObject>& Out)
	{
		const bool bPathOk = MatchesPathFilter(FullPath, Opts.FilterLower);
		if (!bPathOk) { return; }

		// Track category breakdown across path-filtered properties (regardless of category filter).
		FString Cat = P->GetMetaData(TEXT("Category"));
		if (Cat.IsEmpty()) { Cat = TEXT("(uncategorized)"); }
		Ctx.CategoryCounts.FindOrAdd(Cat)++;

		if (!MatchesCategoryFilter(P, Opts.CategoryLower)) { return; }

		++Ctx.TotalAvailable;
		if (IsOwnProp(P, Ctx.MostDerivedClass)) { ++Ctx.OwnCount; } else { ++Ctx.InheritedCount; }

		// Cursor skip + cap.
		if (Ctx.TotalAvailable <= Opts.Cursor) { return; }
		if (Ctx.TotalReturned >= Opts.MaxEntries) { return; }

		if (Opts.bIncludeMetadata)
		{
			Out->SetObjectField(FullPath, FEpicUnrealMCPPropertyUtils::GetPropertyMetadata(P, FullPath, Addr, true, Opts.bExpandEnum));
		}
		else
		{
			TSharedPtr<FJsonValue> V = FEpicUnrealMCPPropertyUtils::SafePropertyToJsonValue(P, Addr);
			if (V.IsValid()) { Out->SetField(FullPath, V); }
		}
		++Ctx.TotalReturned;
	}

	static void WalkRec(
		UStruct* Struct, const void* Addr, const FString& Prefix, int32 RemainingDepth,
		const FPropertyWalkOptions& Opts, FSummaryCtx& Ctx, TSharedPtr<FJsonObject>& Out)
	{
		if (!Struct || !Addr) { return; }

		const EFieldIteratorFlags::SuperClassFlags SuperFlag =
			Opts.bIncludeInherited ? EFieldIteratorFlags::IncludeSuper : EFieldIteratorFlags::ExcludeSuper;

		for (TFieldIterator<FProperty> It(Struct, SuperFlag); It; ++It)
		{
			FProperty* P = *It;
			if (!P) { continue; }
			const FString Name = P->GetName();
			const FString FullPath = Prefix.IsEmpty() ? Name : (Prefix + TEXT(".") + Name);
			const void* PropAddr = P->ContainerPtrToValuePtr<void>(Addr);

			const bool bIsStruct = P->IsA<FStructProperty>();
			const bool bIsArrayDyn = P->IsA<FArrayProperty>();
			const bool bIsArrayStatic = P->ArrayDim > 1;
			const bool bWillRecurse = (RemainingDepth > 0) && (bIsStruct || bIsArrayDyn || bIsArrayStatic);

			if (!bWillRecurse)
			{
				EmitOrSkip(P, PropAddr, FullPath, Opts, Ctx, Out);
				continue;
			}

			if (bIsStruct)
			{
				FStructProperty* SP = CastField<FStructProperty>(P);
				if (SP && SP->Struct)
				{
					WalkRec(SP->Struct, PropAddr, FullPath, RemainingDepth - 1, Opts, Ctx, Out);
				}
				continue;
			}

			if (bIsArrayDyn && Opts.bExpandArrays)
			{
				FArrayProperty* AP = CastField<FArrayProperty>(P);
				FScriptArrayHelper Helper(AP, PropAddr);
				const int32 Limit = FMath::Min(Helper.Num(), Opts.ArrayElementLimit);
				for (int32 i = 0; i < Limit; ++i)
				{
					const FString ElemPath = FString::Printf(TEXT("%s[%d]"), *FullPath, i);
					const void* ElemAddr = Helper.GetRawPtr(i);
					if (FStructProperty* InnerSP = CastField<FStructProperty>(AP->Inner); InnerSP && InnerSP->Struct && RemainingDepth > 1)
					{
						WalkRec(InnerSP->Struct, ElemAddr, ElemPath, RemainingDepth - 1, Opts, Ctx, Out);
					}
					else
					{
						EmitOrSkip(AP->Inner, ElemAddr, ElemPath, Opts, Ctx, Out);
					}
				}
				continue;
			}

			if (bIsArrayStatic && Opts.bExpandArrays)
			{
				const int32 Limit = FMath::Min(P->ArrayDim, Opts.ArrayElementLimit);
				for (int32 i = 0; i < Limit; ++i)
				{
					const FString ElemPath = FString::Printf(TEXT("%s[%d]"), *FullPath, i);
					const void* ElemAddr = P->ContainerPtrToValuePtr<void>(Addr, i);
					if (FStructProperty* InnerSP = CastField<FStructProperty>(P); InnerSP && InnerSP->Struct && RemainingDepth > 1)
					{
						WalkRec(InnerSP->Struct, ElemAddr, ElemPath, RemainingDepth - 1, Opts, Ctx, Out);
					}
					else
					{
						EmitOrSkip(P, ElemAddr, ElemPath, Opts, Ctx, Out);
					}
				}
				continue;
			}
		}
	}

	static TSharedPtr<FJsonObject> BuildSummary(
		UStruct* StartStruct, const FSummaryCtx& Ctx, const FPropertyWalkOptions& Opts,
		bool bIsObjectDescent, bool bDescendedIntoObject)
	{
		TSharedPtr<FJsonObject> Summary = MakeShared<FJsonObject>();
		Summary->SetNumberField(TEXT("total_available"), Ctx.TotalAvailable);
		Summary->SetNumberField(TEXT("total_returned"), Ctx.TotalReturned);

		const bool bTruncated = Ctx.TotalReturned + Opts.Cursor < Ctx.TotalAvailable;
		Summary->SetBoolField(TEXT("truncated"), bTruncated);
		if (bTruncated)
		{
			Summary->SetNumberField(TEXT("next_cursor"), Opts.Cursor + Ctx.TotalReturned);
		}
		else
		{
			Summary->SetField(TEXT("next_cursor"), MakeShared<FJsonValueNull>());
		}

		Summary->SetNumberField(TEXT("own_count"), Ctx.OwnCount);
		Summary->SetNumberField(TEXT("inherited_count"), Ctx.InheritedCount);
		Summary->SetBoolField(TEXT("is_object_descent"), bIsObjectDescent);
		Summary->SetBoolField(TEXT("descended_into_object"), bDescendedIntoObject);

		// Class chain — most-derived first, walking up super chain.
		if (UClass* AsClass = Cast<UClass>(StartStruct))
		{
			TArray<TSharedPtr<FJsonValue>> Chain;
			for (UClass* C = AsClass; C; C = C->GetSuperClass())
			{
				Chain.Add(MakeShared<FJsonValueString>(C->GetName()));
			}
			Summary->SetArrayField(TEXT("class_chain"), Chain);
		}
		else if (UScriptStruct* AsStruct = Cast<UScriptStruct>(StartStruct))
		{
			TArray<TSharedPtr<FJsonValue>> Chain;
			for (UStruct* S = AsStruct; S; S = S->GetSuperStruct())
			{
				Chain.Add(MakeShared<FJsonValueString>(S->GetName()));
			}
			Summary->SetArrayField(TEXT("struct_chain"), Chain);
		}

		// Categories breakdown (path-filtered scope, sorted by count desc).
		if (Ctx.CategoryCounts.Num() > 0)
		{
			TArray<TPair<FString, int32>> Sorted;
			for (const auto& Pair : Ctx.CategoryCounts) { Sorted.Add(TPair<FString, int32>(Pair.Key, Pair.Value)); }
			Sorted.Sort([](const TPair<FString, int32>& A, const TPair<FString, int32>& B) { return A.Value > B.Value; });

			TSharedPtr<FJsonObject> Categories = MakeShared<FJsonObject>();
			for (const auto& Pair : Sorted)
			{
				Categories->SetNumberField(Pair.Key, Pair.Value);
			}
			Summary->SetObjectField(TEXT("categories"), Categories);
		}
		return Summary;
	}
}

TSharedPtr<FJsonObject> FEpicUnrealMCPPropertyUtils::SerializePropertiesFlat(
	UObject* Object, const FPropertyWalkOptions& Options)
{
	TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
	if (!Object) { return Out; }

	MCPFlatWalker::FSummaryCtx Ctx;
	Ctx.MostDerivedClass = Object->GetClass();

	MCPFlatWalker::WalkRec(Object->GetClass(), Object, TEXT(""), Options.MaxDepth, Options, Ctx, Out);

	Out->SetObjectField(TEXT("_summary"),
		MCPFlatWalker::BuildSummary(Object->GetClass(), Ctx, Options, /*bIsObjectDescent*/ false, /*bDescendedIntoObject*/ false));
	return Out;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPPropertyUtils::GetPropertyMetadataTree(
	UObject* Object, const FString& PropertyPath, const FPropertyWalkOptions& Options)
{
	TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
	if (!Object) { return Out; }

	UStruct* StartStruct = Object->GetClass();
	const void* StartAddr = Object;
	FString StartPath;
	bool bLandedOnObjectRef = false;
	bool bDescendedIntoObject = false;

	if (!PropertyPath.IsEmpty())
	{
		TArray<FString> Segments;
		PropertyPath.ParseIntoArray(Segments, TEXT("."));
		UStruct* CurrentStruct = Object->GetClass();
		const void* CurrentAddr = Object;

		for (int32 i = 0; i < Segments.Num(); ++i)
		{
			FString Segment = Segments[i];
			int32 ArrayIdx = INDEX_NONE;
			int32 BracketPos = INDEX_NONE;
			if (Segment.FindChar(TEXT('['), BracketPos))
			{
				int32 CloseBracket = INDEX_NONE;
				if (Segment.FindChar(TEXT(']'), CloseBracket) && CloseBracket > BracketPos + 1)
				{
					ArrayIdx = FCString::Atoi(*Segment.Mid(BracketPos + 1, CloseBracket - BracketPos - 1));
					Segment = Segment.Left(BracketPos);
				}
			}

			FProperty* Prop = nullptr;
			for (UStruct* S = CurrentStruct; S && !Prop; S = S->GetSuperStruct())
			{
				Prop = S->FindPropertyByName(*Segment);
			}
			if (!Prop)
			{
				Out->SetStringField(TEXT("error"),
					FString::Printf(TEXT("Property '%s' not found at path segment %d"), *Segment, i));
				return Out;
			}

			const void* PropAddr = Prop->ContainerPtrToValuePtr<void>(CurrentAddr);
			FProperty* EffectiveProp = Prop;
			const void* EffectiveAddr = PropAddr;

			if (ArrayIdx != INDEX_NONE)
			{
				if (FArrayProperty* AP = CastField<FArrayProperty>(Prop))
				{
					FScriptArrayHelper Helper(AP, PropAddr);
					if (!Helper.IsValidIndex(ArrayIdx))
					{
						Out->SetStringField(TEXT("error"),
							FString::Printf(TEXT("Array index %d out of bounds for '%s' (size %d)"), ArrayIdx, *Segment, Helper.Num()));
						return Out;
					}
					EffectiveProp = AP->Inner;
					EffectiveAddr = Helper.GetRawPtr(ArrayIdx);
				}
				else if (Prop->ArrayDim > 1)
				{
					if (ArrayIdx < 0 || ArrayIdx >= Prop->ArrayDim)
					{
						Out->SetStringField(TEXT("error"),
							FString::Printf(TEXT("Static array index %d out of bounds for '%s' (size %d)"), ArrayIdx, *Segment, Prop->ArrayDim));
						return Out;
					}
					EffectiveAddr = Prop->ContainerPtrToValuePtr<void>(CurrentAddr, ArrayIdx);
				}
				else
				{
					Out->SetStringField(TEXT("error"),
						FString::Printf(TEXT("Property '%s' is not an array but [%d] was given"), *Segment, ArrayIdx));
					return Out;
				}
			}

			const bool bIsLeaf = (i == Segments.Num() - 1);
			if (bIsLeaf)
			{
				// Struct: always auto-descend (small embedded data, no class boundary).
				if (FStructProperty* SP = CastField<FStructProperty>(EffectiveProp); SP && SP->Struct)
				{
					StartStruct = SP->Struct;
					StartAddr = EffectiveAddr;
					StartPath = PropertyPath;
					break;
				}
				// Object: only descend if explicitly requested. Default behavior
				// is to return single-property metadata for the reference + a hint.
				if (FObjectProperty* OP = CastField<FObjectProperty>(EffectiveProp))
				{
					bLandedOnObjectRef = true;
					UObject* Sub = OP->GetObjectPropertyValue(EffectiveAddr);
					if (Options.bDescendIntoObjects && Sub)
					{
						bDescendedIntoObject = true;
						StartStruct = Sub->GetClass();
						StartAddr = Sub;
						StartPath = PropertyPath;
						break;
					}
					// No descent — emit single metadata for the ref + hint.
					TSharedPtr<FJsonObject> RefMeta = GetPropertyMetadata(EffectiveProp, PropertyPath, EffectiveAddr, true, Options.bExpandEnum);
					RefMeta->SetBoolField(TEXT("is_null"), Sub == nullptr);
					RefMeta->SetStringField(TEXT("_hint"),
						TEXT("Object reference. To inspect this object's own properties, "
						     "either use component_name='<comp>' on the actor, or pass descend_into_objects=true."));
					Out->SetObjectField(PropertyPath, RefMeta);

					// Build a tiny summary so the response shape is consistent.
					MCPFlatWalker::FSummaryCtx Ctx;
					Ctx.TotalAvailable = 1;
					Ctx.TotalReturned = 1;
					Ctx.OwnCount = 1;
					Out->SetObjectField(TEXT("_summary"),
						MCPFlatWalker::BuildSummary(nullptr, Ctx, Options, /*bIsObjectDescent*/ true, /*bDescendedIntoObject*/ false));
					return Out;
				}

				// Plain leaf — single metadata, no recursion.
				Out->SetObjectField(PropertyPath,
					GetPropertyMetadata(EffectiveProp, PropertyPath, EffectiveAddr, true, Options.bExpandEnum));
				MCPFlatWalker::FSummaryCtx Ctx;
				Ctx.TotalAvailable = 1;
				Ctx.TotalReturned = 1;
				Ctx.OwnCount = 1;
				Out->SetObjectField(TEXT("_summary"),
					MCPFlatWalker::BuildSummary(nullptr, Ctx, Options, /*bIsObjectDescent*/ false, /*bDescendedIntoObject*/ false));
				return Out;
			}

			// Non-leaf segment: descend into struct or follow object.
			if (FStructProperty* SP = CastField<FStructProperty>(EffectiveProp); SP && SP->Struct)
			{
				CurrentStruct = SP->Struct;
				CurrentAddr = EffectiveAddr;
				continue;
			}
			if (FObjectProperty* OP = CastField<FObjectProperty>(EffectiveProp))
			{
				UObject* Sub = OP->GetObjectPropertyValue(EffectiveAddr);
				if (!Sub)
				{
					Out->SetStringField(TEXT("error"),
						FString::Printf(TEXT("Object property '%s' is null"), *Segment));
					return Out;
				}
				CurrentStruct = Sub->GetClass();
				CurrentAddr = Sub;
				continue;
			}

			Out->SetStringField(TEXT("error"),
				FString::Printf(TEXT("Cannot descend through '%s' (type %s)"), *Segment, *EffectiveProp->GetClass()->GetName()));
			return Out;
		}
	}

	// Walk start point with full options support (filter, category, depth, cap, cursor).
	MCPFlatWalker::FSummaryCtx Ctx;
	Ctx.MostDerivedClass = Cast<UClass>(StartStruct);

	FPropertyWalkOptions WalkOpts = Options;
	WalkOpts.bIncludeMetadata = true;  // metadata mode for this entry point

	// Build a path-prefix-aware walker (so recursion keeps StartPath as prefix).
	TFunction<void(UStruct*, const void*, const FString&, int32)> Walk;
	Walk = [&](UStruct* S, const void* A, const FString& Prefix, int32 Remaining)
	{
		if (!S || !A) { return; }

		const EFieldIteratorFlags::SuperClassFlags SuperFlag =
			Options.bIncludeInherited ? EFieldIteratorFlags::IncludeSuper : EFieldIteratorFlags::ExcludeSuper;

		for (TFieldIterator<FProperty> It(S, SuperFlag); It; ++It)
		{
			FProperty* P = *It;
			if (!P) { continue; }
			const FString FullPath = Prefix.IsEmpty() ? P->GetName() : (Prefix + TEXT(".") + P->GetName());
			const void* PropAddr = P->ContainerPtrToValuePtr<void>(A);

			const bool bIsStruct = P->IsA<FStructProperty>();
			const bool bWillRecurseStruct = (Remaining > 0) && bIsStruct;

			if (!bWillRecurseStruct)
			{
				MCPFlatWalker::EmitOrSkip(P, PropAddr, FullPath, WalkOpts, Ctx, Out);
				continue;
			}

			FStructProperty* SP = CastField<FStructProperty>(P);
			if (SP && SP->Struct)
			{
				Walk(SP->Struct, PropAddr, FullPath, Remaining - 1);
			}
		}
	};
	Walk(StartStruct, StartAddr, StartPath, Options.MaxDepth);

	Out->SetObjectField(TEXT("_summary"),
		MCPFlatWalker::BuildSummary(StartStruct, Ctx, Options, bLandedOnObjectRef, bDescendedIntoObject));
	return Out;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPPropertyUtils::SerializeAllProperties(
	UObject* Object, const FString& FilterLower, bool bIncludeInherited)
{
	TSharedPtr<FJsonObject> Props = MakeShared<FJsonObject>();

	const EFieldIteratorFlags::SuperClassFlags SuperFlag =
		bIncludeInherited ? EFieldIteratorFlags::IncludeSuper : EFieldIteratorFlags::ExcludeSuper;

	for (TFieldIterator<FProperty> It(Object->GetClass(), SuperFlag); It; ++It)
	{
		FProperty* Prop = *It;
		const FString Name = Prop->GetName();

		if (MCPPropertySafety::ShouldSkipPropertyType(Prop))
		{
			continue;
		}

		if (!FilterLower.IsEmpty() && !Name.ToLower().Contains(FilterLower))
		{
			continue;
		}

		const void* Addr = Prop->ContainerPtrToValuePtr<void>(Object);
		TSharedPtr<FJsonValue> Val = SafePropertyToJsonValue(Prop, Addr);

		if (Val.IsValid())
		{
			Props->SetField(Name, Val);
		}
	}

	return Props;
}
