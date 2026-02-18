#pragma once

#include "CoreMinimal.h"
#include "Json.h"
#include "Commands/EpicUnrealMCPPropertyUtils.h"

class UWidgetBlueprint;
class UWidgetTree;
class UWidget;
class UPanelWidget;
class UPanelSlot;

/**
 * Handler class for Widget Blueprint Designer-tree MCP commands.
 *
 * Provides full CRUD operations on the UMG widget hierarchy inside
 * Widget Blueprints:
 *   - Read the complete widget tree as JSON
 *   - Add / remove / move / rename / duplicate widgets
 *   - Read / write widget properties via FEpicUnrealMCPPropertyUtils
 *   - Read / write slot (layout) properties for any panel type
 *   - Enumerate available UWidget subclasses
 */
class UNREALMCPBRIDGE_API FEpicUnrealMCPWidgetCommands
{
public:
	FEpicUnrealMCPWidgetCommands();

	TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType,
	                                       const TSharedPtr<FJsonObject>& Params);

private:
	// -----------------------------------------------------------------
	// Widget Tree CRUD
	// -----------------------------------------------------------------
	TSharedPtr<FJsonObject> HandleGetWidgetTree(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAddWidget(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleRemoveWidget(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleMoveWidget(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleRenameWidget(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleDuplicateWidget(const TSharedPtr<FJsonObject>& Params);

	// -----------------------------------------------------------------
	// Widget / Slot Properties
	// -----------------------------------------------------------------
	TSharedPtr<FJsonObject> HandleGetWidgetProperties(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetWidgetProperties(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetSlotProperties(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetSlotProperties(const TSharedPtr<FJsonObject>& Params);

	// -----------------------------------------------------------------
	// Utility
	// -----------------------------------------------------------------
	TSharedPtr<FJsonObject> HandleListWidgetTypes(const TSharedPtr<FJsonObject>& Params);

	// -----------------------------------------------------------------
	// Internal helpers
	// -----------------------------------------------------------------
	static UWidgetBlueprint* LoadWidgetBlueprint(const FString& Path);
	static UWidget* FindWidgetByName(UWidgetTree* Tree, const FString& Name);
	static UClass* ResolveWidgetClass(const FString& ClassName);
	static TSharedPtr<FJsonObject> SerializeWidgetRecursive(UWidget* Widget);
	static TSharedPtr<FJsonObject> SerializeSlotProperties(UPanelSlot* Slot);
	static void MarkModifiedAndSave(UWidgetBlueprint* WBP);
};
