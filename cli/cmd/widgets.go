package cmd

func init() {
	ensureGroup("widgets", "Widgets (UMG)")
	registerCommands(widgetCommands)
}

var widgetCommands = []CommandSpec{
	{
		Name:    "get_widget_tree",
		Group:   "widgets",
		Short:   "Get the widget hierarchy of a Widget Blueprint",
		Long:    "Returns the full widget hierarchy of a Widget Blueprint as a tree. Shows each widget's class, name, and nesting level. Use this before add_widget or move_widget to understand the existing structure and find valid parent_widget_name values.",
		Example: "ue-cli get_widget_tree --widget-blueprint-path /Game/UI/WBP_MainHUD",
		Params: []ParamSpec{
			{Name: "widget_blueprint_path", Type: "string", Required: true, Help: "Widget Blueprint path"},
		},
	},
	{
		Name:    "add_widget",
		Group:   "widgets",
		Short:   "Add a widget to a Widget Blueprint",
		Long:    "Adds a new widget as a child of an existing widget in a Widget Blueprint. The parent_widget_name must be a panel widget (CanvasPanel, VerticalBox, HorizontalBox, Overlay, etc.) that supports children. Use slot_properties to set layout properties that belong to the parent's slot type (e.g., Anchors and Offsets for CanvasPanel slots, Size.Value for box slots). Use widget_properties to set properties on the widget itself (e.g., Text, ColorAndOpacity, Font). Run list_widget_types to discover available widget classes.",
		Example: `ue-cli add_widget --widget-blueprint-path /Game/UI/WBP_MainHUD --widget-class TextBlock --parent-widget-name RootCanvas --widget-name "TitleText" --widget-properties '{"Text":"Hello World"}'
ue-cli add_widget --widget-blueprint-path /Game/UI/WBP_MainHUD --widget-class Image --parent-widget-name MainOverlay --slot-properties '{"Padding":{"Left":10,"Top":5,"Right":10,"Bottom":5}}'
ue-cli add_widget --widget-blueprint-path /Game/UI/WBP_MainHUD --widget-class VerticalBox --parent-widget-name RootCanvas --widget-name "LeftPanel" --slot-properties '{"LayoutData":{"Anchors":{"Minimum":{"X":0,"Y":0},"Maximum":{"X":0.3,"Y":1}}}}'`,
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
		Name:    "remove_widget",
		Group:   "widgets",
		Short:   "Remove a widget from a Widget Blueprint",
		Long:    "Removes a widget and all its children from a Widget Blueprint. The widget is identified by name. Use get_widget_tree to find the exact widget name before removing.",
		Example: "ue-cli remove_widget --widget-blueprint-path /Game/UI/WBP_MainHUD --widget-name TitleText",
		Params: []ParamSpec{
			{Name: "widget_blueprint_path", Type: "string", Required: true, Help: "Widget Blueprint path"},
			{Name: "widget_name", Type: "string", Required: true, Help: "Widget name to remove"},
		},
	},
	{
		Name:    "move_widget",
		Group:   "widgets",
		Short:   "Move a widget to a new parent",
		Long:    "Reparents a widget (and all its children) to a new parent widget within the same Widget Blueprint. The new parent must be a panel widget that supports children. Use index to control insertion order among siblings (-1 appends to end).",
		Example: `ue-cli move_widget --widget-blueprint-path /Game/UI/WBP_MainHUD --widget-name TitleText --new-parent-name HeaderBox
ue-cli move_widget --widget-blueprint-path /Game/UI/WBP_MainHUD --widget-name StatusPanel --new-parent-name RootCanvas --index 0`,
		Params: []ParamSpec{
			{Name: "widget_blueprint_path", Type: "string", Required: true, Help: "Widget Blueprint path"},
			{Name: "widget_name", Type: "string", Required: true, Help: "Widget name"},
			{Name: "new_parent_name", Type: "string", Required: true, Help: "New parent widget name"},
			{Name: "index", Type: "int", Default: -1, Help: "Insert index (-1 = append)"},
		},
	},
	{
		Name:    "rename_widget",
		Group:   "widgets",
		Short:   "Rename a widget",
		Long:    "Renames a widget within a Widget Blueprint. Updates all internal references to use the new name. The new name must be unique within the blueprint.",
		Example: "ue-cli rename_widget --widget-blueprint-path /Game/UI/WBP_MainHUD --widget-name TextBlock_0 --new-name HealthLabel",
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
		Long:    "Creates a copy of a widget and all its children. The duplicate is placed under the same parent by default, or under a different parent if parent_widget_name is specified. Useful for creating repeated UI elements like list items or grid cells.",
		Example: `ue-cli duplicate_widget --widget-blueprint-path /Game/UI/WBP_MainHUD --widget-name ItemSlot --new-name ItemSlot2
ue-cli duplicate_widget --widget-blueprint-path /Game/UI/WBP_MainHUD --widget-name ResourceRow --new-name OreRow --parent-widget-name ResourceList`,
		LargeOp: true,
		Params: []ParamSpec{
			{Name: "widget_blueprint_path", Type: "string", Required: true, Help: "Widget Blueprint path"},
			{Name: "widget_name", Type: "string", Required: true, Help: "Widget name to duplicate"},
			{Name: "new_name", Type: "string", Help: "New widget name"},
			{Name: "parent_widget_name", Type: "string", Help: "Parent widget name"},
		},
	},
	{
		Name:    "get_widget_properties",
		Group:   "widgets",
		Short:   "Get properties of a widget",
		Long:    "Returns all editable properties of a specific widget within a Widget Blueprint. Use filter to narrow results by property name substring. Properties include visual settings (color, font, visibility), content (text, image), and behavior (is_enabled, cursor). Set include_inherited=false to see only properties declared on the widget's own class.",
		Example: `ue-cli get_widget_properties --widget-blueprint-path /Game/UI/WBP_MainHUD --widget-name TitleText
ue-cli get_widget_properties --widget-blueprint-path /Game/UI/WBP_MainHUD --widget-name HealthBar --filter "Percent"
ue-cli get_widget_properties --widget-blueprint-path /Game/UI/WBP_MainHUD --widget-name TitleText --include-inherited=false`,
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
		Long:    "Sets one or more properties on a widget within a Widget Blueprint. Pass a JSON object mapping property names to values. Supports all UPROPERTY types including structs, enums, colors, and object references. Use get_widget_properties first to discover available property names and their current values.",
		Example: `ue-cli set_widget_properties --widget-blueprint-path /Game/UI/WBP_MainHUD --widget-name TitleText --properties '{"Text":"Game Over","ColorAndOpacity":{"SpecifiedColor":{"R":1,"G":0,"B":0,"A":1}}}'
ue-cli set_widget_properties --widget-blueprint-path /Game/UI/WBP_MainHUD --widget-name HealthBar --properties '{"Percent": 0.75}'`,
		LargeOp: true,
		Params: []ParamSpec{
			{Name: "widget_blueprint_path", Type: "string", Required: true, Help: "Widget Blueprint path"},
			{Name: "widget_name", Type: "string", Required: true, Help: "Widget name"},
			{Name: "properties", Type: "json", Required: true, Help: "JSON object of property name-value pairs"},
		},
	},
	{
		Name:    "get_slot_properties",
		Group:   "widgets",
		Short:   "Get slot properties of a widget",
		Long:    "Returns the slot (layout) properties of a widget. Slot properties are determined by the parent widget type -- CanvasPanel children have Anchors/Offsets, VerticalBox/HorizontalBox children have Size/HorizontalAlignment/VerticalAlignment/Padding. Use filter to narrow results.",
		Example: `ue-cli get_slot_properties --widget-blueprint-path /Game/UI/WBP_MainHUD --widget-name TitleText
ue-cli get_slot_properties --widget-blueprint-path /Game/UI/WBP_MainHUD --widget-name HealthBar --filter "Padding"`,
		Params: []ParamSpec{
			{Name: "widget_blueprint_path", Type: "string", Required: true, Help: "Widget Blueprint path"},
			{Name: "widget_name", Type: "string", Required: true, Help: "Widget name"},
			{Name: "filter", Type: "string", Help: "Filter property names"},
		},
	},
	{
		Name:    "set_slot_properties",
		Group:   "widgets",
		Short:   "Set slot properties on a widget",
		Long:    "Sets layout slot properties on a widget. These are properties controlled by the parent panel, not the widget itself. For CanvasPanel slots: LayoutData (Anchors, Offsets), Alignment, bAutoSize, ZOrder. For box slots: Size (SizeRule, Value), HorizontalAlignment, VerticalAlignment, Padding. Use get_slot_properties to discover available slot properties.",
		Example: `ue-cli set_slot_properties --widget-blueprint-path /Game/UI/WBP_MainHUD --widget-name TitleText --properties '{"Padding":{"Left":20,"Top":10,"Right":20,"Bottom":10}}'
ue-cli set_slot_properties --widget-blueprint-path /Game/UI/WBP_MainHUD --widget-name LeftPanel --properties '{"Size":{"SizeRule":"Fill","Value":1.0}}'`,
		Params: []ParamSpec{
			{Name: "widget_blueprint_path", Type: "string", Required: true, Help: "Widget Blueprint path"},
			{Name: "widget_name", Type: "string", Required: true, Help: "Widget name"},
			{Name: "properties", Type: "json", Required: true, Help: "JSON object of slot property name-value pairs"},
		},
	},
	{
		Name:    "list_widget_types",
		Group:   "widgets",
		Short:   "List available widget types",
		Long:    "Lists all UWidget subclasses available for use with add_widget. Use filter to search by class name. Set panels_only=true to see only panel widgets (CanvasPanel, VerticalBox, HorizontalBox, Overlay, etc.) that can contain children and serve as parent_widget_name values.",
		Example: `ue-cli list_widget_types
ue-cli list_widget_types --filter "Text"
ue-cli list_widget_types --panels-only`,
		Params: []ParamSpec{
			{Name: "filter", Type: "string", Help: "Filter by type name"},
			{Name: "include_abstract", Type: "bool", Default: false, Help: "Include abstract classes"},
			{Name: "panels_only", Type: "bool", Default: false, Help: "Show only panel widgets"},
		},
	},
}
