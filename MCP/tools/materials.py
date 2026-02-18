"""Material tools — create, inspect, and build complex material graphs."""

import json
from typing import Optional

from _bridge import mcp, _send

# ---------------------------------------------------------------------------
# Constant maps
# ---------------------------------------------------------------------------
_BLEND_MODES = {
    "opaque": "BLEND_OPAQUE",
    "masked": "BLEND_MASKED",
    "translucent": "BLEND_TRANSLUCENT",
    "additive": "BLEND_ADDITIVE",
    "modulate": "BLEND_MODULATE",
    "alpha_composite": "BLEND_ALPHA_COMPOSITE",
    "alpha_holdout": "BLEND_ALPHA_HOLDOUT",
}

_SHADING_MODELS = {
    "default_lit": "MSM_DEFAULT_LIT",
    "unlit": "MSM_UNLIT",
    "subsurface": "MSM_SUBSURFACE",
    "clear_coat": "MSM_CLEAR_COAT",
    "subsurface_profile": "MSM_SUBSURFACE_PROFILE",
    "two_sided_foliage": "MSM_TWO_SIDED_FOLIAGE",
    "cloth": "MSM_CLOTH",
    "eye": "MSM_EYE",
    "thin_translucent": "MSM_THIN_TRANSLUCENT",
}

_MATERIAL_PROPERTIES = {
    "BaseColor": "MP_BASE_COLOR",
    "Metallic": "MP_METALLIC",
    "Roughness": "MP_ROUGHNESS",
    "EmissiveColor": "MP_EMISSIVE_COLOR",
    "Opacity": "MP_OPACITY",
    "OpacityMask": "MP_OPACITY_MASK",
    "Normal": "MP_NORMAL",
    "Specular": "MP_SPECULAR",
    "AmbientOcclusion": "MP_AMBIENT_OCCLUSION",
    "WorldPositionOffset": "MP_WORLD_POSITION_OFFSET",
    "SubsurfaceColor": "MP_SUBSURFACE_COLOR",
    "Refraction": "MP_REFRACTION",
}

_OUTPUT_TYPES = {
    "float": "CMOT_FLOAT1",
    "float1": "CMOT_FLOAT1",
    "float2": "CMOT_FLOAT2",
    "float3": "CMOT_FLOAT3",
    "float4": "CMOT_FLOAT4",
    "material_attributes": "CMOT_MATERIAL_ATTRIBUTES",
}

# ---------------------------------------------------------------------------
# Internal helpers — code generation
# ---------------------------------------------------------------------------

def _resolve_class(name: str) -> str:
    """Prefix 'MaterialExpression' if the short form is given."""
    if name.startswith("MaterialExpression"):
        return name
    return f"MaterialExpression{name}"


def _gen_property_value(key: str, val) -> str:
    """Generate a Python expression string for a property value.

    - Strings starting with 'unreal.' -> passed through as code (enum refs)
    - Strings starting with '/' -> load_asset()
    - Other strings -> repr()
    - Bools -> True/False
    - Numbers -> direct
    - Lists of 2 floats -> Vector2D
    - Lists of 3-4 floats -> LinearColor
    """
    if isinstance(val, str):
        if val.startswith("unreal."):
            return val
        if val.startswith("/"):
            return f"unreal.EditorAssetLibrary.load_asset({repr(val)})"
        return repr(val)
    if isinstance(val, bool):
        return "True" if val else "False"
    if isinstance(val, (int, float)):
        return str(val)
    if isinstance(val, list):
        if len(val) == 2:
            return f"unreal.Vector2D({float(val[0])}, {float(val[1])})"
        if len(val) in (3, 4):
            comps = [str(float(c)) for c in val]
            if len(comps) == 3:
                comps.append("1.0")
            return f"unreal.LinearColor({', '.join(comps)})"
    return repr(val)


