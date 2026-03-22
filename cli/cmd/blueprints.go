package cmd

func init() {
	ensureGroup("blueprints", "Blueprints")
	registerCommands(blueprintCommands)
}

var blueprintCommands = []CommandSpec{
	// -- Structure --
	{
		Name:  "search_parent_classes",
		Group: "blueprints",
		Short: "Search for classes usable as Blueprint parents",
		Params: []ParamSpec{
			{Name: "filter", Type: "string", Required: true, Help: "Search filter"},
			{Name: "max_results", Type: "int", Default: 20, Help: "Maximum results"},
			{Name: "include_blueprint_classes", Type: "bool", Default: true, Help: "Include BP classes"},
		},
	},
	{
		Name:  "create_blueprint",
		Group: "blueprints",
		Short: "Create a Blueprint from any parent class",
		Params: []ParamSpec{
			{Name: "name", Type: "string", Required: true, Help: "Blueprint name"},
			{Name: "path", Type: "string", Default: "/Game/Blueprints", Help: "Content path"},
			{Name: "parent_class", Type: "string", Default: "Actor", Help: "Parent class"},
		},
	},
	{
		Name:  "add_component_to_blueprint",
		Group: "blueprints",
		Short: "Add a component to Blueprint defaults",
		Params: []ParamSpec{
			{Name: "blueprint_path", Type: "string", Required: true, Help: "Blueprint asset path"},
			{Name: "component_class", Type: "string", Required: true, Help: "Component class name"},
			{Name: "component_name", Type: "string", Help: "Component name"},
		},
	},
	{
		Name:  "get_blueprint_class_defaults",
		Group: "blueprints",
		Short: "Get all CDO property values",
		Params: []ParamSpec{
			{Name: "blueprint_path", Type: "string", Required: true, Help: "Blueprint asset path"},
			{Name: "filter", Type: "string", Help: "Filter property names"},
			{Name: "include_inherited", Type: "bool", Default: true, Help: "Include inherited properties"},
		},
	},
	{
		Name:  "set_blueprint_class_defaults",
		Group: "blueprints",
		Short: "Set CDO property values",
		Params: []ParamSpec{
			{Name: "blueprint_path", Type: "string", Required: true, Help: "Blueprint asset path"},
			{Name: "property_name", Type: "string", Help: "Property name (single)"},
			{Name: "property_value", Type: "string", Help: "Property value (single)"},
			{Name: "properties", Type: "json", Help: "JSON object of property name-value pairs (batch)"},
		},
	},
	{
		Name:  "compile_blueprint",
		Group: "blueprints",
		Short: "Compile a Blueprint",
		Params: []ParamSpec{
			{Name: "blueprint_name", Type: "string", Required: true, Help: "Blueprint name"},
		},
	},
	{
		Name:    "read_blueprint_content",
		Group:   "blueprints",
		Short:   "Read complete BP: graph, functions, variables, components",
		LargeOp: true,
		Params: []ParamSpec{
			{Name: "blueprint_path", Type: "string", Required: true, Help: "Blueprint asset path"},
			{Name: "include_event_graph", Type: "bool", Default: true, Help: "Include event graph"},
			{Name: "include_functions", Type: "bool", Default: true, Help: "Include functions"},
			{Name: "include_variables", Type: "bool", Default: true, Help: "Include variables"},
			{Name: "include_components", Type: "bool", Default: true, Help: "Include components"},
			{Name: "include_interfaces", Type: "bool", Default: true, Help: "Include interfaces"},
		},
	},
	{
		Name:    "analyze_blueprint_graph",
		Group:   "blueprints",
		Short:   "Analyze a graph (nodes, connections, execution flow)",
		LargeOp: true,
		Params: []ParamSpec{
			{Name: "blueprint_path", Type: "string", Required: true, Help: "Blueprint asset path"},
			{Name: "graph_name", Type: "string", Default: "EventGraph", Help: "Graph name"},
			{Name: "include_node_details", Type: "bool", Default: true, Help: "Include node details"},
			{Name: "include_pin_connections", Type: "bool", Default: true, Help: "Include pin connections"},
			{Name: "trace_execution_flow", Type: "bool", Default: true, Help: "Trace execution flow"},
		},
	},

	// -- Variables --
	{
		Name:  "create_blueprint_variable",
		Group: "blueprints",
		Short: "Create a variable in a Blueprint",
		Params: []ParamSpec{
			{Name: "blueprint_name", Type: "string", Required: true, Help: "Blueprint name"},
			{Name: "variable_name", Type: "string", Required: true, Help: "Variable name"},
			{Name: "variable_type", Type: "string", Required: true, Help: "Variable type"},
			{Name: "category", Type: "string", Default: "Default", Help: "Category"},
			{Name: "is_public", Type: "bool", Default: false, Help: "Public visibility"},
			{Name: "tooltip", Type: "string", Help: "Tooltip text"},
			{Name: "default_value", Type: "string", Help: "Default value"},
		},
	},
	{
		Name:  "get_blueprint_variable_details",
		Group: "blueprints",
		Short: "Inspect variable(s) in a Blueprint",
		Params: []ParamSpec{
			{Name: "blueprint_path", Type: "string", Required: true, Help: "Blueprint asset path"},
			{Name: "variable_name", Type: "string", Help: "Variable name (empty = all)"},
		},
	},
	{
		Name:  "set_blueprint_variable_properties",
		Group: "blueprints",
		Short: "Modify variable properties",
		Params: []ParamSpec{
			{Name: "blueprint_name", Type: "string", Required: true, Help: "Blueprint name"},
			{Name: "variable_name", Type: "string", Required: true, Help: "Variable name"},
			{Name: "category", Type: "string", Help: "Category"},
			{Name: "default_value", Type: "string", Help: "Default value"},
			{Name: "is_public", Type: "bool", Help: "Public visibility"},
			{Name: "tooltip", Type: "string", Help: "Tooltip"},
			{Name: "var_name", Type: "string", Help: "Rename variable"},
			{Name: "var_type", Type: "string", Help: "Change type"},
			{Name: "is_editable_in_instance", Type: "bool", Help: "Instance editable"},
			{Name: "expose_on_spawn", Type: "bool", Help: "Expose on spawn"},
			{Name: "replication_enabled", Type: "bool", Help: "Enable replication"},
			{Name: "replication_condition", Type: "int", Help: "Replication condition"},
		},
	},

	// -- Functions --
	{
		Name:  "create_blueprint_function",
		Group: "blueprints",
		Short: "Create a new function in a Blueprint",
		Params: []ParamSpec{
			{Name: "blueprint_name", Type: "string", Required: true, Help: "Blueprint name"},
			{Name: "function_name", Type: "string", Required: true, Help: "Function name"},
			{Name: "return_type", Type: "string", Default: "void", Help: "Return type"},
		},
	},
	{
		Name:  "get_blueprint_function_details",
		Group: "blueprints",
		Short: "Inspect function(s) with graph",
		Params: []ParamSpec{
			{Name: "blueprint_path", Type: "string", Required: true, Help: "Blueprint asset path"},
			{Name: "function_name", Type: "string", Help: "Function name (empty = all)"},
			{Name: "include_graph", Type: "bool", Default: true, Help: "Include graph details"},
		},
	},
	{
		Name:  "add_function_input",
		Group: "blueprints",
		Short: "Add input parameter to a function",
		Params: []ParamSpec{
			{Name: "blueprint_name", Type: "string", Required: true, Help: "Blueprint name"},
			{Name: "function_name", Type: "string", Required: true, Help: "Function name"},
			{Name: "param_name", Type: "string", Required: true, Help: "Parameter name"},
			{Name: "param_type", Type: "string", Required: true, Help: "Parameter type"},
			{Name: "is_array", Type: "bool", Default: false, Help: "Is array type"},
		},
	},
	{
		Name:  "add_function_output",
		Group: "blueprints",
		Short: "Add output parameter to a function",
		Params: []ParamSpec{
			{Name: "blueprint_name", Type: "string", Required: true, Help: "Blueprint name"},
			{Name: "function_name", Type: "string", Required: true, Help: "Function name"},
			{Name: "param_name", Type: "string", Required: true, Help: "Parameter name"},
			{Name: "param_type", Type: "string", Required: true, Help: "Parameter type"},
			{Name: "is_array", Type: "bool", Default: false, Help: "Is array type"},
		},
	},
	{
		Name:  "delete_blueprint_function",
		Group: "blueprints",
		Short: "Delete a function from a Blueprint",
		Params: []ParamSpec{
			{Name: "blueprint_name", Type: "string", Required: true, Help: "Blueprint name"},
			{Name: "function_name", Type: "string", Required: true, Help: "Function name"},
		},
	},
	{
		Name:  "rename_blueprint_function",
		Group: "blueprints",
		Short: "Rename a function",
		Params: []ParamSpec{
			{Name: "blueprint_name", Type: "string", Required: true, Help: "Blueprint name"},
			{Name: "old_function_name", Type: "string", Required: true, Help: "Current function name"},
			{Name: "new_function_name", Type: "string", Required: true, Help: "New function name"},
		},
	},

	// -- Graph Nodes --
	{
		Name:  "add_blueprint_node",
		Group: "blueprints",
		Short: "Add a node to a Blueprint graph",
		Params: []ParamSpec{
			{Name: "blueprint_name", Type: "string", Required: true, Help: "Blueprint name"},
			{Name: "node_type", Type: "string", Required: true, Help: "Node type (Branch, CallFunction, Print, etc.)"},
			{Name: "pos_x", Type: "float", Default: 0.0, Help: "X position"},
			{Name: "pos_y", Type: "float", Default: 0.0, Help: "Y position"},
			{Name: "event_type", Type: "string", Help: "Event type"},
			{Name: "function_name", Type: "string", Help: "Function name context"},
			{Name: "message", Type: "string", Help: "Message (for Print nodes)"},
			{Name: "variable_name", Type: "string", Help: "Variable name"},
			{Name: "target_function", Type: "string", Help: "Target function"},
			{Name: "target_blueprint", Type: "string", Help: "Target blueprint"},
		},
	},
	{
		Name:  "add_event_node",
		Group: "blueprints",
		Short: "Add an event node (BeginPlay, Tick, etc.)",
		Params: []ParamSpec{
			{Name: "blueprint_name", Type: "string", Required: true, Help: "Blueprint name"},
			{Name: "event_name", Type: "string", Required: true, Help: "Event name"},
			{Name: "pos_x", Type: "float", Default: 0.0, Help: "X position"},
			{Name: "pos_y", Type: "float", Default: 0.0, Help: "Y position"},
		},
	},
	{
		Name:  "connect_blueprint_nodes",
		Group: "blueprints",
		Short: "Wire two nodes together",
		Params: []ParamSpec{
			{Name: "blueprint_name", Type: "string", Required: true, Help: "Blueprint name"},
			{Name: "source_node_id", Type: "string", Required: true, Help: "Source node GUID"},
			{Name: "source_pin_name", Type: "string", Required: true, Help: "Source pin name"},
			{Name: "target_node_id", Type: "string", Required: true, Help: "Target node GUID"},
			{Name: "target_pin_name", Type: "string", Required: true, Help: "Target pin name"},
			{Name: "function_name", Type: "string", Help: "Function graph context"},
		},
	},
	{
		Name:  "delete_blueprint_node",
		Group: "blueprints",
		Short: "Delete a node by GUID",
		Params: []ParamSpec{
			{Name: "blueprint_name", Type: "string", Required: true, Help: "Blueprint name"},
			{Name: "node_id", Type: "string", Required: true, Help: "Node GUID"},
			{Name: "function_name", Type: "string", Help: "Function graph context"},
		},
	},
	{
		Name:  "set_blueprint_node_property",
		Group: "blueprints",
		Short: "Set node property or perform semantic editing",
		Params: []ParamSpec{
			{Name: "blueprint_name", Type: "string", Required: true, Help: "Blueprint name"},
			{Name: "node_id", Type: "string", Required: true, Help: "Node GUID"},
			{Name: "action", Type: "string", Help: "Semantic action (add_pin, set_enum_type, etc.)"},
			{Name: "property_name", Type: "string", Help: "Property name"},
			{Name: "property_value", Type: "string", Help: "Property value"},
			{Name: "pin_name", Type: "string", Help: "Pin name"},
			{Name: "pin_type", Type: "string", Help: "Pin type"},
			{Name: "enum_type", Type: "string", Help: "Enum type"},
			{Name: "new_type", Type: "string", Help: "New type"},
			{Name: "target_type", Type: "string", Help: "Target type"},
			{Name: "target_function", Type: "string", Help: "Target function"},
			{Name: "target_class", Type: "string", Help: "Target class"},
			{Name: "event_type", Type: "string", Help: "Event type"},
			{Name: "function_name", Type: "string", Help: "Function context"},
		},
	},
}
