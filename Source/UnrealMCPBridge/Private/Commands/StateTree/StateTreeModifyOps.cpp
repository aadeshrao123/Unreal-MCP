#include "Commands/EpicUnrealMCPStateTreeCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "Commands/EpicUnrealMCPPropertyUtils.h"
#include "StateTree/StateTreeHelpers.h"

#include "StateTree.h"
#include "StateTreeEditorData.h"
#include "StateTreeState.h"
#include "StateTreeEditorNode.h"
#include "StateTreeTypes.h"
#include "StateTreeSchema.h"
#include "StateTreePropertyBindings.h"
#include "StateTreeEditorPropertyBindings.h"
#include "StructUtils/InstancedStruct.h"

using PU = FEpicUnrealMCPPropertyUtils;

/**
 * Convert a JSON value to a plain string suitable for ImportText_Direct.
 */
static FString JsonValueToImportString(const TSharedPtr<FJsonValue>& Value)
{
	if (!Value.IsValid())
	{
		return FString();
	}

	switch (Value->Type)
	{
	case EJson::String:
		return Value->AsString();
	case EJson::Number:
		return FString::SanitizeFloat(Value->AsNumber());
	case EJson::Boolean:
		return Value->AsBool() ? TEXT("true") : TEXT("false");
	case EJson::Object:
	case EJson::Array:
	{
		FString JsonStr;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonStr);
		FJsonSerializer::Serialize(Value.ToSharedRef(), TEXT(""), Writer);
		return JsonStr;
	}
	default:
		return FString();
	}
}

/**
 * Set a property on an FInstancedStruct by name using reflection.
 * Returns true on success, populates OutError on failure.
 */
static bool SetInstancedStructProperty(
	FInstancedStruct& Struct,
	const FString& PropertyName,
	const FString& ValueStr,
	FString& OutError)
{
	if (!Struct.IsValid())
	{
		OutError = TEXT("InstancedStruct is not valid");
		return false;
	}

	UScriptStruct* ScriptStruct = const_cast<UScriptStruct*>(Struct.GetScriptStruct());
	uint8* StructData = Struct.GetMutableMemory();

	if (!ScriptStruct || !StructData)
	{
		OutError = TEXT("InstancedStruct has no struct or memory");
		return false;
	}

	FProperty* Prop = ScriptStruct->FindPropertyByName(FName(*PropertyName));
	if (!Prop)
	{
		OutError = FString::Printf(
			TEXT("Property '%s' not found on struct '%s'"),
			*PropertyName, *ScriptStruct->GetName());
		return false;
	}

	void* PropertyAddress = Prop->ContainerPtrToValuePtr<void>(StructData);
	const TCHAR* Result = Prop->ImportText_Direct(*ValueStr, PropertyAddress, nullptr, PPF_None);
	if (!Result)
	{
		OutError = FString::Printf(
			TEXT("ImportText failed for property '%s' with value '%s' on struct '%s'"),
			*PropertyName, *ValueStr, *ScriptStruct->GetName());
		return false;
	}

	return true;
}

/**
 * Parse a linear color from a hex string ("#RRGGBB" / "#RRGGBBAA") or
 * comma-separated floats ("R,G,B" / "R,G,B,A").
 */
static bool ParseColorString(const FString& Str, FLinearColor& OutColor)
{
	if (Str.IsEmpty())
	{
		return false;
	}

	// Hex format
	if (Str.StartsWith(TEXT("#")))
	{
		FColor SRGBColor = FColor::FromHex(Str);
		OutColor = FLinearColor(SRGBColor);
		return true;
	}

	// Comma-separated floats
	TArray<FString> Parts;
	Str.ParseIntoArray(Parts, TEXT(","));
	if (Parts.Num() >= 3)
	{
		OutColor.R = FCString::Atof(*Parts[0].TrimStartAndEnd());
		OutColor.G = FCString::Atof(*Parts[1].TrimStartAndEnd());
		OutColor.B = FCString::Atof(*Parts[2].TrimStartAndEnd());
		OutColor.A = Parts.Num() >= 4 ? FCString::Atof(*Parts[3].TrimStartAndEnd()) : 1.0f;
		return true;
	}

	return false;
}

