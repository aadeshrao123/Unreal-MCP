"""Material tools — create, inspect, and build complex material graphs.

All operations are handled natively in C++ by FEpicUnrealMCPMaterialCommands
via the TCP bridge, bypassing the Python scripting plugin entirely.
"""

import json
from typing import Optional

from _bridge import mcp
from _tcp_bridge import _tcp_send_raw


def _call(command: str, params: dict) -> str:
    """Send a material command to the C++ bridge and return a JSON string."""
    resp = _tcp_send_raw(command, params)
    return json.dumps(resp, default=str, indent=2)


@mcp.tool()
def create_material(
    name: str,
    path: str = "/Game/Materials",
    blend_mode: str = "opaque",
    shading_model: str = "default_lit",
    two_sided: bool = False,
    opacity_mask_clip_value: Optional[float] = None,
) -> str:
    """Create a new Material asset.

    Args:
        name: Material name (e.g. "M_MyMaterial")
        path: Content Browser path
        blend_mode: opaque | masked | translucent | additive | modulate | alpha_composite | alpha_holdout
        shading_model: default_lit | unlit | subsurface | clear_coat | subsurface_profile |
            two_sided_foliage | cloth | eye | thin_translucent
        two_sided: Render on both sides
        opacity_mask_clip_value: Clip value for masked blend mode (0.0-1.0)
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
    return _call("create_material", params)


@mcp.tool()
def create_material_instance(
    parent_path: str,
    name: str,
    path: str = "/Game/Materials",
    scalar_params: Optional[str] = None,
    vector_params: Optional[str] = None,
    texture_params: Optional[str] = None,
) -> str:
    """Create a Material Instance from a parent material.

    Args:
        parent_path: Full path to parent material (e.g. "/Game/Materials/M_Base")
        name: Instance name (e.g. "MI_Red")
        path: Content Browser path for the new instance
        scalar_params: JSON dict of scalar overrides — {"Opacity": 0.5, "Metallic": 1.0}
        vector_params: JSON dict of vector overrides — {"BaseColor": [1.0, 0.0, 0.0, 1.0]}
        texture_params: JSON dict of texture overrides — {"Texture": "/Game/Textures/T_Wood"}
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
    Handles Custom HLSL nodes with named inputs, texture samples, parameters, etc.

    By default clears existing expressions first (safe rebuild without deleting the
    asset — all external references stay intact).

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
                - default_value: number or [r,g,b,a] — default parameter value
                - slider_min, slider_max: float — slider range in material instances
                - group: str — parameter group name for organization
                - desc: str — tooltip description
                - sort_priority: int — display order within group
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
            - to_pin: Input pin name, or material property name when to_node="material"
                (BaseColor, Metallic, Roughness, EmissiveColor, Opacity, OpacityMask,
                Normal, Specular, AmbientOcclusion, WorldPositionOffset, SubsurfaceColor,
                Refraction)
        clear_existing: Remove all existing expression nodes before building
            (default True). Keeps the asset and all references intact.
    """
    return _call("build_material_graph", {
        "material_path": material_path,
        "nodes": json.loads(nodes),
        "connections": json.loads(connections),
        "clear_existing": clear_existing,
    })


@mcp.tool()
def get_material_info(material_path: str) -> str:
    """Inspect a material: properties, expression count, parameter names, textures.

    Args:
        material_path: Full path to the material (e.g. "/Game/Materials/M_Portal")
    """
    return _call("get_material_info", {"material_path": material_path})


@mcp.tool()
def recompile_material(material_path: str) -> str:
    """Force recompile a material and save it. Reports success/failure.

    Args:
        material_path: Full path to the material (e.g. "/Game/Materials/M_Portal")
    """
    return _call("recompile_material", {"material_path": material_path})


@mcp.tool()
def get_material_errors(material_path: str, recompile: bool = True) -> str:
    """Get shader compilation errors for a material.

    Returns any shader compilation errors with error messages and the node
    indices that caused them. Use after modifying a material to verify it
    compiles cleanly.

    Args:
        material_path: Full path to the material (e.g. "/Game/Materials/M_Portal")
        recompile: Recompile the material first to get fresh errors (default True)
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
        material_path: Full path to the material
        blend_mode: opaque | masked | translucent | additive | modulate | alpha_composite | alpha_holdout
        shading_model: default_lit | unlit | subsurface | clear_coat | subsurface_profile |
            two_sided_foliage | cloth | eye | thin_translucent
        two_sided: Render both sides
        opacity_mask_clip_value: Clip threshold for masked mode (0.0-1.0)
        dithered_lof_transition: Enable dithered LOD transition
        allow_negative_emissive_color: Allow negative emissive values
        recompile: Recompile after setting properties (default True)
    """
    params: dict = {"material_path": material_path, "recompile": recompile}
    if blend_mode is not None:
        params["blend_mode"] = blend_mode
    if shading_model is not None:
        params["shading_model"] = shading_model
    if two_sided is not None:
        params["two_sided"] = two_sided
    if opacity_mask_clip_value is not None:
        params["opacity_mask_clip_value"] = opacity_mask_clip_value
    if dithered_lof_transition is not None:
        params["dithered_lof_transition"] = dithered_lof_transition
    if allow_negative_emissive_color is not None:
        params["allow_negative_emissive_color"] = allow_negative_emissive_color
    return _call("set_material_properties", params)


@mcp.tool()
def get_material_graph_nodes(material_path: str) -> str:
    """Read all expression nodes in a material graph — types, positions, properties, and connections.

    Returns every node with:
      - index: stable int index used by add/connect/delete_material_expression
      - type: short node type ("ScalarParameter", "Custom", "Multiply", "Time", etc.)
      - pos_x, pos_y: graph position
      - properties: type-specific dict —
          Custom nodes: code (full HLSL), description, output_type, inputs[], outputs[]
          ScalarParameter: parameter_name, default_value, group, slider_min, slider_max
          VectorParameter: parameter_name, default_value [r,g,b,a], group
          Constant: R
          Constant2Vector: R, G
          Constant3Vector/4Vector: Constant [r,g,b,a]
          TextureCoordinate: CoordinateIndex
      - input_connections: dict of {pin_name: {from_node: int, from_pin: str}}

    Use this instead of execute_python to read HLSL code from Custom nodes or
    to understand the existing graph before modifying it.

    Args:
        material_path: Full asset path (e.g. "/Game/Materials/M_Portal")
    """
    return _call("get_material_graph_nodes", {"material_path": material_path})


@mcp.tool()
def add_material_expression(
    material_path: str,
    node: str,
) -> str:
    """Add a single expression node to an existing material without clearing other nodes.

    Returns the node_index of the newly created node. Use that index with
    connect_material_expressions to wire it up.

    Args:
        material_path: Full asset path (e.g. "/Game/Materials/M_Portal")
        node: JSON object defining the node:
            {
              "type": "ScalarParameter",   // Short name — auto-prefixed with MaterialExpression
              "pos_x": -1200,
              "pos_y": -400,
              "properties": {              // Type-specific properties
                "parameter_name": "Speed",
                "default_value": 0.5
              }
            }
            For Custom HLSL nodes use type "Custom" and include top-level fields:
              "code": "...",
              "description": "My Node",
              "output_type": "float3",
              "inputs": ["UV", "Time"],
              "outputs": [{"name": "Alpha", "type": "float"}]
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
    """Connect two expression nodes in a material graph using their indices.

    Node indices come from get_material_graph_nodes (existing nodes) or
    add_material_expression (newly added nodes).

    Args:
        material_path: Full asset path (e.g. "/Game/Materials/M_Portal")
        from_node: Source node index (int)
        to_node: Target node index as string (e.g. "5") OR "material" to connect
            to a material output slot
        to_pin: Input pin name on the target node (e.g. "A", "B", "Alpha"),
            or a material output property when to_node="material":
            BaseColor | Metallic | Roughness | EmissiveColor | Opacity |
            OpacityMask | Normal | Specular | AmbientOcclusion |
            WorldPositionOffset | SubsurfaceColor | Refraction
        from_pin: Output pin name on the source node.
            "" (default) = the node's primary output.
            Named outputs (e.g. "Alpha" from a Custom node's additional outputs).
    """
    return _call("connect_material_expressions", {
        "material_path": material_path,
        "from_node": from_node,
        "from_pin": from_pin,
        "to_node": to_node,
        "to_pin": to_pin,
    })