def _gen_custom_lines(idx: int, node: dict, indent: str) -> list[str]:
    """Generate code lines specific to MaterialExpressionCustom (Custom HLSL).

    Handles:
    - code (HLSL source)
    - description
    - output_type
    - inputs (named input pins — creates fresh CustomInput objects)
    - outputs (additional named output pins)
    """
    lines = []
    var = f"n{idx}"

    # HLSL code body
    if "code" in node:
        lines.append(f"{indent}{var}.set_editor_property('code', {repr(node['code'])})")

    # Description (shows as the node title)
    if "description" in node:
        lines.append(f"{indent}{var}.set_editor_property('description', {repr(node['description'])})")

    # Output type
    if "output_type" in node:
        ot = _OUTPUT_TYPES.get(node["output_type"].lower(), "CMOT_FLOAT3")
        lines.append(f"{indent}{var}.set_editor_property('output_type', unreal.CustomMaterialOutputType.{ot})")

    # Named inputs — UE5 requires fresh CustomInput() objects with set_editor_property
    if "inputs" in node and node["inputs"]:
        input_names = node["inputs"]
        lines.append(f"{indent}_inputs = []")
        for iname in input_names:
            lines.append(f"{indent}_ci = unreal.CustomInput()")
            lines.append(f"{indent}_ci.set_editor_property('input_name', {repr(iname)})")
            lines.append(f"{indent}_inputs.append(_ci)")
        lines.append(f"{indent}{var}.set_editor_property('inputs', _inputs)")

    # Additional outputs
    if "outputs" in node and node["outputs"]:
        lines.append(f"{indent}_outputs = []")
        for out in node["outputs"]:
            oname = out.get("name", "Out")
            otype = _OUTPUT_TYPES.get(out.get("type", "float3").lower(), "CMOT_FLOAT3")
            lines.append(f"{indent}_co = unreal.CustomOutput()")
            lines.append(f"{indent}_co.set_editor_property('output_name', {repr(oname)})")
            lines.append(f"{indent}_co.set_editor_property('output_type', unreal.CustomMaterialOutputType.{otype})")
            lines.append(f"{indent}_outputs.append(_co)")
        lines.append(f"{indent}{var}.set_editor_property('additional_outputs', _outputs)")

    return lines


def _gen_node_lines(idx: int, node: dict) -> list[str]:
    """Generate Python code lines that create one material expression node.

    Returns lines that create variable n{idx} holding the expression.
    Uses _mel alias (defined by build_material_graph).
    """
    indent = "    "
    node_type = node.get("type", "Constant")
    full_class = _resolve_class(node_type)
    pos_x = node.get("pos_x", -300)
    pos_y = node.get("pos_y", 0)
    var = f"n{idx}"

    lines = [
        f"{indent}{var} = _mel.create_material_expression("
        f"mat, unreal.{full_class}, {pos_x}, {pos_y})"
    ]

    # Custom HLSL needs special handling
    is_custom = node_type in ("Custom", "MaterialExpressionCustom")
    if is_custom:
        lines.extend(_gen_custom_lines(idx, node, indent))

    # Generic properties
    props = node.get("properties", {})
    for key, val in props.items():
        lines.append(f"{indent}{var}.set_editor_property({repr(key)}, {_gen_property_value(key, val)})")

    return lines


def _gen_connection_lines(conn: dict) -> list[str]:
    """Generate Python code lines for one connection between nodes or to material.

    Uses _mel alias (defined by build_material_graph).

    Connection format:
        from_node: int (index) or "material"
        from_pin: str (output pin name, "" for default)
        to_node: int (index) or "material"
        to_pin: str (input pin name or material property name)
    """
    indent = "    "
    from_node = conn.get("from_node")
    from_pin = conn.get("from_pin", "")
    to_node = conn.get("to_node")
    to_pin = conn.get("to_pin", "")

    lines = []

    # Connection to material output pin
    if to_node == "material":
        mat_prop = _MATERIAL_PROPERTIES.get(to_pin)
        if mat_prop:
            lines.append(
                f"{indent}_mel.connect_material_property("
                f"n{from_node}, {repr(from_pin)}, unreal.MaterialProperty.{mat_prop})"
            )
        else:
            lines.append(f"{indent}_errors.append('Unknown material property: {to_pin}')")
        return lines

    # Node-to-node connection
    lines.append(
        f"{indent}_mel.connect_material_expressions("
        f"n{from_node}, {repr(from_pin)}, n{to_node}, {repr(to_pin)})"
    )
    return lines


