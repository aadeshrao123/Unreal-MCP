#include "Commands/EpicUnrealMCPWidgetCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "Commands/EpicUnrealMCPPropertyUtils.h"

// UMG (runtime)
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Components/PanelWidget.h"
#include "Components/PanelSlot.h"
#include "Animation/WidgetAnimation.h"

// Editor
#include "BaseWidgetBlueprint.h"
#include "WidgetBlueprint.h"
#include "EditorAssetLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/UObjectIterator.h"
#include "JsonObjectConverter.h"
#include "UObject/EnumProperty.h"

// Convenience alias
using PU = FEpicUnrealMCPPropertyUtils;

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
FEpicUnrealMCPWidgetCommands::FEpicUnrealMCPWidgetCommands()
{
}

// ---------------------------------------------------------------------------
// Dispatch
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FEpicUnrealMCPWidgetCommands::HandleCommand(
	const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
	if (CommandType == TEXT("get_widget_tree"))        return HandleGetWidgetTree(Params);
	if (CommandType == TEXT("add_widget"))             return HandleAddWidget(Params);
	if (CommandType == TEXT("remove_widget"))          return HandleRemoveWidget(Params);
	if (CommandType == TEXT("move_widget"))            return HandleMoveWidget(Params);
	if (CommandType == TEXT("rename_widget"))          return HandleRenameWidget(Params);
	if (CommandType == TEXT("duplicate_widget"))       return HandleDuplicateWidget(Params);
	if (CommandType == TEXT("get_widget_properties"))  return HandleGetWidgetProperties(Params);
	if (CommandType == TEXT("set_widget_properties"))  return HandleSetWidgetProperties(Params);
	if (CommandType == TEXT("get_slot_properties"))    return HandleGetSlotProperties(Params);
	if (CommandType == TEXT("set_slot_properties"))    return HandleSetSlotProperties(Params);
	if (CommandType == TEXT("list_widget_types"))      return HandleListWidgetTypes(Params);

	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
		FString::Printf(TEXT("Unknown widget command: %s"), *CommandType));
}

// ===========================================================================
// HELPERS
// ===========================================================================

UWidgetBlueprint* FEpicUnrealMCPWidgetCommands::LoadWidgetBlueprint(const FString& Path)
{
	UObject* Asset = UEditorAssetLibrary::LoadAsset(Path);
	if (!Asset)
	{
		// Try with .AssetName suffix (e.g. /Game/UI/WBP_Foo.WBP_Foo)
		FString AssetName;
		int32 LastSlash = INDEX_NONE;
		if (Path.FindLastChar('/', LastSlash))
			AssetName = Path.Mid(LastSlash + 1);
		else
			AssetName = Path;

		const FString FullPath = Path + TEXT(".") + AssetName;
		Asset = StaticLoadObject(UWidgetBlueprint::StaticClass(), nullptr, *FullPath);
	}
	return Cast<UWidgetBlueprint>(Asset);
}

UWidget* FEpicUnrealMCPWidgetCommands::FindWidgetByName(UWidgetTree* Tree, const FString& Name)
{
	if (!Tree) return nullptr;
	return Tree->FindWidget(FName(*Name));
}

