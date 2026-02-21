#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "GameFramework/Actor.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_InputAction.h"
#include "K2Node_Self.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

TSharedPtr<FJsonObject> FEpicUnrealMCPCommonUtils::CreateErrorResponse(const FString& Message)
{
	TSharedPtr<FJsonObject> ResponseObject = MakeShared<FJsonObject>();
	ResponseObject->SetBoolField(TEXT("success"), false);
	ResponseObject->SetStringField(TEXT("error"), Message);
	return ResponseObject;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPCommonUtils::CreateSuccessResponse(const TSharedPtr<FJsonObject>& Data)
{
	TSharedPtr<FJsonObject> ResponseObject = MakeShared<FJsonObject>();
	ResponseObject->SetBoolField(TEXT("success"), true);

	if (Data.IsValid())
	{
		ResponseObject->SetObjectField(TEXT("data"), Data);
	}

	return ResponseObject;
}

void FEpicUnrealMCPCommonUtils::GetIntArrayFromJson(
	const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, TArray<int32>& OutArray)
{
	OutArray.Reset();

	if (!JsonObject->HasField(FieldName))
	{
		return;
	}

	const TArray<TSharedPtr<FJsonValue>>* JsonArray;
	if (JsonObject->TryGetArrayField(FieldName, JsonArray))
	{
		for (const TSharedPtr<FJsonValue>& Value : *JsonArray)
		{
			OutArray.Add((int32)Value->AsNumber());
		}
	}
}

void FEpicUnrealMCPCommonUtils::GetFloatArrayFromJson(
	const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, TArray<float>& OutArray)
{
	OutArray.Reset();

	if (!JsonObject->HasField(FieldName))
	{
		return;
	}

	const TArray<TSharedPtr<FJsonValue>>* JsonArray;
	if (JsonObject->TryGetArrayField(FieldName, JsonArray))
	{
		for (const TSharedPtr<FJsonValue>& Value : *JsonArray)
		{
			OutArray.Add((float)Value->AsNumber());
		}
	}
}

FVector2D FEpicUnrealMCPCommonUtils::GetVector2DFromJson(
	const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName)
{
	FVector2D Result(0.0f, 0.0f);

	if (!JsonObject->HasField(FieldName))
	{
		return Result;
	}

	const TArray<TSharedPtr<FJsonValue>>* JsonArray;
	if (JsonObject->TryGetArrayField(FieldName, JsonArray) && JsonArray->Num() >= 2)
	{
		Result.X = (float)(*JsonArray)[0]->AsNumber();
		Result.Y = (float)(*JsonArray)[1]->AsNumber();
	}

	return Result;
}

FVector FEpicUnrealMCPCommonUtils::GetVectorFromJson(
	const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName)
{
	FVector Result(0.0f, 0.0f, 0.0f);

	if (!JsonObject->HasField(FieldName))
	{
		return Result;
	}

	const TArray<TSharedPtr<FJsonValue>>* JsonArray;
	if (JsonObject->TryGetArrayField(FieldName, JsonArray) && JsonArray->Num() >= 3)
	{
		Result.X = (float)(*JsonArray)[0]->AsNumber();
		Result.Y = (float)(*JsonArray)[1]->AsNumber();
		Result.Z = (float)(*JsonArray)[2]->AsNumber();
	}

	return Result;
}

FRotator FEpicUnrealMCPCommonUtils::GetRotatorFromJson(
	const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName)
{
	FRotator Result(0.0f, 0.0f, 0.0f);

	if (!JsonObject->HasField(FieldName))
	{
		return Result;
	}

	const TArray<TSharedPtr<FJsonValue>>* JsonArray;
	if (JsonObject->TryGetArrayField(FieldName, JsonArray) && JsonArray->Num() >= 3)
	{
		Result.Pitch = (float)(*JsonArray)[0]->AsNumber();
		Result.Yaw = (float)(*JsonArray)[1]->AsNumber();
		Result.Roll = (float)(*JsonArray)[2]->AsNumber();
	}

	return Result;
}

UBlueprint* FEpicUnrealMCPCommonUtils::FindBlueprint(const FString& BlueprintName)
{
	return FindBlueprintByName(BlueprintName);
}

UBlueprint* FEpicUnrealMCPCommonUtils::FindBlueprintByName(const FString& BlueprintName)
{
	FString ObjectPath;

	if (BlueprintName.StartsWith(TEXT("/")))
	{
		FString AssetName = FPaths::GetBaseFilename(BlueprintName);
		ObjectPath = FString::Printf(TEXT("%s.%s"), *BlueprintName, *AssetName);
	}
	else
	{
		ObjectPath = FString::Printf(TEXT("/Game/Blueprints/%s.%s"), *BlueprintName, *BlueprintName);
	}

	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *ObjectPath);
	if (Blueprint)
	{
		return Blueprint;
	}

	// Asset Registry fallback — more robust for newly created assets
	FAssetRegistryModule& AssetRegistryModule =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(FSoftObjectPath(ObjectPath));

	if (AssetData.IsValid())
	{
		Blueprint = Cast<UBlueprint>(AssetData.GetAsset());
		if (Blueprint)
		{
			return Blueprint;
		}
	}

	// In-memory fallback for assets not yet fully saved
	FString PackagePath = TEXT("/Game/Blueprints/") + BlueprintName;
	Blueprint = FindObject<UBlueprint>(nullptr, *PackagePath);

	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("FindBlueprintByName: Failed to find or load blueprint: %s"), *BlueprintName);
	}

	return Blueprint;
}

