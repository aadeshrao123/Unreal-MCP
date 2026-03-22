package cmd

func init() {
	ensureGroup("editor", "Editor & Level")
	registerCommands(editorCommands)
}

var editorCommands = []CommandSpec{
	{
		Name:  "get_actors_in_level",
		Group: "editor",
		Short: "List all actors in the current level",
	},
	{
		Name:  "find_actors_by_name",
		Group: "editor",
		Short: "Find actors by name pattern",
		Params: []ParamSpec{
			{Name: "pattern", Type: "string", Required: true, Help: "Name pattern to search"},
		},
	},
	{
		Name:  "spawn_actor",
		Group: "editor",
		Short: "Spawn an actor in the level",
		Params: []ParamSpec{
			{Name: "name", Type: "string", Required: true, Help: "Actor name"},
			{Name: "actor_type", Type: "string", Default: "StaticMeshActor", Help: "Actor type"},
			{Name: "location", Type: "json", Help: "JSON array [x,y,z]"},
			{Name: "rotation", Type: "json", Help: "JSON array [pitch,yaw,roll]"},
			{Name: "scale", Type: "json", Help: "JSON array [x,y,z]"},
			{Name: "static_mesh", Type: "string", Help: "Static mesh path"},
		},
	},
	{
		Name:  "delete_actor",
		Group: "editor",
		Short: "Delete an actor from the level",
		Params: []ParamSpec{
			{Name: "name", Type: "string", Required: true, Help: "Actor name"},
		},
	},
	{
		Name:  "set_actor_transform",
		Group: "editor",
		Short: "Set actor location/rotation/scale",
		Params: []ParamSpec{
			{Name: "name", Type: "string", Required: true, Help: "Actor name"},
			{Name: "location", Type: "json", Help: "JSON array [x,y,z]"},
			{Name: "rotation", Type: "json", Help: "JSON array [pitch,yaw,roll]"},
			{Name: "scale", Type: "json", Help: "JSON array [x,y,z]"},
		},
	},
	{
		Name:  "spawn_blueprint_actor",
		Group: "editor",
		Short: "Spawn a Blueprint actor instance",
		Params: []ParamSpec{
			{Name: "blueprint_path", Type: "string", Required: true, Help: "Blueprint asset path"},
			{Name: "location", Type: "json", Help: "JSON array [x,y,z]"},
			{Name: "rotation", Type: "json", Help: "JSON array [pitch,yaw,roll]"},
			{Name: "scale", Type: "json", Help: "JSON array [x,y,z]"},
			{Name: "name", Type: "string", Help: "Actor name"},
		},
	},
	{
		Name:  "spawn_actor_from_class",
		Group: "editor",
		Short: "Spawn an actor from a class name",
		Params: []ParamSpec{
			{Name: "class_name", Type: "string", Required: true, Help: "Actor class name"},
			{Name: "location_x", Type: "float", Default: 0.0, Help: "X location"},
			{Name: "location_y", Type: "float", Default: 0.0, Help: "Y location"},
			{Name: "location_z", Type: "float", Default: 0.0, Help: "Z location"},
			{Name: "rotation_yaw", Type: "float", Default: 0.0, Help: "Yaw rotation"},
			{Name: "rotation_pitch", Type: "float", Default: 0.0, Help: "Pitch rotation"},
			{Name: "rotation_roll", Type: "float", Default: 0.0, Help: "Roll rotation"},
		},
	},
	{
		Name:  "get_selected_actors",
		Group: "editor",
		Short: "Get currently selected viewport actors",
	},
	{
		Name:  "get_world_info",
		Group: "editor",
		Short: "Get current level info (world name, actor count)",
	},
	{
		Name:  "get_actor_properties",
		Group: "editor",
		Short: "Get all properties from a placed actor instance",
		Params: []ParamSpec{
			{Name: "actor_label", Type: "string", Required: true, Help: "Actor label/name"},
			{Name: "filter", Type: "string", Help: "Filter property names"},
			{Name: "include_components", Type: "bool", Default: false, Help: "Include component properties"},
		},
	},
	{
		Name:  "set_static_mesh_properties",
		Group: "editor",
		Short: "Set static mesh on a BP component",
		Params: []ParamSpec{
			{Name: "blueprint_name", Type: "string", Required: true, Help: "Blueprint name"},
			{Name: "component_name", Type: "string", Required: true, Help: "Component name"},
			{Name: "static_mesh", Type: "string", Default: "/Engine/BasicShapes/Cube.Cube", Help: "Mesh path"},
		},
	},
	{
		Name:  "set_physics_properties",
		Group: "editor",
		Short: "Set physics on a BP component",
		Params: []ParamSpec{
			{Name: "blueprint_name", Type: "string", Required: true, Help: "Blueprint name"},
			{Name: "component_name", Type: "string", Required: true, Help: "Component name"},
			{Name: "simulate_physics", Type: "bool", Default: true, Help: "Simulate physics"},
			{Name: "gravity_enabled", Type: "bool", Default: true, Help: "Enable gravity"},
			{Name: "mass", Type: "float", Default: 1.0, Help: "Mass in kg"},
			{Name: "linear_damping", Type: "float", Default: 0.01, Help: "Linear damping"},
			{Name: "angular_damping", Type: "float", Default: 0.0, Help: "Angular damping"},
		},
	},
	{
		Name:  "set_mesh_material_color",
		Group: "editor",
		Short: "Set material color on a mesh component",
		Params: []ParamSpec{
			{Name: "blueprint_name", Type: "string", Required: true, Help: "Blueprint name"},
			{Name: "component_name", Type: "string", Required: true, Help: "Component name"},
			{Name: "color", Type: "json", Required: true, Help: "JSON array [R,G,B,A]"},
			{Name: "material_path", Type: "string", Default: "/Engine/BasicShapes/BasicShapeMaterial", Help: "Material path"},
			{Name: "parameter_name", Type: "string", Default: "BaseColor", Help: "Material parameter name"},
			{Name: "material_slot", Type: "int", Default: 0, Help: "Material slot index"},
		},
	},
	{
		Name:    "get_available_materials",
		Group:   "editor",
		Short:   "List available materials",
		LargeOp: true,
		Params: []ParamSpec{
			{Name: "search_path", Type: "string", Default: "/Game/", Help: "Search path"},
			{Name: "include_engine_materials", Type: "bool", Default: true, Help: "Include engine materials"},
		},
	},
	{
		Name:  "apply_material_to_actor",
		Group: "editor",
		Short: "Apply material to a level actor",
		Params: []ParamSpec{
			{Name: "actor_name", Type: "string", Required: true, Help: "Actor name"},
			{Name: "material_path", Type: "string", Required: true, Help: "Material asset path"},
			{Name: "material_slot", Type: "int", Default: 0, Help: "Material slot index"},
		},
	},
	{
		Name:  "apply_material_to_blueprint",
		Group: "editor",
		Short: "Apply material to a BP component",
		Params: []ParamSpec{
			{Name: "blueprint_name", Type: "string", Required: true, Help: "Blueprint name"},
			{Name: "component_name", Type: "string", Required: true, Help: "Component name"},
			{Name: "material_path", Type: "string", Required: true, Help: "Material asset path"},
			{Name: "material_slot", Type: "int", Default: 0, Help: "Material slot index"},
		},
	},
	{
		Name:  "get_actor_material_info",
		Group: "editor",
		Short: "Get material info for a level actor",
		Params: []ParamSpec{
			{Name: "actor_name", Type: "string", Required: true, Help: "Actor name"},
		},
	},
	{
		Name:  "get_blueprint_material_info",
		Group: "editor",
		Short: "Get material info for BP components",
		Params: []ParamSpec{
			{Name: "blueprint_name", Type: "string", Required: true, Help: "Blueprint name"},
		},
	},
	{
		Name:  "take_screenshot",
		Group: "editor",
		Short: "Capture viewport or editor window to PNG",
		Params: []ParamSpec{
			{Name: "file_path", Type: "string", Help: "Output file path (default: auto)"},
			{Name: "mode", Type: "string", Default: "viewport", Help: "viewport or window"},
		},
	},
}
