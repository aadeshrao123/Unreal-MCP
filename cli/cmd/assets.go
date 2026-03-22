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
		Long:  "Searches the Unreal Asset Registry. Supports class shortcuts (material, blueprint, static_mesh, texture, data_table, niagara_system) or full class paths. Returns asset paths, class names, and package info. Use name_pattern for case-insensitive substring matching on asset names.",
		Example: `ue-cli find_assets --class-type material --path /Game/Materials
ue-cli find_assets --name-pattern "Miner" --max-results 10
ue-cli find_assets --class-type blueprint --path /Game/Blueprints --recursive`,
		Params: []ParamSpec{
			{Name: "class_type", Type: "string", Help: "Class shortcut (material, blueprint, static_mesh, texture, data_table, niagara_system) or full class path like PackagePath.ClassName"},
			{Name: "path", Type: "string", Help: "Content path filter (e.g. /Game/Materials, /Game/Blueprints)"},
			{Name: "name_pattern", Type: "string", Help: "Substring match on asset name (case-insensitive)"},
			{Name: "recursive", Type: "bool", Default: true, Help: "Search subdirectories"},
			{Name: "max_results", Type: "int", Default: 200, Help: "Maximum results to return"},
		},
	},
	{
		Name:  "list_assets",
		Group: "assets",
		Short: "List assets in a Content Browser directory",
		Long:  "Lists all assets in a Content Browser directory, optionally filtered by class. Simpler than find_assets — use when you know the directory and want to see what's in it.",
		Example: `ue-cli list_assets --path /Game
ue-cli list_assets --path /Game/Materials --class-filter material`,
		Params: []ParamSpec{
			{Name: "path", Type: "string", Default: "/Game", Help: "Content path (e.g. /Game, /Game/Materials)"},
			{Name: "class_filter", Type: "string", Help: "Optional class shortcut filter (material, blueprint, etc.)"},
			{Name: "recursive", Type: "bool", Default: true, Help: "Search subdirectories"},
		},
	},
	{
		Name:  "get_asset_info",
		Group: "assets",
		Short: "Get asset metadata (class, package, properties)",
		Long:  "Returns metadata about an asset including its class, package path, and basic properties. Use to inspect what type of asset something is before modifying it.",
		Example: "ue-cli get_asset_info --asset-path /Game/Materials/M_Base",
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Full content path to the asset"},
		},
	},
	{
		Name:  "get_asset_properties",
		Group: "assets",
		Short: "Get all editable properties of any asset",
		Long:  "Returns all editable UPROPERTY values from any asset. Works with materials, blueprints, data assets, etc. For data assets specifically, prefer get_data_asset_properties which has filtering support.",
		Example: "ue-cli get_asset_properties --asset-path /Game/Materials/M_Base",
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Full content path to the asset"},
		},
	},
	{
		Name:  "set_asset_property",
		Group: "assets",
		Short: "Set a single property on any asset",
		Long:  "Sets a single UPROPERTY on any asset. Auto-falls back to Blueprint CDO if the asset is a Blueprint. Call save_asset after to persist changes.",
		Example: `ue-cli set_asset_property --asset-path /Game/BP_MyActor --property-name "MaxHealth" --property-value "200.0"`,
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Full content path to the asset"},
			{Name: "property_name", Type: "string", Required: true, Help: "Property name to set"},
			{Name: "property_value", Type: "string", Required: true, Help: "New property value (as string)"},
		},
	},
	{
		Name:  "find_references",
		Group: "assets",
		Short: "Find dependents/dependencies of an asset",
		Long:  "Finds what assets reference this asset (dependents) or what this asset references (dependencies). Use before deleting an asset to check for broken references.",
		Example: `ue-cli find_references --asset-path /Game/Materials/M_Base --direction dependents
ue-cli find_references --asset-path /Game/Materials/M_Base --direction both`,
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Full content path to the asset"},
			{Name: "direction", Type: "string", Default: "both", Help: "dependents (what uses this), dependencies (what this uses), or both"},
		},
	},
	{
		Name:    "open_asset",
		Group:   "assets",
		Short:   "Open asset in the editor",
		Long:    "Opens the asset in its appropriate editor (Material Editor, Blueprint Editor, etc.).",
		Example: "ue-cli open_asset --asset-path /Game/Materials/M_Base",
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Full content path to the asset"},
		},
	},
	{
		Name:    "save_asset",
		Group:   "assets",
		Short:   "Save a specific asset to disk",
		Long:    "Saves a modified (dirty) asset to disk. Always call after modifying asset properties, materials, blueprints, etc. Data table operations auto-save.",
		Example: "ue-cli save_asset --asset-path /Game/Materials/M_Base",
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Full content path to the asset"},
		},
	},
	{
		Name:    "save_all",
		Group:   "assets",
		Short:   "Save all unsaved (dirty) assets",
		Long:    "Saves all modified assets to disk. Use after batch operations instead of saving each asset individually.",
		Example: "ue-cli save_all",
	},
	{
		Name:  "delete_asset",
		Group: "assets",
		Short: "Delete an asset or directory (checks references)",
		Long:  "Deletes an asset from the Content Browser. By default checks for references first and refuses if other assets depend on it. Use --force to bypass. Also works on directories.",
		Example: `ue-cli delete_asset --asset-path /Game/Materials/M_OldMaterial
ue-cli delete_asset --asset-path /Game/Materials/M_OldMaterial --force`,
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Full content path to the asset or directory"},
			{Name: "force", Type: "bool", Default: false, Help: "Force delete even with references"},
		},
	},
	{
		Name:  "duplicate_asset",
		Group: "assets",
		Short: "Copy an asset to a new location",
		Long:  "Creates a copy of an asset at a new location with a new name. The copy is independent — changes to one don't affect the other.",
		Example: `ue-cli duplicate_asset --source-path /Game/Materials/M_Base --dest-path /Game/Materials --dest-name M_BaseCopy`,
		Params: []ParamSpec{
			{Name: "source_path", Type: "string", Required: true, Help: "Source asset path"},
			{Name: "dest_path", Type: "string", Required: true, Help: "Destination directory path"},
			{Name: "dest_name", Type: "string", Required: true, Help: "New asset name"},
		},
	},
	{
		Name:  "rename_asset",
		Group: "assets",
		Short: "Rename/move an asset (auto-fixes references)",
		Long:  "Renames or moves an asset to a new path. Automatically updates all references across the project. Safer than manual move.",
		Example: `ue-cli rename_asset --source-path /Game/Materials/M_Old --dest-path /Game/Materials/M_New`,
		Params: []ParamSpec{
			{Name: "source_path", Type: "string", Required: true, Help: "Current asset path"},
			{Name: "dest_path", Type: "string", Required: true, Help: "New asset path (full path including new name)"},
		},
	},
	{
		Name:  "import_asset",
		Group: "assets",
		Short: "Import external file into Content Browser",
		Long:  "Imports an external file (PNG, FBX, WAV, etc.) from the filesystem into the UE Content Browser. Unreal handles conversion to the appropriate asset type.",
		Example: `ue-cli import_asset --source-file "C:/Art/texture.png" --destination-path /Game/Textures --destination-name T_MyTexture`,
		Params: []ParamSpec{
			{Name: "source_file", Type: "string", Required: true, Help: "Full filesystem path to the external file"},
			{Name: "destination_path", Type: "string", Required: true, Help: "Content Browser destination directory"},
			{Name: "destination_name", Type: "string", Help: "Asset name (defaults to filename without extension)"},
			{Name: "replace_existing", Type: "bool", Default: true, Help: "Replace existing asset if name conflicts"},
		},
	},
	{
		Name:  "import_assets_batch",
		Group: "assets",
		Short: "Batch import files into Content Browser",
		Long:  "Imports multiple files at once. Either provide a JSON array of file paths, or specify a source_directory and file extensions to scan. More efficient than calling import_asset in a loop.",
		Example: `ue-cli import_assets_batch --destination-path /Game/Textures --source-directory "C:/Art/Textures" --extensions '["png","jpg"]'
ue-cli import_assets_batch --destination-path /Game/Meshes --files '["C:/Art/mesh1.fbx","C:/Art/mesh2.fbx"]'`,
		Params: []ParamSpec{
			{Name: "destination_path", Type: "string", Required: true, Help: "Content Browser destination directory"},
			{Name: "files", Type: "json", Help: "JSON array of file paths to import"},
			{Name: "source_directory", Type: "string", Help: "Directory to scan for files (alternative to --files)"},
			{Name: "extensions", Type: "json", Help: "JSON array of file extensions to include when scanning directory"},
			{Name: "replace_existing", Type: "bool", Default: true, Help: "Replace existing assets"},
		},
	},
	{
		Name:    "get_selected_assets",
		Group:   "assets",
		Short:   "Get currently selected Content Browser assets",
		Long:    "Returns the assets currently selected in the Content Browser panel. Useful for operating on whatever the user has selected in the editor.",
		Example: "ue-cli get_selected_assets",
	},
	{
		Name:    "sync_browser",
		Group:   "assets",
		Short:   "Navigate Content Browser to show an asset",
		Long:    "Scrolls the Content Browser to show and highlight a specific asset. Use to direct the user's attention to an asset after creating or modifying it.",
		Example: "ue-cli sync_browser --asset-path /Game/Materials/M_NewMaterial",
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Asset path to navigate to"},
		},
	},
}
