#include "Commands/EpicUnrealMCPEnhancedInputCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "Commands/EpicUnrealMCPPropertyUtils.h"

#include "InputAction.h"
#include "InputMappingContext.h"
#include "EnhancedActionKeyMapping.h"
#include "InputTriggers.h"
#include "InputModifiers.h"

#include "Engine/DataAsset.h"
#include "Factories/DataAssetFactory.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "UObject/UObjectIterator.h"
#include "GameFramework/PlayerInput.h"

using PU = FEpicUnrealMCPPropertyUtils;

// ---------------------------------------------------------------------------
// Command dispatch
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPEnhancedInputCommands::HandleCommand(
	const FString& CommandType,
	const TSharedPtr<FJsonObject>& Params)
{
	// Input Action
	if (CommandType == TEXT("create_input_action"))
	{
		return HandleCreateInputAction(Params);
	}
	if (CommandType == TEXT("get_input_action"))
	{
		return HandleGetInputAction(Params);
	}
	if (CommandType == TEXT("set_input_action_properties"))
	{
		return HandleSetInputActionProperties(Params);
	}
	if (CommandType == TEXT("add_input_action_trigger"))
	{
		return HandleAddInputActionTrigger(Params);
	}
	if (CommandType == TEXT("add_input_action_modifier"))
	{
		return HandleAddInputActionModifier(Params);
	}
	if (CommandType == TEXT("remove_input_action_trigger"))
	{
		return HandleRemoveInputActionTrigger(Params);
	}
	if (CommandType == TEXT("remove_input_action_modifier"))
	{
		return HandleRemoveInputActionModifier(Params);
	}
	if (CommandType == TEXT("list_input_actions"))
	{
		return HandleListInputActions(Params);
	}

	// Input Mapping Context
	if (CommandType == TEXT("create_input_mapping_context"))
	{
		return HandleCreateInputMappingContext(Params);
	}
	if (CommandType == TEXT("get_input_mapping_context"))
	{
		return HandleGetInputMappingContext(Params);
	}
	if (CommandType == TEXT("add_key_mapping"))
	{
		return HandleAddKeyMapping(Params);
	}
	if (CommandType == TEXT("remove_key_mapping"))
	{
		return HandleRemoveKeyMapping(Params);
	}
	if (CommandType == TEXT("set_key_mapping"))
	{
		return HandleSetKeyMapping(Params);
	}
	if (CommandType == TEXT("add_mapping_trigger"))
	{
		return HandleAddMappingTrigger(Params);
	}
	if (CommandType == TEXT("add_mapping_modifier"))
	{
		return HandleAddMappingModifier(Params);
	}
	if (CommandType == TEXT("remove_mapping_trigger"))
	{
		return HandleRemoveMappingTrigger(Params);
	}
	if (CommandType == TEXT("remove_mapping_modifier"))
	{
		return HandleRemoveMappingModifier(Params);
	}
	if (CommandType == TEXT("list_input_mapping_contexts"))
	{
		return HandleListInputMappingContexts(Params);
	}

	// Utility
	if (CommandType == TEXT("list_trigger_types"))
	{
		return HandleListTriggerTypes(Params);
	}
	if (CommandType == TEXT("list_modifier_types"))
	{
		return HandleListModifierTypes(Params);
	}
	if (CommandType == TEXT("list_input_keys"))
	{
		return HandleListInputKeys(Params);
	}

	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
		FString::Printf(TEXT("Unknown enhanced input command: %s"), *CommandType));
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

UInputAction* FEpicUnrealMCPEnhancedInputCommands::LoadInputAction(const FString& AssetPath)
{
	UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
	if (!Asset)
	{
		return nullptr;
	}
	return Cast<UInputAction>(Asset);
}

UInputMappingContext* FEpicUnrealMCPEnhancedInputCommands::LoadInputMappingContext(const FString& AssetPath)
{
	UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
	if (!Asset)
	{
		return nullptr;
	}
	return Cast<UInputMappingContext>(Asset);
}

UClass* FEpicUnrealMCPEnhancedInputCommands::ResolveTriggerClass(const FString& TypeName)
{
	TArray<UClass*> Derived;
	GetDerivedClasses(UInputTrigger::StaticClass(), Derived, true);

	for (UClass* C : Derived)
	{
		if (!C || C->HasAnyClassFlags(CLASS_Abstract))
		{
			continue;
		}
		const FString Name = C->GetName();
		if (Name == TypeName ||
			Name == (TEXT("U") + TypeName) ||
			(TypeName.StartsWith(TEXT("U")) && Name == TypeName.RightChop(1)))
		{
			return C;
		}
	}
	return nullptr;
}

UClass* FEpicUnrealMCPEnhancedInputCommands::ResolveModifierClass(const FString& TypeName)
{
	TArray<UClass*> Derived;
	GetDerivedClasses(UInputModifier::StaticClass(), Derived, true);

	for (UClass* C : Derived)
	{
		if (!C || C->HasAnyClassFlags(CLASS_Abstract))
		{
			continue;
		}
		const FString Name = C->GetName();
		if (Name == TypeName ||
			Name == (TEXT("U") + TypeName) ||
			(TypeName.StartsWith(TEXT("U")) && Name == TypeName.RightChop(1)))
		{
			return C;
		}
	}
	return nullptr;
}

UInputTrigger* FEpicUnrealMCPEnhancedInputCommands::CreateTriggerInstance(
	UObject* Outer, const FString& TriggerType,
	const TSharedPtr<FJsonObject>& Properties)
{
	UClass* TriggerClass = ResolveTriggerClass(TriggerType);
	if (!TriggerClass)
	{
		return nullptr;
	}

	UInputTrigger* Trigger = NewObject<UInputTrigger>(Outer, TriggerClass);
	if (Trigger && Properties.IsValid() && Properties->Values.Num() > 0)
	{
		FString Errors;
		PU::SetPropertiesFromJson(Trigger, Properties, Errors);
	}
	return Trigger;
}

