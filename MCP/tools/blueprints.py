"""Blueprint tools — create blueprints and add components."""

import json

from _bridge import mcp
from _tcp_bridge import _tcp_send_raw


def _call(command: str, params: dict) -> str:
    resp = _tcp_send_raw(command, params)
    return json.dumps(resp, default=str, indent=2)


@mcp.tool()
def create_blueprint(
    name: str,
    path: str = "/Game/Blueprints",
    parent_class: str = "Actor",
) -> str:
    """Create a new Blueprint asset.

    Args:
        name: Blueprint name (e.g. "BP_MyActor")
        path: Content Browser path (e.g. "/Game/Blueprints")
        parent_class: Parent class (Actor, Pawn, Character, PlayerController, GameModeBase, etc.)
    """
    return _call("create_blueprint", {
        "name": name,
        "path": path,
        "parent_class": parent_class,
    })


@mcp.tool()
def add_component_to_blueprint(
    blueprint_path: str,
    component_class: str,
    component_name: str = "",
) -> str:
    """Add a component to a Blueprint's default components.

    Args:
        blueprint_path: Full path to blueprint (e.g. "/Game/Blueprints/BP_MyActor")
        component_class: Component class (e.g. "StaticMeshComponent", "PointLightComponent")
        component_name: Optional custom name for the component
    """
    return _call("add_component_to_blueprint", {
        "blueprint_name": blueprint_path,   # C++ accepts full paths via FindBlueprint
        "component_type": component_class,
        "component_name": component_name,
    })


@mcp.tool()
def get_blueprint_class_defaults(
    blueprint_path: str,
    filter: str = "",
    include_inherited: bool = True,
) -> str:
    """Get all default property values from a Blueprint's generated class CDO.

    Unlike get_blueprint_variable_details (which only shows Blueprint-defined
    variables), this reads the Class Default Object of the generated class and
    returns ALL editable/blueprint-visible properties — including those defined
    in the C++ parent class.

    Args:
        blueprint_path: Full content path (e.g. "/Game/Blueprints/BP_MyActor")
        filter: Optional substring to filter property names (case-insensitive)
        include_inherited: Include properties inherited from C++ parent (default True)
    """
    return _call("get_blueprint_class_defaults", {
        "blueprint_path": blueprint_path,
        "filter": filter,
        "include_inherited": include_inherited,
    })
