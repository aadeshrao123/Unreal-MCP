#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for Data Table MCP commands.
 * Implements native C++ data table reading and editing
 * without going through the Python scripting plugin.
 *
 * Commands:
 *   get_data_table_rows      - Read all rows with their field data
 *   get_data_table_row       - Read a single row by name
 *   get_data_table_schema    - Get column names and types from the row struct
 *   add_data_table_row       - Add a new row (with optional initial data)
 *   update_data_table_row    - Update fields on an existing row
 *   delete_data_table_row    - Remove a row by name
 *   duplicate_data_table_row - Copy a row under a new name
 *   rename_data_table_row    - Rename a row in-place (preserves order)
 */
class UNREALMCPBRIDGE_API FEpicUnrealMCPDataTableCommands
{
public:
	FEpicUnrealMCPDataTableCommands();

	TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
	TSharedPtr<FJsonObject> HandleGetDataTableRows(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetDataTableRow(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetDataTableSchema(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAddDataTableRow(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleUpdateDataTableRow(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleDeleteDataTableRow(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleDuplicateDataTableRow(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleRenameDataTableRow(const TSharedPtr<FJsonObject>& Params);

	// Serialises one row's data into a JSON object keyed by field name
	static TSharedPtr<FJsonObject> RowToJson(FName RowName, const uint8* RowData, const UScriptStruct* RowStruct);

	// Writes JSON fields into an allocated row buffer; returns false if nothing was set
	static bool PopulateRowFromJson(const TSharedPtr<FJsonObject>& JsonData,
	                                uint8* RowData, const UScriptStruct* RowStruct,
	                                TArray<FString>& OutErrors);
};
