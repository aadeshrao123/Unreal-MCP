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
#include "StateTreeTaskBase.h"
#include "StateTreeEvaluatorBase.h"
#include "StateTreeConditionBase.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/PropertyBag.h"
#include "PropertyBindingTypes.h"
#include "GameplayTagContainer.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Factories/DataAssetFactory.h"
#include "UObject/SavePackage.h"
#include "EditorAssetLibrary.h"
#include "UObject/UObjectIterator.h"

using PU = FEpicUnrealMCPPropertyUtils;

// ---------------------------------------------------------------------------
// Local helpers
// ---------------------------------------------------------------------------

/**
 * Find a UScriptStruct by short name or full path, constrained to a base struct.
 * Supports names with or without the 'F' prefix.
 */
static UScriptStruct* FindNodeStruct(
	const FString& ClassName,
	UScriptStruct* BaseStruct,
	FString& OutError)
{
	if (ClassName.IsEmpty())
	{
		OutError = TEXT("Empty struct class name");
		return nullptr;
	}

	// Try full path first
	UScriptStruct* Found = FindObject<UScriptStruct>(nullptr, *ClassName);
	if (Found && Found->IsChildOf(BaseStruct))
	{
		return Found;
	}

	// Search by short name across all loaded structs
	for (TObjectIterator<UScriptStruct> It; It; ++It)
	{
		UScriptStruct* Struct = *It;
		if (!Struct || !Struct->IsChildOf(BaseStruct))
		{
			continue;
		}

		const FString ShortName = Struct->GetName();
		if (ShortName == ClassName ||
			ShortName == (TEXT("F") + ClassName) ||
			(ClassName.StartsWith(TEXT("F")) && ShortName == ClassName.RightChop(1)))
		{
			return Struct;
		}
	}

	OutError = FString::Printf(
		TEXT("No struct matching '%s' found (base: %s)"),
		*ClassName, *BaseStruct->GetName());
	return nullptr;
}

/**
 * Create a fully-initialized FStateTreeEditorNode from a UScriptStruct.
 * Sets up Node, Instance, ExecutionRuntimeData, and generates a new ID.
 */
static void InitEditorNode(FStateTreeEditorNode& OutNode, UScriptStruct* NodeStruct)
{
	OutNode.Node.InitializeAs(NodeStruct);
	OutNode.ID = FGuid::NewGuid();

	const FStateTreeNodeBase* NodeBase = OutNode.Node.GetPtr<FStateTreeNodeBase>();
	if (NodeBase)
	{
		if (const UScriptStruct* InstanceType = Cast<UScriptStruct>(NodeBase->GetInstanceDataType()))
		{
			OutNode.Instance.InitializeAs(InstanceType);
		}
		if (const UScriptStruct* RuntimeType = Cast<UScriptStruct>(NodeBase->GetExecutionRuntimeDataType()))
		{
			OutNode.ExecutionRuntimeData.InitializeAs(RuntimeType);
		}
	}
}

/**
 * Apply a JSON object's keys to an FInstancedStruct via ImportText reflection.
 * Returns a list of property names that failed.
 */
static TArray<FString> ApplyPropertiesToInstancedStruct(
	FInstancedStruct& Struct,
	const TSharedPtr<FJsonObject>& PropsJson)
{
	TArray<FString> Failures;
	if (!PropsJson.IsValid() || !Struct.IsValid())
	{
		return Failures;
	}

	UScriptStruct* ScriptStruct = const_cast<UScriptStruct*>(Struct.GetScriptStruct());
	uint8* StructData = Struct.GetMutableMemory();
	if (!ScriptStruct || !StructData)
	{
		return Failures;
	}

	for (const auto& Pair : PropsJson->Values)
	{
		FProperty* Prop = ScriptStruct->FindPropertyByName(FName(*Pair.Key));
		if (!Prop)
		{
			Failures.Add(FString::Printf(TEXT("%s: property not found on %s"), *Pair.Key, *ScriptStruct->GetName()));
			continue;
		}

		// Convert JSON value to string for ImportText
		FString ValueStr;
		if (Pair.Value->Type == EJson::String)
		{
			ValueStr = Pair.Value->AsString();
		}
		else if (Pair.Value->Type == EJson::Number)
		{
			ValueStr = FString::SanitizeFloat(Pair.Value->AsNumber());
		}
		else if (Pair.Value->Type == EJson::Boolean)
		{
			ValueStr = Pair.Value->AsBool() ? TEXT("true") : TEXT("false");
		}
		else
		{
			// For objects/arrays, serialize to JSON string for ImportText
			TSharedRef<FJsonValue> ValRef = Pair.Value.ToSharedRef();
			FString JsonStr;
			TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonStr);
			FJsonSerializer::Serialize(ValRef, TEXT(""), Writer);
			ValueStr = JsonStr;
		}

		void* PropertyAddress = Prop->ContainerPtrToValuePtr<void>(StructData);
		const TCHAR* Result = Prop->ImportText_Direct(*ValueStr, PropertyAddress, nullptr, PPF_None);
		if (!Result)
		{
			Failures.Add(FString::Printf(TEXT("%s: ImportText failed for value '%s'"), *Pair.Key, *ValueStr));
		}
	}

	return Failures;
}

