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
			OutErrors += FString::Printf(TEXT("[field '%s' not found] "), *Pair.Key);
			continue;
		}

		void* FieldAddr = FieldProp->ContainerPtrToValuePtr<void>(StructData);

		// Instanced object array (e.g. TArray<UMassEntityTraitBase*> Instanced)
		if (FArrayProperty* ArrProp = CastField<FArrayProperty>(FieldProp))
		{
			FObjectProperty* InnerObj = CastField<FObjectProperty>(ArrProp->Inner);
			if (InnerObj
				&& InnerObj->HasAnyPropertyFlags(CPF_PersistentInstance | CPF_InstancedReference)
				&& Pair.Value->Type == EJson::Array)
			{
				const TArray<TSharedPtr<FJsonValue>>& JsonArr = Pair.Value->AsArray();
				FScriptArrayHelper ArrHelper(ArrProp, FieldAddr);
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
							UClass* R = FindObject<UClass>(nullptr, *ClassPath);
							if (!R)
							{
								R = LoadObject<UClass>(nullptr, *ClassPath);
							}
							if (R)
							{
								ElemClass = R;
							}
						}

						UObject* SubObj = NewObject<UObject>(Outer, ElemClass);
						FString SubErrors;
						SetPropertiesFromJson(SubObj, ElemJson, SubErrors);
						InnerObj->SetObjectPropertyValue(ElemAddr, SubObj);
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
				continue;
			}

			// FInstancedStruct array
			FStructProperty* InnerSP = CastField<FStructProperty>(ArrProp->Inner);
			if (InnerSP && InnerSP->Struct
				&& InnerSP->Struct->GetName() == TEXT("InstancedStruct")
				&& Pair.Value->Type == EJson::Array)
			{
				const TArray<TSharedPtr<FJsonValue>>& JsonArr = Pair.Value->AsArray();
				FScriptArrayHelper ArrHelper(ArrProp, FieldAddr);
				ArrHelper.EmptyValues();
				ArrHelper.AddValues(JsonArr.Num());

				for (int32 i = 0; i < JsonArr.Num(); ++i)
				{
					const TSharedPtr<FJsonValue>& Elem = JsonArr[i];
					void* ElemAddr = ArrHelper.GetRawPtr(i);
					FInstancedStruct* InstStruct = reinterpret_cast<FInstancedStruct*>(ElemAddr);

					FString StructPath;
					if (Elem->Type == EJson::Object)
					{
						Elem->AsObject()->TryGetStringField(TEXT("_ClassName"), StructPath);
					}
					else if (Elem->Type == EJson::String)
					{
						StructPath = Elem->AsString();
					}

					if (!StructPath.IsEmpty() && StructPath != TEXT("None"))
					{
						UScriptStruct* TargetStruct = FindObject<UScriptStruct>(nullptr, *StructPath);
						if (!TargetStruct)
						{
							TargetStruct = LoadObject<UScriptStruct>(nullptr, *StructPath);
						}
						if (!TargetStruct)
						{
							for (TObjectIterator<UScriptStruct> It; It; ++It)
							{
								const FString N = (*It)->GetName();
								if (N == StructPath || N == (TEXT("F") + StructPath) ||
									(StructPath.StartsWith(TEXT("F")) && N == StructPath.RightChop(1)) ||
									(*It)->GetPathName() == StructPath)
								{
									TargetStruct = *It;
									break;
								}
							}
						}
						if (TargetStruct)
						{
							InstStruct->InitializeAs(TargetStruct);
						}
					}
				}
				continue;
			}
		}

		FJsonObjectConverter::JsonValueToUProperty(Pair.Value, FieldProp, FieldAddr);
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
				UE_LOG(LogTemp, Warning, TEXT("SetProperty '%s' subobject warnings: %s"),
					*PropertyName, *SubErrors);
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
				UE_LOG(LogTemp, Warning, TEXT("SetProperty '%s' struct warnings: %s"),
					*PropertyName, *SubErrors);
			}
			return true;
		}
		OutError = FString::Printf(TEXT("Expected JSON object for struct property '%s'"), *PropertyName);
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
						UE_LOG(LogTemp, Warning, TEXT("SetProperty '%s'[%d] warnings: %s"),
							*PropertyName, i, *SubErrors);
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