UClass* FEpicUnrealMCPWidgetCommands::ResolveWidgetClass(const FString& ClassName)
{
	if (ClassName.IsEmpty()) return nullptr;

	// Full path resolution
	if (ClassName.Contains(TEXT(".")))
	{
		UClass* C = FindObject<UClass>(nullptr, *ClassName);
		if (C && C->IsChildOf(UWidget::StaticClass())) return C;
	}

	// Short name matching against all UWidget subclasses
	TArray<UClass*> Derived;
	GetDerivedClasses(UWidget::StaticClass(), Derived, true);
	for (UClass* C : Derived)
	{
		if (!C) continue;
		const FString CName = C->GetName();
		if (CName == ClassName ||
			CName == (TEXT("U") + ClassName) ||
			(ClassName.StartsWith(TEXT("U")) && CName == ClassName.RightChop(1)))
		{
			return C;
		}
	}
	return nullptr;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPWidgetCommands::SerializeWidgetRecursive(UWidget* Widget)
{
	if (!Widget) return nullptr;

	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetStringField(TEXT("name"), Widget->GetName());
	Obj->SetStringField(TEXT("class"), Widget->GetClass()->GetName());

	const bool bIsPanel = Widget->IsA<UPanelWidget>();
	Obj->SetBoolField(TEXT("is_panel"), bIsPanel);

#if WITH_EDITORONLY_DATA
	Obj->SetBoolField(TEXT("is_variable"), Widget->bIsVariable);
#endif

	// Slot info (if this widget is a child of a panel)
	if (Widget->Slot)
	{
		Obj->SetStringField(TEXT("slot_class"), Widget->Slot->GetClass()->GetName());
	}

	// Recurse into children for panels
	if (bIsPanel)
	{
		UPanelWidget* Panel = Cast<UPanelWidget>(Widget);
		TArray<TSharedPtr<FJsonValue>> ChildArray;
		for (int32 i = 0; i < Panel->GetChildrenCount(); ++i)
		{
			UWidget* Child = Panel->GetChildAt(i);
			if (Child)
			{
				TSharedPtr<FJsonObject> ChildObj = SerializeWidgetRecursive(Child);
				if (ChildObj.IsValid())
					ChildArray.Add(MakeShared<FJsonValueObject>(ChildObj));
			}
		}
		Obj->SetArrayField(TEXT("children"), ChildArray);
	}

	return Obj;
}

// ---------------------------------------------------------------------------
// Safe slot serialization — only serializes editable properties and guards
// against invalid enum FName entries that crash FJsonObjectConverter.
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FEpicUnrealMCPWidgetCommands::SerializeSlotProperties(UPanelSlot* Slot)
{
	if (!Slot) return nullptr;

	TSharedPtr<FJsonObject> Props = MakeShared<FJsonObject>();

	for (TFieldIterator<FProperty> It(Slot->GetClass(), EFieldIteratorFlags::IncludeSuper); It; ++It)
	{
		FProperty* Prop = *It;

		// Only serialize properties visible in the editor — skip internal
		// Slate bookkeeping that can contain uninitialized enum values.
		if (!Prop->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible))
			continue;

		// Skip delegate / multicast delegate properties entirely
		if (CastField<FDelegateProperty>(Prop) || CastField<FMulticastDelegateProperty>(Prop))
			continue;

		// Guard enum properties against out-of-range values
		if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Prop))
		{
			if (UEnum* Enum = EnumProp->GetEnum())
			{
				const void* Addr = Prop->ContainerPtrToValuePtr<void>(Slot);
				FNumericProperty* UnderlyingProp = EnumProp->GetUnderlyingProperty();
				const int64 IntValue = UnderlyingProp->GetSignedIntPropertyValue(Addr);
				const int32 Idx = Enum->GetIndexByValue(IntValue);
				if (Idx == INDEX_NONE)
				{
					Props->SetStringField(Prop->GetName(),
						FString::Printf(TEXT("<invalid enum %lld>"), IntValue));
					continue;
				}
			}
		}
		if (FByteProperty* ByteProp = CastField<FByteProperty>(Prop))
		{
			if (UEnum* Enum = ByteProp->GetIntPropertyEnum())
			{
				const void* Addr = Prop->ContainerPtrToValuePtr<void>(Slot);
				const int64 IntValue = ByteProp->GetSignedIntPropertyValue(Addr);
				const int32 Idx = Enum->GetIndexByValue(IntValue);
				if (Idx == INDEX_NONE)
				{
					Props->SetStringField(Prop->GetName(),
						FString::Printf(TEXT("<invalid enum %lld>"), IntValue));
					continue;
				}
			}
		}

		const void* Addr = Prop->ContainerPtrToValuePtr<void>(Slot);
		TSharedPtr<FJsonValue> Val = FJsonObjectConverter::UPropertyToJsonValue(Prop, Addr);
		if (Val.IsValid())
			Props->SetField(Prop->GetName(), Val);
	}

	return Props;
}