# ---------------------------------------------------------------------------
# Tools
# ---------------------------------------------------------------------------

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
    bm = _BLEND_MODES.get(blend_mode.lower(), "BLEND_OPAQUE")
    sm = _SHADING_MODELS.get(shading_model.lower(), "MSM_DEFAULT_LIT")
    ts = "True" if two_sided else "False"

    lines = [
        "import unreal",
        "factory = unreal.MaterialFactoryNew()",
        "asset_tools = unreal.AssetToolsHelpers.get_asset_tools()",
        f"mat = asset_tools.create_asset({repr(name)}, {repr(path)}, unreal.Material, factory)",
        "if not mat:",
        f"    result = {{'success': False, 'error': 'Failed to create material (may already exist)'}}",
        "else:",
        f"    mat.set_editor_property('blend_mode', unreal.BlendMode.{bm})",
        f"    mat.set_editor_property('shading_model', unreal.MaterialShadingModel.{sm})",
        f"    mat.set_editor_property('two_sided', {ts})",
    ]
    if opacity_mask_clip_value is not None:
        lines.append(f"    mat.set_editor_property('opacity_mask_clip_value', {opacity_mask_clip_value})")
    lines += [
        f"    unreal.EditorAssetLibrary.save_asset('{path}/{name}')",
        f"    result = {{'success': True, 'path': '{path}/{name}'}}",
    ]
    return _send("\n".join(lines))


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
    lines = [
        "import unreal",
        "factory = unreal.MaterialInstanceConstantFactoryNew()",
        "asset_tools = unreal.AssetToolsHelpers.get_asset_tools()",
        f"mi = asset_tools.create_asset({repr(name)}, {repr(path)}, "
        "unreal.MaterialInstanceConstant, factory)",
        "if not mi:",
        "    result = {'success': False, 'error': 'Failed to create material instance'}",
        "else:",
        f"    parent = unreal.EditorAssetLibrary.load_asset({repr(parent_path)})",
        "    if parent:",
        "        mi.set_editor_property('parent', parent)",
    ]

    if scalar_params:
        for pname, pval in json.loads(scalar_params).items():
            lines.append(
                f"    unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value("
                f"mi, {repr(pname)}, {pval})"
            )

    if vector_params:
        for pname, pval in json.loads(vector_params).items():
            r, g, b = float(pval[0]), float(pval[1]), float(pval[2])
            a = float(pval[3]) if len(pval) > 3 else 1.0
            lines.append(
                f"    unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value("
                f"mi, {repr(pname)}, unreal.LinearColor({r}, {g}, {b}, {a}))"
            )

    if texture_params:
        for pname, pval in json.loads(texture_params).items():
            lines.append(
                f"    _tex = unreal.EditorAssetLibrary.load_asset({repr(pval)})"
            )
            lines.append(
                f"    if _tex: unreal.MaterialEditingLibrary."
                f"set_material_instance_texture_parameter_value(mi, {repr(pname)}, _tex)"
            )

    lines += [
        f"    unreal.EditorAssetLibrary.save_asset('{path}/{name}')",
        f"    result = {{'success': True, 'path': '{path}/{name}'}}",
    ]
    return _send("\n".join(lines))


