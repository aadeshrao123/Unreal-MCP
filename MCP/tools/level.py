"""Level/world tools — spawn actors, inspect viewport selection, world info."""

import textwrap

from _bridge import mcp, _send


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
        class_name: Blueprint path ("/Game/BP_Foo") or built-in class ("StaticMeshActor", "PointLight")
        location_x: World X position
        location_y: World Y position
        location_z: World Z position
        rotation_yaw: Yaw in degrees
        rotation_pitch: Pitch in degrees
        rotation_roll: Roll in degrees
    """
    code = textwrap.dedent(f"""\
        import unreal
        loc = unreal.Vector({location_x}, {location_y}, {location_z})
        rot = unreal.Rotator({rotation_pitch}, {rotation_yaw}, {rotation_roll})
        # Try as blueprint asset first
        bp = unreal.EditorAssetLibrary.load_asset('{class_name}')
        if bp and isinstance(bp, unreal.Blueprint):
            actor = unreal.EditorLevelLibrary.spawn_actor_from_class(bp.generated_class(), loc, rot)
        else:
            cls = getattr(unreal, '{class_name}', None)
            if cls:
                actor = unreal.EditorLevelLibrary.spawn_actor_from_class(cls, loc, rot)
            else:
                actor = None
        if actor:
            result = f'Spawned {{actor.get_name()}} ({{actor.get_class().get_name()}}) at ({location_x}, {location_y}, {location_z})'
        else:
            result = 'Error: Failed to spawn — class not found or invalid'
    """)
    return _send(code)


@mcp.tool()
def get_selected_actors() -> str:
    """Get all currently selected actors in the editor viewport."""
    code = textwrap.dedent("""\
        import unreal
        actors = unreal.EditorLevelLibrary.get_selected_level_actors()
        result = []
        for a in actors:
            loc = a.get_actor_location()
            result.append({
                'name': a.get_name(),
                'label': a.get_actor_label(),
                'class': a.get_class().get_name(),
                'location': f'({loc.x:.1f}, {loc.y:.1f}, {loc.z:.1f})',
            })
    """)
    return _send(code)


@mcp.tool()
def get_world_info() -> str:
    """Get information about the current editor level (world name, actor count, actor list)."""
    code = textwrap.dedent("""\
        import unreal
        world = unreal.EditorLevelLibrary.get_editor_world()
        actors = unreal.EditorLevelLibrary.get_all_level_actors()
        result = {
            'world': world.get_name() if world else 'None',
            'actor_count': len(actors),
            'actors': [
                {'name': a.get_name(), 'class': a.get_class().get_name()}
                for a in actors[:100]
            ],
        }
        if len(actors) > 100:
            result['note'] = f'Showing 100 of {len(actors)} actors'
    """)
    return _send(code)