// ---------------------------------------------------------------------------
// Save helpers — structural vs property-only modification
// ---------------------------------------------------------------------------
void FEpicUnrealMCPWidgetCommands::MarkModifiedAndSave(UWidgetBlueprint* WBP)
{
	if (!WBP) return;

	// Sync the GUID map with the current widget set BEFORE compilation.
	// MarkBlueprintAsStructurallyModified triggers synchronous compilation,
	// which runs ValidateAndFixUpVariableGuids. That validator fires
	// ensureAlwaysMsgf warnings for any mismatch between the GUID map and
	// the actual widget set (found via ForEachSourceWidget). By syncing
	// here, we prevent those warnings regardless of what prior operations
	// (DuplicateObject, Rename, RemoveWidget) may have left behind.
	{
		TSet<FName> CurrentNames;
		WBP->ForEachSourceWidget([&CurrentNames](UWidget* W)
		{
			CurrentNames.Add(W->GetFName());
		});
		for (UWidgetAnimation* Anim : WBP->Animations)
		{
			if (Anim)
				CurrentNames.Add(Anim->GetFName());
		}

		WBP->Modify();

		// Add GUIDs for any widgets that don't have one yet
		for (const FName& Name : CurrentNames)
		{
			if (!WBP->WidgetVariableNameToGuidMap.Contains(Name))
			{
				WBP->WidgetVariableNameToGuidMap.Add(Name, FGuid::NewGuid());
			}
		}

		// Remove stale GUIDs for widgets that no longer exist
		for (auto It = WBP->WidgetVariableNameToGuidMap.CreateIterator(); It; ++It)
		{
			if (!CurrentNames.Contains(It.Key()))
			{
				It.RemoveCurrent();
			}
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WBP);
	WBP->MarkPackageDirty();
	const FString AssetPath = WBP->GetOutermost()->GetName();
	UEditorAssetLibrary::SaveAsset(AssetPath);
}

static void MarkPropertyModifiedAndSave(UWidgetBlueprint* WBP)
{
	if (!WBP) return;
	FBlueprintEditorUtils::MarkBlueprintAsModified(WBP);
	WBP->MarkPackageDirty();
	const FString AssetPath = WBP->GetOutermost()->GetName();
	UEditorAssetLibrary::SaveAsset(AssetPath);
}

// ===========================================================================
// get_widget_tree
// ===========================================================================
TSharedPtr<FJsonObject> FEpicUnrealMCPWidgetCommands::HandleGetWidgetTree(
	const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintPath;
	if (!Params->TryGetStringField(TEXT("widget_blueprint_path"), BlueprintPath))
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'widget_blueprint_path'"));

	UWidgetBlueprint* WBP = LoadWidgetBlueprint(BlueprintPath);
	if (!WBP)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Widget Blueprint not found: %s"), *BlueprintPath));

	UWidgetTree* Tree = WBP->WidgetTree;
	if (!Tree)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("WidgetTree is null"));

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("widget_blueprint_path"), BlueprintPath);

	if (Tree->RootWidget)
		Result->SetObjectField(TEXT("root"), SerializeWidgetRecursive(Tree->RootWidget));
	else
		Result->SetField(TEXT("root"), MakeShared<FJsonValueNull>());

	// Total widget count
	TArray<UWidget*> AllWidgets;
	Tree->GetAllWidgets(AllWidgets);
	Result->SetNumberField(TEXT("total_widgets"), AllWidgets.Num());

	return Result;
}

// ===========================================================================
// add_widget
// ===========================================================================
TSharedPtr<FJsonObject> FEpicUnrealMCPWidgetCommands::HandleAddWidget(
	const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintPath, WidgetClassName, ParentName;
	if (!Params->TryGetStringField(TEXT("widget_blueprint_path"), BlueprintPath))
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'widget_blueprint_path'"));
	if (!Params->TryGetStringField(TEXT("widget_class"), WidgetClassName))
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'widget_class'"));
	if (!Params->TryGetStringField(TEXT("parent_widget_name"), ParentName))
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'parent_widget_name'"));

	FString WidgetName;
	Params->TryGetStringField(TEXT("widget_name"), WidgetName);

	double IndexD = -1.0;
	Params->TryGetNumberField(TEXT("index"), IndexD);
	const int32 Index = static_cast<int32>(IndexD);

	UWidgetBlueprint* WBP = LoadWidgetBlueprint(BlueprintPath);
	if (!WBP)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Widget Blueprint not found: %s"), *BlueprintPath));

	UWidgetTree* Tree = WBP->WidgetTree;
	if (!Tree)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("WidgetTree is null"));

	// Resolve widget class
	UClass* WidgetClass = ResolveWidgetClass(WidgetClassName);
	if (!WidgetClass)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Widget class not found: '%s'"), *WidgetClassName));
	if (WidgetClass->HasAnyClassFlags(CLASS_Abstract))
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Cannot instantiate abstract class: '%s'"), *WidgetClassName));

	// Find parent panel
	UWidget* ParentWidget = FindWidgetByName(Tree, ParentName);
	if (!ParentWidget)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Parent widget '%s' not found"), *ParentName));

	UPanelWidget* ParentPanel = Cast<UPanelWidget>(ParentWidget);
	if (!ParentPanel)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Parent widget '%s' is not a panel (class: %s)"),
				*ParentName, *ParentWidget->GetClass()->GetName()));

	if (!ParentPanel->CanAddMoreChildren())
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Parent panel '%s' cannot accept more children"), *ParentName));

	// Generate name if not provided
	if (WidgetName.IsEmpty())
	{
		FString BaseName = WidgetClass->GetName();
		if (BaseName.StartsWith(TEXT("U")))
			BaseName = BaseName.RightChop(1);
		int32 Counter = 0;
		FString CandidateName;
		do
		{
			CandidateName = FString::Printf(TEXT("%s_%d"), *BaseName, Counter++);
		} while (Tree->FindWidget(FName(*CandidateName)) != nullptr);
		WidgetName = CandidateName;
	}
	else
	{
		if (Tree->FindWidget(FName(*WidgetName)))
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Widget name '%s' already exists in tree"), *WidgetName));
	}

	// Create the widget (same as UWidgetTree::ConstructWidget for non-UserWidget types)
	UWidget* NewWidget = NewObject<UWidget>(Tree, WidgetClass, FName(*WidgetName), RF_Transactional);
	if (!NewWidget)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create widget"));

	// Add to parent FIRST (engine flow: create → add → register GUID → save)
	UPanelSlot* NewSlot = nullptr;
	if (Index >= 0)
		NewSlot = ParentPanel->InsertChildAt(Index, NewWidget);
	else
		NewSlot = ParentPanel->AddChild(NewWidget);

	if (!NewSlot)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to add widget to parent panel"));

	// Register widget GUID AFTER adding to parent — matches the engine's
	// WidgetBlueprintEditorUtils.cpp flow: Create → AddChild → OnVariableAdded.
	WBP->OnVariableAdded(NewWidget->GetFName());

	// Set widget properties if provided
	const TSharedPtr<FJsonObject>* WidgetProps = nullptr;
	if (Params->TryGetObjectField(TEXT("widget_properties"), WidgetProps) && WidgetProps)
	{
		for (const auto& Pair : (*WidgetProps)->Values)
		{
			FString Err;
			PU::SetProperty(NewWidget, Pair.Key, Pair.Value, Err);
		}
	}

	// Set slot properties if provided
	const TSharedPtr<FJsonObject>* SlotProps = nullptr;
	if (Params->TryGetObjectField(TEXT("slot_properties"), SlotProps) && SlotProps)
	{
		for (const auto& Pair : (*SlotProps)->Values)
		{
			FString Err;
			PU::SetProperty(NewSlot, Pair.Key, Pair.Value, Err);
		}
	}

	MarkModifiedAndSave(WBP);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("widget_name"), WidgetName);
	Result->SetStringField(TEXT("widget_class"), WidgetClass->GetName());
	Result->SetStringField(TEXT("parent"), ParentName);
	Result->SetStringField(TEXT("slot_class"), NewSlot->GetClass()->GetName());
	if (Index >= 0)
		Result->SetNumberField(TEXT("index"), Index);
	return Result;
}

