"""Niagara tools — create and control Niagara particle systems, emitters, modules, and renderers."""

from unrealmcp._bridge import mcp
from unrealmcp._tcp_bridge import _call


# ---------------------------------------------------------------------------
# System Management
# ---------------------------------------------------------------------------

@mcp.tool()
def create_niagara_system(
    asset_path: str,
    template: str = "empty",
) -> str:
    """Create a new Niagara System asset.

    Args:
        asset_path: Full content path including name (e.g. "/Game/FX/NS_Fire")
        template: Template to base the system on — "empty" or a path to an existing system/template
    """
    return _call("create_niagara_system", {
        "asset_path": asset_path,
        "template": template,
    })


@mcp.tool()
def get_niagara_system_info(
    system_path: str,
    include: str = "all",
    filter: str | None = None,
) -> str:
    """Get detailed information about a Niagara System.

    Returns system name, emitter list, user parameters, and compilation status.
    Use filter to narrow emitters and parameters by name substring.

    Args:
        system_path: Path to the Niagara System asset
        include: Comma-separated sections — emitters, parameters, compilation, all
        filter: Optional name substring filter on emitters and parameters
            (e.g. "Spark", "Color", "Flame")
    """
    params: dict = {
        "system_path": system_path,
        "include": include,
    }
    if filter is not None:
        params["filter"] = filter
    return _call("get_niagara_system_info", params)


@mcp.tool()
def list_niagara_systems(
    path: str = "/Game",
    name_filter: str | None = None,
    max_results: int = 100,
) -> str:
    """List Niagara System assets in the project.

    Args:
        path: Content Browser directory to search in
        name_filter: Optional substring filter on asset name
        max_results: Maximum number of results to return
    """
    params: dict = {
        "path": path,
        "max_results": max_results,
    }
    if name_filter is not None:
        params["name_filter"] = name_filter
    return _call("list_niagara_systems", params)


@mcp.tool()
def delete_niagara_system(
    system_path: str,
    force: bool = False,
) -> str:
    """Delete a Niagara System asset.

    Args:
        system_path: Path to the Niagara System asset
        force: If true, skip reference checks and force deletion
    """
    return _call("delete_niagara_system", {
        "system_path": system_path,
        "force": force,
    })


@mcp.tool()
def compile_niagara_system(
    system_path: str,
    wait_for_completion: bool = True,
) -> str:
    """Compile a Niagara System and report any errors.

    Args:
        system_path: Path to the Niagara System asset
        wait_for_completion: If true, block until compilation finishes
    """
    return _call("compile_niagara_system", {
        "system_path": system_path,
        "wait_for_completion": wait_for_completion,
    })


# ---------------------------------------------------------------------------
# Emitter Management
# ---------------------------------------------------------------------------

@mcp.tool()
def get_niagara_emitters(
    system_path: str,
    filter: str | None = None,
) -> str:
    """Get emitters in a Niagara System.

    Returns name, unique_name, index, enabled state, sim_target, local_space,
    determinism, and renderer_count per emitter.
    Use filter to get specific emitters by name substring.

    Args:
        system_path: Path to the Niagara System asset
        filter: Optional name filter (e.g. "Spark", "Flame", "Trail")
    """
    params: dict = {"system_path": system_path}
    if filter is not None:
        params["filter"] = filter
    return _call("get_niagara_emitters", params)


@mcp.tool()
def add_niagara_emitter(
    system_path: str,
    emitter_path: str | None = None,
    emitter_name: str | None = None,
    template: str | None = None,
) -> str:
    """Add an emitter to a Niagara System.

    Provide either emitter_path (existing emitter asset) or template (built-in template name).

    Args:
        system_path: Path to the Niagara System asset
        emitter_path: Path to an existing NiagaraEmitter asset to add
        emitter_name: Optional display name for the emitter in this system
        template: Built-in template name (e.g. "Fountain", "Sprite", "Mesh")
    """
    params: dict = {"system_path": system_path}
    if emitter_path is not None:
        params["emitter_path"] = emitter_path
    if emitter_name is not None:
        params["emitter_name"] = emitter_name
    if template is not None:
        params["template"] = template
    return _call("add_niagara_emitter", params)


@mcp.tool()
def remove_niagara_emitter(
    system_path: str,
    emitter_name: str,
) -> str:
    """Remove an emitter from a Niagara System by name.

    Args:
        system_path: Path to the Niagara System asset
        emitter_name: Name of the emitter to remove
    """
    return _call("remove_niagara_emitter", {
        "system_path": system_path,
        "emitter_name": emitter_name,
    })


@mcp.tool()
def set_niagara_emitter_property(
    system_path: str,
    emitter_name: str,
    property: str,
    value: str,
) -> str:
    """Set a property on an emitter within a Niagara System.

    Args:
        system_path: Path to the Niagara System asset
        emitter_name: Name of the target emitter
        property: Property name (e.g. "SimTarget", "FixedBounds", "LocalSpace")
        value: Property value as string
    """
    return _call("set_niagara_emitter_property", {
        "system_path": system_path,
        "emitter_name": emitter_name,
        "property": property,
        "value": value,
    })


@mcp.tool()
def duplicate_niagara_emitter(
    system_path: str,
    emitter_name: str,
    new_name: str | None = None,
) -> str:
    """Duplicate an emitter within a Niagara System.

    Args:
        system_path: Path to the Niagara System asset
        emitter_name: Name of the emitter to duplicate
        new_name: Optional name for the duplicated emitter
    """
    params: dict = {
        "system_path": system_path,
        "emitter_name": emitter_name,
    }
    if new_name is not None:
        params["new_name"] = new_name
    return _call("duplicate_niagara_emitter", params)


@mcp.tool()
def reorder_niagara_emitter(
    system_path: str,
    emitter_name: str,
    new_index: int,
) -> str:
    """Move an emitter to a new position in the system's emitter list.

    Args:
        system_path: Path to the Niagara System asset
        emitter_name: Name of the emitter to move
        new_index: Target index (0-based) in the emitter list
    """
    return _call("reorder_niagara_emitter", {
        "system_path": system_path,
        "emitter_name": emitter_name,
        "new_index": new_index,
    })


# ---------------------------------------------------------------------------
# Module Stack
# ---------------------------------------------------------------------------

@mcp.tool()
def get_niagara_modules(
    system_path: str,
    emitter_name: str,
    script_usage: str = "all",
    include_inputs: bool = True,
    filter: str | None = None,
) -> str:
    """Get modules in an emitter's script usage stack.

    Returns name, display_name, index, script_usage, script_path, and optionally
    input parameters per module. Use filter to get specific modules by function name.

    Args:
        system_path: Path to the Niagara System asset
        emitter_name: Name of the target emitter
        script_usage: Which stack — EmitterSpawn, EmitterUpdate, ParticleSpawn, ParticleUpdate,
            SystemSpawn, SystemUpdate, or "all" for every stack
        include_inputs: If true, include each module's input parameters
        filter: Optional function name filter (e.g. "Spawn", "Color", "Gravity", "Velocity")
    """
    params: dict = {
        "system_path": system_path,
        "emitter_name": emitter_name,
        "script_usage": script_usage,
        "include_inputs": include_inputs,
    }
    if filter is not None:
        params["filter"] = filter
    return _call("get_niagara_modules", params)


@mcp.tool()
def add_niagara_module(
    system_path: str,
    emitter_name: str,
    module_path: str,
    script_usage: str,
    index: int = -1,
) -> str:
    """Add a module to an emitter's script usage stack.

    Args:
        system_path: Path to the Niagara System asset
        emitter_name: Name of the target emitter
        module_path: Path to the module script asset (e.g. "/Niagara/Modules/Update/SolveForcesAndVelocity")
        script_usage: Target stack — EmitterSpawn, EmitterUpdate, ParticleSpawn, ParticleUpdate,
            SystemSpawn, SystemUpdate
        index: Position in the stack (-1 = append at end)
    """
    return _call("add_niagara_module", {
        "system_path": system_path,
        "emitter_name": emitter_name,
        "module_path": module_path,
        "script_usage": script_usage,
        "index": index,
    })


@mcp.tool()
def remove_niagara_module(
    system_path: str,
    emitter_name: str,
    module_name: str,
    script_usage: str,
) -> str:
    """Remove a module from an emitter's script usage stack.

    Args:
        system_path: Path to the Niagara System asset
        emitter_name: Name of the target emitter
        module_name: Name of the module to remove
        script_usage: Stack the module is in — EmitterSpawn, EmitterUpdate, ParticleSpawn, ParticleUpdate,
            SystemSpawn, SystemUpdate
    """
    return _call("remove_niagara_module", {
        "system_path": system_path,
        "emitter_name": emitter_name,
        "module_name": module_name,
        "script_usage": script_usage,
    })


@mcp.tool()
def set_niagara_module_enabled(
    system_path: str,
    emitter_name: str,
    module_name: str,
    script_usage: str,
    enabled: bool,
) -> str:
    """Enable or disable a module in an emitter's stack.

    Args:
        system_path: Path to the Niagara System asset
        emitter_name: Name of the target emitter
        module_name: Name of the module
        script_usage: Stack the module is in
        enabled: True to enable, false to disable
    """
    return _call("set_niagara_module_enabled", {
        "system_path": system_path,
        "emitter_name": emitter_name,
        "module_name": module_name,
        "script_usage": script_usage,
        "enabled": enabled,
    })


@mcp.tool()
def reorder_niagara_module(
    system_path: str,
    emitter_name: str,
    module_name: str,
    script_usage: str,
    new_index: int,
) -> str:
    """Move a module to a new position within its script usage stack.

    Args:
        system_path: Path to the Niagara System asset
        emitter_name: Name of the target emitter
        module_name: Name of the module to move
        script_usage: Stack the module is in
        new_index: Target index (0-based) within the stack
    """
    return _call("reorder_niagara_module", {
        "system_path": system_path,
        "emitter_name": emitter_name,
        "module_name": module_name,
        "script_usage": script_usage,
        "new_index": new_index,
    })