/**
 * Serialize an FStateTreeEditorNode to a compact JSON summary for return values.
 */
static TSharedPtr<FJsonObject> NodeToJsonResult(const FStateTreeEditorNode& Node, int32 Index)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetNumberField(TEXT("index"), Index);
	Obj->SetStringField(TEXT("id"), Node.ID.ToString());

	if (Node.Node.IsValid())
	{
		Obj->SetStringField(TEXT("struct"), Node.Node.GetScriptStruct()->GetName());
	}

	if (Node.Instance.IsValid())
	{
		Obj->SetStringField(TEXT("instance_struct"), Node.Instance.GetScriptStruct()->GetName());
	}

	return Obj;
}

/**
 * Resolve a UStateTreeSchema subclass by short name or full path.
 */
static UClass* ResolveSchemaClass(const FString& SchemaName, FString& OutError)
{
	if (SchemaName.IsEmpty())
	{
		OutError = TEXT("Empty schema_class");
		return nullptr;
	}

	// Try full path
	UClass* Found = FindObject<UClass>(nullptr, *SchemaName);
	if (Found && Found->IsChildOf(UStateTreeSchema::StaticClass()))
	{
		return Found;
	}

	// Search by short name
	TArray<UClass*> Derived;
	GetDerivedClasses(UStateTreeSchema::StaticClass(), Derived, true);
	for (UClass* C : Derived)
	{
		if (!C || C->HasAnyClassFlags(CLASS_Abstract))
		{
			continue;
		}

		const FString Name = C->GetName();
		if (Name == SchemaName ||
			Name == (TEXT("U") + SchemaName) ||
			(SchemaName.StartsWith(TEXT("U")) && Name == SchemaName.RightChop(1)))
		{
			return C;
		}
	}

	OutError = FString::Printf(TEXT("Schema class not found: '%s'"), *SchemaName);
	return nullptr;
}

/**
 * Parse an EStateTreeStateType from a string.
 */
static bool ParseStateType(const FString& Str, EStateTreeStateType& OutType)
{
	if (Str.IsEmpty() || Str.Equals(TEXT("State"), ESearchCase::IgnoreCase))
	{
		OutType = EStateTreeStateType::State;
		return true;
	}
	if (Str.Equals(TEXT("Group"), ESearchCase::IgnoreCase))
	{
		OutType = EStateTreeStateType::Group;
		return true;
	}
	if (Str.Equals(TEXT("Linked"), ESearchCase::IgnoreCase))
	{
		OutType = EStateTreeStateType::Linked;
		return true;
	}
	if (Str.Equals(TEXT("LinkedAsset"), ESearchCase::IgnoreCase))
	{
		OutType = EStateTreeStateType::LinkedAsset;
		return true;
	}
	if (Str.Equals(TEXT("Subtree"), ESearchCase::IgnoreCase))
	{
		OutType = EStateTreeStateType::Subtree;
		return true;
	}
	return false;
}

/**
 * Parse an EStateTreeStateSelectionBehavior from a string.
 */
static bool ParseSelectionBehavior(const FString& Str, EStateTreeStateSelectionBehavior& OutBehavior)
{
	if (Str.Equals(TEXT("None"), ESearchCase::IgnoreCase))
	{
		OutBehavior = EStateTreeStateSelectionBehavior::None;
		return true;
	}
	if (Str.Equals(TEXT("TryEnterState"), ESearchCase::IgnoreCase) ||
		Str.Equals(TEXT("TryEnter"), ESearchCase::IgnoreCase))
	{
		OutBehavior = EStateTreeStateSelectionBehavior::TryEnterState;
		return true;
	}
	if (Str.Equals(TEXT("TrySelectChildrenInOrder"), ESearchCase::IgnoreCase))
	{
		OutBehavior = EStateTreeStateSelectionBehavior::TrySelectChildrenInOrder;
		return true;
	}
	if (Str.Equals(TEXT("TrySelectChildrenAtRandom"), ESearchCase::IgnoreCase))
	{
		OutBehavior = EStateTreeStateSelectionBehavior::TrySelectChildrenAtRandom;
		return true;
	}
	if (Str.Equals(TEXT("TrySelectChildrenWithHighestUtility"), ESearchCase::IgnoreCase))
	{
		OutBehavior = EStateTreeStateSelectionBehavior::TrySelectChildrenWithHighestUtility;
		return true;
	}
	if (Str.Equals(TEXT("TrySelectChildrenAtRandomWeightedByUtility"), ESearchCase::IgnoreCase))
	{
		OutBehavior = EStateTreeStateSelectionBehavior::TrySelectChildrenAtRandomWeightedByUtility;
		return true;
	}
	return false;
}

/**
 * Parse an EStateTreeTransitionTrigger from a string.
 */
