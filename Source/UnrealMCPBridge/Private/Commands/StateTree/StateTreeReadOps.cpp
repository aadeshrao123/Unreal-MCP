#include "Commands/EpicUnrealMCPStateTreeCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
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
// HandleGetStateTreeInfo
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPStateTreeCommands::HandleGetStateTreeInfo(
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

	// Recursively count all states
	int32 TotalStateCount = 0;
	TFunction<void(const UStateTreeState*)> CountStates = [&](const UStateTreeState* State)
	{
		if (!State)
		{
			return;
		}
		TotalStateCount++;
		for (const UStateTreeState* Child : State->Children)
		{
			CountStates(Child);
		}
	};

	for (const UStateTreeState* SubTree : EditorData->SubTrees)
	{
		CountStates(SubTree);
	}

	// Schema class name
	FString SchemaClassName;
	if (EditorData->Schema)
	{
		SchemaClassName = EditorData->Schema->GetClass()->GetName();
	}

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("asset_path"), AssetPath);
	Result->SetStringField(TEXT("schema_class"), SchemaClassName);
	Result->SetNumberField(TEXT("state_count"), TotalStateCount);
	Result->SetNumberField(TEXT("evaluator_count"), EditorData->Evaluators.Num());
	Result->SetNumberField(TEXT("global_task_count"), EditorData->GlobalTasks.Num());
	Result->SetNumberField(TEXT("color_count"), EditorData->Colors.Num());
	Result->SetNumberField(TEXT("root_state_count"), EditorData->SubTrees.Num());

	// Parameter count from the root parameter property bag
	int32 ParameterCount = 0;
	const FInstancedPropertyBag& RootParams = EditorData->GetRootParametersPropertyBag();
	const UPropertyBag* BagStruct = RootParams.GetPropertyBagStruct();
	if (BagStruct)
	{
		ParameterCount = BagStruct->GetPropertyDescs().Num();
	}
	Result->SetNumberField(TEXT("parameter_count"), ParameterCount);

	return Result;
}

// ---------------------------------------------------------------------------
// HandleGetStateTreeStates
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPStateTreeCommands::HandleGetStateTreeStates(
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

	// Optional parameters
	int32 MaxDepth = -1;
	if (Params->HasField(TEXT("max_depth")))
	{
		MaxDepth = static_cast<int32>(Params->GetNumberField(TEXT("max_depth")));
	}

	FString NameFilter;
	Params->TryGetStringField(TEXT("name_filter"), NameFilter);

	// Check if a state or any of its descendants match the name filter
	TFunction<bool(const UStateTreeState*)> MatchesFilter = [&](const UStateTreeState* State) -> bool
	{
		if (!State)
		{
			return false;
		}

		if (State->Name.ToString().Contains(NameFilter, ESearchCase::IgnoreCase))
		{
			return true;
		}

		for (const UStateTreeState* Child : State->Children)
		{
			if (MatchesFilter(Child))
			{
				return true;
			}
		}

		return false;
	};

	// Recursive builder
	TFunction<TSharedPtr<FJsonObject>(UStateTreeState*, int32)> BuildStateJson =
		[&](UStateTreeState* State, int32 CurrentDepth) -> TSharedPtr<FJsonObject>
	{
		if (!State)
		{
			return nullptr;
		}

		auto StateJson = StateTreeHelpers::StateToJsonSummary(EditorData, State);
		if (!StateJson.IsValid())
		{
			return nullptr;
		}

		// Add children if depth allows
		TArray<TSharedPtr<FJsonValue>> ChildrenArray;
		bool bCanRecurse = (MaxDepth < 0) || (CurrentDepth < MaxDepth);

		if (bCanRecurse)
		{
			for (UStateTreeState* Child : State->Children)
			{
				if (!Child)
				{
					continue;
				}

				// Apply name filter if set
				if (!NameFilter.IsEmpty() && !MatchesFilter(Child))
				{
					continue;
				}

				TSharedPtr<FJsonObject> ChildJson = BuildStateJson(Child, CurrentDepth + 1);
				if (ChildJson.IsValid())
				{
					ChildrenArray.Add(MakeShared<FJsonValueObject>(ChildJson));
				}
			}
		}

		StateJson->SetArrayField(TEXT("children"), ChildrenArray);
		return StateJson;
	};

	// Build root-level array
	TArray<TSharedPtr<FJsonValue>> StatesArray;
	for (UStateTreeState* SubTree : EditorData->SubTrees)
	{
		if (!SubTree)
		{
			continue;
		}

		// Apply name filter at root level too
		if (!NameFilter.IsEmpty() && !MatchesFilter(SubTree))
		{
			continue;
		}

		TSharedPtr<FJsonObject> StateJson = BuildStateJson(SubTree, 0);
		if (StateJson.IsValid())
		{
			StatesArray.Add(MakeShared<FJsonValueObject>(StateJson));
		}
	}

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("asset_path"), AssetPath);
	Result->SetArrayField(TEXT("states"), StatesArray);
	Result->SetNumberField(TEXT("count"), StatesArray.Num());
	return Result;
}