@mcp.tool()
def get_niagara_module_inputs(
    system_path: str,
    emitter_name: str,
    module_name: str,
    script_usage: str,
    input_filter: str | None = None,
    include_schema: bool = False,
) -> str:
    """Get all input parameters for a specific module.

    Args:
        system_path: Path to the Niagara System asset
        emitter_name: Name of the target emitter
        module_name: Name of the module
        script_usage: Stack the module is in
        input_filter: Optional substring filter on input parameter names
        include_schema: If true, include massive structural type schemas for inputs
    """
    params: dict = {
        "system_path": system_path,
        "emitter_name": emitter_name,
        "module_name": module_name,
        "script_usage": script_usage,
        "include_schema": include_schema,
    }
    if input_filter is not None:
        params["input_filter"] = input_filter
    return _call("get_niagara_module_inputs", params)


# ---------------------------------------------------------------------------
# Module Inputs
# ---------------------------------------------------------------------------

@mcp.tool()
def set_niagara_module_input(
    system_path: str,
    emitter_name: str,
    module_name: str,
    input_name: str,
    value: str,
    script_usage: str,
) -> str:
    """Set a static value on a module input parameter.

    Args:
        system_path: Path to the Niagara System asset
        emitter_name: Name of the target emitter
        module_name: Name of the module
        input_name: Name of the input parameter to set
        value: Value as string — scalars, vectors "(X,Y,Z)", colors "(R,G,B,A)", enums, bools
        script_usage: Stack the module is in
    """
    return _call("set_niagara_module_input", {
        "system_path": system_path,
        "emitter_name": emitter_name,
        "module_name": module_name,
        "input_name": input_name,
        "value": value,
        "script_usage": script_usage,
    })


@mcp.tool()
def set_niagara_dynamic_input(
    system_path: str,
    emitter_name: str,
    module_name: str,
    input_name: str,
    script_usage: str,
    dynamic_input_type: str,
    min_value: str | None = None,
    max_value: str | None = None,
    parameter_name: str | None = None,
    expression: str | None = None,
    dynamic_input_script_path: str | None = None,
    suggested_name: str | None = None,
    pin_defaults: dict | None = None,
) -> str:
    """Set a dynamic input (random range, linked parameter, custom expression, or arbitrary script) on a module input.

    Uses FNiagaraStackGraphUtilities::SetDynamicInputForFunctionInput for the script
    path, which is the same code path the editor uses when picking a dynamic input
    from the stack UI.

    Args:
        system_path: Path to the Niagara System asset
        emitter_name: Name of the target emitter
        module_name: Name of the module
        input_name: Name of the input parameter
        script_usage: Stack the module is in
        dynamic_input_type: One of:
            - "random_range" / "uniform_random"   — Ranged random (min_value, max_value)
            - "parameter_link"                    — Link to an existing parameter (parameter_name)
            - "custom_expression"                 — Custom HLSL expression node (expression)
            - "script" / "asset"                  — Arbitrary dynamic input script (dynamic_input_script_path)
        min_value / max_value: Values for random_range types (float or vector JSON object)
        parameter_name: Parameter name for parameter_link type
        expression: HLSL expression for custom_expression type
        dynamic_input_script_path: Asset path of a DynamicInput-usage UNiagaraScript
            (for dynamic_input_type="script"). Discover candidates via search_niagara_functions.
        suggested_name: Optional friendly name for the dynamic input in the stack UI
        pin_defaults: Optional {pin_name: default_value_string} dict applied to the
            new dynamic input node's own input pins after wiring
    """
    params: dict = {
        "system_path": system_path,
        "emitter_name": emitter_name,
        "module_name": module_name,
        "input_name": input_name,
        "script_usage": script_usage,
        "dynamic_input_type": dynamic_input_type,
    }
    if min_value is not None:
        params["min_value"] = min_value
    if max_value is not None:
        params["max_value"] = max_value
    if parameter_name is not None:
        params["parameter_name"] = parameter_name
    if expression is not None:
        params["expression"] = expression
    if dynamic_input_script_path is not None:
        params["dynamic_input_script_path"] = dynamic_input_script_path
    if suggested_name is not None:
        params["suggested_name"] = suggested_name
    if pin_defaults is not None:
        params["pin_defaults"] = pin_defaults
    return _call("set_niagara_dynamic_input", params)


@mcp.tool()
def set_niagara_curve(
    system_path: str,
    emitter_name: str,
    module_name: str,
    input_name: str,
    script_usage: str,
    curve_type: str,
    keys: str,
) -> str:
    """Set a curve on a module input parameter.

    Args:
        system_path: Path to the Niagara System asset
        emitter_name: Name of the target emitter
        module_name: Name of the module
        input_name: Name of the input parameter
        script_usage: Stack the module is in
        curve_type: Curve type — "FloatCurve", "ColorCurve", "VectorCurve"
        keys: JSON array of curve keys, e.g. [{"time":0,"value":1},{"time":1,"value":0}]
            For color curves: [{"time":0,"r":1,"g":0,"b":0,"a":1}]
    """
    return _call("set_niagara_curve", {
        "system_path": system_path,
        "emitter_name": emitter_name,
        "module_name": module_name,
        "input_name": input_name,
        "script_usage": script_usage,
        "curve_type": curve_type,
        "keys": keys,
    })


# ---------------------------------------------------------------------------
# User Parameters
# ---------------------------------------------------------------------------

@mcp.tool()
def get_niagara_user_parameters(
    system_path: str | None = None,
    actor_name: str | None = None,
    filter: str | None = None,
) -> str:
    """Get user-exposed parameters from a Niagara System asset or a level actor's component.

    Returns name, type, and value_type per parameter.
    Provide either system_path (asset) or actor_name (level instance).
    Use filter to narrow by parameter name substring.

    Args:
        system_path: Path to the Niagara System asset
        actor_name: Name of an actor in the level with a NiagaraComponent
        filter: Optional name filter (e.g. "Color", "Intensity", "Size")
    """
    params: dict = {}
    if system_path is not None:
        params["system_path"] = system_path
    if actor_name is not None:
        params["actor_name"] = actor_name
    if filter is not None:
        params["filter"] = filter
    return _call("get_niagara_user_parameters", params)


@mcp.tool()
def add_niagara_user_parameter(
    system_path: str,
    parameter_name: str,
    parameter_type: str,
    default_value: str | None = None,
) -> str:
    """Add a user parameter to a Niagara System. Supports every type the editor's
    User Parameters "+" menu can add — primitives, structs, enums, and Data Interfaces.

    Use list_niagara_parameter_types to discover valid type names before calling.

    Args:
        system_path: Path to the Niagara System asset
        parameter_name: Name for the new parameter (auto-prefixed with "User.")
        parameter_type: Type name. Resolved via NiagaraTypeRegistry, so anything
            Niagara registered is valid. Examples:
              - Primitives: "float", "int", "bool", "Vector2", "Vector", "Vector4",
                "LinearColor", "Quat", "Position", "Matrix"
              - Niagara structs: "NiagaraID", "NiagaraRandInfo", "NiagaraSpawnInfo"
              - Enums: "ENiagaraExecutionState", any registered UEnum path
              - Data Interfaces (bare or prefixed):
                "ArrayFloat", "ArrayVector", "ArrayVector4", "ArrayInt32",
                "Curve", "Vector2DCurve", "VectorCurve", "Vector4Curve",
                "CurlNoise", "Grid2D", "Grid3D", "SkeletalMesh", "StaticMesh",
                "Texture", "TextureCube", "VolumeTexture", "Texture2DArray",
                "RenderTarget2D", "SplineComponent", "Camera", "Collision",
                "PhysicsAsset", "Spline", and any other NiagaraDataInterface subclass
                including plugin-provided ones.
        default_value: Optional default. For scalars pass as string or number;
            for vector/color/struct pass UE-standard text form like "(X=1,Y=2,Z=3)"
            or "(R=1,G=0.5,B=0,A=1)". Data interfaces use the CDO defaults — use
            set_niagara_user_parameter afterwards for richer initialization.

    Returns the created parameter with its resolved type name, kind
    (primitive / struct / enum / data_interface / object), size_bytes, and
    (for DIs) class_path.
    """
    params: dict = {
        "system_path": system_path,
        "parameter_name": parameter_name,
        "parameter_type": parameter_type,
    }
    if default_value is not None:
        params["default_value"] = default_value
    return _call("add_niagara_user_parameter", params)


@mcp.tool()
def set_niagara_user_parameter(
    parameter_name: str,
    parameter_type: str,
    value: str,
    actor_name: str | None = None,
    system_path: str | None = None,
) -> str:
    """Set a user parameter value on a Niagara System asset or level actor instance.

    Provide either system_path (asset default) or actor_name (runtime override).

    Args:
        parameter_name: Name of the user parameter
        parameter_type: Type of the parameter (must match existing type)
        value: Value as string
        actor_name: Name of an actor in the level for runtime override
        system_path: Path to the system asset for default value
    """
    params: dict = {
        "parameter_name": parameter_name,
        "parameter_type": parameter_type,
        "value": value,
    }
    if actor_name is not None:
        params["actor_name"] = actor_name
    if system_path is not None:
        params["system_path"] = system_path
    return _call("set_niagara_user_parameter", params)


@mcp.tool()
def remove_niagara_user_parameter(
    system_path: str,
    parameter_name: str,
) -> str:
    """Remove a user parameter from a Niagara System.

    Args:
        system_path: Path to the Niagara System asset
        parameter_name: Name of the parameter to remove
    """
    return _call("remove_niagara_user_parameter", {
        "system_path": system_path,
        "parameter_name": parameter_name,
    })


@mcp.tool()
def link_niagara_parameter(
    system_path: str,
    emitter_name: str,
    module_name: str,
    input_name: str,
    script_usage: str,
    linked_parameter: str,
) -> str:
    """Link a module input to a Niagara parameter (user, system, emitter, or particle scope).

    Args:
        system_path: Path to the Niagara System asset
        emitter_name: Name of the target emitter
        module_name: Name of the module
        input_name: Name of the input to link
        script_usage: Stack the module is in
        linked_parameter: Full parameter name including namespace
            (e.g. "User.MyParam", "Emitter.Age", "Particles.Velocity")
    """
    return _call("link_niagara_parameter", {
        "system_path": system_path,
        "emitter_name": emitter_name,
        "module_name": module_name,
        "input_name": input_name,
        "script_usage": script_usage,
        "linked_parameter": linked_parameter,
    })


# ---------------------------------------------------------------------------
# Renderers
# ---------------------------------------------------------------------------

