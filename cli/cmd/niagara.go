package cmd

func init() {
	ensureGroup("niagara", "Niagara VFX")
	registerCommands(niagaraCommands)
}

var niagaraCommands = []CommandSpec{
	// -- System Management --
	{
		Name:  "create_niagara_system",
		Group: "niagara",
		Short: "Create a new Niagara particle system",
		Long: "Creates a new Niagara system asset at the given path. Use template to start from a preset " +
			"(e.g., \"empty\" for a blank system, or a template name for pre-configured emitters). " +
			"Returns the created asset path. Call add_niagara_emitter afterward to populate it.",
		Example: `ue-cli create_niagara_system --asset-path /Game/VFX/NS_Explosion
ue-cli create_niagara_system --asset-path /Game/VFX/NS_Fire --template "Fountain"`,
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Content Browser path for the new system"},
			{Name: "template", Type: "string", Default: "empty", Help: "Template name: empty, Fountain, etc."},
		},
	},
	{
		Name:  "get_niagara_system_info",
		Group: "niagara",
		Short: "Get full info about a Niagara system",
		Long: "Returns detailed information about a Niagara system including its emitters, user parameters, " +
			"system settings, and compilation status. Use include to control what sections are returned " +
			"(all, emitters, parameters, compilation). Use filter to narrow emitters and parameters " +
			"by name substring.",
		Example: `ue-cli get_niagara_system_info --system-path /Game/VFX/NS_Explosion
ue-cli get_niagara_system_info --system-path /Game/VFX/NS_Fire --include emitters
ue-cli get_niagara_system_info --system-path /Game/VFX/NS_Fire --filter Spark`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Asset path of the Niagara system"},
			{Name: "include", Type: "string", Default: "all", Help: "Sections to include: all, emitters, parameters, compilation"},
			{Name: "filter", Type: "string", Help: "Substring filter on emitter and parameter names"},
		},
	},
	{
		Name:  "list_niagara_systems",
		Group: "niagara",
		Short: "List Niagara system assets",
		Long: "Lists all Niagara system assets under the given path. Use name_filter to narrow results by " +
			"name substring. Useful for discovering existing systems before creating new ones or for " +
			"finding systems to modify.",
		Example: `ue-cli list_niagara_systems
ue-cli list_niagara_systems --path /Game/VFX --name-filter "Fire"
ue-cli list_niagara_systems --max-results 50`,
		Params: []ParamSpec{
			{Name: "path", Type: "string", Default: "/Game", Help: "Content Browser path to search"},
			{Name: "name_filter", Type: "string", Help: "Filter systems by name substring"},
			{Name: "max_results", Type: "int", Default: 100, Help: "Maximum number of results"},
		},
	},
	{
		Name:  "delete_niagara_system",
		Group: "niagara",
		Short: "Delete a Niagara system asset",
		Long: "Deletes a Niagara system asset. Checks for references by default. Set force to true " +
			"to delete even if other assets reference this system. Use find_references first to " +
			"understand the impact of deletion.",
		Example: `ue-cli delete_niagara_system --system-path /Game/VFX/NS_OldEffect
ue-cli delete_niagara_system --system-path /Game/VFX/NS_Unused --force`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Asset path of the system to delete"},
			{Name: "force", Type: "bool", Help: "Delete even if referenced by other assets"},
		},
	},
	{
		Name:  "compile_niagara_system",
		Group: "niagara",
		Short: "Compile a Niagara system",
		Long: "Triggers compilation of a Niagara system. Compilation resolves all module scripts, " +
			"validates parameter bindings, and generates GPU compute shaders. Set wait_for_completion " +
			"to false for async compilation. Returns compilation status and any errors.",
		Example: `ue-cli compile_niagara_system --system-path /Game/VFX/NS_Explosion
ue-cli compile_niagara_system --system-path /Game/VFX/NS_Fire --wait-for-completion=false`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Asset path of the system to compile"},
			{Name: "wait_for_completion", Type: "bool", Default: true, Help: "Wait for compilation to finish"},
		},
	},

	// -- Emitter Management --
	{
		Name:  "get_niagara_emitters",
		Group: "niagara",
		Short: "List emitters in a Niagara system",
		Long: "Returns emitters in a Niagara system with their names, indices, enabled states, and " +
			"summary info (sim target, local_space, determinism, renderer_count). Use filter to " +
			"narrow results by emitter name substring.",
		Example: `ue-cli get_niagara_emitters --system-path /Game/VFX/NS_Explosion
ue-cli get_niagara_emitters --system-path /Game/VFX/NS_Fire --filter Spark`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Asset path of the Niagara system"},
			{Name: "filter", Type: "string", Help: "Substring filter on emitter names"},
		},
	},
	{
		Name:  "add_niagara_emitter",
		Group: "niagara",
		Short: "Add an emitter to a Niagara system",
		Long: "Adds an emitter to a Niagara system. Provide emitter_path to add from an existing emitter " +
			"asset, or template to add from a built-in template. Use emitter_name to set a custom " +
			"display name. If neither emitter_path nor template is given, adds an empty emitter.",
		Example: `ue-cli add_niagara_emitter --system-path /Game/VFX/NS_Fire --template "Fountain" --emitter-name "Sparks"
ue-cli add_niagara_emitter --system-path /Game/VFX/NS_Fire --emitter-path /Game/VFX/Emitters/NE_Smoke`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Asset path of the target Niagara system"},
			{Name: "emitter_path", Type: "string", Help: "Asset path of an existing emitter to add"},
			{Name: "emitter_name", Type: "string", Help: "Display name for the emitter"},
			{Name: "template", Type: "string", Help: "Built-in template name (e.g., Fountain, Sprite)"},
		},
	},
	{
		Name:  "remove_niagara_emitter",
		Group: "niagara",
		Short: "Remove an emitter from a Niagara system",
		Long: "Removes an emitter from a Niagara system by name. This also removes all modules, renderers, " +
			"and event handlers associated with the emitter. Use get_niagara_emitters to confirm the " +
			"exact emitter name before removal.",
		Example: "ue-cli remove_niagara_emitter --system-path /Game/VFX/NS_Fire --emitter-name Sparks",
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Asset path of the Niagara system"},
			{Name: "emitter_name", Type: "string", Required: true, Help: "Name of the emitter to remove"},
		},
	},
	{
		Name:  "set_niagara_emitter_property",
		Group: "niagara",
		Short: "Set a property on an emitter",
		Long: "Sets a property on an emitter within a Niagara system. Common properties include " +
			"SimTarget (CPUSim/GPUComputeSim), CalculateBoundsMode, FixedBounds, " +
			"bLocalSpace, bDeterminism. Value is JSON-encoded.",
		Example: `ue-cli set_niagara_emitter_property --system-path /Game/VFX/NS_Fire --emitter-name Sparks --property SimTarget --value '"GPUComputeSim"'
ue-cli set_niagara_emitter_property --system-path /Game/VFX/NS_Fire --emitter-name Sparks --property bLocalSpace --value true`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Asset path of the Niagara system"},
			{Name: "emitter_name", Type: "string", Required: true, Help: "Name of the emitter"},
			{Name: "property", Type: "string", Required: true, Help: "Property name to set"},
			{Name: "value", Type: "json", Required: true, Help: "JSON-encoded property value"},
		},
	},
	{
		Name:  "duplicate_niagara_emitter",
		Group: "niagara",
		Short: "Duplicate an emitter within a system",
		Long: "Creates a copy of an existing emitter within the same Niagara system. The duplicate " +
			"inherits all modules, renderers, and settings from the source. Optionally provide " +
			"new_name to rename the copy; otherwise it gets a default suffix.",
		Example: `ue-cli duplicate_niagara_emitter --system-path /Game/VFX/NS_Fire --emitter-name Sparks --new-name "Sparks_Large"`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Asset path of the Niagara system"},
			{Name: "emitter_name", Type: "string", Required: true, Help: "Name of the emitter to duplicate"},
			{Name: "new_name", Type: "string", Help: "Name for the duplicated emitter"},
		},
	},
	{
		Name:  "reorder_niagara_emitter",
		Group: "niagara",
		Short: "Move an emitter to a new position",
		Long: "Changes the execution order of an emitter within a Niagara system by moving it to a " +
			"new index. Emitter order affects rendering draw order (later emitters draw on top) " +
			"and can affect event handler resolution.",
		Example: "ue-cli reorder_niagara_emitter --system-path /Game/VFX/NS_Fire --emitter-name Sparks --new-index 0",
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Asset path of the Niagara system"},
			{Name: "emitter_name", Type: "string", Required: true, Help: "Name of the emitter to move"},
			{Name: "new_index", Type: "int", Required: true, Help: "New zero-based position index"},
		},
	},

	// -- Module Stack --
	{
		Name:  "get_niagara_modules",
		Group: "niagara",
		Short: "List modules in an emitter's stack",
		Long: "Returns modules in an emitter's script stack. Filter by script_usage to see only " +
			"specific stages: EmitterSpawn, EmitterUpdate, ParticleSpawn, ParticleUpdate, or all. " +
			"Use filter to narrow by module function name substring. " +
			"Set include_inputs to true to also return each module's input parameters and their current values.",
		Example: `ue-cli get_niagara_modules --system-path /Game/VFX/NS_Fire --emitter-name Sparks
ue-cli get_niagara_modules --system-path /Game/VFX/NS_Fire --emitter-name Sparks --script-usage ParticleUpdate
ue-cli get_niagara_modules --system-path /Game/VFX/NS_Fire --emitter-name Sparks --filter Color
ue-cli get_niagara_modules --system-path /Game/VFX/NS_Fire --emitter-name Sparks --include-inputs=false`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Asset path of the Niagara system"},
			{Name: "emitter_name", Type: "string", Required: true, Help: "Name of the emitter"},
			{Name: "script_usage", Type: "string", Default: "all", Help: "Stage filter: all, EmitterSpawn, EmitterUpdate, ParticleSpawn, ParticleUpdate"},
			{Name: "include_inputs", Type: "bool", Default: true, Help: "Include module input parameters"},
			{Name: "filter", Type: "string", Help: "Substring filter on module function names"},
		},
	},
	{
		Name:  "add_niagara_module",
		Group: "niagara",
		Short: "Add a module to an emitter's stack",
		Long: "Adds a module to an emitter's script stack at the specified stage. module_path is the " +
			"asset path of a Niagara module script or scratch pad module. Use list_niagara_modules to " +
			"discover available modules. Set index to -1 (default) to append at the end, or specify " +
			"a zero-based position to insert at.",
		Example: `ue-cli add_niagara_module --system-path /Game/VFX/NS_Fire --emitter-name Sparks --module-path /Niagara/Modules/ScaleSpriteSize --script-usage ParticleUpdate
ue-cli add_niagara_module --system-path /Game/VFX/NS_Fire --emitter-name Sparks --module-path /Niagara/Modules/AddVelocity --script-usage ParticleSpawn --index 0`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Asset path of the Niagara system"},
			{Name: "emitter_name", Type: "string", Required: true, Help: "Name of the emitter"},
			{Name: "module_path", Type: "string", Required: true, Help: "Asset path of the module script to add"},
			{Name: "script_usage", Type: "string", Required: true, Help: "Stage: EmitterSpawn, EmitterUpdate, ParticleSpawn, ParticleUpdate"},
			{Name: "index", Type: "int", Default: -1, Help: "Insert position (-1 = append at end)"},
		},
	},
	{
		Name:  "remove_niagara_module",
		Group: "niagara",
		Short: "Remove a module from an emitter's stack",
		Long: "Removes a module from an emitter's script stack by name and stage. Use get_niagara_modules " +
			"to find the exact module name and which script_usage stage it belongs to.",
		Example: "ue-cli remove_niagara_module --system-path /Game/VFX/NS_Fire --emitter-name Sparks --module-name ScaleSpriteSize --script-usage ParticleUpdate",
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Asset path of the Niagara system"},
			{Name: "emitter_name", Type: "string", Required: true, Help: "Name of the emitter"},
			{Name: "module_name", Type: "string", Required: true, Help: "Name of the module to remove"},
			{Name: "script_usage", Type: "string", Required: true, Help: "Stage the module belongs to"},
		},
	},
	{
		Name:  "set_niagara_module_enabled",
		Group: "niagara",
		Short: "Enable or disable a module",
		Long: "Enables or disables a module in an emitter's stack without removing it. Disabled modules " +
			"are skipped during simulation but their configuration is preserved. Useful for debugging " +
			"or A/B testing different module setups.",
		Example: `ue-cli set_niagara_module_enabled --system-path /Game/VFX/NS_Fire --emitter-name Sparks --module-name Drag --script-usage ParticleUpdate --enabled=false
ue-cli set_niagara_module_enabled --system-path /Game/VFX/NS_Fire --emitter-name Sparks --module-name Drag --script-usage ParticleUpdate --enabled`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Asset path of the Niagara system"},
			{Name: "emitter_name", Type: "string", Required: true, Help: "Name of the emitter"},
			{Name: "module_name", Type: "string", Required: true, Help: "Name of the module"},
			{Name: "script_usage", Type: "string", Required: true, Help: "Stage the module belongs to"},
			{Name: "enabled", Type: "bool", Required: true, Help: "True to enable, false to disable"},
		},
	},
	{
		Name:  "reorder_niagara_module",
		Group: "niagara",
		Short: "Move a module to a new position in stack",
		Long: "Changes the execution order of a module within its script stage. Module order within a " +
			"stage determines execution priority (earlier modules run first). Useful for ensuring " +
			"dependent calculations happen in the correct sequence.",
		Example: "ue-cli reorder_niagara_module --system-path /Game/VFX/NS_Fire --emitter-name Sparks --module-name Drag --script-usage ParticleUpdate --new-index 0",
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Asset path of the Niagara system"},
			{Name: "emitter_name", Type: "string", Required: true, Help: "Name of the emitter"},
			{Name: "module_name", Type: "string", Required: true, Help: "Name of the module to move"},
			{Name: "script_usage", Type: "string", Required: true, Help: "Stage the module belongs to"},
			{Name: "new_index", Type: "int", Required: true, Help: "New zero-based position index"},
		},
	},
	{
		Name:  "get_niagara_module_inputs",
		Group: "niagara",
		Short: "Get input parameters for a module",
		Long: "Returns all input parameters for a specific module in an emitter's stack, including " +
			"parameter names, types, current values, and whether they are bound to user parameters " +
			"or dynamic inputs. Use input_filter to narrow results by name substring.",
		Example: `ue-cli get_niagara_module_inputs --system-path /Game/VFX/NS_Fire --emitter-name Sparks --module-name AddVelocity --script-usage ParticleSpawn
ue-cli get_niagara_module_inputs --system-path /Game/VFX/NS_Fire --emitter-name Sparks --module-name AddVelocity --script-usage ParticleSpawn --input-filter "Velocity"`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Asset path of the Niagara system"},
			{Name: "emitter_name", Type: "string", Required: true, Help: "Name of the emitter"},
			{Name: "module_name", Type: "string", Required: true, Help: "Name of the module"},
			{Name: "script_usage", Type: "string", Required: true, Help: "Stage the module belongs to"},
			{Name: "input_filter", Type: "string", Help: "Filter inputs by name substring"},
		},
	},

	// -- Module Inputs --
	{
		Name:  "set_niagara_module_input",
		Group: "niagara",
		Short: "Set an input value on a module",
		Long: "Sets a static input value on a module. The value is JSON-encoded and must match the " +
			"parameter type (float for scalars, array for vectors, object for structs). Use " +
			"get_niagara_module_inputs to discover available inputs and their types. For dynamic " +
			"values (random ranges, curves, linked parameters), use set_niagara_dynamic_input instead.",
		Example: `ue-cli set_niagara_module_input --system-path /Game/VFX/NS_Fire --emitter-name Sparks --module-name SpawnRate --input-name SpawnRate --value 500 --script-usage EmitterUpdate
ue-cli set_niagara_module_input --system-path /Game/VFX/NS_Fire --emitter-name Sparks --module-name AddVelocity --input-name "Velocity" --value '{"X": 0, "Y": 0, "Z": 200}' --script-usage ParticleSpawn`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Asset path of the Niagara system"},
			{Name: "emitter_name", Type: "string", Required: true, Help: "Name of the emitter"},
			{Name: "module_name", Type: "string", Required: true, Help: "Name of the module"},
			{Name: "input_name", Type: "string", Required: true, Help: "Name of the input parameter"},
			{Name: "value", Type: "json", Required: true, Help: "JSON-encoded value matching the parameter type"},
			{Name: "script_usage", Type: "string", Required: true, Help: "Stage the module belongs to"},
		},
	},
	{
		Name:  "set_niagara_dynamic_input",
		Group: "niagara",
		Short: "Set a dynamic input on a module",
		Long: "Assigns a dynamic input to a module parameter. Dynamic inputs provide runtime-computed values: " +
			"UniformRangedFloat/Vector for random ranges (min_value/max_value), CurveFloat/Vector " +
			"for time-based curves, LinkedParameter to bind to a user or engine parameter, " +
			"CustomHLSL for arbitrary expression code. Not all fields apply to every type.",
		Example: `ue-cli set_niagara_dynamic_input --system-path /Game/VFX/NS_Fire --emitter-name Sparks --module-name InitialSize --input-name "SpriteSize" --script-usage ParticleSpawn --dynamic-input-type UniformRangedFloat --min-value 5.0 --max-value 15.0
ue-cli set_niagara_dynamic_input --system-path /Game/VFX/NS_Fire --emitter-name Sparks --module-name Color --input-name "Color" --script-usage ParticleUpdate --dynamic-input-type LinkedParameter --parameter-name "User.ParticleColor"`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Asset path of the Niagara system"},
			{Name: "emitter_name", Type: "string", Required: true, Help: "Name of the emitter"},
			{Name: "module_name", Type: "string", Required: true, Help: "Name of the module"},
			{Name: "input_name", Type: "string", Required: true, Help: "Name of the input parameter"},
			{Name: "script_usage", Type: "string", Required: true, Help: "Stage the module belongs to"},
			{Name: "dynamic_input_type", Type: "string", Required: true, Help: "Type: UniformRangedFloat, UniformRangedVector, CurveFloat, LinkedParameter, CustomHLSL, etc."},
			{Name: "min_value", Type: "json", Help: "Minimum value for ranged types"},
			{Name: "max_value", Type: "json", Help: "Maximum value for ranged types"},
			{Name: "parameter_name", Type: "string", Help: "Parameter name for LinkedParameter type"},
			{Name: "expression", Type: "string", Help: "HLSL expression for CustomHLSL type"},
		},
	},
	{
		Name:  "set_niagara_curve",
		Group: "niagara",
		Short: "Set a curve on a module input",
		Long: "Assigns a curve to a module input parameter. curve_type specifies the curve dimensionality " +
			"(Float, Vector2, Vector3, Vector4, LinearColor). keys is a JSON array of key objects " +
			"with time and value fields. Use for time-varying properties like color over life, " +
			"size over life, or velocity curves.",
		Example: `ue-cli set_niagara_curve --system-path /Game/VFX/NS_Fire --emitter-name Flame --module-name ScaleColor --input-name "Scale Factor" --script-usage ParticleUpdate --curve-type Float --keys '[{"time": 0, "value": 0}, {"time": 0.2, "value": 1}, {"time": 1, "value": 0}]'
ue-cli set_niagara_curve --system-path /Game/VFX/NS_Fire --emitter-name Flame --module-name Color --input-name "Color" --script-usage ParticleUpdate --curve-type LinearColor --keys '[{"time": 0, "value": [1,0.5,0,1]}, {"time": 1, "value": [0.2,0,0,0]}]'`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Asset path of the Niagara system"},
			{Name: "emitter_name", Type: "string", Required: true, Help: "Name of the emitter"},
			{Name: "module_name", Type: "string", Required: true, Help: "Name of the module"},
			{Name: "input_name", Type: "string", Required: true, Help: "Name of the input parameter"},
			{Name: "script_usage", Type: "string", Required: true, Help: "Stage the module belongs to"},
			{Name: "curve_type", Type: "string", Required: true, Help: "Curve type: Float, Vector2, Vector3, Vector4, LinearColor"},
			{Name: "keys", Type: "json", Required: true, Help: "JSON array of curve keys [{time, value}, ...]"},
		},
	},

	// -- User Parameters --
	{
		Name:  "get_niagara_user_parameters",
		Group: "niagara",
		Short: "Get user-exposed parameters",
		Long: "Returns user-exposed parameters on a Niagara system asset or on a placed Niagara " +
			"component in the level. Provide system_path to read the asset definition, or actor_name " +
			"to read runtime overrides on a placed instance. Shows parameter names, types, and value types. " +
			"Use filter to narrow by parameter name substring.",
		Example: `ue-cli get_niagara_user_parameters --system-path /Game/VFX/NS_Fire
ue-cli get_niagara_user_parameters --actor-name "NS_Fire_Instance"
ue-cli get_niagara_user_parameters --system-path /Game/VFX/NS_Fire --filter Color`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Help: "Asset path of the Niagara system"},
			{Name: "actor_name", Type: "string", Help: "Name of a placed Niagara actor in the level"},
			{Name: "filter", Type: "string", Help: "Substring filter on parameter names"},
		},
	},
	{
		Name:  "add_niagara_user_parameter",
		Group: "niagara",
		Short: "Add a user parameter to a system",
		Long: "Adds a new user-exposed parameter to a Niagara system. User parameters appear in the " +
			"Details panel when the system is placed in a level and can be set per-instance. " +
			"parameter_type is the Niagara type name (Float, Vector, LinearColor, Bool, Int32, etc.). " +
			"Optionally provide a JSON-encoded default_value.",
		Example: `ue-cli add_niagara_user_parameter --system-path /Game/VFX/NS_Fire --parameter-name "Intensity" --parameter-type Float --default-value 1.0
ue-cli add_niagara_user_parameter --system-path /Game/VFX/NS_Fire --parameter-name "ParticleColor" --parameter-type LinearColor --default-value '[1, 0.5, 0, 1]'`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Asset path of the Niagara system"},
			{Name: "parameter_name", Type: "string", Required: true, Help: "Name for the new parameter"},
			{Name: "parameter_type", Type: "string", Required: true, Help: "Niagara type: Float, Vector, LinearColor, Bool, Int32, etc."},
			{Name: "default_value", Type: "json", Help: "JSON-encoded default value"},
		},
	},
	{
		Name:  "set_niagara_user_parameter",
		Group: "niagara",
		Short: "Set a user parameter value",
		Long: "Sets the value of a user-exposed parameter. When actor_name is provided, sets the " +
			"per-instance override on a placed Niagara component. When system_path is provided, " +
			"sets the default value on the asset. parameter_type must match the parameter's declared type.",
		Example: `ue-cli set_niagara_user_parameter --actor-name "NS_Fire_Instance" --parameter-name "Intensity" --parameter-type Float --value 2.5
ue-cli set_niagara_user_parameter --system-path /Game/VFX/NS_Fire --parameter-name "ParticleColor" --parameter-type LinearColor --value '[0, 1, 0, 1]'`,
		Params: []ParamSpec{
			{Name: "parameter_name", Type: "string", Required: true, Help: "Name of the user parameter"},
			{Name: "parameter_type", Type: "string", Required: true, Help: "Niagara type: Float, Vector, LinearColor, Bool, Int32, etc."},
			{Name: "value", Type: "json", Required: true, Help: "JSON-encoded parameter value"},
			{Name: "actor_name", Type: "string", Help: "Name of a placed Niagara actor for per-instance override"},
			{Name: "system_path", Type: "string", Help: "Asset path to set the default value"},
		},
	},
	{
		Name:  "remove_niagara_user_parameter",
		Group: "niagara",
		Short: "Remove a user parameter from a system",
		Long: "Removes a user-exposed parameter from a Niagara system. Any module inputs linked to " +
			"this parameter will be unlinked. Placed instances that override this parameter will " +
			"lose their overrides. Use get_niagara_user_parameters to confirm the parameter name.",
		Example: "ue-cli remove_niagara_user_parameter --system-path /Game/VFX/NS_Fire --parameter-name Intensity",
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Asset path of the Niagara system"},
			{Name: "parameter_name", Type: "string", Required: true, Help: "Name of the parameter to remove"},
		},
	},
	{
		Name:  "link_niagara_parameter",
		Group: "niagara",
		Short: "Link a module input to a parameter",
		Long: "Links a module input to a Niagara parameter by namespace path. This makes the module " +
			"input read its value from the linked parameter at runtime instead of using a static value. " +
			"Common parameter paths include User.ParamName for user parameters, Emitter.Age, " +
			"Particles.Lifetime, Engine.DeltaTime, etc.",
		Example: `ue-cli link_niagara_parameter --system-path /Game/VFX/NS_Fire --emitter-name Sparks --module-name ScaleColor --input-name "Scale Factor" --script-usage ParticleUpdate --linked-parameter "User.Intensity"
ue-cli link_niagara_parameter --system-path /Game/VFX/NS_Fire --emitter-name Sparks --module-name Color --input-name "Color" --script-usage ParticleUpdate --linked-parameter "User.ParticleColor"`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Asset path of the Niagara system"},
			{Name: "emitter_name", Type: "string", Required: true, Help: "Name of the emitter"},
			{Name: "module_name", Type: "string", Required: true, Help: "Name of the module"},
			{Name: "input_name", Type: "string", Required: true, Help: "Name of the input parameter to link"},
			{Name: "script_usage", Type: "string", Required: true, Help: "Stage the module belongs to"},
			{Name: "linked_parameter", Type: "string", Required: true, Help: "Namespace path of the parameter to link (e.g., User.Intensity)"},
		},
	},

	// -- Renderers --
	{
		Name:  "add_niagara_renderer",
		Group: "niagara",
		Short: "Add a renderer to an emitter",
		Long: "Adds a renderer to an emitter. renderer_type specifies the kind: SpriteRenderer, " +
			"MeshRenderer, RibbonRenderer, LightRenderer, ComponentRenderer, etc. " +
			"Optionally set the material and mesh at creation time. Use set_niagara_renderer_property " +
			"and set_niagara_renderer_binding to configure further.",
		Example: `ue-cli add_niagara_renderer --system-path /Game/VFX/NS_Fire --emitter-name Sparks --renderer-type SpriteRenderer --material-path /Game/Materials/M_Spark
ue-cli add_niagara_renderer --system-path /Game/VFX/NS_Fire --emitter-name Debris --renderer-type MeshRenderer --mesh-path /Game/Meshes/SM_Chunk --material-path /Game/Materials/M_Rock`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Asset path of the Niagara system"},
			{Name: "emitter_name", Type: "string", Required: true, Help: "Name of the emitter"},
			{Name: "renderer_type", Type: "string", Required: true, Help: "Type: SpriteRenderer, MeshRenderer, RibbonRenderer, LightRenderer, ComponentRenderer"},
			{Name: "material_path", Type: "string", Help: "Material asset path to assign"},
			{Name: "mesh_path", Type: "string", Help: "Static mesh asset path (for MeshRenderer)"},
		},
	},
	{
		Name:  "remove_niagara_renderer",
		Group: "niagara",
		Short: "Remove a renderer from an emitter",
		Long: "Removes a renderer from an emitter by its zero-based index. An emitter can have multiple " +
			"renderers; use get_niagara_emitters or get_niagara_renderer_info to see their indices.",
		Example: "ue-cli remove_niagara_renderer --system-path /Game/VFX/NS_Fire --emitter-name Sparks --renderer-index 1",
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Asset path of the Niagara system"},
			{Name: "emitter_name", Type: "string", Required: true, Help: "Name of the emitter"},
			{Name: "renderer_index", Type: "int", Required: true, Help: "Zero-based index of the renderer to remove"},
		},
	},
	{
		Name:  "get_niagara_renderer_info",
		Group: "niagara",
		Short: "Get renderer properties and bindings",
		Long: "Returns detailed information about a renderer on an emitter, including its type, " +
			"material, mesh (if applicable), all configurable properties, and all attribute bindings " +
			"(which particle attributes drive which rendering features). Use to inspect before modifying.",
		Example: `ue-cli get_niagara_renderer_info --system-path /Game/VFX/NS_Fire --emitter-name Sparks
ue-cli get_niagara_renderer_info --system-path /Game/VFX/NS_Fire --emitter-name Sparks --renderer-index 1`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Asset path of the Niagara system"},
			{Name: "emitter_name", Type: "string", Required: true, Help: "Name of the emitter"},
			{Name: "renderer_index", Type: "int", Default: 0, Help: "Zero-based renderer index"},
		},
	},
	{
		Name:  "set_niagara_renderer_property",
		Group: "niagara",
		Short: "Set a property on a renderer",
		Long: "Sets a property on a renderer. Common properties include SortMode, SubImageSize, " +
			"Material, Alignment, FacingMode, SourceMode (for ribbons), and mesh-specific " +
			"settings. Use get_niagara_renderer_info to discover available properties.",
		Example: `ue-cli set_niagara_renderer_property --system-path /Game/VFX/NS_Fire --emitter-name Sparks --property SortMode --value '"ViewDistance"'
ue-cli set_niagara_renderer_property --system-path /Game/VFX/NS_Fire --emitter-name Sparks --property SubImageSize --value '{"X": 4, "Y": 4}'`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Asset path of the Niagara system"},
			{Name: "emitter_name", Type: "string", Required: true, Help: "Name of the emitter"},
			{Name: "property", Type: "string", Required: true, Help: "Property name to set"},
			{Name: "value", Type: "json", Required: true, Help: "JSON-encoded property value"},
			{Name: "renderer_index", Type: "int", Default: 0, Help: "Zero-based renderer index"},
		},
	},
	{
		Name:  "set_niagara_renderer_binding",
		Group: "niagara",
		Short: "Bind a renderer slot to a particle attribute",
		Long: "Binds a renderer attribute slot to a particle attribute. Bindings control which particle " +
			"data drives rendering features. Common binding_name values: PositionBinding, ColorBinding, " +
			"SpriteRotationBinding, SpriteSizeBinding, VelocityBinding, DynamicMaterialBinding, " +
			"NormalizedAgeBinding, etc.",
		Example: `ue-cli set_niagara_renderer_binding --system-path /Game/VFX/NS_Fire --emitter-name Sparks --binding-name ColorBinding --attribute "Particles.Color"
ue-cli set_niagara_renderer_binding --system-path /Game/VFX/NS_Fire --emitter-name Sparks --binding-name SpriteSizeBinding --attribute "Particles.SpriteSize"`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Asset path of the Niagara system"},
			{Name: "emitter_name", Type: "string", Required: true, Help: "Name of the emitter"},
			{Name: "binding_name", Type: "string", Required: true, Help: "Renderer binding slot name (e.g., ColorBinding, SpriteSizeBinding)"},
			{Name: "attribute", Type: "string", Required: true, Help: "Particle attribute path (e.g., Particles.Color)"},
			{Name: "renderer_index", Type: "int", Default: 0, Help: "Zero-based renderer index"},
		},
	},

	// -- Scratch Pad & Module Assets --
	{
		Name:  "create_niagara_scratch_pad_module",
		Group: "niagara",
		Short: "Create a scratch pad module on a system",
		Long: "Creates a new scratch pad module within a Niagara system. Scratch pad modules are system-local " +
			"scripts that can be used across emitters within that system. They are useful for prototyping " +
			"custom logic before extracting to a standalone module asset. Specify the script_usage stage " +
			"and an optional module_name.",
		Example: `ue-cli create_niagara_scratch_pad_module --system-path /Game/VFX/NS_Fire --script-usage ParticleUpdate --module-name "CustomDrag"`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Asset path of the Niagara system"},
			{Name: "script_usage", Type: "string", Required: true, Help: "Stage: EmitterSpawn, EmitterUpdate, ParticleSpawn, ParticleUpdate"},
			{Name: "module_name", Type: "string", Help: "Display name for the scratch pad module"},
		},
	},
	{
		Name:  "set_niagara_scratch_pad_hlsl",
		Group: "niagara",
		Short: "Set HLSL code on a scratch pad module",
		Long: "Sets the HLSL code body, input definitions, and output definitions on a scratch pad module. " +
			"inputs is a JSON array of {name, type} objects defining input pins. outputs is a JSON " +
			"array of {name, type} objects defining output pins. The HLSL code runs per-particle or " +
			"per-emitter depending on the module's script_usage.",
		Example: `ue-cli set_niagara_scratch_pad_hlsl --system-path /Game/VFX/NS_Fire --module-name "CustomDrag" --hlsl-code "OutVelocity = Velocity * (1.0 - DragCoeff * DeltaTime);" --inputs '[{"name":"Velocity","type":"Vector"},{"name":"DragCoeff","type":"Float"}]' --outputs '[{"name":"OutVelocity","type":"Vector"}]'`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Asset path of the Niagara system"},
			{Name: "module_name", Type: "string", Required: true, Help: "Name of the scratch pad module"},
			{Name: "hlsl_code", Type: "string", Required: true, Help: "HLSL code body for the module"},
			{Name: "inputs", Type: "json", Help: "JSON array of input definitions [{name, type}, ...]"},
			{Name: "outputs", Type: "json", Help: "JSON array of output definitions [{name, type}, ...]"},
		},
	},
	{
		Name:  "create_niagara_module_asset",
		Group: "niagara",
		Short: "Create a standalone Niagara module asset",
		Long: "Creates a new standalone Niagara module script asset. Unlike scratch pad modules, module " +
			"assets can be shared across systems. Set expose_to_library to true to make it appear in " +
			"the module browser. Use set_niagara_scratch_pad_hlsl afterward to set its code.",
		Example: `ue-cli create_niagara_module_asset --asset-path /Game/VFX/Modules/NM_CustomForce --script-usage ParticleUpdate --description "Applies custom force based on distance from origin"
ue-cli create_niagara_module_asset --asset-path /Game/VFX/Modules/NM_WindEffect --script-usage ParticleUpdate --expose-to-library`,
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Content Browser path for the new module asset"},
			{Name: "script_usage", Type: "string", Required: true, Help: "Stage: EmitterSpawn, EmitterUpdate, ParticleSpawn, ParticleUpdate"},
			{Name: "description", Type: "string", Help: "Description shown in the module browser"},
			{Name: "expose_to_library", Type: "bool", Default: true, Help: "Make visible in the module browser"},
		},
	},

	// -- Events & Simulation Stages --
	{
		Name:  "add_niagara_event_handler",
		Group: "niagara",
		Short: "Add an event handler to an emitter",
		Long: "Adds an event handler that responds to events generated by another emitter. execution_mode " +
			"controls how the handler runs: every_particle (run on each existing particle), " +
			"spawned_particles (spawn new particles per event). Use for collision responses, death " +
			"events, or inter-emitter communication.",
		Example: `ue-cli add_niagara_event_handler --system-path /Game/VFX/NS_Fire --emitter-name Sparks --source-emitter Flame --event-name "CollisionEvent" --execution-mode spawned_particles --spawn-number 3
ue-cli add_niagara_event_handler --system-path /Game/VFX/NS_Fire --emitter-name Smoke --source-emitter Flame --event-name "DeathEvent" --execution-mode every_particle`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Asset path of the Niagara system"},
			{Name: "emitter_name", Type: "string", Required: true, Help: "Name of the emitter receiving the event"},
			{Name: "source_emitter", Type: "string", Required: true, Help: "Name of the emitter generating the event"},
			{Name: "event_name", Type: "string", Required: true, Help: "Event type name (e.g., CollisionEvent, DeathEvent)"},
			{Name: "execution_mode", Type: "string", Default: "every_particle", Help: "Mode: every_particle, spawned_particles"},
			{Name: "spawn_number", Type: "int", Default: 0, Help: "Number of particles to spawn per event (for spawned_particles mode)"},
			{Name: "max_events_per_frame", Type: "int", Default: 0, Help: "Max events per frame (0 = unlimited)"},
		},
	},
	{
		Name:  "add_niagara_simulation_stage",
		Group: "niagara",
		Short: "Add a simulation stage to an emitter",
		Long: "Adds a custom simulation stage to an emitter. Simulation stages run additional passes " +
			"after the main ParticleUpdate, useful for multi-pass algorithms like fluid simulation, " +
			"neighbor searches, or iterative constraint solving. iteration_source can be particles " +
			"or a data interface.",
		Example: `ue-cli add_niagara_simulation_stage --system-path /Game/VFX/NS_Fluid --emitter-name Particles --stage-name "PressureSolve" --iteration-source particles --num-iterations 4
ue-cli add_niagara_simulation_stage --system-path /Game/VFX/NS_Cloth --emitter-name Vertices --stage-name "ConstraintSolve" --num-iterations 8`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Asset path of the Niagara system"},
			{Name: "emitter_name", Type: "string", Required: true, Help: "Name of the emitter"},
			{Name: "stage_name", Type: "string", Required: true, Help: "Display name for the simulation stage"},
			{Name: "iteration_source", Type: "string", Default: "particles", Help: "Iteration source: particles or a data interface name"},
			{Name: "num_iterations", Type: "int", Default: 1, Help: "Number of iterations per frame"},
		},
	},
	{
		Name:  "get_niagara_event_handlers",
		Group: "niagara",
		Short: "List event handlers on an emitter",
		Long: "Returns all event handlers configured on an emitter, including source emitter, event " +
			"type, execution mode, spawn number, and associated module stacks. Use before adding " +
			"or modifying event handlers.",
		Example: "ue-cli get_niagara_event_handlers --system-path /Game/VFX/NS_Fire --emitter-name Sparks",
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Asset path of the Niagara system"},
			{Name: "emitter_name", Type: "string", Required: true, Help: "Name of the emitter"},
		},
	},

	// -- Runtime --
	{
		Name:  "spawn_niagara_effect",
		Group: "niagara",
		Short: "Spawn a Niagara effect in the level",
		Long: "Spawns a Niagara system as an actor in the current level at the specified location. " +
			"Returns the spawned actor name. Set auto_activate to false to spawn deactivated " +
			"(use control_niagara_effect to activate later). Optionally provide rotation, scale, " +
			"and a custom actor name.",
		Example: `ue-cli spawn_niagara_effect --system-path /Game/VFX/NS_Explosion --location '{"X": 100, "Y": 200, "Z": 0}'
ue-cli spawn_niagara_effect --system-path /Game/VFX/NS_Fire --location '{"X": 0, "Y": 0, "Z": 50}' --scale '{"X": 2, "Y": 2, "Z": 2}' --name "CampfireVFX"
ue-cli spawn_niagara_effect --system-path /Game/VFX/NS_Ambient --location '{"X": 0, "Y": 0, "Z": 0}' --auto-activate=false`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Asset path of the Niagara system to spawn"},
			{Name: "location", Type: "json", Help: "JSON object {X, Y, Z} for world position"},
			{Name: "rotation", Type: "json", Help: "JSON object {Pitch, Yaw, Roll} for rotation"},
			{Name: "scale", Type: "json", Help: "JSON object {X, Y, Z} for scale"},
			{Name: "name", Type: "string", Help: "Custom actor label"},
			{Name: "auto_activate", Type: "bool", Default: true, Help: "Activate the effect immediately on spawn"},
		},
	},
	{
		Name:  "control_niagara_effect",
		Group: "niagara",
		Short: "Control a spawned Niagara effect",
		Long: "Controls a Niagara effect actor in the level. action can be: activate (start playing), " +
			"deactivate (stop with completion), kill (stop immediately), reset (restart from beginning), " +
			"or toggle (flip active state). Use get_niagara_actors to find effect actor names.",
		Example: `ue-cli control_niagara_effect --actor-name "NS_Fire_Instance" --action activate
ue-cli control_niagara_effect --actor-name "CampfireVFX" --action kill
ue-cli control_niagara_effect --actor-name "NS_Ambient_0" --action reset`,
		Params: []ParamSpec{
			{Name: "actor_name", Type: "string", Required: true, Help: "Name of the Niagara actor in the level"},
			{Name: "action", Type: "string", Required: true, Help: "Action: activate, deactivate, kill, reset, toggle"},
		},
	},
	{
		Name:  "add_niagara_component",
		Group: "niagara",
		Short: "Add a Niagara component to an actor",
		Long: "Adds a UNiagaraComponent to an existing actor in the level. The component plays the " +
			"specified Niagara system as a child of the actor. Use relative_location to offset the " +
			"effect from the actor's root. Set auto_activate to false if you want to trigger it later.",
		Example: `ue-cli add_niagara_component --actor-name "Torch_BP" --system-path /Game/VFX/NS_TorchFlame --component-name "FlameVFX" --relative-location '{"X": 0, "Y": 0, "Z": 100}'
ue-cli add_niagara_component --actor-name "PlayerCharacter" --system-path /Game/VFX/NS_FootDust --auto-activate=false`,
		Params: []ParamSpec{
			{Name: "actor_name", Type: "string", Required: true, Help: "Name of the target actor in the level"},
			{Name: "system_path", Type: "string", Required: true, Help: "Asset path of the Niagara system"},
			{Name: "component_name", Type: "string", Help: "Name for the new component"},
			{Name: "relative_location", Type: "json", Help: "JSON object {X, Y, Z} offset from actor root"},
			{Name: "auto_activate", Type: "bool", Default: true, Help: "Activate the component immediately"},
		},
	},
	{
		Name:  "get_niagara_actors",
		Group: "niagara",
		Short: "List Niagara effect actors in the level",
		Long: "Returns all Niagara system actors currently placed in the level. Optionally filter by " +
			"system asset path to find instances of a specific effect. Shows actor name, system path, " +
			"location, and active state.",
		Example: `ue-cli get_niagara_actors
ue-cli get_niagara_actors --system-filter "/Game/VFX/NS_Fire"`,
		Params: []ParamSpec{
			{Name: "system_filter", Type: "string", Help: "Filter by Niagara system asset path"},
		},
	},

	// -- Discovery --
	{
		Name:  "list_niagara_modules",
		Group: "niagara",
		Short: "List available Niagara modules",
		Long: "Lists Niagara module scripts available in the project and engine. Filter by category " +
			"(all, spawn, update, common) or search by name substring. Returns module asset paths, " +
			"descriptions, and compatible script usages. Use to discover modules before adding them " +
			"to emitters with add_niagara_module.",
		Example: `ue-cli list_niagara_modules
ue-cli list_niagara_modules --category spawn --search "Velocity"
ue-cli list_niagara_modules --search "Color" --max-results 20`,
		Params: []ParamSpec{
			{Name: "category", Type: "string", Default: "all", Help: "Category filter: all, spawn, update, common"},
			{Name: "search", Type: "string", Help: "Search modules by name substring"},
			{Name: "max_results", Type: "int", Default: 100, Help: "Maximum number of results"},
		},
	},
	{
		Name:  "list_niagara_emitter_templates",
		Group: "niagara",
		Short: "List available emitter templates",
		Long: "Lists built-in and project emitter templates that can be used with add_niagara_emitter. " +
			"Templates provide pre-configured emitters with modules and renderers for common effects " +
			"like fountains, sprites, mesh particles, ribbons, etc. Filter by category to narrow results.",
		Example: `ue-cli list_niagara_emitter_templates
ue-cli list_niagara_emitter_templates --category sprite`,
		Params: []ParamSpec{
			{Name: "category", Type: "string", Default: "all", Help: "Category filter: all, sprite, mesh, ribbon, gpu, etc."},
		},
	},
	{
		Name:  "list_niagara_data_interfaces",
		Group: "niagara",
		Short: "List Niagara data interface types",
		Long: "Lists available Niagara data interface types. Data interfaces provide external data " +
			"to Niagara simulations (meshes, textures, audio, physics, landscape, splines, etc.). " +
			"Use filter to search by name. Returns type names and descriptions.",
		Example: `ue-cli list_niagara_data_interfaces
ue-cli list_niagara_data_interfaces --filter "Mesh"`,
		Params: []ParamSpec{
			{Name: "filter", Type: "string", Help: "Filter data interfaces by name substring"},
		},
	},
	{
		Name:  "list_niagara_parameter_types",
		Group: "niagara",
		Short: "List Niagara parameter types by scope",
		Long: "Lists valid Niagara parameter types for a given scope. scope controls which namespace " +
			"to list: user (User.* parameters), particle (Particles.* attributes), emitter " +
			"(Emitter.* variables), system (System.* variables), or engine (Engine.* built-ins). " +
			"Use filter to narrow results by name.",
		Example: `ue-cli list_niagara_parameter_types
ue-cli list_niagara_parameter_types --scope particle --filter "Color"
ue-cli list_niagara_parameter_types --scope engine`,
		Params: []ParamSpec{
			{Name: "scope", Type: "string", Default: "user", Help: "Parameter scope: user, particle, emitter, system, engine"},
			{Name: "filter", Type: "string", Help: "Filter parameter types by name substring"},
		},
	},
	{
		Name:  "get_niagara_emitter_attributes",
		Group: "niagara",
		Short: "Get attributes available on an emitter",
		Long: "Returns attributes available on an emitter including rapid iteration parameters and " +
			"well-known particle attributes (Position, Velocity, Color, etc.). Shows names, types, " +
			"scope, and source. Use filter to narrow by attribute name substring.",
		Example: `ue-cli get_niagara_emitter_attributes --system-path /Game/VFX/NS_Fire --emitter-name Sparks
ue-cli get_niagara_emitter_attributes --system-path /Game/VFX/NS_Fire --emitter-name Sparks --scope particle
ue-cli get_niagara_emitter_attributes --system-path /Game/VFX/NS_Fire --emitter-name Sparks --filter Color`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Asset path of the Niagara system"},
			{Name: "emitter_name", Type: "string", Required: true, Help: "Name of the emitter"},
			{Name: "scope", Type: "string", Default: "particle", Help: "Attribute scope: particle, emitter, system, engine"},
			{Name: "filter", Type: "string", Help: "Substring filter on attribute names"},
		},
	},

	// -- Renderer Property Discovery --
	{
		Name:    "get_niagara_renderer_properties",
		Group:   "niagara",
		Short:   "List editable properties on a renderer with current values",
		Long:    "Returns all editable UPROPERTY fields on a renderer (sprite, ribbon, mesh, light, component) with their current values and valid enum options. Use filter to get specific properties — strongly recommended to save tokens. Property names can be used directly with set_niagara_renderer_property.",
		Example: `ue-cli get_niagara_renderer_properties --system-path /Game/VFX/NS_Fire --emitter-name Sparks --filter Facing
ue-cli get_niagara_renderer_properties --system-path /Game/VFX/NS_Fire --emitter-name Sparks --filter Material
ue-cli get_niagara_renderer_properties --system-path /Game/VFX/NS_Fire --emitter-name Sparks --filter Sort`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Asset path of the Niagara system"},
			{Name: "emitter_name", Type: "string", Required: true, Help: "Name of the emitter"},
			{Name: "renderer_index", Type: "int", Default: 0, Help: "Renderer index (default 0)"},
			{Name: "filter", Type: "string", Help: "Substring filter on property name (e.g. Facing, Material, Sort, Shadow)"},
		},
	},

	// -- System Properties --
	{
		Name:    "set_niagara_system_property",
		Group:   "niagara",
		Short:   "Set a system-level property via reflection",
		Long:    "Sets any UPROPERTY on UNiagaraSystem: WarmupTime, WarmupTickDelta, bDeterminism, RandomSeed, bFixedBounds, FixedBounds, bFixedTickDelta, FixedTickDeltaTime, etc. Use the exact C++ property name (PascalCase). For booleans, can auto-add 'b' prefix.",
		Example: `ue-cli set_niagara_system_property --system-path /Game/VFX/NS_Fire --property WarmupTime --value 2.0
ue-cli set_niagara_system_property --system-path /Game/VFX/NS_Fire --property bDeterminism --value true
ue-cli set_niagara_system_property --system-path /Game/VFX/NS_Fire --property RandomSeed --value 42`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Asset path of the Niagara system"},
			{Name: "property", Type: "string", Required: true, Help: "Property name (PascalCase, e.g. WarmupTime, bDeterminism)"},
			{Name: "value", Type: "string", Required: true, Help: "Value as string"},
		},
	},

	// -- Rapid Iteration Parameters --
	{
		Name:    "get_niagara_rapid_iteration_parameters",
		Group:   "niagara",
		Short:   "Get actual module input values (spawn rate, colors, forces, etc.)",
		Long:    "Returns all rapid iteration parameters — the real configurable values on Niagara modules. These include spawn rate, gravity, colors, sizes, lifetimes, and every other editable module input. Each parameter includes its module name, input name, current value, and type. Use the filter parameter to narrow results.",
		Example: `ue-cli get_niagara_rapid_iteration_parameters --system-path /Game/VFX/NS_Fire --emitter-name Sparks
ue-cli get_niagara_rapid_iteration_parameters --system-path /Game/VFX/NS_Fire --emitter-name Sparks --filter SpawnRate
ue-cli get_niagara_rapid_iteration_parameters --system-path /Game/VFX/NS_Fire --emitter-name Sparks --script-usage particle_spawn --filter Color`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Asset path of the Niagara system"},
			{Name: "emitter_name", Type: "string", Required: true, Help: "Name of the emitter"},
			{Name: "script_usage", Type: "string", Default: "all", Help: "Stack filter: emitter_spawn, emitter_update, particle_spawn, particle_update, or all"},
			{Name: "filter", Type: "string", Help: "Substring filter on parameter name (e.g. Color, SpawnRate, Gravity)"},
		},
	},
	{
		Name:    "set_niagara_rapid_iteration_parameter",
		Group:   "niagara",
		Short:   "Set a module input value (spawn rate, color, force, size, etc.)",
		Long:    "Sets a rapid iteration parameter value on a Niagara module. This is the primary way to change module inputs like spawn rate, colors, forces, sizes, and lifetimes. Use get_niagara_rapid_iteration_parameters first to discover available parameters. Value format: float (5.0), int (10), bool (true), vector ({\"x\":1,\"y\":2,\"z\":3}), color ({\"r\":1,\"g\":0.5,\"b\":0,\"a\":1}).",
		Example: `ue-cli set_niagara_rapid_iteration_parameter --system-path /Game/VFX/NS_Fire --emitter-name Sparks --module-name SpawnRate --input-name SpawnRate --value 50 --script-usage emitter_update
ue-cli set_niagara_rapid_iteration_parameter --system-path /Game/VFX/NS_Fire --emitter-name Sparks --module-name InitializeParticle --input-name Color --value '{"r":1,"g":0.3,"b":0,"a":1}' --script-usage particle_spawn
ue-cli set_niagara_rapid_iteration_parameter --system-path /Game/VFX/NS_Fire --emitter-name Sparks --module-name GravityForce --input-name Gravity --value '{"x":0,"y":0,"z":-980}' --script-usage particle_update`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Asset path of the Niagara system"},
			{Name: "emitter_name", Type: "string", Required: true, Help: "Name of the emitter"},
			{Name: "module_name", Type: "string", Required: true, Help: "Module name (e.g. SpawnRate, InitializeParticle, GravityForce)"},
			{Name: "input_name", Type: "string", Required: true, Help: "Input parameter name (e.g. SpawnRate, Color, Gravity)"},
			{Name: "value", Type: "json", Required: true, Help: "Value to set — format depends on type"},
			{Name: "script_usage", Type: "string", Required: true, Help: "Stack: emitter_update, particle_spawn, particle_update, etc."},
		},
	},

	// -- Diagnostics & Timeline --
	{
		Name:  "get_niagara_system_errors",
		Group: "niagara",
		Short: "Get compilation errors and warnings for a Niagara system",
		Long: "Returns compilation errors, warnings, and validation issues for a Niagara system. " +
			"Use to diagnose why a system isn't working, find outdated modules, or check for " +
			"configuration problems. Filter by emitter name or severity level.",
		Example: `ue-cli get_niagara_system_errors --system-path /Game/VFX/NS_Fire
ue-cli get_niagara_system_errors --system-path /Game/VFX/NS_Fire --emitter-name Sparks --severity error`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Asset path of the Niagara system"},
			{Name: "emitter_name", Type: "string", Help: "Filter to issues from a specific emitter"},
			{Name: "severity", Type: "string", Default: "all", Help: "Severity filter: error, warning, info, or all"},
		},
	},
	{
		Name:  "get_niagara_particle_stats",
		Group: "niagara",
		Short: "Get live particle counts and emitter execution state",
		Long: "Returns live particle counts per emitter, total spawned count, execution state " +
			"(Active/Inactive/Complete), and bounds info. Requires the system to be previewing " +
			"in the Niagara editor or spawned in the level. Use to verify emitters are spawning " +
			"particles and to diagnose empty effects.",
		Example: `ue-cli get_niagara_particle_stats --system-path /Game/VFX/NS_Fire
ue-cli get_niagara_particle_stats --system-path /Game/VFX/NS_Fire --emitter-name Sparks`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Asset path of the Niagara system"},
			{Name: "emitter_name", Type: "string", Help: "Filter to a specific emitter's stats"},
		},
	},
	{
		Name:  "set_niagara_playback_range",
		Group: "niagara",
		Short: "Set the timeline playback range in the Niagara editor",
		Long: "Controls how long the Niagara editor preview plays. If your effect dies at 0.02s, " +
			"extend the range. For looping effects set a longer range (e.g. 5-10 seconds). " +
			"Optionally override the frame rate.",
		Example: `ue-cli set_niagara_playback_range --system-path /Game/VFX/NS_Fire --range-end 5.0
ue-cli set_niagara_playback_range --system-path /Game/VFX/NS_Fire --range-start 0.5 --range-end 10.0 --frame-rate 30`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Asset path of the Niagara system"},
			{Name: "range_end", Type: "float", Required: true, Help: "End time in seconds (e.g. 5.0)"},
			{Name: "range_start", Type: "float", Default: 0.0, Help: "Start time in seconds"},
			{Name: "frame_rate", Type: "int", Help: "Optional frame rate override (default 60)"},
		},
	},
	{
		Name:  "get_niagara_playback_range",
		Group: "niagara",
		Short: "Get the current playback range and frame rate settings",
		Long: "Returns the current timeline playback range (start/end in seconds) and frame rate " +
			"configuration. Use to check if the playback range is too short — a common cause of " +
			"effects appearing to not play.",
		Example: "ue-cli get_niagara_playback_range --system-path /Game/VFX/NS_Fire",
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Asset path of the Niagara system"},
		},
	},
	{
		Name:  "get_niagara_module_versions",
		Group: "niagara",
		Short: "Check for outdated modules in an emitter",
		Long: "Identifies modules that have newer versions available — a common source of Niagara " +
			"warnings and compatibility issues. Returns current and available versions per module. " +
			"Use filter to narrow by module name substring.",
		Example: `ue-cli get_niagara_module_versions --system-path /Game/VFX/NS_Fire --emitter-name Sparks
ue-cli get_niagara_module_versions --system-path /Game/VFX/NS_Fire --emitter-name Sparks --filter "Spawn"`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Asset path of the Niagara system"},
			{Name: "emitter_name", Type: "string", Required: true, Help: "Name of the emitter"},
			{Name: "filter", Type: "string", Help: "Filter modules by name substring"},
		},
	},

	// -- Graph Introspection --
	{
		Name:  "get_niagara_graph_nodes",
		Group: "niagara",
		Short: "Introspect every node inside a Niagara graph",
		Long: "Three resolver modes: (1) system_path + module_name for scratch pad module, " +
			"(2) system_path + emitter_name + script_usage for emitter stack graph, " +
			"(3) script_path for standalone UNiagaraScript asset. Returns nodes with pin/link detail " +
			"based on verbosity. Token-efficient — use type_filter/name_filter to narrow.",
		Example: `ue-cli get_niagara_graph_nodes --system-path /Game/VFX/NS_Fire --module-name MyScratchPad
ue-cli get_niagara_graph_nodes --system-path /Game/VFX/NS_Fire --emitter-name Sparks --script-usage emitter_update --type-filter FunctionCall`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Help: "Niagara System asset path"},
			{Name: "module_name", Type: "string", Help: "Scratch pad module name (mode 1)"},
			{Name: "emitter_name", Type: "string", Help: "Emitter name (mode 2)"},
			{Name: "script_usage", Type: "string", Help: "emitter_spawn|emitter_update|particle_spawn|particle_update (mode 2)"},
			{Name: "script_path", Type: "string", Help: "Standalone UNiagaraScript asset path (mode 3)"},
			{Name: "verbosity", Type: "string", Default: "connections", Help: "summary | connections | full"},
			{Name: "type_filter", Type: "string", Help: "Substring on short class name (e.g. MapGet, Op, CustomHlsl)"},
			{Name: "name_filter", Type: "string", Help: "Substring on node title"},
		},
	},
	{
		Name:  "get_niagara_node_info",
		Group: "niagara",
		Short: "Deep inspect a single Niagara node by index, class, or GUID",
		Long: "Returns full pin layout, link detail, and node-type-specific fields (op_name for Op, " +
			"function_script for FunctionCall, hlsl_preview for CustomHlsl, input_name/type for Input).",
		Example: `ue-cli get_niagara_node_info --system-path /Game/VFX/NS_Fire --module-name MyDI --node-index 2
ue-cli get_niagara_node_info --script-path /Game/VFX/MyModule --node-class MapGet`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Help: "Niagara System asset path"},
			{Name: "module_name", Type: "string", Help: "Scratch pad module name"},
			{Name: "script_path", Type: "string", Help: "Standalone script asset path"},
			{Name: "node_index", Type: "int", Help: "Ordinal index in Graph->Nodes"},
			{Name: "node_class", Type: "string", Help: "Short ('MapGet') or full ('NiagaraNodeParameterMapGet')"},
			{Name: "node_id", Type: "string", Help: "FGuid string matching UEdGraphNode::NodeGuid"},
		},
	},
	{
		Name:  "trace_niagara_connection",
		Group: "niagara",
		Short: "BFS trace of connections upstream or downstream from a node",
		Long: "Walks the graph from a starting node with optional pin_name filter. Returns each visited " +
			"node with depth so the dependency chain is visible without dumping the whole graph.",
		Example: `ue-cli trace_niagara_connection --system-path /Game/VFX/NS_Fire --module-name MyDI --node-class MapGet --direction downstream --pin-name "Vector Array"`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Help: "Niagara System asset path"},
			{Name: "module_name", Type: "string", Help: "Scratch pad module name"},
			{Name: "script_path", Type: "string", Help: "Standalone script asset path"},
			{Name: "node_index", Type: "int", Help: "Starting node index"},
			{Name: "node_class", Type: "string", Help: "Starting node class"},
			{Name: "node_id", Type: "string", Help: "Starting node GUID"},
			{Name: "direction", Type: "string", Default: "both", Help: "upstream | downstream | both"},
			{Name: "max_depth", Type: "int", Default: 8, Help: "Max BFS depth"},
			{Name: "pin_name", Type: "string", Help: "Optional starting-pin name filter"},
		},
	},
	{
		Name:  "validate_niagara_graph",
		Group: "niagara",
		Short: "Classify orphaned, dead-end, and missing-input nodes in a Niagara graph",
		Long: "Returns three arrays: orphaned (no incoming or outgoing links), dead_ends (inputs connected " +
			"but no outputs consumed), missing_inputs (no link and no default). Skips +Add placeholder pins.",
		Example: `ue-cli validate_niagara_graph --system-path /Game/VFX/NS_Fire --module-name MyDI`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Help: "Niagara System asset path"},
			{Name: "module_name", Type: "string", Help: "Scratch pad module name"},
			{Name: "script_path", Type: "string", Help: "Standalone script asset path"},
		},
	},

	// -- Scratch Pad Apply / Save --
	{
		Name:  "apply_niagara_scratch_pad",
		Group: "niagara",
		Short: "Commit a scratch pad module's edit-copy to the original script (Apply button)",
		Long:  "Mirrors FNiagaraScratchPadScriptViewModel::ApplyChanges. Requires the Niagara System to be open in the editor.",
		Example: `ue-cli apply_niagara_scratch_pad --system-path /Game/VFX/NS_Fire --module-name MyDI`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Niagara System asset path"},
			{Name: "module_name", Type: "string", Required: true, Help: "Scratch pad module name"},
		},
	},
	{
		Name:  "apply_and_save_niagara_scratch_pad",
		Group: "niagara",
		Short: "Apply a scratch pad module's edit-copy and save the asset (Apply & Save button)",
		Long:  "Mirrors FNiagaraScratchPadScriptViewModel::ApplyChangesAndSave.",
		Example: `ue-cli apply_and_save_niagara_scratch_pad --system-path /Game/VFX/NS_Fire --module-name MyDI`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Niagara System asset path"},
			{Name: "module_name", Type: "string", Required: true, Help: "Scratch pad module name"},
		},
	},
	{
		Name:  "list_niagara_scratch_pad_modules",
		Group: "niagara",
		Short: "List all scratch pad modules on a Niagara System",
		Long:  "Returns name, path, usage, node counts, and custom HLSL node count per script.",
		Example: `ue-cli list_niagara_scratch_pad_modules --system-path /Game/VFX/NS_Fire`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Niagara System asset path"},
		},
	},
	{
		Name:  "delete_niagara_scratch_pad_module",
		Group: "niagara",
		Short: "Delete a scratch pad module from a Niagara System",
		Example: `ue-cli delete_niagara_scratch_pad_module --system-path /Game/VFX/NS_Fire --module-name MyDI`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Niagara System asset path"},
			{Name: "module_name", Type: "string", Required: true, Help: "Scratch pad module name"},
		},
	},
	{
		Name:  "duplicate_niagara_scratch_pad_module",
		Group: "niagara",
		Short: "Duplicate a scratch pad module with a new name",
		Example: `ue-cli duplicate_niagara_scratch_pad_module --system-path /Game/VFX/NS_Fire --module-name MyDI --new-name MyDICopy`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Niagara System asset path"},
			{Name: "module_name", Type: "string", Required: true, Help: "Source scratch pad module name"},
			{Name: "new_name", Type: "string", Required: true, Help: "New scratch pad module name"},
		},
	},
	{
		Name:  "rename_niagara_scratch_pad_module",
		Group: "niagara",
		Short: "Rename a scratch pad module",
		Example: `ue-cli rename_niagara_scratch_pad_module --system-path /Game/VFX/NS_Fire --module-name Old --new-name New`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Niagara System asset path"},
			{Name: "module_name", Type: "string", Required: true, Help: "Current scratch pad module name"},
			{Name: "new_name", Type: "string", Required: true, Help: "New scratch pad module name"},
		},
	},

	// -- Script Properties (details panel) --
	{
		Name:  "get_niagara_script_properties",
		Group: "niagara",
		Short: "Read the details-panel properties of a Niagara script",
		Long: "Returns Category, Description, Keywords, ModuleUsageBitmask, ProvidedDependencies, " +
			"RequiredDependencies, LibraryVisibility, bDeprecated, DeprecationMessage, bExperimental, " +
			"ExperimentalMessage, NumericOutputTypeSelectionMode, ScriptMetaData, ConversionUtility.",
		Example: `ue-cli get_niagara_script_properties --system-path /Game/VFX/NS_Fire --module-name MyDI`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Help: "Niagara System asset path (with module_name)"},
			{Name: "module_name", Type: "string", Help: "Scratch pad module name"},
			{Name: "script_path", Type: "string", Help: "Standalone script asset path"},
		},
	},
	{
		Name:  "set_niagara_script_properties",
		Group: "niagara",
		Short: "Batch-set details-panel properties on a Niagara script",
		Long: "Pass a JSON object under --properties with any subset of the editable fields. " +
			"ModuleUsageBitmask uses ENiagaraScriptUsage bitmask (e.g. particle spawn + update = (1<<3)|(1<<4)). " +
			"ProvidedDependencies takes a JSON array of strings; RequiredDependencies an array of objects.",
		Example: `ue-cli set_niagara_script_properties --system-path /Game/VFX/NS_Fire --module-name MyDI --properties '{"Category":"MCP/DI","bExperimental":true,"ProvidedDependencies":["MCP.ArrayLength"]}'`,
		Params: []ParamSpec{
			{Name: "properties", Type: "object", Required: true, Help: "JSON object of property name -> value"},
			{Name: "system_path", Type: "string", Help: "Niagara System asset path (with module_name)"},
			{Name: "module_name", Type: "string", Help: "Scratch pad module name"},
			{Name: "script_path", Type: "string", Help: "Standalone script asset path"},
		},
	},

	// -- Script Parameters (input / output) --
	{
		Name:  "list_niagara_script_parameters",
		Group: "niagara",
		Short: "List input + output parameters of a Niagara script",
		Long:  "Outputs come from UNiagaraNodeOutput::Outputs. Inputs come from script variable metadata filtered to Module.* / Input.* / User.* namespaces.",
		Example: `ue-cli list_niagara_script_parameters --system-path /Game/VFX/NS_Fire --module-name MyDI`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Help: "Niagara System asset path"},
			{Name: "module_name", Type: "string", Help: "Scratch pad module name"},
			{Name: "script_path", Type: "string", Help: "Standalone script asset path"},
		},
	},
	{
		Name:  "add_niagara_script_parameter",
		Group: "niagara",
		Short: "Add an input or output parameter to a Niagara script",
		Long: "For 'output' direction appends to UNiagaraNodeOutput::Outputs. Type must be a FNiagaraTypeRegistry " +
			"name (e.g. float, Vector, Vector2D, Vector4, LinearColor, Quat, Position, NiagaraID, int32, bool, " +
			"Matrix, NiagaraDataInterfaceArrayPosition).",
		Example: `ue-cli add_niagara_script_parameter --system-path /Game/VFX/NS_Fire --module-name MyDI --name TestFloat --type float --direction output`,
		Params: []ParamSpec{
			{Name: "name", Type: "string", Required: true, Help: "Parameter name (no namespace → becomes Module.<name>)"},
			{Name: "type", Type: "string", Required: true, Help: "Niagara type name"},
			{Name: "direction", Type: "string", Default: "output", Help: "output | input"},
			{Name: "system_path", Type: "string", Help: "Niagara System asset path"},
			{Name: "module_name", Type: "string", Help: "Scratch pad module name"},
			{Name: "script_path", Type: "string", Help: "Standalone script asset path"},
		},
	},
	{
		Name:  "remove_niagara_script_parameter",
		Group: "niagara",
		Short: "Remove a named input or output parameter — cascades to Map Get/Set pins",
		Example: `ue-cli remove_niagara_script_parameter --system-path /Game/VFX/NS_Fire --module-name MyDI --name Module.Default --direction input`,
		Params: []ParamSpec{
			{Name: "name", Type: "string", Required: true, Help: "Parameter name"},
			{Name: "direction", Type: "string", Default: "output", Help: "output | input"},
			{Name: "system_path", Type: "string", Help: "Niagara System asset path"},
			{Name: "module_name", Type: "string", Help: "Scratch pad module name"},
			{Name: "script_path", Type: "string", Help: "Standalone script asset path"},
		},
	},
	{
		Name:  "rename_niagara_script_parameter",
		Group: "niagara",
		Short: "Rename a script parameter in-place across asset and edit-copy graphs",
		Example: `ue-cli rename_niagara_script_parameter --system-path /Game/VFX/NS_Fire --module-name MyDI --old-name NewOutput --new-name FinalNoise --direction output`,
		Params: []ParamSpec{
			{Name: "old_name", Type: "string", Required: true, Help: "Current parameter name"},
			{Name: "new_name", Type: "string", Required: true, Help: "New parameter name"},
			{Name: "direction", Type: "string", Default: "output", Help: "output | input"},
			{Name: "system_path", Type: "string", Help: "Niagara System asset path"},
			{Name: "module_name", Type: "string", Help: "Scratch pad module name"},
			{Name: "script_path", Type: "string", Help: "Standalone script asset path"},
		},
	},

	// -- Graph Node CRUD --
	{
		Name:  "add_niagara_graph_node",
		Group: "niagara",
		Short: "Create a new node inside a Niagara graph",
		Long: "node_type options: Op (requires op_name), FunctionCall (requires function_script), " +
			"DataInterfaceFunction (requires di_class + function_name), ParameterMapGet, ParameterMapSet, " +
			"Reroute, Input (requires input_name + input_type). Discover op_name via get_niagara_schema_actions " +
			"and DI function_name via list_niagara_data_interface_functions — never guess.",
		Example: `ue-cli add_niagara_graph_node --system-path /Game/VFX/NS_Fire --module-name MyDI --node-type Op --op-name "Numeric::Mul" --pos-x 100 --pos-y 100
ue-cli add_niagara_graph_node --system-path /Game/VFX/NS_Fire --module-name MyDI --node-type DataInterfaceFunction --di-class NiagaraDataInterfaceArrayPosition --function-name Length`,
		Params: []ParamSpec{
			{Name: "node_type", Type: "string", Required: true, Help: "Op | FunctionCall | DataInterfaceFunction | ParameterMapGet | ParameterMapSet | Reroute | Input"},
			{Name: "pos_x", Type: "int", Default: 0, Help: "Graph X position"},
			{Name: "pos_y", Type: "int", Default: 0, Help: "Graph Y position"},
			{Name: "op_name", Type: "string", Help: "(Op) e.g. Numeric::Mul, Numeric::Add, Numeric::Length"},
			{Name: "function_script", Type: "string", Help: "(FunctionCall) UNiagaraScript asset path"},
			{Name: "input_name", Type: "string", Help: "(Input) variable name"},
			{Name: "input_type", Type: "string", Help: "(Input) Niagara type"},
			{Name: "di_class", Type: "string", Help: "(DataInterfaceFunction) DI class name"},
			{Name: "function_name", Type: "string", Help: "(DataInterfaceFunction) DI member function name"},
			{Name: "system_path", Type: "string", Help: "Niagara System asset path"},
			{Name: "module_name", Type: "string", Help: "Scratch pad module name"},
			{Name: "script_path", Type: "string", Help: "Standalone script asset path"},
		},
	},
	{
		Name:  "delete_niagara_graph_node",
		Group: "niagara",
		Short: "Delete a node from a Niagara graph",
		Long:  "Provide node_index (ordinal) OR node_id (FGuid from get_niagara_graph_nodes).",
		Example: `ue-cli delete_niagara_graph_node --system-path /Game/VFX/NS_Fire --module-name MyDI --node-index 4`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Help: "Niagara System asset path"},
			{Name: "module_name", Type: "string", Help: "Scratch pad module name"},
			{Name: "script_path", Type: "string", Help: "Standalone script asset path"},
			{Name: "node_index", Type: "int", Help: "Ordinal index"},
			{Name: "node_id", Type: "string", Help: "FGuid string"},
		},
	},

	// -- Custom HLSL Pin Management --
	{
		Name:  "add_niagara_custom_hlsl_input",
		Group: "niagara",
		Short: "Add an input pin to a Custom HLSL node",
		Example: `ue-cli add_niagara_custom_hlsl_input --system-path /Game/VFX/NS_Fire --module-name MyDI --pin-name Value --pin-type float`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Niagara System asset path"},
			{Name: "module_name", Type: "string", Required: true, Help: "Scratch pad module name"},
			{Name: "pin_name", Type: "string", Required: true, Help: "New input pin name"},
			{Name: "pin_type", Type: "string", Required: true, Help: "Niagara type (float, Vector, int32, etc.)"},
		},
	},
	{
		Name:  "add_niagara_custom_hlsl_output",
		Group: "niagara",
		Short: "Add an output pin to a Custom HLSL node",
		Example: `ue-cli add_niagara_custom_hlsl_output --system-path /Game/VFX/NS_Fire --module-name MyDI --pin-name Result --pin-type float`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Niagara System asset path"},
			{Name: "module_name", Type: "string", Required: true, Help: "Scratch pad module name"},
			{Name: "pin_name", Type: "string", Required: true, Help: "New output pin name"},
			{Name: "pin_type", Type: "string", Required: true, Help: "Niagara type"},
		},
	},
	{
		Name:  "rename_niagara_custom_hlsl_pin",
		Group: "niagara",
		Short: "Rename a Custom HLSL pin (also rewrites {PinName} references in the HLSL body)",
		Example: `ue-cli rename_niagara_custom_hlsl_pin --system-path /Game/VFX/NS_Fire --module-name MyDI --old-name Value --new-name Scalar`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Niagara System asset path"},
			{Name: "module_name", Type: "string", Required: true, Help: "Scratch pad module name"},
			{Name: "old_name", Type: "string", Required: true, Help: "Current pin name"},
			{Name: "new_name", Type: "string", Required: true, Help: "New pin name"},
		},
	},
	{
		Name:  "remove_niagara_custom_hlsl_pin",
		Group: "niagara",
		Short: "Remove a pin from a Custom HLSL node",
		Example: `ue-cli remove_niagara_custom_hlsl_pin --system-path /Game/VFX/NS_Fire --module-name MyDI --pin-name Value`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Niagara System asset path"},
			{Name: "module_name", Type: "string", Required: true, Help: "Scratch pad module name"},
			{Name: "pin_name", Type: "string", Required: true, Help: "Pin to remove"},
		},
	},

	// -- Map Get / Map Set / Generic Pin CRUD --
	{
		Name:  "add_niagara_map_get_pin",
		Group: "niagara",
		Short: "Add a typed output pin to the first ParameterMapGet node in a scratch pad graph",
		Long:  "Mirrors clicking + on a Map Get node in the editor. The pin name becomes the parameter handle (e.g. Module.MyParam, Engine.DeltaTime, User.MyParam).",
		Example: `ue-cli add_niagara_map_get_pin --system-path /Game/VFX/NS_Fire --module-name MyDI --parameter-name Module.Position --parameter-type Vector`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Niagara System asset path"},
			{Name: "module_name", Type: "string", Required: true, Help: "Scratch pad module name"},
			{Name: "parameter_name", Type: "string", Required: true, Help: "Variable name including namespace"},
			{Name: "parameter_type", Type: "string", Default: "float", Help: "Niagara type"},
		},
	},
	{
		Name:  "add_niagara_map_set_pin",
		Group: "niagara",
		Short: "Add a typed input pin to the first ParameterMapSet node in a scratch pad graph",
		Example: `ue-cli add_niagara_map_set_pin --system-path /Game/VFX/NS_Fire --module-name MyModule --parameter-name Particles.Velocity --parameter-type Vector`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Niagara System asset path"},
			{Name: "module_name", Type: "string", Required: true, Help: "Scratch pad module name"},
			{Name: "parameter_name", Type: "string", Required: true, Help: "Variable name including namespace"},
			{Name: "parameter_type", Type: "string", Default: "float", Help: "Niagara type"},
		},
	},
	{
		Name:  "add_niagara_node_pin",
		Group: "niagara",
		Short: "Add a pin to any UNiagaraNodeWithDynamicPins-derived node",
		Example: `ue-cli add_niagara_node_pin --system-path /Game/VFX/NS_Fire --module-name MyDI --node-index 3 --pin-name Extra --pin-type float --direction output`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Niagara System asset path"},
			{Name: "module_name", Type: "string", Required: true, Help: "Scratch pad module name"},
			{Name: "pin_name", Type: "string", Required: true, Help: "New pin name"},
			{Name: "pin_type", Type: "string", Required: true, Help: "Niagara type"},
			{Name: "direction", Type: "string", Default: "input", Help: "input | output"},
			{Name: "node_index", Type: "int", Help: "Target node index"},
			{Name: "node_class", Type: "string", Help: "Target node class"},
			{Name: "node_id", Type: "string", Help: "Target node GUID"},
		},
	},
	{
		Name:  "rename_niagara_node_pin",
		Group: "niagara",
		Short: "Rename a pin on a dynamic-pin node",
		Example: `ue-cli rename_niagara_node_pin --system-path /Game/VFX/NS_Fire --module-name MyDI --node-index 3 --old-name Foo --new-name Bar`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Niagara System asset path"},
			{Name: "module_name", Type: "string", Required: true, Help: "Scratch pad module name"},
			{Name: "old_name", Type: "string", Required: true, Help: "Current pin name"},
			{Name: "new_name", Type: "string", Required: true, Help: "New pin name"},
			{Name: "node_index", Type: "int", Help: "Target node index"},
			{Name: "node_class", Type: "string", Help: "Target node class"},
			{Name: "node_id", Type: "string", Help: "Target node GUID"},
		},
	},
	{
		Name:  "remove_niagara_node_pin",
		Group: "niagara",
		Short: "Remove a dynamic pin from any node in a scratch pad graph",
		Example: `ue-cli remove_niagara_node_pin --system-path /Game/VFX/NS_Fire --module-name MyDI --node-index 2 --pin-name Module.Default`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Niagara System asset path"},
			{Name: "module_name", Type: "string", Required: true, Help: "Scratch pad module name"},
			{Name: "pin_name", Type: "string", Required: true, Help: "Pin name to remove"},
			{Name: "node_index", Type: "int", Help: "Target node index"},
			{Name: "node_class", Type: "string", Help: "Target node class"},
			{Name: "node_id", Type: "string", Help: "Target node GUID"},
		},
	},

	// -- Pin Connections --
	{
		Name:  "connect_niagara_pins",
		Group: "niagara",
		Short: "Wire one node's output pin to another node's input pin inside a scratch pad graph",
		Long:  "Validated via UEdGraphSchema_Niagara::TryCreateConnection. Identify each side via (class|index|id) + pin name.",
		Example: `ue-cli connect_niagara_pins --system-path /Game/VFX/NS_Fire --module-name MyDI --from-node-index 2 --from-pin Module.Position --to-node-index 3 --to-pin Array\ interface`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Niagara System asset path"},
			{Name: "module_name", Type: "string", Required: true, Help: "Scratch pad module name"},
			{Name: "from_pin", Type: "string", Required: true, Help: "Source pin name"},
			{Name: "to_pin", Type: "string", Required: true, Help: "Destination pin name"},
			{Name: "from_node_index", Type: "int", Help: "Source node index"},
			{Name: "from_node_class", Type: "string", Help: "Source node class"},
			{Name: "from_node_id", Type: "string", Help: "Source node GUID"},
			{Name: "to_node_index", Type: "int", Help: "Destination node index"},
			{Name: "to_node_class", Type: "string", Help: "Destination node class"},
			{Name: "to_node_id", Type: "string", Help: "Destination node GUID"},
		},
	},
	{
		Name:  "disconnect_niagara_pins",
		Group: "niagara",
		Short: "Break a pin connection in a scratch pad graph",
		Example: `ue-cli disconnect_niagara_pins --system-path /Game/VFX/NS_Fire --module-name MyDI --node-index 3 --pin-name "Array interface"`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Niagara System asset path"},
			{Name: "module_name", Type: "string", Required: true, Help: "Scratch pad module name"},
			{Name: "pin_name", Type: "string", Required: true, Help: "Pin to break"},
			{Name: "node_index", Type: "int", Help: "Target node index"},
			{Name: "node_class", Type: "string", Help: "Target node class"},
			{Name: "node_id", Type: "string", Help: "Target node GUID"},
		},
	},

	// -- Node Discovery / Schema --
	{
		Name:  "list_niagara_node_types",
		Group: "niagara",
		Short: "Enumerate Niagara node classes available for spawning",
		Example: `ue-cli list_niagara_node_types --filter Parameter`,
		Params: []ParamSpec{
			{Name: "filter", Type: "string", Help: "Case-insensitive substring on class name"},
		},
	},
	{
		Name:  "get_niagara_node_type_info",
		Group: "niagara",
		Short: "Get pin schema for a node class or script asset",
		Example: `ue-cli get_niagara_node_type_info --class-name NiagaraNodeParameterMapGet`,
		Params: []ParamSpec{
			{Name: "class_name", Type: "string", Help: "UClass name (short or full)"},
			{Name: "script_path", Type: "string", Help: "UNiagaraScript asset path"},
		},
	},
	{
		Name:  "search_niagara_functions",
		Group: "niagara",
		Short: "Find Niagara script assets by usage + name filter",
		Example: `ue-cli search_niagara_functions --filter Length --usage function`,
		Params: []ParamSpec{
			{Name: "filter", Type: "string", Help: "Name/path substring"},
			{Name: "usage", Type: "string", Default: "function", Help: "module | dynamic_input | function"},
			{Name: "include_engine", Type: "bool", Default: true, Help: "Include /Niagara/* engine scripts"},
			{Name: "max_results", Type: "int", Default: 100, Help: "Max returned"},
		},
	},
	{
		Name:  "get_niagara_schema_actions",
		Group: "niagara",
		Short: "Return the full graph context-menu actions for a scratch pad module",
		Long:  "Wraps UEdGraphSchema_Niagara::GetGraphActions — same data the editor's right-click menu uses. Each entry has display_name, category, tooltip, op_name (for Op), function_script (for FunctionCall), input_name/type (for Input), and full pin schema.",
		Example: `ue-cli get_niagara_schema_actions --system-path /Game/VFX/NS_Fire --module-name MyDI --filter multiply`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Niagara System asset path"},
			{Name: "module_name", Type: "string", Required: true, Help: "Scratch pad module name"},
			{Name: "filter", Type: "string", Help: "Case-insensitive substring filter"},
			{Name: "max_results", Type: "int", Default: 300, Help: "Max returned entries"},
		},
	},
	{
		Name:  "describe_niagara_type",
		Group: "niagara",
		Short: "Generic type query — handles registered Niagara types AND raw UEnums/UScriptStructs",
		Long:  "First tries FNiagaraTypeRegistry; falls back to UEnum/UScriptStruct reflection for script-property enums like ENiagaraScriptLibraryVisibility or any custom project enum.",
		Example: `ue-cli describe_niagara_type --type Vector
ue-cli describe_niagara_type --type ENiagaraScriptLibraryVisibility`,
		Params: []ParamSpec{
			{Name: "type", Type: "string", Required: true, Help: "Type name (pin-registered or UEnum/UScriptStruct)"},
		},
	},
	{
		Name:  "get_niagara_data_interface_schema",
		Group: "niagara",
		Short: "Walk a UNiagaraDataInterface class default object and return its editable property schema",
		Example: `ue-cli get_niagara_data_interface_schema --di-class NiagaraDataInterfaceArrayPosition`,
		Params: []ParamSpec{
			{Name: "di_class", Type: "string", Required: true, Help: "DI class short or full name"},
		},
	},
	{
		Name:  "list_niagara_available_parameters",
		Group: "niagara",
		Short: "List available Niagara parameters (Engine.*, Particles.*, Emitter.*, User.*)",
		Example: `ue-cli list_niagara_available_parameters --namespace engine --filter Time`,
		Params: []ParamSpec{
			{Name: "filter", Type: "string", Help: "Case-insensitive substring on parameter name"},
			{Name: "namespace", Type: "string", Default: "all", Help: "all | engine | particles | emitter | user | system"},
			{Name: "max_results", Type: "int", Default: 500, Help: "Max returned"},
			{Name: "system_path", Type: "string", Help: "Optional — include system's user parameters"},
			{Name: "module_name", Type: "string", Help: "Optional — scope to a scratch pad module's namespace"},
		},
	},

	// -- Stack Input Binding (the dropdown loop) --
	{
		Name:  "get_niagara_module_input_binding",
		Group: "niagara",
		Short: "Resolve the actual binding of each module input — Default / Local / Linked / Dynamic / Data",
		Long: "Answers 'what is Spawn Count actually driven by?'. Recursively walks dynamic-input children up to max_depth so a single call returns the full binding tree with nested parameter links and literal values.",
		Example: `ue-cli get_niagara_module_input_binding --system-path /Game/VFX/NS_Fire --emitter-name Sparks --module-name SpawnPerFrame --script-usage emitter_update`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Niagara System asset path"},
			{Name: "emitter_name", Type: "string", Required: true, Help: "Emitter name"},
			{Name: "module_name", Type: "string", Required: true, Help: "Module display name"},
			{Name: "script_usage", Type: "string", Required: true, Help: "emitter_spawn|emitter_update|particle_spawn|particle_update|system_spawn|system_update"},
			{Name: "input_filter", Type: "string", Help: "Case-insensitive substring on input name"},
			{Name: "max_depth", Type: "int", Default: 3, Help: "Recursive descent cap for dynamic-input children"},
		},
	},
	{
		Name:  "clear_niagara_module_input",
		Group: "niagara",
		Short: "Reset a module input to its default (including nested paths like 'Spawn Count.Position Array')",
		Long: "Finds the override pin, breaks its connections, removes orphan nodes exclusively feeding it, then removes the override pin. Mirrors 'Reset to Default' in the stack UI. Supports nested path syntax to reach inputs inside dynamic inputs.",
		Example: `ue-cli clear_niagara_module_input --system-path /Game/VFX/NS_Fire --emitter-name Sparks --module-name SpawnPerFrame --script-usage emitter_update --input-name "Spawn Count.Position Array"`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Niagara System asset path"},
			{Name: "emitter_name", Type: "string", Required: true, Help: "Emitter name"},
			{Name: "module_name", Type: "string", Required: true, Help: "Module display name"},
			{Name: "script_usage", Type: "string", Required: true, Help: "Stack the module is in"},
			{Name: "input_name", Type: "string", Required: true, Help: "Input name (supports 'Parent.Child' nesting)"},
		},
	},
	{
		Name:  "list_niagara_input_source_menu",
		Group: "niagara",
		Short: "Reproduce the stack-UI source dropdown for a specific input",
		Long: "Returns the exact options the editor offers: dynamic_inputs (engine UNiagaraScript assets + scratch-pad DIs with source='scratch_pad' tag) + link_parameters grouped by namespace. Authoritative — the AI should call this before set_niagara_dynamic_input / link_niagara_parameter to never guess.",
		Example: `ue-cli list_niagara_input_source_menu --system-path /Game/VFX/NS_Fire --emitter-name Sparks --module-name SpawnPerFrame --script-usage emitter_update --input-name "Spawn Count" --name-filter Length`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Niagara System asset path"},
			{Name: "emitter_name", Type: "string", Required: true, Help: "Emitter name"},
			{Name: "module_name", Type: "string", Required: true, Help: "Module display name"},
			{Name: "script_usage", Type: "string", Required: true, Help: "Stack the module is in"},
			{Name: "input_name", Type: "string", Required: true, Help: "Target input name"},
			{Name: "name_filter", Type: "string", Help: "Optional substring to narrow dynamic inputs / parameters"},
		},
	},

	// -- Reverse Lookup / Built-in Resolution / DI Discovery --
	{
		Name:  "find_niagara_scratch_pad_usage",
		Group: "niagara",
		Short: "Find where a scratch pad script is referenced across all emitter/system graphs",
		Long:  "Scans every emitter × {spawn, update, particle_spawn, particle_update} + system_spawn + system_update graphs for UNiagaraNodeFunctionCall nodes whose FunctionScript matches. Returns usage sites as {emitter, script_usage, function_name, node_id, is_dynamic_input}.",
		Example: `ue-cli find_niagara_scratch_pad_usage --system-path /Game/VFX/NS_Fire --module-name GetDataInterfaceLength`,
		Params: []ParamSpec{
			{Name: "system_path", Type: "string", Required: true, Help: "Niagara System asset path"},
			{Name: "module_name", Type: "string", Required: true, Help: "Scratch pad module name to hunt for"},
		},
	},
	{
		Name:  "resolve_niagara_built_in_dynamic_input",
		Group: "niagara",
		Short: "Discover built-in dynamic-input script paths via AssetRegistry (replaces hardcoded guesses)",
		Long:  "Replaces guessed paths like /Niagara/Modules/DynamicInputs/UniformRangedFloat (which vary across UE versions) with a live AssetRegistry scan. Returns UNiagaraScript assets with Usage=DynamicInput matching the filter.",
		Example: `ue-cli resolve_niagara_built_in_dynamic_input --name-filter Random --max-results 10`,
		Params: []ParamSpec{
			{Name: "name_filter", Type: "string", Help: "Case-insensitive substring"},
			{Name: "exact_name", Type: "string", Help: "Return only the asset whose AssetName equals this"},
			{Name: "max_results", Type: "int", Default: 20, Help: "Max returned"},
		},
	},
	{
		Name:  "list_niagara_data_interface_functions",
		Group: "niagara",
		Short: "Enumerate member functions on a Niagara data interface class",
		Long: "Returns the functions the editor's right-click 'Functions' submenu shows on a DI pin — built-in DI members like Array.Length, Array.Get, Array.Add. These aren't UNiagaraScript assets so search_niagara_functions won't find them. Pass the result as function_name to add_niagara_graph_node(node_type='DataInterfaceFunction').",
		Example: `ue-cli list_niagara_data_interface_functions --di-class NiagaraDataInterfaceArrayPosition --filter Length`,
		Params: []ParamSpec{
			{Name: "di_class", Type: "string", Required: true, Help: "Short ('NiagaraDataInterfaceArrayPosition') or full path"},
			{Name: "filter", Type: "string", Help: "Case-insensitive substring on function name"},
			{Name: "include_pins", Type: "bool", Default: true, Help: "Include input/output pin schema"},
		},
	},
}