static bool ParseTransitionTrigger(const FString& Str, EStateTreeTransitionTrigger& OutTrigger)
{
	if (Str.Equals(TEXT("OnStateCompleted"), ESearchCase::IgnoreCase))
	{
		OutTrigger = EStateTreeTransitionTrigger::OnStateCompleted;
		return true;
	}
	if (Str.Equals(TEXT("OnStateSucceeded"), ESearchCase::IgnoreCase))
	{
		OutTrigger = EStateTreeTransitionTrigger::OnStateSucceeded;
		return true;
	}
	if (Str.Equals(TEXT("OnStateFailed"), ESearchCase::IgnoreCase))
	{
		OutTrigger = EStateTreeTransitionTrigger::OnStateFailed;
		return true;
	}
	if (Str.Equals(TEXT("OnTick"), ESearchCase::IgnoreCase))
	{
		OutTrigger = EStateTreeTransitionTrigger::OnTick;
		return true;
	}
	if (Str.Equals(TEXT("OnEvent"), ESearchCase::IgnoreCase))
	{
		OutTrigger = EStateTreeTransitionTrigger::OnEvent;
		return true;
	}
	return false;
}

/**
 * Parse an EStateTreeTransitionPriority from a string.
 */
static bool ParseTransitionPriority(const FString& Str, EStateTreeTransitionPriority& OutPriority)
{
	if (Str.IsEmpty() || Str.Equals(TEXT("Normal"), ESearchCase::IgnoreCase))
	{
		OutPriority = EStateTreeTransitionPriority::Normal;
		return true;
	}
	if (Str.Equals(TEXT("Low"), ESearchCase::IgnoreCase))
	{
		OutPriority = EStateTreeTransitionPriority::Low;
		return true;
	}
	if (Str.Equals(TEXT("Medium"), ESearchCase::IgnoreCase))
	{
		OutPriority = EStateTreeTransitionPriority::Medium;
		return true;
	}
	if (Str.Equals(TEXT("High"), ESearchCase::IgnoreCase))
	{
		OutPriority = EStateTreeTransitionPriority::High;
		return true;
	}
	if (Str.Equals(TEXT("Critical"), ESearchCase::IgnoreCase))
	{
		OutPriority = EStateTreeTransitionPriority::Critical;
		return true;
	}
	return false;
}

/**
 * Parse an EPropertyBagPropertyType from a user-facing type string.
 * Returns None on unknown types. Also outputs a UScriptStruct* for struct types.
 */
static EPropertyBagPropertyType ParsePropertyBagType(
	const FString& TypeStr,
	const UObject*& OutValueTypeObject)
{
	OutValueTypeObject = nullptr;

	if (TypeStr.Equals(TEXT("bool"), ESearchCase::IgnoreCase))
	{
		return EPropertyBagPropertyType::Bool;
	}
	if (TypeStr.Equals(TEXT("byte"), ESearchCase::IgnoreCase) ||
		TypeStr.Equals(TEXT("uint8"), ESearchCase::IgnoreCase))
	{
		return EPropertyBagPropertyType::Byte;
	}
	if (TypeStr.Equals(TEXT("int32"), ESearchCase::IgnoreCase) ||
		TypeStr.Equals(TEXT("int"), ESearchCase::IgnoreCase))
	{
		return EPropertyBagPropertyType::Int32;
	}
	if (TypeStr.Equals(TEXT("int64"), ESearchCase::IgnoreCase))
	{
		return EPropertyBagPropertyType::Int64;
	}
	if (TypeStr.Equals(TEXT("float"), ESearchCase::IgnoreCase))
	{
		return EPropertyBagPropertyType::Float;
	}
	if (TypeStr.Equals(TEXT("double"), ESearchCase::IgnoreCase))
	{
		return EPropertyBagPropertyType::Double;
	}
	if (TypeStr.Equals(TEXT("FName"), ESearchCase::IgnoreCase) ||
		TypeStr.Equals(TEXT("Name"), ESearchCase::IgnoreCase))
	{
		return EPropertyBagPropertyType::Name;
	}
	if (TypeStr.Equals(TEXT("FString"), ESearchCase::IgnoreCase) ||
		TypeStr.Equals(TEXT("String"), ESearchCase::IgnoreCase))
	{
		return EPropertyBagPropertyType::String;
	}
	if (TypeStr.Equals(TEXT("FText"), ESearchCase::IgnoreCase) ||
		TypeStr.Equals(TEXT("Text"), ESearchCase::IgnoreCase))
	{
		return EPropertyBagPropertyType::Text;
	}

	// Try to resolve as a struct type (e.g. "FVector", "FRotator", "FGameplayTag")
	FString StructName = TypeStr;
	if (!StructName.StartsWith(TEXT("F")))
	{
		StructName = TEXT("F") + StructName;
	}

	for (TObjectIterator<UScriptStruct> It; It; ++It)
	{
		UScriptStruct* Struct = *It;
		if (Struct && Struct->GetName() == StructName)
		{
			OutValueTypeObject = Struct;
			return EPropertyBagPropertyType::Struct;
		}
	}

	// Also try without the F prefix if user already passed it
	for (TObjectIterator<UScriptStruct> It; It; ++It)
	{
		UScriptStruct* Struct = *It;
		if (Struct && Struct->GetName() == TypeStr)
		{
			OutValueTypeObject = Struct;
			return EPropertyBagPropertyType::Struct;
		}
	}

	return EPropertyBagPropertyType::None;
}