// ---------------------------------------------------------------------------
// HandleSetStateTreeStateProperty
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPStateTreeCommands::HandleSetStateTreeStateProperty(
	const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));
	}

	FString PropertyName;
	if (!Params->TryGetStringField(TEXT("property_name"), PropertyName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'property_name' parameter"));
	}

	FString ValueStr;
	if (!Params->TryGetStringField(TEXT("value"), ValueStr))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'value' parameter"));
	}

	FString Error;
	UStateTree* Tree = StateTreeHelpers::LoadStateTree(AssetPath, Error);
	if (!Tree)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	UStateTreeEditorData* EditorData = StateTreeHelpers::GetEditorData(Tree, Error);
	if (!EditorData)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	UStateTreeState* State = StateTreeHelpers::ResolveState(EditorData, Params, Error);
	if (!State)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	// Handle well-known property names
	if (PropertyName.Equals(TEXT("Name"), ESearchCase::IgnoreCase))
	{
		State->Name = FName(*ValueStr);
	}
	else if (PropertyName.Equals(TEXT("Type"), ESearchCase::IgnoreCase))
	{
		EStateTreeStateType NewType;
		if (!StateTreeHelpers::ParseStateType(ValueStr, NewType))
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Invalid state type: '%s'. Valid: State, Group, Linked, LinkedAsset, Subtree"), *ValueStr));
		}
		State->Type = NewType;
	}
	else if (PropertyName.Equals(TEXT("SelectionBehavior"), ESearchCase::IgnoreCase))
	{
		EStateTreeStateSelectionBehavior NewBehavior;
		if (!StateTreeHelpers::ParseSelectionBehavior(ValueStr, NewBehavior))
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Invalid selection behavior: '%s'. Valid: None, TryEnterState, TrySelectChildrenInOrder, TrySelectChildrenAtRandom, TrySelectChildrenWithHighestUtility, TrySelectChildrenAtRandomWeightedByUtility, TryFollowTransitions"), *ValueStr));
		}
		State->SelectionBehavior = NewBehavior;
	}
	else if (PropertyName.Equals(TEXT("Enabled"), ESearchCase::IgnoreCase))
	{
		State->bEnabled = ValueStr.Equals(TEXT("true"), ESearchCase::IgnoreCase) || ValueStr == TEXT("1");
	}
	else if (PropertyName.Equals(TEXT("Tag"), ESearchCase::IgnoreCase))
	{
		State->Tag = FGameplayTag::RequestGameplayTag(FName(*ValueStr), false);
		if (!State->Tag.IsValid() && !ValueStr.IsEmpty())
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("GameplayTag '%s' is not registered"), *ValueStr));
		}
	}
	else if (PropertyName.Equals(TEXT("Weight"), ESearchCase::IgnoreCase))
	{
		State->Weight = FCString::Atof(*ValueStr);
	}
	else
	{
		// Fallback: try UObject reflection on the UStateTreeState
		FProperty* Prop = State->GetClass()->FindPropertyByName(FName(*PropertyName));
		if (!Prop)
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Unknown state property: '%s'"), *PropertyName));
		}

		void* PropertyAddress = Prop->ContainerPtrToValuePtr<void>(State);
		const TCHAR* Result = Prop->ImportText_Direct(*ValueStr, PropertyAddress, State, PPF_None);
		if (!Result)
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("ImportText failed for state property '%s' with value '%s'"), *PropertyName, *ValueStr));
		}
	}

	Tree->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("state_name"), State->Name.ToString());
	Result->SetStringField(TEXT("state_id"), State->ID.ToString());
	Result->SetStringField(TEXT("property"), PropertyName);
	Result->SetStringField(TEXT("value"), ValueStr);
	return Result;
}

