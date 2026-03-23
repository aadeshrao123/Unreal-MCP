"""Material tools — create, inspect, and build complex material graphs.

All operations go through C++ FEpicUnrealMCPMaterialCommands via the TCP bridge.
"""

import json
from typing import Optional

from unrealmcp._bridge import mcp
from unrealmcp._tcp_bridge import _call


@mcp.tool()
def create_material(
    name: str,
    path: str = "/Game/Materials",
    blend_mode: str = "opaque",
    shading_model: str = "default_lit",
    two_sided: bool = False,
    opacity_mask_clip_value: Optional[float] = None,
    force: bool = False,
) -> str:
    """Create a new Material asset.

    Returns error if the asset already exists (avoids editor overwrite popup).
    Pass force=true to delete and recreate.

    Args:
        name: Material name (e.g. "M_MyMaterial")
        path: Content Browser path
        blend_mode: opaque | masked | translucent | additive | modulate | alpha_composite | alpha_holdout
        shading_model: default_lit | unlit | subsurface | clear_coat | subsurface_profile |
            two_sided_foliage | cloth | eye | thin_translucent
        two_sided: Render on both sides
        opacity_mask_clip_value: Clip value for masked blend mode (0.0-1.0)
        force: If true, delete existing asset and recreate (default false)
    """
    params = {
        "name": name,
        "path": path,
        "blend_mode": blend_mode,
        "shading_model": shading_model,
        "two_sided": two_sided,
    }
    if opacity_mask_clip_value is not None:
        params["opacity_mask_clip_value"] = opacity_mask_clip_value
    if force:
        params["force"] = True
    return _call("create_material", params)


@mcp.tool()
def create_material_instance(
    parent_path: str,
    name: str,
    path: str = "/Game/Materials",
    scalar_params: Optional[str] = None,
    vector_params: Optional[str] = None,
    texture_params: Optional[str] = None,
    force: bool = False,
) -> str:
    """Create a Material Instance from a parent material.

    Returns error if asset already exists. Pass force=true to delete and recreate.

    Args:
        parent_path: Full path to parent (e.g. "/Game/Materials/M_Base")
        name: Instance name (e.g. "MI_Red")
        scalar_params: JSON dict of scalar overrides — {"Opacity": 0.5, "Metallic": 1.0}
        vector_params: JSON dict of vector overrides — {"BaseColor": [1.0, 0.0, 0.0, 1.0]}
        texture_params: JSON dict of texture overrides — {"Texture": "/Game/Textures/T_Wood"}
        force: If true, delete existing asset and recreate (default false)
    """
    params = {
        "parent_path": parent_path,
        "name": name,
        "path": path,
    }
    if scalar_params is not None:
        params["scalar_params"] = json.loads(scalar_params)
    if vector_params is not None:
        params["vector_params"] = json.loads(vector_params)
    if texture_params is not None:
        params["texture_params"] = json.loads(texture_params)
    if force:
        params["force"] = True
    return _call("create_material_instance", params)


