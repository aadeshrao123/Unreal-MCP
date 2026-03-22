package cmd

func init() {
	ensureGroup("input", "Enhanced Input")
	registerCommands(enhancedInputCommands)
}

var enhancedInputCommands = []CommandSpec{
	// -- Input Actions --
	{
		Name:  "create_input_action",
		Group: "input",
		Short: "Create a new UInputAction asset",
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Asset path"},
			{Name: "value_type", Type: "string", Default: "Boolean", Help: "Boolean, Axis1D, Axis2D, or Axis3D"},
			{Name: "properties", Type: "json", Help: "JSON object of additional properties"},
		},
	},
	{
		Name:  "get_input_action",
		Group: "input",
		Short: "Read all properties, triggers, and modifiers of an Input Action",
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Asset path"},
		},
	},
	{
		Name:  "set_input_action_properties",
		Group: "input",
		Short: "Set properties on an Input Action",
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Asset path"},
			{Name: "value_type", Type: "string", Help: "Value type"},
			{Name: "properties", Type: "json", Help: "JSON object of properties"},
		},
	},
	{
		Name:  "add_input_action_trigger",
		Group: "input",
		Short: "Add a trigger to an Input Action",
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Asset path"},
			{Name: "trigger_type", Type: "string", Required: true, Help: "Hold, Pressed, Released, Tap, etc."},
			{Name: "properties", Type: "json", Help: "JSON object of trigger properties"},
		},
	},
	{
		Name:  "add_input_action_modifier",
		Group: "input",
		Short: "Add a modifier to an Input Action",
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Asset path"},
			{Name: "modifier_type", Type: "string", Required: true, Help: "DeadZone, Negate, Scalar, etc."},
			{Name: "properties", Type: "json", Help: "JSON object of modifier properties"},
		},
	},
	{
		Name:  "remove_input_action_trigger",
		Group: "input",
		Short: "Remove a trigger by index",
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Asset path"},
			{Name: "index", Type: "int", Required: true, Help: "Trigger index to remove"},
		},
	},
	{
		Name:  "remove_input_action_modifier",
		Group: "input",
		Short: "Remove a modifier by index",
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Asset path"},
			{Name: "index", Type: "int", Required: true, Help: "Modifier index to remove"},
		},
	},
	{
		Name:  "list_input_actions",
		Group: "input",
		Short: "List all UInputAction assets",
		Params: []ParamSpec{
			{Name: "path", Type: "string", Default: "/Game", Help: "Search path"},
			{Name: "filter", Type: "string", Help: "Name filter"},
			{Name: "recursive", Type: "bool", Default: true, Help: "Search subdirectories"},
			{Name: "max_results", Type: "int", Default: 200, Help: "Maximum results"},
		},
	},

	// -- Input Mapping Contexts --
	{
		Name:  "create_input_mapping_context",
		Group: "input",
		Short: "Create a new UInputMappingContext asset",
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Asset path"},
			{Name: "description", Type: "string", Help: "Description"},
		},
	},
	{
		Name:  "get_input_mapping_context",
		Group: "input",
		Short: "Read all key mappings with triggers/modifiers",
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Asset path"},
		},
	},
	{
		Name:  "add_key_mapping",
		Group: "input",
		Short: "Add a key-to-action mapping",
		Params: []ParamSpec{
			{Name: "context_path", Type: "string", Required: true, Help: "Mapping context path"},
			{Name: "action_path", Type: "string", Required: true, Help: "Input action path"},
			{Name: "key", Type: "string", Required: true, Help: "Key name (e.g. W, SpaceBar, LeftMouseButton)"},
			{Name: "triggers", Type: "json", Help: "JSON array of trigger definitions"},
			{Name: "modifiers", Type: "json", Help: "JSON array of modifier definitions"},
		},
	},
	{
		Name:  "remove_key_mapping",
		Group: "input",
		Short: "Remove a mapping by index or action+key",
		Params: []ParamSpec{
			{Name: "context_path", Type: "string", Required: true, Help: "Mapping context path"},
			{Name: "index", Type: "int", Help: "Mapping index"},
			{Name: "action_path", Type: "string", Help: "Action path (alternative to index)"},
			{Name: "key", Type: "string", Help: "Key name (alternative to index)"},
		},
	},
	{
		Name:  "set_key_mapping",
		Group: "input",
		Short: "Change key or action on existing mapping",
		Params: []ParamSpec{
			{Name: "context_path", Type: "string", Required: true, Help: "Mapping context path"},
			{Name: "index", Type: "int", Required: true, Help: "Mapping index"},
			{Name: "key", Type: "string", Help: "New key name"},
			{Name: "action_path", Type: "string", Help: "New action path"},
		},
	},
	{
		Name:  "add_mapping_trigger",
		Group: "input",
		Short: "Add a trigger to a specific mapping",
		Params: []ParamSpec{
			{Name: "context_path", Type: "string", Required: true, Help: "Mapping context path"},
			{Name: "mapping_index", Type: "int", Required: true, Help: "Mapping index"},
			{Name: "trigger_type", Type: "string", Required: true, Help: "Trigger type"},
			{Name: "properties", Type: "json", Help: "JSON object of trigger properties"},
		},
	},
	{
		Name:  "add_mapping_modifier",
		Group: "input",
		Short: "Add a modifier to a specific mapping",
		Params: []ParamSpec{
			{Name: "context_path", Type: "string", Required: true, Help: "Mapping context path"},
			{Name: "mapping_index", Type: "int", Required: true, Help: "Mapping index"},
			{Name: "modifier_type", Type: "string", Required: true, Help: "Modifier type"},
			{Name: "properties", Type: "json", Help: "JSON object of modifier properties"},
		},
	},
	{
		Name:  "remove_mapping_trigger",
		Group: "input",
		Short: "Remove a trigger from a mapping",
		Params: []ParamSpec{
			{Name: "context_path", Type: "string", Required: true, Help: "Mapping context path"},
			{Name: "mapping_index", Type: "int", Required: true, Help: "Mapping index"},
			{Name: "trigger_index", Type: "int", Required: true, Help: "Trigger index"},
		},
	},
	{
		Name:  "remove_mapping_modifier",
		Group: "input",
		Short: "Remove a modifier from a mapping",
		Params: []ParamSpec{
			{Name: "context_path", Type: "string", Required: true, Help: "Mapping context path"},
			{Name: "mapping_index", Type: "int", Required: true, Help: "Mapping index"},
			{Name: "modifier_index", Type: "int", Required: true, Help: "Modifier index"},
		},
	},
	{
		Name:  "list_input_mapping_contexts",
		Group: "input",
		Short: "List all UInputMappingContext assets",
		Params: []ParamSpec{
			{Name: "path", Type: "string", Default: "/Game", Help: "Search path"},
			{Name: "filter", Type: "string", Help: "Name filter"},
			{Name: "recursive", Type: "bool", Default: true, Help: "Search subdirectories"},
			{Name: "max_results", Type: "int", Default: 200, Help: "Maximum results"},
		},
	},

	// -- Discovery --
	{
		Name:  "list_trigger_types",
		Group: "input",
		Short: "List all UInputTrigger subclasses with properties",
		Params: []ParamSpec{
			{Name: "filter", Type: "string", Help: "Name filter"},
		},
	},
	{
		Name:  "list_modifier_types",
		Group: "input",
		Short: "List all UInputModifier subclasses with properties",
		Params: []ParamSpec{
			{Name: "filter", Type: "string", Help: "Name filter"},
		},
	},
	{
		Name:  "list_input_keys",
		Group: "input",
		Short: "List available FKey values (keyboard, mouse, gamepad)",
		Params: []ParamSpec{
			{Name: "filter", Type: "string", Help: "Name filter"},
			{Name: "category", Type: "string", Help: "Category filter"},
			{Name: "max_results", Type: "int", Default: 500, Help: "Maximum results"},
		},
	},
}