UInputModifier* FEpicUnrealMCPEnhancedInputCommands::CreateModifierInstance(
	UObject* Outer, const FString& ModifierType,
	const TSharedPtr<FJsonObject>& Properties)
{
	UClass* ModifierClass = ResolveModifierClass(ModifierType);
	if (!ModifierClass)
	{
		return nullptr;
	}

	UInputModifier* Modifier = NewObject<UInputModifier>(Outer, ModifierClass);
	if (Modifier && Properties.IsValid() && Properties->Values.Num() > 0)
	{
		FString Errors;
		PU::SetPropertiesFromJson(Modifier, Properties, Errors);
	}
	return Modifier;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEnhancedInputCommands::SerializeTrigger(
	const UInputTrigger* Trigger, int32 Index)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetNumberField(TEXT("index"), Index);

	if (!Trigger)
	{
		Obj->SetStringField(TEXT("type"), TEXT("null"));
		return Obj;
	}

	Obj->SetStringField(TEXT("type"), Trigger->GetClass()->GetName());
	Obj->SetNumberField(TEXT("actuation_threshold"), Trigger->ActuationThreshold);
	Obj->SetBoolField(TEXT("should_always_tick"), Trigger->bShouldAlwaysTick);

	// Serialize all editable properties
	Obj->SetObjectField(TEXT("properties"),
		PU::SerializeAllProperties(const_cast<UInputTrigger*>(Trigger), TEXT(""), true));

	return Obj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEnhancedInputCommands::SerializeModifier(
	const UInputModifier* Modifier, int32 Index)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetNumberField(TEXT("index"), Index);

	if (!Modifier)
	{
		Obj->SetStringField(TEXT("type"), TEXT("null"));
		return Obj;
	}

	Obj->SetStringField(TEXT("type"), Modifier->GetClass()->GetName());

	// Serialize all editable properties
	Obj->SetObjectField(TEXT("properties"),
		PU::SerializeAllProperties(const_cast<UInputModifier*>(Modifier), TEXT(""), true));

	return Obj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEnhancedInputCommands::SerializeMapping(
	const FEnhancedActionKeyMapping& Mapping, int32 Index)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetNumberField(TEXT("index"), Index);
	Obj->SetStringField(TEXT("key"), Mapping.Key.GetFName().ToString());
	Obj->SetStringField(TEXT("action"),
		Mapping.Action ? Mapping.Action->GetPathName() : TEXT("None"));
	Obj->SetStringField(TEXT("action_name"),
		Mapping.Action ? Mapping.Action->GetName() : TEXT("None"));

	// Triggers
	TArray<TSharedPtr<FJsonValue>> TriggerArr;
	for (int32 i = 0; i < Mapping.Triggers.Num(); ++i)
	{
		TriggerArr.Add(MakeShared<FJsonValueObject>(
			SerializeTrigger(Mapping.Triggers[i], i)));
	}
	Obj->SetArrayField(TEXT("triggers"), TriggerArr);

	// Modifiers
	TArray<TSharedPtr<FJsonValue>> ModifierArr;
	for (int32 i = 0; i < Mapping.Modifiers.Num(); ++i)
	{
		ModifierArr.Add(MakeShared<FJsonValueObject>(
			SerializeModifier(Mapping.Modifiers[i], i)));
	}
	Obj->SetArrayField(TEXT("modifiers"), ModifierArr);

	return Obj;
}