@mcp.tool()
def build_material_graph(
    material_path: str,
    nodes: str,
    connections: str,
    clear_existing: bool = True,
) -> str:
    """Build a complete material node graph in one atomic operation.

    Creates expression nodes and wires them together in a single call.
    By default clears existing expressions first (safe rebuild — all external
    references stay intact).

    Args:
        material_path: Full path to existing material (e.g. "/Game/Materials/M_Portal")
        nodes: JSON array of node definitions. Each node has:
            - type: Short class name (e.g. "TextureCoordinate", "Custom", "ScalarParameter",
                "Constant3Vector", "TextureSample", "Multiply", "Add", "Panner", "Time",
                "VectorParameter", "LinearInterpolate", "ComponentMask")
                Auto-prefixed with "MaterialExpression" if needed.
            - pos_x, pos_y: Graph position (default -300, 0)
            - properties: Dict of editor properties to set. Common ones:
                - parameter_name: str — name shown in material instances
                - default_value: number or [r,g,b,a]
                - slider_min, slider_max: float — slider range
                - group: str — parameter group name
                - Values starting with "/" are loaded as assets
                - Lists of 3-4 become LinearColor, lists of 2 become Vector2D
            For Custom HLSL nodes (type="Custom"), also supports:
            - code: HLSL source code string
            - description: Node title in the graph
            - output_type: "float" | "float2" | "float3" | "float4"
            - inputs: List of input pin names (e.g. ["UV", "Speed"])
            - outputs: List of {"name": str, "type": str} for additional outputs
        connections: JSON array of connections. Each connection has:
            - from_node: Source node index (int, 0-based into nodes array)
            - from_pin: Output pin name ("" for default output)
            - to_node: Target node index (int) or "material" for material output
            - to_pin: Input pin name, or material property when to_node="material":
                BaseColor, Metallic, Specular, Roughness, Anisotropy, EmissiveColor,
                Opacity, OpacityMask, Normal, Tangent, WorldPositionOffset, Displacement,
                SubsurfaceColor, CustomData0, CustomData1, AmbientOcclusion, Refraction,
                PixelDepthOffset, ShadingModel, SurfaceThickness, FrontMaterial
                (FrontMaterial is for Substrate BSDF nodes — use get_available_material_pins
                to see which pins are active for a specific material)
        clear_existing: Remove existing expressions before building (default True)
    """
    return _call("build_material_graph", {
        "material_path": material_path,
        "nodes": json.loads(nodes),
        "connections": json.loads(connections),
        "clear_existing": clear_existing,
    })


@mcp.tool()
def get_material_info(
    material_path: str,
    include: Optional[str] = None,
) -> str:
    """Inspect a material's properties.

    Always returns: blend mode, shading model, two-sided, expression count.

    Use include (comma-separated) to request additional sections:
    - "parameters": scalar/vector/texture parameter names
    - "textures": used texture paths
    - "statistics": shader instruction counts

    Omit include to get only basic properties (minimal tokens).
    Pass include="parameters,textures,statistics" for everything.
    """
    params: dict = {"material_path": material_path}
    if include is not None:
        params["include"] = include
    return _call("get_material_info", params)


@mcp.tool()
def recompile_material(material_path: str) -> str:
    """Force recompile and save a material. Reports success/failure."""
    return _call("recompile_material", {"material_path": material_path})


@mcp.tool()
def get_material_errors(material_path: str, recompile: bool = True) -> str:
    """Get shader compilation errors for a material.

    Returns error messages and the node indices that caused them.
    Recompiles first by default to get fresh errors.
    """
    return _call("get_material_errors", {
        "material_path": material_path,
        "recompile": recompile,
    })


@mcp.tool()
def set_material_properties(
    material_path: str,
    blend_mode: Optional[str] = None,
    shading_model: Optional[str] = None,
    two_sided: Optional[bool] = None,
    opacity_mask_clip_value: Optional[float] = None,
    dithered_lof_transition: Optional[bool] = None,
    allow_negative_emissive_color: Optional[bool] = None,
    recompile: bool = True,
) -> str:
    """Bulk-set material-level properties in one call.

    Args:
        blend_mode: opaque | masked | translucent | additive | modulate | alpha_composite | alpha_holdout
        shading_model: default_lit | unlit | subsurface | clear_coat | etc.
        recompile: Recompile after setting properties (default True)
    """
    params: dict = {
        "material_path": material_path,
        "recompile": recompile,
    }
    for key, val in [
        ("blend_mode", blend_mode), ("shading_model", shading_model),
        ("two_sided", two_sided), ("opacity_mask_clip_value", opacity_mask_clip_value),
        ("dithered_lof_transition", dithered_lof_transition),
        ("allow_negative_emissive_color", allow_negative_emissive_color),
    ]:
        if val is not None:
            params[key] = val
    return _call("set_material_properties", params)


