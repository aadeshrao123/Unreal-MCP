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
#include "StructUtils/PropertyBag.h"

// ---------------------------------------------------------------------------
// Local helpers
// ---------------------------------------------------------------------------

/**
 * Collect all node IDs from a state and its descendants (tasks, conditions,
 * transition conditions, considerations, evaluators referenced by the state).
 * Used to clean up bindings when a state is removed.
 */
static void CollectNodeIDsRecursive(
	const UStateTreeState* State,
	TSet<FGuid>& OutNodeIDs)
{
	if (!State)
	{
		return;
	}

	// Tasks
	for (const FStateTreeEditorNode& Node : State->Tasks)
	{
		OutNodeIDs.Add(Node.ID);
	}

	// SingleTask
	if (State->SingleTask.ID.IsValid())
	{
		OutNodeIDs.Add(State->SingleTask.ID);
	}

	// Enter conditions
	for (const FStateTreeEditorNode& Node : State->EnterConditions)
	{
		OutNodeIDs.Add(Node.ID);
	}

	// Considerations
	for (const FStateTreeEditorNode& Node : State->Considerations)
	{
		OutNodeIDs.Add(Node.ID);
	}

	// Transition conditions
	for (const FStateTreeTransition& Trans : State->Transitions)
	{
		for (const FStateTreeEditorNode& Node : Trans.Conditions)
		{
			OutNodeIDs.Add(Node.ID);
		}
	}

	// Recurse into children
	for (const UStateTreeState* Child : State->Children)
	{
		CollectNodeIDsRecursive(Child, OutNodeIDs);
	}
}

/**
 * Remove all bindings whose source or target struct ID matches any GUID
 * in the provided set.
 */
static void RemoveBindingsForNodeIDs(
	UStateTreeEditorData* EditorData,
	const TSet<FGuid>& NodeIDs)
{
	if (!EditorData || NodeIDs.Num() == 0)
	{
		return;
	}

	EditorData->EditorBindings.RemoveBindings(
		[&NodeIDs](FPropertyBindingBinding& Binding) -> bool
		{
			const FGuid& SourceID = Binding.GetSourcePath().GetStructID();
			const FGuid& TargetID = Binding.GetTargetPath().GetStructID();
			return NodeIDs.Contains(SourceID) || NodeIDs.Contains(TargetID);
		});
}

// ---------------------------------------------------------------------------
// HandleRemoveStateTreeState
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPStateTreeCommands::HandleRemoveStateTreeState(
	const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));
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

	FString RemovedName = State->Name.ToString();
	FString RemovedID = State->ID.ToString();
	FString RemovedPath = StateTreeHelpers::ComputeIndexPath(EditorData, State);

	// Collect node IDs from the state subtree for binding cleanup
	TSet<FGuid> NodeIDs;
	CollectNodeIDsRecursive(State, NodeIDs);

	// Remove the state from its parent or from SubTrees
	bool bRemoved = false;

	if (State->Parent)
	{
		int32 ChildIndex = State->Parent->Children.IndexOfByKey(State);
		if (ChildIndex != INDEX_NONE)
		{
			State->Parent->Children.RemoveAt(ChildIndex);
			bRemoved = true;
		}
	}
	else
	{
		// Root-level state: remove from SubTrees
		int32 SubTreeIndex = EditorData->SubTrees.IndexOfByKey(State);
		if (SubTreeIndex != INDEX_NONE)
		{
			EditorData->SubTrees.RemoveAt(SubTreeIndex);
			bRemoved = true;
		}
	}

	if (!bRemoved)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Failed to remove state '%s' from its container"), *RemovedName));
	}

	// Clean up bindings referencing removed nodes
	RemoveBindingsForNodeIDs(EditorData, NodeIDs);

	Tree->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("removed_state"), RemovedName);
	Result->SetStringField(TEXT("removed_id"), RemovedID);
	Result->SetStringField(TEXT("removed_path"), RemovedPath);
	Result->SetNumberField(TEXT("bindings_cleaned"), NodeIDs.Num());
	return Result;
}

// ---------------------------------------------------------------------------
// HandleRemoveStateTreeNode
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPStateTreeCommands::HandleRemoveStateTreeNode(
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

	if (!ContainingArray || NodeIndex == INDEX_NONE)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Internal error: node found but containing array/index is invalid"));
	}

	// Capture info before removal
	FString NodeStructName = TEXT("Unknown");
	if (EditorNode->Node.IsValid())
	{
		NodeStructName = EditorNode->Node.GetScriptStruct()->GetName();
	}

	// Clean up bindings referencing this node
	TSet<FGuid> NodeIDs;
	NodeIDs.Add(NodeGuid);
	RemoveBindingsForNodeIDs(EditorData, NodeIDs);

	// Remove the node from its array
	ContainingArray->RemoveAt(NodeIndex);

	Tree->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("removed_node_id"), NodeGuidStr);
	Result->SetStringField(TEXT("removed_node_class"), NodeStructName);
	Result->SetStringField(TEXT("context"), Context);
	return Result;
}