@mcp.tool()
def delete_material_expression(
    material_path: str,
    node_index: int,
) -> str:
    """Delete a single expression node from a material by its index.

    After deletion the material is recompiled and saved. Indices of remaining
    nodes may shift — re-query with get_material_graph_nodes if you need to
    reference them afterwards.

    Args:
        material_path: Full asset path (e.g. "/Game/Materials/M_Portal")
        node_index: Index of the node to delete (from get_material_graph_nodes)
    """
    return _call("delete_material_expression", {
        "material_path": material_path,
        "node_index": node_index,
    })


@mcp.tool()
def add_material_comments(
    material_path: str,
    comments: str,
) -> str:
    """Add comment boxes to a material graph for organization and documentation.

    Comment boxes visually group and annotate sections of the material graph.
    They can enclose nodes and move them as a group when dragged.

    Args:
        material_path: Full path to the material (e.g. "/Game/Materials/M_Portal")
        comments: JSON array of comment definitions. Each comment has:
            - text: str — The comment text (required)
            - pos_x, pos_y: int — Position in the graph (default 0, 0)
            - size_x, size_y: int — Box dimensions in pixels (default 400, 200)
            - font_size: int — Text size 1-1000 (default 18)
            - color: [r, g, b] or [r, g, b, a] — Comment box color (default white)
            - show_bubble: bool — Show zoom-invariant bubble when zoomed out (default false)
            - color_bubble: bool — Use comment color for the bubble background (default false)
            - group_mode: bool — Move enclosed nodes when dragging the comment (default true)
    """
    return _call("add_material_comments", {
        "material_path": material_path,
        "comments": json.loads(comments),
    })


