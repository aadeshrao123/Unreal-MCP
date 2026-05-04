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
		Long:  "Returns every actor currently placed in the active level, including their names, types, and transforms. Use this to get an overview of level contents before performing targeted operations.",
		Example: `  umcp get_actors_in_level`,
	},
	{
		Name:  "find_actors_by_name",
		Group: "editor",
		Short: "Find actors by name pattern",
		Long:  "Searches the current level for actors whose label matches the given pattern. Supports partial matching, so you can find groups of related actors without listing every actor in the level.",
		Example: `  umcp find_actors_by_name --pattern "Light"
  umcp find_actors_by_name --pattern "BP_Conveyor"`,
		Params: []ParamSpec{
			{Name: "pattern", Type: "string", Required: true, Help: "Name pattern to search"},
		},
	},
	{
		Name:  "spawn_actor",
		Group: "editor",
		Short: "Spawn an actor in the level",
		Long:  "Creates a new actor instance in the current level at the specified transform. The actor-type defaults to StaticMeshActor but can be any built-in actor class. Use spawn_blueprint_actor for Blueprint-based actors instead.",
		Example: `  umcp spawn_actor --name "MyCube" --actor-type "StaticMeshActor" --location "[0,0,100]" --rotation "[0,45,0]" --scale "[2,2,2]"
  umcp spawn_actor --name "FloorPlane" --static-mesh "/Engine/BasicShapes/Plane.Plane" --location "[0,0,0]"`,
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
		Long:  "Removes a placed actor from the current level by its label name. The deletion is immediate and cannot be undone from the CLI, so verify the target with find_actors_by_name first if unsure.",
		Example: `  umcp delete_actor --name "MyCube"`,
		Params: []ParamSpec{
			{Name: "name", Type: "string", Required: true, Help: "Actor name"},
		},
	},
	{
		Name:  "set_actor_transform",
		Group: "editor",
		Short: "Set actor location/rotation/scale",
		Long:  "Updates the world transform of an existing actor in the level. You can set any combination of location, rotation, and scale -- omitted fields are left unchanged.",
		Example: `  umcp set_actor_transform --name "MyCube" --location "[100,200,50]"
  umcp set_actor_transform --name "MyCube" --rotation "[0,90,0]" --scale "[1,1,2]"`,
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
		Long:  "Spawns an instance of a Blueprint asset into the current level. Use this instead of spawn_actor when you need to place a Blueprint-based actor with its full component hierarchy and default properties.",
		Example: `  umcp spawn_blueprint_actor --blueprint-path "/Game/Blueprints/BP_Miner" --location "[500,0,0]"
  umcp spawn_blueprint_actor --blueprint-path "/Game/Blueprints/BP_ConveyorBelt" --location "[0,0,0]" --rotation "[0,90,0]" --name "Belt_01"`,
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
		Long:  "Spawns an actor using a C++ or Blueprint class name with individual float parameters for location and rotation instead of JSON arrays. Useful when you know the exact class name and want to specify coordinates directly.",
		Example: `  umcp spawn_actor_from_class --class-name "PointLight" --location-x 100 --location-y 200 --location-z 300
  umcp spawn_actor_from_class --class-name "AMyCustomActor" --location-z 50 --rotation-yaw 45`,
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
		Name:    "get_selected_actors",
		Group:   "editor",
		Short:   "Get currently selected viewport actors",
		Long:    "Returns the list of actors currently selected in the Unreal Editor viewport. Useful for inspecting or modifying whatever the user has selected in the editor.",
		Example: `  umcp get_selected_actors`,
	},
	{
		Name:    "get_world_info",
		Group:   "editor",
		Short:   "Get current level info (world name, actor count)",
		Long:    "Returns metadata about the currently loaded world, including the level name, total actor count, and other summary information. Use this to confirm which level is active before performing operations.",
		Example: `  umcp get_world_info`,
	},
	{
		Name:  "get_actor_properties",
		Group: "editor",
		Short: "Get all properties from a placed actor instance",
		Long:  "Reads the editable properties from an actor placed in the level. Use the filter parameter to narrow results to specific property names. Enable include-components to also list properties on the actor's components.",
		Example: `  umcp get_actor_properties --actor-label "BP_Miner_01"
  umcp get_actor_properties --actor-label "BP_Miner_01" --filter "Power" --include-components true`,
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
		Long:  "Assigns a static mesh asset to a mesh component within a Blueprint. Use this to change the visual geometry of a Blueprint component without opening the Blueprint editor.",
		Example: `  umcp set_static_mesh_properties --blueprint-name "BP_Miner" --component-name "MeshComp" --static-mesh "/Game/Meshes/SM_Miner.SM_Miner"
  umcp set_static_mesh_properties --blueprint-name "BP_Crate" --component-name "BaseMesh" --static-mesh "/Engine/BasicShapes/Cube.Cube"`,
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
		Long:  "Configures physics simulation settings on a component within a Blueprint. Controls whether the component simulates physics, responds to gravity, and sets mass and damping values.",
		Example: `  umcp set_physics_properties --blueprint-name "BP_Crate" --component-name "MeshComp" --simulate-physics true --mass 50.0
  umcp set_physics_properties --blueprint-name "BP_Crate" --component-name "MeshComp" --gravity-enabled false --linear-damping 0.5`,
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
		Long:  "Creates a dynamic material instance on a mesh component and sets a color vector parameter. The color is a JSON array of [R,G,B,A] floats (0.0-1.0). The material-slot parameter is the zero-based index of the material slot on the mesh to modify.",
		Example: `  umcp set_mesh_material_color --blueprint-name "BP_Crate" --component-name "MeshComp" --color "[1,0,0,1]"
  umcp set_mesh_material_color --blueprint-name "BP_Belt" --component-name "BeltMesh" --color "[0.2,0.2,0.8,1]" --material-slot 1 --parameter-name "TintColor"`,
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
		Name:     "get_available_materials",
		Group:    "editor",
		Short:    "List available materials",
		Long:     "Searches the Content Browser for material assets within the given search path. Returns material names and paths. This can be a large operation if searching broadly, so narrow the search-path when possible.",
		LargeOp:  true,
		Example:  `  umcp get_available_materials --search-path "/Game/Materials/"
  umcp get_available_materials --search-path "/Game/" --include-engine-materials false`,
		Params: []ParamSpec{
			{Name: "search_path", Type: "string", Default: "/Game/", Help: "Search path"},
			{Name: "include_engine_materials", Type: "bool", Default: true, Help: "Include engine materials"},
		},
	},
	{
		Name:  "apply_material_to_actor",
		Group: "editor",
		Short: "Apply material to a level actor",
		Long:  "Assigns a material asset to a placed actor's mesh component in the level. The material-slot is the zero-based index of the material slot on the mesh -- use get_actor_material_info to see which slots are available and what materials are currently assigned.",
		Example: `  umcp apply_material_to_actor --actor-name "Floor_01" --material-path "/Game/Materials/M_Concrete"
  umcp apply_material_to_actor --actor-name "Wall_03" --material-path "/Game/Materials/M_Brick" --material-slot 1`,
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
		Long:  "Assigns a material asset to a specific component within a Blueprint's defaults. The material-slot is the zero-based index of the material slot on the mesh component -- use get_blueprint_material_info to inspect available slots first.",
		Example: `  umcp apply_material_to_blueprint --blueprint-name "BP_Crate" --component-name "MeshComp" --material-path "/Game/Materials/M_Metal"
  umcp apply_material_to_blueprint --blueprint-name "BP_Belt" --component-name "BeltMesh" --material-path "/Game/Materials/M_Rubber" --material-slot 2`,
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
		Long:  "Returns material slot information for a placed actor in the level, including which materials are assigned to each slot and the slot indices. Use this before apply_material_to_actor to identify available slots.",
		Example: `  umcp get_actor_material_info --actor-name "Floor_01"`,
		Params: []ParamSpec{
			{Name: "actor_name", Type: "string", Required: true, Help: "Actor name"},
		},
	},
	{
		Name:  "get_blueprint_material_info",
		Group: "editor",
		Short: "Get material info for BP components",
		Long:  "Returns material slot information for all mesh components in a Blueprint, including current material assignments and slot indices. Use this before apply_material_to_blueprint to identify available slots and components.",
		Example: `  umcp get_blueprint_material_info --blueprint-name "BP_Crate"`,
		Params: []ParamSpec{
			{Name: "blueprint_name", Type: "string", Required: true, Help: "Blueprint name"},
		},
	},
	{
		Name:  "take_screenshot",
		Group: "editor",
		Short: "Capture viewport or editor window to PNG",
		Long:  "Captures a screenshot of the editor viewport or entire editor window and saves it as a PNG file. If no file path is specified, an auto-generated path is used. The resulting file can be viewed with the Read tool.",
		Example: `  umcp take_screenshot
  umcp take_screenshot --file-path "C:/Screenshots/level_overview.png" --mode "viewport"
  umcp take_screenshot --mode "window"`,
		Params: []ParamSpec{
			{Name: "file_path", Type: "string", Help: "Output file path (default: auto)"},
			{Name: "mode", Type: "string", Default: "viewport", Help: "viewport or window"},
		},
	},
	{
		Name:  "set_actor_property",
		Group: "editor",
		Short: "Set a property at any nested path on a placed actor",
		Long:  "Writes any property on the actual placed level actor (not the CDO/Blueprint defaults). Walks dotted paths through structs, FArrayProperty, FSetProperty and static C arrays via Field[N] syntax. Fires PostEditChangeProperty on the top-level FProperty so the renderer/component refresh, and marks the level dirty for save.",
		Example: `  umcp set_actor_property --actor-label "PostProcessVolume_1" --property-path "Settings.BloomIntensity" --property-value 2.0
  umcp set_actor_property --actor-label "PostProcessVolume_1" --property-path "Settings.bOverride_BloomIntensity" --property-value true
  umcp set_actor_property --actor-label "PostProcessVolume_1" --property-path "Settings.LensFlareTints[2]" --property-value '{"R":0,"G":1,"B":0,"A":1}'
  umcp set_actor_property --actor-label "MyMeshActor" --component-name "StaticMeshComponent" --property-path "Mobility" --property-value "Movable"`,
		Params: []ParamSpec{
			{Name: "actor_label", Type: "string", Required: true, Help: "Actor outliner label or UObject name"},
			{Name: "property_path", Type: "string", Required: true, Help: "Dotted property path (e.g. Settings.BloomIntensity)"},
			{Name: "property_value", Type: "json", Required: true, Help: "JSON-encoded value (number, bool, string, object, array)"},
			{Name: "component_name", Type: "string", Help: "Optional: anchor path to a named component"},
		},
	},
	{
		Name:  "get_actor_property_metadata",
		Group: "editor",
		Short: "Inspect property type/clamp/enum metadata on a placed actor",
		Long:  "Returns per-property metadata (cpp_type, ue_type, category, current_value, clamp_min/max, ui_min/max, valid_values for enums, inner/element/key/value types for containers, object_class/meta_class for refs, is_struct/is_array/is_enum/is_object/is_bool flags) PLUS a _summary header (total_available, total_returned, truncated, next_cursor, own_count, inherited_count, class_chain, categories breakdown). Object refs DO NOT auto-descend by default — use component_name to switch context, or pass descend_into_objects=true. max_entries=0 returns only the _summary so you can plan first.",
		Example: `  umcp get_actor_property_metadata --actor-label "PostProcessVolume_1" --property-path "Settings.AutoExposureMethod"
  umcp get_actor_property_metadata --actor-label "PostProcessVolume_1" --property-path "Settings" --filter "bloom" --depth 1
  umcp get_actor_property_metadata --actor-label "MCP_TestPointLight" --component-name "LightComponent0" --category "Light" --max-entries 20
  umcp get_actor_property_metadata --actor-label "MCP_TestPointLight" --max-entries 0
  umcp get_actor_property_metadata --actor-label "MCP_TestPointLight" --include-inherited false --component-name "LightComponent0"`,
		Params: []ParamSpec{
			{Name: "actor_label", Type: "string", Required: true, Help: "Actor outliner label or UObject name"},
			{Name: "property_path", Type: "string", Help: "Optional dotted path; empty = top-level"},
			{Name: "filter", Type: "string", Help: "Case-insensitive substring on full path"},
			{Name: "category", Type: "string", Help: "UPROPERTY(Category=X) exact match (case-insensitive)"},
			{Name: "depth", Type: "int", Default: "1", Help: "Struct nesting levels to expand"},
			{Name: "expand_enums", Type: "bool", Default: "true", Help: "Include valid_values for enum-typed fields"},
			{Name: "include_inherited", Type: "bool", Default: "true", Help: "Walk super class properties"},
			{Name: "descend_into_objects", Type: "bool", Default: "false", Help: "If path lands on object ref, enumerate its class"},
			{Name: "max_entries", Type: "int", Default: "50", Help: "Cap on emitted entries (0 = summary only)"},
			{Name: "cursor", Type: "int", Default: "0", Help: "Pagination offset (use _summary.next_cursor)"},
			{Name: "component_name", Type: "string", Help: "Optional: anchor to a named component"},
		},
	},
	{
		Name:  "spawn_actor_by_class",
		Group: "editor",
		Short: "Spawn ANY AActor subclass — engine, plugin, or Blueprint",
		Long:  "Resolves class_path as full UClass path (\"/Script/Engine.PostProcessVolume\"), Blueprint asset path (\"/Game/BP_Miner\"), or short name (\"PostProcessVolume\"). Validates child of AActor + not abstract + not deprecated. Uses UEditorActorSubsystem::SpawnActorFromClass when available so Ctrl+Z restores. Replaces the legacy whitelist of spawn_actor.",
		Example: `  umcp spawn_actor_by_class --class-path "/Script/Engine.PostProcessVolume" --name "GlobalPP" --location "[0,0,0]"
  umcp spawn_actor_by_class --class-path "/Script/Engine.SkyAtmosphere"
  umcp spawn_actor_by_class --class-path "/Game/Blueprints/BP_Miner.BP_Miner_C" --location "[500,0,100]" --rotation "[0,90,0]"`,
		Params: []ParamSpec{
			{Name: "class_path", Type: "string", Required: true, Help: "Full path, BP path, or short name"},
			{Name: "name", Type: "string", Help: "Optional outliner label"},
			{Name: "location", Type: "json", Help: "JSON [x,y,z]"},
			{Name: "rotation", Type: "json", Help: "JSON [pitch,yaw,roll]"},
			{Name: "scale", Type: "json", Help: "JSON [x,y,z]"},
		},
	},
	{
		Name:  "find_actors",
		Group: "editor",
		Short: "Flexible actor search by name, label, class, or tag",
		Long:  "Combine any number of filters (all AND'd together) to query the level. Useful to confirm an actor exists, list all of a class, or filter by tag. Returns total_scanned/total_matched even when truncated — so you know if you need to widen max_results.",
		Example: `  umcp find_actors --label-pattern "Light" --max-results 10
  umcp find_actors --class-filter "PointLight" --include-transform true
  umcp find_actors --class-filter "/Script/Engine.PostProcessVolume" --exact-class true
  umcp find_actors --tag "MCPModified"
  umcp find_actors --name-pattern "Conveyor" --class-filter "ConveyorBeltActor" --max-results 5`,
		Params: []ParamSpec{
			{Name: "name_pattern", Type: "string", Help: "Case-insensitive substring on UObject name"},
			{Name: "label_pattern", Type: "string", Help: "Case-insensitive substring on outliner label"},
			{Name: "class_filter", Type: "string", Help: "Class name (short or full path)"},
			{Name: "tag", Type: "string", Help: "Match actors with this Tag"},
			{Name: "exact_class", Type: "bool", Default: "false", Help: "Require exact class match (else IsA)"},
			{Name: "max_results", Type: "int", Default: "100", Help: "Result cap (0 = unlimited)"},
			{Name: "include_transform", Type: "bool", Default: "false", Help: "Include location/rotation/scale"},
		},
	},
	{
		Name:  "get_actor_properties",
		Group: "editor",
		Short: "Get property values from a live placed actor instance",
		Long:  "Reads the placed instance (not CDO). flat=false returns nested JSON with top-level filter; flat=true returns dotted-path dict with path-aware filter, depth limit, and optional per-property metadata — use this for searching deep into structs like FPostProcessSettings.",
		Example: `  umcp get_actor_properties --actor-label "PostProcessVolume_1" --filter "bloom"
  umcp get_actor_properties --actor-label "PostProcessVolume_1" --flat true --filter "bloom" --max-depth 4
  umcp get_actor_properties --actor-label "MyActor" --include-components true --flat true --include-metadata true`,
		Params: []ParamSpec{
			{Name: "actor_label", Type: "string", Required: true, Help: "Actor label or UObject name"},
			{Name: "filter", Type: "string", Help: "Substring filter (top-level for nested, full path for flat)"},
			{Name: "include_components", Type: "bool", Default: "false", Help: "Also return component properties"},
			{Name: "flat", Type: "bool", Default: "false", Help: "Flat dotted-path dict mode"},
			{Name: "max_depth", Type: "int", Default: "3", Help: "(flat) max struct nesting"},
			{Name: "include_metadata", Type: "bool", Default: "false", Help: "(flat) emit metadata blob per leaf"},
			{Name: "expand_arrays", Type: "bool", Default: "true", Help: "(flat) emit Field[N] entries"},
			{Name: "array_element_limit", Type: "int", Default: "16", Help: "(flat) max array elements"},
			{Name: "category", Type: "string", Help: "(flat) UPROPERTY(Category=X) exact match"},
			{Name: "include_inherited", Type: "bool", Default: "true", Help: "(flat) walk super classes"},
			{Name: "max_entries", Type: "int", Default: "200", Help: "(flat) cap on emitted entries"},
			{Name: "cursor", Type: "int", Default: "0", Help: "(flat) pagination offset"},
		},
	},
}