// ---------------------------------------------------------------------------
// HandleGetStateTreeState
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPStateTreeCommands::HandleGetStateTreeState(
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

	TSharedPtr<FJsonObject> StateJson = StateTreeHelpers::StateToJsonDetailed(EditorData, State);
	if (!StateJson.IsValid())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to serialize state"));
	}

	StateJson->SetBoolField(TEXT("success"), true);
	StateJson->SetStringField(TEXT("asset_path"), AssetPath);
	return StateJson;
}

// ---------------------------------------------------------------------------
// HandleGetStateTreeEvaluators
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPStateTreeCommands::HandleGetStateTreeEvaluators(
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

	FString Filter;
	Params->TryGetStringField(TEXT("filter"), Filter);

	TArray<TSharedPtr<FJsonValue>> EvaluatorsArray;
	for (int32 i = 0; i < EditorData->Evaluators.Num(); ++i)
	{
		const FStateTreeEditorNode& Node = EditorData->Evaluators[i];

		// Apply class name filter if provided
		if (!Filter.IsEmpty())
		{
			FString NodeClassName;
			if (Node.Node.IsValid())
			{
				NodeClassName = Node.Node.GetScriptStruct()->GetName();
			}

			if (!NodeClassName.Contains(Filter, ESearchCase::IgnoreCase))
			{
				continue;
			}
		}

		TSharedPtr<FJsonObject> NodeJson = StateTreeHelpers::EditorNodeToJson(Node, i);
		if (NodeJson.IsValid())
		{
			EvaluatorsArray.Add(MakeShared<FJsonValueObject>(NodeJson));
		}
	}

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("asset_path"), AssetPath);
	Result->SetArrayField(TEXT("evaluators"), EvaluatorsArray);
	Result->SetNumberField(TEXT("count"), EvaluatorsArray.Num());
	return Result;
}

// ---------------------------------------------------------------------------
// HandleGetStateTreeGlobalTasks
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPStateTreeCommands::HandleGetStateTreeGlobalTasks(
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

	FString Filter;
	Params->TryGetStringField(TEXT("filter"), Filter);

	TArray<TSharedPtr<FJsonValue>> TasksArray;
	for (int32 i = 0; i < EditorData->GlobalTasks.Num(); ++i)
	{
		const FStateTreeEditorNode& Node = EditorData->GlobalTasks[i];

		// Apply class name filter if provided
		if (!Filter.IsEmpty())
		{
			FString NodeClassName;
			if (Node.Node.IsValid())
			{
				NodeClassName = Node.Node.GetScriptStruct()->GetName();
			}

			if (!NodeClassName.Contains(Filter, ESearchCase::IgnoreCase))
			{
				continue;
			}
		}

		TSharedPtr<FJsonObject> NodeJson = StateTreeHelpers::EditorNodeToJson(Node, i);
		if (NodeJson.IsValid())
		{
			TasksArray.Add(MakeShared<FJsonValueObject>(NodeJson));
		}
	}

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("asset_path"), AssetPath);
	Result->SetArrayField(TEXT("global_tasks"), TasksArray);
	Result->SetNumberField(TEXT("count"), TasksArray.Num());
	return Result;
}