// ---------------------------------------------------------------------------
// HandleSetStateTreeNodeProperty
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPStateTreeCommands::HandleSetStateTreeNodeProperty(
	const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));
	}

	FString NodeGuidStr;
	if (!Params->TryGetStringField(TEXT("node_guid"), NodeGuidStr))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'node_guid' parameter"));
	}

	FString PropertyName;
	if (!Params->TryGetStringField(TEXT("property_name"), PropertyName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'property_name' parameter"));
	}

	// Accept both string and complex JSON values for the property value
	TSharedPtr<FJsonValue> PropertyValueJson = Params->TryGetField(TEXT("property_value"));
	if (!PropertyValueJson.IsValid())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'property_value' parameter"));
	}
	FString PropertyValueStr = JsonValueToImportString(PropertyValueJson);

	FString TargetStr = TEXT("node");
	Params->TryGetStringField(TEXT("target"), TargetStr);

	FGuid NodeGuid;
	if (!FGuid::Parse(NodeGuidStr, NodeGuid))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Invalid GUID format: '%s'"), *NodeGuidStr));
	}

	FString Error;
	UStateTree* Tree = StateTreeHelpers::LoadStateTree(AssetPath, Error);
	if (!Tree)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	UStateTreeEditorData* EditorData = StateTreeHelpers::GetEditorData(Tree, Error);
	if (!EditorData)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	FString Context;
	TArray<FStateTreeEditorNode>* ContainingArray = nullptr;
	int32 NodeIndex = INDEX_NONE;
	FStateTreeEditorNode* EditorNode = StateTreeHelpers::ResolveNodeByGuid(
		EditorData, NodeGuid, Context, ContainingArray, NodeIndex, Error);

	if (!EditorNode)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	// Determine which FInstancedStruct to write to
	FInstancedStruct* TargetStruct = nullptr;
	FString TargetLabel;

	if (TargetStr.Equals(TEXT("instance"), ESearchCase::IgnoreCase))
	{
		if (!EditorNode->Instance.IsValid())
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				TEXT("Node has no instance data to set properties on"));
		}
		TargetStruct = &EditorNode->Instance;
		TargetLabel = TEXT("instance");
	}
	else
	{
		if (!EditorNode->Node.IsValid())
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				TEXT("Node struct is not valid"));
		}
		TargetStruct = &EditorNode->Node;
		TargetLabel = TEXT("node");
	}

	FString SetError;
	if (!SetInstancedStructProperty(*TargetStruct, PropertyName, PropertyValueStr, SetError))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(SetError);
	}

	Tree->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("node_id"), NodeGuidStr);
	Result->SetStringField(TEXT("context"), Context);
	Result->SetStringField(TEXT("target"), TargetLabel);
	Result->SetStringField(TEXT("property"), PropertyName);
	Result->SetStringField(TEXT("value"), PropertyValueStr);
	return Result;
}