// ===========================================================================
// remove_widget
// ===========================================================================
TSharedPtr<FJsonObject> FEpicUnrealMCPWidgetCommands::HandleRemoveWidget(
	const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintPath, WidgetName;
	if (!Params->TryGetStringField(TEXT("widget_blueprint_path"), BlueprintPath))
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'widget_blueprint_path'"));
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'widget_name'"));

	UWidgetBlueprint* WBP = LoadWidgetBlueprint(BlueprintPath);
	if (!WBP)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Widget Blueprint not found: %s"), *BlueprintPath));

	UWidgetTree* Tree = WBP->WidgetTree;
	if (!Tree)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("WidgetTree is null"));

	UWidget* Widget = FindWidgetByName(Tree, WidgetName);
	if (!Widget)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Widget '%s' not found"), *WidgetName));

	if (Widget == Tree->RootWidget)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Cannot remove the root widget"));

	// Collect widget names BEFORE removal — same flow as engine's
	// WidgetBlueprintEditorUtils::DeleteWidgets() in WidgetBlueprintEditorUtils.cpp.
	TArray<FName> RemovedNames;
	UWidgetTree::ForWidgetAndChildren(Widget, [&](UWidget* SubWidget)
	{
		if (SubWidget)
			RemovedNames.Add(SubWidget->GetFName());
	});

	if (!Tree->RemoveWidget(Widget))
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Failed to remove widget '%s'"), *WidgetName));

	// Move removed widget to transient package (engine convention)
	Widget->Rename(nullptr, GetTransientPackage());

	// Remove GUID entries via the engine's canonical callback.
	// Only remove if no other widget with the same name still exists in the tree
	// (handles replacement scenarios, matching engine's DeleteWidgets flow).
	TArray<UWidget*> RemainingWidgets;
	Tree->GetAllWidgets(RemainingWidgets);
	TSet<FName> RemainingNames;
	for (UWidget* W : RemainingWidgets)
	{
		if (W) RemainingNames.Add(W->GetFName());
	}

	for (const FName& Name : RemovedNames)
	{
		if (!RemainingNames.Contains(Name))
		{
			WBP->OnVariableRemoved(Name);
		}
	}

	MarkModifiedAndSave(WBP);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("removed"), WidgetName);
	return Result;
}

