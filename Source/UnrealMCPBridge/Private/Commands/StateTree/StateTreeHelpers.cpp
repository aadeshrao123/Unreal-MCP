#include "StateTreeHelpers.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"

#include "StateTree.h"
#include "StateTreeEditorData.h"
#include "StateTreeState.h"
#include "StateTreeEditorNode.h"
#include "StateTreeTypes.h"
#include "StateTreeCompiler.h"
#include "StateTreeCompilerLog.h"
#include "StructUtils/InstancedStruct.h"

#include "Logging/TokenizedMessage.h"
#include "UObject/UnrealType.h"

// ---------------------------------------------------------------------------
// Asset Loading
// ---------------------------------------------------------------------------

UStateTree* StateTreeHelpers::LoadStateTree(const FString& AssetPath, FString& OutError)
{
	if (AssetPath.IsEmpty())
	{
		OutError = TEXT("Asset path is empty");
		return nullptr;
	}

	UStateTree* Tree = LoadObject<UStateTree>(nullptr, *AssetPath);
	if (!Tree)
	{
		// Try with explicit object path (e.g. "/Game/AI/ST_Enemy" -> "/Game/AI/ST_Enemy.ST_Enemy")
		FString FullPath = AssetPath;
		if (!FullPath.Contains(TEXT(".")))
		{
			FString AssetName = FPaths::GetBaseFilename(FullPath);
			FullPath = FullPath + TEXT(".") + AssetName;
		}
		Tree = LoadObject<UStateTree>(nullptr, *FullPath);
	}

	if (!Tree)
	{
		OutError = FString::Printf(TEXT("Failed to load StateTree at '%s'"), *AssetPath);
	}
	return Tree;
}

UStateTreeEditorData* StateTreeHelpers::GetEditorData(UStateTree* Tree, FString& OutError)
{
	if (!Tree)
	{
		OutError = TEXT("StateTree is null");
		return nullptr;
	}

	UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(Tree->EditorData);
	if (!EditorData)
	{
		OutError = TEXT("StateTree has no editor data (may not be an editor build)");
	}
	return EditorData;
}

// ---------------------------------------------------------------------------
// State Resolution (tri-modal)
// ---------------------------------------------------------------------------

UStateTreeState* StateTreeHelpers::ResolveState(
	UStateTreeEditorData* EditorData,
	const TSharedPtr<FJsonObject>& Params,
	FString& OutError)
{
	if (!EditorData)
	{
		OutError = TEXT("EditorData is null");
		return nullptr;
	}

	// Priority 1: index path ("state")
	FString IndexPath;
	if (Params->TryGetStringField(TEXT("state"), IndexPath))
	{
		return ResolveStateByPath(EditorData, IndexPath, OutError);
	}

	// Priority 2: name ("state_name")
	FString StateName;
	if (Params->TryGetStringField(TEXT("state_name"), StateName))
	{
		return ResolveStateByName(EditorData, StateName, OutError);
	}

	// Priority 3: GUID ("state_guid")
	FString GuidStr;
	if (Params->TryGetStringField(TEXT("state_guid"), GuidStr))
	{
		FGuid Guid;
		if (!FGuid::Parse(GuidStr, Guid))
		{
			OutError = FString::Printf(TEXT("Invalid GUID format: '%s'"), *GuidStr);
			return nullptr;
		}
		return ResolveStateByGuid(EditorData, Guid, OutError);
	}

	OutError = TEXT("No state identifier provided. Use 'state' (index path), 'state_name', or 'state_guid'");
	return nullptr;
}