UEdGraph* FEpicUnrealMCPCommonUtils::FindOrCreateEventGraph(UBlueprint* Blueprint)
{
	if (!Blueprint)
	{
		return nullptr;
	}

	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (Graph->GetName().Contains(TEXT("EventGraph")))
		{
			return Graph;
		}
	}

	UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(
		Blueprint, FName(TEXT("EventGraph")),
		UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
	FBlueprintEditorUtils::AddUbergraphPage(Blueprint, NewGraph);
	return NewGraph;
}

UK2Node_Event* FEpicUnrealMCPCommonUtils::CreateEventNode(
	UEdGraph* Graph, const FString& EventName, const FVector2D& Position)
{
	if (!Graph)
	{
		return nullptr;
	}

	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph);
	if (!Blueprint)
	{
		return nullptr;
	}

	// Reuse existing event node if one already exists
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node);
		if (EventNode && EventNode->EventReference.GetMemberName() == FName(*EventName))
		{
			UE_LOG(LogTemp, Display, TEXT("Using existing event node: %s (ID: %s)"),
				*EventName, *EventNode->NodeGuid.ToString());
			return EventNode;
		}
	}

	UK2Node_Event* EventNode = nullptr;
	UClass* BlueprintClass = Blueprint->GeneratedClass;
	UFunction* EventFunction = BlueprintClass->FindFunctionByName(FName(*EventName));

	if (EventFunction)
	{
		EventNode = NewObject<UK2Node_Event>(Graph);
		EventNode->EventReference.SetExternalMember(FName(*EventName), BlueprintClass);
		EventNode->NodePosX = Position.X;
		EventNode->NodePosY = Position.Y;
		Graph->AddNode(EventNode, true);
		EventNode->PostPlacedNewNode();
		EventNode->AllocateDefaultPins();
		UE_LOG(LogTemp, Display, TEXT("Created event node: %s (ID: %s)"),
			*EventName, *EventNode->NodeGuid.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to find function for event: %s"), *EventName);
	}

	return EventNode;
}

