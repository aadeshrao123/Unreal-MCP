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
	Result->SetStringField(TEXT("global_tasks_completion"),
		StateTreeHelpers::TaskCompletionTypeToString(static_cast<uint8>(EditorData->GlobalTasksCompletion)));
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

	// Binding count
	int32 BindingCount = 0;
	const FStateTreeEditorPropertyBindings* EditorBindings = EditorData->GetPropertyEditorBindings();
	if (EditorBindings)
	{
		BindingCount = EditorBindings->GetBindings().Num();
	}
	Result->SetNumberField(TEXT("binding_count"), BindingCount);

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

		BindingsArray.Add(MakeShared<FJsonValueObject>(
			StateTreeHelpers::BindingToJson(Binding, i, EditorData)));
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

// ---------------------------------------------------------------------------
// HandleGetStateTreeFullInfo
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPStateTreeCommands::HandleGetStateTreeFullInfo(
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

	// Verbosity: "summary" = counts only, "standard" = states+nodes, "full" = everything + bindings inline
	FString Verbosity = TEXT("standard");
	Params->TryGetStringField(TEXT("verbosity"), Verbosity);

	// Optional section filter: comma-separated list of sections to include
	// Valid sections: states, evaluators, global_tasks, parameters, bindings
	// Empty = all sections
	FString SectionFilter;
	Params->TryGetStringField(TEXT("sections"), SectionFilter);

	TSet<FString> RequestedSections;
	if (!SectionFilter.IsEmpty())
	{
		TArray<FString> Parts;
		SectionFilter.ParseIntoArray(Parts, TEXT(","));
		for (FString& Part : Parts)
		{
			Part.TrimStartAndEndInline();
			RequestedSections.Add(Part.ToLower());
		}
	}

	auto WantSection = [&](const FString& Section) -> bool
	{
		return RequestedSections.Num() == 0 || RequestedSections.Contains(Section);
	};

	bool bIncludeBindings = Verbosity.Equals(TEXT("full"), ESearchCase::IgnoreCase);
	bool bSummaryOnly = Verbosity.Equals(TEXT("summary"), ESearchCase::IgnoreCase);

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("asset_path"), AssetPath);
	Result->SetStringField(TEXT("verbosity"), Verbosity);

	// Schema
	FString SchemaClassName;
	if (EditorData->Schema)
	{
		SchemaClassName = EditorData->Schema->GetClass()->GetName();
	}
	Result->SetStringField(TEXT("schema_class"), SchemaClassName);

	// Global tasks completion
	Result->SetStringField(TEXT("global_tasks_completion"),
		StateTreeHelpers::TaskCompletionTypeToString(static_cast<uint8>(EditorData->GlobalTasksCompletion)));

	// Counts (always included)
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

	Result->SetNumberField(TEXT("state_count"), TotalStateCount);
	Result->SetNumberField(TEXT("evaluator_count"), EditorData->Evaluators.Num());
	Result->SetNumberField(TEXT("global_task_count"), EditorData->GlobalTasks.Num());
	Result->SetNumberField(TEXT("root_state_count"), EditorData->SubTrees.Num());

	int32 BindingCount = 0;
	const FStateTreeEditorPropertyBindings* EditorBindings = EditorData->GetPropertyEditorBindings();
	if (EditorBindings)
	{
		BindingCount = EditorBindings->GetBindings().Num();
	}
	Result->SetNumberField(TEXT("binding_count"), BindingCount);

	// Parameter count
	int32 ParameterCount = 0;
	const FInstancedPropertyBag& RootParams = EditorData->GetRootParametersPropertyBag();
	const UPropertyBag* BagStruct = RootParams.GetPropertyBagStruct();
	if (BagStruct)
	{
		ParameterCount = BagStruct->GetPropertyDescs().Num();
	}
	Result->SetNumberField(TEXT("parameter_count"), ParameterCount);

	if (bSummaryOnly)
	{
		return Result;
	}

	// ---- Parameters ----
	if (WantSection(TEXT("parameters")) && BagStruct)
	{
		auto ParametersJson = MakeShared<FJsonObject>();
		TConstArrayView<FPropertyBagPropertyDesc> Descs = BagStruct->GetPropertyDescs();
		FConstStructView BagValue = RootParams.GetValue();

		for (const FPropertyBagPropertyDesc& Desc : Descs)
		{
			auto ParamJson = MakeShared<FJsonObject>();

			FString TypeName;
			const UEnum* TypeEnum = StaticEnum<EPropertyBagPropertyType>();
			if (TypeEnum)
			{
				TypeName = TypeEnum->GetNameStringByValue(static_cast<int64>(Desc.ValueType));
			}
			ParamJson->SetStringField(TEXT("type"), TypeName);

			if (Desc.ValueTypeObject != nullptr)
			{
				ParamJson->SetStringField(TEXT("value_type_object"), Desc.ValueTypeObject->GetPathName());
			}

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
			ParamJson->SetStringField(TEXT("id"), Desc.ID.ToString());

			ParametersJson->SetObjectField(Desc.Name.ToString(), ParamJson);
		}

		Result->SetObjectField(TEXT("parameters"), ParametersJson);
	}

	// ---- Evaluators ----
	if (WantSection(TEXT("evaluators")) && EditorData->Evaluators.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> EvaluatorsArray;
		for (int32 i = 0; i < EditorData->Evaluators.Num(); ++i)
		{
			if (bIncludeBindings)
			{
				EvaluatorsArray.Add(MakeShared<FJsonValueObject>(
					StateTreeHelpers::EditorNodeToJsonWithBindings(EditorData->Evaluators[i], i, EditorData)));
			}
			else
			{
				EvaluatorsArray.Add(MakeShared<FJsonValueObject>(
					StateTreeHelpers::EditorNodeToJson(EditorData->Evaluators[i], i)));
			}
		}
		Result->SetArrayField(TEXT("evaluators"), EvaluatorsArray);
	}

	// ---- Global Tasks ----
	if (WantSection(TEXT("global_tasks")) && EditorData->GlobalTasks.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> GlobalTasksArray;
		for (int32 i = 0; i < EditorData->GlobalTasks.Num(); ++i)
		{
			if (bIncludeBindings)
			{
				GlobalTasksArray.Add(MakeShared<FJsonValueObject>(
					StateTreeHelpers::EditorNodeToJsonWithBindings(EditorData->GlobalTasks[i], i, EditorData)));
			}
			else
			{
				GlobalTasksArray.Add(MakeShared<FJsonValueObject>(
					StateTreeHelpers::EditorNodeToJson(EditorData->GlobalTasks[i], i)));
			}
		}
		Result->SetArrayField(TEXT("global_tasks"), GlobalTasksArray);
	}

	// ---- States (recursive, fully detailed) ----
	if (WantSection(TEXT("states")))
	{
		TFunction<TSharedPtr<FJsonObject>(UStateTreeState*)> BuildStateJson =
			[&](UStateTreeState* State) -> TSharedPtr<FJsonObject>
		{
			if (!State)
			{
				return nullptr;
			}

			TSharedPtr<FJsonObject> StateJson = StateTreeHelpers::StateToJsonDetailed(
				EditorData, State, bIncludeBindings);

			// Replace children summary with full recursive children
			if (State->Children.Num() > 0)
			{
				TArray<TSharedPtr<FJsonValue>> ChildrenArray;
				for (UStateTreeState* Child : State->Children)
				{
					TSharedPtr<FJsonObject> ChildJson = BuildStateJson(Child);
					if (ChildJson.IsValid())
					{
						ChildrenArray.Add(MakeShared<FJsonValueObject>(ChildJson));
					}
				}
				StateJson->SetArrayField(TEXT("children"), ChildrenArray);
			}

			return StateJson;
		};

		TArray<TSharedPtr<FJsonValue>> StatesArray;
		for (UStateTreeState* SubTree : EditorData->SubTrees)
		{
			TSharedPtr<FJsonObject> StateJson = BuildStateJson(SubTree);
			if (StateJson.IsValid())
			{
				StatesArray.Add(MakeShared<FJsonValueObject>(StateJson));
			}
		}
		Result->SetArrayField(TEXT("states"), StatesArray);
	}

	// ---- Bindings (standalone section) ----
	if (WantSection(TEXT("bindings")) && EditorBindings)
	{
		TConstArrayView<FStateTreePropertyPathBinding> Bindings = EditorBindings->GetBindings();
		if (Bindings.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> BindingsArray;
			for (int32 i = 0; i < Bindings.Num(); ++i)
			{
				BindingsArray.Add(MakeShared<FJsonValueObject>(
					StateTreeHelpers::BindingToJson(Bindings[i], i, EditorData)));
			}
			Result->SetArrayField(TEXT("bindings"), BindingsArray);
		}
	}

	return Result;
}