@mcp.tool()
def add_niagara_renderer(
    system_path: str,
    emitter_name: str,
    renderer_type: str,
    material_path: str | None = None,
    mesh_path: str | None = None,
) -> str:
    """Add a renderer to an emitter.

    Args:
        system_path: Path to the Niagara System asset
        emitter_name: Name of the target emitter
        renderer_type: Renderer class — SpriteRenderer, MeshRenderer, RibbonRenderer,
            LightRenderer, ComponentRenderer
        material_path: Optional material asset path for sprite/mesh/ribbon renderers
        mesh_path: Optional static mesh path for MeshRenderer
    """
    params: dict = {
        "system_path": system_path,
        "emitter_name": emitter_name,
        "renderer_type": renderer_type,
    }
    if material_path is not None:
        params["material_path"] = material_path
    if mesh_path is not None:
        params["mesh_path"] = mesh_path
    return _call("add_niagara_renderer", params)


@mcp.tool()
def remove_niagara_renderer(
    system_path: str,
    emitter_name: str,
    renderer_index: int,
) -> str:
    """Remove a renderer from an emitter by index.

    Args:
        system_path: Path to the Niagara System asset
        emitter_name: Name of the target emitter
        renderer_index: Index of the renderer to remove (from get_niagara_emitters)
    """
    return _call("remove_niagara_renderer", {
        "system_path": system_path,
        "emitter_name": emitter_name,
        "renderer_index": renderer_index,
    })


@mcp.tool()
def get_niagara_renderer_info(
    system_path: str,
    emitter_name: str,
    renderer_index: int = 0,
) -> str:
    """Get detailed information about a renderer on an emitter.

    Args:
        system_path: Path to the Niagara System asset
        emitter_name: Name of the target emitter
        renderer_index: Index of the renderer (default 0, the first renderer)
    """
    return _call("get_niagara_renderer_info", {
        "system_path": system_path,
        "emitter_name": emitter_name,
        "renderer_index": renderer_index,
    })


@mcp.tool()
def set_niagara_renderer_property(
    system_path: str,
    emitter_name: str,
    property: str,
    value: str,
    renderer_index: int = 0,
) -> str:
    """Set a property on an emitter's renderer.

    Args:
        system_path: Path to the Niagara System asset
        emitter_name: Name of the target emitter
        property: Property name (e.g. "Material", "Alignment", "FacingMode", "SortOrder")
        value: Property value as string
        renderer_index: Index of the renderer (default 0)
    """
    return _call("set_niagara_renderer_property", {
        "system_path": system_path,
        "emitter_name": emitter_name,
        "property": property,
        "value": value,
        "renderer_index": renderer_index,
    })


@mcp.tool()
def set_niagara_renderer_binding(
    system_path: str,
    emitter_name: str,
    binding_name: str,
    attribute: str,
    renderer_index: int = 0,
) -> str:
    """Set an attribute binding on an emitter's renderer.

    Binds a renderer input (e.g. color, size) to a particle/emitter attribute.

    Args:
        system_path: Path to the Niagara System asset
        emitter_name: Name of the target emitter
        binding_name: Binding slot name (e.g. "ColorBinding", "DynamicMaterialBinding",
            "PositionBinding", "SpriteRotationBinding")
        attribute: Attribute to bind to (e.g. "Particles.Color", "Particles.Scale")
        renderer_index: Index of the renderer (default 0)
    """
    return _call("set_niagara_renderer_binding", {
        "system_path": system_path,
        "emitter_name": emitter_name,
        "binding_name": binding_name,
        "attribute": attribute,
        "renderer_index": renderer_index,
    })


# ---------------------------------------------------------------------------
# Scratch Pad & Module Assets
# ---------------------------------------------------------------------------

# Scratch pad + module asset + pin ops moved to the bottom of this file —
# see the "Scratch Pad Script Manager", "Custom HLSL pin management",
# "Node discovery & schema introspection", and "Parameter enumeration + pin
# operations" sections below. Those versions match the rewritten C++ handlers
# (template duplication, proper pin creation, etc.).


# ---------------------------------------------------------------------------
# Events & Simulation Stages
# ---------------------------------------------------------------------------

@mcp.tool()
def add_niagara_event_handler(
    system_path: str,
    emitter_name: str,
    source_emitter: str,
    event_name: str,
    execution_mode: str = "every_particle",
    spawn_number: int = 0,
    max_events_per_frame: int = 0,
) -> str:
    """Add an event handler to an emitter that responds to events from a source emitter.

    Args:
        system_path: Path to the Niagara System asset
        emitter_name: Name of the receiving emitter
        source_emitter: Name of the emitter producing the event
        event_name: Event name (e.g. "CollisionEvent", "DeathEvent", "LocationEvent")
        execution_mode: How events are handled — "every_particle", "spawn_particles",
            "single_particle"
        spawn_number: Number of particles to spawn per event (for spawn_particles mode)
        max_events_per_frame: Limit on events processed per frame (0 = unlimited)
    """
    return _call("add_niagara_event_handler", {
        "system_path": system_path,
        "emitter_name": emitter_name,
        "source_emitter": source_emitter,
        "event_name": event_name,
        "execution_mode": execution_mode,
        "spawn_number": spawn_number,
        "max_events_per_frame": max_events_per_frame,
    })


@mcp.tool()
def add_niagara_simulation_stage(
    system_path: str,
    emitter_name: str,
    stage_name: str,
    iteration_source: str = "particles",
    num_iterations: int = 1,
) -> str:
    """Add a simulation stage to an emitter.

    Simulation stages allow additional processing passes over particles or data interfaces.

    Args:
        system_path: Path to the Niagara System asset
        emitter_name: Name of the target emitter
        stage_name: Display name for the simulation stage
        iteration_source: What to iterate over — "particles" or a data interface name
        num_iterations: Number of times to execute this stage per frame
    """
    return _call("add_niagara_simulation_stage", {
        "system_path": system_path,
        "emitter_name": emitter_name,
        "stage_name": stage_name,
        "iteration_source": iteration_source,
        "num_iterations": num_iterations,
    })


@mcp.tool()
def get_niagara_event_handlers(
    system_path: str,
    emitter_name: str,
) -> str:
    """Get all event handlers configured on an emitter.

    Args:
        system_path: Path to the Niagara System asset
        emitter_name: Name of the target emitter
    """
    return _call("get_niagara_event_handlers", {
        "system_path": system_path,
        "emitter_name": emitter_name,
    })


# ---------------------------------------------------------------------------
# Runtime & Level
# ---------------------------------------------------------------------------

@mcp.tool()
def spawn_niagara_effect(
    system_path: str,
    location: str | None = None,
    rotation: str | None = None,
    scale: str | None = None,
    name: str | None = None,
    auto_activate: bool = True,
) -> str:
    """Spawn a Niagara System as an actor in the level.

    Args:
        system_path: Path to the Niagara System asset
        location: World location as "(X,Y,Z)" — defaults to origin
        rotation: Rotation as "(Pitch,Yaw,Roll)" — defaults to zero
        scale: Scale as "(X,Y,Z)" — defaults to (1,1,1)
        name: Optional actor label name
        auto_activate: If true, the system begins playing immediately
    """
    params: dict = {
        "system_path": system_path,
        "auto_activate": auto_activate,
    }
    if location is not None:
        params["location"] = location
    if rotation is not None:
        params["rotation"] = rotation
    if scale is not None:
        params["scale"] = scale
    if name is not None:
        params["name"] = name
    return _call("spawn_niagara_effect", params)


@mcp.tool()
def control_niagara_effect(
    actor_name: str,
    action: str,
) -> str:
    """Control a Niagara effect actor in the level.

    Args:
        actor_name: Name/label of the Niagara actor
        action: Control action — "activate", "deactivate", "reset", "destroy"
    """
    return _call("control_niagara_effect", {
        "actor_name": actor_name,
        "action": action,
    })


@mcp.tool()
def add_niagara_component(
    actor_name: str,
    system_path: str,
    component_name: str | None = None,
    relative_location: str | None = None,
    auto_activate: bool = True,
) -> str:
    """Add a NiagaraComponent to an existing actor in the level.

    Args:
        actor_name: Name/label of the target actor
        system_path: Path to the Niagara System asset to assign
        component_name: Optional name for the component
        relative_location: Relative offset as "(X,Y,Z)"
        auto_activate: If true, auto-activate on begin play
    """
    params: dict = {
        "actor_name": actor_name,
        "system_path": system_path,
        "auto_activate": auto_activate,
    }
    if component_name is not None:
        params["component_name"] = component_name
    if relative_location is not None:
        params["relative_location"] = relative_location
    return _call("add_niagara_component", params)


@mcp.tool()
def get_niagara_actors(
    system_filter: str | None = None,
) -> str:
    """Get all Niagara actors in the current level.

    Args:
        system_filter: Optional filter on the Niagara System name or path
    """
    params: dict = {}
    if system_filter is not None:
        params["system_filter"] = system_filter
    return _call("get_niagara_actors", params)


# ---------------------------------------------------------------------------
# Discovery
# ---------------------------------------------------------------------------

@mcp.tool()
def list_niagara_modules(
    category: str = "all",
    search: str | None = None,
    max_results: int = 100,
) -> str:
    """List available Niagara module scripts for use with add_niagara_module.

    Args:
        category: Filter by category — "all", "spawn", "update", "forces", "rendering",
            "collision", "audio", "events"
        search: Optional substring search on module name
        max_results: Maximum number of results to return
    """
    params: dict = {
        "category": category,
        "max_results": max_results,
    }
    if search is not None:
        params["search"] = search
    return _call("list_niagara_modules", params)


@mcp.tool()
def list_niagara_emitter_templates(
    category: str = "all",
) -> str:
    """List available emitter templates for use with add_niagara_emitter.

    Args:
        category: Filter by category — "all", "sprite", "mesh", "ribbon", "light", "audio"
    """
    return _call("list_niagara_emitter_templates", {
        "category": category,
    })


@mcp.tool()
def list_niagara_data_interfaces(
    filter: str | None = None,
) -> str:
    """List available Niagara Data Interfaces (Grid2D, Grid3D, AudioSpectrum, etc.).

    Args:
        filter: Optional substring filter on data interface name
    """
    params: dict = {}
    if filter is not None:
        params["filter"] = filter
    return _call("list_niagara_data_interfaces", params)