// ---------------------------------------------------------------------------
// HandleSetStateTreeTransitionProperty
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPStateTreeCommands::HandleSetStateTreeTransitionProperty(
	const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));
	}

	double TransitionIndexD = -1;
	if (!Params->TryGetNumberField(TEXT("transition_index"), TransitionIndexD))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'transition_index' parameter"));
	}
	int32 TransitionIndex = static_cast<int32>(TransitionIndexD);

	FString PropertyName;
	if (!Params->TryGetStringField(TEXT("property_name"), PropertyName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'property_name' parameter"));
	}

	FString ValueStr;
	if (!Params->TryGetStringField(TEXT("value"), ValueStr))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'value' parameter"));
	}

	FString Error;
	UStateTree* Tree = StateTreeHelpers::LoadStateTree(AssetPath, Error);
	if (!Tree)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	UStateTreeEditorData* EditorData = StateTreeHelpers::GetEditorData(Tree, Error);
	if (!EditorData)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	UStateTreeState* State = StateTreeHelpers::ResolveState(EditorData, Params, Error);
	if (!State)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	if (TransitionIndex < 0 || TransitionIndex >= State->Transitions.Num())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Transition index %d out of range (state '%s' has %d transitions)"),
				TransitionIndex, *State->Name.ToString(), State->Transitions.Num()));
	}

	FStateTreeTransition& Transition = State->Transitions[TransitionIndex];

	if (PropertyName.Equals(TEXT("Trigger"), ESearchCase::IgnoreCase))
	{
		EStateTreeTransitionTrigger NewTrigger;
		if (!StateTreeHelpers::ParseTransitionTrigger(ValueStr, NewTrigger))
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Invalid trigger: '%s'. Valid: OnStateCompleted, OnStateSucceeded, OnStateFailed, OnTick, OnEvent"), *ValueStr));
		}
		Transition.Trigger = NewTrigger;
	}
	else if (PropertyName.Equals(TEXT("Priority"), ESearchCase::IgnoreCase))
	{
		EStateTreeTransitionPriority NewPriority;
		if (!StateTreeHelpers::ParseTransitionPriority(ValueStr, NewPriority))
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Invalid priority: '%s'. Valid: Low, Normal, Medium, High, Critical"), *ValueStr));
		}
		Transition.Priority = NewPriority;
	}
	else if (PropertyName.Equals(TEXT("TargetState"), ESearchCase::IgnoreCase))
	{
		// Resolve target state — support special keywords and state resolution
		if (ValueStr.Equals(TEXT("Succeeded"), ESearchCase::IgnoreCase) ||
			ValueStr.Equals(TEXT("TreeSucceeded"), ESearchCase::IgnoreCase))
		{
#if WITH_EDITORONLY_DATA
			Transition.State.LinkType = EStateTreeTransitionType::Succeeded;
			Transition.State.ID = FGuid();
			Transition.State.Name = NAME_None;
#endif
		}
		else if (ValueStr.Equals(TEXT("Failed"), ESearchCase::IgnoreCase) ||
				 ValueStr.Equals(TEXT("TreeFailed"), ESearchCase::IgnoreCase))
		{
#if WITH_EDITORONLY_DATA
			Transition.State.LinkType = EStateTreeTransitionType::Failed;
			Transition.State.ID = FGuid();
			Transition.State.Name = NAME_None;
#endif
		}
		else if (ValueStr.Equals(TEXT("None"), ESearchCase::IgnoreCase) ||
				 ValueStr.Equals(TEXT("TreeStopped"), ESearchCase::IgnoreCase))
		{
#if WITH_EDITORONLY_DATA
			Transition.State.LinkType = EStateTreeTransitionType::None;
			Transition.State.ID = FGuid();
			Transition.State.Name = NAME_None;
#endif
		}
		else if (ValueStr.Equals(TEXT("NextSibling"), ESearchCase::IgnoreCase) ||
				 ValueStr.Equals(TEXT("NextState"), ESearchCase::IgnoreCase))
		{
#if WITH_EDITORONLY_DATA
			Transition.State.LinkType = EStateTreeTransitionType::NextState;
			Transition.State.ID = FGuid();
			Transition.State.Name = NAME_None;
#endif
		}
		else if (ValueStr.Equals(TEXT("NextSelectableState"), ESearchCase::IgnoreCase))
		{
#if WITH_EDITORONLY_DATA
			Transition.State.LinkType = EStateTreeTransitionType::NextSelectableState;
			Transition.State.ID = FGuid();
			Transition.State.Name = NAME_None;
#endif
		}
		else
		{
			FGuid TargetGuid;
			UStateTreeState* TargetState = nullptr;

			if (FGuid::Parse(ValueStr, TargetGuid))
			{
				FString ResolveError;
				TargetState = StateTreeHelpers::ResolveStateByGuid(EditorData, TargetGuid, ResolveError);
			}

			if (!TargetState)
			{
				FString ResolveError;
				TargetState = StateTreeHelpers::ResolveStateByName(EditorData, ValueStr, ResolveError);
				if (!TargetState)
				{
					return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
						FString::Printf(TEXT("Could not resolve target state '%s': %s"), *ValueStr, *ResolveError));
				}
			}

#if WITH_EDITORONLY_DATA
			Transition.State = TargetState->GetLinkToState();
#endif
		}
	}
	else if (PropertyName.Equals(TEXT("DelayDuration"), ESearchCase::IgnoreCase))
	{
		Transition.DelayDuration = FCString::Atof(*ValueStr);
	}
	else if (PropertyName.Equals(TEXT("DelayRandomVariance"), ESearchCase::IgnoreCase))
	{
		Transition.DelayRandomVariance = FCString::Atof(*ValueStr);
	}
	else if (PropertyName.Equals(TEXT("DelayTransition"), ESearchCase::IgnoreCase))
	{
		Transition.bDelayTransition = ValueStr.Equals(TEXT("true"), ESearchCase::IgnoreCase) || ValueStr == TEXT("1");
	}
	else if (PropertyName.Equals(TEXT("Enabled"), ESearchCase::IgnoreCase))
	{
		Transition.bTransitionEnabled = ValueStr.Equals(TEXT("true"), ESearchCase::IgnoreCase) || ValueStr == TEXT("1");
	}
	else if (PropertyName.Equals(TEXT("EventTag"), ESearchCase::IgnoreCase))
	{
		Transition.RequiredEvent.Tag = FGameplayTag::RequestGameplayTag(FName(*ValueStr), false);
		if (!Transition.RequiredEvent.Tag.IsValid() && !ValueStr.IsEmpty())
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("GameplayTag '%s' is not registered"), *ValueStr));
		}
	}
	else
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(
				TEXT("Unknown transition property: '%s'. Valid: Trigger, Priority, TargetState, DelayDuration, DelayRandomVariance, DelayTransition, Enabled, EventTag"),
				*PropertyName));
	}

	Tree->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("state_name"), State->Name.ToString());
	Result->SetNumberField(TEXT("transition_index"), TransitionIndex);
	Result->SetStringField(TEXT("property"), PropertyName);
	Result->SetStringField(TEXT("value"), ValueStr);
	return Result;
}

