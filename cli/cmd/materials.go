package cmd

func init() {
	ensureGroup("materials", "Materials")
	registerCommands(materialCommands)
}

var materialCommands = []CommandSpec{
	{
		Name:  "create_material",
		Group: "materials",
		Short: "Create a new Material",
		Long:  "Creates a new empty Material asset at the specified path. You can set blend_mode, shading_model, and two_sided during creation. Call save_asset after to persist changes to disk.",
		Example: `  ue-cli create_material --name M_MyMaterial --path /Game/Materials --blend-mode opaque --shading-model default_lit
  ue-cli create_material --name M_Glass --path /Game/Materials --blend-mode translucent --two-sided true`,
		Params: []ParamSpec{
			{Name: "name", Type: "string", Required: true, Help: "Material name"},
			{Name: "path", Type: "string", Default: "/Game/Materials", Help: "Content path"},
			{Name: "blend_mode", Type: "string", Default: "opaque", Help: "Blend mode"},
			{Name: "shading_model", Type: "string", Default: "default_lit", Help: "Shading model"},
			{Name: "two_sided", Type: "bool", Default: false, Help: "Two-sided rendering"},
			{Name: "opacity_mask_clip_value", Type: "float", Help: "Opacity mask clip value"},
		},
	},
	{
		Name:  "create_material_instance",
		Group: "materials",
		Short: "Create a Material Instance",
		Long:  "Creates a Material Instance from a parent material, optionally setting scalar, vector, and texture parameter overrides. Use this for variations of a base material without duplicating the graph. Call save_asset after.",
		Example: `  ue-cli create_material_instance --parent-path /Game/Materials/M_Base --name MI_Red --path /Game/Materials
  ue-cli create_material_instance --parent-path /Game/Materials/M_Base --name MI_Custom --scalar-params '{"Roughness": 0.8}' --vector-params '{"BaseColor": {"R":1,"G":0,"B":0,"A":1}}'`,
		Params: []ParamSpec{
			{Name: "parent_path", Type: "string", Required: true, Help: "Parent material path"},
			{Name: "name", Type: "string", Required: true, Help: "Instance name"},
			{Name: "path", Type: "string", Default: "/Game/Materials", Help: "Content path"},
			{Name: "scalar_params", Type: "json", Help: "JSON string of scalar parameters"},
			{Name: "vector_params", Type: "json", Help: "JSON string of vector parameters"},
			{Name: "texture_params", Type: "json", Help: "JSON string of texture parameters"},
		},
	},
	{
		Name:    "build_material_graph",
		Group:   "materials",
		Short:   "Build complete node graph in one atomic call",
		Long:    "Builds an entire material node graph atomically: creates expression nodes and wires them together in a single call. By default clears existing expressions first (safe rebuild -- external references stay intact). Preferred over adding nodes one at a time. Call save_asset after.",
		Example: `  ue-cli build_material_graph --material-path /Game/Materials/M_Test --nodes '[{"type":"Constant3Vector","pos_x":-400,"pos_y":0,"properties":{"Constant":"(R=1,G=0,B=0)"}}]' --connections '[{"from_node":0,"to_node":"material","to_pin":"BaseColor"}]'
  ue-cli build_material_graph --material-path /Game/Materials/M_Existing --clear-existing false --nodes '[{"type":"Multiply","pos_x":-200,"pos_y":100}]' --connections '[]'`,
		LargeOp: true,
		Params: []ParamSpec{
			{Name: "material_path", Type: "string", Required: true, Help: "Material asset path"},
			{Name: "nodes", Type: "json", Required: true, Help: "JSON array of node definitions"},
			{Name: "connections", Type: "json", Required: true, Help: "JSON array of connections"},
			{Name: "clear_existing", Type: "bool", Default: true, Help: "Clear existing expressions"},
		},
	},
	{
		Name:    "get_material_info",
		Group:   "materials",
		Short:   "Inspect material properties, parameters, textures",
		Long:    "Returns material metadata including blend mode, shading model, and optionally parameters, textures, and shader statistics. Use the include flag to control which sections are returned.",
		Example: `  ue-cli get_material_info --material-path /Game/Materials/M_Test
  ue-cli get_material_info --material-path /Game/Materials/M_Test --include parameters,textures,statistics`,
		Params: []ParamSpec{
			{Name: "material_path", Type: "string", Required: true, Help: "Material asset path"},
			{Name: "include", Type: "string", Help: "Comma-sep: parameters,textures,statistics"},
		},
	},
	{
		Name:    "set_material_properties",
		Group:   "materials",
		Short:   "Bulk-set material properties",
		Long:    "Sets one or more material-level properties such as blend mode, shading model, or two-sided in a single call. Recompiles the material by default after changes. Call save_asset after.",
		Example: `  ue-cli set_material_properties --material-path /Game/Materials/M_Test --blend-mode masked --two-sided true
  ue-cli set_material_properties --material-path /Game/Materials/M_Test --shading-model unlit --recompile false`,
		Params: []ParamSpec{
			{Name: "material_path", Type: "string", Required: true, Help: "Material asset path"},
			{Name: "blend_mode", Type: "string", Help: "Blend mode"},
			{Name: "shading_model", Type: "string", Help: "Shading model"},
			{Name: "two_sided", Type: "bool", Help: "Two-sided"},
			{Name: "opacity_mask_clip_value", Type: "float", Help: "Clip value"},
			{Name: "dithered_lof_transition", Type: "bool", Help: "Dithered LOF transition"},
			{Name: "allow_negative_emissive_color", Type: "bool", Help: "Allow negative emissive"},
			{Name: "recompile", Type: "bool", Default: true, Help: "Recompile after changes"},
		},
	},
	{
		Name:    "recompile_material",
		Group:   "materials",
		Short:   "Force recompile and save",
		Long:    "Forces a recompilation of the material shader and saves the asset. Use after making manual changes to the graph or when the material appears out of date.",
		Example: `  ue-cli recompile_material --material-path /Game/Materials/M_Test`,
		Params: []ParamSpec{
			{Name: "material_path", Type: "string", Required: true, Help: "Material asset path"},
		},
	},
	{
		Name:    "get_material_errors",
		Group:   "materials",
		Short:   "Get material compilation errors",
		Long:    "Returns any compilation errors for the material. By default recompiles first to get fresh errors. Use this to diagnose broken material graphs before fixing them.",
		Example: `  ue-cli get_material_errors --material-path /Game/Materials/M_Test
  ue-cli get_material_errors --material-path /Game/Materials/M_Test --recompile false`,
		Params: []ParamSpec{
			{Name: "material_path", Type: "string", Required: true, Help: "Material asset path"},
			{Name: "recompile", Type: "bool", Default: true, Help: "Recompile first"},
		},
	},
	{
		Name:    "add_material_comments",
		Group:   "materials",
		Short:   "Add comment boxes to material graph",
		Long:    "Adds one or more comment boxes to a material graph for visual organization. Each comment has a position, size, text, and optional color. Call save_asset after.",
		Example: `  ue-cli add_material_comments --material-path /Game/Materials/M_Test --comments '[{"text":"Base Color Setup","pos_x":-500,"pos_y":-200,"size_x":400,"size_y":300}]'`,
		Params: []ParamSpec{
			{Name: "material_path", Type: "string", Required: true, Help: "Material asset path"},
			{Name: "comments", Type: "json", Required: true, Help: "JSON array of comment definitions"},
		},
	},
	{
		Name:    "get_material_graph_nodes",
		Group:   "materials",
		Short:   "Read graph nodes with verbosity control",
		Long:    "Returns all expression nodes in a material graph. Use verbosity to control detail: 'summary' for node list, 'connections' for wiring info, 'full' for all properties. Use type_filter to narrow results to specific node types.",
		Example: `  ue-cli get_material_graph_nodes --material-path /Game/Materials/M_Test
  ue-cli get_material_graph_nodes --material-path /Game/Materials/M_Test --verbosity full --type-filter TextureSample`,
		Params: []ParamSpec{
			{Name: "material_path", Type: "string", Required: true, Help: "Material asset path"},
			{Name: "type_filter", Type: "string", Help: "Filter by node type"},
			{Name: "verbosity", Type: "string", Default: "connections", Help: "summary, connections, or full"},
		},
	},
	{
		Name:    "add_material_expression",
		Group:   "materials",
		Short:   "Add a material expression node",
		Long:    "Adds a single expression node to a material graph. The node definition is a JSON object with type, position, and optional properties. Use build_material_graph instead when creating multiple nodes at once. Call save_asset after.",
		Example: `  ue-cli add_material_expression --material-path /Game/Materials/M_Test --node '{"type":"Constant3Vector","pos_x":-400,"pos_y":0,"properties":{"Constant":"(R=0.5,G=0.5,B=1.0)"}}'
  ue-cli add_material_expression --material-path /Game/Materials/M_Test --node '{"type":"TextureSample","pos_x":-600,"pos_y":0}'`,
		Params: []ParamSpec{
			{Name: "material_path", Type: "string", Required: true, Help: "Material asset path"},
			{Name: "node", Type: "json", Required: true, Help: "JSON node definition"},
		},
	},
	{
		Name:    "connect_material_expressions",
		Group:   "materials",
		Short:   "Connect two material expression nodes",
		Long:    "Wires an output pin of one node to an input pin of another node or the material output. Use to_node 'material' to connect to material-level pins (BaseColor, Normal, etc.). Call save_asset after.",
		Example: `  ue-cli connect_material_expressions --material-path /Game/Materials/M_Test --from-node 0 --to-node material --to-pin BaseColor
  ue-cli connect_material_expressions --material-path /Game/Materials/M_Test --from-node 0 --from-pin RGB --to-node 1 --to-pin A`,
		Params: []ParamSpec{
			{Name: "material_path", Type: "string", Required: true, Help: "Material asset path"},
			{Name: "from_node", Type: "int", Required: true, Help: "Source node index"},
			{Name: "to_node", Type: "string", Required: true, Help: "Target node or 'material'"},
			{Name: "to_pin", Type: "string", Required: true, Help: "Target pin name"},
			{Name: "from_pin", Type: "string", Help: "Source pin name"},
		},
	},
	{
		Name:    "delete_material_expression",
		Group:   "materials",
		Short:   "Delete a material expression by index",
		Long:    "Removes a single expression node from the material graph by its index. Note that deleting a node shifts indices of subsequent nodes. Call save_asset after.",
		Example: `  ue-cli delete_material_expression --material-path /Game/Materials/M_Test --node-index 3`,
		Params: []ParamSpec{
			{Name: "material_path", Type: "string", Required: true, Help: "Material asset path"},
			{Name: "node_index", Type: "int", Required: true, Help: "Node index"},
		},
	},
	{
		Name:    "set_material_expression_property",
		Group:   "materials",
		Short:   "Set a property on a material expression",
		Long:    "Sets a single property on a material expression node identified by index. Use get_expression_type_info to discover available properties for a node type. Call save_asset after.",
		Example: `  ue-cli set_material_expression_property --material-path /Game/Materials/M_Test --node-index 0 --property-name Constant --property-value "(R=1,G=0,B=0)"
  ue-cli set_material_expression_property --material-path /Game/Materials/M_Test --node-index 2 --property-name R --property-value 0.5`,
		Params: []ParamSpec{
			{Name: "material_path", Type: "string", Required: true, Help: "Material asset path"},
			{Name: "node_index", Type: "int", Required: true, Help: "Node index"},
			{Name: "property_name", Type: "string", Required: true, Help: "Property name"},
			{Name: "property_value", Type: "string", Required: true, Help: "Property value"},
		},
	},
	{
		Name:    "move_material_expression",
		Group:   "materials",
		Short:   "Move a material expression node",
		Long:    "Repositions a material expression node in the graph editor to the specified X/Y coordinates. Use this to organize graph layout. Call save_asset after.",
		Example: `  ue-cli move_material_expression --material-path /Game/Materials/M_Test --node-index 0 --pos-x -400 --pos-y 200`,
		Params: []ParamSpec{
			{Name: "material_path", Type: "string", Required: true, Help: "Material asset path"},
			{Name: "node_index", Type: "int", Required: true, Help: "Node index"},
			{Name: "pos_x", Type: "int", Required: true, Help: "X position"},
			{Name: "pos_y", Type: "int", Required: true, Help: "Y position"},
		},
	},
	{
		Name:    "duplicate_material_expression",
		Group:   "materials",
		Short:   "Duplicate a material expression",
		Long:    "Creates a copy of an existing material expression node at an offset position. The new node has the same type and properties but no connections. Call save_asset after.",
		Example: `  ue-cli duplicate_material_expression --material-path /Game/Materials/M_Test --node-index 0
  ue-cli duplicate_material_expression --material-path /Game/Materials/M_Test --node-index 0 --offset-x 200 --offset-y 200`,
		Params: []ParamSpec{
			{Name: "material_path", Type: "string", Required: true, Help: "Material asset path"},
			{Name: "node_index", Type: "int", Required: true, Help: "Node index to duplicate"},
			{Name: "offset_x", Type: "int", Default: 0, Help: "X offset"},
			{Name: "offset_y", Type: "int", Default: 150, Help: "Y offset"},
		},
	},
	{
		Name:    "layout_material_expressions",
		Group:   "materials",
		Short:   "Auto-layout all material expressions",
		Long:    "Automatically repositions all expression nodes in the material graph for a cleaner layout. Useful after building a graph programmatically. Call save_asset after.",
		Example: `  ue-cli layout_material_expressions --material-path /Game/Materials/M_Test`,
		Params: []ParamSpec{
			{Name: "material_path", Type: "string", Required: true, Help: "Material asset path"},
		},
	},
	{
		Name:    "get_material_instance_parameters",
		Group:   "materials",
		Short:   "Get all parameters of a material instance",
		Long:    "Returns all scalar, vector, and texture parameters of a material instance, including both overridden values and inherited defaults from the parent material.",
		Example: `  ue-cli get_material_instance_parameters --material-path /Game/Materials/MI_Red`,
		Params: []ParamSpec{
			{Name: "material_path", Type: "string", Required: true, Help: "Material instance path"},
		},
	},
	{
		Name:    "set_material_instance_parameter",
		Group:   "materials",
		Short:   "Set a parameter on a material instance",
		Long:    "Sets a single parameter override on a material instance. Specify the param_type as scalar, vector, or texture. Call save_asset after.",
		Example: `  ue-cli set_material_instance_parameter --material-path /Game/Materials/MI_Red --param-name Roughness --param-type scalar --value 0.8
  ue-cli set_material_instance_parameter --material-path /Game/Materials/MI_Red --param-name BaseColor --param-type vector --value "(R=1,G=0,B=0,A=1)"`,
		Params: []ParamSpec{
			{Name: "material_path", Type: "string", Required: true, Help: "Material instance path"},
			{Name: "param_name", Type: "string", Required: true, Help: "Parameter name"},
			{Name: "param_type", Type: "string", Required: true, Help: "Parameter type"},
			{Name: "value", Type: "string", Required: true, Help: "Parameter value"},
		},
	},
	{
		Name:    "list_material_expression_types",
		Group:   "materials",
		Short:   "List available material expression types",
		Long:    "Lists all available material expression node types that can be used in material graphs. Use filter to narrow results by name. Returns type names and optionally details about each type.",
		Example: `  ue-cli list_material_expression_types --filter Texture --max-results 20
  ue-cli list_material_expression_types --include-details false`,
		Params: []ParamSpec{
			{Name: "filter", Type: "string", Help: "Filter by type name"},
			{Name: "max_results", Type: "int", Default: 0, Help: "Max results (0 = all)"},
			{Name: "include_details", Type: "bool", Default: true, Help: "Include type details"},
		},
	},
	{
		Name:    "get_expression_type_info",
		Group:   "materials",
		Short:   "Look up pins & properties for a node type",
		Long:    "Returns the input/output pins and configurable properties for a material expression type without creating a node. Use this to discover available pins before connecting nodes.",
		Example: `  ue-cli get_expression_type_info --type-name Multiply
  ue-cli get_expression_type_info --type-name TextureSample`,
		Params: []ParamSpec{
			{Name: "type_name", Type: "string", Required: true, Help: "Expression type (e.g. Multiply, TextureSample)"},
		},
	},
	{
		Name:    "disconnect_material_expression",
		Group:   "materials",
		Short:   "Disconnect a specific input pin",
		Long:    "Breaks the connection to a specific input pin on a material expression node. The node and its other connections remain intact. Call save_asset after.",
		Example: `  ue-cli disconnect_material_expression --material-path /Game/Materials/M_Test --node-index 2 --input-pin A`,
		Params: []ParamSpec{
			{Name: "material_path", Type: "string", Required: true, Help: "Material asset path"},
			{Name: "node_index", Type: "int", Required: true, Help: "Node index"},
			{Name: "input_pin", Type: "string", Required: true, Help: "Input pin name"},
		},
	},
	{
		Name:    "search_material_functions",
		Group:   "materials",
		Short:   "Find Material Functions by name",
		Long:    "Searches for Material Function assets by name. Use include_engine to also search engine-provided functions. Returns asset paths that can be used as MaterialFunctionCall nodes in material graphs.",
		Example: `  ue-cli search_material_functions --filter Fresnel
  ue-cli search_material_functions --filter Blend --include-engine true --max-results 10`,
		Params: []ParamSpec{
			{Name: "filter", Type: "string", Help: "Name filter"},
			{Name: "path", Type: "string", Default: "/Game", Help: "Search path"},
			{Name: "max_results", Type: "int", Default: 50, Help: "Max results"},
			{Name: "include_engine", Type: "bool", Default: false, Help: "Include engine functions"},
		},
	},
	{
		Name:    "validate_material_graph",
		Group:   "materials",
		Short:   "Diagnose orphaned, dead-end, and unconnected nodes",
		Long:    "Analyzes a material graph and reports orphaned nodes (no connections), dead-end nodes (output not connected to anything), and other structural issues. Use before cleanup_material_graph to preview problems.",
		Example: `  ue-cli validate_material_graph --material-path /Game/Materials/M_Test`,
		Params: []ParamSpec{
			{Name: "material_path", Type: "string", Required: true, Help: "Material asset path"},
		},
	},
	{
		Name:    "trace_material_connection",
		Group:   "materials",
		Short:   "Trace upstream/downstream from a node",
		Long:    "Recursively traces connections from a node upstream (toward inputs), downstream (toward material output), or both. Use max_depth to control how far to trace. Helpful for understanding data flow in complex graphs.",
		Example: `  ue-cli trace_material_connection --material-path /Game/Materials/M_Test --node-index 3 --direction upstream --max-depth 5
  ue-cli trace_material_connection --material-path /Game/Materials/M_Test --node-index 0 --direction both`,
		Params: []ParamSpec{
			{Name: "material_path", Type: "string", Required: true, Help: "Material asset path"},
			{Name: "node_index", Type: "int", Required: true, Help: "Node index"},
			{Name: "direction", Type: "string", Default: "both", Help: "upstream, downstream, or both"},
			{Name: "max_depth", Type: "int", Default: 1, Help: "Max trace depth"},
		},
	},
	{
		Name:    "cleanup_material_graph",
		Group:   "materials",
		Short:   "Delete orphaned/dead-end nodes",
		Long:    "Removes orphaned or dead-end nodes from a material graph. Use dry_run to preview what would be deleted without making changes. Mode controls which nodes to target: orphaned (no connections), dead_ends (output unused), or all. Call save_asset after.",
		Example: `  ue-cli cleanup_material_graph --material-path /Game/Materials/M_Test --dry-run true
  ue-cli cleanup_material_graph --material-path /Game/Materials/M_Test --mode all`,
		Params: []ParamSpec{
			{Name: "material_path", Type: "string", Required: true, Help: "Material asset path"},
			{Name: "mode", Type: "string", Default: "orphaned", Help: "orphaned, dead_ends, or all"},
			{Name: "dry_run", Type: "bool", Default: false, Help: "Preview without deleting"},
		},
	},

	// -- Material Functions --
	{
		Name:    "create_material_function",
		Group:   "materials",
		Short:   "Create a new Material Function asset",
		Long:    "Creates a new Material Function asset that can be reused across multiple materials via MaterialFunctionCall nodes. Set expose_to_library to make it appear in the material editor function list. Call save_asset after.",
		Example: `  ue-cli create_material_function --name MF_CustomBlend --path /Game/Materials/Functions --description "Custom blend utility"
  ue-cli create_material_function --name MF_Helper --expose-to-library false`,
		Params: []ParamSpec{
			{Name: "name", Type: "string", Required: true, Help: "Function name"},
			{Name: "path", Type: "string", Default: "/Game/Materials/Functions", Help: "Content path"},
			{Name: "description", Type: "string", Help: "Description"},
			{Name: "expose_to_library", Type: "bool", Default: true, Help: "Expose to material library"},
		},
	},
	{
		Name:    "get_material_function_info",
		Group:   "materials",
		Short:   "Inspect a Material Function",
		Long:    "Returns the inputs, outputs, and internal expression nodes of a Material Function. Use this to understand a function's interface before using it in a material graph.",
		Example: `  ue-cli get_material_function_info --function-path /Game/Materials/Functions/MF_CustomBlend`,
		Params: []ParamSpec{
			{Name: "function_path", Type: "string", Required: true, Help: "Function asset path"},
		},
	},
	{
		Name:    "build_material_function_graph",
		Group:   "materials",
		Short:   "Build complete MF node graph atomically",
		Long:    "Builds an entire Material Function node graph in a single atomic call, similar to build_material_graph but for functions. Clears existing expressions by default. Call save_asset after.",
		Example: `  ue-cli build_material_function_graph --function-path /Game/Materials/Functions/MF_CustomBlend --nodes '[{"type":"Multiply","pos_x":-200,"pos_y":0}]' --connections '[{"from_node":0,"to_node":1,"to_pin":"Result"}]'`,
		LargeOp: true,
		Params: []ParamSpec{
			{Name: "function_path", Type: "string", Required: true, Help: "Function asset path"},
			{Name: "nodes", Type: "json", Required: true, Help: "JSON array of node definitions"},
			{Name: "connections", Type: "json", Required: true, Help: "JSON array of connections"},
			{Name: "clear_existing", Type: "bool", Default: true, Help: "Clear existing expressions"},
		},
	},
	{
		Name:    "add_material_function_input",
		Group:   "materials",
		Short:   "Add a FunctionInput pin",
		Long:    "Adds an input pin to a Material Function. The input type can be Scalar, Vector2, Vector3, Vector4, Texture2D, TextureCube, StaticBool, Bool, or MaterialAttributes. Set sort_priority to control pin ordering. Call save_asset after.",
		Example: `  ue-cli add_material_function_input --function-path /Game/Materials/Functions/MF_Blend --input-name BaseColor --input-type Vector3
  ue-cli add_material_function_input --function-path /Game/Materials/Functions/MF_Blend --input-name Strength --input-type Scalar --preview-value 1.0 --use-preview-as-default true`,
		Params: []ParamSpec{
			{Name: "function_path", Type: "string", Required: true, Help: "Function asset path"},
			{Name: "input_name", Type: "string", Required: true, Help: "Input name"},
			{Name: "input_type", Type: "string", Default: "Scalar", Help: "Input type"},
			{Name: "description", Type: "string", Help: "Description"},
			{Name: "sort_priority", Type: "int", Default: 0, Help: "Sort priority"},
			{Name: "use_preview_as_default", Type: "bool", Default: false, Help: "Use preview as default"},
			{Name: "preview_value", Type: "string", Help: "Preview value"},
			{Name: "pos_x", Type: "int", Default: -600, Help: "X position"},
			{Name: "pos_y", Type: "int", Default: 0, Help: "Y position"},
		},
	},
	{
		Name:    "add_material_function_output",
		Group:   "materials",
		Short:   "Add a FunctionOutput pin",
		Long:    "Adds an output pin to a Material Function. The output receives its type from whatever expression is connected to it. Set sort_priority to control pin ordering when the function has multiple outputs. Call save_asset after.",
		Example: `  ue-cli add_material_function_output --function-path /Game/Materials/Functions/MF_Blend --output-name Result
  ue-cli add_material_function_output --function-path /Game/Materials/Functions/MF_Blend --output-name Alpha --sort-priority 1 --pos-y 200`,
		Params: []ParamSpec{
			{Name: "function_path", Type: "string", Required: true, Help: "Function asset path"},
			{Name: "output_name", Type: "string", Required: true, Help: "Output name"},
			{Name: "description", Type: "string", Help: "Description"},
			{Name: "sort_priority", Type: "int", Default: 0, Help: "Sort priority"},
			{Name: "pos_x", Type: "int", Default: 200, Help: "X position"},
			{Name: "pos_y", Type: "int", Default: 0, Help: "Y position"},
		},
	},
	{
		Name:    "set_material_function_input",
		Group:   "materials",
		Short:   "Modify an existing FunctionInput",
		Long:    "Modifies properties of an existing FunctionInput node in a Material Function, identified by node_index. You can change the name, type, description, preview value, and sort priority. Call save_asset after.",
		Example: `  ue-cli set_material_function_input --function-path /Game/Materials/Functions/MF_Blend --node-index 0 --input-name Opacity --input-type Scalar
  ue-cli set_material_function_input --function-path /Game/Materials/Functions/MF_Blend --node-index 1 --preview-value 0.5 --use-preview-as-default true`,
		Params: []ParamSpec{
			{Name: "function_path", Type: "string", Required: true, Help: "Function asset path"},
			{Name: "node_index", Type: "int", Required: true, Help: "Node index"},
			{Name: "input_name", Type: "string", Help: "Input name"},
			{Name: "input_type", Type: "string", Help: "Input type"},
			{Name: "description", Type: "string", Help: "Description"},
			{Name: "sort_priority", Type: "int", Help: "Sort priority"},
			{Name: "use_preview_as_default", Type: "bool", Help: "Use preview as default"},
			{Name: "preview_value", Type: "string", Help: "Preview value"},
		},
	},
	{
		Name:    "set_material_function_output",
		Group:   "materials",
		Short:   "Modify an existing FunctionOutput",
		Long:    "Modifies properties of an existing FunctionOutput node in a Material Function, identified by node_index. You can change the name, description, and sort priority. Call save_asset after.",
		Example: `  ue-cli set_material_function_output --function-path /Game/Materials/Functions/MF_Blend --node-index 2 --output-name BlendedColor
  ue-cli set_material_function_output --function-path /Game/Materials/Functions/MF_Blend --node-index 2 --description "Final blended output" --sort-priority 0`,
		Params: []ParamSpec{
			{Name: "function_path", Type: "string", Required: true, Help: "Function asset path"},
			{Name: "node_index", Type: "int", Required: true, Help: "Node index"},
			{Name: "output_name", Type: "string", Help: "Output name"},
			{Name: "description", Type: "string", Help: "Description"},
			{Name: "sort_priority", Type: "int", Help: "Sort priority"},
		},
	},
	{
		Name:    "validate_material_function",
		Group:   "materials",
		Short:   "Validate a Material Function",
		Long:    "Checks a Material Function for structural issues such as missing inputs/outputs, orphaned nodes, and broken connections. Use this to verify a function is well-formed before using it in materials.",
		Example: `  ue-cli validate_material_function --function-path /Game/Materials/Functions/MF_Blend`,
		Params: []ParamSpec{
			{Name: "function_path", Type: "string", Required: true, Help: "Function asset path"},
		},
	},
	{
		Name:    "cleanup_material_function",
		Group:   "materials",
		Short:   "Cleanup orphaned nodes in a Material Function",
		Long:    "Removes orphaned nodes from a Material Function graph. Use dry_run to preview which nodes would be deleted without making changes. Call save_asset after.",
		Example: `  ue-cli cleanup_material_function --function-path /Game/Materials/Functions/MF_Blend --dry-run true
  ue-cli cleanup_material_function --function-path /Game/Materials/Functions/MF_Blend`,
		Params: []ParamSpec{
			{Name: "function_path", Type: "string", Required: true, Help: "Function asset path"},
			{Name: "dry_run", Type: "bool", Default: false, Help: "Preview without deleting"},
		},
	},
}