@mcp.tool()
def list_niagara_parameter_types(
    scope: str = "user",
    kind: str = "all",
    filter: str | None = None,
    max_results: int = 500,
) -> str:
    """Live query of FNiagaraTypeRegistry — every type the editor's User Parameters
    "+" menu can add, including plugin-registered Data Interfaces.

    Args:
        scope: Which registry slice to query. "user" (default) = types addable as
            User.* parameters. Other scopes: "system", "emitter", "particle",
            "parameter", "payload", "numeric", "index", "all".
        kind: Filter by classification — "primitive", "struct", "enum",
            "data_interface", "object", or "all" (default).
        filter: Case-insensitive substring match on type name. For data interfaces
            also matches against the short name (without the "NiagaraDataInterface" prefix),
            so filter="ArrayFloat" finds NiagaraDataInterfaceArrayFloat.
        max_results: Cap on returned entries (default 500).

    Returns entries with name, kind, size_bytes, plus:
        - data_interface: class_path, short_name, category, description
        - enum: enum_path, num_entries
        - struct: struct_path, description
    """
    params: dict = {"scope": scope, "kind": kind, "max_results": max_results}
    if filter is not None:
        params["filter"] = filter
    return _call("list_niagara_parameter_types", params)


@mcp.tool()
def get_niagara_emitter_attributes(
    system_path: str,
    emitter_name: str,
    scope: str = "particle",
    filter: str | None = None,
) -> str:
    """Get attributes defined on an emitter at a given scope.

    Returns name, type, scope, and source per attribute. Includes both rapid iteration
    parameters and well-known particle attributes (Position, Velocity, Color, etc.).
    Use filter to narrow by attribute name substring.

    Args:
        system_path: Path to the Niagara System asset
        emitter_name: Name of the target emitter
        scope: Attribute scope — "particle", "emitter", "system", "all"
        filter: Optional name filter (e.g. "Color", "Position", "Velocity", "Size")
    """
    params: dict = {
        "system_path": system_path,
        "emitter_name": emitter_name,
        "scope": scope,
    }
    if filter is not None:
        params["filter"] = filter
    return _call("get_niagara_emitter_attributes", params)


# ---------------------------------------------------------------------------
# Rapid Iteration Parameters
# ---------------------------------------------------------------------------

@mcp.tool()
def get_niagara_rapid_iteration_parameters(
    system_path: str,
    emitter_name: str,
    script_usage: str = "all",
    filter: str | None = None,
) -> str:
    """Get rapid iteration parameters (the actual configurable values on modules).

    These are the real module inputs — spawn rate, gravity, colors, sizes, etc.
    Returns parameter names, types, current values, and which module they belong to.

    Args:
        system_path: Path to the Niagara System asset
        emitter_name: Name of the target emitter
        script_usage: Stack filter — emitter_spawn, emitter_update, particle_spawn, particle_update, or all
        filter: Optional substring filter on parameter name (e.g. "Color", "SpawnRate", "Gravity")
    """
    params = {
        "system_path": system_path,
        "emitter_name": emitter_name,
        "script_usage": script_usage,
    }
    if filter:
        params["filter"] = filter
    return _call("get_niagara_rapid_iteration_parameters", params)


@mcp.tool()
def set_niagara_rapid_iteration_parameter(
    system_path: str,
    emitter_name: str,
    module_name: str,
    input_name: str,
    value: object,
    script_usage: str,
) -> str:
    """Set a rapid iteration parameter value on a module.

    This is the primary way to change module input values like spawn rate, colors,
    forces, sizes, lifetimes, etc. Use get_niagara_rapid_iteration_parameters first
    to discover available parameters and their types.

    Value format depends on type:
      float: 5.0
      int: 10
      bool: true/false
      vector: {"x": 1, "y": 2, "z": 3} or [1, 2, 3]
      color: {"r": 1, "g": 0.5, "b": 0, "a": 1} or [1, 0.5, 0, 1]

    Args:
        system_path: Path to the Niagara System asset
        emitter_name: Name of the target emitter
        module_name: Module name (e.g. "SpawnRate", "InitializeParticle", "GravityForce")
        input_name: Input parameter name (e.g. "SpawnRate", "Color", "Gravity")
        value: Value to set (type must match the parameter's type)
        script_usage: Stack the module is in — emitter_update, particle_spawn, particle_update, etc.
    """
    return _call("set_niagara_rapid_iteration_parameter", {
        "system_path": system_path,
        "emitter_name": emitter_name,
        "module_name": module_name,
        "input_name": input_name,
        "value": value,
        "script_usage": script_usage,
    })


# ---------------------------------------------------------------------------
# Renderer Property Discovery
# ---------------------------------------------------------------------------

@mcp.tool()
def get_niagara_renderer_properties(
    system_path: str,
    emitter_name: str,
    renderer_index: int = 0,
    filter: str | None = None,
) -> str:
    """Get editable properties of a renderer with current values and valid enum values.

    Token-efficient: use filter to get only specific properties instead of all.
    Property names can be used directly with set_niagara_renderer_property.

    Common filters: "Facing", "Material", "Sort", "Shadow", "Width", "Shape", "UV"

    Args:
        system_path: Path to the Niagara System asset
        emitter_name: Name of the target emitter
        renderer_index: Renderer index (default 0)
        filter: Substring filter on property name — strongly recommended to save tokens
    """
    params: dict = {
        "system_path": system_path,
        "emitter_name": emitter_name,
        "renderer_index": renderer_index,
    }
    if filter:
        params["filter"] = filter
    return _call("get_niagara_renderer_properties", params)


# ---------------------------------------------------------------------------
# System Properties
# ---------------------------------------------------------------------------

@mcp.tool()
def set_niagara_system_property(
    system_path: str,
    property: str,
    value: str,
) -> str:
    """Set a system-level property on a Niagara System via reflection.

    Supports any UPROPERTY on UNiagaraSystem: WarmupTime, WarmupTickDelta,
    bDeterminism, RandomSeed, bFixedBounds, FixedBounds, bFixedTickDelta,
    FixedTickDeltaTime, etc. Use exact C++ property name (PascalCase).

    Args:
        system_path: Path to the Niagara System asset
        property: Property name (e.g. "WarmupTime", "bDeterminism", "RandomSeed")
        value: Value as string (e.g. "2.0", "true", "42")
    """
    return _call("set_niagara_system_property", {
        "system_path": system_path,
        "property": property,
        "value": value,
    })


# ---------------------------------------------------------------------------
# Diagnostics & Timeline
# ---------------------------------------------------------------------------

@mcp.tool()
def get_niagara_system_errors(
    system_path: str,
    emitter_name: str | None = None,
    severity: str = "all",
) -> str:
    """Get compilation errors, warnings, and validation issues for a Niagara System.

    Returns issues found during compilation and validation. Use to diagnose why
    a system isn't working, find outdated modules, or check for configuration problems.

    Args:
        system_path: Path to the Niagara System asset
        emitter_name: Optional — filter to issues from a specific emitter
        severity: Filter by severity — "error", "warning", "info", or "all" (default)
    """
    params: dict = {
        "system_path": system_path,
        "severity": severity,
    }
    if emitter_name is not None:
        params["emitter_name"] = emitter_name
    return _call("get_niagara_system_errors", params)


@mcp.tool()
def get_niagara_particle_stats(
    system_path: str,
    emitter_name: str | None = None,
) -> str:
    """Get live particle counts and emitter execution state from running preview.

    Shows how many particles are alive per emitter, total spawned count,
    execution state (Active/Inactive/Complete), and bounds info.
    Requires the system to be previewing in the Niagara editor or spawned in level.

    Args:
        system_path: Path to the Niagara System asset
        emitter_name: Optional — filter to a specific emitter's stats
    """
    params: dict = {
        "system_path": system_path,
    }
    if emitter_name is not None:
        params["emitter_name"] = emitter_name
    return _call("get_niagara_particle_stats", params)


@mcp.tool()
def set_niagara_playback_range(
    system_path: str,
    range_end: float,
    range_start: float = 0.0,
    frame_rate: int | None = None,
) -> str:
    """Set the timeline playback range in the Niagara editor.

    Controls how long the preview plays. If your effect dies at 0.02s, extend
    the range. For looping effects set a longer range (e.g. 5-10 seconds).

    Args:
        system_path: Path to the Niagara System asset
        range_end: End time in seconds (e.g. 5.0 for 5 second preview)
        range_start: Start time in seconds (default 0.0)
        frame_rate: Optional frame rate override (default 60)
    """
    params: dict = {
        "system_path": system_path,
        "range_end": range_end,
        "range_start": range_start,
    }
    if frame_rate is not None:
        params["frame_rate"] = frame_rate
    return _call("set_niagara_playback_range", params)


@mcp.tool()
def get_niagara_playback_range(
    system_path: str,
) -> str:
    """Get the current timeline playback range and frame rate settings.

    Returns the start/end time in seconds and the frame rate configuration.
    Use to check if the playback range is too short (common cause of effects
    appearing to not play).

    Args:
        system_path: Path to the Niagara System asset
    """
    return _call("get_niagara_playback_range", {
        "system_path": system_path,
    })


@mcp.tool()
def get_niagara_module_versions(
    system_path: str,
    emitter_name: str,
    filter: str | None = None,
) -> str:
    """Check for outdated modules in an emitter and their available versions.

    Identifies modules that have newer versions available — a common source of
    Niagara warnings and compatibility issues. Use to find modules that need upgrading.

    Args:
        system_path: Path to the Niagara System asset
        emitter_name: Name of the target emitter
        filter: Optional module name filter (e.g. "Spawn", "Color", "Force")
    """
    params: dict = {
        "system_path": system_path,
        "emitter_name": emitter_name,
    }
    if filter is not None:
        params["filter"] = filter
    return _call("get_niagara_module_versions", params)


# ---------------------------------------------------------------------------
# Scratch Pad Script Manager
# ---------------------------------------------------------------------------