// ===========================================================================
// move_widget
// ===========================================================================
TSharedPtr<FJsonObject> FEpicUnrealMCPWidgetCommands::HandleMoveWidget(
	const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintPath, WidgetName, NewParentName;
	if (!Params->TryGetStringField(TEXT("widget_blueprint_path"), BlueprintPath))
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'widget_blueprint_path'"));
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'widget_name'"));
	if (!Params->TryGetStringField(TEXT("new_parent_name"), NewParentName))
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'new_parent_name'"));

	double IndexD = -1.0;
	Params->TryGetNumberField(TEXT("index"), IndexD);
	const int32 Index = static_cast<int32>(IndexD);

	UWidgetBlueprint* WBP = LoadWidgetBlueprint(BlueprintPath);
	if (!WBP)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Widget Blueprint not found: %s"), *BlueprintPath));

	UWidgetTree* Tree = WBP->WidgetTree;
	if (!Tree)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("WidgetTree is null"));

	UWidget* Widget = FindWidgetByName(Tree, WidgetName);
	if (!Widget)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Widget '%s' not found"), *WidgetName));

	if (Widget == Tree->RootWidget)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Cannot move the root widget"));

	UWidget* NewParentWidget = FindWidgetByName(Tree, NewParentName);
	if (!NewParentWidget)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("New parent widget '%s' not found"), *NewParentName));

	UPanelWidget* NewParentPanel = Cast<UPanelWidget>(NewParentWidget);
	if (!NewParentPanel)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("New parent '%s' is not a panel"), *NewParentName));

	// Prevent moving a widget into itself or its own descendants
	UWidget* Ancestor = NewParentWidget;
	while (Ancestor)
	{
		if (Ancestor == Widget)
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				TEXT("Cannot move widget into itself or its descendants"));
		Ancestor = Ancestor->GetParent();
	}

	// Remove from old parent
	UPanelWidget* OldParent = Widget->GetParent();
	FString OldParentName = OldParent ? OldParent->GetName() : TEXT("none");
	if (OldParent)
		OldParent->RemoveChild(Widget);

	// Add to new parent
	UPanelSlot* NewSlot = nullptr;
	if (Index >= 0)
		NewSlot = NewParentPanel->InsertChildAt(Index, Widget);
	else
		NewSlot = NewParentPanel->AddChild(Widget);

	if (!NewSlot)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to add widget to new parent"));

	MarkModifiedAndSave(WBP);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("widget"), WidgetName);
	Result->SetStringField(TEXT("old_parent"), OldParentName);
	Result->SetStringField(TEXT("new_parent"), NewParentName);
	Result->SetStringField(TEXT("slot_class"), NewSlot->GetClass()->GetName());
	return Result;
}

// ===========================================================================
// rename_widget
// ===========================================================================
TSharedPtr<FJsonObject> FEpicUnrealMCPWidgetCommands::HandleRenameWidget(
	const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintPath, WidgetName, NewName;
	if (!Params->TryGetStringField(TEXT("widget_blueprint_path"), BlueprintPath))
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'widget_blueprint_path'"));
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'widget_name'"));
	if (!Params->TryGetStringField(TEXT("new_name"), NewName))
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'new_name'"));

	UWidgetBlueprint* WBP = LoadWidgetBlueprint(BlueprintPath);
	if (!WBP)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Widget Blueprint not found: %s"), *BlueprintPath));

	UWidgetTree* Tree = WBP->WidgetTree;
	if (!Tree)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("WidgetTree is null"));

	UWidget* Widget = FindWidgetByName(Tree, WidgetName);
	if (!Widget)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Widget '%s' not found"), *WidgetName));

	// Check name uniqueness
	if (Tree->FindWidget(FName(*NewName)))
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Widget name '%s' already exists"), *NewName));

	const FName OldFName = Widget->GetFName();
#if WITH_EDITORONLY_DATA
	const bool bWasVariable = Widget->bIsVariable;
#else
	const bool bWasVariable = false;
#endif

	// Rename the UObject
	Widget->Rename(*NewName, Tree, REN_DontCreateRedirectors);

	// Update GUID map via the engine's canonical callback — OnVariableRenamed
	// handles ALL widgets (not just bIsVariable), preserves the existing GUID,
	// and falls back to OnVariableAdded if the old entry was missing.
	WBP->OnVariableRenamed(OldFName, FName(*NewName));

	MarkModifiedAndSave(WBP);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("old_name"), OldFName.ToString());
	Result->SetStringField(TEXT("new_name"), NewName);
	Result->SetBoolField(TEXT("was_variable"), bWasVariable);
	return Result;
}