// ---------------------------------------------------------------------------
// HandleSetStateTreeSchema
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPStateTreeCommands::HandleSetStateTreeSchema(
	const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));
	}

	FString SchemaClassName;
	if (!Params->TryGetStringField(TEXT("schema_class"), SchemaClassName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'schema_class' parameter"));
	}

	FString Error;
	UStateTree* Tree = StateTreeHelpers::LoadStateTree(AssetPath, Error);
	if (!Tree)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	UStateTreeEditorData* EditorData = StateTreeHelpers::GetEditorData(Tree, Error);
	if (!EditorData)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	UClass* SchemaClass = StateTreeHelpers::ResolveSchemaClass(SchemaClassName, Error);
	if (!SchemaClass)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	// Capture old schema for reporting
	FString OldSchemaName = TEXT("None");
	if (EditorData->Schema)
	{
		OldSchemaName = EditorData->Schema->GetClass()->GetName();
	}

	// Create new schema instance, owned by the EditorData
	UStateTreeSchema* NewSchema = NewObject<UStateTreeSchema>(EditorData, SchemaClass);
	if (!NewSchema)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Failed to create schema instance of class '%s'"), *SchemaClassName));
	}

	EditorData->Schema = NewSchema;

	Tree->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("old_schema"), OldSchemaName);
	Result->SetStringField(TEXT("new_schema"), SchemaClass->GetName());
	Result->SetStringField(TEXT("warning"),
		TEXT("Schema changed. Existing nodes may become invalid if they are not supported by the new schema. Recompile to check."));
	return Result;
}

