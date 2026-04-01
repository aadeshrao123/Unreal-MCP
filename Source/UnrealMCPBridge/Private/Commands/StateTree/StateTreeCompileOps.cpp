#include "Commands/EpicUnrealMCPStateTreeCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "StateTree/StateTreeHelpers.h"

#include "StateTree.h"
#include "StateTreeEditorData.h"
#include "StateTreeTypes.h"
#include "StateTreeSchema.h"
#include "StateTreeTaskBase.h"
#include "StateTreeEvaluatorBase.h"
#include "StateTreeConditionBase.h"

#include "UObject/UObjectIterator.h"

// ---------------------------------------------------------------------------
// HandleCompileStateTree
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPStateTreeCommands::HandleCompileStateTree(
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

	TArray<FString> Messages;
	bool bCompiled = StateTreeHelpers::CompileTree(Tree, Messages);

	// Count errors and warnings
	int32 ErrorCount = 0;
	int32 WarningCount = 0;
	TArray<TSharedPtr<FJsonValue>> MessageArray;

	for (const FString& Msg : Messages)
	{
		MessageArray.Add(MakeShared<FJsonValueString>(Msg));

		if (Msg.StartsWith(TEXT("[Error]")))
		{
			ErrorCount++;
		}
		else if (Msg.StartsWith(TEXT("[Warning]")))
		{
			WarningCount++;
		}
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetBoolField(TEXT("compiled"), bCompiled);
	Result->SetArrayField(TEXT("messages"), MessageArray);
	Result->SetNumberField(TEXT("error_count"), ErrorCount);
	Result->SetNumberField(TEXT("warning_count"), WarningCount);
	return Result;
}

// ---------------------------------------------------------------------------
// HandleListStateTreeNodeTypes
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPStateTreeCommands::HandleListStateTreeNodeTypes(
	const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));
	}

	FString Category;
	if (!Params->TryGetStringField(TEXT("category"), Category))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'category' parameter (task/evaluator/condition)"));
	}

	FString Filter;
	Params->TryGetStringField(TEXT("filter"), Filter);

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

	UStateTreeSchema* Schema = EditorData->Schema;
	if (!Schema)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("StateTree has no schema set. Set a schema first with set_statetree_schema."));
	}

	// Determine the base struct for the requested category
	UScriptStruct* BaseStruct = nullptr;
	FString CategoryLower = Category.ToLower();

	if (CategoryLower == TEXT("task"))
	{
		BaseStruct = FStateTreeTaskBase::StaticStruct();
	}
	else if (CategoryLower == TEXT("evaluator"))
	{
		BaseStruct = FStateTreeEvaluatorBase::StaticStruct();
	}
	else if (CategoryLower == TEXT("condition"))
	{
		BaseStruct = FStateTreeConditionBase::StaticStruct();
	}
	else
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Invalid category: '%s'. Valid: task, evaluator, condition"), *Category));
	}

	FString FilterLower = Filter.ToLower();

	// Iterate all UScriptStruct subclasses of the base struct
	TArray<TSharedPtr<FJsonValue>> TypesArray;
	for (TObjectIterator<UScriptStruct> It; It; ++It)
	{
		UScriptStruct* Struct = *It;
		if (!Struct || !Struct->IsChildOf(BaseStruct))
		{
			continue;
		}

		// Skip the base struct itself
		if (Struct == BaseStruct)
		{
			continue;
		}

		// Skip abstract/hidden structs
		if (Struct->HasMetaData(TEXT("Hidden")) || Struct->HasMetaData(TEXT("Abstract")))
		{
			continue;
		}

		// Check if schema allows this struct
		if (!Schema->IsStructAllowed(Struct))
		{
			continue;
		}

		FString StructName = Struct->GetName();

		// Apply name filter
		if (!FilterLower.IsEmpty())
		{
			if (!StructName.ToLower().Contains(FilterLower))
			{
				continue;
			}
		}

		TSharedPtr<FJsonObject> TypeObj = MakeShared<FJsonObject>();
		TypeObj->SetStringField(TEXT("class_name"), StructName);
		TypeObj->SetStringField(TEXT("class_path"), Struct->GetPathName());

		// Try to get description from metadata
		FString Description;
		if (Struct->HasMetaData(TEXT("DisplayName")))
		{
			Description = Struct->GetMetaData(TEXT("DisplayName"));
		}
		else if (Struct->HasMetaData(TEXT("ToolTip")))
		{
			Description = Struct->GetMetaData(TEXT("ToolTip"));
		}
		TypeObj->SetStringField(TEXT("description"), Description);

		TypesArray.Add(MakeShared<FJsonValueObject>(TypeObj));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("category"), Category);
	Result->SetStringField(TEXT("schema"), Schema->GetClass()->GetName());
	Result->SetNumberField(TEXT("count"), TypesArray.Num());
	Result->SetArrayField(TEXT("types"), TypesArray);

	if (!Filter.IsEmpty())
	{
		Result->SetStringField(TEXT("filter"), Filter);
	}

	return Result;
}