// ===========================================================================
// duplicate_widget
// ===========================================================================
TSharedPtr<FJsonObject> FEpicUnrealMCPWidgetCommands::HandleDuplicateWidget(
	const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintPath, WidgetName;
	if (!Params->TryGetStringField(TEXT("widget_blueprint_path"), BlueprintPath))
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'widget_blueprint_path'"));
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'widget_name'"));

	FString NewName;
	Params->TryGetStringField(TEXT("new_name"), NewName);

	FString TargetParentName;
	Params->TryGetStringField(TEXT("parent_widget_name"), TargetParentName);

	UWidgetBlueprint* WBP = LoadWidgetBlueprint(BlueprintPath);
	if (!WBP)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Widget Blueprint not found: %s"), *BlueprintPath));

	UWidgetTree* Tree = WBP->WidgetTree;
	if (!Tree)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("WidgetTree is null"));

	UWidget* SourceWidget = FindWidgetByName(Tree, WidgetName);
	if (!SourceWidget)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Widget '%s' not found"), *WidgetName));

	if (SourceWidget == Tree->RootWidget)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Cannot duplicate the root widget"));

	// Determine target parent
	UPanelWidget* TargetParent = nullptr;
	if (!TargetParentName.IsEmpty())
	{
		UWidget* TP = FindWidgetByName(Tree, TargetParentName);
		if (!TP)
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Target parent '%s' not found"), *TargetParentName));
		TargetParent = Cast<UPanelWidget>(TP);
		if (!TargetParent)
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Target parent '%s' is not a panel"), *TargetParentName));
	}
	else
	{
		TargetParent = SourceWidget->GetParent();
		if (!TargetParent)
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				TEXT("Source widget has no parent and no target parent specified"));
	}

	// Generate name if not provided
	if (NewName.IsEmpty())
	{
		int32 Counter = 0;
		do
		{
			NewName = FString::Printf(TEXT("%s_Copy%d"), *WidgetName, Counter++);
		} while (Tree->FindWidget(FName(*NewName)) != nullptr);
	}
	else
	{
		if (Tree->FindWidget(FName(*NewName)))
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Widget name '%s' already exists"), *NewName));
	}

	// Duplicate the widget (deep copy including children)
	UWidget* DuplicatedWidget = DuplicateObject<UWidget>(SourceWidget, Tree, FName(*NewName));
	if (!DuplicatedWidget)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to duplicate widget"));

	// DuplicateObject may set child widgets' Outer to the duplicated widget instead
	// of the WidgetTree. ForEachSourceWidget (used by the compiler's GUID validation)
	// uses ForEachObjectWithOuter(WidgetTree, bIncludeNestedObjects=false), which only
	// finds objects whose DIRECT Outer is the WidgetTree. Re-outer all children.
	UWidgetTree::ForWidgetAndChildren(DuplicatedWidget, [&](UWidget* SubWidget)
	{
		if (SubWidget && SubWidget->GetOuter() != Tree)
		{
			SubWidget->Rename(nullptr, Tree, REN_DontCreateRedirectors);
		}
	});

	// Rename any child widgets that conflict with existing names in the tree
	if (DuplicatedWidget->IsA<UPanelWidget>())
	{
		TArray<UWidget*> ExistingWidgets;
		Tree->GetAllWidgets(ExistingWidgets);
		TSet<FName> ExistingNames;
		for (UWidget* W : ExistingWidgets)
			if (W) ExistingNames.Add(W->GetFName());

		UWidgetTree::ForWidgetAndChildren(DuplicatedWidget, [&](UWidget* SubWidget)
		{
			if (!SubWidget || SubWidget == DuplicatedWidget) return;
			if (ExistingNames.Contains(SubWidget->GetFName()))
			{
				int32 Counter = 0;
				FName UniqueName;
				do
				{
					UniqueName = FName(*FString::Printf(TEXT("%s_Dup%d"),
						*SubWidget->GetName(), Counter++));
				} while (ExistingNames.Contains(UniqueName));

				SubWidget->Rename(*UniqueName.ToString(), Tree, REN_DontCreateRedirectors);
				ExistingNames.Add(UniqueName);
			}
		});
	}

	// Clear the duplicated Slot reference — AddChild will create a fresh one.
	// Without this, the stale duplicated slot remains as a UObject with Outer=Tree.
	DuplicatedWidget->Slot = nullptr;

	// Add to target parent
	UPanelSlot* NewSlot = TargetParent->AddChild(DuplicatedWidget);
	if (!NewSlot)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Failed to add duplicated widget to parent"));

	// Register GUIDs using ForEachSourceWidget — the EXACT same function the
	// compiler's ValidateAndFixUpVariableGuids uses. It calls
	// ForEachObjectWithOuter(WidgetTree, ..., EInternalObjectFlags::Garbage)
	// which excludes garbage/pending-kill objects. Using our own
	// ForEachObjectWithOuter WITHOUT the Garbage flag was registering GUIDs
	// for orphaned objects, causing "Variable was deleted but still has a GUID".
	WBP->Modify();
	WBP->ForEachSourceWidget([&](UWidget* W)
	{
		if (!WBP->WidgetVariableNameToGuidMap.Contains(W->GetFName()))
		{
			WBP->WidgetVariableNameToGuidMap.Add(W->GetFName(), FGuid::NewGuid());
		}
	});

	MarkModifiedAndSave(WBP);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("source"), WidgetName);
	Result->SetStringField(TEXT("new_widget"), NewName);
	Result->SetStringField(TEXT("parent"), TargetParent->GetName());
	Result->SetStringField(TEXT("slot_class"), NewSlot->GetClass()->GetName());
	return Result;
}