UStateTreeState* StateTreeHelpers::ResolveStateByPath(
	UStateTreeEditorData* EditorData,
	const FString& IndexPath,
	FString& OutError)
{
	if (!EditorData)
	{
		OutError = TEXT("EditorData is null");
		return nullptr;
	}

	TArray<FString> Parts;
	IndexPath.ParseIntoArray(Parts, TEXT("/"));

	if (Parts.Num() == 0)
	{
		OutError = TEXT("Index path is empty");
		return nullptr;
	}

	// First index selects from SubTrees
	int32 RootIdx = FCString::Atoi(*Parts[0]);
	if (RootIdx < 0 || RootIdx >= EditorData->SubTrees.Num())
	{
		OutError = FString::Printf(
			TEXT("SubTrees index %d out of range (has %d subtrees)"),
			RootIdx, EditorData->SubTrees.Num());
		return nullptr;
	}

	UStateTreeState* Current = EditorData->SubTrees[RootIdx];
	if (!Current)
	{
		OutError = FString::Printf(TEXT("SubTrees[%d] is null"), RootIdx);
		return nullptr;
	}

	// Remaining indices walk Children
	for (int32 PartIdx = 1; PartIdx < Parts.Num(); ++PartIdx)
	{
		int32 ChildIdx = FCString::Atoi(*Parts[PartIdx]);
		if (ChildIdx < 0 || ChildIdx >= Current->Children.Num())
		{
			OutError = FString::Printf(
				TEXT("Children index %d out of range at '%s' (state '%s' has %d children)"),
				ChildIdx,
				*IndexPath.Left(IndexPath.Find(Parts[PartIdx])),
				*Current->Name.ToString(),
				Current->Children.Num());
			return nullptr;
		}

		Current = Current->Children[ChildIdx];
		if (!Current)
		{
			OutError = FString::Printf(TEXT("Child state at index %d is null"), ChildIdx);
			return nullptr;
		}
	}

	return Current;
}

// ---------------------------------------------------------------------------
// Helper: recursive name search
// ---------------------------------------------------------------------------

namespace StateTreeHelpersPrivate
{
	static void FindStatesByNameRecursive(
		UStateTreeState* State,
		const FString& Name,
		TArray<UStateTreeState*>& OutMatches)
	{
		if (!State)
		{
			return;
		}

		if (State->Name.ToString().Equals(Name, ESearchCase::IgnoreCase))
		{
			OutMatches.Add(State);
		}

		for (UStateTreeState* Child : State->Children)
		{
			FindStatesByNameRecursive(Child, Name, OutMatches);
		}
	}
}

UStateTreeState* StateTreeHelpers::ResolveStateByName(
	UStateTreeEditorData* EditorData,
	const FString& Name,
	FString& OutError)
{
	if (!EditorData)
	{
		OutError = TEXT("EditorData is null");
		return nullptr;
	}

	TArray<UStateTreeState*> Matches;
	for (UStateTreeState* SubTree : EditorData->SubTrees)
	{
		StateTreeHelpersPrivate::FindStatesByNameRecursive(SubTree, Name, Matches);
	}

	if (Matches.Num() == 0)
	{
		OutError = FString::Printf(TEXT("No state found with name '%s'"), *Name);
		return nullptr;
	}

	if (Matches.Num() > 1)
	{
		OutError = FString::Printf(
			TEXT("Ambiguous: %d states match name '%s'. Use 'state' (index path) or 'state_guid' instead"),
			Matches.Num(), *Name);
		return nullptr;
	}

	return Matches[0];
}

// ---------------------------------------------------------------------------
// Helper: recursive GUID search
// ---------------------------------------------------------------------------

namespace StateTreeHelpersPrivate
{
	static UStateTreeState* FindStateByGuidRecursive(UStateTreeState* State, const FGuid& Guid)
	{
		if (!State)
		{
			return nullptr;
		}

		if (State->ID == Guid)
		{
			return State;
		}

		for (UStateTreeState* Child : State->Children)
		{
			UStateTreeState* Found = FindStateByGuidRecursive(Child, Guid);
			if (Found)
			{
				return Found;
			}
		}

		return nullptr;
	}
}

UStateTreeState* StateTreeHelpers::ResolveStateByGuid(
	UStateTreeEditorData* EditorData,
	const FGuid& Guid,
	FString& OutError)
{
	if (!EditorData)
	{
		OutError = TEXT("EditorData is null");
		return nullptr;
	}

	for (UStateTreeState* SubTree : EditorData->SubTrees)
	{
		UStateTreeState* Found = StateTreeHelpersPrivate::FindStateByGuidRecursive(SubTree, Guid);
		if (Found)
		{
			return Found;
		}
	}

	OutError = FString::Printf(TEXT("No state found with GUID '%s'"), *Guid.ToString());
	return nullptr;
}

// ---------------------------------------------------------------------------
// Index Path Computation
// ---------------------------------------------------------------------------