// ---------------------------------------------------------------------------
// Handler 1: HandleCreateStateTree
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPStateTreeCommands::HandleCreateStateTree(
	const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path'"));
	}

	FString SchemaClassName;
	if (!Params->TryGetStringField(TEXT("schema_class"), SchemaClassName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'schema_class'"));
	}

	// Parse package path and asset name
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

	// Resolve schema class
	FString SchemaError;
	UClass* SchemaClass = ResolveSchemaClass(SchemaClassName, SchemaError);
	if (!SchemaClass)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(SchemaError);
	}

	// Create the UStateTree asset via IAssetTools
	UDataAssetFactory* Factory = NewObject<UDataAssetFactory>();
	Factory->DataAssetClass = UStateTree::StaticClass();

	IAssetTools& AssetTools =
		FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();

	UObject* NewAsset = AssetTools.CreateAsset(AssetName, PackagePath, UStateTree::StaticClass(), Factory);
	if (!NewAsset)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Failed to create StateTree asset '%s'"), *FullPath));
	}

	UStateTree* StateTree = Cast<UStateTree>(NewAsset);
	if (!StateTree)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Created asset is not a UStateTree"));
	}

	// Create and assign editor data with schema
	UStateTreeEditorData* EditorData = NewObject<UStateTreeEditorData>(StateTree, TEXT("EditorData"), RF_Transactional);
	EditorData->Schema = NewObject<UStateTreeSchema>(EditorData, SchemaClass, NAME_None, RF_Transactional);

#if WITH_EDITORONLY_DATA
	StateTree->EditorData = EditorData;
#endif

	StateTree->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("asset_path"), FullPath);
	Result->SetStringField(TEXT("schema"), SchemaClass->GetName());
	return Result;
}

// ---------------------------------------------------------------------------
// Handler 2: HandleAddStateTreeState
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPStateTreeCommands::HandleAddStateTreeState(
	const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path'"));
	}

	FString StateName;
	if (!Params->TryGetStringField(TEXT("name"), StateName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name'"));
	}

	// Load the tree
	FString LoadError;
	UStateTree* Tree = StateTreeHelpers::LoadStateTree(AssetPath, LoadError);
	if (!Tree)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(LoadError);
	}

	UStateTreeEditorData* EditorData = StateTreeHelpers::GetEditorData(Tree, LoadError);
	if (!EditorData)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(LoadError);
	}

	// Parse optional state type
	FString StateTypeStr;
	Params->TryGetStringField(TEXT("state_type"), StateTypeStr);
	EStateTreeStateType StateType = EStateTreeStateType::State;
	if (!StateTypeStr.IsEmpty() && !ParseStateType(StateTypeStr, StateType))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Invalid state_type: '%s'"), *StateTypeStr));
	}

	// Parse optional position (-1 = append)
	int32 Position = -1;
	if (Params->HasField(TEXT("position")))
	{
		Position = static_cast<int32>(Params->GetNumberField(TEXT("position")));
	}

	// Parse optional enabled
	bool bEnabled = true;
	Params->TryGetBoolField(TEXT("enabled"), bEnabled);

	// Resolve optional parent state
	UStateTreeState* ParentState = nullptr;
	bool bHasParent = Params->HasField(TEXT("state")) ||
	                   Params->HasField(TEXT("state_name")) ||
	                   Params->HasField(TEXT("state_guid"));
	if (bHasParent)
	{
		FString ResolveError;
		ParentState = StateTreeHelpers::ResolveState(EditorData, Params, ResolveError);
		if (!ParentState)
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(ResolveError);
		}
	}

	// Create the new state
	UStateTreeState* NewState = nullptr;
	if (ParentState)
	{
		NewState = NewObject<UStateTreeState>(ParentState, FName(), RF_Transactional);
		NewState->Parent = ParentState;

		if (Position >= 0 && Position < ParentState->Children.Num())
		{
			ParentState->Children.Insert(NewState, Position);
		}
		else
		{
			ParentState->Children.Add(NewState);
		}
	}
	else
	{
		NewState = NewObject<UStateTreeState>(EditorData, FName(), RF_Transactional);

		if (Position >= 0 && Position < EditorData->SubTrees.Num())
		{
			EditorData->SubTrees.Insert(NewState, Position);
		}
		else
		{
			EditorData->SubTrees.Add(NewState);
		}
	}

	NewState->Name = FName(*StateName);
	NewState->Type = StateType;
	NewState->bEnabled = bEnabled;
	NewState->ID = FGuid::NewGuid();

	// Parse optional selection behavior
	FString SelectionStr;
	if (Params->TryGetStringField(TEXT("selection_behavior"), SelectionStr))
	{
		EStateTreeStateSelectionBehavior Behavior;
		if (ParseSelectionBehavior(SelectionStr, Behavior))
		{
			NewState->SelectionBehavior = Behavior;
		}
	}

	Tree->MarkPackageDirty();

	// Build response
	FString IndexPath = StateTreeHelpers::ComputeIndexPath(EditorData, NewState);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("name"), StateName);
	Result->SetStringField(TEXT("type"), StateTreeHelpers::StateTypeToString(static_cast<uint8>(StateType)));
	Result->SetStringField(TEXT("id"), NewState->ID.ToString());
	Result->SetStringField(TEXT("index_path"), IndexPath);
	return Result;
}

