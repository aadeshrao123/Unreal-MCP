"""Editor commands via C++ TCP bridge — actors, materials, physics, screenshots."""

from typing import Any, Dict, List, Optional

from unrealmcp._bridge import mcp
from unrealmcp._tcp_bridge import _call


# -- Actors ------------------------------------------------------------------

@mcp.tool()
def get_actors_in_level() -> str:
    """List all actors in the current level."""
    return _call("get_actors_in_level")


@mcp.tool()
def find_actors_by_name(pattern: str) -> str:
    """Find actors whose name matches the given pattern."""
    return _call("find_actors_by_name", {"pattern": pattern})


@mcp.tool()
def spawn_actor(
    name: str,
    actor_type: str = "StaticMeshActor",
    location: List[float] = None,
    rotation: List[float] = None,
    scale: List[float] = None,
    static_mesh: str = "",
) -> str:
    """Spawn an actor in the level.

    Args:
        name: Actor name
        actor_type: StaticMeshActor, PointLight, etc.
        location/rotation/scale: [x,y,z] arrays
        static_mesh: Path to static mesh asset (for mesh actors)
    """
    params: Dict[str, Any] = {"name": name, "type": actor_type}
    if location:
        params["location"] = location
    if rotation:
        params["rotation"] = rotation
    if scale:
        params["scale"] = scale
    if static_mesh:
        params["static_mesh"] = static_mesh
    return _call("spawn_actor", params)


@mcp.tool()
def delete_actor(name: str) -> str:
    """Delete an actor from the level by name."""
    return _call("delete_actor", {"name": name})


@mcp.tool()
def set_actor_transform(
    name: str,
    location: List[float] = None,
    rotation: List[float] = None,
    scale: List[float] = None,
) -> str:
    """Set an actor's location, rotation, and/or scale."""
    params: Dict[str, Any] = {"name": name}
    if location:
        params["location"] = location
    if rotation:
        params["rotation"] = rotation
    if scale:
        params["scale"] = scale
    return _call("set_actor_transform", params)


@mcp.tool()
def spawn_blueprint_actor(
    blueprint_path: str,
    location: List[float] = None,
    rotation: List[float] = None,
    scale: List[float] = None,
    name: str = "",
) -> str:
    """Spawn a Blueprint actor instance in the level."""
    params: Dict[str, Any] = {"blueprint_path": blueprint_path}
    if location:
        params["location"] = location
    if rotation:
        params["rotation"] = rotation
    if scale:
        params["scale"] = scale
    if name:
        params["name"] = name
    return _call("spawn_blueprint_actor", params)


# -- Blueprint Component/Material Tools -------------------------------------

@mcp.tool()
def set_static_mesh_properties(blueprint_name: str, component_name: str,
                               static_mesh: str = "/Engine/BasicShapes/Cube.Cube") -> str:
    """Set the static mesh on a StaticMeshComponent in a Blueprint."""
    return _call("set_static_mesh_properties", {
        "blueprint_name": blueprint_name,
        "component_name": component_name,
        "static_mesh": static_mesh,
    })


@mcp.tool()
def set_physics_properties(
    blueprint_name: str,
    component_name: str,
    simulate_physics: bool = True,
    gravity_enabled: bool = True,
    mass: float = 1.0,
    linear_damping: float = 0.01,
    angular_damping: float = 0.0,
) -> str:
    """Set physics properties on a Blueprint component."""
    return _call("set_physics_properties", {
        "blueprint_name": blueprint_name,
        "component_name": component_name,
        "simulate_physics": simulate_physics,
        "gravity_enabled": gravity_enabled,
        "mass": mass,
        "linear_damping": linear_damping,
        "angular_damping": angular_damping,
    })


@mcp.tool()
def set_mesh_material_color(
    blueprint_name: str,
    component_name: str,
    color: List[float],
    material_path: str = "/Engine/BasicShapes/BasicShapeMaterial",
    parameter_name: str = "BaseColor",
    material_slot: int = 0,
) -> str:
    """Set material color on a mesh component.

    color: [R, G, B, A] values 0-1
    """
    return _call("set_mesh_material_color", {
        "blueprint_name": blueprint_name,
        "component_name": component_name,
        "color": color,
        "material_path": material_path,
        "parameter_name": parameter_name,
        "material_slot": material_slot,
    })


@mcp.tool()
def get_available_materials(search_path: str = "/Game/", include_engine_materials: bool = True) -> str:
    """List materials that can be applied to objects."""
    return _call("get_available_materials", {
        "search_path": search_path,
        "include_engine_materials": include_engine_materials,
    })


@mcp.tool()
def apply_material_to_actor(actor_name: str, material_path: str, material_slot: int = 0) -> str:
    """Apply a material to a level actor."""
    return _call("apply_material_to_actor", {
        "actor_name": actor_name,
        "material_path": material_path,
        "material_slot": material_slot,
    })


@mcp.tool()
def apply_material_to_blueprint(
    blueprint_name: str, component_name: str,
    material_path: str, material_slot: int = 0,
) -> str:
    """Apply a material to a Blueprint component."""
    return _call("apply_material_to_blueprint", {
        "blueprint_name": blueprint_name,
        "component_name": component_name,
        "material_path": material_path,
        "material_slot": material_slot,
    })


@mcp.tool()
def get_actor_material_info(actor_name: str) -> str:
    """Get material slot info for a level actor."""
    return _call("get_actor_material_info", {"actor_name": actor_name})


@mcp.tool()
def get_blueprint_material_info(blueprint_name: str) -> str:
    """Get material slot info for a Blueprint's mesh components."""
    return _call("get_blueprint_material_info", {"blueprint_name": blueprint_name})


# -- Screenshot --------------------------------------------------------------

@mcp.tool()
def take_screenshot(file_path: str = "", mode: str = "viewport") -> str:
    """Capture the editor to a PNG.

    mode "viewport" captures the level viewport; "window" captures the
    entire active editor window (material editor, BP graph, widget designer, etc.).

    Returns the file path, width, and height. Use Read tool to view the image.
    """
    params = {}
    if file_path:
        params["file_path"] = file_path
    if mode != "viewport":
        params["mode"] = mode
    return _call("take_screenshot", params)