FString StateTreeHelpers::ComputeIndexPath(UStateTreeEditorData* EditorData, const UStateTreeState* State)
{
	if (!EditorData || !State)
	{
		return TEXT("");
	}

	// Build path from leaf to root
	TArray<int32> Indices;
	const UStateTreeState* Current = State;

	while (Current)
	{
		const UStateTreeState* ParentState = Current->Parent;

		if (ParentState)
		{
			// Find index in parent's Children array
			int32 ChildIndex = ParentState->Children.IndexOfByKey(Current);
			if (ChildIndex == INDEX_NONE)
			{
				return TEXT("?");
			}
			Indices.Add(ChildIndex);
		}
		else
		{
			// Root level: find index in SubTrees
			int32 SubTreeIndex = EditorData->SubTrees.IndexOfByKey(Current);
			if (SubTreeIndex == INDEX_NONE)
			{
				return TEXT("?");
			}
			Indices.Add(SubTreeIndex);
		}

		Current = ParentState;
	}

	// Reverse to get root-to-leaf order
	Algo::Reverse(Indices);

	FString Path;
	for (int32 i = 0; i < Indices.Num(); ++i)
	{
		if (i > 0)
		{
			Path += TEXT("/");
		}
		Path += FString::FromInt(Indices[i]);
	}

	return Path;
}

// ---------------------------------------------------------------------------
// Node Resolution
// ---------------------------------------------------------------------------

FStateTreeEditorNode* StateTreeHelpers::ResolveNodeByGuid(
	UStateTreeEditorData* EditorData,
	const FGuid& NodeGuid,
	FString& OutContext,
	TArray<FStateTreeEditorNode>*& OutArray,
	int32& OutIndex,
	FString& OutError)
{
	if (!EditorData)
	{
		OutError = TEXT("EditorData is null");
		return nullptr;
	}

	// Search evaluators
	for (int32 i = 0; i < EditorData->Evaluators.Num(); ++i)
	{
		if (EditorData->Evaluators[i].ID == NodeGuid)
		{
			OutContext = TEXT("evaluator");
			OutArray = &EditorData->Evaluators;
			OutIndex = i;
			return &EditorData->Evaluators[i];
		}
	}

	// Search global tasks
	for (int32 i = 0; i < EditorData->GlobalTasks.Num(); ++i)
	{
		if (EditorData->GlobalTasks[i].ID == NodeGuid)
		{
			OutContext = TEXT("global_task");
			OutArray = &EditorData->GlobalTasks;
			OutIndex = i;
			return &EditorData->GlobalTasks[i];
		}
	}

	// Recursive search through all states
	TFunction<FStateTreeEditorNode*(UStateTreeState*)> SearchState;
	SearchState = [&](UStateTreeState* State) -> FStateTreeEditorNode*
	{
		if (!State)
		{
			return nullptr;
		}

		FString StateName = State->Name.ToString();

		// Search tasks
		for (int32 i = 0; i < State->Tasks.Num(); ++i)
		{
			if (State->Tasks[i].ID == NodeGuid)
			{
				OutContext = FString::Printf(TEXT("task:%s"), *StateName);
				OutArray = &State->Tasks;
				OutIndex = i;
				return &State->Tasks[i];
			}
		}

		// Search enter conditions
		for (int32 i = 0; i < State->EnterConditions.Num(); ++i)
		{
			if (State->EnterConditions[i].ID == NodeGuid)
			{
				OutContext = FString::Printf(TEXT("enter_condition:%s"), *StateName);
				OutArray = &State->EnterConditions;
				OutIndex = i;
				return &State->EnterConditions[i];
			}
		}

		// Search transition conditions
		for (int32 TransIdx = 0; TransIdx < State->Transitions.Num(); ++TransIdx)
		{
			TArray<FStateTreeEditorNode>& Conditions = State->Transitions[TransIdx].Conditions;
			for (int32 i = 0; i < Conditions.Num(); ++i)
			{
				if (Conditions[i].ID == NodeGuid)
				{
					OutContext = FString::Printf(TEXT("transition_condition:%s"), *StateName);
					OutArray = &Conditions;
					OutIndex = i;
					return &Conditions[i];
				}
			}
		}

		// Search considerations
		for (int32 i = 0; i < State->Considerations.Num(); ++i)
		{
			if (State->Considerations[i].ID == NodeGuid)
			{
				OutContext = FString::Printf(TEXT("consideration:%s"), *StateName);
				OutArray = &State->Considerations;
				OutIndex = i;
				return &State->Considerations[i];
			}
		}

		// Recurse into children
		for (UStateTreeState* Child : State->Children)
		{
			FStateTreeEditorNode* Found = SearchState(Child);
			if (Found)
			{
				return Found;
			}
		}

		return nullptr;
	};

	for (UStateTreeState* SubTree : EditorData->SubTrees)
	{
		FStateTreeEditorNode* Found = SearchState(SubTree);
		if (Found)
		{
			return Found;
		}
	}

	OutError = FString::Printf(TEXT("No node found with GUID '%s'"), *NodeGuid.ToString());
	OutArray = nullptr;
	OutIndex = INDEX_NONE;
	return nullptr;
}

