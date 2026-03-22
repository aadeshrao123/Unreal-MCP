package cmd

func init() {
	ensureGroup("widgets", "Widgets (UMG)")
	registerCommands(widgetCommands)
}

var widgetCommands = []CommandSpec{
	{
		Name:  "get_widget_tree",
		Group: "widgets",
		Short: "Get the widget hierarchy of a Widget Blueprint",
		Params: []ParamSpec{
			{Name: "widget_blueprint_path", Type: "string", Required: true, Help: "Widget Blueprint path"},
		},
	},
	{
		Name:    "add_widget",
		Group:   "widgets",
		Short:   "Add a widget to a Widget Blueprint",
		LargeOp: true,
		Params: []ParamSpec{
			{Name: "widget_blueprint_path", Type: "string", Required: true, Help: "Widget Blueprint path"},
			{Name: "widget_class", Type: "string", Required: true, Help: "Widget class name"},
			{Name: "parent_widget_name", Type: "string", Required: true, Help: "Parent widget name"},
			{Name: "widget_name", Type: "string", Help: "Widget name"},
			{Name: "index", Type: "int", Default: -1, Help: "Insert index (-1 = append)"},
			{Name: "slot_properties", Type: "json", Help: "JSON object of slot properties"},
			{Name: "widget_properties", Type: "json", Help: "JSON object of widget properties"},
		},
	},
	{
		Name:  "remove_widget",
		Group: "widgets",
		Short: "Remove a widget from a Widget Blueprint",
		Params: []ParamSpec{
			{Name: "widget_blueprint_path", Type: "string", Required: true, Help: "Widget Blueprint path"},
			{Name: "widget_name", Type: "string", Required: true, Help: "Widget name to remove"},
		},
	},
	{
		Name:  "move_widget",
		Group: "widgets",
		Short: "Move a widget to a new parent",
		Params: []ParamSpec{
			{Name: "widget_blueprint_path", Type: "string", Required: true, Help: "Widget Blueprint path"},
			{Name: "widget_name", Type: "string", Required: true, Help: "Widget name"},
			{Name: "new_parent_name", Type: "string", Required: true, Help: "New parent widget name"},
			{Name: "index", Type: "int", Default: -1, Help: "Insert index (-1 = append)"},
		},
	},
	{
		Name:  "rename_widget",
		Group: "widgets",
		Short: "Rename a widget",
		Params: []ParamSpec{
			{Name: "widget_blueprint_path", Type: "string", Required: true, Help: "Widget Blueprint path"},
			{Name: "widget_name", Type: "string", Required: true, Help: "Current widget name"},
			{Name: "new_name", Type: "string", Required: true, Help: "New widget name"},
		},
	},
	{
		Name:    "duplicate_widget",
		Group:   "widgets",
		Short:   "Duplicate a widget",
		LargeOp: true,
		Params: []ParamSpec{
			{Name: "widget_blueprint_path", Type: "string", Required: true, Help: "Widget Blueprint path"},
			{Name: "widget_name", Type: "string", Required: true, Help: "Widget name to duplicate"},
			{Name: "new_name", Type: "string", Help: "New widget name"},
			{Name: "parent_widget_name", Type: "string", Help: "Parent widget name"},
		},
	},
	{
		Name:  "get_widget_properties",
		Group: "widgets",
		Short: "Get properties of a widget",
		Params: []ParamSpec{
			{Name: "widget_blueprint_path", Type: "string", Required: true, Help: "Widget Blueprint path"},
			{Name: "widget_name", Type: "string", Required: true, Help: "Widget name"},
			{Name: "filter", Type: "string", Help: "Filter property names"},
			{Name: "include_inherited", Type: "bool", Default: true, Help: "Include inherited properties"},
		},
	},
	{
		Name:    "set_widget_properties",
		Group:   "widgets",
		Short:   "Set properties on a widget",
		LargeOp: true,
		Params: []ParamSpec{
			{Name: "widget_blueprint_path", Type: "string", Required: true, Help: "Widget Blueprint path"},
			{Name: "widget_name", Type: "string", Required: true, Help: "Widget name"},
			{Name: "properties", Type: "json", Required: true, Help: "JSON object of property name-value pairs"},
		},
	},
	{
		Name:  "get_slot_properties",
		Group: "widgets",
		Short: "Get slot properties of a widget",
		Params: []ParamSpec{
			{Name: "widget_blueprint_path", Type: "string", Required: true, Help: "Widget Blueprint path"},
			{Name: "widget_name", Type: "string", Required: true, Help: "Widget name"},
			{Name: "filter", Type: "string", Help: "Filter property names"},
		},
	},
	{
		Name:  "set_slot_properties",
		Group: "widgets",
		Short: "Set slot properties on a widget",
		Params: []ParamSpec{
			{Name: "widget_blueprint_path", Type: "string", Required: true, Help: "Widget Blueprint path"},
			{Name: "widget_name", Type: "string", Required: true, Help: "Widget name"},
			{Name: "properties", Type: "json", Required: true, Help: "JSON object of slot property name-value pairs"},
		},
	},
	{
		Name:  "list_widget_types",
		Group: "widgets",
		Short: "List available widget types",
		Params: []ParamSpec{
			{Name: "filter", Type: "string", Help: "Filter by type name"},
			{Name: "include_abstract", Type: "bool", Default: false, Help: "Include abstract classes"},
			{Name: "panels_only", Type: "bool", Default: false, Help: "Show only panel widgets"},
		},
	},
}