// ===========================================================================
// get_widget_properties
// ===========================================================================
TSharedPtr<FJsonObject> FEpicUnrealMCPWidgetCommands::HandleGetWidgetProperties(
	const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintPath, WidgetName;
	if (!Params->TryGetStringField(TEXT("widget_blueprint_path"), BlueprintPath))
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'widget_blueprint_path'"));
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'widget_name'"));

	FString Filter;
	Params->TryGetStringField(TEXT("filter"), Filter);
	bool bIncludeInherited = true;
	Params->TryGetBoolField(TEXT("include_inherited"), bIncludeInherited);

	UWidgetBlueprint* WBP = LoadWidgetBlueprint(BlueprintPath);
	if (!WBP)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Widget Blueprint not found: %s"), *BlueprintPath));

	UWidgetTree* Tree = WBP->WidgetTree;
	if (!Tree)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("WidgetTree is null"));

	UWidget* Widget = FindWidgetByName(Tree, WidgetName);
	if (!Widget)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Widget '%s' not found"), *WidgetName));

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("widget_name"), WidgetName);
	Result->SetStringField(TEXT("widget_class"), Widget->GetClass()->GetName());
	Result->SetObjectField(TEXT("properties"),
		PU::SerializeAllProperties(Widget, Filter.ToLower(), bIncludeInherited));
	return Result;
}

// ===========================================================================
// set_widget_properties
// ===========================================================================
TSharedPtr<FJsonObject> FEpicUnrealMCPWidgetCommands::HandleSetWidgetProperties(
	const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintPath, WidgetName;
	if (!Params->TryGetStringField(TEXT("widget_blueprint_path"), BlueprintPath))
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'widget_blueprint_path'"));
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'widget_name'"));

	const TSharedPtr<FJsonObject>* PropsObj = nullptr;
	if (!Params->TryGetObjectField(TEXT("properties"), PropsObj) || !PropsObj)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'properties' object"));

	UWidgetBlueprint* WBP = LoadWidgetBlueprint(BlueprintPath);
	if (!WBP)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Widget Blueprint not found: %s"), *BlueprintPath));

	UWidgetTree* Tree = WBP->WidgetTree;
	if (!Tree)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("WidgetTree is null"));

	UWidget* Widget = FindWidgetByName(Tree, WidgetName);
	if (!Widget)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Widget '%s' not found"), *WidgetName));

	Widget->Modify();

	TArray<FString> Set, Failed;
	for (const auto& Pair : (*PropsObj)->Values)
	{
		FString Err;
		if (PU::SetProperty(Widget, Pair.Key, Pair.Value, Err))
			Set.Add(Pair.Key);
		else
			Failed.Add(FString::Printf(TEXT("%s: %s"), *Pair.Key, *Err));
	}

	MarkPropertyModifiedAndSave(WBP);

	TArray<TSharedPtr<FJsonValue>> SetArr, FailArr;
	for (const FString& S : Set)    SetArr.Add(MakeShared<FJsonValueString>(S));
	for (const FString& F : Failed) FailArr.Add(MakeShared<FJsonValueString>(F));

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), Failed.IsEmpty());
	Result->SetStringField(TEXT("widget_name"), WidgetName);
	Result->SetNumberField(TEXT("set_count"), Set.Num());
	Result->SetNumberField(TEXT("failed_count"), Failed.Num());
	Result->SetArrayField(TEXT("set"), SetArr);
	if (!FailArr.IsEmpty()) Result->SetArrayField(TEXT("failed"), FailArr);
	return Result;
}

// ===========================================================================
// get_slot_properties
// ===========================================================================
TSharedPtr<FJsonObject> FEpicUnrealMCPWidgetCommands::HandleGetSlotProperties(
	const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintPath, WidgetName;
	if (!Params->TryGetStringField(TEXT("widget_blueprint_path"), BlueprintPath))
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'widget_blueprint_path'"));
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'widget_name'"));

	FString Filter;
	Params->TryGetStringField(TEXT("filter"), Filter);

	UWidgetBlueprint* WBP = LoadWidgetBlueprint(BlueprintPath);
	if (!WBP)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Widget Blueprint not found: %s"), *BlueprintPath));

	UWidgetTree* Tree = WBP->WidgetTree;
	if (!Tree)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("WidgetTree is null"));

	UWidget* Widget = FindWidgetByName(Tree, WidgetName);
	if (!Widget)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Widget '%s' not found"), *WidgetName));

	if (!Widget->Slot)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Widget '%s' has no slot (root widget?)"), *WidgetName));

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("widget_name"), WidgetName);
	Result->SetStringField(TEXT("slot_class"), Widget->Slot->GetClass()->GetName());
	// Use safe serializer that filters to editable properties and guards
	// enum values — PU::SerializeAllProperties crashes on internal Slate
	// properties with invalid enum FName entries.
	TSharedPtr<FJsonObject> SlotProps = SerializeSlotProperties(Widget->Slot);

	// Apply filter if provided
	if (!Filter.IsEmpty() && SlotProps.IsValid())
	{
		const FString FilterLower = Filter.ToLower();
		TSharedPtr<FJsonObject> Filtered = MakeShared<FJsonObject>();
		for (const auto& Pair : SlotProps->Values)
		{
			if (Pair.Key.ToLower().Contains(FilterLower))
				Filtered->SetField(Pair.Key, Pair.Value);
		}
		SlotProps = Filtered;
	}

	Result->SetObjectField(TEXT("properties"), SlotProps);
	return Result;
}