// ---------------------------------------------------------------------------
// JSON Serialization
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> StateTreeHelpers::SerializeInstancedStructProperties(const FInstancedStruct& Struct)
{
	auto PropsObj = MakeShared<FJsonObject>();

	if (!Struct.IsValid())
	{
		return PropsObj;
	}

	const UScriptStruct* ScriptStruct = Struct.GetScriptStruct();
	const uint8* StructMemory = Struct.GetMemory();

	if (!ScriptStruct || !StructMemory)
	{
		return PropsObj;
	}

	for (TFieldIterator<FProperty> PropIt(ScriptStruct); PropIt; ++PropIt)
	{
		const FProperty* Prop = *PropIt;
		if (!Prop)
		{
			continue;
		}

		FString ValueStr;
		const void* PropAddr = Prop->ContainerPtrToValuePtr<void>(StructMemory);
		Prop->ExportTextItem_Direct(ValueStr, PropAddr, nullptr, nullptr, PPF_None);

		PropsObj->SetStringField(Prop->GetName(), ValueStr);
	}

	return PropsObj;
}

TSharedPtr<FJsonObject> StateTreeHelpers::EditorNodeToJson(const FStateTreeEditorNode& Node, int32 Index)
{
	auto Obj = MakeShared<FJsonObject>();

	Obj->SetNumberField(TEXT("index"), Index);
	Obj->SetStringField(TEXT("id"), Node.ID.ToString());
	Obj->SetStringField(TEXT("operand"), ExpressionOperandToString(static_cast<uint8>(Node.ExpressionOperand)));
	Obj->SetNumberField(TEXT("indent"), Node.ExpressionIndent);

	// Node struct info
	if (Node.Node.IsValid())
	{
		const UScriptStruct* NodeStruct = Node.Node.GetScriptStruct();
		if (NodeStruct)
		{
			Obj->SetStringField(TEXT("node_class"), NodeStruct->GetName());
		}

		Obj->SetObjectField(TEXT("properties"), SerializeInstancedStructProperties(Node.Node));
	}
	else
	{
		Obj->SetStringField(TEXT("node_class"), TEXT("None"));
	}

	// Instance data
	if (Node.Instance.IsValid())
	{
		const UScriptStruct* InstanceStruct = Node.Instance.GetScriptStruct();
		if (InstanceStruct)
		{
			Obj->SetStringField(TEXT("instance_class"), InstanceStruct->GetName());
		}

		Obj->SetObjectField(TEXT("instance_properties"), SerializeInstancedStructProperties(Node.Instance));
	}

	// Instance object (Blueprint-based nodes)
	if (Node.InstanceObject)
	{
		Obj->SetStringField(TEXT("instance_object_class"), Node.InstanceObject->GetClass()->GetName());
	}

	return Obj;
}

