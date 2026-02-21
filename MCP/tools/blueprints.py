"""Blueprint tools — create blueprints and add components."""

from _bridge import mcp
from _tcp_bridge import _call


@mcp.tool()
def search_parent_classes(
    filter: str,
    max_results: int = 20,
    include_blueprint_classes: bool = True,
) -> str:
    """Search for classes that can be used as Blueprint parents.

    Use this BEFORE create_blueprint to find the correct parent class name.
    Returns a filtered list of matching classes — never dumps all classes.

    Args:
        filter: Keyword to search for (e.g. "Miner", "Actor", "Pawn", "Widget")
        max_results: Maximum results (default 20, max 100)
        include_blueprint_classes: Also include Blueprint-generated classes (default True)
    """
    return _call("search_parent_classes", {
        "filter": filter,
        "max_results": max_results,
        "include_blueprint_classes": include_blueprint_classes,
    })


@mcp.tool()
def create_blueprint(
    name: str,
    path: str = "/Game/Blueprints",
    parent_class: str = "Actor",
) -> str:
    """Create a new Blueprint asset from any C++ or Blueprint parent class.

    Supports parent classes from ANY module (Engine, Game, plugins, etc.).
    Use search_parent_classes first to find the correct name.

    parent_class accepts short names ("MinerActor"), prefixed ("AMinerActor"),
    full paths ("/Script/Jiggify.AMinerActor"), or BP paths ("/Game/Blueprints/BP_Base").
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
        component_class: e.g. "StaticMeshComponent", "PointLightComponent"
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

    Unlike get_blueprint_variable_details (which only shows BP-defined variables),
    this reads the CDO and returns ALL editable properties — including C++ parent ones.

    Args:
        filter: Substring to filter property names (case-insensitive)
        include_inherited: Include properties from C++ parent (default True)
    """
    return _call("get_blueprint_class_defaults", {
        "blueprint_path": blueprint_path,
        "filter": filter,
        "include_inherited": include_inherited,
    })