// ===========================================================================
// set_slot_properties
// ===========================================================================
TSharedPtr<FJsonObject> FEpicUnrealMCPWidgetCommands::HandleSetSlotProperties(
	const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintPath, WidgetName;
	if (!Params->TryGetStringField(TEXT("widget_blueprint_path"), BlueprintPath))
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'widget_blueprint_path'"));
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'widget_name'"));

	const TSharedPtr<FJsonObject>* PropsObj = nullptr;
	if (!Params->TryGetObjectField(TEXT("properties"), PropsObj) || !PropsObj)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'properties' object"));

	UWidgetBlueprint* WBP = LoadWidgetBlueprint(BlueprintPath);
	if (!WBP)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Widget Blueprint not found: %s"), *BlueprintPath));

	UWidgetTree* Tree = WBP->WidgetTree;
	if (!Tree)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("WidgetTree is null"));

	UWidget* Widget = FindWidgetByName(Tree, WidgetName);
	if (!Widget)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Widget '%s' not found"), *WidgetName));

	if (!Widget->Slot)
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Widget '%s' has no slot (root widget?)"), *WidgetName));

	Widget->Slot->Modify();

	TArray<FString> Set, Failed;
	for (const auto& Pair : (*PropsObj)->Values)
	{
		FString Err;
		if (PU::SetProperty(Widget->Slot, Pair.Key, Pair.Value, Err))
			Set.Add(Pair.Key);
		else
			Failed.Add(FString::Printf(TEXT("%s: %s"), *Pair.Key, *Err));
	}

	Widget->Slot->SynchronizeProperties();
	MarkPropertyModifiedAndSave(WBP);

	TArray<TSharedPtr<FJsonValue>> SetArr, FailArr;
	for (const FString& S : Set)    SetArr.Add(MakeShared<FJsonValueString>(S));
	for (const FString& F : Failed) FailArr.Add(MakeShared<FJsonValueString>(F));

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), Failed.IsEmpty());
	Result->SetStringField(TEXT("widget_name"), WidgetName);
	Result->SetStringField(TEXT("slot_class"), Widget->Slot->GetClass()->GetName());
	Result->SetNumberField(TEXT("set_count"), Set.Num());
	Result->SetNumberField(TEXT("failed_count"), Failed.Num());
	Result->SetArrayField(TEXT("set"), SetArr);
	if (!FailArr.IsEmpty()) Result->SetArrayField(TEXT("failed"), FailArr);
	return Result;
}

// ===========================================================================
// list_widget_types
// ===========================================================================
TSharedPtr<FJsonObject> FEpicUnrealMCPWidgetCommands::HandleListWidgetTypes(
	const TSharedPtr<FJsonObject>& Params)
{
	FString Filter;
	Params->TryGetStringField(TEXT("filter"), Filter);
	const FString FilterLower = Filter.ToLower();

	bool bIncludeAbstract = false;
	Params->TryGetBoolField(TEXT("include_abstract"), bIncludeAbstract);

	bool bPanelsOnly = false;
	Params->TryGetBoolField(TEXT("panels_only"), bPanelsOnly);

	TArray<UClass*> Derived;
	GetDerivedClasses(UWidget::StaticClass(), Derived, true);

	TArray<TSharedPtr<FJsonValue>> TypeArray;
	for (UClass* C : Derived)
	{
		if (!C) continue;
		if (!bIncludeAbstract && C->HasAnyClassFlags(CLASS_Abstract)) continue;
		if (C->HasAnyClassFlags(CLASS_Deprecated)) continue;

		const bool bIsPanel = C->IsChildOf(UPanelWidget::StaticClass());
		if (bPanelsOnly && !bIsPanel) continue;

		const FString Name = C->GetName();
		if (!FilterLower.IsEmpty() && !Name.ToLower().Contains(FilterLower))
			continue;

		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("name"), Name);
		Obj->SetStringField(TEXT("path"), C->GetPathName());
		Obj->SetStringField(TEXT("parent"),
			C->GetSuperClass() ? C->GetSuperClass()->GetName() : TEXT("None"));
		Obj->SetBoolField(TEXT("is_panel"), bIsPanel);
		Obj->SetBoolField(TEXT("is_abstract"), C->HasAnyClassFlags(CLASS_Abstract));
		TypeArray.Add(MakeShared<FJsonValueObject>(Obj));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetNumberField(TEXT("count"), TypeArray.Num());
	Result->SetArrayField(TEXT("widget_types"), TypeArray);
	if (!FilterLower.IsEmpty())
		Result->SetStringField(TEXT("filter"), Filter);
	if (bPanelsOnly)
		Result->SetBoolField(TEXT("panels_only"), true);
	return Result;
}
