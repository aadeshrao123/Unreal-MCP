"""Widget Blueprint tools — read and modify UMG Designer widget trees."""

from typing import Optional

from unrealmcp._bridge import mcp
from unrealmcp._tcp_bridge import _call


# -- Widget Tree CRUD --------------------------------------------------------

@mcp.tool()
def get_widget_tree(widget_blueprint_path: str) -> str:
    """Read the complete widget hierarchy from a Widget Blueprint's Designer.

    Returns a recursive JSON tree: name, class, is_panel, is_variable,
    slot_class, and children[] for panels.
    """
    return _call("get_widget_tree", {"widget_blueprint_path": widget_blueprint_path})


@mcp.tool()
def add_widget(
    widget_blueprint_path: str,
    widget_class: str,
    parent_widget_name: str,
    widget_name: str = "",
    index: int = -1,
    slot_properties: Optional[dict] = None,
    widget_properties: Optional[dict] = None,
) -> str:
    """Add a new widget to a Widget Blueprint's Designer tree.

    Args:
        widget_class: Short name like "TextBlock", "Image", "Button",
            "CanvasPanel", "VerticalBox", "HorizontalBox", "Overlay",
            "SizeBox", "Border", "ScrollBox", "ProgressBar", etc.
        parent_widget_name: Name of the parent panel (e.g. "CanvasPanel_0")
        widget_name: Optional name (auto-generated if empty)
        index: Insert position among siblings (-1 = append)
        slot_properties: Canvas anchors, offsets, etc.
        widget_properties: Widget values like {"Text": "Hello"}
    """
    params = {
        "widget_blueprint_path": widget_blueprint_path,
        "widget_class": widget_class,
        "parent_widget_name": parent_widget_name,
    }
    if widget_name:
        params["widget_name"] = widget_name
    if index >= 0:
        params["index"] = index
    if slot_properties:
        params["slot_properties"] = slot_properties
    if widget_properties:
        params["widget_properties"] = widget_properties
    return _call("add_widget", params)


@mcp.tool()
def remove_widget(widget_blueprint_path: str, widget_name: str) -> str:
    """Remove a widget and its children. Cannot remove the root widget."""
    return _call("remove_widget", {
        "widget_blueprint_path": widget_blueprint_path,
        "widget_name": widget_name,
    })


@mcp.tool()
def move_widget(
    widget_blueprint_path: str,
    widget_name: str,
    new_parent_name: str,
    index: int = -1,
) -> str:
    """Move a widget to a different parent panel.

    Cannot move root or into own descendants.
    """
    params = {
        "widget_blueprint_path": widget_blueprint_path,
        "widget_name": widget_name,
        "new_parent_name": new_parent_name,
    }
    if index >= 0:
        params["index"] = index
    return _call("move_widget", params)


@mcp.tool()
def rename_widget(widget_blueprint_path: str, widget_name: str, new_name: str) -> str:
    """Rename a widget (fixes up Blueprint variable tracking if bIsVariable)."""
    return _call("rename_widget", {
        "widget_blueprint_path": widget_blueprint_path,
        "widget_name": widget_name,
        "new_name": new_name,
    })


@mcp.tool()
def duplicate_widget(
    widget_blueprint_path: str,
    widget_name: str,
    new_name: str = "",
    parent_widget_name: str = "",
) -> str:
    """Deep-copy a widget including children. Auto-renames on conflicts."""
    params = {
        "widget_blueprint_path": widget_blueprint_path,
        "widget_name": widget_name,
    }
    if new_name:
        params["new_name"] = new_name
    if parent_widget_name:
        params["parent_widget_name"] = parent_widget_name
    return _call("duplicate_widget", params)


# -- Widget / Slot Properties ------------------------------------------------

@mcp.tool()
def get_widget_properties(
    widget_blueprint_path: str,
    widget_name: str,
    filter: str = "",
    include_inherited: bool = True,
) -> str:
    """Read all property values from a widget (supports all FProperty types)."""
    return _call("get_widget_properties", {
        "widget_blueprint_path": widget_blueprint_path,
        "widget_name": widget_name,
        "filter": filter,
        "include_inherited": include_inherited,
    })


@mcp.tool()
def set_widget_properties(widget_blueprint_path: str, widget_name: str, properties: dict) -> str:
    """Set multiple widget properties. Same format as set_data_asset_properties."""
    return _call("set_widget_properties", {
        "widget_blueprint_path": widget_blueprint_path,
        "widget_name": widget_name,
        "properties": properties,
    })


@mcp.tool()
def get_slot_properties(widget_blueprint_path: str, widget_name: str, filter: str = "") -> str:
    """Read layout slot properties (type depends on parent: CanvasPanelSlot, HorizontalBoxSlot, etc.)."""
    params = {
        "widget_blueprint_path": widget_blueprint_path,
        "widget_name": widget_name,
    }
    if filter:
        params["filter"] = filter
    return _call("get_slot_properties", params)


@mcp.tool()
def set_slot_properties(widget_blueprint_path: str, widget_name: str, properties: dict) -> str:
    """Set layout slot properties (anchors, offsets, padding, alignment, etc.)."""
    return _call("set_slot_properties", {
        "widget_blueprint_path": widget_blueprint_path,
        "widget_name": widget_name,
        "properties": properties,
    })


# -- Utility -----------------------------------------------------------------

@mcp.tool()
def list_widget_types(filter: str = "", include_abstract: bool = False, panels_only: bool = False) -> str:
    """List available UWidget subclasses. Use filter to narrow results."""
    return _call("list_widget_types", {
        "filter": filter,
        "include_abstract": include_abstract,
        "panels_only": panels_only,
    })