@mcp.tool()
def create_niagara_scratch_pad_module(
    system_path: str,
    module_name: str = "ScratchPadModule",
    module_type: str = "module",
) -> str:
    """Create a new scratch pad script on a Niagara System.

    Duplicates the editor's default template (DefaultModuleScript /
    DefaultDynamicInputScript / DefaultFunctionScript from NiagaraEditorSettings) so
    the resulting graph is pre-wired with Input / MapGet / MapSet / Output just like
    clicking "Create New Module" in the Scratch Script Manager. Falls back to a
    manually-built minimal graph if the template asset is unavailable.

    Args:
        system_path: Niagara System asset path
        module_name: Desired scratch script name (unique within the system's ScratchPadScripts)
        module_type: "module" (default), "dynamic_input", or "function"
    """
    return _call("create_niagara_scratch_pad_module", {
        "system_path": system_path,
        "module_name": module_name,
        "module_type": module_type,
    })


@mcp.tool()
def duplicate_niagara_scratch_pad_module(
    system_path: str,
    module_name: str,
    new_name: str | None = None,
) -> str:
    """Duplicate an existing scratch pad module on a system.

    Args:
        system_path: Niagara System asset path
        module_name: Source scratch pad module name
        new_name: Optional new name (defaults to "<source>_Copy")
    """
    params: dict = {"system_path": system_path, "module_name": module_name}
    if new_name is not None:
        params["new_name"] = new_name
    return _call("duplicate_niagara_scratch_pad_module", params)


@mcp.tool()
def delete_niagara_scratch_pad_module(system_path: str, module_name: str) -> str:
    """Delete a scratch pad module from a Niagara System."""
    return _call("delete_niagara_scratch_pad_module", {
        "system_path": system_path,
        "module_name": module_name,
    })


@mcp.tool()
def rename_niagara_scratch_pad_module(
    system_path: str, module_name: str, new_name: str
) -> str:
    """Rename a scratch pad module on a Niagara System."""
    return _call("rename_niagara_scratch_pad_module", {
        "system_path": system_path,
        "module_name": module_name,
        "new_name": new_name,
    })


@mcp.tool()
def get_niagara_graph_nodes(
    system_path: str | None = None,
    module_name: str | None = None,
    script_path: str | None = None,
    emitter_name: str | None = None,
    script_usage: str | None = None,
    verbosity: str = "connections",
    type_filter: str | None = None,
    name_filter: str | None = None,
) -> str:
    """Introspect every node inside a Niagara graph.

    Three resolver modes:
      1. system_path + module_name        — scratch pad module graph
      2. system_path + emitter_name + script_usage — EMITTER STACK GRAPH
         (emitter_spawn|emitter_update|particle_spawn|particle_update),
         where stack modules appear as NiagaraNodeFunctionCall nodes
      3. script_path                      — standalone UNiagaraScript asset

    Args:
        system_path: Niagara System asset path
        module_name: Scratch pad module name (mode 1)
        emitter_name + script_usage: Emitter stack graph (mode 2)
        script_path: Standalone script asset (mode 3)
        verbosity: "summary" | "connections" (default) | "full"
        type_filter: Case-insensitive substring on short class name
        name_filter: Case-insensitive substring on node title
    """
    params: dict = {"verbosity": verbosity}
    if system_path:
        params["system_path"] = system_path
    if module_name:
        params["module_name"] = module_name
    if script_path:
        params["script_path"] = script_path
    if emitter_name:
        params["emitter_name"] = emitter_name
    if script_usage:
        params["script_usage"] = script_usage
    if type_filter:
        params["type_filter"] = type_filter
    if name_filter:
        params["name_filter"] = name_filter
    return _call("get_niagara_graph_nodes", params)


@mcp.tool()
def get_niagara_node_info(
    system_path: str | None = None,
    module_name: str | None = None,
    script_path: str | None = None,
    node_index: int | None = None,
    node_class: str | None = None,
    node_id: str | None = None,
) -> str:
    """Deep inspect a single Niagara node: full pin layout, connections, type-specific detail.

    Provide exactly one of node_index / node_class / node_id.

    Args:
        system_path + module_name: Resolve scratch pad module (one option)
        script_path: Resolve standalone script asset (alternative)
        node_index: Ordinal index into Graph->Nodes (stable per session)
        node_class: Short ("MapGet") or full ("NiagaraNodeParameterMapGet")
        node_id: FGuid string matching UEdGraphNode::NodeGuid
    """
    params: dict = {}
    if system_path:
        params["system_path"] = system_path
    if module_name:
        params["module_name"] = module_name
    if script_path:
        params["script_path"] = script_path
    if node_index is not None:
        params["node_index"] = node_index
    if node_class:
        params["node_class"] = node_class
    if node_id:
        params["node_id"] = node_id
    return _call("get_niagara_node_info", params)


@mcp.tool()
def trace_niagara_connection(
    system_path: str | None = None,
    module_name: str | None = None,
    script_path: str | None = None,
    node_index: int | None = None,
    node_class: str | None = None,
    node_id: str | None = None,
    direction: str = "both",
    max_depth: int = 8,
    pin_name: str | None = None,
) -> str:
    """Breadth-first trace of connections from a starting node through the graph.

    Answers "what feeds this node?" (upstream) and "where does this node's
    output go?" (downstream). Each visited node reports its depth, so the
    dependency chain is visible without dumping the whole graph.

    Args:
        system_path + module_name OR script_path: Graph resolver
        node_index / node_class / node_id: Starting node identifier
        direction: "upstream" | "downstream" | "both" (default)
        max_depth: Max BFS depth (default 8)
        pin_name: Optional starting-pin filter — only walk links on pins whose
                  name contains this substring (e.g. "Vector Array")
    """
    params: dict = {"direction": direction, "max_depth": max_depth}
    if system_path:
        params["system_path"] = system_path
    if module_name:
        params["module_name"] = module_name
    if script_path:
        params["script_path"] = script_path
    if node_index is not None:
        params["node_index"] = node_index
    if node_class:
        params["node_class"] = node_class
    if node_id:
        params["node_id"] = node_id
    if pin_name:
        params["pin_name"] = pin_name
    return _call("trace_niagara_connection", params)


@mcp.tool()
def validate_niagara_graph(
    system_path: str | None = None,
    module_name: str | None = None,
    script_path: str | None = None,
) -> str:
    """Classify orphaned, dead-end, and missing-input nodes in a Niagara graph.

    Returns three arrays:
      orphaned       — nodes with no incoming or outgoing links (excl. anchors)
      dead_ends      — non-anchor nodes with inputs connected but no outputs consumed
      missing_inputs — nodes whose input pins have no link and no default value
    """
    params: dict = {}
    if system_path:
        params["system_path"] = system_path
    if module_name:
        params["module_name"] = module_name
    if script_path:
        params["script_path"] = script_path
    return _call("validate_niagara_graph", params)


@mcp.tool()
def list_niagara_scratch_pad_modules(system_path: str) -> str:
    """List all scratch pad modules on a Niagara System.

    Returns name, path, usage, node counts, and custom HLSL node count per script.
    """
    return _call("list_niagara_scratch_pad_modules", {"system_path": system_path})


@mcp.tool()
def apply_niagara_scratch_pad(system_path: str, module_name: str) -> str:
    """Commit a scratch pad module's edit-copy to the original script (Apply button).

    Mirrors FNiagaraScratchPadScriptViewModel::ApplyChanges. Requires the
    Niagara System to be open in the editor so the view model exists.
    """
    return _call("apply_niagara_scratch_pad", {
        "system_path": system_path,
        "module_name": module_name,
    })


@mcp.tool()
def apply_and_save_niagara_scratch_pad(system_path: str, module_name: str) -> str:
    """Apply a scratch pad module's edit-copy AND save the asset (Apply & Save button).

    Mirrors FNiagaraScratchPadScriptViewModel::ApplyChangesAndSave.
    """
    return _call("apply_and_save_niagara_scratch_pad", {
        "system_path": system_path,
        "module_name": module_name,
    })


@mcp.tool()
def get_niagara_script_properties(
    system_path: str | None = None,
    module_name: str | None = None,
    script_path: str | None = None,
) -> str:
    """Read the details-panel properties of a Niagara script.

    Returns Category, Description, Keywords, ModuleUsageBitmask,
    ProvidedDependencies, RequiredDependencies, LibraryVisibility,
    bDeprecated, DeprecationMessage, bExperimental, ExperimentalMessage,
    NumericOutputTypeSelectionMode, ScriptMetaData, ConversionUtility.

    All properties live on FVersionedNiagaraScriptData; output shape matches
    NiagaraIntrospection::SerializeProperty (kind + value + schema fields).
    """
    params: dict = {}
    if system_path:
        params["system_path"] = system_path
    if module_name:
        params["module_name"] = module_name
    if script_path:
        params["script_path"] = script_path
    return _call("get_niagara_script_properties", params)


@mcp.tool()
def set_niagara_script_properties(
    properties: dict,
    system_path: str | None = None,
    module_name: str | None = None,
    script_path: str | None = None,
) -> str:
    """Batch-set details-panel properties on a Niagara script.

    Args:
        properties: Dict of property name -> value. Supported:
            Category (str), Description (str), Keywords (str),
            ModuleUsageBitmask (int), ProvidedDependencies (list[str]),
            RequiredDependencies (list[dict]), LibraryVisibility (str enum),
            bDeprecated (bool), DeprecationMessage (str),
            bExperimental (bool), ExperimentalMessage (str).
        system_path + module_name: Resolve scratch pad module
        script_path: OR resolve standalone script asset

    ModuleUsageBitmask combines ENiagaraScriptUsage values; e.g. particle
    spawn + update = (1<<3) | (1<<4).
    """
    params: dict = {"properties": properties}
    if system_path:
        params["system_path"] = system_path
    if module_name:
        params["module_name"] = module_name
    if script_path:
        params["script_path"] = script_path
    return _call("set_niagara_script_properties", params)


@mcp.tool()
def list_niagara_script_parameters(
    system_path: str | None = None,
    module_name: str | None = None,
    script_path: str | None = None,
) -> str:
    """List input + output parameters of a Niagara script.

    Outputs come from UNiagaraNodeOutput::Outputs (the Output Dynamic Input
    node for dynamic inputs, or Output Module for modules).
    Inputs come from graph script variable metadata filtered to
    Module.* / Input.* / User.* namespaces.

    Useful for discovering what an "Add Parameter" dropdown would show or
    for diff-auditing a graph before/after mutation.
    """
    params: dict = {}
    if system_path:
        params["system_path"] = system_path
    if module_name:
        params["module_name"] = module_name
    if script_path:
        params["script_path"] = script_path
    return _call("list_niagara_script_parameters", params)