// ---------------------------------------------------------------------------
// HandleGetStateTreeParameters
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPStateTreeCommands::HandleGetStateTreeParameters(
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

	const FInstancedPropertyBag& RootParams = EditorData->GetRootParametersPropertyBag();
	const UPropertyBag* BagStruct = RootParams.GetPropertyBagStruct();

	auto ParametersJson = MakeShared<FJsonObject>();
	int32 ParameterCount = 0;

	if (BagStruct)
	{
		TConstArrayView<FPropertyBagPropertyDesc> Descs = BagStruct->GetPropertyDescs();
		FConstStructView BagValue = RootParams.GetValue();

		for (const FPropertyBagPropertyDesc& Desc : Descs)
		{
			auto ParamJson = MakeShared<FJsonObject>();

			// Type name from the enum
			FString TypeName;
			const UEnum* TypeEnum = StaticEnum<EPropertyBagPropertyType>();
			if (TypeEnum)
			{
				TypeName = TypeEnum->GetNameStringByValue(static_cast<int64>(Desc.ValueType));
			}
			ParamJson->SetStringField(TEXT("type"), TypeName);

			// If there is a value type object (enum, struct, class), include its name
			if (Desc.ValueTypeObject != nullptr)
			{
				ParamJson->SetStringField(TEXT("value_type_object"), Desc.ValueTypeObject->GetPathName());
			}

			// Serialize current value as string
			FString ValueString;
			if (BagValue.IsValid())
			{
				TValueOrError<FString, EPropertyBagResult> SerializedValue =
					RootParams.GetValueSerializedString(Desc.Name);
				if (SerializedValue.IsValid())
				{
					ValueString = SerializedValue.GetValue();
				}
			}
			ParamJson->SetStringField(TEXT("value"), ValueString);

			// Property GUID
			ParamJson->SetStringField(TEXT("id"), Desc.ID.ToString());

			ParametersJson->SetObjectField(Desc.Name.ToString(), ParamJson);
			ParameterCount++;
		}
	}

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("asset_path"), AssetPath);
	Result->SetObjectField(TEXT("parameters"), ParametersJson);
	Result->SetNumberField(TEXT("count"), ParameterCount);
	return Result;
}

// ---------------------------------------------------------------------------
// HandleGetStateTreeNode
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPStateTreeCommands::HandleGetStateTreeNode(
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
			FString::Printf(TEXT("Invalid GUID format: %s"), *NodeGuidStr));
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
	TArray<FStateTreeEditorNode>* OutArray = nullptr;
	int32 OutIndex = INDEX_NONE;

	FStateTreeEditorNode* FoundNode = StateTreeHelpers::ResolveNodeByGuid(
		EditorData, NodeGuid, Context, OutArray, OutIndex, Error);

	if (!FoundNode)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	TSharedPtr<FJsonObject> NodeJson = StateTreeHelpers::EditorNodeToJson(*FoundNode, OutIndex);
	if (!NodeJson.IsValid())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to serialize node"));
	}

	NodeJson->SetBoolField(TEXT("success"), true);
	NodeJson->SetStringField(TEXT("asset_path"), AssetPath);
	NodeJson->SetStringField(TEXT("context"), Context);
	return NodeJson;
}