// ---------------------------------------------------------------------------
// Input Action Commands
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPEnhancedInputCommands::HandleCreateInputAction(
	const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path'"));
	}

	FString PackagePath;
	FString AssetName;
	int32 LastSlash = INDEX_NONE;
	if (AssetPath.FindLastChar('/', LastSlash) && LastSlash != INDEX_NONE)
	{
		PackagePath = AssetPath.Left(LastSlash);
		AssetName = AssetPath.Mid(LastSlash + 1);
	}
	else
	{
		AssetName = AssetPath;
		PackagePath = TEXT("/Game");
	}

	const FString FullPath = PackagePath / AssetName;
	if (UEditorAssetLibrary::DoesAssetExist(FullPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Asset already exists: %s"), *FullPath));
	}

	UDataAssetFactory* Factory = NewObject<UDataAssetFactory>();
	Factory->DataAssetClass = UInputAction::StaticClass();

	IAssetTools& AssetTools =
		FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();

	UObject* NewAsset = AssetTools.CreateAsset(
		AssetName, PackagePath, UInputAction::StaticClass(), Factory);
	if (!NewAsset)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Failed to create input action: %s"), *FullPath));
	}

	UInputAction* Action = Cast<UInputAction>(NewAsset);

	// Apply optional value_type
	FString ValueTypeStr;
	if (Params->TryGetStringField(TEXT("value_type"), ValueTypeStr))
	{
		if (ValueTypeStr == TEXT("Boolean"))
		{
			Action->ValueType = EInputActionValueType::Boolean;
		}
		else if (ValueTypeStr == TEXT("Axis1D") || ValueTypeStr == TEXT("Float"))
		{
			Action->ValueType = EInputActionValueType::Axis1D;
		}
		else if (ValueTypeStr == TEXT("Axis2D") || ValueTypeStr == TEXT("Vector2D"))
		{
			Action->ValueType = EInputActionValueType::Axis2D;
		}
		else if (ValueTypeStr == TEXT("Axis3D") || ValueTypeStr == TEXT("Vector"))
		{
			Action->ValueType = EInputActionValueType::Axis3D;
		}
	}

	// Apply optional properties
	const TSharedPtr<FJsonObject>* PropsObj = nullptr;
	if (Params->TryGetObjectField(TEXT("properties"), PropsObj) && PropsObj)
	{
		FString Errors;
		PU::SetPropertiesFromJson(Action, *PropsObj, Errors);
	}

	Action->MarkPackageDirty();
	UEditorAssetLibrary::SaveAsset(FullPath);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("path"), FullPath);
	Result->SetStringField(TEXT("class"), TEXT("InputAction"));
	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEnhancedInputCommands::HandleGetInputAction(
	const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path'"));
	}

	UInputAction* Action = LoadInputAction(AssetPath);
	if (!Action)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Input action not found: %s"), *AssetPath));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("asset_path"), AssetPath);
	Result->SetStringField(TEXT("name"), Action->GetName());

	// Basic properties
	FString ValueTypeStr;
	switch (Action->ValueType)
	{
	case EInputActionValueType::Boolean:  ValueTypeStr = TEXT("Boolean"); break;
	case EInputActionValueType::Axis1D:   ValueTypeStr = TEXT("Axis1D"); break;
	case EInputActionValueType::Axis2D:   ValueTypeStr = TEXT("Axis2D"); break;
	case EInputActionValueType::Axis3D:   ValueTypeStr = TEXT("Axis3D"); break;
	}
	Result->SetStringField(TEXT("value_type"), ValueTypeStr);
	Result->SetBoolField(TEXT("trigger_when_paused"), Action->bTriggerWhenPaused);
	Result->SetBoolField(TEXT("consume_input"), Action->bConsumeInput);
	Result->SetBoolField(TEXT("consumes_action_and_axis_mappings"), Action->bConsumesActionAndAxisMappings);
	Result->SetBoolField(TEXT("reserve_all_mappings"), Action->bReserveAllMappings);
	Result->SetStringField(TEXT("description"), Action->ActionDescription.ToString());

	FString AccumStr;
	switch (Action->AccumulationBehavior)
	{
	case EInputActionAccumulationBehavior::TakeHighestAbsoluteValue:
		AccumStr = TEXT("TakeHighestAbsoluteValue"); break;
	case EInputActionAccumulationBehavior::Cumulative:
		AccumStr = TEXT("Cumulative"); break;
	}
	Result->SetStringField(TEXT("accumulation_behavior"), AccumStr);

	// Triggers
	TArray<TSharedPtr<FJsonValue>> TriggerArr;
	for (int32 i = 0; i < Action->Triggers.Num(); ++i)
	{
		TriggerArr.Add(MakeShared<FJsonValueObject>(
			SerializeTrigger(Action->Triggers[i], i)));
	}
	Result->SetArrayField(TEXT("triggers"), TriggerArr);

	// Modifiers
	TArray<TSharedPtr<FJsonValue>> ModifierArr;
	for (int32 i = 0; i < Action->Modifiers.Num(); ++i)
	{
		ModifierArr.Add(MakeShared<FJsonValueObject>(
			SerializeModifier(Action->Modifiers[i], i)));
	}
	Result->SetArrayField(TEXT("modifiers"), ModifierArr);

	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEnhancedInputCommands::HandleSetInputActionProperties(
	const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path'"));
	}

	UInputAction* Action = LoadInputAction(AssetPath);
	if (!Action)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Input action not found: %s"), *AssetPath));
	}

	TArray<FString> Set;
	TArray<FString> Failed;

	// Handle value_type specially for convenience
	FString ValueTypeStr;
	if (Params->TryGetStringField(TEXT("value_type"), ValueTypeStr))
	{
		if (ValueTypeStr == TEXT("Boolean"))
		{
			Action->ValueType = EInputActionValueType::Boolean;
			Set.Add(TEXT("value_type"));
		}
		else if (ValueTypeStr == TEXT("Axis1D") || ValueTypeStr == TEXT("Float"))
		{
			Action->ValueType = EInputActionValueType::Axis1D;
			Set.Add(TEXT("value_type"));
		}
		else if (ValueTypeStr == TEXT("Axis2D") || ValueTypeStr == TEXT("Vector2D"))
		{
			Action->ValueType = EInputActionValueType::Axis2D;
			Set.Add(TEXT("value_type"));
		}
		else if (ValueTypeStr == TEXT("Axis3D") || ValueTypeStr == TEXT("Vector"))
		{
			Action->ValueType = EInputActionValueType::Axis3D;
			Set.Add(TEXT("value_type"));
		}
		else
		{
			Failed.Add(FString::Printf(TEXT("value_type: Unknown '%s'"), *ValueTypeStr));
		}
	}

	// Generic properties via PropertyUtils
	const TSharedPtr<FJsonObject>* PropsObj = nullptr;
	if (Params->TryGetObjectField(TEXT("properties"), PropsObj) && PropsObj)
	{
		for (const auto& Pair : (*PropsObj)->Values)
		{
			FString Err;
			if (PU::SetProperty(Action, Pair.Key, Pair.Value, Err))
			{
				Set.Add(Pair.Key);
			}
			else
			{
				Failed.Add(FString::Printf(TEXT("%s: %s"), *Pair.Key, *Err));
			}
		}
	}

	Action->MarkPackageDirty();
	UEditorAssetLibrary::SaveAsset(AssetPath);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), Failed.IsEmpty());
	Result->SetStringField(TEXT("asset_path"), AssetPath);
	Result->SetNumberField(TEXT("set_count"), Set.Num());

	if (!Failed.IsEmpty())
	{
		TArray<TSharedPtr<FJsonValue>> FailArr;
		for (const FString& F : Failed)
		{
			FailArr.Add(MakeShared<FJsonValueString>(F));
		}
		Result->SetArrayField(TEXT("failed"), FailArr);
	}
	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEnhancedInputCommands::HandleAddInputActionTrigger(
	const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	FString TriggerType;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path'"));
	}
	if (!Params->TryGetStringField(TEXT("trigger_type"), TriggerType))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'trigger_type'"));
	}

	UInputAction* Action = LoadInputAction(AssetPath);
	if (!Action)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Input action not found: %s"), *AssetPath));
	}

	const TSharedPtr<FJsonObject>* PropsObj = nullptr;
	TSharedPtr<FJsonObject> EmptyProps = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> Props = EmptyProps;
	if (Params->TryGetObjectField(TEXT("properties"), PropsObj) && PropsObj)
	{
		Props = *PropsObj;
	}

	UInputTrigger* Trigger = CreateTriggerInstance(Action, TriggerType, Props);
	if (!Trigger)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Unknown trigger type: '%s'"), *TriggerType));
	}

	Action->Triggers.Add(Trigger);
	Action->MarkPackageDirty();
	UEditorAssetLibrary::SaveAsset(AssetPath);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("asset_path"), AssetPath);
	Result->SetStringField(TEXT("trigger_type"), Trigger->GetClass()->GetName());
	Result->SetNumberField(TEXT("trigger_index"), Action->Triggers.Num() - 1);
	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEnhancedInputCommands::HandleAddInputActionModifier(
	const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	FString ModifierType;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path'"));
	}
	if (!Params->TryGetStringField(TEXT("modifier_type"), ModifierType))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'modifier_type'"));
	}

	UInputAction* Action = LoadInputAction(AssetPath);
	if (!Action)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Input action not found: %s"), *AssetPath));
	}

	const TSharedPtr<FJsonObject>* PropsObj = nullptr;
	TSharedPtr<FJsonObject> EmptyProps = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> Props = EmptyProps;
	if (Params->TryGetObjectField(TEXT("properties"), PropsObj) && PropsObj)
	{
		Props = *PropsObj;
	}

	UInputModifier* Modifier = CreateModifierInstance(Action, ModifierType, Props);
	if (!Modifier)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Unknown modifier type: '%s'"), *ModifierType));
	}

	Action->Modifiers.Add(Modifier);
	Action->MarkPackageDirty();
	UEditorAssetLibrary::SaveAsset(AssetPath);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("asset_path"), AssetPath);
	Result->SetStringField(TEXT("modifier_type"), Modifier->GetClass()->GetName());
	Result->SetNumberField(TEXT("modifier_index"), Action->Modifiers.Num() - 1);
	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEnhancedInputCommands::HandleRemoveInputActionTrigger(
	const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	double IndexD = -1;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path'"));
	}
	if (!Params->TryGetNumberField(TEXT("index"), IndexD))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'index'"));
	}

	UInputAction* Action = LoadInputAction(AssetPath);
	if (!Action)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Input action not found: %s"), *AssetPath));
	}

	const int32 Index = static_cast<int32>(IndexD);
	if (!Action->Triggers.IsValidIndex(Index))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Trigger index %d out of range (0-%d)"),
				Index, Action->Triggers.Num() - 1));
	}

	FString RemovedType = Action->Triggers[Index]
		? Action->Triggers[Index]->GetClass()->GetName()
		: TEXT("null");

	Action->Triggers.RemoveAt(Index);
	Action->MarkPackageDirty();
	UEditorAssetLibrary::SaveAsset(AssetPath);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("removed_type"), RemovedType);
	Result->SetNumberField(TEXT("remaining_count"), Action->Triggers.Num());
	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEnhancedInputCommands::HandleRemoveInputActionModifier(
	const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	double IndexD = -1;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path'"));
	}
	if (!Params->TryGetNumberField(TEXT("index"), IndexD))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'index'"));
	}

	UInputAction* Action = LoadInputAction(AssetPath);
	if (!Action)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Input action not found: %s"), *AssetPath));
	}

	const int32 Index = static_cast<int32>(IndexD);
	if (!Action->Modifiers.IsValidIndex(Index))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Modifier index %d out of range (0-%d)"),
				Index, Action->Modifiers.Num() - 1));
	}

	FString RemovedType = Action->Modifiers[Index]
		? Action->Modifiers[Index]->GetClass()->GetName()
		: TEXT("null");

	Action->Modifiers.RemoveAt(Index);
	Action->MarkPackageDirty();
	UEditorAssetLibrary::SaveAsset(AssetPath);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("removed_type"), RemovedType);
	Result->SetNumberField(TEXT("remaining_count"), Action->Modifiers.Num());
	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEnhancedInputCommands::HandleListInputActions(
	const TSharedPtr<FJsonObject>& Params)
{
	FString SearchPath = TEXT("/Game");
	bool bRecursive = true;
	FString Filter;
	int32 MaxResults = 200;

	Params->TryGetStringField(TEXT("path"), SearchPath);
	Params->TryGetBoolField(TEXT("recursive"), bRecursive);
	Params->TryGetStringField(TEXT("filter"), Filter);
	double MaxD = 200;
	if (Params->TryGetNumberField(TEXT("max_results"), MaxD))
	{
		MaxResults = static_cast<int32>(MaxD);
	}

	IAssetRegistry& Registry =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

	FARFilter ARFilter;
	ARFilter.PackagePaths.Add(FName(*SearchPath));
	ARFilter.bRecursivePaths = bRecursive;
	ARFilter.bRecursiveClasses = true;
	ARFilter.ClassPaths.Add(UInputAction::StaticClass()->GetClassPathName());

	TArray<FAssetData> Assets;
	Registry.GetAssets(ARFilter, Assets);

	const FString FilterLower = Filter.ToLower();

	TArray<TSharedPtr<FJsonValue>> AssetArray;
	for (const FAssetData& AD : Assets)
	{
		if (!FilterLower.IsEmpty() &&
			!AD.AssetName.ToString().ToLower().Contains(FilterLower))
		{
			continue;
		}

		if (AssetArray.Num() >= MaxResults)
		{
			break;
		}

		// Load to get ValueType
		UInputAction* IA = Cast<UInputAction>(AD.GetAsset());

		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("name"), AD.AssetName.ToString());
		Obj->SetStringField(TEXT("path"), AD.GetObjectPathString());

		if (IA)
		{
			FString VT;
			switch (IA->ValueType)
			{
			case EInputActionValueType::Boolean: VT = TEXT("Boolean"); break;
			case EInputActionValueType::Axis1D:  VT = TEXT("Axis1D"); break;
			case EInputActionValueType::Axis2D:  VT = TEXT("Axis2D"); break;
			case EInputActionValueType::Axis3D:  VT = TEXT("Axis3D"); break;
			}
			Obj->SetStringField(TEXT("value_type"), VT);
			Obj->SetNumberField(TEXT("trigger_count"), IA->Triggers.Num());
			Obj->SetNumberField(TEXT("modifier_count"), IA->Modifiers.Num());
		}

		AssetArray.Add(MakeShared<FJsonValueObject>(Obj));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetNumberField(TEXT("count"), AssetArray.Num());
	Result->SetArrayField(TEXT("input_actions"), AssetArray);
	return Result;
}

