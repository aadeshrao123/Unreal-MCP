#include "Commands/EpicUnrealMCPDataTableCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"

#include "Engine/DataTable.h"
#include "DataTableEditorUtils.h"
#include "EditorAssetLibrary.h"
#include "JsonObjectConverter.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

FEpicUnrealMCPDataTableCommands::FEpicUnrealMCPDataTableCommands()
{
}

TSharedPtr<FJsonObject> FEpicUnrealMCPDataTableCommands::HandleCommand(
	const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
	if (CommandType == TEXT("get_data_table_rows"))
	{
		return HandleGetDataTableRows(Params);
	}
	else if (CommandType == TEXT("get_data_table_row"))
	{
		return HandleGetDataTableRow(Params);
	}
	else if (CommandType == TEXT("get_data_table_schema"))
	{
		return HandleGetDataTableSchema(Params);
	}
	else if (CommandType == TEXT("add_data_table_row"))
	{
		return HandleAddDataTableRow(Params);
	}
	else if (CommandType == TEXT("update_data_table_row"))
	{
		return HandleUpdateDataTableRow(Params);
	}
	else if (CommandType == TEXT("delete_data_table_row"))
	{
		return HandleDeleteDataTableRow(Params);
	}
	else if (CommandType == TEXT("duplicate_data_table_row"))
	{
		return HandleDuplicateDataTableRow(Params);
	}
	else if (CommandType == TEXT("rename_data_table_row"))
	{
		return HandleRenameDataTableRow(Params);
	}

	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
		FString::Printf(TEXT("Unknown data table command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPDataTableCommands::RowToJson(
	FName RowName, const uint8* RowData, const UScriptStruct* RowStruct)
{
	TSharedPtr<FJsonObject> RowJson = MakeShared<FJsonObject>();
	RowJson->SetStringField(TEXT("name"), RowName.ToString());

	TSharedPtr<FJsonObject> DataJson = MakeShared<FJsonObject>();
	for (TFieldIterator<FProperty> PropIt(RowStruct); PropIt; ++PropIt)
	{
		FProperty* Prop = *PropIt;
		const void* PropData = Prop->ContainerPtrToValuePtr<void>(RowData);

		// UE5.5: UPropertyToJsonValue returns by value
		TSharedPtr<FJsonValue> JsonValue = FJsonObjectConverter::UPropertyToJsonValue(Prop, PropData);
		if (JsonValue.IsValid())
		{
			DataJson->SetField(Prop->GetName(), JsonValue);
		}
	}
	RowJson->SetObjectField(TEXT("data"), DataJson);
	return RowJson;
}

bool FEpicUnrealMCPDataTableCommands::PopulateRowFromJson(
	const TSharedPtr<FJsonObject>& JsonData,
	uint8* RowData, const UScriptStruct* RowStruct,
	TArray<FString>& OutErrors)
{
	bool bAnySet = false;
	for (TFieldIterator<FProperty> PropIt(RowStruct); PropIt; ++PropIt)
	{
		FProperty* Prop = *PropIt;
		TSharedPtr<FJsonValue> JsonVal = JsonData->TryGetField(Prop->GetName());
		if (!JsonVal.IsValid())
		{
			continue;
		}

		void* PropData = Prop->ContainerPtrToValuePtr<void>(RowData);
		if (!FJsonObjectConverter::JsonValueToUProperty(JsonVal, Prop, PropData, 0, 0))
		{
			OutErrors.Add(FString::Printf(TEXT("Failed to set property '%s'"), *Prop->GetName()));
		}
		else
		{
			bAnySet = true;
		}
	}
	return bAnySet;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPDataTableCommands::HandleGetDataTableRows(
	const TSharedPtr<FJsonObject>& Params)
{
	FString DataTablePath;
	if (!Params->TryGetStringField(TEXT("data_table_path"), DataTablePath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'data_table_path' parameter"));
	}

	UObject* Asset = UEditorAssetLibrary::LoadAsset(DataTablePath);
	UDataTable* DataTable = Cast<UDataTable>(Asset);
	if (!DataTable)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Data table not found: %s"), *DataTablePath));
	}

	const UScriptStruct* RowStruct = DataTable->GetRowStruct();
	if (!RowStruct)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Data table has no row struct"));
	}

	const TMap<FName, uint8*>& RowMap = DataTable->GetRowMap();
	TArray<TSharedPtr<FJsonValue>> RowsArray;
	for (auto& [RowName, RowData] : RowMap)
	{
		RowsArray.Add(MakeShared<FJsonValueObject>(RowToJson(RowName, RowData, RowStruct)));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("path"), DataTablePath);
	Result->SetStringField(TEXT("row_struct"), RowStruct->GetName());
	Result->SetNumberField(TEXT("row_count"), RowsArray.Num());
	Result->SetArrayField(TEXT("rows"), RowsArray);
	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPDataTableCommands::HandleGetDataTableRow(
	const TSharedPtr<FJsonObject>& Params)
{
	FString DataTablePath, RowName;
	if (!Params->TryGetStringField(TEXT("data_table_path"), DataTablePath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'data_table_path' parameter"));
	}
	if (!Params->TryGetStringField(TEXT("row_name"), RowName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'row_name' parameter"));
	}

	UObject* Asset = UEditorAssetLibrary::LoadAsset(DataTablePath);
	UDataTable* DataTable = Cast<UDataTable>(Asset);
	if (!DataTable)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Data table not found: %s"), *DataTablePath));
	}

	const UScriptStruct* RowStruct = DataTable->GetRowStruct();
	FName RowFName(*RowName);
	uint8* RowData = DataTable->FindRowUnchecked(RowFName);
	if (!RowData)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Row '%s' not found in '%s'"), *RowName, *DataTablePath));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("path"), DataTablePath);
	Result->SetObjectField(TEXT("row"), RowToJson(RowFName, RowData, RowStruct));
	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPDataTableCommands::HandleGetDataTableSchema(
	const TSharedPtr<FJsonObject>& Params)
{
	FString DataTablePath;
	if (!Params->TryGetStringField(TEXT("data_table_path"), DataTablePath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'data_table_path' parameter"));
	}

	UObject* Asset = UEditorAssetLibrary::LoadAsset(DataTablePath);
	UDataTable* DataTable = Cast<UDataTable>(Asset);
	if (!DataTable)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Data table not found: %s"), *DataTablePath));
	}

	const UScriptStruct* RowStruct = DataTable->GetRowStruct();
	if (!RowStruct)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Data table has no row struct"));
	}

	TArray<TSharedPtr<FJsonValue>> Columns;
	for (TFieldIterator<FProperty> PropIt(RowStruct); PropIt; ++PropIt)
	{
		FProperty* Prop = *PropIt;

		TSharedPtr<FJsonObject> ColObj = MakeShared<FJsonObject>();
		ColObj->SetStringField(TEXT("name"), Prop->GetName());
		ColObj->SetStringField(TEXT("type"), Prop->GetClass()->GetName());
		ColObj->SetStringField(TEXT("cpp_type"), Prop->GetCPPType());

		Columns.Add(MakeShared<FJsonValueObject>(ColObj));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("path"), DataTablePath);
	Result->SetStringField(TEXT("row_struct"), RowStruct->GetName());
	Result->SetNumberField(TEXT("row_count"), DataTable->GetRowMap().Num());
	Result->SetArrayField(TEXT("columns"), Columns);
	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPDataTableCommands::HandleAddDataTableRow(
	const TSharedPtr<FJsonObject>& Params)
{
	FString DataTablePath, RowName;
	if (!Params->TryGetStringField(TEXT("data_table_path"), DataTablePath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'data_table_path' parameter"));
	}
	if (!Params->TryGetStringField(TEXT("row_name"), RowName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'row_name' parameter"));
	}

	UObject* Asset = UEditorAssetLibrary::LoadAsset(DataTablePath);
	UDataTable* DataTable = Cast<UDataTable>(Asset);
	if (!DataTable)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Data table not found: %s"), *DataTablePath));
	}

	FName RowFName(*RowName);

	if (DataTable->FindRowUnchecked(RowFName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Row '%s' already exists"), *RowName));
	}

	if (!FDataTableEditorUtils::AddRow(DataTable, RowFName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Failed to add row '%s'"), *RowName));
	}

	TArray<FString> Errors;
	const TSharedPtr<FJsonObject>* DataJsonPtr;
	if (Params->TryGetObjectField(TEXT("data"), DataJsonPtr) && DataJsonPtr->IsValid())
	{
		uint8* RowData = DataTable->FindRowUnchecked(RowFName);
		if (RowData)
		{
			PopulateRowFromJson(*DataJsonPtr, RowData, DataTable->GetRowStruct(), Errors);
		}
	}

	DataTable->MarkPackageDirty();
	UEditorAssetLibrary::SaveAsset(DataTablePath);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("row_name"), RowName);

	TArray<TSharedPtr<FJsonValue>> ErrorArray;
	for (const FString& Err : Errors)
	{
		ErrorArray.Add(MakeShared<FJsonValueString>(Err));
	}
	Result->SetArrayField(TEXT("errors"), ErrorArray);
	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPDataTableCommands::HandleUpdateDataTableRow(
	const TSharedPtr<FJsonObject>& Params)
{
	FString DataTablePath, RowName;
	if (!Params->TryGetStringField(TEXT("data_table_path"), DataTablePath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'data_table_path' parameter"));
	}
	if (!Params->TryGetStringField(TEXT("row_name"), RowName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'row_name' parameter"));
	}
	const TSharedPtr<FJsonObject>* DataJsonPtr;
	if (!Params->TryGetObjectField(TEXT("data"), DataJsonPtr))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'data' object parameter"));
	}

	UObject* Asset = UEditorAssetLibrary::LoadAsset(DataTablePath);
	UDataTable* DataTable = Cast<UDataTable>(Asset);
	if (!DataTable)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Data table not found: %s"), *DataTablePath));
	}

	FName RowFName(*RowName);
	uint8* RowData = DataTable->FindRowUnchecked(RowFName);
	if (!RowData)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Row '%s' not found in '%s'"), *RowName, *DataTablePath));
	}

	// Notify editor before mutating so the data table UI stays in sync
	FDataTableEditorUtils::BroadcastPreChange(DataTable, FDataTableEditorUtils::EDataTableChangeInfo::RowData);

	TArray<FString> Errors;
	PopulateRowFromJson(*DataJsonPtr, RowData, DataTable->GetRowStruct(), Errors);

	FDataTableEditorUtils::BroadcastPostChange(DataTable, FDataTableEditorUtils::EDataTableChangeInfo::RowData);

	DataTable->MarkPackageDirty();
	UEditorAssetLibrary::SaveAsset(DataTablePath);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("row_name"), RowName);

	TArray<TSharedPtr<FJsonValue>> ErrorArray;
	for (const FString& Err : Errors)
	{
		ErrorArray.Add(MakeShared<FJsonValueString>(Err));
	}
	Result->SetArrayField(TEXT("errors"), ErrorArray);
	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPDataTableCommands::HandleDeleteDataTableRow(
	const TSharedPtr<FJsonObject>& Params)
{
	FString DataTablePath, RowName;
	if (!Params->TryGetStringField(TEXT("data_table_path"), DataTablePath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'data_table_path' parameter"));
	}
	if (!Params->TryGetStringField(TEXT("row_name"), RowName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'row_name' parameter"));
	}

	UObject* Asset = UEditorAssetLibrary::LoadAsset(DataTablePath);
	UDataTable* DataTable = Cast<UDataTable>(Asset);
	if (!DataTable)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Data table not found: %s"), *DataTablePath));
	}

	FName RowFName(*RowName);
	if (!DataTable->FindRowUnchecked(RowFName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Row '%s' not found in '%s'"), *RowName, *DataTablePath));
	}

	if (!FDataTableEditorUtils::RemoveRow(DataTable, RowFName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Failed to remove row '%s'"), *RowName));
	}

	DataTable->MarkPackageDirty();
	UEditorAssetLibrary::SaveAsset(DataTablePath);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("deleted_row"), RowName);
	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPDataTableCommands::HandleDuplicateDataTableRow(
	const TSharedPtr<FJsonObject>& Params)
{
	FString DataTablePath, SourceRowName, NewRowName;
	if (!Params->TryGetStringField(TEXT("data_table_path"), DataTablePath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'data_table_path' parameter"));
	}
	if (!Params->TryGetStringField(TEXT("source_row_name"), SourceRowName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'source_row_name' parameter"));
	}
	if (!Params->TryGetStringField(TEXT("new_row_name"), NewRowName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'new_row_name' parameter"));
	}

	UObject* Asset = UEditorAssetLibrary::LoadAsset(DataTablePath);
	UDataTable* DataTable = Cast<UDataTable>(Asset);
	if (!DataTable)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Data table not found: %s"), *DataTablePath));
	}

	FName SourceFName(*SourceRowName);
	FName NewFName(*NewRowName);

	uint8* SourceData = DataTable->FindRowUnchecked(SourceFName);
	if (!SourceData)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Source row '%s' not found"), *SourceRowName));
	}
	if (DataTable->FindRowUnchecked(NewFName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Row '%s' already exists"), *NewRowName));
	}

	// Add empty row then binary-copy the source struct into it
	if (!FDataTableEditorUtils::AddRow(DataTable, NewFName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Failed to create row '%s'"), *NewRowName));
	}

	uint8* NewData = DataTable->FindRowUnchecked(NewFName);
	if (NewData)
	{
		const UScriptStruct* RowStruct = DataTable->GetRowStruct();
		RowStruct->CopyScriptStruct(NewData, SourceData);
	}

	DataTable->MarkPackageDirty();
	UEditorAssetLibrary::SaveAsset(DataTablePath);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("source_row"), SourceRowName);
	Result->SetStringField(TEXT("new_row"), NewRowName);
	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPDataTableCommands::HandleRenameDataTableRow(
	const TSharedPtr<FJsonObject>& Params)
{
	FString DataTablePath, OldRowName, NewRowName;
	if (!Params->TryGetStringField(TEXT("data_table_path"), DataTablePath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'data_table_path' parameter"));
	}
	if (!Params->TryGetStringField(TEXT("old_row_name"), OldRowName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'old_row_name' parameter"));
	}
	if (!Params->TryGetStringField(TEXT("new_row_name"), NewRowName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'new_row_name' parameter"));
	}

	UObject* Asset = UEditorAssetLibrary::LoadAsset(DataTablePath);
	UDataTable* DataTable = Cast<UDataTable>(Asset);
	if (!DataTable)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Data table not found: %s"), *DataTablePath));
	}

	FName OldFName(*OldRowName);
	FName NewFName(*NewRowName);

	if (!DataTable->FindRowUnchecked(OldFName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Row '%s' not found"), *OldRowName));
	}
	if (DataTable->FindRowUnchecked(NewFName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Row '%s' already exists"), *NewRowName));
	}

	if (!FDataTableEditorUtils::RenameRow(DataTable, OldFName, NewFName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Failed to rename row '%s' to '%s'"), *OldRowName, *NewRowName));
	}

	DataTable->MarkPackageDirty();
	UEditorAssetLibrary::SaveAsset(DataTablePath);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("old_row_name"), OldRowName);
	Result->SetStringField(TEXT("new_row_name"), NewRowName);
	return Result;
}