// ---------------------------------------------------------------------------
// HandleGetStateTreeBindings
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPStateTreeCommands::HandleGetStateTreeBindings(
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

	// Optional node_guid filter
	FGuid FilterGuid;
	bool bHasFilter = false;
	FString FilterGuidStr;
	if (Params->TryGetStringField(TEXT("node_guid"), FilterGuidStr))
	{
		if (!FGuid::Parse(FilterGuidStr, FilterGuid))
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Invalid GUID format: %s"), *FilterGuidStr));
		}
		bHasFilter = true;
	}

	const FStateTreeEditorPropertyBindings* EditorBindings = EditorData->GetPropertyEditorBindings();
	if (!EditorBindings)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor bindings available"));
	}

	TConstArrayView<FStateTreePropertyPathBinding> Bindings = EditorBindings->GetBindings();

	TArray<TSharedPtr<FJsonValue>> BindingsArray;
	for (int32 i = 0; i < Bindings.Num(); ++i)
	{
		const FStateTreePropertyPathBinding& Binding = Bindings[i];
		const FPropertyBindingPath& SourcePath = Binding.GetSourcePath();
		const FPropertyBindingPath& TargetPath = Binding.GetTargetPath();

		if (bHasFilter)
		{
			if (SourcePath.GetStructID() != FilterGuid && TargetPath.GetStructID() != FilterGuid)
			{
				continue;
			}
		}

		auto BindingJson = MakeShared<FJsonObject>();
		BindingJson->SetNumberField(TEXT("index"), i);
		BindingJson->SetStringField(TEXT("source_struct_id"), SourcePath.GetStructID().ToString());
		BindingJson->SetStringField(TEXT("target_struct_id"), TargetPath.GetStructID().ToString());

		BindingsArray.Add(MakeShared<FJsonValueObject>(BindingJson));
	}

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("asset_path"), AssetPath);
	Result->SetArrayField(TEXT("bindings"), BindingsArray);
	Result->SetNumberField(TEXT("count"), BindingsArray.Num());
	return Result;
}

// ---------------------------------------------------------------------------
// HandleGetStateTreeTransitionTargets
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPStateTreeCommands::HandleGetStateTreeTransitionTargets(
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

	// Collect all states with their name, GUID, and index path
	TArray<TSharedPtr<FJsonValue>> StatesArray;

	TFunction<void(UStateTreeState*, const FString&)> CollectStates =
		[&](UStateTreeState* State, const FString& ParentPath)
	{
		if (!State) return;

		FString IndexPath = ParentPath;
		if (!ParentPath.IsEmpty())
		{
			IndexPath += TEXT("/");
		}
		// Compute this state's child index within its parent
		int32 SiblingIndex = 0;
		if (State->Parent)
		{
			SiblingIndex = State->Parent->Children.Find(State);
			if (SiblingIndex == INDEX_NONE) SiblingIndex = 0;
		}
		IndexPath += FString::FromInt(SiblingIndex);

		auto EntryJson = MakeShared<FJsonObject>();
		EntryJson->SetStringField(TEXT("name"), State->Name.ToString());
		EntryJson->SetStringField(TEXT("guid"), State->ID.ToString());
		EntryJson->SetStringField(TEXT("index_path"), IndexPath);
		EntryJson->SetStringField(TEXT("type"), StateTreeHelpers::StateTypeToString(static_cast<uint8>(State->Type)));
		StatesArray.Add(MakeShared<FJsonValueObject>(EntryJson));

		for (UStateTreeState* Child : State->Children)
		{
			CollectStates(Child, IndexPath);
		}
	};

	for (UStateTreeState* SubTree : EditorData->SubTrees)
	{
		CollectStates(SubTree, TEXT(""));
	}

	// Meta-transition targets (built-in, always available)
	TArray<TSharedPtr<FJsonValue>> MetaArray;

	auto AddMeta = [&](const FString& Name, const FString& Description)
	{
		auto Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("name"), Name);
		Entry->SetStringField(TEXT("description"), Description);
		MetaArray.Add(MakeShared<FJsonValueObject>(Entry));
	};

	AddMeta(TEXT("None"), TEXT("No transition target (disabled)"));
	AddMeta(TEXT("TreeSucceeded"), TEXT("Transition when the tree succeeds"));
	AddMeta(TEXT("TreeFailed"), TEXT("Transition when the tree fails"));
	AddMeta(TEXT("TreeStopped"), TEXT("Transition when the tree is stopped"));
	AddMeta(TEXT("NextSibling"), TEXT("Transition to the next sibling state"));
	AddMeta(TEXT("NextState"), TEXT("Transition to the next state in order"));
	AddMeta(TEXT("NextSelectableState"), TEXT("Transition to the next state that can be selected"));

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("asset_path"), AssetPath);
	Result->SetArrayField(TEXT("states"), StatesArray);
	Result->SetArrayField(TEXT("meta_targets"), MetaArray);
	Result->SetNumberField(TEXT("state_count"), StatesArray.Num());
	Result->SetNumberField(TEXT("meta_count"), MetaArray.Num());
	return Result;
}
