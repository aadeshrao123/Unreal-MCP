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
		LargeOp: true,
		Params: []ParamSpec{
			{Name: "material_path", Type: "string", Required: true, Help: "Material asset path"},
			{Name: "nodes", Type: "json", Required: true, Help: "JSON array of node definitions"},
			{Name: "connections", Type: "json", Required: true, Help: "JSON array of connections"},
			{Name: "clear_existing", Type: "bool", Default: true, Help: "Clear existing expressions"},
		},
	},
	{
		Name:  "get_material_info",
		Group: "materials",
		Short: "Inspect material properties, parameters, textures",
		Params: []ParamSpec{
			{Name: "material_path", Type: "string", Required: true, Help: "Material asset path"},
			{Name: "include", Type: "string", Help: "Comma-sep: parameters,textures,statistics"},
		},
	},
	{
		Name:  "set_material_properties",
		Group: "materials",
		Short: "Bulk-set material properties",
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
		Name:  "recompile_material",
		Group: "materials",
		Short: "Force recompile and save",
		Params: []ParamSpec{
			{Name: "material_path", Type: "string", Required: true, Help: "Material asset path"},
		},
	},
	{
		Name:  "get_material_errors",
		Group: "materials",
		Short: "Get material compilation errors",
		Params: []ParamSpec{
			{Name: "material_path", Type: "string", Required: true, Help: "Material asset path"},
			{Name: "recompile", Type: "bool", Default: true, Help: "Recompile first"},
		},
	},
	{
		Name:  "add_material_comments",
		Group: "materials",
		Short: "Add comment boxes to material graph",
		Params: []ParamSpec{
			{Name: "material_path", Type: "string", Required: true, Help: "Material asset path"},
			{Name: "comments", Type: "json", Required: true, Help: "JSON array of comment definitions"},
		},
	},
	{
		Name:  "get_material_graph_nodes",
		Group: "materials",
		Short: "Read graph nodes with verbosity control",
		Params: []ParamSpec{
			{Name: "material_path", Type: "string", Required: true, Help: "Material asset path"},
			{Name: "type_filter", Type: "string", Help: "Filter by node type"},
			{Name: "verbosity", Type: "string", Default: "connections", Help: "summary, connections, or full"},
		},
	},
	{
		Name:  "add_material_expression",
		Group: "materials",
		Short: "Add a material expression node",
		Params: []ParamSpec{
			{Name: "material_path", Type: "string", Required: true, Help: "Material asset path"},
			{Name: "node", Type: "json", Required: true, Help: "JSON node definition"},
		},
	},
	{
		Name:  "connect_material_expressions",
		Group: "materials",
		Short: "Connect two material expression nodes",
		Params: []ParamSpec{
			{Name: "material_path", Type: "string", Required: true, Help: "Material asset path"},
			{Name: "from_node", Type: "int", Required: true, Help: "Source node index"},
			{Name: "to_node", Type: "string", Required: true, Help: "Target node or 'material'"},
			{Name: "to_pin", Type: "string", Required: true, Help: "Target pin name"},
			{Name: "from_pin", Type: "string", Help: "Source pin name"},
		},
	},
	{
		Name:  "delete_material_expression",
		Group: "materials",
		Short: "Delete a material expression by index",
		Params: []ParamSpec{
			{Name: "material_path", Type: "string", Required: true, Help: "Material asset path"},
			{Name: "node_index", Type: "int", Required: true, Help: "Node index"},
		},
	},
	{
		Name:  "set_material_expression_property",
		Group: "materials",
		Short: "Set a property on a material expression",
		Params: []ParamSpec{
			{Name: "material_path", Type: "string", Required: true, Help: "Material asset path"},
			{Name: "node_index", Type: "int", Required: true, Help: "Node index"},
			{Name: "property_name", Type: "string", Required: true, Help: "Property name"},
			{Name: "property_value", Type: "string", Required: true, Help: "Property value"},
		},
	},
	{
		Name:  "move_material_expression",
		Group: "materials",
		Short: "Move a material expression node",
		Params: []ParamSpec{
			{Name: "material_path", Type: "string", Required: true, Help: "Material asset path"},
			{Name: "node_index", Type: "int", Required: true, Help: "Node index"},
			{Name: "pos_x", Type: "int", Required: true, Help: "X position"},
			{Name: "pos_y", Type: "int", Required: true, Help: "Y position"},
		},
	},
	{
		Name:  "duplicate_material_expression",
		Group: "materials",
		Short: "Duplicate a material expression",
		Params: []ParamSpec{
			{Name: "material_path", Type: "string", Required: true, Help: "Material asset path"},
			{Name: "node_index", Type: "int", Required: true, Help: "Node index to duplicate"},
			{Name: "offset_x", Type: "int", Default: 0, Help: "X offset"},
			{Name: "offset_y", Type: "int", Default: 150, Help: "Y offset"},
		},
	},
	{
		Name:  "layout_material_expressions",
		Group: "materials",
		Short: "Auto-layout all material expressions",
		Params: []ParamSpec{
			{Name: "material_path", Type: "string", Required: true, Help: "Material asset path"},
		},
	},
	{
		Name:  "get_material_instance_parameters",
		Group: "materials",
		Short: "Get all parameters of a material instance",
		Params: []ParamSpec{
			{Name: "material_path", Type: "string", Required: true, Help: "Material instance path"},
		},
	},
	{
		Name:  "set_material_instance_parameter",
		Group: "materials",
		Short: "Set a parameter on a material instance",
		Params: []ParamSpec{
			{Name: "material_path", Type: "string", Required: true, Help: "Material instance path"},
			{Name: "param_name", Type: "string", Required: true, Help: "Parameter name"},
			{Name: "param_type", Type: "string", Required: true, Help: "Parameter type"},
			{Name: "value", Type: "string", Required: true, Help: "Parameter value"},
		},
	},
	{
		Name:  "list_material_expression_types",
		Group: "materials",
		Short: "List available material expression types",
		Params: []ParamSpec{
			{Name: "filter", Type: "string", Help: "Filter by type name"},
			{Name: "max_results", Type: "int", Default: 0, Help: "Max results (0 = all)"},
			{Name: "include_details", Type: "bool", Default: true, Help: "Include type details"},
		},
	},
	{
		Name:  "get_expression_type_info",
		Group: "materials",
		Short: "Look up pins & properties for a node type",
		Params: []ParamSpec{
			{Name: "type_name", Type: "string", Required: true, Help: "Expression type (e.g. Multiply, TextureSample)"},
		},
	},
	{
		Name:  "disconnect_material_expression",
		Group: "materials",
		Short: "Disconnect a specific input pin",
		Params: []ParamSpec{
			{Name: "material_path", Type: "string", Required: true, Help: "Material asset path"},
			{Name: "node_index", Type: "int", Required: true, Help: "Node index"},
			{Name: "input_pin", Type: "string", Required: true, Help: "Input pin name"},
		},
	},
	{
		Name:  "search_material_functions",
		Group: "materials",
		Short: "Find Material Functions by name",
		Params: []ParamSpec{
			{Name: "filter", Type: "string", Help: "Name filter"},
			{Name: "path", Type: "string", Default: "/Game", Help: "Search path"},
			{Name: "max_results", Type: "int", Default: 50, Help: "Max results"},
			{Name: "include_engine", Type: "bool", Default: false, Help: "Include engine functions"},
		},
	},
	{
		Name:  "validate_material_graph",
		Group: "materials",
		Short: "Diagnose orphaned, dead-end, and unconnected nodes",
		Params: []ParamSpec{
			{Name: "material_path", Type: "string", Required: true, Help: "Material asset path"},
		},
	},
	{
		Name:  "trace_material_connection",
		Group: "materials",
		Short: "Trace upstream/downstream from a node",
		Params: []ParamSpec{
			{Name: "material_path", Type: "string", Required: true, Help: "Material asset path"},
			{Name: "node_index", Type: "int", Required: true, Help: "Node index"},
			{Name: "direction", Type: "string", Default: "both", Help: "upstream, downstream, or both"},
			{Name: "max_depth", Type: "int", Default: 1, Help: "Max trace depth"},
		},
	},
	{
		Name:  "cleanup_material_graph",
		Group: "materials",
		Short: "Delete orphaned/dead-end nodes",
		Params: []ParamSpec{
			{Name: "material_path", Type: "string", Required: true, Help: "Material asset path"},
			{Name: "mode", Type: "string", Default: "orphaned", Help: "orphaned, dead_ends, or all"},
			{Name: "dry_run", Type: "bool", Default: false, Help: "Preview without deleting"},
		},
	},

	// -- Material Functions --
	{
		Name:  "create_material_function",
		Group: "materials",
		Short: "Create a new Material Function asset",
		Params: []ParamSpec{
			{Name: "name", Type: "string", Required: true, Help: "Function name"},
			{Name: "path", Type: "string", Default: "/Game/Materials/Functions", Help: "Content path"},
			{Name: "description", Type: "string", Help: "Description"},
			{Name: "expose_to_library", Type: "bool", Default: true, Help: "Expose to material library"},
		},
	},
	{
		Name:  "get_material_function_info",
		Group: "materials",
		Short: "Inspect a Material Function",
		Params: []ParamSpec{
			{Name: "function_path", Type: "string", Required: true, Help: "Function asset path"},
		},
	},
	{
		Name:    "build_material_function_graph",
		Group:   "materials",
		Short:   "Build complete MF node graph atomically",
		LargeOp: true,
		Params: []ParamSpec{
			{Name: "function_path", Type: "string", Required: true, Help: "Function asset path"},
			{Name: "nodes", Type: "json", Required: true, Help: "JSON array of node definitions"},
			{Name: "connections", Type: "json", Required: true, Help: "JSON array of connections"},
			{Name: "clear_existing", Type: "bool", Default: true, Help: "Clear existing expressions"},
		},
	},
	{
		Name:  "add_material_function_input",
		Group: "materials",
		Short: "Add a FunctionInput pin",
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
		Name:  "add_material_function_output",
		Group: "materials",
		Short: "Add a FunctionOutput pin",
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
		Name:  "set_material_function_input",
		Group: "materials",
		Short: "Modify an existing FunctionInput",
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
		Name:  "set_material_function_output",
		Group: "materials",
		Short: "Modify an existing FunctionOutput",
		Params: []ParamSpec{
			{Name: "function_path", Type: "string", Required: true, Help: "Function asset path"},
			{Name: "node_index", Type: "int", Required: true, Help: "Node index"},
			{Name: "output_name", Type: "string", Help: "Output name"},
			{Name: "description", Type: "string", Help: "Description"},
			{Name: "sort_priority", Type: "int", Help: "Sort priority"},
		},
	},
	{
		Name:  "validate_material_function",
		Group: "materials",
		Short: "Validate a Material Function",
		Params: []ParamSpec{
			{Name: "function_path", Type: "string", Required: true, Help: "Function asset path"},
		},
	},
	{
		Name:  "cleanup_material_function",
		Group: "materials",
		Short: "Cleanup orphaned nodes in a Material Function",
		Params: []ParamSpec{
			{Name: "function_path", Type: "string", Required: true, Help: "Function asset path"},
			{Name: "dry_run", Type: "bool", Default: false, Help: "Preview without deleting"},
		},
	},
}