@mcp.tool()
def get_material_expression_info(
    material_path: str,
    node_index: int,
) -> str:
    """Get detailed info for a single material expression node, including all available pins.

    Returns full node data with:
      - type, pos_x, pos_y, properties (same as get_material_graph_nodes)
      - available_inputs: list of all input pin names (connected or not)
      - available_outputs: list of all output pin names (connected or not)
      - input_connections: which other nodes feed each input pin

    Use this before connecting nodes to discover exact pin names for a node type.

    Args:
        material_path: Full asset path (e.g. "/Game/Materials/M_Portal")
        node_index: Node index from get_material_graph_nodes
    """
    return _call("get_material_expression_info", {
        "material_path": material_path,
        "node_index": node_index,
    })


@mcp.tool()
def get_material_property_connections(material_path: str) -> str:
    """Query which expression node feeds each material output slot.

    Returns a dict of material output properties and the node/pin that drives them.
    Only lists slots that have something connected.

    Output format:
        {
          "connections": {
            "BaseColor": {"node_index": 3, "node_type": "Multiply", "from_pin": ""},
            "Roughness":  {"node_index": 1, "node_type": "ScalarParameter", "from_pin": ""}
          }
        }

    Args:
        material_path: Full asset path (e.g. "/Game/Materials/M_Portal")
    """
    return _call("get_material_property_connections", {"material_path": material_path})


