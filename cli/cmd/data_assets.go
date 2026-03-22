package cmd

func init() {
	ensureGroup("dataassets", "Data Assets")
	registerCommands(dataAssetCommands)
}

var dataAssetCommands = []CommandSpec{
	{
		Name:  "list_data_asset_classes",
		Group: "dataassets",
		Short: "List all loaded UDataAsset subclasses",
		Params: []ParamSpec{
			{Name: "filter", Type: "string", Help: "Filter by class name"},
			{Name: "include_abstract", Type: "bool", Default: false, Help: "Include abstract classes"},
		},
	},
	{
		Name:  "create_data_asset",
		Group: "dataassets",
		Short: "Create a new UDataAsset subclass instance",
		Params: []ParamSpec{
			{Name: "class_name", Type: "string", Required: true, Help: "Data asset class name"},
			{Name: "asset_path", Type: "string", Required: true, Help: "Asset path to create at"},
			{Name: "initial_properties", Type: "json", Help: "JSON object of initial property values"},
		},
	},
	{
		Name:  "get_data_asset_properties",
		Group: "dataassets",
		Short: "Read all properties of a data asset",
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Asset path"},
			{Name: "filter", Type: "string", Help: "Filter property names"},
			{Name: "include_inherited", Type: "bool", Default: true, Help: "Include inherited properties"},
		},
	},
	{
		Name:  "set_data_asset_property",
		Group: "dataassets",
		Short: "Set a single property on a data asset",
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Asset path"},
			{Name: "property_name", Type: "string", Required: true, Help: "Property name"},
			{Name: "property_value", Type: "json", Required: true, Help: "Property value (JSON)"},
		},
	},
	{
		Name:  "set_data_asset_properties",
		Group: "dataassets",
		Short: "Set multiple properties on a data asset in one call",
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Asset path"},
			{Name: "properties", Type: "json", Required: true, Help: "JSON object of property name-value pairs"},
		},
	},
	{
		Name:  "get_property_valid_types",
		Group: "dataassets",
		Short: "Query valid dropdown values for a property slot",
		Params: []ParamSpec{
			{Name: "class_name", Type: "string", Required: true, Help: "Class name"},
			{Name: "property_path", Type: "string", Required: true, Help: "Property path"},
			{Name: "filter", Type: "string", Help: "Filter results (recommended to avoid huge responses)"},
			{Name: "include_abstract", Type: "bool", Default: false, Help: "Include abstract types"},
		},
	},
	{
		Name:  "search_class_paths",
		Group: "dataassets",
		Short: "Search for class paths by filter",
		Params: []ParamSpec{
			{Name: "filter", Type: "string", Help: "Filter by class name"},
			{Name: "parent_class", Type: "string", Help: "Filter by parent class"},
			{Name: "max_results", Type: "int", Default: 50, Help: "Maximum results"},
			{Name: "include_properties", Type: "bool", Default: false, Help: "Include property details"},
		},
	},
	{
		Name:  "get_mass_config_traits",
		Group: "dataassets",
		Short: "Get Mass Entity config traits from a data asset",
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Asset path"},
		},
	},
	{
		Name:  "list_data_assets",
		Group: "dataassets",
		Short: "List data assets by path/class",
		Params: []ParamSpec{
			{Name: "path", Type: "string", Default: "/Game", Help: "Content path"},
			{Name: "class_filter", Type: "string", Help: "Filter by class"},
			{Name: "recursive", Type: "bool", Default: true, Help: "Search subdirectories"},
			{Name: "max_results", Type: "int", Default: 200, Help: "Maximum results"},
		},
	},
}