// ---------------------------------------------------------------------------
// HandleRemoveStateTreeTransition
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPStateTreeCommands::HandleRemoveStateTreeTransition(
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

	// Collect condition node IDs from the transition for binding cleanup
	TSet<FGuid> NodeIDs;
	const FStateTreeTransition& Trans = State->Transitions[TransitionIndex];
	for (const FStateTreeEditorNode& CondNode : Trans.Conditions)
	{
		NodeIDs.Add(CondNode.ID);
	}

	FString RemovedID = Trans.ID.ToString();

	// Remove bindings for the transition's condition nodes
	RemoveBindingsForNodeIDs(EditorData, NodeIDs);

	State->Transitions.RemoveAt(TransitionIndex);

	Tree->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("state_name"), State->Name.ToString());
	Result->SetNumberField(TEXT("removed_index"), TransitionIndex);
	Result->SetStringField(TEXT("removed_id"), RemovedID);
	return Result;
}

// ---------------------------------------------------------------------------
// HandleRemoveStateTreeBinding
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPStateTreeCommands::HandleRemoveStateTreeBinding(
	const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));
	}

	double BindingIndexD = -1;
	if (!Params->TryGetNumberField(TEXT("binding_index"), BindingIndexD))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'binding_index' parameter"));
	}
	int32 BindingIndex = static_cast<int32>(BindingIndexD);

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

	TArrayView<FStateTreePropertyPathBinding> Bindings = EditorData->EditorBindings.GetMutableBindings();

	if (BindingIndex < 0 || BindingIndex >= Bindings.Num())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Binding index %d out of range (has %d bindings)"),
				BindingIndex, Bindings.Num()));
	}

	// Capture info before removal
	FString SourceID = Bindings[BindingIndex].GetSourcePath().GetStructID().ToString();
	FString TargetID = Bindings[BindingIndex].GetTargetPath().GetStructID().ToString();

	int32 CurrentIndex = 0;
	const int32 TargetIdx = BindingIndex;
	EditorData->EditorBindings.RemoveBindings(
		[&CurrentIndex, TargetIdx](FPropertyBindingBinding& /*Binding*/) -> bool
		{
			bool bShouldRemove = (CurrentIndex == TargetIdx);
			CurrentIndex++;
			return bShouldRemove;
		});

	Tree->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetNumberField(TEXT("removed_index"), BindingIndex);
	Result->SetStringField(TEXT("source_struct_id"), SourceID);
	Result->SetStringField(TEXT("target_struct_id"), TargetID);
	return Result;
}

// ---------------------------------------------------------------------------
// HandleRemoveStateTreeParameter
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPStateTreeCommands::HandleRemoveStateTreeParameter(
	const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));
	}

	FString ParamName;
	if (!Params->TryGetStringField(TEXT("name"), ParamName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
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

	// Access the root parameter property bag.
	// UStateTreeEditorData exposes GetRootParametersPropertyBag() as const, so we need to
	// work through CreateRootProperties or use reflection to get the mutable bag.
	// The mutable bag is the private member RootParameterPropertyBag, accessible via FindPropertyByName.
	FProperty* BagProp = EditorData->GetClass()->FindPropertyByName(TEXT("RootParameterPropertyBag"));
	if (!BagProp)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Cannot access RootParameterPropertyBag on EditorData"));
	}

	FInstancedPropertyBag* PropertyBag = BagProp->ContainerPtrToValuePtr<FInstancedPropertyBag>(EditorData);
	if (!PropertyBag)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("RootParameterPropertyBag is null"));
	}

	// Check if the parameter exists before removing
	const FPropertyBagPropertyDesc* ExistingDesc = PropertyBag->FindPropertyDescByName(FName(*ParamName));
	if (!ExistingDesc)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Parameter '%s' not found in the property bag"), *ParamName));
	}

	EPropertyBagAlterationResult RemoveResult = PropertyBag->RemovePropertyByName(FName(*ParamName));
	if (RemoveResult != EPropertyBagAlterationResult::Success)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Failed to remove parameter '%s' (result: %d)"),
				*ParamName, static_cast<int32>(RemoveResult)));
	}

	Tree->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("removed_parameter"), ParamName);
	return Result;
}