@mcp.tool()
def get_material_graph_nodes(
    material_path: str,
    verbosity: str = "connections",
    type_filter: Optional[str] = None,
) -> str:
    """Read expression nodes in a material graph.

    Use verbosity to control response size:
    - "summary": index, type, position only (~30 tokens/node)
    - "connections": + input connections (default, good balance)
    - "full": + properties + available pins (large response)

    Use type_filter to only return nodes matching a type substring
    (e.g. "Parameter" returns only parameter nodes).
    """
    params: dict = {
        "material_path": material_path,
        "verbosity": verbosity,
    }
    if type_filter is not None:
        params["type_filter"] = type_filter
    return _call("get_material_graph_nodes", params)


@mcp.tool()
def add_material_expression(material_path: str, node: str) -> str:
    """Add a single expression node without clearing other nodes.

    Returns the new node_index. Wire it up with connect_material_expressions.

    node is a JSON object: {"type": "ScalarParameter", "pos_x": -1200, "pos_y": -400,
    "properties": {"parameter_name": "Speed", "default_value": 0.5}}

    For Custom HLSL nodes, include top-level "code", "description", "output_type",
    "inputs", and "outputs" fields.
    """
    return _call("add_material_expression", {
        "material_path": material_path,
        "node": json.loads(node),
    })


@mcp.tool()
def connect_material_expressions(
    material_path: str,
    from_node: int,
    to_node: str,
    to_pin: str,
    from_pin: str = "",
) -> str:
    """Connect two expression nodes using their indices.

    Args:
        from_node: Source node index
        to_node: Target node index (e.g. "5") or "material" for material output
        to_pin: Input pin name, or material property when to_node="material":
            BaseColor, Metallic, Specular, Roughness, Anisotropy, EmissiveColor,
            Opacity, OpacityMask, Normal, Tangent, WorldPositionOffset, Displacement,
            SubsurfaceColor, CustomData0, CustomData1, AmbientOcclusion, Refraction,
            PixelDepthOffset, ShadingModel, SurfaceThickness, FrontMaterial
        from_pin: Output pin name ("" = primary output)
    """
    return _call("connect_material_expressions", {
        "material_path": material_path,
        "from_node": from_node,
        "from_pin": from_pin,
        "to_node": to_node,
        "to_pin": to_pin,
    })


@mcp.tool()
def delete_material_expression(material_path: str, node_index: int) -> str:
    """Delete a single expression node by index. Recompiles and saves automatically.

    Remaining node indices may shift — re-query with get_material_graph_nodes afterwards.
    """
    return _call("delete_material_expression", {
        "material_path": material_path,
        "node_index": node_index,
    })


@mcp.tool()
def add_material_comments(material_path: str, comments: str) -> str:
    """Add comment boxes to a material graph for organization.

    comments is a JSON array. Each entry: {text, pos_x, pos_y, size_x, size_y,
    font_size, color: [r,g,b], show_bubble, color_bubble, group_mode}.
    """
    return _call("add_material_comments", {
        "material_path": material_path,
        "comments": json.loads(comments),
    })


@mcp.tool()
def get_material_expression_info(material_path: str, node_index: int) -> str:
    """Get detailed info for a single node, including all available input/output pins.

    Use this before connecting nodes to discover exact pin names.
    """
    return _call("get_material_expression_info", {
        "material_path": material_path,
        "node_index": node_index,
    })


@mcp.tool()
def get_material_property_connections(material_path: str) -> str:
    """Query which expression node feeds each material output slot.

    Only lists slots that have something connected.
    """
    return _call("get_material_property_connections", {"material_path": material_path})


@mcp.tool()
def set_material_expression_property(
    material_path: str,
    node_index: int,
    property_name: str,
    property_value: str,
) -> str:
    """Set a property on an existing material expression node.

    Accepts snake_case ("parameter_name") or PascalCase ("ParameterName").

    Common properties: parameter_name, default_value, slider_min, slider_max,
    group, texture, code, description, output_type, inputs, add_inputs, outputs,
    SpeedX, SpeedY, CoordinateIndex, R, Constant.
    """
    return _call("set_material_expression_property", {
        "material_path": material_path,
        "node_index": node_index,
        "property_name": property_name,
        "property_value": json.loads(property_value),
    })