UK2Node_CallFunction* FEpicUnrealMCPCommonUtils::CreateFunctionCallNode(
	UEdGraph* Graph, UFunction* Function, const FVector2D& Position)
{
	if (!Graph || !Function)
	{
		return nullptr;
	}

	UK2Node_CallFunction* FunctionNode = NewObject<UK2Node_CallFunction>(Graph);
	FunctionNode->SetFromFunction(Function);
	FunctionNode->NodePosX = Position.X;
	FunctionNode->NodePosY = Position.Y;
	Graph->AddNode(FunctionNode, true);
	FunctionNode->CreateNewGuid();
	FunctionNode->PostPlacedNewNode();
	FunctionNode->AllocateDefaultPins();

	return FunctionNode;
}

UK2Node_VariableGet* FEpicUnrealMCPCommonUtils::CreateVariableGetNode(
	UEdGraph* Graph, UBlueprint* Blueprint, const FString& VariableName, const FVector2D& Position)
{
	if (!Graph || !Blueprint)
	{
		return nullptr;
	}

	UK2Node_VariableGet* VariableGetNode = NewObject<UK2Node_VariableGet>(Graph);

	FName VarName(*VariableName);
	FProperty* Property = FindFProperty<FProperty>(Blueprint->GeneratedClass, VarName);

	if (Property)
	{
		VariableGetNode->VariableReference.SetFromField<FProperty>(Property, false);
		VariableGetNode->NodePosX = Position.X;
		VariableGetNode->NodePosY = Position.Y;
		Graph->AddNode(VariableGetNode, true);
		VariableGetNode->PostPlacedNewNode();
		VariableGetNode->AllocateDefaultPins();

		return VariableGetNode;
	}

	return nullptr;
}

UK2Node_VariableSet* FEpicUnrealMCPCommonUtils::CreateVariableSetNode(
	UEdGraph* Graph, UBlueprint* Blueprint, const FString& VariableName, const FVector2D& Position)
{
	if (!Graph || !Blueprint)
	{
		return nullptr;
	}

	UK2Node_VariableSet* VariableSetNode = NewObject<UK2Node_VariableSet>(Graph);

	FName VarName(*VariableName);
	FProperty* Property = FindFProperty<FProperty>(Blueprint->GeneratedClass, VarName);

	if (Property)
	{
		VariableSetNode->VariableReference.SetFromField<FProperty>(Property, false);
		VariableSetNode->NodePosX = Position.X;
		VariableSetNode->NodePosY = Position.Y;
		Graph->AddNode(VariableSetNode, true);
		VariableSetNode->PostPlacedNewNode();
		VariableSetNode->AllocateDefaultPins();

		return VariableSetNode;
	}

	return nullptr;
}

UK2Node_InputAction* FEpicUnrealMCPCommonUtils::CreateInputActionNode(
	UEdGraph* Graph, const FString& ActionName, const FVector2D& Position)
{
	if (!Graph)
	{
		return nullptr;
	}

	UK2Node_InputAction* InputActionNode = NewObject<UK2Node_InputAction>(Graph);
	InputActionNode->InputActionName = FName(*ActionName);
	InputActionNode->NodePosX = Position.X;
	InputActionNode->NodePosY = Position.Y;
	Graph->AddNode(InputActionNode, true);
	InputActionNode->CreateNewGuid();
	InputActionNode->PostPlacedNewNode();
	InputActionNode->AllocateDefaultPins();

	return InputActionNode;
}

UK2Node_Self* FEpicUnrealMCPCommonUtils::CreateSelfReferenceNode(
	UEdGraph* Graph, const FVector2D& Position)
{
	if (!Graph)
	{
		return nullptr;
	}

	UK2Node_Self* SelfNode = NewObject<UK2Node_Self>(Graph);
	SelfNode->NodePosX = Position.X;
	SelfNode->NodePosY = Position.Y;
	Graph->AddNode(SelfNode, true);
	SelfNode->CreateNewGuid();
	SelfNode->PostPlacedNewNode();
	SelfNode->AllocateDefaultPins();

	return SelfNode;
}

