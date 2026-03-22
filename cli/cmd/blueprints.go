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
		Long:  "Searches the engine and project class hierarchy for classes that can be used as Blueprint parents. Use this before create_blueprint to find the correct parent class name. Returns class names, module, and whether they are C++ or Blueprint-based.",
		Example: `  ue-cli search_parent_classes --filter "Actor" --max-results 10
  ue-cli search_parent_classes --filter "MassProcessor" --include-blueprint-classes=false`,
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
		Long:  "Creates a new Blueprint asset at the specified content path with the given parent class. The parent class can be a C++ class short name, a prefixed name, or a full Blueprint asset path. Use search_parent_classes first if you are unsure of the exact parent class name.",
		Example: `  ue-cli create_blueprint --name "BP_ConveyorSplitter" --path "/Game/Blueprints/Logistics" --parent-class "InteractableActor"
  ue-cli create_blueprint --name "BP_MinerT2" --parent-class "MinerActor"`,
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
		Long:  "Adds a component of the specified class to a Blueprint's default component hierarchy. Use this to add mesh components, port components, collision, or any UActorComponent subclass. The component appears in the Blueprint's Components panel.",
		Example: `  ue-cli add_component_to_blueprint --blueprint-path "/Game/Blueprints/BP_Smelter" --component-class "StaticMeshComponent" --component-name "OutputMesh"
  ue-cli add_component_to_blueprint --blueprint-path "/Game/Blueprints/BP_Miner" --component-class "PortComponent"`,
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
		Long:  "Reads all Class Default Object (CDO) property values from a Blueprint, including both C++ and Blueprint-defined properties. Use the filter parameter to narrow results when dealing with Blueprints that have many properties. Returns property names, types, and current values.",
		Example: `  ue-cli get_blueprint_class_defaults --blueprint-path "/Game/Blueprints/BP_Smelter"
  ue-cli get_blueprint_class_defaults --blueprint-path "/Game/Blueprints/BP_Miner" --filter "Power" --include-inherited=false`,
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
		Long:  "Sets one or more Class Default Object (CDO) property values on a Blueprint. Supports single property mode (property-name + property-value) or batch mode (properties JSON object). Use get_blueprint_class_defaults first to inspect current values and verify property names.",
		Example: `  ue-cli set_blueprint_class_defaults --blueprint-path "/Game/Blueprints/BP_Miner" --property-name "MiningRate" --property-value "2.0"
  ue-cli set_blueprint_class_defaults --blueprint-path "/Game/Blueprints/BP_Smelter" --properties '{"PowerDraw": "50.0", "ProcessingTime": "4.0"}'`,
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
		Long:  "Compiles a Blueprint and reports any errors or warnings. Always compile after making structural changes such as adding nodes, variables, functions, or modifying the graph. Returns compilation status and any diagnostic messages.",
		Example: `  ue-cli compile_blueprint --blueprint-name "BP_ConveyorSplitter"`,
		Params: []ParamSpec{
			{Name: "blueprint_name", Type: "string", Required: true, Help: "Blueprint name"},
		},
	},
	{
		Name:    "read_blueprint_content",
		Group:   "blueprints",
		Short:   "Read complete BP: graph, functions, variables, components",
		Long:    "Reads the complete structure of a Blueprint including its event graph, functions, variables, components, and interfaces. This is a large operation that returns comprehensive data. Use the include flags to limit output to only the sections you need.",
		Example: `  ue-cli read_blueprint_content --blueprint-path "/Game/Blueprints/BP_Manufacturer"
  ue-cli read_blueprint_content --blueprint-path "/Game/Blueprints/BP_Miner" --include-functions=false --include-variables=false`,
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
		Long:    "Performs a detailed analysis of a Blueprint graph, including node inventory, pin connections, and execution flow tracing. Use this to understand how a Blueprint works before modifying it, or to debug execution paths. Defaults to the EventGraph but can target any named graph.",
		Example: `  ue-cli analyze_blueprint_graph --blueprint-path "/Game/Blueprints/BP_Smelter"
  ue-cli analyze_blueprint_graph --blueprint-path "/Game/Blueprints/BP_Miner" --graph-name "ProcessOre" --trace-execution-flow=true`,
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
		Long:  "Creates a new variable in a Blueprint with the specified type, category, and optional default value. Supports all UE types including primitives, structs, objects, and enums. The variable appears in the Blueprint's My Blueprint panel under the specified category.",
		Example: `  ue-cli create_blueprint_variable --blueprint-name "BP_Smelter" --variable-name "ProcessingSpeed" --variable-type "Float" --category "Production" --is-public=true
  ue-cli create_blueprint_variable --blueprint-name "BP_StorageContainer" --variable-name "MaxSlots" --variable-type "Integer" --default-value "20"`,
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
		Long:  "Returns detailed information about one or all variables in a Blueprint, including type, default value, category, replication settings, and tooltip. Omit variable-name to list all variables.",
		Example: `  ue-cli get_blueprint_variable_details --blueprint-path "/Game/Blueprints/BP_Smelter" --variable-name "ProcessingSpeed"
  ue-cli get_blueprint_variable_details --blueprint-path "/Game/Blueprints/BP_Manufacturer"`,
		Params: []ParamSpec{
			{Name: "blueprint_path", Type: "string", Required: true, Help: "Blueprint asset path"},
			{Name: "variable_name", Type: "string", Help: "Variable name (empty = all)"},
		},
	},
	{
		Name:  "set_blueprint_variable_properties",
		Group: "blueprints",
		Short: "Modify variable properties",
		Long:  "Modifies properties of an existing Blueprint variable such as its name, type, default value, category, replication, and visibility. Use this to rename variables, change types, enable replication, or toggle instance editability. Only specified fields are changed; omitted fields remain unchanged.",
		Example: `  ue-cli set_blueprint_variable_properties --blueprint-name "BP_Smelter" --variable-name "Speed" --var-name "ProcessingSpeed" --category "Production"
  ue-cli set_blueprint_variable_properties --blueprint-name "BP_PowerPole" --variable-name "NetworkID" --replication-enabled=true --replication-condition 1`,
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
		Long:  "Creates a new function graph in a Blueprint with the specified name and optional return type. The function starts with an entry node and can be extended with add_function_input, add_function_output, and add_blueprint_node. Defaults to void return type.",
		Example: `  ue-cli create_blueprint_function --blueprint-name "BP_Manufacturer" --function-name "CalculateEfficiency" --return-type "Float"
  ue-cli create_blueprint_function --blueprint-name "BP_Smelter" --function-name "ResetProductionCycle"`,
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
		Long:  "Returns detailed information about one or all functions in a Blueprint, including parameters, return type, and optionally the full graph with nodes and connections. Omit function-name to list all functions. Useful for understanding a Blueprint's API before modifying it.",
		Example: `  ue-cli get_blueprint_function_details --blueprint-path "/Game/Blueprints/BP_Manufacturer" --function-name "CalculateEfficiency"
  ue-cli get_blueprint_function_details --blueprint-path "/Game/Blueprints/BP_Smelter" --include-graph=false`,
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
		Long:  "Adds an input parameter to an existing Blueprint function. Supports all UE types including primitives, structs, objects, and arrays. The parameter appears as an output pin on the function's entry node.",
		Example: `  ue-cli add_function_input --blueprint-name "BP_Manufacturer" --function-name "SetRecipe" --param-name "RecipeData" --param-type "FRecipeData"
  ue-cli add_function_input --blueprint-name "BP_Storage" --function-name "AddItems" --param-name "Items" --param-type "FStorageSlot" --is-array=true`,
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
		Long:  "Adds an output parameter to an existing Blueprint function. The parameter appears as an input pin on the function's return node. Multiple outputs create a struct-like return with named fields.",
		Example: `  ue-cli add_function_output --blueprint-name "BP_Manufacturer" --function-name "GetProductionStats" --param-name "ItemsPerMinute" --param-type "Float"
  ue-cli add_function_output --blueprint-name "BP_Storage" --function-name "GetContents" --param-name "Slots" --param-type "FStorageSlot" --is-array=true`,
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
		Long:  "Deletes a function and its entire graph from a Blueprint. This is destructive and cannot be undone. Any calls to this function from other graphs will become broken nodes. Check references before deleting.",
		Example: `  ue-cli delete_blueprint_function --blueprint-name "BP_Smelter" --function-name "OldCalculation"`,
		Params: []ParamSpec{
			{Name: "blueprint_name", Type: "string", Required: true, Help: "Blueprint name"},
			{Name: "function_name", Type: "string", Required: true, Help: "Function name"},
		},
	},
	{
		Name:  "rename_blueprint_function",
		Group: "blueprints",
		Short: "Rename a function",
		Long:  "Renames a function within a Blueprint. This updates the function graph name but does not automatically update call sites in other graphs or Blueprints. Compile the Blueprint afterward to verify all references resolve correctly.",
		Example: `  ue-cli rename_blueprint_function --blueprint-name "BP_Manufacturer" --old-function-name "CalcSpeed" --new-function-name "CalculateProcessingSpeed"`,
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
		Long:  "Adds a node to a Blueprint's event graph or function graph. Supports 23+ node types including Branch, CallFunction, Print, ForEachLoop, MakeArray, Cast, and more. Use the function-name parameter to target a specific function graph instead of the EventGraph. Returns the new node's GUID for use with connect_blueprint_nodes.",
		Example: `  ue-cli add_blueprint_node --blueprint-name "BP_Smelter" --node-type "Branch" --pos-x 400 --pos-y 200
  ue-cli add_blueprint_node --blueprint-name "BP_Miner" --node-type "CallFunction" --target-function "StartMining" --pos-x 600 --pos-y 100`,
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
		Long:  "Adds an event node to a Blueprint's event graph. Common events include BeginPlay, Tick, EndPlay, ActorBeginOverlap, and any custom events. Each event type can only exist once per event graph. Returns the node GUID for wiring to other nodes.",
		Example: `  ue-cli add_event_node --blueprint-name "BP_ConveyorBelt" --event-name "BeginPlay" --pos-x 0 --pos-y 0
  ue-cli add_event_node --blueprint-name "BP_Manufacturer" --event-name "Tick"`,
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
		Long:  "Creates a connection between an output pin on one node and an input pin on another. Use node GUIDs from add_blueprint_node or read_blueprint_content. Pin names must match exactly (e.g., \"then\" for execution, \"ReturnValue\" for outputs). Use the function-name parameter when connecting nodes inside a function graph.",
		Example: `  ue-cli connect_blueprint_nodes --blueprint-name "BP_Smelter" --source-node-id "ABC123" --source-pin-name "then" --target-node-id "DEF456" --target-pin-name "execute"
  ue-cli connect_blueprint_nodes --blueprint-name "BP_Miner" --source-node-id "ABC123" --source-pin-name "ReturnValue" --target-node-id "DEF456" --target-pin-name "Condition" --function-name "CheckPower"`,
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
		Long:  "Deletes a single node from a Blueprint graph by its GUID. All connections to and from the node are automatically removed. Use read_blueprint_content or analyze_blueprint_graph to find node GUIDs. Specify function-name to target nodes inside a function graph.",
		Example: `  ue-cli delete_blueprint_node --blueprint-name "BP_Smelter" --node-id "ABC123DEF456"
  ue-cli delete_blueprint_node --blueprint-name "BP_Miner" --node-id "ABC123DEF456" --function-name "ProcessOre"`,
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
		Long:  "Sets a property on a Blueprint node or performs a semantic editing action. Supports direct property assignment (property-name + property-value) and semantic actions like add_pin, remove_pin, set_enum_type, change_pin_type, and set_function_reference. Use analyze_blueprint_graph to discover available properties on a node.",
		Example: `  ue-cli set_blueprint_node_property --blueprint-name "BP_Smelter" --node-id "ABC123" --property-name "DefaultValue" --property-value "100.0"
  ue-cli set_blueprint_node_property --blueprint-name "BP_Miner" --node-id "DEF456" --action "set_enum_type" --enum-type "EResourceType"`,
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