@mcp.tool()
def move_material_expression(material_path: str, node_index: int, pos_x: int, pos_y: int) -> str:
    """Move a material expression node to a new graph position."""
    return _call("move_material_expression", {
        "material_path": material_path,
        "node_index": node_index,
        "pos_x": pos_x,
        "pos_y": pos_y,
    })


@mcp.tool()
def duplicate_material_expression(
    material_path: str,
    node_index: int,
    offset_x: int = 0,
    offset_y: int = 150,
) -> str:
    """Duplicate a node (same type and properties, offset from original).

    Returns the new node_index. Connections are NOT copied.
    """
    return _call("duplicate_material_expression", {
        "material_path": material_path,
        "node_index": node_index,
        "offset_x": offset_x,
        "offset_y": offset_y,
    })


@mcp.tool()
def layout_material_expressions(material_path: str) -> str:
    """Auto-layout all nodes using Unreal's built-in arrangement algorithm."""
    return _call("layout_material_expressions", {"material_path": material_path})


@mcp.tool()
def get_material_instance_parameters(material_path: str) -> str:
    """Get all overridable parameters from a Material Instance (scalar, vector, texture, switch)."""
    return _call("get_material_instance_parameters", {"material_path": material_path})


@mcp.tool()
def set_material_instance_parameter(
    material_path: str,
    param_name: str,
    param_type: str,
    value: str,
) -> str:
    """Set a parameter override on a Material Instance.

    Args:
        param_type: scalar | vector | texture | static_switch
        value: JSON-encoded — float, [r,g,b,a], "/Game/path", or true/false
    """
    return _call("set_material_instance_parameter", {
        "material_path": material_path,
        "param_name": param_name,
        "param_type": param_type,
        "value": json.loads(value),
    })


@mcp.tool()
def list_material_expression_types(
    filter: Optional[str] = None,
    max_results: int = 0,
    include_details: bool = True,
) -> str:
    """List available material expression node types.

    Use filter to narrow results (e.g. "texture", "parameter", "math", "noise").
    Set max_results to limit output (0 = unlimited).
    Set include_details=false for compact output (type names only).
    """
    params: dict = {}
    if filter is not None:
        params["filter"] = filter
    if max_results > 0:
        params["max_results"] = max_results
    if not include_details:
        params["include_details"] = False
    return _call("list_material_expression_types", params)


@mcp.tool()
def get_expression_type_info(
    type_name: str,
    function_path: Optional[str] = None,
) -> str:
    """Look up pin names and editable properties for a node type WITHOUT creating one.

    Returns input pins, output pins, and all editable properties with types and defaults.
    Use this BEFORE creating nodes to know exact pin names and avoid connection errors.

    Special node types:
    - Custom: Returns additional custom_hlsl_schema with code/inputs/outputs documentation.
    - MaterialFunctionCall: Pass function_path to see the actual inputs/outputs of
        that function. Without it, pins will be empty since they depend on which
        function is loaded. Use search_material_functions to find functions first.
    - Substrate nodes (SubstrateSlabBSDF, SubstrateShadingModels, etc.): Returns
        substrate_note explaining how to connect to FrontMaterial pin.

    Args:
        type_name: Short type name (e.g. "Multiply", "TextureSample", "Custom",
            "MaterialFunctionCall", "SubstrateSlabBSDF", "SubstrateShadingModels")
        function_path: For MaterialFunctionCall only — path to the material function
            to load (e.g. "/Engine/Functions/Engine_MaterialFunctions03/Blends/Blend_Overlay").
            Populates the node with the function's actual input/output pins.
    """
    params: dict = {"type_name": type_name}
    if function_path is not None:
        params["function_path"] = function_path
    return _call("get_expression_type_info", params)