TSharedPtr<FJsonObject> StateTreeHelpers::TransitionToJson(const FStateTreeTransition& Transition, int32 Index)
{
	auto Obj = MakeShared<FJsonObject>();

	Obj->SetNumberField(TEXT("index"), Index);
	Obj->SetStringField(TEXT("id"), Transition.ID.ToString());
	Obj->SetStringField(TEXT("trigger"), TransitionTriggerToString(static_cast<uint8>(Transition.Trigger)));
	Obj->SetStringField(TEXT("priority"), TransitionPriorityToString(static_cast<uint8>(Transition.Priority)));
	Obj->SetBoolField(TEXT("enabled"), Transition.bTransitionEnabled);
	Obj->SetBoolField(TEXT("delay_enabled"), Transition.bDelayTransition);
	Obj->SetNumberField(TEXT("delay_duration"), Transition.DelayDuration);
	Obj->SetNumberField(TEXT("delay_variance"), Transition.DelayRandomVariance);
	Obj->SetNumberField(TEXT("condition_count"), Transition.Conditions.Num());

	// Target state info
#if WITH_EDITORONLY_DATA
	auto TargetObj = MakeShared<FJsonObject>();
	TargetObj->SetStringField(TEXT("name"), Transition.State.Name.ToString());
	TargetObj->SetStringField(TEXT("id"), Transition.State.ID.ToString());

	FString LinkTypeStr;
	switch (Transition.State.LinkType)
	{
	case EStateTreeTransitionType::None:
		LinkTypeStr = TEXT("None");
		break;
	case EStateTreeTransitionType::Succeeded:
		LinkTypeStr = TEXT("Succeeded");
		break;
	case EStateTreeTransitionType::Failed:
		LinkTypeStr = TEXT("Failed");
		break;
	case EStateTreeTransitionType::GotoState:
		LinkTypeStr = TEXT("GotoState");
		break;
	case EStateTreeTransitionType::NextState:
		LinkTypeStr = TEXT("NextState");
		break;
	case EStateTreeTransitionType::NextSelectableState:
		LinkTypeStr = TEXT("NextSelectableState");
		break;
	default:
		LinkTypeStr = TEXT("Unknown");
		break;
	}
	TargetObj->SetStringField(TEXT("link_type"), LinkTypeStr);
	Obj->SetObjectField(TEXT("target_state"), TargetObj);
#endif

	// Serialize conditions
	if (Transition.Conditions.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> ConditionsArray;
		for (int32 i = 0; i < Transition.Conditions.Num(); ++i)
		{
			ConditionsArray.Add(MakeShared<FJsonValueObject>(EditorNodeToJson(Transition.Conditions[i], i)));
		}
		Obj->SetArrayField(TEXT("conditions"), ConditionsArray);
	}

	return Obj;
}

TSharedPtr<FJsonObject> StateTreeHelpers::StateToJsonSummary(
	UStateTreeEditorData* EditorData,
	UStateTreeState* State)
{
	auto Obj = MakeShared<FJsonObject>();

	if (!State)
	{
		return Obj;
	}

	Obj->SetStringField(TEXT("name"), State->Name.ToString());
	Obj->SetStringField(TEXT("type"), StateTypeToString(static_cast<uint8>(State->Type)));
	Obj->SetStringField(TEXT("id"), State->ID.ToString());
	Obj->SetStringField(TEXT("index_path"), ComputeIndexPath(EditorData, State));
	Obj->SetStringField(TEXT("selection_behavior"), SelectionBehaviorToString(static_cast<uint8>(State->SelectionBehavior)));
	Obj->SetBoolField(TEXT("enabled"), State->bEnabled);
	Obj->SetNumberField(TEXT("task_count"), State->Tasks.Num());
	Obj->SetNumberField(TEXT("transition_count"), State->Transitions.Num());
	Obj->SetNumberField(TEXT("enter_condition_count"), State->EnterConditions.Num());
	Obj->SetNumberField(TEXT("consideration_count"), State->Considerations.Num());
	Obj->SetNumberField(TEXT("child_count"), State->Children.Num());

	// Color ref
	if (State->ColorRef.ID.IsValid())
	{
		Obj->SetStringField(TEXT("color_ref"), State->ColorRef.ID.ToString());
	}

	return Obj;
}