@mcp.tool()
def add_niagara_script_parameter(
    name: str,
    type: str,
    direction: str = "output",
    system_path: str | None = None,
    module_name: str | None = None,
    script_path: str | None = None,
) -> str:
    """Add an input or output parameter to a Niagara script.

    For a dynamic input, "output" adds a new entry to the Output Dynamic
    Input node (screenshot 5's "Add Parameter" dropdown). Type must be a
    name registered in FNiagaraTypeRegistry — use list_niagara_parameter_types
    to discover valid options (e.g. "float", "Vector", "LinearColor", "Quat",
    "Position", "NiagaraID", "Matrix").

    Args:
        name: Parameter name (input names without namespace prefix become Module.<name>)
        type: Niagara type name
        direction: "output" (default — appends to output node) | "input"
    """
    params: dict = {"name": name, "type": type, "direction": direction}
    if system_path:
        params["system_path"] = system_path
    if module_name:
        params["module_name"] = module_name
    if script_path:
        params["script_path"] = script_path
    return _call("add_niagara_script_parameter", params)


@mcp.tool()
def remove_niagara_script_parameter(
    name: str,
    direction: str = "output",
    system_path: str | None = None,
    module_name: str | None = None,
    script_path: str | None = None,
) -> str:
    """Remove a named input or output parameter from a Niagara script."""
    params: dict = {"name": name, "direction": direction}
    if system_path:
        params["system_path"] = system_path
    if module_name:
        params["module_name"] = module_name
    if script_path:
        params["script_path"] = script_path
    return _call("remove_niagara_script_parameter", params)


@mcp.tool()
def rename_niagara_script_parameter(
    old_name: str,
    new_name: str,
    direction: str = "output",
    system_path: str | None = None,
    module_name: str | None = None,
    script_path: str | None = None,
) -> str:
    """Rename a script parameter in-place across both asset and edit-copy graphs."""
    params: dict = {"old_name": old_name, "new_name": new_name, "direction": direction}
    if system_path:
        params["system_path"] = system_path
    if module_name:
        params["module_name"] = module_name
    if script_path:
        params["script_path"] = script_path
    return _call("rename_niagara_script_parameter", params)


@mcp.tool()
def add_niagara_graph_node(
    node_type: str,
    system_path: str | None = None,
    module_name: str | None = None,
    script_path: str | None = None,
    pos_x: int = 0,
    pos_y: int = 0,
    op_name: str | None = None,
    function_script: str | None = None,
    input_name: str | None = None,
    input_type: str | None = None,
    di_class: str | None = None,
    function_name: str | None = None,
) -> str:
    """Create a new node inside a Niagara scratch-pad / dynamic-input / module graph.

    Mirrors the node-creation pattern used by the Niagara editor schema actions
    (FGraphNodeCreator + AllocateDefaultPins + NotifyGraphChanged). Spawns on
    both asset and edit-copy graphs when a system is open.

    Args:
        node_type: One of "Op", "FunctionCall", "DataInterfaceFunction",
                   "ParameterMapGet", "ParameterMapSet", "Reroute", "Input"
        pos_x, pos_y: Graph position
        op_name: (node_type=Op) — discover via get_niagara_schema_actions
                 (e.g. "Numeric::Add", "Numeric::Mul", "Numeric::Length")
        function_script: (node_type=FunctionCall) UNiagaraScript asset path
        input_name + input_type: (node_type=Input) variable name + Niagara type
        di_class + function_name: (node_type=DataInterfaceFunction) — wraps a
                 data-interface member function. Discover via
                 list_niagara_data_interface_functions. Examples:
                 di_class="NiagaraDataInterfaceArrayPosition", function_name="Length"

    Use list_niagara_script_parameters after to verify the pin layout.
    """
    params: dict = {"node_type": node_type, "pos_x": pos_x, "pos_y": pos_y}
    if system_path:
        params["system_path"] = system_path
    if module_name:
        params["module_name"] = module_name
    if script_path:
        params["script_path"] = script_path
    if op_name:
        params["op_name"] = op_name
    if function_script:
        params["function_script"] = function_script
    if input_name:
        params["input_name"] = input_name
    if input_type:
        params["input_type"] = input_type
    if di_class:
        params["di_class"] = di_class
    if function_name:
        params["function_name"] = function_name
    return _call("add_niagara_graph_node", params)


@mcp.tool()
def list_niagara_data_interface_functions(
    di_class: str,
    filter: str | None = None,
    include_pins: bool = True,
) -> str:
    """Enumerate member functions on a Niagara data interface class.

    These are the functions you see in the right-click "Functions" submenu
    on a DI pin in the editor — built-in DI members like Array.Length,
    Array.Get, Array.Add, etc. They aren't UNiagaraScript assets so they
    don't show up in search_niagara_functions. Pass the result as
    function_name to add_niagara_graph_node(node_type="DataInterfaceFunction").

    Each entry reports:
      - name: pass to function_name param
      - inputs / outputs: pin schema with names + Niagara types
      - supports_cpu / supports_gpu / read_function / write_function
      - description (when DI provides it)

    Authoritative source: UNiagaraDataInterface::GetFunctionSignatures
    (NIAGARA_API exported, NiagaraDataInterface.h:681).

    Args:
        di_class: Short ("NiagaraDataInterfaceArrayPosition") or full path
        filter: Optional substring on function name (e.g. "Length", "Get")
        include_pins: Default True — include input/output pin schema
    """
    params: dict = {"di_class": di_class, "include_pins": include_pins}
    if filter:
        params["filter"] = filter
    return _call("list_niagara_data_interface_functions", params)


@mcp.tool()
def delete_niagara_graph_node(
    system_path: str | None = None,
    module_name: str | None = None,
    script_path: str | None = None,
    node_index: int | None = None,
    node_id: str | None = None,
) -> str:
    """Delete a node from a Niagara graph (asset + edit-copy).

    Provide node_index (ordinal) OR node_id (FGuid string from
    get_niagara_graph_nodes).
    """
    params: dict = {}
    if system_path:
        params["system_path"] = system_path
    if module_name:
        params["module_name"] = module_name
    if script_path:
        params["script_path"] = script_path
    if node_index is not None:
        params["node_index"] = node_index
    if node_id:
        params["node_id"] = node_id
    return _call("delete_niagara_graph_node", params)


@mcp.tool()
def list_niagara_ops(
    filter: str | None = None,
    category: str | None = None,
    exact_name: str | None = None,
    include_pins: bool = True,
    max_results: int = 500,
) -> str:
    """Enumerate all valid UNiagaraNodeOp operations from FNiagaraOpInfo registry.

    This is the authoritative source the editor's context menu reads from —
    use it BEFORE calling add_niagara_graph_node with node_type="Op" so you
    pass a real op_name (e.g. "Numeric::Mul" vs the invalid "Numeric::Multiply").

    Each entry reports: name (for op_name param), friendly_name, category,
    description, keywords, alternate_name, compact_name, supports_added_inputs,
    and — when include_pins=True — full input/output pin schema with types
    and defaults.

    Args:
        filter: Case-insensitive substring, matches name / friendly_name /
                category / description / keywords / alternate name. E.g.
                "multiply" finds "Numeric::Mul" via its friendly name.
        category: Exact category match (e.g. "Math", "Comparison", "Boolean",
                  "Vector", "Quaternion"). Use the all_categories field in a
                  first response to discover valid values.
        exact_name: Return the single op whose internal Name equals this
                    string (bypasses other filters).
        include_pins: Include inputs/outputs per op (default True).
        max_results: Cap on returned entries (default 500).
    """
    params: dict = {"include_pins": include_pins, "max_results": max_results}
    if filter:
        params["filter"] = filter
    if category:
        params["category"] = category
    if exact_name:
        params["exact_name"] = exact_name
    return _call("list_niagara_ops", params)


@mcp.tool()
def get_niagara_module_input_binding(
    system_path: str,
    emitter_name: str,
    module_name: str,
    script_usage: str,
    input_filter: str | None = None,
    max_depth: int = 3,
) -> str:
    """Resolve the actual binding of each module input — Default/Local/Linked/Dynamic/Data.

    This answers "what is Spawn Count actually driven by?" — something
    get_niagara_module_inputs + get_niagara_rapid_iteration_parameters can't
    do on their own. Returns per-input:
      - mode: default | local | linked | dynamic | data | function_call | expression | unknown
      - value: literal string when mode=local
      - linked_parameter: full parameter name when mode=linked/data
      - script_path + function_name: when mode=dynamic/function_call
      - children: recursive — for dynamic inputs, lists their own bound inputs

    Uses FNiagaraStackGraphUtilities::GetStackFunctionInputs (exported) and
    replicates GetStackFunctionInputOverridePin manually (non-exported).

    Args:
        system_path: Niagara System asset
        emitter_name: Emitter in the system
        module_name: Module display name (e.g. "SpawnPerFrame")
        script_usage: emitter_spawn|emitter_update|particle_spawn|particle_update|
                      system_spawn|system_update
        input_filter: Case-insensitive substring on input name
        max_depth: Recursive descent cap for dynamic-input children (default 3)
    """
    params: dict = {
        "system_path": system_path,
        "emitter_name": emitter_name,
        "module_name": module_name,
        "script_usage": script_usage,
        "max_depth": max_depth,
    }
    if input_filter:
        params["input_filter"] = input_filter
    return _call("get_niagara_module_input_binding", params)


@mcp.tool()
def clear_niagara_module_input(
    system_path: str,
    emitter_name: str,
    module_name: str,
    script_usage: str,
    input_name: str,
) -> str:
    """Reset a module input to its default — equivalent to "Reset to Default" in the stack UI.

    Finds the override pin, breaks its connections, removes any orphan
    nodes that were exclusively feeding it (dynamic input node, map get,
    custom HLSL, etc.), then removes the override pin itself from the
    ParameterMapSet so the input reverts cleanly.

    Supports nested path syntax: input_name="Spawn Count.Position Array"
    clears the Position Array binding INSIDE the dynamic input attached
    to Spawn Count — without touching Spawn Count's own binding.

    Mirrors FNiagaraStackGraphUtilities::RemoveNodesForStackFunctionInputOverridePin
    (non-exported — replicated here).
    """
    return _call("clear_niagara_module_input", {
        "system_path": system_path,
        "emitter_name": emitter_name,
        "module_name": module_name,
        "script_usage": script_usage,
        "input_name": input_name,
    })