// ---------------------------------------------------------------------------
// Handler 3: HandleAddStateTreeTask
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPStateTreeCommands::HandleAddStateTreeTask(
	const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path'"));
	}

	FString TaskClassName;
	if (!Params->TryGetStringField(TEXT("task_class"), TaskClassName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'task_class'"));
	}

	// Load tree and editor data
	FString LoadError;
	UStateTree* Tree = StateTreeHelpers::LoadStateTree(AssetPath, LoadError);
	if (!Tree)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(LoadError);
	}

	UStateTreeEditorData* EditorData = StateTreeHelpers::GetEditorData(Tree, LoadError);
	if (!EditorData)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(LoadError);
	}

	// Resolve the target state
	FString StateError;
	UStateTreeState* State = StateTreeHelpers::ResolveState(EditorData, Params, StateError);
	if (!State)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(StateError);
	}

	// Find the task struct
	FString StructError;
	UScriptStruct* TaskStruct = FindNodeStruct(
		TaskClassName, FStateTreeTaskBase::StaticStruct(), StructError);
	if (!TaskStruct)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(StructError);
	}

	// Create the editor node
	FStateTreeEditorNode& NewNode = State->Tasks.AddDefaulted_GetRef();
	InitEditorNode(NewNode, TaskStruct);

	// Apply optional properties
	const TSharedPtr<FJsonObject>* PropsObj = nullptr;
	TArray<FString> PropWarnings;
	if (Params->TryGetObjectField(TEXT("properties"), PropsObj) && PropsObj)
	{
		PropWarnings = ApplyPropertiesToInstancedStruct(NewNode.Node, *PropsObj);
	}

	Tree->MarkPackageDirty();

	int32 NodeIndex = State->Tasks.Num() - 1;
	TSharedPtr<FJsonObject> Result = NodeToJsonResult(NewNode, NodeIndex);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("state"), State->Name.ToString());

	if (PropWarnings.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> WarnArr;
		for (const FString& W : PropWarnings)
		{
			WarnArr.Add(MakeShared<FJsonValueString>(W));
		}
		Result->SetArrayField(TEXT("property_warnings"), WarnArr);
	}

	return Result;
}

// ---------------------------------------------------------------------------
// Handler 4: HandleAddStateTreeEvaluator
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPStateTreeCommands::HandleAddStateTreeEvaluator(
	const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path'"));
	}

	FString EvalClassName;
	if (!Params->TryGetStringField(TEXT("evaluator_class"), EvalClassName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'evaluator_class'"));
	}

	FString LoadError;
	UStateTree* Tree = StateTreeHelpers::LoadStateTree(AssetPath, LoadError);
	if (!Tree)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(LoadError);
	}

	UStateTreeEditorData* EditorData = StateTreeHelpers::GetEditorData(Tree, LoadError);
	if (!EditorData)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(LoadError);
	}

	// Find the evaluator struct
	FString StructError;
	UScriptStruct* EvalStruct = FindNodeStruct(
		EvalClassName, FStateTreeEvaluatorBase::StaticStruct(), StructError);
	if (!EvalStruct)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(StructError);
	}

	// Create the editor node
	FStateTreeEditorNode& NewNode = EditorData->Evaluators.AddDefaulted_GetRef();
	InitEditorNode(NewNode, EvalStruct);

	// Apply optional properties
	const TSharedPtr<FJsonObject>* PropsObj = nullptr;
	TArray<FString> PropWarnings;
	if (Params->TryGetObjectField(TEXT("properties"), PropsObj) && PropsObj)
	{
		PropWarnings = ApplyPropertiesToInstancedStruct(NewNode.Node, *PropsObj);
	}

	Tree->MarkPackageDirty();

	int32 NodeIndex = EditorData->Evaluators.Num() - 1;
	TSharedPtr<FJsonObject> Result = NodeToJsonResult(NewNode, NodeIndex);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("location"), TEXT("evaluators"));

	if (PropWarnings.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> WarnArr;
		for (const FString& W : PropWarnings)
		{
			WarnArr.Add(MakeShared<FJsonValueString>(W));
		}
		Result->SetArrayField(TEXT("property_warnings"), WarnArr);
	}

	return Result;
}

