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
		Long:  "Lists all C++ and Blueprint UDataAsset subclasses available in the project. Use to discover which class_name to pass to create_data_asset.",
		Example: `ue-cli list_data_asset_classes --filter "Recipe"`,
		Params: []ParamSpec{
			{Name: "filter", Type: "string", Help: "Filter by class name substring"},
			{Name: "include_abstract", Type: "bool", Default: false, Help: "Include abstract classes (not instantiable)"},
		},
	},
	{
		Name:  "create_data_asset",
		Group: "dataassets",
		Short: "Create a new UDataAsset subclass instance",
		Long:  "Creates a new data asset of the specified class at the given path. Optionally sets initial property values. Call save_asset after to persist.",
		Example: `ue-cli create_data_asset --class-name "RecipeData" --asset-path /Game/Data/DA_NewRecipe --initial-properties '{"CraftTime":5.0}'`,
		Params: []ParamSpec{
			{Name: "class_name", Type: "string", Required: true, Help: "UDataAsset subclass name"},
			{Name: "asset_path", Type: "string", Required: true, Help: "Content path to create at (e.g. /Game/Data/DA_MyAsset)"},
			{Name: "initial_properties", Type: "json", Help: "JSON object of initial property name-value pairs"},
		},
	},
	{
		Name:  "get_data_asset_properties",
		Group: "dataassets",
		Short: "Read all properties of a data asset",
		Long:  "Returns all UPROPERTY values from a data asset. Supports filtering by property name substring to reduce output. Use include_inherited to also see parent class properties.",
		Example: `ue-cli get_data_asset_properties --asset-path /Game/Data/DA_MyRecipe
ue-cli get_data_asset_properties --asset-path /Game/Data/DA_MyRecipe --filter "Power"`,
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Content path to the data asset"},
			{Name: "filter", Type: "string", Help: "Filter property names by substring (recommended to reduce output)"},
			{Name: "include_inherited", Type: "bool", Default: true, Help: "Include properties from parent classes"},
		},
	},
	{
		Name:  "set_data_asset_property",
		Group: "dataassets",
		Short: "Set a single property on a data asset",
		Long:  "Sets one property value on a data asset. The value is a JSON value (string, number, bool, object, or array depending on the property type). Call save_asset after.",
		Example: `ue-cli set_data_asset_property --asset-path /Game/Data/DA_MyRecipe --property-name "CraftTime" --property-value "5.0"`,
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Content path to the data asset"},
			{Name: "property_name", Type: "string", Required: true, Help: "Property name"},
			{Name: "property_value", Type: "json", Required: true, Help: "Property value as JSON (string, number, bool, object, or array)"},
		},
	},
	{
		Name:  "set_data_asset_properties",
		Group: "dataassets",
		Short: "Set multiple properties on a data asset in one call",
		Long:  "Sets multiple properties at once — more efficient than calling set_data_asset_property repeatedly as it loads/saves only once. Call save_asset after.",
		Example: `ue-cli set_data_asset_properties --asset-path /Game/Data/DA_MyRecipe --properties '{"CraftTime":5.0,"OutputCount":2}'`,
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Content path to the data asset"},
			{Name: "properties", Type: "json", Required: true, Help: "JSON object of {property_name: value} pairs"},
		},
	},
	{
		Name:  "get_property_valid_types",
		Group: "dataassets",
		Short: "Query valid dropdown values for a property slot",
		Long:  "Returns the list of valid values for instanced/dropdown properties (e.g. Mass Entity Fragments, Traits). IMPORTANT: Always use the filter parameter — without it, some properties return 200+ entries.",
		Example: `ue-cli get_property_valid_types --class-name "MassEntityConfigAsset" --property-path "Traits" --filter "Miner"`,
		Params: []ParamSpec{
			{Name: "class_name", Type: "string", Required: true, Help: "Class name that owns the property"},
			{Name: "property_path", Type: "string", Required: true, Help: "Property path (e.g. Traits, Fragments)"},
			{Name: "filter", Type: "string", Help: "Filter results by name (strongly recommended to avoid huge responses)"},
			{Name: "include_abstract", Type: "bool", Default: false, Help: "Include abstract (non-instantiable) types"},
		},
	},
	{
		Name:  "search_class_paths",
		Group: "dataassets",
		Short: "Search for class paths by filter",
		Long:  "Searches for UClass paths matching a filter. Use to find the exact class path for a type you want to reference in properties or create instances of.",
		Example: `ue-cli search_class_paths --filter "Miner" --parent-class "MassProcessor"`,
		Params: []ParamSpec{
			{Name: "filter", Type: "string", Help: "Filter by class name substring"},
			{Name: "parent_class", Type: "string", Help: "Only show subclasses of this parent"},
			{Name: "max_results", Type: "int", Default: 50, Help: "Maximum results"},
			{Name: "include_properties", Type: "bool", Default: false, Help: "Include property details for each class"},
		},
	},
	{
		Name:  "get_mass_config_traits",
		Group: "dataassets",
		Short: "Get Mass Entity config traits from a data asset",
		Long:  "Reads the Mass Entity traits configured on a MassEntityConfigAsset. Specific to Mass Entity Framework — returns trait classes and their fragment requirements.",
		Example: "ue-cli get_mass_config_traits --asset-path /Game/Mass/DA_MinerConfig",
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Content path to the Mass Entity config asset"},
		},
	},
	{
		Name:  "list_data_assets",
		Group: "dataassets",
		Short: "List data assets by path/class",
		Long:  "Lists all data asset instances in a directory, optionally filtered by class. Returns asset paths and class names.",
		Example: `ue-cli list_data_assets --path /Game/Data --class-filter "RecipeData"`,
		Params: []ParamSpec{
			{Name: "path", Type: "string", Default: "/Game", Help: "Content path to search"},
			{Name: "class_filter", Type: "string", Help: "Filter by data asset class name"},
			{Name: "recursive", Type: "bool", Default: true, Help: "Search subdirectories"},
			{Name: "max_results", Type: "int", Default: 200, Help: "Maximum results"},
		},
	},
}
