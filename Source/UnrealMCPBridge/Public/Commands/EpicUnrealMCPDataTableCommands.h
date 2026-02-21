#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler for Data Table MCP commands.
 * Native C++ data table reading and editing.
 */
class UNREALMCPBRIDGE_API FEpicUnrealMCPDataTableCommands
{
public:
	FEpicUnrealMCPDataTableCommands();

	TSharedPtr<FJsonObject> HandleCommand(
		const FString& CommandType,
		const TSharedPtr<FJsonObject>& Params);

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
	static bool PopulateRowFromJson(
		const TSharedPtr<FJsonObject>& JsonData,
		uint8* RowData, const UScriptStruct* RowStruct,
		TArray<FString>& OutErrors);
};
