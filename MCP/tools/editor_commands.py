"""Editor commands via C++ TCP bridge (port 55557).

These tools wrap the C++ UnrealMCPBridge commands for editor operations:
actor manipulation, Blueprint component/material operations, and level queries.
"""

from typing import Any, Dict, List, Optional

from _bridge import mcp
from _tcp_bridge import _tcp_send_raw

import json


def _call(command: str, params: Dict[str, Any]) -> str:
    return json.dumps(_tcp_send_raw(command, params), default=str, indent=2)


# ── Actor Tools ────────────────────────────────────────────────────────────

@mcp.tool()
def get_actors_in_level() -> str:
    """Get a list of all actors in the current level."""
    return _call("get_actors_in_level", {})


@mcp.tool()
def find_actors_by_name(pattern: str) -> str:
    """Find actors by name pattern.

    Args:
        pattern: Name pattern to search for
    """
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
        actor_type: Type of actor (StaticMeshActor, PointLight, etc.)
        location: [x, y, z] location
        rotation: [pitch, yaw, roll] rotation
        scale: [x, y, z] scale
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
    """Delete an actor from the level by name.

    Args:
        name: Name of the actor to delete
    """
    return _call("delete_actor", {"name": name})


@mcp.tool()
def set_actor_transform(
    name: str,
    location: List[float] = None,
    rotation: List[float] = None,
    scale: List[float] = None,
) -> str:
    """Set an actor's transform (location, rotation, scale).

    Args:
        name: Actor name
        location: [x, y, z] new location
        rotation: [pitch, yaw, roll] new rotation
        scale: [x, y, z] new scale
    """
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
    """Spawn an instance of a Blueprint actor in the level.

    Args:
        blueprint_path: Full path to the Blueprint asset
        location: [x, y, z] spawn location
        rotation: [pitch, yaw, roll] rotation
        scale: [x, y, z] scale
        name: Optional actor name
    """
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


# ── Blueprint Component/Material Tools (via C++ bridge) ────────────────────

@mcp.tool()
def set_static_mesh_properties(
    blueprint_name: str,
    component_name: str,
    static_mesh: str = "/Engine/BasicShapes/Cube.Cube",
) -> str:
    """Set static mesh on a StaticMeshComponent in a Blueprint.

    Args:
        blueprint_name: Name of the Blueprint
        component_name: Name of the component
        static_mesh: Path to static mesh asset
    """
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
    """Set physics properties on a Blueprint component.

    Args:
        blueprint_name: Name of the Blueprint
        component_name: Name of the component
        simulate_physics: Enable physics simulation
        gravity_enabled: Enable gravity
        mass: Mass in kg
        linear_damping: Linear damping factor
        angular_damping: Angular damping factor
    """
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

    Args:
        blueprint_name: Name of the Blueprint
        component_name: Name of the mesh component
        color: [R, G, B, A] color values (0-1)
        material_path: Path to material
        parameter_name: Color parameter name
        material_slot: Material slot index
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
def get_available_materials(
    search_path: str = "/Game/",
    include_engine_materials: bool = True,
) -> str:
    """Get available materials that can be applied to objects.

    Args:
        search_path: Path to search for materials
        include_engine_materials: Include engine built-in materials
    """
    return _call("get_available_materials", {
        "search_path": search_path,
        "include_engine_materials": include_engine_materials,
    })


@mcp.tool()
def apply_material_to_actor(
    actor_name: str,
    material_path: str,
    material_slot: int = 0,
) -> str:
    """Apply a material to an actor in the level.

    Args:
        actor_name: Name of the actor
        material_path: Full path to the material asset
        material_slot: Material slot index
    """
    return _call("apply_material_to_actor", {
        "actor_name": actor_name,
        "material_path": material_path,
        "material_slot": material_slot,
    })


@mcp.tool()
def apply_material_to_blueprint(
    blueprint_name: str,
    component_name: str,
    material_path: str,
    material_slot: int = 0,
) -> str:
    """Apply a material to a Blueprint component.

    Args:
        blueprint_name: Name of the Blueprint
        component_name: Name of the mesh component
        material_path: Full path to the material asset
        material_slot: Material slot index
    """
    return _call("apply_material_to_blueprint", {
        "blueprint_name": blueprint_name,
        "component_name": component_name,
        "material_path": material_path,
        "material_slot": material_slot,
    })


@mcp.tool()
def get_actor_material_info(actor_name: str) -> str:
    """Get material info for an actor.

    Args:
        actor_name: Name of the actor
    """
    return _call("get_actor_material_info", {"actor_name": actor_name})


@mcp.tool()
def get_blueprint_material_info(blueprint_name: str) -> str:
    """Get material info for a Blueprint's components.

    Args:
        blueprint_name: Name of the Blueprint
    """
    return _call("get_blueprint_material_info", {"blueprint_name": blueprint_name})