@mcp.tool()
def build_material_graph(
    material_path: str,
    nodes: str,
    connections: str,
    clear_existing: bool = True,
) -> str:
    """Build a complete material node graph in one atomic operation.

    Creates expression nodes and wires them together in a single exec() call.
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
                - Values starting with "unreal." are passed as code (enum refs)
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
    node_list = json.loads(nodes)
    conn_list = json.loads(connections)

    # Build the script
    code_lines = [
        "import unreal",
        f"mat = unreal.EditorAssetLibrary.load_asset({repr(material_path)})",
        "if not mat:",
        f"    result = {{'success': False, 'error': 'Material not found: {material_path}'}}",
        "else:",
        "    _errors = []",
        "    _mel = unreal.MaterialEditingLibrary",
    ]

    # Clear existing expressions (two passes needed — UE5 doesn't always get all in one)
    if clear_existing:
        code_lines += [
            "    _mel.delete_all_material_expressions(mat)",
            "    _mel.delete_all_material_expressions(mat)",
        ]

    # Generate node creation
    for idx, node in enumerate(node_list):
        code_lines.extend(_gen_node_lines(idx, node))

    # Generate connections
    for conn in conn_list:
        code_lines.extend(_gen_connection_lines(conn))

    # Recompile, save, report
    nc = len(node_list)
    cc = len(conn_list)
    code_lines += [
        "    _mel.recompile_material(mat)",
        f"    unreal.EditorAssetLibrary.save_asset({repr(material_path)})",
        f"    result = {{'success': True, 'path': {repr(material_path)}, 'nodes_created': {nc}, 'connections_made': {cc}, 'errors': _errors}}",
    ]

    return _send("\n".join(code_lines))


@mcp.tool()
def get_material_info(material_path: str) -> str:
    """Inspect a material: properties, expression count, parameter names, textures.

    Args:
        material_path: Full path to the material (e.g. "/Game/Materials/M_Portal")
    """
    lines = [
        "import unreal",
        f"mat = unreal.EditorAssetLibrary.load_asset({repr(material_path)})",
        "if not mat:",
        f"    result = {{'success': False, 'error': 'Material not found: {material_path}'}}",
        "else:",
        "    _mel = unreal.MaterialEditingLibrary",
        "    info = {}",
        f"    info['path'] = {repr(material_path)}",
        "    info['blend_mode'] = str(mat.get_editor_property('blend_mode'))",
        "    info['shading_model'] = str(mat.get_editor_property('shading_model'))",
        "    info['two_sided'] = mat.get_editor_property('two_sided')",
        "    info['num_expressions'] = _mel.get_num_material_expressions(mat)",
        # Collect parameter names by scanning expressions
        "    _params = {'scalar': [], 'vector': [], 'texture': []}",
        "    info['used_textures'] = []",
        "    try:",
        "        _texs = _mel.get_used_textures(mat)",
        "        if _texs:",
        "            info['used_textures'] = [str(t.get_path_name()) for t in _texs]",
        "    except Exception:",
        "        pass",
        # Get material stats if available
        "    try:",
        "        _stats = _mel.get_statistics(mat)",
        "        if _stats:",
        "            info['stats'] = str(_stats)",
        "    except Exception:",
        "        pass",
        "    result = {'success': True, 'info': info}",
    ]
    return _send("\n".join(lines))


@mcp.tool()
def recompile_material(material_path: str) -> str:
    """Force recompile a material and save it. Reports success/failure.

    Args:
        material_path: Full path to the material (e.g. "/Game/Materials/M_Portal")
    """
    lines = [
        "import unreal",
        f"mat = unreal.EditorAssetLibrary.load_asset({repr(material_path)})",
        "if not mat:",
        f"    result = {{'success': False, 'error': 'Material not found: {material_path}'}}",
        "else:",
        "    unreal.MaterialEditingLibrary.recompile_material(mat)",
        f"    unreal.EditorAssetLibrary.save_asset({repr(material_path)})",
        f"    result = {{'success': True, 'path': {repr(material_path)}, 'message': 'Recompiled and saved'}}",
    ]
    return _send("\n".join(lines))


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
    lines = [
        "import unreal",
        f"mat = unreal.EditorAssetLibrary.load_asset({repr(material_path)})",
        "if not mat:",
        f"    result = {{'success': False, 'error': 'Material not found: {material_path}'}}",
        "else:",
        "    _changed = []",
    ]

    if blend_mode is not None:
        bm = _BLEND_MODES.get(blend_mode.lower(), "BLEND_OPAQUE")
        lines.append(f"    mat.set_editor_property('blend_mode', unreal.BlendMode.{bm})")
        lines.append(f"    _changed.append('blend_mode={blend_mode}')")

    if shading_model is not None:
        sm = _SHADING_MODELS.get(shading_model.lower(), "MSM_DEFAULT_LIT")
        lines.append(f"    mat.set_editor_property('shading_model', unreal.MaterialShadingModel.{sm})")
        lines.append(f"    _changed.append('shading_model={shading_model}')")

    if two_sided is not None:
        lines.append(f"    mat.set_editor_property('two_sided', {'True' if two_sided else 'False'})")
        lines.append(f"    _changed.append('two_sided={two_sided}')")

    if opacity_mask_clip_value is not None:
        lines.append(f"    mat.set_editor_property('opacity_mask_clip_value', {opacity_mask_clip_value})")
        lines.append(f"    _changed.append('opacity_mask_clip_value={opacity_mask_clip_value}')")

    if dithered_lof_transition is not None:
        val = "True" if dithered_lof_transition else "False"
        lines.append(f"    mat.set_editor_property('dithered_lof_transition', {val})")
        lines.append(f"    _changed.append('dithered_lof_transition={dithered_lof_transition}')")

    if allow_negative_emissive_color is not None:
        val = "True" if allow_negative_emissive_color else "False"
        lines.append(f"    mat.set_editor_property('allow_negative_emissive_color', {val})")
        lines.append(f"    _changed.append('allow_negative_emissive_color={allow_negative_emissive_color}')")

    if recompile:
        lines.append("    unreal.MaterialEditingLibrary.recompile_material(mat)")

    lines += [
        f"    unreal.EditorAssetLibrary.save_asset({repr(material_path)})",
        f"    result = {{'success': True, 'path': {repr(material_path)}, 'changed': _changed}}",
    ]
    return _send("\n".join(lines))


