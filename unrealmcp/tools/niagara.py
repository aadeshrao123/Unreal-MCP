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
) -> str:
    """Get all input parameters for a specific module.

    Args:
        system_path: Path to the Niagara System asset
        emitter_name: Name of the target emitter
        module_name: Name of the module
        script_usage: Stack the module is in
        input_filter: Optional substring filter on input parameter names
    """
    params: dict = {
        "system_path": system_path,
        "emitter_name": emitter_name,
        "module_name": module_name,
        "script_usage": script_usage,
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
) -> str:
    """Set a dynamic input (random range, linked parameter, custom expression) on a module input.

    Args:
        system_path: Path to the Niagara System asset
        emitter_name: Name of the target emitter
        module_name: Name of the module
        input_name: Name of the input parameter
        script_usage: Stack the module is in
        dynamic_input_type: Type of dynamic input — "UniformRangedFloat", "UniformRangedVector",
            "LinkedParameter", "CustomExpression", etc.
        min_value: Minimum value for range types
        max_value: Maximum value for range types
        parameter_name: Parameter name for LinkedParameter type
        expression: HLSL expression for CustomExpression type
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
    """Add a user parameter to a Niagara System.

    Args:
        system_path: Path to the Niagara System asset
        parameter_name: Name for the new parameter
        parameter_type: Type — Float, Vector, Color, Bool, Int32, Texture2D, StaticMesh, etc.
        default_value: Optional default value as string
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

@mcp.tool()
def create_niagara_scratch_pad_module(
    system_path: str,
    script_usage: str,
    module_name: str | None = None,
) -> str:
    """Create a new scratch pad module inline within a Niagara System.

    Scratch pad modules are local to the system and allow custom HLSL logic.

    Args:
        system_path: Path to the Niagara System asset
        script_usage: Target stack — ParticleSpawn, ParticleUpdate, EmitterSpawn, EmitterUpdate,
            SystemSpawn, SystemUpdate
        module_name: Optional display name for the scratch pad module
    """
    params: dict = {
        "system_path": system_path,
        "script_usage": script_usage,
    }
    if module_name is not None:
        params["module_name"] = module_name
    return _call("create_niagara_scratch_pad_module", params)


@mcp.tool()
def set_niagara_scratch_pad_hlsl(
    system_path: str,
    module_name: str,
    hlsl_code: str,
    inputs: str | None = None,
    outputs: str | None = None,
) -> str:
    """Set the HLSL code and pin definitions on a scratch pad module.

    Args:
        system_path: Path to the Niagara System asset
        module_name: Name of the scratch pad module
        hlsl_code: The HLSL code body
        inputs: Optional JSON array of input pins, e.g. [{"name":"Speed","type":"float"}]
        outputs: Optional JSON array of output pins, e.g. [{"name":"OutColor","type":"float4"}]
    """
    params: dict = {
        "system_path": system_path,
        "module_name": module_name,
        "hlsl_code": hlsl_code,
    }
    if inputs is not None:
        params["inputs"] = inputs
    if outputs is not None:
        params["outputs"] = outputs
    return _call("set_niagara_scratch_pad_hlsl", params)


@mcp.tool()
def create_niagara_module_asset(
    asset_path: str,
    script_usage: str,
    description: str | None = None,
    expose_to_library: bool = True,
) -> str:
    """Create a new standalone Niagara Module Script asset.

    Args:
        asset_path: Full content path including name (e.g. "/Game/FX/Modules/NMS_CustomForce")
        script_usage: Module usage — ParticleSpawn, ParticleUpdate, EmitterSpawn, EmitterUpdate,
            SystemSpawn, SystemUpdate
        description: Optional description for the module
        expose_to_library: If true, the module appears in the module browser
    """
    params: dict = {
        "asset_path": asset_path,
        "script_usage": script_usage,
        "expose_to_library": expose_to_library,
    }
    if description is not None:
        params["description"] = description
    return _call("create_niagara_module_asset", params)


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
    filter: str | None = None,
) -> str:
    """List available Niagara parameter types and their valid scopes.

    Args:
        scope: Parameter scope — "user", "system", "emitter", "particle", "all"
        filter: Optional substring filter on type name
    """
    params: dict = {"scope": scope}
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