@mcp.tool()
def get_available_material_pins(material_path: str) -> str:
    """Query all available material output pins for a specific material.

    Returns every pin that is currently visible (based on material settings),
    whether it's connected, what type it expects, and which node feeds it.

    Use this to discover valid to_pin values for connect_material_expressions
    with to_node="material". The available pins change based on blend mode,
    shading model, and whether Substrate is enabled.

    Also reports substrate_enabled and has_front_material_connected status.
    """
    return _call("get_available_material_pins", {"material_path": material_path})


@mcp.tool()
def disconnect_material_expression(
    material_path: str,
    node_index: int,
    input_pin: str,
) -> str:
    """Disconnect a specific input pin on a material expression node.

    Args:
        node_index: The target node whose input pin will be disconnected
        input_pin: Name of the input pin to disconnect
    """
    return _call("disconnect_material_expression", {
        "material_path": material_path,
        "node_index": node_index,
        "input_pin": input_pin,
    })


@mcp.tool()
def search_material_functions(
    filter: Optional[str] = None,
    path: str = "/Game",
    max_results: int = 50,
    include_engine: bool = False,
) -> str:
    """Search for Material Functions by name.

    Args:
        filter: Name substring to match (e.g. "Blend", "Normal", "Fresnel")
        path: Content Browser path to search in (default "/Game")
        max_results: Maximum results to return (default 50)
        include_engine: Include engine built-in material functions
    """
    params: dict = {
        "path": path,
        "max_results": max_results,
        "include_engine": include_engine,
    }
    if filter is not None:
        params["filter"] = filter
    return _call("search_material_functions", params)


@mcp.tool()
def validate_material_graph(material_path: str) -> str:
    """Diagnose connection issues in a material graph.

    Categorises every node into:
    - orphaned: no inputs AND output not consumed — safe to delete
    - dead_ends: has inputs but output goes nowhere
    - missing_inputs: some input pins are empty
    - unconnected_inputs: ALL input pins are empty (node expects connections)
    - unconnected_outputs: output not consumed by anything

    Returns healthy=true when no orphaned or dead-end nodes exist.
    """
    return _call("validate_material_graph", {"material_path": material_path})


@mcp.tool()
def trace_material_connection(
    material_path: str,
    node_index: int,
    direction: str = "both",
    max_depth: int = 1,
) -> str:
    """Trace connections upstream and/or downstream from a specific node.

    Shows exactly what feeds into each input pin and what consumes each output pin,
    including connections to material outputs (BaseColor, Normal, etc.).

    Args:
        node_index: The node to trace from
        direction: "upstream" (what feeds in), "downstream" (what consumes output),
            or "both" (default)
        max_depth: How many hops to trace recursively (1 = immediate neighbors,
            2+ = follow the chain). Max 50. Default 1.
    """
    return _call("trace_material_connection", {
        "material_path": material_path,
        "node_index": node_index,
        "direction": direction,
        "max_depth": max_depth,
    })


@mcp.tool()
def cleanup_material_graph(
    material_path: str,
    mode: str = "orphaned",
    dry_run: bool = False,
) -> str:
    """Delete orphaned and/or dead-end nodes from a material graph.

    WARNING: NEVER call this tool unless the user explicitly asks to clean up
    or delete unconnected nodes. Users may intentionally keep disconnected nodes
    as references, scratchpads, or work-in-progress. Deleting them without
    permission will cause data loss and frustration.

    Args:
        mode: What to delete:
            "orphaned" — only nodes with NO connections at all (safest)
            "dead_ends" — nodes whose output goes nowhere (has inputs but no consumers)
            "all" — both orphaned and dead-ends
        dry_run: If true, reports what WOULD be deleted without actually deleting.
            Always use dry_run=true first to preview before deleting.
    """
    return _call("cleanup_material_graph", {
        "material_path": material_path,
        "mode": mode,
        "dry_run": dry_run,
    })


# ---------------------------------------------------------------------------
# Material Function tools
# ---------------------------------------------------------------------------


