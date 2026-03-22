package cmd

func init() {
	ensureGroup("assets", "Asset Management")
	registerCommands(assetCommands)
}

var assetCommands = []CommandSpec{
	{
		Name:  "find_assets",
		Group: "assets",
		Short: "Search the Asset Registry by class, path, and/or name pattern",
		Params: []ParamSpec{
			{Name: "class_type", Type: "string", Help: "Class shortcut (material, blueprint, static_mesh, etc.) or full class path"},
			{Name: "path", Type: "string", Help: "Content path filter (e.g. /Game/Materials)"},
			{Name: "name_pattern", Type: "string", Help: "Substring match on asset name (case-insensitive)"},
			{Name: "recursive", Type: "bool", Default: true, Help: "Search subdirectories"},
			{Name: "max_results", Type: "int", Default: 200, Help: "Maximum results to return"},
		},
	},
	{
		Name:  "list_assets",
		Group: "assets",
		Short: "List assets in a Content Browser directory",
		Params: []ParamSpec{
			{Name: "path", Type: "string", Default: "/Game", Help: "Content path"},
			{Name: "class_filter", Type: "string", Help: "Optional class shortcut"},
			{Name: "recursive", Type: "bool", Default: true, Help: "Search subdirectories"},
		},
	},
	{
		Name:  "get_asset_info",
		Group: "assets",
		Short: "Get asset metadata (class, package, properties)",
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Asset path"},
		},
	},
	{
		Name:  "get_asset_properties",
		Group: "assets",
		Short: "Get all editable properties of any asset",
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Asset path"},
		},
	},
	{
		Name:  "set_asset_property",
		Group: "assets",
		Short: "Set a single property on any asset",
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Asset path"},
			{Name: "property_name", Type: "string", Required: true, Help: "Property name"},
			{Name: "property_value", Type: "string", Required: true, Help: "Property value"},
		},
	},
	{
		Name:  "find_references",
		Group: "assets",
		Short: "Find dependents/dependencies of an asset",
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Asset path"},
			{Name: "direction", Type: "string", Default: "both", Help: "dependents, dependencies, or both"},
		},
	},
	{
		Name:  "open_asset",
		Group: "assets",
		Short: "Open asset in the editor",
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Asset path"},
		},
	},
	{
		Name:  "save_asset",
		Group: "assets",
		Short: "Save a specific asset to disk",
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Asset path"},
		},
	},
	{
		Name:  "save_all",
		Group: "assets",
		Short: "Save all unsaved (dirty) assets",
	},
	{
		Name:  "delete_asset",
		Group: "assets",
		Short: "Delete an asset or directory (checks references)",
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Asset path"},
			{Name: "force", Type: "bool", Default: false, Help: "Force delete even with references"},
		},
	},
	{
		Name:  "duplicate_asset",
		Group: "assets",
		Short: "Copy an asset to a new location",
		Params: []ParamSpec{
			{Name: "source_path", Type: "string", Required: true, Help: "Source asset path"},
			{Name: "dest_path", Type: "string", Required: true, Help: "Destination path"},
			{Name: "dest_name", Type: "string", Required: true, Help: "Destination asset name"},
		},
	},
	{
		Name:  "rename_asset",
		Group: "assets",
		Short: "Rename/move an asset (auto-fixes references)",
		Params: []ParamSpec{
			{Name: "source_path", Type: "string", Required: true, Help: "Current asset path"},
			{Name: "dest_path", Type: "string", Required: true, Help: "New asset path"},
		},
	},
	{
		Name:  "import_asset",
		Group: "assets",
		Short: "Import external file into Content Browser",
		Params: []ParamSpec{
			{Name: "source_file", Type: "string", Required: true, Help: "External file path"},
			{Name: "destination_path", Type: "string", Required: true, Help: "Content Browser destination"},
			{Name: "destination_name", Type: "string", Help: "Asset name (default: filename)"},
			{Name: "replace_existing", Type: "bool", Default: true, Help: "Replace if exists"},
		},
	},
	{
		Name:  "import_assets_batch",
		Group: "assets",
		Short: "Batch import files into Content Browser",
		Params: []ParamSpec{
			{Name: "destination_path", Type: "string", Required: true, Help: "Content Browser destination"},
			{Name: "files", Type: "json", Help: "JSON array of file paths"},
			{Name: "source_directory", Type: "string", Help: "Directory to scan"},
			{Name: "extensions", Type: "json", Help: "JSON array of extensions to include"},
			{Name: "replace_existing", Type: "bool", Default: true, Help: "Replace if exists"},
		},
	},
	{
		Name:  "get_selected_assets",
		Group: "assets",
		Short: "Get currently selected Content Browser assets",
	},
	{
		Name:  "sync_browser",
		Group: "assets",
		Short: "Navigate Content Browser to show an asset",
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Asset path to navigate to"},
		},
	},
}