TSharedPtr<FJsonObject> StateTreeHelpers::StateToJsonDetailed(
	UStateTreeEditorData* EditorData,
	UStateTreeState* State)
{
	// Start with the summary
	TSharedPtr<FJsonObject> Obj = StateToJsonSummary(EditorData, State);

	if (!State)
	{
		return Obj;
	}

	// Description
	if (!State->Description.IsEmpty())
	{
		Obj->SetStringField(TEXT("description"), State->Description);
	}

	// Tag
	if (State->Tag.IsValid())
	{
		Obj->SetStringField(TEXT("tag"), State->Tag.ToString());
	}

	// Tasks
	if (State->Tasks.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> TasksArray;
		for (int32 i = 0; i < State->Tasks.Num(); ++i)
		{
			TasksArray.Add(MakeShared<FJsonValueObject>(EditorNodeToJson(State->Tasks[i], i)));
		}
		Obj->SetArrayField(TEXT("tasks"), TasksArray);
	}

	// Single task (some schemas use single task mode)
	if (State->SingleTask.Node.IsValid())
	{
		Obj->SetObjectField(TEXT("single_task"), EditorNodeToJson(State->SingleTask, 0));
	}

	// Enter conditions
	if (State->EnterConditions.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> ConditionsArray;
		for (int32 i = 0; i < State->EnterConditions.Num(); ++i)
		{
			ConditionsArray.Add(MakeShared<FJsonValueObject>(EditorNodeToJson(State->EnterConditions[i], i)));
		}
		Obj->SetArrayField(TEXT("enter_conditions"), ConditionsArray);
	}

	// Transitions
	if (State->Transitions.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> TransitionsArray;
		for (int32 i = 0; i < State->Transitions.Num(); ++i)
		{
			TransitionsArray.Add(MakeShared<FJsonValueObject>(TransitionToJson(State->Transitions[i], i)));
		}
		Obj->SetArrayField(TEXT("transitions"), TransitionsArray);
	}

	// Considerations
	if (State->Considerations.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> ConsiderationsArray;
		for (int32 i = 0; i < State->Considerations.Num(); ++i)
		{
			ConsiderationsArray.Add(MakeShared<FJsonValueObject>(EditorNodeToJson(State->Considerations[i], i)));
		}
		Obj->SetArrayField(TEXT("considerations"), ConsiderationsArray);
	}

	// Linked subtree
#if WITH_EDITORONLY_DATA
	if (State->Type == EStateTreeStateType::Linked && State->LinkedSubtree.ID.IsValid())
	{
		auto LinkedObj = MakeShared<FJsonObject>();
		LinkedObj->SetStringField(TEXT("name"), State->LinkedSubtree.Name.ToString());
		LinkedObj->SetStringField(TEXT("id"), State->LinkedSubtree.ID.ToString());
		Obj->SetObjectField(TEXT("linked_subtree"), LinkedObj);
	}
#endif

	// Linked asset
	if (State->Type == EStateTreeStateType::LinkedAsset && State->LinkedAsset)
	{
		Obj->SetStringField(TEXT("linked_asset"), State->LinkedAsset->GetPathName());
	}

	// Children summary
	if (State->Children.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> ChildrenArray;
		for (UStateTreeState* Child : State->Children)
		{
			ChildrenArray.Add(MakeShared<FJsonValueObject>(StateToJsonSummary(EditorData, Child)));
		}
		Obj->SetArrayField(TEXT("children"), ChildrenArray);
	}

	return Obj;
}

// ---------------------------------------------------------------------------
// Enum Conversion
// ---------------------------------------------------------------------------

FString StateTreeHelpers::StateTypeToString(uint8 Type)
{
	switch (static_cast<EStateTreeStateType>(Type))
	{
	case EStateTreeStateType::State:
		return TEXT("State");
	case EStateTreeStateType::Group:
		return TEXT("Group");
	case EStateTreeStateType::Linked:
		return TEXT("Linked");
	case EStateTreeStateType::LinkedAsset:
		return TEXT("LinkedAsset");
	case EStateTreeStateType::Subtree:
		return TEXT("Subtree");
	default:
		return TEXT("Unknown");
	}
}

FString StateTreeHelpers::SelectionBehaviorToString(uint8 Behavior)
{
	switch (static_cast<EStateTreeStateSelectionBehavior>(Behavior))
	{
	case EStateTreeStateSelectionBehavior::None:
		return TEXT("None");
	case EStateTreeStateSelectionBehavior::TryEnterState:
		return TEXT("TryEnterState");
	case EStateTreeStateSelectionBehavior::TrySelectChildrenInOrder:
		return TEXT("TrySelectChildrenInOrder");
	case EStateTreeStateSelectionBehavior::TrySelectChildrenAtRandom:
		return TEXT("TrySelectChildrenAtRandom");
	case EStateTreeStateSelectionBehavior::TrySelectChildrenWithHighestUtility:
		return TEXT("TrySelectChildrenWithHighestUtility");
	case EStateTreeStateSelectionBehavior::TrySelectChildrenAtRandomWeightedByUtility:
		return TEXT("TrySelectChildrenAtRandomWeightedByUtility");
	case EStateTreeStateSelectionBehavior::TryFollowTransitions:
		return TEXT("TryFollowTransitions");
	default:
		return TEXT("Unknown");
	}
}