@mcp.tool()
def create_material_function(
    name: str,
    path: str = "/Game/Materials/Functions",
    description: str = "",
    expose_to_library: bool = True,
    force: bool = False,
) -> str:
    """Create a new Material Function asset.

    Returns error if asset already exists. Pass force=true to delete and recreate.

    Args:
        name: Function name (e.g. "MF_CustomBlend")
        path: Content Browser path
        description: Function description shown in tooltips
        expose_to_library: Whether the function appears in the material editor's function library
        force: If true, delete existing asset and recreate (default false)
    """
    params: dict = {
        "name": name,
        "path": path,
        "expose_to_library": expose_to_library,
    }
    if description:
        params["description"] = description
    if force:
        params["force"] = True
    return _call("create_material_function", params)


@mcp.tool()
def get_material_function_info(function_path: str) -> str:
    """Inspect a Material Function: inputs, outputs, internal nodes.

    Returns all FunctionInput pins (name, type, default value, sort priority),
    all FunctionOutput pins (name, connections), and a summary of internal nodes.
    """
    return _call("get_material_function_info", {"function_path": function_path})


@mcp.tool()
def build_material_function_graph(
    function_path: str,
    nodes: str,
    connections: str,
    clear_existing: bool = True,
) -> str:
    """Build the internal node graph of a Material Function in one atomic call.

    Same interface as build_material_graph but for Material Functions.

    FunctionInput nodes use type="FunctionInput" with extra fields:
        input_name, input_type (Scalar/Vector2/Vector3/Vector4/Texture2D/TextureCube/
        Texture2DArray/VolumeTexture/StaticBool/MaterialAttributes/TextureExternal/Bool/Substrate),
        description, sort_priority, use_preview_as_default, preview_value ([x,y,z,w])

    FunctionOutput nodes use type="FunctionOutput" with extra fields:
        output_name, description, sort_priority

    Connections between nodes use the same format as build_material_graph:
        from_node (int), from_pin (str), to_node (int), to_pin (str)

    To connect a node to a FunctionOutput's input, use to_pin="" (the output has
    a single unnamed input pin labeled "A" internally).

    Args:
        function_path: Full path to existing material function
        nodes: JSON array of node definitions
        connections: JSON array of connections
        clear_existing: Remove existing expressions before building (default True)
    """
    return _call("build_material_function_graph", {
        "function_path": function_path,
        "nodes": json.loads(nodes),
        "connections": json.loads(connections),
        "clear_existing": clear_existing,
    })


@mcp.tool()
def add_material_function_input(
    function_path: str,
    input_name: str,
    input_type: str = "Scalar",
    description: str = "",
    sort_priority: int = 0,
    use_preview_as_default: bool = False,
    preview_value: Optional[str] = None,
    pos_x: int = -600,
    pos_y: int = 0,
) -> str:
    """Add an input pin to a Material Function.

    Args:
        function_path: Full path to existing material function
        input_name: Pin name (e.g. "BaseColor", "Strength", "UV")
        input_type: Scalar | Vector2 | Vector3 | Vector4 | Texture2D | TextureCube |
            Texture2DArray | VolumeTexture | StaticBool | MaterialAttributes |
            TextureExternal | Bool | Substrate
        description: Tooltip text
        sort_priority: Controls display order (lower = higher)
        use_preview_as_default: Use preview_value as the default when input is unconnected
        preview_value: JSON array for default value (e.g. "[0.5]", "[1.0, 0.0, 0.0, 1.0]")
        pos_x: Graph X position (default -600)
        pos_y: Graph Y position (default 0)
    """
    params: dict = {
        "function_path": function_path,
        "input_name": input_name,
        "input_type": input_type,
        "pos_x": pos_x,
        "pos_y": pos_y,
    }
    if description:
        params["description"] = description
    if sort_priority != 0:
        params["sort_priority"] = sort_priority
    if use_preview_as_default:
        params["use_preview_as_default"] = True
    if preview_value is not None:
        params["preview_value"] = json.loads(preview_value)
    return _call("add_material_function_input", params)