// ---------------------------------------------------------------------------
// HandleListStateTreeEnumValues
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPStateTreeCommands::HandleListStateTreeEnumValues(
	const TSharedPtr<FJsonObject>& Params)
{
	FString Category;
	if (!Params->TryGetStringField(TEXT("category"), Category))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Missing 'category'. Valid: trigger, priority, state_type, selection_behavior, operand"));
	}

	FString CategoryLower = Category.ToLower();

	auto MakeEntry = [](const FString& Name, const FString& Desc) -> TSharedPtr<FJsonValue>
	{
		auto Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("value"), Name);
		Obj->SetStringField(TEXT("description"), Desc);
		return MakeShared<FJsonValueObject>(Obj);
	};

	TArray<TSharedPtr<FJsonValue>> Values;

	if (CategoryLower == TEXT("trigger"))
	{
		Values.Add(MakeEntry(TEXT("OnStateCompleted"), TEXT("Trigger when state succeeds or fails")));
		Values.Add(MakeEntry(TEXT("OnStateSucceeded"), TEXT("Trigger when state succeeds")));
		Values.Add(MakeEntry(TEXT("OnStateFailed"), TEXT("Trigger when state fails")));
		Values.Add(MakeEntry(TEXT("OnTick"), TEXT("Trigger every tick (condition-gated)")));
		Values.Add(MakeEntry(TEXT("OnEvent"), TEXT("Trigger on gameplay event (requires event_tag)")));
	}
	else if (CategoryLower == TEXT("priority"))
	{
		Values.Add(MakeEntry(TEXT("Low"), TEXT("Low priority — evaluated last")));
		Values.Add(MakeEntry(TEXT("Normal"), TEXT("Normal priority (default)")));
		Values.Add(MakeEntry(TEXT("Medium"), TEXT("Medium priority")));
		Values.Add(MakeEntry(TEXT("High"), TEXT("High priority")));
		Values.Add(MakeEntry(TEXT("Critical"), TEXT("Critical priority — evaluated first")));
	}
	else if (CategoryLower == TEXT("state_type"))
	{
		Values.Add(MakeEntry(TEXT("State"), TEXT("Normal state with tasks and children")));
		Values.Add(MakeEntry(TEXT("Group"), TEXT("Container-only state (no tasks, only children)")));
		Values.Add(MakeEntry(TEXT("Linked"), TEXT("Links to another state in the same tree")));
		Values.Add(MakeEntry(TEXT("LinkedAsset"), TEXT("Links to an external StateTree asset")));
		Values.Add(MakeEntry(TEXT("Subtree"), TEXT("Marked as a subtree that can be linked to")));
	}
	else if (CategoryLower == TEXT("selection_behavior"))
	{
		Values.Add(MakeEntry(TEXT("None"), TEXT("Cannot be directly selected")));
		Values.Add(MakeEntry(TEXT("TryEnterState"), TEXT("Select this state directly")));
		Values.Add(MakeEntry(TEXT("TrySelectChildrenInOrder"), TEXT("Select first valid child in order")));
		Values.Add(MakeEntry(TEXT("TrySelectChildrenAtRandom"), TEXT("Select random child")));
		Values.Add(MakeEntry(TEXT("TrySelectChildrenWithHighestUtility"), TEXT("Select child with highest utility score")));
		Values.Add(MakeEntry(TEXT("TrySelectChildrenAtRandomWeightedByUtility"), TEXT("Weighted random by utility")));
		Values.Add(MakeEntry(TEXT("TryFollowTransitions"), TEXT("Follow transitions instead of selecting")));
	}
	else if (CategoryLower == TEXT("operand"))
	{
		Values.Add(MakeEntry(TEXT("And"), TEXT("Logical AND with previous condition")));
		Values.Add(MakeEntry(TEXT("Or"), TEXT("Logical OR with previous condition")));
	}
	else
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Invalid category: '%s'. Valid: trigger, priority, state_type, selection_behavior, operand"), *Category));
	}

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("category"), Category);
	Result->SetNumberField(TEXT("count"), Values.Num());
	Result->SetArrayField(TEXT("values"), Values);
	return Result;
}