// ---------------------------------------------------------------------------
// Handler 5: HandleAddStateTreeGlobalTask
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPStateTreeCommands::HandleAddStateTreeGlobalTask(
	const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path'"));
	}

	FString TaskClassName;
	if (!Params->TryGetStringField(TEXT("task_class"), TaskClassName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'task_class'"));
	}

	FString LoadError;
	UStateTree* Tree = StateTreeHelpers::LoadStateTree(AssetPath, LoadError);
	if (!Tree)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(LoadError);
	}

	UStateTreeEditorData* EditorData = StateTreeHelpers::GetEditorData(Tree, LoadError);
	if (!EditorData)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(LoadError);
	}

	// Find the task struct
	FString StructError;
	UScriptStruct* TaskStruct = FindNodeStruct(
		TaskClassName, FStateTreeTaskBase::StaticStruct(), StructError);
	if (!TaskStruct)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(StructError);
	}

	// Create the editor node
	FStateTreeEditorNode& NewNode = EditorData->GlobalTasks.AddDefaulted_GetRef();
	InitEditorNode(NewNode, TaskStruct);

	// Apply optional properties
	const TSharedPtr<FJsonObject>* PropsObj = nullptr;
	TArray<FString> PropWarnings;
	if (Params->TryGetObjectField(TEXT("properties"), PropsObj) && PropsObj)
	{
		PropWarnings = ApplyPropertiesToInstancedStruct(NewNode.Node, *PropsObj);
	}

	Tree->MarkPackageDirty();

	int32 NodeIndex = EditorData->GlobalTasks.Num() - 1;
	TSharedPtr<FJsonObject> Result = NodeToJsonResult(NewNode, NodeIndex);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("location"), TEXT("global_tasks"));

	if (PropWarnings.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> WarnArr;
		for (const FString& W : PropWarnings)
		{
			WarnArr.Add(MakeShared<FJsonValueString>(W));
		}
		Result->SetArrayField(TEXT("property_warnings"), WarnArr);
	}

	return Result;
}

// ---------------------------------------------------------------------------
// Handler 6: HandleAddStateTreeCondition
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPStateTreeCommands::HandleAddStateTreeCondition(
	const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path'"));
	}

	FString CondClassName;
	if (!Params->TryGetStringField(TEXT("condition_class"), CondClassName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'condition_class'"));
	}

	FString LoadError;
	UStateTree* Tree = StateTreeHelpers::LoadStateTree(AssetPath, LoadError);
	if (!Tree)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(LoadError);
	}

	UStateTreeEditorData* EditorData = StateTreeHelpers::GetEditorData(Tree, LoadError);
	if (!EditorData)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(LoadError);
	}

	// Resolve state
	FString StateError;
	UStateTreeState* State = StateTreeHelpers::ResolveState(EditorData, Params, StateError);
	if (!State)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(StateError);
	}

	// Find the condition struct
	FString StructError;
	UScriptStruct* CondStruct = FindNodeStruct(
		CondClassName, FStateTreeConditionBase::StaticStruct(), StructError);
	if (!CondStruct)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(StructError);
	}

	// Determine target: "enter_conditions" (default) or "transition"
	FString Target;
	Params->TryGetStringField(TEXT("target"), Target);

	// Parse operand
	FString OperandStr;
	Params->TryGetStringField(TEXT("operand"), OperandStr);
	EStateTreeExpressionOperand Operand = EStateTreeExpressionOperand::And;
	if (OperandStr.Equals(TEXT("Or"), ESearchCase::IgnoreCase))
	{
		Operand = EStateTreeExpressionOperand::Or;
	}

	TArray<FStateTreeEditorNode>* TargetArray = nullptr;
	FString LocationDesc;

	if (Target.Equals(TEXT("transition"), ESearchCase::IgnoreCase))
	{
		// Require transition_index
		if (!Params->HasField(TEXT("transition_index")))
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				TEXT("'transition_index' required when target='transition'"));
		}

		int32 TransIdx = static_cast<int32>(Params->GetNumberField(TEXT("transition_index")));
		if (TransIdx < 0 || TransIdx >= State->Transitions.Num())
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("transition_index %d out of range (0..%d)"),
					TransIdx, State->Transitions.Num() - 1));
		}

		TargetArray = &State->Transitions[TransIdx].Conditions;
		LocationDesc = FString::Printf(TEXT("transition[%d].conditions"), TransIdx);
	}
	else
	{
		TargetArray = &State->EnterConditions;
		LocationDesc = TEXT("enter_conditions");
	}

	// Create the editor node
	FStateTreeEditorNode& NewNode = TargetArray->AddDefaulted_GetRef();
	InitEditorNode(NewNode, CondStruct);
	NewNode.ExpressionOperand = Operand;

	// Apply optional properties
	const TSharedPtr<FJsonObject>* PropsObj = nullptr;
	TArray<FString> PropWarnings;
	if (Params->TryGetObjectField(TEXT("properties"), PropsObj) && PropsObj)
	{
		PropWarnings = ApplyPropertiesToInstancedStruct(NewNode.Node, *PropsObj);
	}

	Tree->MarkPackageDirty();

	int32 NodeIndex = TargetArray->Num() - 1;
	TSharedPtr<FJsonObject> Result = NodeToJsonResult(NewNode, NodeIndex);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("state"), State->Name.ToString());
	Result->SetStringField(TEXT("location"), LocationDesc);

	if (PropWarnings.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> WarnArr;
		for (const FString& W : PropWarnings)
		{
			WarnArr.Add(MakeShared<FJsonValueString>(W));
		}
		Result->SetArrayField(TEXT("property_warnings"), WarnArr);
	}

	return Result;
}