// ---------------------------------------------------------------------------
// HandleAddStateTreeBinding
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPStateTreeCommands::HandleAddStateTreeBinding(
	const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));
	}

	FString SourceNodeGuidStr;
	if (!Params->TryGetStringField(TEXT("source_node_guid"), SourceNodeGuidStr))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'source_node_guid' parameter"));
	}

	FString SourceProperty;
	if (!Params->TryGetStringField(TEXT("source_property"), SourceProperty))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'source_property' parameter"));
	}

	FString TargetNodeGuidStr;
	if (!Params->TryGetStringField(TEXT("target_node_guid"), TargetNodeGuidStr))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'target_node_guid' parameter"));
	}

	FString TargetProperty;
	if (!Params->TryGetStringField(TEXT("target_property"), TargetProperty))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'target_property' parameter"));
	}

	FGuid SourceGuid;
	if (!FGuid::Parse(SourceNodeGuidStr, SourceGuid))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Invalid source GUID: '%s'"), *SourceNodeGuidStr));
	}

	FGuid TargetGuid;
	if (!FGuid::Parse(TargetNodeGuidStr, TargetGuid))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Invalid target GUID: '%s'"), *TargetNodeGuidStr));
	}

	FString Error;
	UStateTree* Tree = StateTreeHelpers::LoadStateTree(AssetPath, Error);
	if (!Tree)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	UStateTreeEditorData* EditorData = StateTreeHelpers::GetEditorData(Tree, Error);
	if (!EditorData)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	// Verify source and target nodes exist
	FString SourceContext;
	TArray<FStateTreeEditorNode>* SourceArray = nullptr;
	int32 SourceIndex = INDEX_NONE;
	FStateTreeEditorNode* SourceNode = StateTreeHelpers::ResolveNodeByGuid(
		EditorData, SourceGuid, SourceContext, SourceArray, SourceIndex, Error);

	if (!SourceNode)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Source node not found: %s"), *Error));
	}

	FString TargetContext;
	TArray<FStateTreeEditorNode>* TargetArray = nullptr;
	int32 TargetIndex = INDEX_NONE;
	FStateTreeEditorNode* TargetNode = StateTreeHelpers::ResolveNodeByGuid(
		EditorData, TargetGuid, TargetContext, TargetArray, TargetIndex, Error);

	if (!TargetNode)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Target node not found: %s"), *Error));
	}

	// Construct binding paths using node IDs and property path strings
	FPropertyBindingPath SourcePath(SourceNode->ID);
	SourcePath.FromString(SourceProperty);

	FPropertyBindingPath TargetPath(TargetNode->ID);
	TargetPath.FromString(TargetProperty);

	// AddBinding removes any existing binding to the target path first
	EditorData->EditorBindings.AddBinding(SourcePath, TargetPath);

	Tree->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("source_node"), SourceNodeGuidStr);
	Result->SetStringField(TEXT("source_property"), SourceProperty);
	Result->SetStringField(TEXT("source_context"), SourceContext);
	Result->SetStringField(TEXT("target_node"), TargetNodeGuidStr);
	Result->SetStringField(TEXT("target_property"), TargetProperty);
	Result->SetStringField(TEXT("target_context"), TargetContext);
	return Result;
}

// ---------------------------------------------------------------------------
// HandleSetStateTreeColor
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPStateTreeCommands::HandleSetStateTreeColor(
	const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));
	}

	FString DisplayName;
	if (!Params->TryGetStringField(TEXT("name"), DisplayName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
	}

	FString ColorStr;
	if (!Params->TryGetStringField(TEXT("color"), ColorStr))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'color' parameter"));
	}

	FLinearColor ParsedColor;
	if (!ParseColorString(ColorStr, ParsedColor))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Invalid color format: '%s'. Use '#RRGGBB', '#RRGGBBAA', or 'R,G,B,A' (floats)"), *ColorStr));
	}

	FString Error;
	UStateTree* Tree = StateTreeHelpers::LoadStateTree(AssetPath, Error);
	if (!Tree)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	UStateTreeEditorData* EditorData = StateTreeHelpers::GetEditorData(Tree, Error);
	if (!EditorData)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	// Search for existing color with this display name
	FStateTreeEditorColor* ExistingColor = nullptr;
	bool bCreatedNew = false;

	for (FStateTreeEditorColor& ColorEntry : EditorData->Colors)
	{
		if (ColorEntry.DisplayName.Equals(DisplayName, ESearchCase::IgnoreCase))
		{
			ExistingColor = &ColorEntry;
			break;
		}
	}

	FGuid ColorID;

	if (ExistingColor)
	{
		// Update existing
		ExistingColor->Color = ParsedColor;
		ColorID = ExistingColor->ColorRef.ID;
	}
	else
	{
		// Create new color entry
		FStateTreeEditorColor NewColor;
		NewColor.DisplayName = DisplayName;
		NewColor.Color = ParsedColor;
		ColorID = NewColor.ColorRef.ID;

		EditorData->Colors.Add(NewColor);
		bCreatedNew = true;
	}

	Tree->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("color_id"), ColorID.ToString());
	Result->SetStringField(TEXT("name"), DisplayName);
	Result->SetStringField(TEXT("color"), ColorStr);
	Result->SetBoolField(TEXT("created_new"), bCreatedNew);
	return Result;
}
