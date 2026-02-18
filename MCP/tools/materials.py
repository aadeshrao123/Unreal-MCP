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