// ---------------------------------------------------------------------------
// Handler 7: HandleAddStateTreeTransition
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPStateTreeCommands::HandleAddStateTreeTransition(
	const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path'"));
	}

	FString TriggerStr;
	if (!Params->TryGetStringField(TEXT("trigger"), TriggerStr))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'trigger'"));
	}

	FString LoadError;
	UStateTree* Tree = StateTreeHelpers::LoadStateTree(AssetPath, LoadError);
	if (!Tree)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(LoadError);
	}

	UStateTreeEditorData* EditorData = StateTreeHelpers::GetEditorData(Tree, LoadError);
	if (!EditorData)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(LoadError);
	}

	// Resolve source state
	FString StateError;
	UStateTreeState* State = StateTreeHelpers::ResolveState(EditorData, Params, StateError);
	if (!State)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(StateError);
	}

	// Parse trigger
	EStateTreeTransitionTrigger Trigger;
	if (!ParseTransitionTrigger(TriggerStr, Trigger))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Invalid trigger: '%s'"), *TriggerStr));
	}

	// Parse priority
	FString PriorityStr;
	Params->TryGetStringField(TEXT("priority"), PriorityStr);
	EStateTreeTransitionPriority Priority = EStateTreeTransitionPriority::Normal;
	if (!PriorityStr.IsEmpty() && !ParseTransitionPriority(PriorityStr, Priority))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Invalid priority: '%s'"), *PriorityStr));
	}

	// Create the transition
	FStateTreeTransition& NewTransition = State->Transitions.AddDefaulted_GetRef();
	NewTransition.ID = FGuid::NewGuid();
	NewTransition.Trigger = Trigger;
	NewTransition.Priority = Priority;

	// Handle delay
	float Delay = 0.0f;
	if (Params->TryGetNumberField(TEXT("delay"), Delay) && Delay > 0.0f)
	{
		NewTransition.bDelayTransition = true;
		NewTransition.DelayDuration = Delay;

		float DelayVariance = 0.0f;
		if (Params->TryGetNumberField(TEXT("delay_variance"), DelayVariance))
		{
			NewTransition.DelayRandomVariance = DelayVariance;
		}
	}

	// Handle event tag for OnEvent trigger
	FString EventTagStr;
	if (Params->TryGetStringField(TEXT("event_tag"), EventTagStr) && !EventTagStr.IsEmpty())
	{
		NewTransition.RequiredEvent.Tag = FGameplayTag::RequestGameplayTag(FName(*EventTagStr), false);
	}

	// Resolve target state from three possible params (matching the Python/Go tool signatures):
	//   "target_state"       — index path, special keyword, or GUID
	//   "target_state_name"  — state name (case-insensitive)
	//   "target_state_guid"  — state GUID string
	// Priority: target_state > target_state_guid > target_state_name
	FString TargetStateStr;
	Params->TryGetStringField(TEXT("target_state"), TargetStateStr);

	FString TargetStateName;
	Params->TryGetStringField(TEXT("target_state_name"), TargetStateName);

	FString TargetStateGuidStr;
	Params->TryGetStringField(TEXT("target_state_guid"), TargetStateGuidStr);

	FString TargetDesc = TEXT("none");

	bool bHasTarget = !TargetStateStr.IsEmpty() || !TargetStateGuidStr.IsEmpty() || !TargetStateName.IsEmpty();

	if (bHasTarget)
	{
		auto SetMetaTarget = [&](EStateTreeTransitionType InType, const FString& InDesc)
		{
#if WITH_EDITORONLY_DATA
			NewTransition.State = FStateTreeStateLink(InType);
#endif
			TargetDesc = InDesc;
		};

		auto SetGotoStateTarget = [&](UStateTreeState* TargetState, const FString& InError) -> TSharedPtr<FJsonObject>
		{
			if (!TargetState)
			{
				return FEpicUnrealMCPCommonUtils::CreateErrorResponse(InError);
			}

#if WITH_EDITORONLY_DATA
			NewTransition.State = TargetState->GetLinkToState();
#endif
			TargetDesc = TargetState->Name.ToString();
			return nullptr;
		};

		// Priority 1: "target_state" — keyword, GUID, index path, or name
		if (!TargetStateStr.IsEmpty())
		{
			if (TargetStateStr.Equals(TEXT("TreeSucceeded"), ESearchCase::IgnoreCase) ||
				TargetStateStr.Equals(TEXT("Succeeded"), ESearchCase::IgnoreCase))
			{
				SetMetaTarget(EStateTreeTransitionType::Succeeded, TEXT("TreeSucceeded"));
			}
			else if (TargetStateStr.Equals(TEXT("TreeFailed"), ESearchCase::IgnoreCase) ||
				TargetStateStr.Equals(TEXT("Failed"), ESearchCase::IgnoreCase))
			{
				SetMetaTarget(EStateTreeTransitionType::Failed, TEXT("TreeFailed"));
			}
			else if (TargetStateStr.Equals(TEXT("TreeStopped"), ESearchCase::IgnoreCase) ||
				TargetStateStr.Equals(TEXT("None"), ESearchCase::IgnoreCase))
			{
				SetMetaTarget(EStateTreeTransitionType::None, TEXT("TreeStopped"));
			}
			else if (TargetStateStr.Equals(TEXT("NextSelectableState"), ESearchCase::IgnoreCase))
			{
				SetMetaTarget(EStateTreeTransitionType::NextSelectableState, TEXT("NextSelectableState"));
			}
			else if (TargetStateStr.Equals(TEXT("NextSibling"), ESearchCase::IgnoreCase) ||
				TargetStateStr.Equals(TEXT("NextState"), ESearchCase::IgnoreCase))
			{
				SetMetaTarget(EStateTreeTransitionType::NextState, TEXT("NextSibling"));
			}
			else
			{
				TSharedPtr<FJsonObject> TargetParams = MakeShared<FJsonObject>();

				FGuid ParsedGuid;
				if (FGuid::Parse(TargetStateStr, ParsedGuid))
				{
					TargetParams->SetStringField(TEXT("state_guid"), TargetStateStr);
				}
				else if (TargetStateStr.Contains(TEXT("/")))
				{
					TargetParams->SetStringField(TEXT("state"), TargetStateStr);
				}
				else
				{
					TargetParams->SetStringField(TEXT("state_name"), TargetStateStr);
				}

				FString ResolveError;
				UStateTreeState* TargetState = StateTreeHelpers::ResolveState(EditorData, TargetParams, ResolveError);
				if (TSharedPtr<FJsonObject> Err = SetGotoStateTarget(TargetState, ResolveError))
				{
					return Err;
				}
			}
		}
		// Priority 2: "target_state_guid" — explicit GUID
		else if (!TargetStateGuidStr.IsEmpty())
		{
			FGuid ParsedGuid;
			if (!FGuid::Parse(TargetStateGuidStr, ParsedGuid))
			{
				return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
					FString::Printf(TEXT("Invalid GUID format: '%s'"), *TargetStateGuidStr));
			}

			FString ResolveError;
			UStateTreeState* TargetState = StateTreeHelpers::ResolveStateByGuid(EditorData, ParsedGuid, ResolveError);
			if (TSharedPtr<FJsonObject> Err = SetGotoStateTarget(TargetState, ResolveError))
			{
				return Err;
			}
		}
			// Priority 3: "target_state_name" — case-insensitive name
		else if (!TargetStateName.IsEmpty())
		{
			FString ResolveError;
			UStateTreeState* TargetState = StateTreeHelpers::ResolveStateByName(EditorData, TargetStateName, ResolveError);
			if (TSharedPtr<FJsonObject> Err = SetGotoStateTarget(TargetState, ResolveError))
			{
				return Err;
			}
		}
	}

	Tree->MarkPackageDirty();

	// Build response
	int32 TransIndex = State->Transitions.Num() - 1;
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetNumberField(TEXT("index"), TransIndex);
	Result->SetStringField(TEXT("id"), NewTransition.ID.ToString());
	Result->SetStringField(TEXT("state"), State->Name.ToString());
	Result->SetStringField(TEXT("trigger"), TriggerStr);
	Result->SetStringField(TEXT("target"), TargetDesc);
	Result->SetStringField(TEXT("priority"),
		StateTreeHelpers::TransitionPriorityToString(static_cast<uint8>(Priority)));

	return Result;
}