FString StateTreeHelpers::TransitionTriggerToString(uint8 Trigger)
{
	EStateTreeTransitionTrigger TriggerEnum = static_cast<EStateTreeTransitionTrigger>(Trigger);

	// Handle the flag-based enum
	if (TriggerEnum == EStateTreeTransitionTrigger::None)
	{
		return TEXT("None");
	}
	if (TriggerEnum == EStateTreeTransitionTrigger::OnStateCompleted)
	{
		return TEXT("OnStateCompleted");
	}
	if (TriggerEnum == EStateTreeTransitionTrigger::OnStateSucceeded)
	{
		return TEXT("OnStateSucceeded");
	}
	if (TriggerEnum == EStateTreeTransitionTrigger::OnStateFailed)
	{
		return TEXT("OnStateFailed");
	}
	if (TriggerEnum == EStateTreeTransitionTrigger::OnTick)
	{
		return TEXT("OnTick");
	}
	if (TriggerEnum == EStateTreeTransitionTrigger::OnEvent)
	{
		return TEXT("OnEvent");
	}

	// Check for the OnDelegate flag (0x10)
	if (EnumHasAnyFlags(TriggerEnum, static_cast<EStateTreeTransitionTrigger>(0x10)))
	{
		return TEXT("OnDelegate");
	}

	return FString::Printf(TEXT("Combined(0x%02X)"), Trigger);
}

FString StateTreeHelpers::TransitionPriorityToString(uint8 Priority)
{
	switch (static_cast<EStateTreeTransitionPriority>(Priority))
	{
	case EStateTreeTransitionPriority::None:
		return TEXT("None");
	case EStateTreeTransitionPriority::Low:
		return TEXT("Low");
	case EStateTreeTransitionPriority::Normal:
		return TEXT("Normal");
	case EStateTreeTransitionPriority::Medium:
		return TEXT("Medium");
	case EStateTreeTransitionPriority::High:
		return TEXT("High");
	case EStateTreeTransitionPriority::Critical:
		return TEXT("Critical");
	default:
		return TEXT("Unknown");
	}
}

FString StateTreeHelpers::ExpressionOperandToString(uint8 Operand)
{
	switch (static_cast<EStateTreeExpressionOperand>(Operand))
	{
	case EStateTreeExpressionOperand::Copy:
		return TEXT("Copy");
	case EStateTreeExpressionOperand::And:
		return TEXT("And");
	case EStateTreeExpressionOperand::Or:
		return TEXT("Or");
	case EStateTreeExpressionOperand::Multiply:
		return TEXT("Multiply");
	default:
		return TEXT("Unknown");
	}
}

// ---------------------------------------------------------------------------
// Compilation
// ---------------------------------------------------------------------------

bool StateTreeHelpers::CompileTree(UStateTree* Tree, TArray<FString>& OutMessages)
{
	if (!Tree)
	{
		OutMessages.Add(TEXT("StateTree is null"));
		return false;
	}

	FStateTreeCompilerLog CompilerLog;
	FStateTreeCompiler Compiler(CompilerLog);
	bool bSuccess = Compiler.Compile(Tree);

	// Extract messages from the tokenized message log
	TArray<TSharedRef<FTokenizedMessage>> TokenizedMessages = CompilerLog.ToTokenizedMessages();
	for (const TSharedRef<FTokenizedMessage>& Msg : TokenizedMessages)
	{
		FString SeverityStr;
		switch (Msg->GetSeverity())
		{
		case EMessageSeverity::Error:
			SeverityStr = TEXT("Error");
			break;
		case EMessageSeverity::PerformanceWarning:
		case EMessageSeverity::Warning:
			SeverityStr = TEXT("Warning");
			break;
		case EMessageSeverity::Info:
			SeverityStr = TEXT("Info");
			break;
		default:
			SeverityStr = TEXT("Note");
			break;
		}

		OutMessages.Add(FString::Printf(TEXT("[%s] %s"), *SeverityStr, *Msg->ToText().ToString()));
	}

	if (bSuccess)
	{
		Tree->MarkPackageDirty();
	}

	return bSuccess;
}