// ---------------------------------------------------------------------------
// Input Mapping Context Commands
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPEnhancedInputCommands::HandleCreateInputMappingContext(
	const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path'"));
	}

	FString PackagePath;
	FString AssetName;
	int32 LastSlash = INDEX_NONE;
	if (AssetPath.FindLastChar('/', LastSlash) && LastSlash != INDEX_NONE)
	{
		PackagePath = AssetPath.Left(LastSlash);
		AssetName = AssetPath.Mid(LastSlash + 1);
	}
	else
	{
		AssetName = AssetPath;
		PackagePath = TEXT("/Game");
	}

	const FString FullPath = PackagePath / AssetName;
	if (UEditorAssetLibrary::DoesAssetExist(FullPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Asset already exists: %s"), *FullPath));
	}

	UDataAssetFactory* Factory = NewObject<UDataAssetFactory>();
	Factory->DataAssetClass = UInputMappingContext::StaticClass();

	IAssetTools& AssetTools =
		FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();

	UObject* NewAsset = AssetTools.CreateAsset(
		AssetName, PackagePath, UInputMappingContext::StaticClass(), Factory);
	if (!NewAsset)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Failed to create input mapping context: %s"), *FullPath));
	}

	UInputMappingContext* IMC = Cast<UInputMappingContext>(NewAsset);

	// Optional description
	FString Description;
	if (Params->TryGetStringField(TEXT("description"), Description))
	{
		IMC->ContextDescription = FText::FromString(Description);
	}

	IMC->MarkPackageDirty();
	UEditorAssetLibrary::SaveAsset(FullPath);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("path"), FullPath);
	Result->SetStringField(TEXT("class"), TEXT("InputMappingContext"));
	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEnhancedInputCommands::HandleGetInputMappingContext(
	const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path'"));
	}

	UInputMappingContext* IMC = LoadInputMappingContext(AssetPath);
	if (!IMC)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Input mapping context not found: %s"), *AssetPath));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("asset_path"), AssetPath);
	Result->SetStringField(TEXT("name"), IMC->GetName());
	Result->SetStringField(TEXT("description"), IMC->ContextDescription.ToString());

	// Mappings
	const TArray<FEnhancedActionKeyMapping>& Mappings = IMC->GetMappings();
	TArray<TSharedPtr<FJsonValue>> MappingArr;
	for (int32 i = 0; i < Mappings.Num(); ++i)
	{
		MappingArr.Add(MakeShared<FJsonValueObject>(SerializeMapping(Mappings[i], i)));
	}
	Result->SetNumberField(TEXT("mapping_count"), Mappings.Num());
	Result->SetArrayField(TEXT("mappings"), MappingArr);

	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEnhancedInputCommands::HandleAddKeyMapping(
	const TSharedPtr<FJsonObject>& Params)
{
	FString ContextPath;
	FString ActionPath;
	FString KeyName;
	if (!Params->TryGetStringField(TEXT("context_path"), ContextPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'context_path'"));
	}
	if (!Params->TryGetStringField(TEXT("action_path"), ActionPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'action_path'"));
	}
	if (!Params->TryGetStringField(TEXT("key"), KeyName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'key'"));
	}

	UInputMappingContext* IMC = LoadInputMappingContext(ContextPath);
	if (!IMC)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("IMC not found: %s"), *ContextPath));
	}

	UInputAction* Action = LoadInputAction(ActionPath);
	if (!Action)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Input action not found: %s"), *ActionPath));
	}

	FKey Key(*KeyName);
	if (!Key.IsValid())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Invalid key: '%s'"), *KeyName));
	}

	FEnhancedActionKeyMapping& NewMapping = IMC->MapKey(Action, Key);

	// Optional triggers array
	const TArray<TSharedPtr<FJsonValue>>* TriggersArr = nullptr;
	if (Params->TryGetArrayField(TEXT("triggers"), TriggersArr) && TriggersArr)
	{
		for (const TSharedPtr<FJsonValue>& TrigVal : *TriggersArr)
		{
			const TSharedPtr<FJsonObject>* TrigObj = nullptr;
			if (TrigVal->TryGetObject(TrigObj) && TrigObj)
			{
				FString TrigType;
				if ((*TrigObj)->TryGetStringField(TEXT("type"), TrigType))
				{
					const TSharedPtr<FJsonObject>* TrigProps = nullptr;
					TSharedPtr<FJsonObject> EmptyProps = MakeShared<FJsonObject>();
					TSharedPtr<FJsonObject> Props = EmptyProps;
					if ((*TrigObj)->TryGetObjectField(TEXT("properties"), TrigProps) && TrigProps)
					{
						Props = *TrigProps;
					}

					UInputTrigger* Trig = CreateTriggerInstance(IMC, TrigType, Props);
					if (Trig)
					{
						NewMapping.Triggers.Add(Trig);
					}
				}
			}
		}
	}

	// Optional modifiers array
	const TArray<TSharedPtr<FJsonValue>>* ModifiersArr = nullptr;
	if (Params->TryGetArrayField(TEXT("modifiers"), ModifiersArr) && ModifiersArr)
	{
		for (const TSharedPtr<FJsonValue>& ModVal : *ModifiersArr)
		{
			const TSharedPtr<FJsonObject>* ModObj = nullptr;
			if (ModVal->TryGetObject(ModObj) && ModObj)
			{
				FString ModType;
				if ((*ModObj)->TryGetStringField(TEXT("type"), ModType))
				{
					const TSharedPtr<FJsonObject>* ModProps = nullptr;
					TSharedPtr<FJsonObject> EmptyProps = MakeShared<FJsonObject>();
					TSharedPtr<FJsonObject> Props = EmptyProps;
					if ((*ModObj)->TryGetObjectField(TEXT("properties"), ModProps) && ModProps)
					{
						Props = *ModProps;
					}

					UInputModifier* Mod = CreateModifierInstance(IMC, ModType, Props);
					if (Mod)
					{
						NewMapping.Modifiers.Add(Mod);
					}
				}
			}
		}
	}

	IMC->MarkPackageDirty();
	UEditorAssetLibrary::SaveAsset(ContextPath);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("context_path"), ContextPath);
	Result->SetStringField(TEXT("action"), Action->GetName());
	Result->SetStringField(TEXT("key"), KeyName);
	Result->SetNumberField(TEXT("mapping_index"), IMC->GetMappings().Num() - 1);
	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEnhancedInputCommands::HandleRemoveKeyMapping(
	const TSharedPtr<FJsonObject>& Params)
{
	FString ContextPath;
	double IndexD = -1;
	if (!Params->TryGetStringField(TEXT("context_path"), ContextPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'context_path'"));
	}
	if (!Params->TryGetNumberField(TEXT("index"), IndexD))
	{
		// Alternative: remove by action + key
		FString ActionPath;
		FString KeyName;
		if (Params->TryGetStringField(TEXT("action_path"), ActionPath) &&
			Params->TryGetStringField(TEXT("key"), KeyName))
		{
			UInputMappingContext* IMC = LoadInputMappingContext(ContextPath);
			if (!IMC)
			{
				return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
					FString::Printf(TEXT("IMC not found: %s"), *ContextPath));
			}

			UInputAction* Action = LoadInputAction(ActionPath);
			if (!Action)
			{
				return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
					FString::Printf(TEXT("Input action not found: %s"), *ActionPath));
			}

			FKey Key(*KeyName);
			IMC->UnmapKey(Action, Key);
			IMC->MarkPackageDirty();
			UEditorAssetLibrary::SaveAsset(ContextPath);

			TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
			Result->SetBoolField(TEXT("success"), true);
			Result->SetNumberField(TEXT("remaining_count"), IMC->GetMappings().Num());
			return Result;
		}

		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Missing 'index' or ('action_path' + 'key')"));
	}

	UInputMappingContext* IMC = LoadInputMappingContext(ContextPath);
	if (!IMC)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("IMC not found: %s"), *ContextPath));
	}

	const int32 Index = static_cast<int32>(IndexD);
	const TArray<FEnhancedActionKeyMapping>& Mappings = IMC->GetMappings();
	if (!Mappings.IsValidIndex(Index))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Mapping index %d out of range (0-%d)"),
				Index, Mappings.Num() - 1));
	}

	// Use UnmapKey with the action and key from the mapping at that index
	const UInputAction* Action = Mappings[Index].Action;
	FKey Key = Mappings[Index].Key;
	IMC->UnmapKey(Action, Key);

	IMC->MarkPackageDirty();
	UEditorAssetLibrary::SaveAsset(ContextPath);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetNumberField(TEXT("remaining_count"), IMC->GetMappings().Num());
	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEnhancedInputCommands::HandleSetKeyMapping(
	const TSharedPtr<FJsonObject>& Params)
{
	FString ContextPath;
	double IndexD = -1;
	if (!Params->TryGetStringField(TEXT("context_path"), ContextPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'context_path'"));
	}
	if (!Params->TryGetNumberField(TEXT("index"), IndexD))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'index'"));
	}

	UInputMappingContext* IMC = LoadInputMappingContext(ContextPath);
	if (!IMC)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("IMC not found: %s"), *ContextPath));
	}

	const int32 Index = static_cast<int32>(IndexD);
	const TArray<FEnhancedActionKeyMapping>& Mappings = IMC->GetMappings();
	if (!Mappings.IsValidIndex(Index))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Mapping index %d out of range (0-%d)"),
				Index, Mappings.Num() - 1));
	}

	FEnhancedActionKeyMapping& Mapping = IMC->GetMapping(Index);

	// Update key if provided
	FString KeyName;
	if (Params->TryGetStringField(TEXT("key"), KeyName))
	{
		FKey NewKey(*KeyName);
		if (!NewKey.IsValid())
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Invalid key: '%s'"), *KeyName));
		}
		Mapping.Key = NewKey;
	}

	// Update action if provided
	FString ActionPath;
	if (Params->TryGetStringField(TEXT("action_path"), ActionPath))
	{
		UInputAction* NewAction = LoadInputAction(ActionPath);
		if (!NewAction)
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Input action not found: %s"), *ActionPath));
		}
		Mapping.Action = NewAction;
	}

	IMC->MarkPackageDirty();
	UEditorAssetLibrary::SaveAsset(ContextPath);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetObjectField(TEXT("mapping"), SerializeMapping(Mapping, Index));
	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEnhancedInputCommands::HandleAddMappingTrigger(
	const TSharedPtr<FJsonObject>& Params)
{
	FString ContextPath;
	FString TriggerType;
	double MappingIndexD = -1;
	if (!Params->TryGetStringField(TEXT("context_path"), ContextPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'context_path'"));
	}
	if (!Params->TryGetNumberField(TEXT("mapping_index"), MappingIndexD))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'mapping_index'"));
	}
	if (!Params->TryGetStringField(TEXT("trigger_type"), TriggerType))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'trigger_type'"));
	}

	UInputMappingContext* IMC = LoadInputMappingContext(ContextPath);
	if (!IMC)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("IMC not found: %s"), *ContextPath));
	}

	const int32 MappingIndex = static_cast<int32>(MappingIndexD);
	if (!IMC->GetMappings().IsValidIndex(MappingIndex))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Mapping index %d out of range"), MappingIndex));
	}

	const TSharedPtr<FJsonObject>* PropsObj = nullptr;
	TSharedPtr<FJsonObject> EmptyProps = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> Props = EmptyProps;
	if (Params->TryGetObjectField(TEXT("properties"), PropsObj) && PropsObj)
	{
		Props = *PropsObj;
	}

	UInputTrigger* Trigger = CreateTriggerInstance(IMC, TriggerType, Props);
	if (!Trigger)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Unknown trigger type: '%s'"), *TriggerType));
	}

	FEnhancedActionKeyMapping& Mapping = IMC->GetMapping(MappingIndex);
	Mapping.Triggers.Add(Trigger);
	IMC->MarkPackageDirty();
	UEditorAssetLibrary::SaveAsset(ContextPath);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetNumberField(TEXT("mapping_index"), MappingIndex);
	Result->SetStringField(TEXT("trigger_type"), Trigger->GetClass()->GetName());
	Result->SetNumberField(TEXT("trigger_index"), Mapping.Triggers.Num() - 1);
	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEnhancedInputCommands::HandleAddMappingModifier(
	const TSharedPtr<FJsonObject>& Params)
{
	FString ContextPath;
	FString ModifierType;
	double MappingIndexD = -1;
	if (!Params->TryGetStringField(TEXT("context_path"), ContextPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'context_path'"));
	}
	if (!Params->TryGetNumberField(TEXT("mapping_index"), MappingIndexD))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'mapping_index'"));
	}
	if (!Params->TryGetStringField(TEXT("modifier_type"), ModifierType))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'modifier_type'"));
	}

	UInputMappingContext* IMC = LoadInputMappingContext(ContextPath);
	if (!IMC)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("IMC not found: %s"), *ContextPath));
	}

	const int32 MappingIndex = static_cast<int32>(MappingIndexD);
	if (!IMC->GetMappings().IsValidIndex(MappingIndex))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Mapping index %d out of range"), MappingIndex));
	}

	const TSharedPtr<FJsonObject>* PropsObj = nullptr;
	TSharedPtr<FJsonObject> EmptyProps = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> Props = EmptyProps;
	if (Params->TryGetObjectField(TEXT("properties"), PropsObj) && PropsObj)
	{
		Props = *PropsObj;
	}

	UInputModifier* Modifier = CreateModifierInstance(IMC, ModifierType, Props);
	if (!Modifier)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Unknown modifier type: '%s'"), *ModifierType));
	}

	FEnhancedActionKeyMapping& Mapping = IMC->GetMapping(MappingIndex);
	Mapping.Modifiers.Add(Modifier);
	IMC->MarkPackageDirty();
	UEditorAssetLibrary::SaveAsset(ContextPath);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetNumberField(TEXT("mapping_index"), MappingIndex);
	Result->SetStringField(TEXT("modifier_type"), Modifier->GetClass()->GetName());
	Result->SetNumberField(TEXT("modifier_index"), Mapping.Modifiers.Num() - 1);
	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEnhancedInputCommands::HandleRemoveMappingTrigger(
	const TSharedPtr<FJsonObject>& Params)
{
	FString ContextPath;
	double MappingIndexD = -1;
	double TriggerIndexD = -1;
	if (!Params->TryGetStringField(TEXT("context_path"), ContextPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'context_path'"));
	}
	if (!Params->TryGetNumberField(TEXT("mapping_index"), MappingIndexD))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'mapping_index'"));
	}
	if (!Params->TryGetNumberField(TEXT("trigger_index"), TriggerIndexD))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'trigger_index'"));
	}

	UInputMappingContext* IMC = LoadInputMappingContext(ContextPath);
	if (!IMC)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("IMC not found: %s"), *ContextPath));
	}

	const int32 MappingIndex = static_cast<int32>(MappingIndexD);
	if (!IMC->GetMappings().IsValidIndex(MappingIndex))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Mapping index %d out of range"), MappingIndex));
	}

	FEnhancedActionKeyMapping& Mapping = IMC->GetMapping(MappingIndex);
	const int32 TriggerIndex = static_cast<int32>(TriggerIndexD);
	if (!Mapping.Triggers.IsValidIndex(TriggerIndex))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Trigger index %d out of range (0-%d)"),
				TriggerIndex, Mapping.Triggers.Num() - 1));
	}

	FString RemovedType = Mapping.Triggers[TriggerIndex]
		? Mapping.Triggers[TriggerIndex]->GetClass()->GetName()
		: TEXT("null");

	Mapping.Triggers.RemoveAt(TriggerIndex);
	IMC->MarkPackageDirty();
	UEditorAssetLibrary::SaveAsset(ContextPath);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("removed_type"), RemovedType);
	Result->SetNumberField(TEXT("remaining_count"), Mapping.Triggers.Num());
	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEnhancedInputCommands::HandleRemoveMappingModifier(
	const TSharedPtr<FJsonObject>& Params)
{
	FString ContextPath;
	double MappingIndexD = -1;
	double ModifierIndexD = -1;
	if (!Params->TryGetStringField(TEXT("context_path"), ContextPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'context_path'"));
	}
	if (!Params->TryGetNumberField(TEXT("mapping_index"), MappingIndexD))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'mapping_index'"));
	}
	if (!Params->TryGetNumberField(TEXT("modifier_index"), ModifierIndexD))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'modifier_index'"));
	}

	UInputMappingContext* IMC = LoadInputMappingContext(ContextPath);
	if (!IMC)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("IMC not found: %s"), *ContextPath));
	}

	const int32 MappingIndex = static_cast<int32>(MappingIndexD);
	if (!IMC->GetMappings().IsValidIndex(MappingIndex))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Mapping index %d out of range"), MappingIndex));
	}

	FEnhancedActionKeyMapping& Mapping = IMC->GetMapping(MappingIndex);
	const int32 ModifierIndex = static_cast<int32>(ModifierIndexD);
	if (!Mapping.Modifiers.IsValidIndex(ModifierIndex))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Modifier index %d out of range (0-%d)"),
				ModifierIndex, Mapping.Modifiers.Num() - 1));
	}

	FString RemovedType = Mapping.Modifiers[ModifierIndex]
		? Mapping.Modifiers[ModifierIndex]->GetClass()->GetName()
		: TEXT("null");

	Mapping.Modifiers.RemoveAt(ModifierIndex);
	IMC->MarkPackageDirty();
	UEditorAssetLibrary::SaveAsset(ContextPath);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("removed_type"), RemovedType);
	Result->SetNumberField(TEXT("remaining_count"), Mapping.Modifiers.Num());
	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEnhancedInputCommands::HandleListInputMappingContexts(
	const TSharedPtr<FJsonObject>& Params)
{
	FString SearchPath = TEXT("/Game");
	bool bRecursive = true;
	FString Filter;
	int32 MaxResults = 200;

	Params->TryGetStringField(TEXT("path"), SearchPath);
	Params->TryGetBoolField(TEXT("recursive"), bRecursive);
	Params->TryGetStringField(TEXT("filter"), Filter);
	double MaxD = 200;
	if (Params->TryGetNumberField(TEXT("max_results"), MaxD))
	{
		MaxResults = static_cast<int32>(MaxD);
	}

	IAssetRegistry& Registry =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

	FARFilter ARFilter;
	ARFilter.PackagePaths.Add(FName(*SearchPath));
	ARFilter.bRecursivePaths = bRecursive;
	ARFilter.bRecursiveClasses = true;
	ARFilter.ClassPaths.Add(UInputMappingContext::StaticClass()->GetClassPathName());

	TArray<FAssetData> Assets;
	Registry.GetAssets(ARFilter, Assets);

	const FString FilterLower = Filter.ToLower();

	TArray<TSharedPtr<FJsonValue>> AssetArray;
	for (const FAssetData& AD : Assets)
	{
		if (!FilterLower.IsEmpty() &&
			!AD.AssetName.ToString().ToLower().Contains(FilterLower))
		{
			continue;
		}

		if (AssetArray.Num() >= MaxResults)
		{
			break;
		}

		UInputMappingContext* IMC = Cast<UInputMappingContext>(AD.GetAsset());

		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("name"), AD.AssetName.ToString());
		Obj->SetStringField(TEXT("path"), AD.GetObjectPathString());

		if (IMC)
		{
			Obj->SetNumberField(TEXT("mapping_count"), IMC->GetMappings().Num());
			Obj->SetStringField(TEXT("description"), IMC->ContextDescription.ToString());
		}

		AssetArray.Add(MakeShared<FJsonValueObject>(Obj));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetNumberField(TEXT("count"), AssetArray.Num());
	Result->SetArrayField(TEXT("mapping_contexts"), AssetArray);
	return Result;
}

