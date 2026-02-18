"""Level/world tools — spawn actors, inspect viewport selection, world info."""

import json

from _bridge import mcp
from _tcp_bridge import _tcp_send_raw


def _call(command: str, params: dict) -> str:
    resp = _tcp_send_raw(command, params)
    return json.dumps(resp, default=str, indent=2)


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
        location_x: World X position
        location_y: World Y position
        location_z: World Z position
        rotation_yaw: Yaw in degrees
        rotation_pitch: Pitch in degrees
        rotation_roll: Roll in degrees
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
    return _call("get_selected_actors", {})


@mcp.tool()
def get_world_info() -> str:
    """Get information about the current editor level (world name, actor count, actor list)."""
    return _call("get_world_info", {})


@mcp.tool()
def get_actor_properties(
    actor_label: str,
    filter: str = "",
    include_components: bool = False,
) -> str:
    """Get ALL property values from a live actor instance placed in the world.

    Unlike get_blueprint_class_defaults (which reads CDO/default values),
    this reads the ACTUAL placed instance — so per-instance overrides set
    in the editor (e.g. a specific ResourceType on one BP_ResourceNode) are
    returned correctly.

    Iterates every FProperty on the actor and its C++ parent classes with no
    flag filter, so Blueprint variables AND C++ properties are both returned.

    Args:
        actor_label: The actor's display label in the editor (shown in the
                     Outliner). Also accepts the internal UObject name.
        filter: Optional substring to filter property names (case-insensitive).
                E.g. "resource" to only return ResourceType and TotalAmount.
        include_components: If True, also return property maps for each
                            attached component.
    """
    return _call("get_actor_properties", {
        "actor_label":        actor_label,
        "filter":             filter,
        "include_components": include_components,
    })
