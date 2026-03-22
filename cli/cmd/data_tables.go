package cmd

func init() {
	ensureGroup("datatables", "Data Tables")
	registerCommands(dataTableCommands)
}

var dataTableCommands = []CommandSpec{
	{
		Name:  "get_data_table_rows",
		Group: "datatables",
		Short: "Get all rows from a Data Table",
		Long:  "Returns every row in a Data Table with all field values. Use get_data_table_schema first to understand column types.",
		Example: "ue-cli get_data_table_rows --data-table-path /Game/Data/DT_Items",
		Params: []ParamSpec{
			{Name: "data_table_path", Type: "string", Required: true, Help: "Content path to the data table"},
		},
	},
	{
		Name:  "get_data_table_row",
		Group: "datatables",
		Short: "Get a single row by name",
		Long:  "Returns one row from a Data Table by its row name. More efficient than get_data_table_rows when you only need one row.",
		Example: `ue-cli get_data_table_row --data-table-path /Game/Data/DT_Items --row-name "IronOre"`,
		Params: []ParamSpec{
			{Name: "data_table_path", Type: "string", Required: true, Help: "Content path to the data table"},
			{Name: "row_name", Type: "string", Required: true, Help: "Row name to look up"},
		},
	},
	{
		Name:  "get_data_table_schema",
		Group: "datatables",
		Short: "Get column names and types from row struct",
		Long:  "Returns the struct definition behind a Data Table: column names, types, and default values. Always call this before adding or updating rows to understand the expected field types.",
		Example: "ue-cli get_data_table_schema --data-table-path /Game/Data/DT_Items",
		Params: []ParamSpec{
			{Name: "data_table_path", Type: "string", Required: true, Help: "Content path to the data table"},
		},
	},
	{
		Name:  "add_data_table_row",
		Group: "datatables",
		Short: "Add a new row with optional initial data",
		Long:  "Adds a new row to a Data Table. The data parameter is a JSON string of field name-value pairs matching the table's schema. Auto-saves the table.",
		Example: `ue-cli add_data_table_row --data-table-path /Game/Data/DT_Items --row-name "CopperOre" --data '{"DisplayName":"Copper Ore","StackSize":100,"ItemForm":"Solid"}'`,
		Params: []ParamSpec{
			{Name: "data_table_path", Type: "string", Required: true, Help: "Content path to the data table"},
			{Name: "row_name", Type: "string", Required: true, Help: "New row name (must be unique)"},
			{Name: "data", Type: "json", Help: "JSON object of field values (check schema first)"},
		},
	},
	{
		Name:  "update_data_table_row",
		Group: "datatables",
		Short: "Update specific fields on an existing row",
		Long:  "Updates one or more fields on an existing Data Table row. Only the fields specified in data are modified; other fields keep their current values. Auto-saves.",
		Example: `ue-cli update_data_table_row --data-table-path /Game/Data/DT_Items --row-name "IronOre" --data '{"StackSize":200}'`,
		Params: []ParamSpec{
			{Name: "data_table_path", Type: "string", Required: true, Help: "Content path to the data table"},
			{Name: "row_name", Type: "string", Required: true, Help: "Row name to update"},
			{Name: "data", Type: "json", Required: true, Help: "JSON object of field values to update"},
		},
	},
	{
		Name:  "delete_data_table_row",
		Group: "datatables",
		Short: "Delete a row by name",
		Long:  "Removes a row from a Data Table. Auto-saves. Cannot be undone.",
		Example: `ue-cli delete_data_table_row --data-table-path /Game/Data/DT_Items --row-name "OldItem"`,
		Params: []ParamSpec{
			{Name: "data_table_path", Type: "string", Required: true, Help: "Content path to the data table"},
			{Name: "row_name", Type: "string", Required: true, Help: "Row name to delete"},
		},
	},
	{
		Name:  "rename_data_table_row",
		Group: "datatables",
		Short: "Rename a row in-place",
		Long:  "Changes the name of an existing Data Table row while preserving all its data and order. Auto-saves.",
		Example: `ue-cli rename_data_table_row --data-table-path /Game/Data/DT_Items --old-row-name "Item1" --new-row-name "IronIngot"`,
		Params: []ParamSpec{
			{Name: "data_table_path", Type: "string", Required: true, Help: "Content path to the data table"},
			{Name: "old_row_name", Type: "string", Required: true, Help: "Current row name"},
			{Name: "new_row_name", Type: "string", Required: true, Help: "New row name"},
		},
	},
	{
		Name:  "duplicate_data_table_row",
		Group: "datatables",
		Short: "Copy a row under a new name",
		Long:  "Creates a copy of an existing row with a new name, keeping all field values identical. Useful for creating similar items with minor variations.",
		Example: `ue-cli duplicate_data_table_row --data-table-path /Game/Data/DT_Items --source-row-name "IronOre" --new-row-name "CopperOre"`,
		Params: []ParamSpec{
			{Name: "data_table_path", Type: "string", Required: true, Help: "Content path to the data table"},
			{Name: "source_row_name", Type: "string", Required: true, Help: "Source row name to copy from"},
			{Name: "new_row_name", Type: "string", Required: true, Help: "New row name for the copy"},
		},
	},
}