bool FEpicUnrealMCPCommonUtils::ConnectGraphNodes(
	UEdGraph* Graph, UEdGraphNode* SourceNode, const FString& SourcePinName,
	UEdGraphNode* TargetNode, const FString& TargetPinName)
{
	if (!Graph || !SourceNode || !TargetNode)
	{
		return false;
	}

	UEdGraphPin* SourcePin = FindPin(SourceNode, SourcePinName, EGPD_Output);
	UEdGraphPin* TargetPin = FindPin(TargetNode, TargetPinName, EGPD_Input);

	if (SourcePin && TargetPin)
	{
		SourcePin->MakeLinkTo(TargetPin);
		return true;
	}

	return false;
}

UEdGraphPin* FEpicUnrealMCPCommonUtils::FindPin(
	UEdGraphNode* Node, const FString& PinName, EEdGraphPinDirection Direction)
{
	if (!Node)
	{
		return nullptr;
	}

	UE_LOG(LogTemp, Display, TEXT("FindPin: Looking for '%s' (Direction: %d) in node '%s'"),
		*PinName, (int32)Direction, *Node->GetName());

	// Exact match
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin->PinName.ToString() == PinName && (Direction == EGPD_MAX || Pin->Direction == Direction))
		{
			return Pin;
		}
	}

	// Case-insensitive fallback
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin->PinName.ToString().Equals(PinName, ESearchCase::IgnoreCase) &&
			(Direction == EGPD_MAX || Pin->Direction == Direction))
		{
			return Pin;
		}
	}

	// For variable getters, fall back to first data output pin
	if (Direction == EGPD_Output && Cast<UK2Node_VariableGet>(Node) != nullptr)
	{
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin->Direction == EGPD_Output && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
			{
				return Pin;
			}
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("FindPin: No match for '%s'"), *PinName);
	return nullptr;
}

TSharedPtr<FJsonValue> FEpicUnrealMCPCommonUtils::ActorToJson(AActor* Actor)
{
	if (!Actor)
	{
		return MakeShared<FJsonValueNull>();
	}

	TSharedPtr<FJsonObject> ActorObject = MakeShared<FJsonObject>();
	ActorObject->SetStringField(TEXT("name"), Actor->GetName());
	ActorObject->SetStringField(TEXT("class"), Actor->GetClass()->GetName());

	FVector Location = Actor->GetActorLocation();
	TArray<TSharedPtr<FJsonValue>> LocationArray;
	LocationArray.Add(MakeShared<FJsonValueNumber>(Location.X));
	LocationArray.Add(MakeShared<FJsonValueNumber>(Location.Y));
	LocationArray.Add(MakeShared<FJsonValueNumber>(Location.Z));
	ActorObject->SetArrayField(TEXT("location"), LocationArray);

	FRotator Rotation = Actor->GetActorRotation();
	TArray<TSharedPtr<FJsonValue>> RotationArray;
	RotationArray.Add(MakeShared<FJsonValueNumber>(Rotation.Pitch));
	RotationArray.Add(MakeShared<FJsonValueNumber>(Rotation.Yaw));
	RotationArray.Add(MakeShared<FJsonValueNumber>(Rotation.Roll));
	ActorObject->SetArrayField(TEXT("rotation"), RotationArray);

	FVector Scale = Actor->GetActorScale3D();
	TArray<TSharedPtr<FJsonValue>> ScaleArray;
	ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.X));
	ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.Y));
	ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.Z));
	ActorObject->SetArrayField(TEXT("scale"), ScaleArray);

	return MakeShared<FJsonValueObject>(ActorObject);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPCommonUtils::ActorToJsonObject(AActor* Actor, bool bDetailed)
{
	if (!Actor)
	{
		return nullptr;
	}

	TSharedPtr<FJsonObject> ActorObject = MakeShared<FJsonObject>();
	ActorObject->SetStringField(TEXT("name"), Actor->GetName());
	ActorObject->SetStringField(TEXT("class"), Actor->GetClass()->GetName());

	FVector Location = Actor->GetActorLocation();
	TArray<TSharedPtr<FJsonValue>> LocationArray;
	LocationArray.Add(MakeShared<FJsonValueNumber>(Location.X));
	LocationArray.Add(MakeShared<FJsonValueNumber>(Location.Y));
	LocationArray.Add(MakeShared<FJsonValueNumber>(Location.Z));
	ActorObject->SetArrayField(TEXT("location"), LocationArray);

	FRotator Rotation = Actor->GetActorRotation();
	TArray<TSharedPtr<FJsonValue>> RotationArray;
	RotationArray.Add(MakeShared<FJsonValueNumber>(Rotation.Pitch));
	RotationArray.Add(MakeShared<FJsonValueNumber>(Rotation.Yaw));
	RotationArray.Add(MakeShared<FJsonValueNumber>(Rotation.Roll));
	ActorObject->SetArrayField(TEXT("rotation"), RotationArray);

	FVector Scale = Actor->GetActorScale3D();
	TArray<TSharedPtr<FJsonValue>> ScaleArray;
	ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.X));
	ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.Y));
	ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.Z));
	ActorObject->SetArrayField(TEXT("scale"), ScaleArray);

	return ActorObject;
}

