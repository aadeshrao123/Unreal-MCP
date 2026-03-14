"""Level/world tools — spawn actors, inspect viewport selection, world info."""

from unrealmcp._bridge import mcp
from unrealmcp._tcp_bridge import _call


@mcp.tool()
def spawn_actor(
    class_name: str,
    location_x: float = 0.0,
    location_y: float = 0.0,
    location_z: float = 0.0,
    rotation_yaw: float = 0.0,
    rotation_pitch: float = 0.0,
    rotation_roll: float = 0.0,
) -> str:
    """Spawn an actor in the current editor level.

    Args:
        class_name: Blueprint path ("/Game/BP_Foo") or built-in class name
                    ("StaticMeshActor", "PointLight", "SpotLight", "DirectionalLight", "CameraActor")
        location_x, location_y, location_z: World position
        rotation_yaw, rotation_pitch, rotation_roll: Rotation in degrees
    """
    return _call("spawn_actor_from_class", {
        "class_name":     class_name,
        "location_x":     location_x,
        "location_y":     location_y,
        "location_z":     location_z,
        "rotation_yaw":   rotation_yaw,
        "rotation_pitch": rotation_pitch,
        "rotation_roll":  rotation_roll,
    })


@mcp.tool()
def get_selected_actors() -> str:
    """Get all currently selected actors in the editor viewport."""
    return _call("get_selected_actors")


@mcp.tool()
def get_world_info() -> str:
    """Get current editor level info (world name, actor count, actor list)."""
    return _call("get_world_info")


@mcp.tool()
def get_actor_properties(
    actor_label: str,
    filter: str = "",
    include_components: bool = False,
) -> str:
    """Get ALL property values from a live actor instance placed in the world.

    Unlike get_blueprint_class_defaults (which reads CDO/default values),
    this reads the ACTUAL placed instance — so per-instance overrides set
    in the editor are returned correctly.

    Args:
        actor_label: The actor's display label in the Outliner (also accepts UObject name)
        filter: Substring to filter property names (case-insensitive), e.g. "resource"
        include_components: Also return property maps for attached components
    """
    return _call("get_actor_properties", {
        "actor_label":        actor_label,
        "filter":             filter,
        "include_components": include_components,
    })