// ---------------------------------------------------------------------------
// Discovery / Utility Commands
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPEnhancedInputCommands::HandleListTriggerTypes(
	const TSharedPtr<FJsonObject>& Params)
{
	FString Filter;
	Params->TryGetStringField(TEXT("filter"), Filter);
	const FString FilterLower = Filter.ToLower();

	TArray<UClass*> Derived;
	GetDerivedClasses(UInputTrigger::StaticClass(), Derived, true);

	TArray<TSharedPtr<FJsonValue>> Items;
	for (UClass* C : Derived)
	{
		if (!C || C->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated))
		{
			continue;
		}
		// Skip hidden dropdown classes
		if (C->HasMetaData(TEXT("HideDropdown")))
		{
			continue;
		}

		const FString Name = C->GetName();
		if (!FilterLower.IsEmpty() && !Name.ToLower().Contains(FilterLower))
		{
			continue;
		}

		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("name"), Name);

		// Get display name from metadata
		FString DisplayName = Name;
		if (C->HasMetaData(TEXT("DisplayName")))
		{
			DisplayName = C->GetMetaData(TEXT("DisplayName"));
		}
		Obj->SetStringField(TEXT("display_name"), DisplayName);

		Obj->SetStringField(TEXT("parent"), C->GetSuperClass()->GetName());

		// List editable properties
		TArray<TSharedPtr<FJsonValue>> PropArr;
		for (TFieldIterator<FProperty> It(C, EFieldIteratorFlags::ExcludeSuper); It; ++It)
		{
			if ((*It)->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible))
			{
				TSharedPtr<FJsonObject> PropObj = MakeShared<FJsonObject>();
				PropObj->SetStringField(TEXT("name"), (*It)->GetName());
				PropObj->SetStringField(TEXT("type"), (*It)->GetCPPType());
				PropArr.Add(MakeShared<FJsonValueObject>(PropObj));
			}
		}
		Obj->SetArrayField(TEXT("properties"), PropArr);

		Items.Add(MakeShared<FJsonValueObject>(Obj));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetNumberField(TEXT("count"), Items.Num());
	Result->SetArrayField(TEXT("trigger_types"), Items);
	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEnhancedInputCommands::HandleListModifierTypes(
	const TSharedPtr<FJsonObject>& Params)
{
	FString Filter;
	Params->TryGetStringField(TEXT("filter"), Filter);
	const FString FilterLower = Filter.ToLower();

	TArray<UClass*> Derived;
	GetDerivedClasses(UInputModifier::StaticClass(), Derived, true);

	TArray<TSharedPtr<FJsonValue>> Items;
	for (UClass* C : Derived)
	{
		if (!C || C->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated))
		{
			continue;
		}

		const FString Name = C->GetName();
		if (!FilterLower.IsEmpty() && !Name.ToLower().Contains(FilterLower))
		{
			continue;
		}

		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("name"), Name);

		FString DisplayName = Name;
		if (C->HasMetaData(TEXT("DisplayName")))
		{
			DisplayName = C->GetMetaData(TEXT("DisplayName"));
		}
		Obj->SetStringField(TEXT("display_name"), DisplayName);

		Obj->SetStringField(TEXT("parent"), C->GetSuperClass()->GetName());

		// List editable properties
		TArray<TSharedPtr<FJsonValue>> PropArr;
		for (TFieldIterator<FProperty> It(C, EFieldIteratorFlags::ExcludeSuper); It; ++It)
		{
			if ((*It)->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible))
			{
				TSharedPtr<FJsonObject> PropObj = MakeShared<FJsonObject>();
				PropObj->SetStringField(TEXT("name"), (*It)->GetName());
				PropObj->SetStringField(TEXT("type"), (*It)->GetCPPType());
				PropArr.Add(MakeShared<FJsonValueObject>(PropObj));
			}
		}
		Obj->SetArrayField(TEXT("properties"), PropArr);

		Items.Add(MakeShared<FJsonValueObject>(Obj));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetNumberField(TEXT("count"), Items.Num());
	Result->SetArrayField(TEXT("modifier_types"), Items);
	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEnhancedInputCommands::HandleListInputKeys(
	const TSharedPtr<FJsonObject>& Params)
{
	FString Filter;
	FString Category;
	Params->TryGetStringField(TEXT("filter"), Filter);
	Params->TryGetStringField(TEXT("category"), Category);
	const FString FilterLower = Filter.ToLower();
	const FString CategoryLower = Category.ToLower();

	int32 MaxResults = 500;
	double MaxD = 500;
	if (Params->TryGetNumberField(TEXT("max_results"), MaxD))
	{
		MaxResults = static_cast<int32>(MaxD);
	}

	TArray<FKey> AllKeys;
	EKeys::GetAllKeys(AllKeys);

	TArray<TSharedPtr<FJsonValue>> Items;
	for (const FKey& Key : AllKeys)
	{
		if (Items.Num() >= MaxResults)
		{
			break;
		}

		const FString KeyName = Key.GetFName().ToString();
		const FString DisplayName = Key.GetDisplayName().ToString();

		if (!FilterLower.IsEmpty())
		{
			if (!KeyName.ToLower().Contains(FilterLower) &&
				!DisplayName.ToLower().Contains(FilterLower))
			{
				continue;
			}
		}

		// Category filtering
		if (!CategoryLower.IsEmpty())
		{
			bool bMatch = false;
			if (CategoryLower == TEXT("keyboard") && Key.IsDigital() && !Key.IsGamepadKey() && !Key.IsMouseButton())
			{
				bMatch = true;
			}
			else if (CategoryLower == TEXT("mouse") && Key.IsMouseButton())
			{
				bMatch = true;
			}
			else if (CategoryLower == TEXT("gamepad") && Key.IsGamepadKey())
			{
				bMatch = true;
			}
			else if (CategoryLower == TEXT("analog") && Key.IsAnalog())
			{
				bMatch = true;
			}
			else if (CategoryLower == TEXT("digital") && Key.IsDigital())
			{
				bMatch = true;
			}

			if (!bMatch)
			{
				continue;
			}
		}

		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("name"), KeyName);
		Obj->SetStringField(TEXT("display_name"), DisplayName);
		Obj->SetBoolField(TEXT("is_gamepad"), Key.IsGamepadKey());
		Obj->SetBoolField(TEXT("is_mouse"), Key.IsMouseButton());
		Obj->SetBoolField(TEXT("is_analog"), Key.IsAnalog());
		Obj->SetBoolField(TEXT("is_digital"), Key.IsDigital());

		Items.Add(MakeShared<FJsonValueObject>(Obj));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetNumberField(TEXT("count"), Items.Num());
	Result->SetArrayField(TEXT("keys"), Items);
	return Result;
}