UK2Node_Event* FEpicUnrealMCPCommonUtils::FindExistingEventNode(UEdGraph* Graph, const FString& EventName)
{
	if (!Graph)
	{
		return nullptr;
	}

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node);
		if (EventNode && EventNode->EventReference.GetMemberName() == FName(*EventName))
		{
			UE_LOG(LogTemp, Display, TEXT("Found existing event node: %s"), *EventName);
			return EventNode;
		}
	}

	return nullptr;
}

bool FEpicUnrealMCPCommonUtils::SetObjectProperty(
	UObject* Object, const FString& PropertyName,
	const TSharedPtr<FJsonValue>& Value, FString& OutErrorMessage)
{
	if (!Object)
	{
		OutErrorMessage = TEXT("Invalid object");
		return false;
	}

	FProperty* Property = Object->GetClass()->FindPropertyByName(*PropertyName);
	if (!Property)
	{
		OutErrorMessage = FString::Printf(TEXT("Property not found: %s"), *PropertyName);
		return false;
	}

	void* PropertyAddr = Property->ContainerPtrToValuePtr<void>(Object);

	if (Property->IsA<FBoolProperty>())
	{
		((FBoolProperty*)Property)->SetPropertyValue(PropertyAddr, Value->AsBool());
		return true;
	}
	else if (Property->IsA<FIntProperty>())
	{
		FIntProperty* IntProperty = CastField<FIntProperty>(Property);
		if (IntProperty)
		{
			int32 IntValue = static_cast<int32>(Value->AsNumber());
			IntProperty->SetPropertyValue_InContainer(Object, IntValue);
			return true;
		}
	}
	else if (Property->IsA<FFloatProperty>())
	{
		((FFloatProperty*)Property)->SetPropertyValue(PropertyAddr, Value->AsNumber());
		return true;
	}
	else if (Property->IsA<FNameProperty>())
	{
		((FNameProperty*)Property)->SetPropertyValue(PropertyAddr, FName(*Value->AsString()));
		return true;
	}
	else if (Property->IsA<FStrProperty>())
	{
		((FStrProperty*)Property)->SetPropertyValue(PropertyAddr, Value->AsString());
		return true;
	}
	else if (Property->IsA<FByteProperty>())
	{
		FByteProperty* ByteProp = CastField<FByteProperty>(Property);
		UEnum* EnumDef = ByteProp ? ByteProp->GetIntPropertyEnum() : nullptr;

		if (EnumDef)
		{
			if (Value->Type == EJson::Number)
			{
				uint8 ByteValue = static_cast<uint8>(Value->AsNumber());
				ByteProp->SetPropertyValue(PropertyAddr, ByteValue);
				return true;
			}
			else if (Value->Type == EJson::String)
			{
				FString EnumValueName = Value->AsString();

				if (EnumValueName.IsNumeric())
				{
					uint8 ByteValue = FCString::Atoi(*EnumValueName);
					ByteProp->SetPropertyValue(PropertyAddr, ByteValue);
					return true;
				}

				// Strip qualified prefix (e.g. "EAutoReceiveInput::Player0" -> "Player0")
				if (EnumValueName.Contains(TEXT("::")))
				{
					EnumValueName.Split(TEXT("::"), nullptr, &EnumValueName);
				}

				int64 EnumValue = EnumDef->GetValueByNameString(EnumValueName);
				if (EnumValue == INDEX_NONE)
				{
					EnumValue = EnumDef->GetValueByNameString(Value->AsString());
				}

				if (EnumValue != INDEX_NONE)
				{
					ByteProp->SetPropertyValue(PropertyAddr, static_cast<uint8>(EnumValue));
					return true;
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("Could not find enum value for '%s'. Available options:"),
						*EnumValueName);
					for (int32 i = 0; i < EnumDef->NumEnums(); i++)
					{
						UE_LOG(LogTemp, Warning, TEXT("  - %s (value: %d)"),
							*EnumDef->GetNameStringByIndex(i), EnumDef->GetValueByIndex(i));
					}

					OutErrorMessage = FString::Printf(
						TEXT("Could not find enum value for '%s'"), *EnumValueName);
					return false;
				}
			}
		}
		else
		{
			uint8 ByteValue = static_cast<uint8>(Value->AsNumber());
			ByteProp->SetPropertyValue(PropertyAddr, ByteValue);
			return true;
		}
	}
	else if (Property->IsA<FEnumProperty>())
	{
		FEnumProperty* EnumProp = CastField<FEnumProperty>(Property);
		UEnum* EnumDef = EnumProp ? EnumProp->GetEnum() : nullptr;
		FNumericProperty* UnderlyingNumericProp = EnumProp ? EnumProp->GetUnderlyingProperty() : nullptr;

		if (EnumDef && UnderlyingNumericProp)
		{
			if (Value->Type == EJson::Number)
			{
				int64 EnumValue = static_cast<int64>(Value->AsNumber());
				UnderlyingNumericProp->SetIntPropertyValue(PropertyAddr, EnumValue);
				return true;
			}
			else if (Value->Type == EJson::String)
			{
				FString EnumValueName = Value->AsString();

				if (EnumValueName.IsNumeric())
				{
					int64 EnumValue = FCString::Atoi64(*EnumValueName);
					UnderlyingNumericProp->SetIntPropertyValue(PropertyAddr, EnumValue);
					return true;
				}

				if (EnumValueName.Contains(TEXT("::")))
				{
					EnumValueName.Split(TEXT("::"), nullptr, &EnumValueName);
				}

				int64 EnumValue = EnumDef->GetValueByNameString(EnumValueName);
				if (EnumValue == INDEX_NONE)
				{
					EnumValue = EnumDef->GetValueByNameString(Value->AsString());
				}

				if (EnumValue != INDEX_NONE)
				{
					UnderlyingNumericProp->SetIntPropertyValue(PropertyAddr, EnumValue);
					return true;
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("Could not find enum value for '%s'. Available options:"),
						*EnumValueName);
					for (int32 i = 0; i < EnumDef->NumEnums(); i++)
					{
						UE_LOG(LogTemp, Warning, TEXT("  - %s (value: %d)"),
							*EnumDef->GetNameStringByIndex(i), EnumDef->GetValueByIndex(i));
					}

					OutErrorMessage = FString::Printf(
						TEXT("Could not find enum value for '%s'"), *EnumValueName);
					return false;
				}
			}
		}
	}

	OutErrorMessage = FString::Printf(TEXT("Unsupported property type: %s for property %s"),
		*Property->GetClass()->GetName(), *PropertyName);
	return false;
}