// ---------------------------------------------------------------------------
// HandleSearchStateTreeNodes
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPStateTreeCommands::HandleSearchStateTreeNodes(
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

	// Filter: class name substring
	FString ClassFilter;
	Params->TryGetStringField(TEXT("class_filter"), ClassFilter);

	// Category filter: "task", "evaluator", "condition", "consideration", or empty for all
	FString CategoryFilter;
	Params->TryGetStringField(TEXT("category"), CategoryFilter);
	FString CategoryLower = CategoryFilter.ToLower();

	// Helper: check if a node matches filters
	auto NodeMatchesFilter = [&](const FStateTreeEditorNode& Node) -> bool
	{
		if (!Node.Node.IsValid())
		{
			return false;
		}

		if (!ClassFilter.IsEmpty())
		{
			FString ClassName = Node.Node.GetScriptStruct()->GetName();
			if (!ClassName.Contains(ClassFilter, ESearchCase::IgnoreCase))
			{
				return false;
			}
		}

		return true;
	};

	// Helper: serialize a search result
	auto MakeResult = [&](const FStateTreeEditorNode& Node, int32 NodeIndex,
		const FString& Location, const FString& StateName) -> TSharedPtr<FJsonObject>
	{
		auto ResultObj = StateTreeHelpers::EditorNodeToJson(Node, NodeIndex);
		ResultObj->SetStringField(TEXT("location"), Location);

		if (!StateName.IsEmpty())
		{
			ResultObj->SetStringField(TEXT("state_name"), StateName);
		}

		return ResultObj;
	};

	TArray<TSharedPtr<FJsonValue>> Results;

	// Search evaluators
	if (CategoryLower.IsEmpty() || CategoryLower == TEXT("evaluator"))
	{
		for (int32 i = 0; i < EditorData->Evaluators.Num(); ++i)
		{
			if (NodeMatchesFilter(EditorData->Evaluators[i]))
			{
				Results.Add(MakeShared<FJsonValueObject>(
					MakeResult(EditorData->Evaluators[i], i, TEXT("evaluator"), TEXT(""))));
			}
		}
	}

	// Search global tasks
	if (CategoryLower.IsEmpty() || CategoryLower == TEXT("global_task") || CategoryLower == TEXT("task"))
	{
		for (int32 i = 0; i < EditorData->GlobalTasks.Num(); ++i)
		{
			if (NodeMatchesFilter(EditorData->GlobalTasks[i]))
			{
				Results.Add(MakeShared<FJsonValueObject>(
					MakeResult(EditorData->GlobalTasks[i], i, TEXT("global_task"), TEXT(""))));
			}
		}
	}

	// Recursive state search
	TFunction<void(UStateTreeState*)> SearchState = [&](UStateTreeState* State)
	{
		if (!State)
		{
			return;
		}

		FString StateName = State->Name.ToString();

		// Tasks
		if (CategoryLower.IsEmpty() || CategoryLower == TEXT("task"))
		{
			for (int32 i = 0; i < State->Tasks.Num(); ++i)
			{
				if (NodeMatchesFilter(State->Tasks[i]))
				{
					Results.Add(MakeShared<FJsonValueObject>(
						MakeResult(State->Tasks[i], i, TEXT("task"), StateName)));
				}
			}

			if (State->SingleTask.Node.IsValid() && NodeMatchesFilter(State->SingleTask))
			{
				Results.Add(MakeShared<FJsonValueObject>(
					MakeResult(State->SingleTask, 0, TEXT("single_task"), StateName)));
			}
		}

		// Enter conditions
		if (CategoryLower.IsEmpty() || CategoryLower == TEXT("condition"))
		{
			for (int32 i = 0; i < State->EnterConditions.Num(); ++i)
			{
				if (NodeMatchesFilter(State->EnterConditions[i]))
				{
					Results.Add(MakeShared<FJsonValueObject>(
						MakeResult(State->EnterConditions[i], i, TEXT("enter_condition"), StateName)));
				}
			}
		}

		// Transition conditions
		if (CategoryLower.IsEmpty() || CategoryLower == TEXT("condition"))
		{
			for (int32 TransIdx = 0; TransIdx < State->Transitions.Num(); ++TransIdx)
			{
				for (int32 i = 0; i < State->Transitions[TransIdx].Conditions.Num(); ++i)
				{
					if (NodeMatchesFilter(State->Transitions[TransIdx].Conditions[i]))
					{
						FString Location = FString::Printf(
							TEXT("transition_condition[%d]"), TransIdx);
						Results.Add(MakeShared<FJsonValueObject>(
							MakeResult(State->Transitions[TransIdx].Conditions[i], i, Location, StateName)));
					}
				}
			}
		}

		// Considerations
		if (CategoryLower.IsEmpty() || CategoryLower == TEXT("consideration"))
		{
			for (int32 i = 0; i < State->Considerations.Num(); ++i)
			{
				if (NodeMatchesFilter(State->Considerations[i]))
				{
					Results.Add(MakeShared<FJsonValueObject>(
						MakeResult(State->Considerations[i], i, TEXT("consideration"), StateName)));
				}
			}
		}

		// Recurse into children
		for (UStateTreeState* Child : State->Children)
		{
			SearchState(Child);
		}
	};

	for (UStateTreeState* SubTree : EditorData->SubTrees)
	{
		SearchState(SubTree);
	}

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("asset_path"), AssetPath);
	Result->SetArrayField(TEXT("results"), Results);
	Result->SetNumberField(TEXT("count"), Results.Num());

	if (!ClassFilter.IsEmpty())
	{
		Result->SetStringField(TEXT("class_filter"), ClassFilter);
	}
	if (!CategoryFilter.IsEmpty())
	{
		Result->SetStringField(TEXT("category"), CategoryFilter);
	}

	return Result;
}
