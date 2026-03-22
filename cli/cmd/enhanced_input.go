package cmd

func init() {
	ensureGroup("input", "Enhanced Input")
	registerCommands(enhancedInputCommands)
}

var enhancedInputCommands = []CommandSpec{
	// -- Input Actions --
	{
		Name:    "create_input_action",
		Group:   "input",
		Short:   "Create a new UInputAction asset",
		Long:    "Creates a new UInputAction asset at the given path. Set value_type to control the action's output dimensionality (Boolean for buttons, Axis1D for triggers, Axis2D for sticks/mouse, Axis3D for spatial). Pass additional properties as JSON to configure consumption, accumulation, and other settings.",
		Example: `ue-cli create_input_action --asset-path /Game/Input/Actions/IA_Jump --value-type Boolean
ue-cli create_input_action --asset-path /Game/Input/Actions/IA_Move --value-type Axis2D
ue-cli create_input_action --asset-path /Game/Input/Actions/IA_Look --value-type Axis2D --properties '{"bConsumeInput": true}'`,
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Asset path"},
			{Name: "value_type", Type: "string", Default: "Boolean", Help: "Boolean, Axis1D, Axis2D, or Axis3D"},
			{Name: "properties", Type: "json", Help: "JSON object of additional properties"},
		},
	},
	{
		Name:    "get_input_action",
		Group:   "input",
		Short:   "Read all properties, triggers, and modifiers of an Input Action",
		Long:    "Returns the full configuration of an Input Action including its value type, all attached triggers (Hold, Pressed, Tap, etc.), and all attached modifiers (DeadZone, Negate, Scalar, etc.). Use to inspect an action before adding or removing triggers/modifiers.",
		Example: "ue-cli get_input_action --asset-path /Game/Input/Actions/IA_Jump",
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Asset path"},
		},
	},
	{
		Name:    "set_input_action_properties",
		Group:   "input",
		Short:   "Set properties on an Input Action",
		Long:    "Modifies properties on an existing Input Action. Can change the value_type or set any UPROPERTY via the properties JSON. Call save_asset after to persist changes.",
		Example: `ue-cli set_input_action_properties --asset-path /Game/Input/Actions/IA_Jump --value-type Boolean
ue-cli set_input_action_properties --asset-path /Game/Input/Actions/IA_Move --properties '{"bConsumeInput": false}'`,
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Asset path"},
			{Name: "value_type", Type: "string", Help: "Value type"},
			{Name: "properties", Type: "json", Help: "JSON object of properties"},
		},
	},
	{
		Name:    "add_input_action_trigger",
		Group:   "input",
		Short:   "Add a trigger to an Input Action",
		Long:    "Adds a trigger directly to the Input Action asset. Triggers define when the action fires (Pressed for instant, Hold for sustained press, Tap for quick press-release, Released for key-up). Properties vary by trigger type -- e.g., Hold has HoldTimeThreshold, Tap has TapReleaseTimeThreshold. Use list_trigger_types to discover available types.",
		Example: `ue-cli add_input_action_trigger --asset-path /Game/Input/Actions/IA_Interact --trigger-type Pressed
ue-cli add_input_action_trigger --asset-path /Game/Input/Actions/IA_Crouch --trigger-type Hold --properties '{"HoldTimeThreshold": 0.5}'
ue-cli add_input_action_trigger --asset-path /Game/Input/Actions/IA_Dodge --trigger-type Tap --properties '{"TapReleaseTimeThreshold": 0.2}'`,
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Asset path"},
			{Name: "trigger_type", Type: "string", Required: true, Help: "Hold, Pressed, Released, Tap, etc."},
			{Name: "properties", Type: "json", Help: "JSON object of trigger properties"},
		},
	},
	{
		Name:    "add_input_action_modifier",
		Group:   "input",
		Short:   "Add a modifier to an Input Action",
		Long:    "Adds a modifier directly to the Input Action asset. Modifiers transform the input value before it reaches gameplay code (DeadZone filters noise, Negate inverts axes, Scalar multiplies values, SwizzleInputAxisValues reorders axes). Use list_modifier_types to discover available types and their properties.",
		Example: `ue-cli add_input_action_modifier --asset-path /Game/Input/Actions/IA_Look --modifier-type DeadZone --properties '{"LowerThreshold": 0.1}'
ue-cli add_input_action_modifier --asset-path /Game/Input/Actions/IA_Look --modifier-type Negate
ue-cli add_input_action_modifier --asset-path /Game/Input/Actions/IA_Move --modifier-type Scalar --properties '{"Scalar": {"X": 2.0, "Y": 2.0, "Z": 1.0}}'`,
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Asset path"},
			{Name: "modifier_type", Type: "string", Required: true, Help: "DeadZone, Negate, Scalar, etc."},
			{Name: "properties", Type: "json", Help: "JSON object of modifier properties"},
		},
	},
	{
		Name:    "remove_input_action_trigger",
		Group:   "input",
		Short:   "Remove a trigger by index",
		Long:    "Removes a trigger from an Input Action by its zero-based index. Use get_input_action first to see the list of triggers and their indices.",
		Example: "ue-cli remove_input_action_trigger --asset-path /Game/Input/Actions/IA_Jump --index 0",
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Asset path"},
			{Name: "index", Type: "int", Required: true, Help: "Trigger index to remove"},
		},
	},
	{
		Name:    "remove_input_action_modifier",
		Group:   "input",
		Short:   "Remove a modifier by index",
		Long:    "Removes a modifier from an Input Action by its zero-based index. Use get_input_action first to see the list of modifiers and their indices.",
		Example: "ue-cli remove_input_action_modifier --asset-path /Game/Input/Actions/IA_Look --index 1",
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Asset path"},
			{Name: "index", Type: "int", Required: true, Help: "Modifier index to remove"},
		},
	},
	{
		Name:    "list_input_actions",
		Group:   "input",
		Short:   "List all UInputAction assets",
		Long:    "Lists all UInputAction assets under the given path. Use filter to narrow results by name substring. Useful for discovering existing actions before creating new ones or setting up mapping contexts.",
		Example: `ue-cli list_input_actions
ue-cli list_input_actions --path /Game/Input/Actions --filter "Move"
ue-cli list_input_actions --recursive=false --max-results 50`,
		Params: []ParamSpec{
			{Name: "path", Type: "string", Default: "/Game", Help: "Search path"},
			{Name: "filter", Type: "string", Help: "Name filter"},
			{Name: "recursive", Type: "bool", Default: true, Help: "Search subdirectories"},
			{Name: "max_results", Type: "int", Default: 200, Help: "Maximum results"},
		},
	},

	// -- Input Mapping Contexts --
	{
		Name:    "create_input_mapping_context",
		Group:   "input",
		Short:   "Create a new UInputMappingContext asset",
		Long:    "Creates a new Input Mapping Context asset. Mapping contexts bind keys to Input Actions and are added to players at runtime via AddMappingContext. After creation, use add_key_mapping to populate it with key-to-action bindings.",
		Example: `ue-cli create_input_mapping_context --asset-path /Game/Input/IMC_Default
ue-cli create_input_mapping_context --asset-path /Game/Input/IMC_Vehicle --description "Vehicle-specific controls"`,
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Asset path"},
			{Name: "description", Type: "string", Help: "Description"},
		},
	},
	{
		Name:    "get_input_mapping_context",
		Group:   "input",
		Short:   "Read all key mappings with triggers/modifiers",
		Long:    "Returns the full contents of a mapping context including every key-to-action binding with their per-mapping triggers and modifiers. Each mapping entry includes its index, which you need for set_key_mapping, add_mapping_trigger, and add_mapping_modifier.",
		Example: "ue-cli get_input_mapping_context --asset-path /Game/Input/IMC_Default",
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Asset path"},
		},
	},
	{
		Name:    "add_key_mapping",
		Group:   "input",
		Short:   "Add a key-to-action mapping",
		Long:    "Adds a key binding to a mapping context, linking a physical key to an Input Action. Optionally attach per-mapping triggers and modifiers as JSON arrays. Key names use Unreal FKey names -- use list_input_keys to discover valid key names.",
		Example: `ue-cli add_key_mapping --context-path /Game/Input/IMC_Default --action-path /Game/Input/Actions/IA_Jump --key SpaceBar
ue-cli add_key_mapping --context-path /Game/Input/IMC_Default --action-path /Game/Input/Actions/IA_Move --key W --modifiers '[{"type":"SwizzleInputAxisValues","properties":{"Order":"YXZ"}}]'
ue-cli add_key_mapping --context-path /Game/Input/IMC_Default --action-path /Game/Input/Actions/IA_Shoot --key LeftMouseButton --triggers '[{"type":"Pressed"}]'`,
		Params: []ParamSpec{
			{Name: "context_path", Type: "string", Required: true, Help: "Mapping context path"},
			{Name: "action_path", Type: "string", Required: true, Help: "Input action path"},
			{Name: "key", Type: "string", Required: true, Help: "Key name (e.g. W, SpaceBar, LeftMouseButton)"},
			{Name: "triggers", Type: "json", Help: "JSON array of trigger definitions"},
			{Name: "modifiers", Type: "json", Help: "JSON array of modifier definitions"},
		},
	},
	{
		Name:    "remove_key_mapping",
		Group:   "input",
		Short:   "Remove a mapping by index or action+key",
		Long:    "Removes a key-to-action mapping from a context. Specify either the mapping index (from get_input_mapping_context output) or an action_path+key combination to identify the mapping. Using index is faster when you know it; action+key is more readable.",
		Example: `ue-cli remove_key_mapping --context-path /Game/Input/IMC_Default --index 0
ue-cli remove_key_mapping --context-path /Game/Input/IMC_Default --action-path /Game/Input/Actions/IA_Jump --key SpaceBar`,
		Params: []ParamSpec{
			{Name: "context_path", Type: "string", Required: true, Help: "Mapping context path"},
			{Name: "index", Type: "int", Help: "Mapping index"},
			{Name: "action_path", Type: "string", Help: "Action path (alternative to index)"},
			{Name: "key", Type: "string", Help: "Key name (alternative to index)"},
		},
	},
	{
		Name:    "set_key_mapping",
		Group:   "input",
		Short:   "Change key or action on existing mapping",
		Long:    "Modifies an existing mapping in-place. Use to rebind a key or reassign which action a mapping triggers without removing and re-adding the entry (preserves any attached triggers/modifiers).",
		Example: `ue-cli set_key_mapping --context-path /Game/Input/IMC_Default --index 0 --key E
ue-cli set_key_mapping --context-path /Game/Input/IMC_Default --index 2 --action-path /Game/Input/Actions/IA_Interact`,
		Params: []ParamSpec{
			{Name: "context_path", Type: "string", Required: true, Help: "Mapping context path"},
			{Name: "index", Type: "int", Required: true, Help: "Mapping index"},
			{Name: "key", Type: "string", Help: "New key name"},
			{Name: "action_path", Type: "string", Help: "New action path"},
		},
	},
	{
		Name:    "add_mapping_trigger",
		Group:   "input",
		Short:   "Add a trigger to a specific mapping",
		Long:    "Adds a trigger to a specific key mapping within a context. Per-mapping triggers override the action-level triggers for that particular key binding. Use get_input_mapping_context to find the mapping_index.",
		Example: `ue-cli add_mapping_trigger --context-path /Game/Input/IMC_Default --mapping-index 0 --trigger-type Hold --properties '{"HoldTimeThreshold": 1.0}'
ue-cli add_mapping_trigger --context-path /Game/Input/IMC_Default --mapping-index 3 --trigger-type Pressed`,
		Params: []ParamSpec{
			{Name: "context_path", Type: "string", Required: true, Help: "Mapping context path"},
			{Name: "mapping_index", Type: "int", Required: true, Help: "Mapping index"},
			{Name: "trigger_type", Type: "string", Required: true, Help: "Trigger type"},
			{Name: "properties", Type: "json", Help: "JSON object of trigger properties"},
		},
	},
	{
		Name:    "add_mapping_modifier",
		Group:   "input",
		Short:   "Add a modifier to a specific mapping",
		Long:    "Adds a modifier to a specific key mapping within a context. Per-mapping modifiers apply only to that key binding, useful for axis remapping (e.g., Negate on the S key for backward movement, SwizzleInputAxisValues on A/D for lateral).",
		Example: `ue-cli add_mapping_modifier --context-path /Game/Input/IMC_Default --mapping-index 1 --modifier-type Negate
ue-cli add_mapping_modifier --context-path /Game/Input/IMC_Default --mapping-index 2 --modifier-type SwizzleInputAxisValues --properties '{"Order":"YXZ"}'`,
		Params: []ParamSpec{
			{Name: "context_path", Type: "string", Required: true, Help: "Mapping context path"},
			{Name: "mapping_index", Type: "int", Required: true, Help: "Mapping index"},
			{Name: "modifier_type", Type: "string", Required: true, Help: "Modifier type"},
			{Name: "properties", Type: "json", Help: "JSON object of modifier properties"},
		},
	},
	{
		Name:    "remove_mapping_trigger",
		Group:   "input",
		Short:   "Remove a trigger from a mapping",
		Long:    "Removes a trigger from a specific key mapping by its zero-based trigger index. Use get_input_mapping_context to see the triggers on each mapping and identify the correct indices.",
		Example: "ue-cli remove_mapping_trigger --context-path /Game/Input/IMC_Default --mapping-index 0 --trigger-index 0",
		Params: []ParamSpec{
			{Name: "context_path", Type: "string", Required: true, Help: "Mapping context path"},
			{Name: "mapping_index", Type: "int", Required: true, Help: "Mapping index"},
			{Name: "trigger_index", Type: "int", Required: true, Help: "Trigger index"},
		},
	},
	{
		Name:    "remove_mapping_modifier",
		Group:   "input",
		Short:   "Remove a modifier from a mapping",
		Long:    "Removes a modifier from a specific key mapping by its zero-based modifier index. Use get_input_mapping_context to see the modifiers on each mapping and identify the correct indices.",
		Example: "ue-cli remove_mapping_modifier --context-path /Game/Input/IMC_Default --mapping-index 1 --modifier-index 0",
		Params: []ParamSpec{
			{Name: "context_path", Type: "string", Required: true, Help: "Mapping context path"},
			{Name: "mapping_index", Type: "int", Required: true, Help: "Mapping index"},
			{Name: "modifier_index", Type: "int", Required: true, Help: "Modifier index"},
		},
	},
	{
		Name:    "list_input_mapping_contexts",
		Group:   "input",
		Short:   "List all UInputMappingContext assets",
		Long:    "Lists all UInputMappingContext assets under the given path. Use to discover existing mapping contexts before creating new ones or adding key mappings.",
		Example: `ue-cli list_input_mapping_contexts
ue-cli list_input_mapping_contexts --path /Game/Input --filter "Vehicle"`,
		Params: []ParamSpec{
			{Name: "path", Type: "string", Default: "/Game", Help: "Search path"},
			{Name: "filter", Type: "string", Help: "Name filter"},
			{Name: "recursive", Type: "bool", Default: true, Help: "Search subdirectories"},
			{Name: "max_results", Type: "int", Default: 200, Help: "Maximum results"},
		},
	},

	// -- Discovery --
	{
		Name:    "list_trigger_types",
		Group:   "input",
		Short:   "List all UInputTrigger subclasses with properties",
		Long:    "Lists all available trigger types that can be used with add_input_action_trigger and add_mapping_trigger. Shows each trigger's configurable properties. Use filter to narrow results.",
		Example: `ue-cli list_trigger_types
ue-cli list_trigger_types --filter Hold`,
		Params: []ParamSpec{
			{Name: "filter", Type: "string", Help: "Name filter"},
		},
	},
	{
		Name:    "list_modifier_types",
		Group:   "input",
		Short:   "List all UInputModifier subclasses with properties",
		Long:    "Lists all available modifier types that can be used with add_input_action_modifier and add_mapping_modifier. Shows each modifier's configurable properties. Use filter to narrow results.",
		Example: `ue-cli list_modifier_types
ue-cli list_modifier_types --filter Dead`,
		Params: []ParamSpec{
			{Name: "filter", Type: "string", Help: "Name filter"},
		},
	},
	{
		Name:    "list_input_keys",
		Group:   "input",
		Short:   "List available FKey values (keyboard, mouse, gamepad)",
		Long:    "Lists all valid FKey names that can be used in add_key_mapping. Filter by category (Keyboard, Mouse, Gamepad, Touch) or by name substring. Key names are case-sensitive and must match exactly when used in mappings.",
		Example: `ue-cli list_input_keys --filter "Mouse"
ue-cli list_input_keys --category Gamepad
ue-cli list_input_keys --filter "Thumb" --max-results 20`,
		Params: []ParamSpec{
			{Name: "filter", Type: "string", Help: "Name filter"},
			{Name: "category", Type: "string", Help: "Category filter"},
			{Name: "max_results", Type: "int", Default: 500, Help: "Maximum results"},
		},
	},
}
