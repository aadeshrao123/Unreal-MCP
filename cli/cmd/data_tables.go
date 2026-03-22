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
		Params: []ParamSpec{
			{Name: "data_table_path", Type: "string", Required: true, Help: "Path to the data table"},
		},
	},
	{
		Name:  "get_data_table_row",
		Group: "datatables",
		Short: "Get a single row by name",
		Params: []ParamSpec{
			{Name: "data_table_path", Type: "string", Required: true, Help: "Path to the data table"},
			{Name: "row_name", Type: "string", Required: true, Help: "Row name"},
		},
	},
	{
		Name:  "get_data_table_schema",
		Group: "datatables",
		Short: "Get column names and types from row struct",
		Params: []ParamSpec{
			{Name: "data_table_path", Type: "string", Required: true, Help: "Path to the data table"},
		},
	},
	{
		Name:  "add_data_table_row",
		Group: "datatables",
		Short: "Add a new row with optional initial data",
		Params: []ParamSpec{
			{Name: "data_table_path", Type: "string", Required: true, Help: "Path to the data table"},
			{Name: "row_name", Type: "string", Required: true, Help: "Row name"},
			{Name: "data", Type: "json", Help: "JSON object of field values"},
		},
	},
	{
		Name:  "update_data_table_row",
		Group: "datatables",
		Short: "Update specific fields on an existing row",
		Params: []ParamSpec{
			{Name: "data_table_path", Type: "string", Required: true, Help: "Path to the data table"},
			{Name: "row_name", Type: "string", Required: true, Help: "Row name"},
			{Name: "data", Type: "json", Required: true, Help: "JSON object of field values to update"},
		},
	},
	{
		Name:  "delete_data_table_row",
		Group: "datatables",
		Short: "Delete a row by name",
		Params: []ParamSpec{
			{Name: "data_table_path", Type: "string", Required: true, Help: "Path to the data table"},
			{Name: "row_name", Type: "string", Required: true, Help: "Row name to delete"},
		},
	},
	{
		Name:  "rename_data_table_row",
		Group: "datatables",
		Short: "Rename a row in-place",
		Params: []ParamSpec{
			{Name: "data_table_path", Type: "string", Required: true, Help: "Path to the data table"},
			{Name: "old_row_name", Type: "string", Required: true, Help: "Current row name"},
			{Name: "new_row_name", Type: "string", Required: true, Help: "New row name"},
		},
	},
	{
		Name:  "duplicate_data_table_row",
		Group: "datatables",
		Short: "Copy a row under a new name",
		Params: []ParamSpec{
			{Name: "data_table_path", Type: "string", Required: true, Help: "Path to the data table"},
			{Name: "source_row_name", Type: "string", Required: true, Help: "Source row name"},
			{Name: "new_row_name", Type: "string", Required: true, Help: "New row name"},
		},
	},
}