@mcp.tool()
def list_niagara_input_source_menu(
    system_path: str,
    emitter_name: str,
    module_name: str,
    script_usage: str,
    input_name: str,
    name_filter: str | None = None,
) -> str:
    """Reproduce the stack-UI source dropdown for a specific input.

    Returns the exact options the editor offers when you click the dropdown
    on a stack input (screenshot reference: "Change Source" menu with
    Dynamic Inputs / Link Inputs / Make sections). Output:
      - input_type: the Niagara type of the target input (so you can filter)
      - dynamic_inputs: every UNiagaraScript with Usage=DynamicInput,
                        with script_path ready to pass to set_niagara_dynamic_input
      - link_parameters: well-known engine/particle parameters + this system's
                         user parameters, grouped by namespace

    Discovery is via:
      - FNiagaraEditorUtilities::GetFilteredScriptAssets (exported) for dynamic inputs
      - FNiagaraConstants::GetEngineConstants / GetCommonParticleAttributes for
        engine-side parameters
      - UNiagaraSystem::GetExposedParameters for the system's own user params
    """
    params: dict = {
        "system_path": system_path,
        "emitter_name": emitter_name,
        "module_name": module_name,
        "script_usage": script_usage,
        "input_name": input_name,
    }
    if name_filter:
        params["name_filter"] = name_filter
    return _call("list_niagara_input_source_menu", params)


@mcp.tool()
def find_niagara_scratch_pad_usage(system_path: str, module_name: str) -> str:
    """Reverse lookup: find where a scratch pad script is referenced in the stack.

    Scans every emitter/system script graph (system_spawn, system_update,
    emitter_spawn, emitter_update, particle_spawn, particle_update) for
    UNiagaraNodeFunctionCall nodes whose FunctionScript matches the named
    scratch pad. Returns each site as {emitter, script_usage, function_name,
    node_id, is_dynamic_input}.

    Use this to answer "what uses GetDataInterfaceLength?" without walking
    every emitter manually.
    """
    return _call("find_niagara_scratch_pad_usage", {
        "system_path": system_path,
        "module_name": module_name,
    })


@mcp.tool()
def resolve_niagara_built_in_dynamic_input(
    name_filter: str | None = None,
    exact_name: str | None = None,
    max_results: int = 20,
) -> str:
    """Discover built-in dynamic-input script asset paths via AssetRegistry.

    Replaces guessed paths like "/Niagara/Modules/DynamicInputs/UniformRangedFloat"
    (which vary across UE versions) with a live scan. Returns all UNiagaraScript
    assets with Usage=DynamicInput matching the filter.

    Args:
        name_filter: Case-insensitive substring on asset name (e.g. "Random",
                     "UniformRanged", "Mask", "MultiplyVector")
        exact_name: If set, return only the asset whose name equals this
        max_results: Cap on returned entries (default 20)
    """
    params: dict = {"max_results": max_results}
    if name_filter:
        params["name_filter"] = name_filter
    if exact_name:
        params["exact_name"] = exact_name
    return _call("resolve_niagara_built_in_dynamic_input", params)


@mcp.tool()
def create_niagara_module_asset(
    asset_path: str,
    module_type: str = "module",
    description: str | None = None,
) -> str:
    """Create a standalone Niagara script asset (module / dynamic input / function).

    Duplicates the editor's default template when available so the produced asset
    has the same initial graph as one created via the Niagara asset wizard.

    Args:
        asset_path: Full content path including name (e.g. "/Game/FX/Modules/MyModule")
        module_type: "module", "dynamic_input", or "function"
        description: Optional description text
    """
    params: dict = {"asset_path": asset_path, "module_type": module_type}
    if description is not None:
        params["description"] = description
    return _call("create_niagara_module_asset", params)


@mcp.tool()
def set_niagara_scratch_pad_hlsl(
    system_path: str,
    module_name: str,
    hlsl_code: str,
    inputs: list[dict] | None = None,
    outputs: list[dict] | None = None,
    clear_existing_pins: bool = False,
) -> str:
    """Set HLSL source on a scratch pad module's Custom HLSL node, creating pins as needed.

    Pin creation uses exported schema APIs (UEdGraphSchema_Niagara::TypeDefinitionToPinType)
    plus reflection on the CustomHlsl UPROPERTY (SetCustomHlsl is not NIAGARAEDITOR_API
    exported). After changes the node's FNiagaraFunctionSignature is rebuilt from the
    pin list, mirroring UNiagaraNodeCustomHlsl::RebuildSignatureFromPins exactly.

    Args:
        system_path: Niagara System asset path
        module_name: Scratch pad module name containing the Custom HLSL node
        hlsl_code: Raw HLSL source. Reference pins by name (e.g. Result = Value * Scale;)
        inputs: Optional list of {"name": str, "type": str} — input pins to create
        outputs: Optional list of {"name": str, "type": str} — output pins to create
        clear_existing_pins: If true, remove all current non-Add pins before adding new ones
    """
    params: dict = {
        "system_path": system_path,
        "module_name": module_name,
        "hlsl_code": hlsl_code,
        "clear_existing_pins": clear_existing_pins,
    }
    if inputs is not None:
        params["inputs"] = inputs
    if outputs is not None:
        params["outputs"] = outputs
    return _call("set_niagara_scratch_pad_hlsl", params)


# ---------------------------------------------------------------------------
# Custom HLSL pin management
# ---------------------------------------------------------------------------

@mcp.tool()
def add_niagara_custom_hlsl_input(
    system_path: str, module_name: str, pin_name: str, pin_type: str = "float"
) -> str:
    """Add a typed input pin to a scratch pad module's Custom HLSL node.

    Types: float, int, bool, vec2/3/4, color, quat, matrix, position, ParameterMap,
    or any registered Niagara type. The pin's name is usable inside the HLSL source.
    """
    return _call("add_niagara_custom_hlsl_input", {
        "system_path": system_path,
        "module_name": module_name,
        "pin_name": pin_name,
        "pin_type": pin_type,
    })


@mcp.tool()
def add_niagara_custom_hlsl_output(
    system_path: str, module_name: str, pin_name: str, pin_type: str = "float"
) -> str:
    """Add a typed output pin to a scratch pad module's Custom HLSL node."""
    return _call("add_niagara_custom_hlsl_output", {
        "system_path": system_path,
        "module_name": module_name,
        "pin_name": pin_name,
        "pin_type": pin_type,
    })


@mcp.tool()
def rename_niagara_custom_hlsl_pin(
    system_path: str, module_name: str, old_name: str, new_name: str
) -> str:
    """Rename a Custom HLSL pin and update references inside the HLSL source.

    Braced references ("{OldName}") inside the HLSL code are rewritten to "{NewName}"
    via a whole-word replacement after the pin is renamed.
    """
    return _call("rename_niagara_custom_hlsl_pin", {
        "system_path": system_path,
        "module_name": module_name,
        "old_name": old_name,
        "new_name": new_name,
    })


@mcp.tool()
def remove_niagara_custom_hlsl_pin(
    system_path: str, module_name: str, pin_name: str
) -> str:
    """Remove a pin from a Custom HLSL node and rebuild its signature."""
    return _call("remove_niagara_custom_hlsl_pin", {
        "system_path": system_path,
        "module_name": module_name,
        "pin_name": pin_name,
    })


# ---------------------------------------------------------------------------
# Node discovery & schema introspection
# ---------------------------------------------------------------------------

@mcp.tool()
def list_niagara_node_types(
    filter: str | None = None,
    kind: str = "all",
    include_engine: bool = False,
    max_results: int = 500,
) -> str:
    """Enumerate every Niagara node type that can be added to a graph.

    Combines three sources, mirroring what the right-click menu shows in the editor:
      * Node classes (UNiagaraNode subclasses) — Map Get, Map Set, Custom HLSL, If, etc.
      * Script assets — module / dynamic_input / function scripts from the Asset Registry
      * Data Interface classes (UNiagaraDataInterface subclasses)

    Filter-first to keep output compact. Each entry includes kind, display_name,
    category, tooltip/keywords, and a class_path or script_path for use with
    add_niagara_module / add_niagara_node_pin.

    Args:
        filter: Case-insensitive substring applied to name/display/category/keywords/tooltip
        kind: "all" (default), "node_class", "module_script", "dynamic_input_script",
              "function_script", "data_interface"
        include_engine: When false, script asset scan is restricted to /Game and /Niagara
        max_results: Cap on total entries returned (default 500)
    """
    params: dict = {"kind": kind, "include_engine": include_engine, "max_results": max_results}
    if filter is not None:
        params["filter"] = filter
    return _call("list_niagara_node_types", params)


@mcp.tool()
def get_niagara_node_type_info(type: str, script_path: str | None = None) -> str:
    """Get pin/property schema for a Niagara node type or script asset.

    For a node class (e.g. "CustomHlsl", "ParameterMapGet"), returns the editable
    UPROPERTY list via reflection. For CustomHlsl specifically, returns the full
    HLSL schema (like Material's get_expression_type_info does for the Custom node)
    with an example payload.

    For a script_path, loads the asset and returns its input/output parameters.

    Args:
        type: Node short name (e.g. "CustomHlsl", "ParameterMapGet") or full class name
        script_path: Optional script asset path — when set, returns script input/output info instead
    """
    params: dict = {"type": type}
    if script_path is not None:
        params["script_path"] = script_path
    return _call("get_niagara_node_type_info", params)


@mcp.tool()
def search_niagara_functions(
    filter: str | None = None,
    usage: str = "function",
    include_engine: bool = True,
    max_results: int = 100,
) -> str:
    """Search Niagara script assets by usage + name filter.

    Dedicated shortcut for finding module / dynamic_input / function script assets
    so you can pass the result directly to add_niagara_module or set_niagara_dynamic_input.

    Args:
        filter: Optional name/path substring
        usage: "module", "dynamic_input", or "function" (default)
        include_engine: When false, restricts scan to /Game
        max_results: Cap on returned entries
    """
    params: dict = {"usage": usage, "include_engine": include_engine, "max_results": max_results}
    if filter is not None:
        params["filter"] = filter
    return _call("search_niagara_functions", params)