@mcp.tool()
def set_material_expression_property(
    material_path: str,
    node_index: int,
    property_name: str,
    property_value: str,
) -> str:
    """Set a single property on an existing material expression node.

    Accepts snake_case property names (e.g. "parameter_name") or PascalCase
    (e.g. "ParameterName") — both are handled correctly.

    Common properties by node type:
      ScalarParameter:  parameter_name (str), default_value (float),
                        slider_min, slider_max, group, sort_priority
      VectorParameter:  parameter_name (str), default_value ([r,g,b,a]), group
      TextureSample:    texture ("/Game/path"), sampler_type
      Constant:         R (float)
      Constant3Vector:  Constant ([r,g,b,a])
      Custom (HLSL):    code (str), description (str)
      Panner:           SpeedX (float), SpeedY (float)
      TextureCoordinate: CoordinateIndex (int)

    For asset references (textures, etc.), pass the full content path as a string
    starting with "/" (e.g. "/Game/Textures/T_Wood").

    Args:
        material_path: Full asset path (e.g. "/Game/Materials/M_Portal")
        node_index: Node index from get_material_graph_nodes
        property_name: Property name (snake_case or PascalCase)
        property_value: JSON-encoded value — string, number, bool, or array
    """
    return _call("set_material_expression_property", {
        "material_path": material_path,
        "node_index": node_index,
        "property_name": property_name,
        "property_value": json.loads(property_value),
    })


@mcp.tool()
def move_material_expression(
    material_path: str,
    node_index: int,
    pos_x: int,
    pos_y: int,
) -> str:
    """Move a material expression node to a new graph position.

    Args:
        material_path: Full asset path (e.g. "/Game/Materials/M_Portal")
        node_index: Node index from get_material_graph_nodes
        pos_x: New X position in the material graph (negative = left of output)
        pos_y: New Y position in the material graph
    """
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
    """Duplicate an existing material expression node.

    Creates an exact copy of the node (same type and properties) offset from
    the original. Returns the new node's index.

    Connections are NOT copied — wire the new node separately with
    connect_material_expressions.

    Args:
        material_path: Full asset path (e.g. "/Game/Materials/M_Portal")
        node_index: Index of the node to duplicate
        offset_x: X offset from the original node's position (default 0)
        offset_y: Y offset from the original node's position (default 150)
    """
    return _call("duplicate_material_expression", {
        "material_path": material_path,
        "node_index": node_index,
        "offset_x": offset_x,
        "offset_y": offset_y,
    })


@mcp.tool()
def layout_material_expressions(material_path: str) -> str:
    """Auto-layout all expression nodes in a material graph.

    Uses Unreal's built-in layout algorithm to arrange nodes neatly.
    Useful after building a graph programmatically to make it readable in
    the material editor.

    Args:
        material_path: Full asset path (e.g. "/Game/Materials/M_Portal")
    """
    return _call("layout_material_expressions", {"material_path": material_path})


@mcp.tool()
def get_material_instance_parameters(material_path: str) -> str:
    """Get all overridable parameters from a Material Instance.

    Returns all scalar, vector, texture, and static switch parameters
    with their current override values (or the parent default if not overridden).

    Args:
        material_path: Full asset path to the Material Instance
                       (e.g. "/Game/Materials/MI_Rock")
    """
    return _call("get_material_instance_parameters", {"material_path": material_path})


@mcp.tool()
def set_material_instance_parameter(
    material_path: str,
    param_name: str,
    param_type: str,
    value: str,
) -> str:
    """Set a single parameter override on a Material Instance.

    Args:
        material_path: Full asset path to the Material Instance
                       (e.g. "/Game/Materials/MI_Rock")
        param_name: Parameter name as defined in the parent material
        param_type: One of: scalar | vector | texture | static_switch
        value: JSON-encoded value:
            scalar:        float number (e.g. "0.5")
            vector:        [r, g, b, a] array (e.g. "[1.0, 0.0, 0.0, 1.0]")
            texture:       content path string (e.g. '"/Game/Textures/T_Wood"')
            static_switch: bool (e.g. "true" or "false")
    """
    return _call("set_material_instance_parameter", {
        "material_path": material_path,
        "param_name": param_name,
        "param_type": param_type,
        "value": json.loads(value),
    })