// ---------------------------------------------------------------------------
// Handler 8: HandleAddStateTreeParameter
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPStateTreeCommands::HandleAddStateTreeParameter(
	const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path'"));
	}

	FString ParamName;
	if (!Params->TryGetStringField(TEXT("name"), ParamName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name'"));
	}

	FString TypeStr;
	if (!Params->TryGetStringField(TEXT("type"), TypeStr))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'type'"));
	}

	FString LoadError;
	UStateTree* Tree = StateTreeHelpers::LoadStateTree(AssetPath, LoadError);
	if (!Tree)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(LoadError);
	}

	UStateTreeEditorData* EditorData = StateTreeHelpers::GetEditorData(Tree, LoadError);
	if (!EditorData)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(LoadError);
	}

	// Parse the type into EPropertyBagPropertyType
	const UObject* ValueTypeObject = nullptr;
	EPropertyBagPropertyType BagType = ParsePropertyBagType(TypeStr, ValueTypeObject);
	if (BagType == EPropertyBagPropertyType::None)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Unknown parameter type: '%s'"), *TypeStr));
	}

	// Build property creation descriptor and add via the EditorData builder API.
	// CreateRootProperties forwards to CreateUniquelyNamedPropertiesInPropertyBag
	// which adds properties to the private RootParameterPropertyBag.
	TArray<UE::PropertyBinding::FPropertyCreationDescriptor> CreationDescs;
	UE::PropertyBinding::FPropertyCreationDescriptor& Desc = CreationDescs.AddDefaulted_GetRef();
	Desc.PropertyDesc = FPropertyBagPropertyDesc(FName(*ParamName), BagType, ValueTypeObject);

	EditorData->CreateRootProperties(CreationDescs);

	Tree->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("name"), ParamName);
	Result->SetStringField(TEXT("type"), TypeStr);
	return Result;
}