@mcp.tool()
def describe_niagara_type(type: str) -> str:
    """Return the full schema of any Niagara type — primitive, enum, struct, or data interface.

    Built on a generic FProperty introspector, so the output handles every
    Unreal property kind recursively (bool / int{8,16,32,64} / uint{16,32,64} /
    float / double / string / name / text / enum / struct / array / map / set /
    object / class / soft_object / soft_class / interface / delegate). For
    enums, returns every entry with name + display_name + value + tooltip. For
    structs, walks every editable field. For data interfaces, returns the full
    CDO schema.

    Args:
        type: Type identifier — built-in name ("float", "vec3", "Color"),
              registered Niagara type, UEnum / UScriptStruct path, or
              UNiagaraDataInterface subclass name (with or without the U prefix).
    """
    return _call("describe_niagara_type", {"type": type})


@mcp.tool()
def get_niagara_data_interface_schema(class_: str) -> str:
    """Return the full editable-property schema of a UNiagaraDataInterface subclass.

    Walks the CDO via the modular FProperty introspector. Covers every DI:
    Array{Float,Float2,Float3,Float4,Color,Quat,Position,Matrix},
    SkeletalMesh, StaticMesh, Spline, Curve, RenderTarget2D, VolumeTexture,
    CameraQuery, NeighborGrid3D, Grid2D, Texture, etc.

    Each field returns: name, kind, sub-type, display_name, tooltip, category,
    clamp/UI min/max where present, recursive nested struct schema, enum entries
    for enum-typed fields, etc. Use to discover what fields a DI supports
    before configuring or instantiating one.

    Args:
        class_: Class name with or without the "UNiagaraDataInterface" prefix.
                Examples: "ArrayFloat", "UNiagaraDataInterfaceSkeletalMesh",
                "Spline", "RenderTarget2D".
    """
    return _call("get_niagara_data_interface_schema", {"class": class_})


@mcp.tool()
def get_niagara_schema_actions(
    system_path: str,
    module_name: str,
    filter: str | None = None,
    max_results: int = 300,
) -> str:
    """Return the full graph context-menu actions for a scratch pad module.

    Wraps UEdGraphSchema_Niagara::GetGraphActions — the same call the editor uses to
    populate the right-click "add node" menu for a Niagara script graph. Each entry
    has display_name, category, tooltip, keywords, and template_class.

    Args:
        system_path: Niagara System asset path
        module_name: Scratch pad module name (the actions are specific to its graph)
        filter: Optional substring applied to display_name/category/tooltip/keywords
        max_results: Cap on returned entries
    """
    params: dict = {
        "system_path": system_path,
        "module_name": module_name,
        "max_results": max_results,
    }
    if filter is not None:
        params["filter"] = filter
    return _call("get_niagara_schema_actions", params)


# ---------------------------------------------------------------------------
# Parameter enumeration + pin operations
# ---------------------------------------------------------------------------

@mcp.tool()
def list_niagara_available_parameters(
    filter: str | None = None,
    namespace: str = "all",
    max_results: int = 500,
    system_path: str | None = None,
    module_name: str | None = None,
) -> str:
    """List parameters that can be bound to a Map Get / Map Set pin.

    Returns well-known Engine.* parameters and Particles.* attributes unconditionally.
    When system_path + module_name are supplied, also adds:
      * User.* parameters from the system's exposed parameter collection
      * Rapid-iteration parameters from every emitter script
      * The target scratch pad module's own script graph variables (Module.* / Input.* / Transient.*)

    Args:
        filter: Optional case-insensitive substring filter on parameter name
        namespace: "all" (default), "engine", "particles", "user", "module", "input", "transient", "rapid_iteration"
        max_results: Cap on returned entries (default 500)
        system_path: Optional system path to pull per-emitter + user params
        module_name: Optional scratch pad module to pull script-local params
    """
    params: dict = {"namespace": namespace, "max_results": max_results}
    if filter is not None:
        params["filter"] = filter
    if system_path is not None:
        params["system_path"] = system_path
    if module_name is not None:
        params["module_name"] = module_name
    return _call("list_niagara_available_parameters", params)


@mcp.tool()
def add_niagara_map_get_pin(
    system_path: str, module_name: str, parameter_name: str, parameter_type: str = "float"
) -> str:
    """Add a typed output pin to the first ParameterMapGet node in a scratch pad graph.

    Mirrors clicking "+" on a Map Get node in the editor and picking a variable.
    The pin name becomes the parameter handle (e.g. "Engine.DeltaTime", "User.MyParam",
    "Particles.Velocity").

    Args:
        system_path: Niagara System asset path
        module_name: Scratch pad module name
        parameter_name: Variable name including namespace (e.g. "Engine.DeltaTime")
        parameter_type: Niagara type (float, int, bool, vec2/3/4, color, quat, matrix, position, ...)
    """
    return _call("add_niagara_map_get_pin", {
        "system_path": system_path,
        "module_name": module_name,
        "parameter_name": parameter_name,
        "parameter_type": parameter_type,
    })


@mcp.tool()
def add_niagara_map_set_pin(
    system_path: str, module_name: str, parameter_name: str, parameter_type: str = "float"
) -> str:
    """Add a typed input pin to the first ParameterMapSet node in a scratch pad graph.

    Mirrors clicking "+" on a Map Set node in the editor.

    Args:
        system_path: Niagara System asset path
        module_name: Scratch pad module name
        parameter_name: Variable name including namespace
        parameter_type: Niagara type (see add_niagara_map_get_pin)
    """
    return _call("add_niagara_map_set_pin", {
        "system_path": system_path,
        "module_name": module_name,
        "parameter_name": parameter_name,
        "parameter_type": parameter_type,
    })


@mcp.tool()
def add_niagara_node_pin(
    system_path: str,
    module_name: str,
    pin_name: str,
    pin_type: str = "float",
    direction: str = "input",
    node_class: str | None = None,
    node_index: int | None = None,
    node_id: str | None = None,
) -> str:
    """Generic: add a typed dynamic pin to any node in a scratch pad graph.

    Works with Custom HLSL, Map Get, Map Set, or any node inheriting from
    UNiagaraNodeWithDynamicPins. Identify the target node via exactly one of
    node_class / node_index / node_id.

    Args:
        system_path: Niagara System asset path
        module_name: Scratch pad module name
        pin_name: New pin name
        pin_type: Niagara type string (float, vec3, color, etc.)
        direction: "input" or "output"
        node_class: Class short name (e.g. "NiagaraNodeParameterMapGet", "ParameterMapGet")
        node_index: Raw Graph->Nodes index
        node_id: UEdGraphNode::NodeGuid string
    """
    params: dict = {
        "system_path": system_path,
        "module_name": module_name,
        "pin_name": pin_name,
        "pin_type": pin_type,
        "direction": direction,
    }
    if node_class is not None:
        params["node_class"] = node_class
    if node_index is not None:
        params["node_index"] = node_index
    if node_id is not None:
        params["node_id"] = node_id
    return _call("add_niagara_node_pin", params)


@mcp.tool()
def rename_niagara_node_pin(
    system_path: str,
    module_name: str,
    old_name: str,
    new_name: str,
    node_class: str | None = None,
    node_index: int | None = None,
    node_id: str | None = None,
) -> str:
    """Rename a dynamic pin on any node in a scratch pad graph.

    For Custom HLSL nodes, braced references inside the HLSL source ("{OldName}") are
    rewritten automatically.
    """
    params: dict = {
        "system_path": system_path,
        "module_name": module_name,
        "old_name": old_name,
        "new_name": new_name,
    }
    if node_class is not None:
        params["node_class"] = node_class
    if node_index is not None:
        params["node_index"] = node_index
    if node_id is not None:
        params["node_id"] = node_id
    return _call("rename_niagara_node_pin", params)


@mcp.tool()
def remove_niagara_node_pin(
    system_path: str,
    module_name: str,
    pin_name: str,
    node_class: str | None = None,
    node_index: int | None = None,
    node_id: str | None = None,
) -> str:
    """Remove a dynamic pin from any node in a scratch pad graph."""
    params: dict = {
        "system_path": system_path,
        "module_name": module_name,
        "pin_name": pin_name,
    }
    if node_class is not None:
        params["node_class"] = node_class
    if node_index is not None:
        params["node_index"] = node_index
    if node_id is not None:
        params["node_id"] = node_id
    return _call("remove_niagara_node_pin", params)


@mcp.tool()
def connect_niagara_pins(
    system_path: str,
    module_name: str,
    from_pin: str,
    to_pin: str,
    from_node_class: str | None = None,
    from_node_index: int | None = None,
    from_node_id: str | None = None,
    to_node_class: str | None = None,
    to_node_index: int | None = None,
    to_node_id: str | None = None,
) -> str:
    """Wire one node's output pin to another node's input pin inside a scratch pad graph.

    Connection is validated via UEdGraphSchema_Niagara::TryCreateConnection so type
    mismatches are rejected cleanly. Identify each side via the (class | index | id) trio.
    """
    params: dict = {
        "system_path": system_path,
        "module_name": module_name,
        "from_pin": from_pin,
        "to_pin": to_pin,
    }
    if from_node_class is not None:
        params["from_node_class"] = from_node_class
    if from_node_index is not None:
        params["from_node_index"] = from_node_index
    if from_node_id is not None:
        params["from_node_id"] = from_node_id
    if to_node_class is not None:
        params["to_node_class"] = to_node_class
    if to_node_index is not None:
        params["to_node_index"] = to_node_index
    if to_node_id is not None:
        params["to_node_id"] = to_node_id
    return _call("connect_niagara_pins", params)


@mcp.tool()
def disconnect_niagara_pins(
    system_path: str,
    module_name: str,
    pin_name: str,
    node_class: str | None = None,
    node_index: int | None = None,
    node_id: str | None = None,
) -> str:
    """Break all connections on a specific pin of a node in a scratch pad graph."""
    params: dict = {
        "system_path": system_path,
        "module_name": module_name,
        "pin_name": pin_name,
    }
    if node_class is not None:
        params["node_class"] = node_class
    if node_index is not None:
        params["node_index"] = node_index
    if node_id is not None:
        params["node_id"] = node_id
    return _call("disconnect_niagara_pins", params)