@mcp.tool()
def add_material_function_output(
    function_path: str,
    output_name: str,
    description: str = "",
    sort_priority: int = 0,
    pos_x: int = 200,
    pos_y: int = 0,
) -> str:
    """Add an output pin to a Material Function.

    Args:
        function_path: Full path to existing material function
        output_name: Pin name (e.g. "Result", "Normal", "Mask")
        description: Tooltip text
        sort_priority: Controls display order (lower = higher)
        pos_x: Graph X position (default 200)
        pos_y: Graph Y position (default 0)
    """
    params: dict = {
        "function_path": function_path,
        "output_name": output_name,
        "pos_x": pos_x,
        "pos_y": pos_y,
    }
    if description:
        params["description"] = description
    if sort_priority != 0:
        params["sort_priority"] = sort_priority
    return _call("add_material_function_output", params)


@mcp.tool()
def set_material_function_input(
    function_path: str,
    node_index: int,
    input_name: Optional[str] = None,
    input_type: Optional[str] = None,
    description: Optional[str] = None,
    sort_priority: Optional[int] = None,
    use_preview_as_default: Optional[bool] = None,
    preview_value: Optional[str] = None,
) -> str:
    """Modify an existing FunctionInput node's properties.

    Args:
        function_path: Full path to existing material function
        node_index: Index of the FunctionInput node (from get_material_function_info)
        input_name: New pin name
        input_type: New type (Scalar/Vector2/Vector3/Vector4/Texture2D/TextureCube/
            Texture2DArray/VolumeTexture/StaticBool/MaterialAttributes/
            TextureExternal/Bool/Substrate)
        description: New tooltip
        sort_priority: New display order
        use_preview_as_default: Use preview value as default
        preview_value: JSON array for default value (e.g. "[0.5]", "[1.0, 0.0, 0.0, 1.0]")
    """
    params: dict = {
        "function_path": function_path,
        "node_index": node_index,
    }
    if input_name is not None:
        params["input_name"] = input_name
    if input_type is not None:
        params["input_type"] = input_type
    if description is not None:
        params["description"] = description
    if sort_priority is not None:
        params["sort_priority"] = sort_priority
    if use_preview_as_default is not None:
        params["use_preview_as_default"] = use_preview_as_default
    if preview_value is not None:
        params["preview_value"] = json.loads(preview_value)
    return _call("set_material_function_input", params)


@mcp.tool()
def set_material_function_output(
    function_path: str,
    node_index: int,
    output_name: Optional[str] = None,
    description: Optional[str] = None,
    sort_priority: Optional[int] = None,
) -> str:
    """Modify an existing FunctionOutput node's properties.

    Args:
        function_path: Full path to existing material function
        node_index: Index of the FunctionOutput node (from get_material_function_info)
        output_name: New pin name
        description: New tooltip
        sort_priority: New display order
    """
    params: dict = {
        "function_path": function_path,
        "node_index": node_index,
    }
    if output_name is not None:
        params["output_name"] = output_name
    if description is not None:
        params["description"] = description
    if sort_priority is not None:
        params["sort_priority"] = sort_priority
    return _call("set_material_function_output", params)


@mcp.tool()
def validate_material_function(function_path: str) -> str:
    """Diagnose issues in a Material Function's internal graph.

    Reports:
    - unconnected_outputs: FunctionOutput nodes with nothing wired to their input
    - unused_inputs: FunctionInput nodes whose output nobody consumes
    - orphaned: internal nodes with no connections at all

    Returns healthy=true when no issues found.
    """
    return _call("validate_material_function", {"function_path": function_path})


@mcp.tool()
def cleanup_material_function(
    function_path: str,
    dry_run: bool = False,
) -> str:
    """Remove unconnected/orphaned nodes from a Material Function.

    Deletes:
    - FunctionOutput nodes with no input wired (duplicate output pins)
    - FunctionInput nodes whose output nobody consumes (duplicate input pins)
    - Orphaned internal nodes

    WARNING: Only call when the user explicitly asks to clean up.

    Args:
        dry_run: If true, reports what would be deleted without deleting.
    """
    return _call("cleanup_material_function", {
        "function_path": function_path,
        "dry_run": dry_run,
    })
