"""Widget Blueprint tools — read and modify UMG Designer widget trees."""

import json
from typing import Optional

from _bridge import mcp
from _tcp_bridge import _tcp_send_raw


def _call(command: str, params: dict) -> str:
    resp = _tcp_send_raw(command, params)
    return json.dumps(resp, default=str, indent=2)


# ---------------------------------------------------------------------------
# Widget Tree CRUD
# ---------------------------------------------------------------------------


@mcp.tool()
def get_widget_tree(widget_blueprint_path: str) -> str:
    """Read the complete widget hierarchy from a Widget Blueprint's Designer.

    Returns a recursive JSON tree where each node has: name, class, is_panel,
    is_variable, slot_class, and children[] (for panels).

    Args:
        widget_blueprint_path: Full content path to the Widget Blueprint
                               (e.g. "/Game/UI/WBP_HUD").
    """
    return _call("get_widget_tree", {
        "widget_blueprint_path": widget_blueprint_path,
    })


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

    Creates a widget of the given class and adds it as a child of the
    specified parent panel widget.

    Args:
        widget_blueprint_path: Full content path to the Widget Blueprint.
        widget_class: Widget class name — short name like "TextBlock",
                      "Image", "Button", "CanvasPanel", "VerticalBox",
                      "HorizontalBox", "Overlay", "SizeBox", "Border",
                      "ScrollBox", "Spacer", "ProgressBar", "CheckBox",
                      "EditableTextBox", "ComboBoxString", "RichTextBlock".
                      Also accepts full path or "U"-prefixed names.
        parent_widget_name: Name of the parent panel widget (e.g.
                            "CanvasPanel_0"). Must be a UPanelWidget.
        widget_name: Optional name for the new widget. Auto-generated
                     if empty (e.g. "TextBlock_0").
        index: Insert position among siblings (-1 = append at end).
        slot_properties: Optional dict of slot property values to set
                         immediately (e.g. canvas anchors, offsets).
        widget_properties: Optional dict of widget property values to set
                           immediately (e.g. {"Text": "Hello"}).
    """
    params: dict = {
        "widget_blueprint_path": widget_blueprint_path,
        "widget_class":          widget_class,
        "parent_widget_name":    parent_widget_name,
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
def remove_widget(
    widget_blueprint_path: str,
    widget_name: str,
) -> str:
    """Remove a widget (and its children) from a Widget Blueprint.

    Cannot remove the root widget.

    Args:
        widget_blueprint_path: Full content path to the Widget Blueprint.
        widget_name: Name of the widget to remove.
    """
    return _call("remove_widget", {
        "widget_blueprint_path": widget_blueprint_path,
        "widget_name":           widget_name,
    })


@mcp.tool()
def move_widget(
    widget_blueprint_path: str,
    widget_name: str,
    new_parent_name: str,
    index: int = -1,
) -> str:
    """Move a widget to a different parent panel in the widget tree.

    Removes the widget from its current parent and adds it to the new
    parent. Cannot move the root widget or move a widget into itself
    or its own descendants.

    Args:
        widget_blueprint_path: Full content path to the Widget Blueprint.
        widget_name: Name of the widget to move.
        new_parent_name: Name of the new parent panel widget.
        index: Insert position among siblings (-1 = append at end).
    """
    params: dict = {
        "widget_blueprint_path": widget_blueprint_path,
        "widget_name":           widget_name,
        "new_parent_name":       new_parent_name,
    }
    if index >= 0:
        params["index"] = index
    return _call("move_widget", params)


@mcp.tool()
def rename_widget(
    widget_blueprint_path: str,
    widget_name: str,
    new_name: str,
) -> str:
    """Rename a widget in a Widget Blueprint.

    Updates the widget's UObject name and fixes up Blueprint variable
    tracking if the widget was marked as a variable (bIsVariable).

    Args:
        widget_blueprint_path: Full content path to the Widget Blueprint.
        widget_name: Current name of the widget.
        new_name: New name for the widget (must be unique in the tree).
    """
    return _call("rename_widget", {
        "widget_blueprint_path": widget_blueprint_path,
        "widget_name":           widget_name,
        "new_name":              new_name,
    })


@mcp.tool()
def duplicate_widget(
    widget_blueprint_path: str,
    widget_name: str,
    new_name: str = "",
    parent_widget_name: str = "",
) -> str:
    """Duplicate a widget (deep copy including children) in a Widget Blueprint.

    Creates a copy of the specified widget and adds it to the target
    parent (or the same parent as the source if not specified). Child
    widgets that would cause name conflicts are automatically renamed.

    Args:
        widget_blueprint_path: Full content path to the Widget Blueprint.
        widget_name: Name of the widget to duplicate.
        new_name: Optional name for the copy. Auto-generated if empty.
        parent_widget_name: Optional target parent panel. Defaults to
                            the source widget's current parent.
    """
    params: dict = {
        "widget_blueprint_path": widget_blueprint_path,
        "widget_name":           widget_name,
    }
    if new_name:
        params["new_name"] = new_name
    if parent_widget_name:
        params["parent_widget_name"] = parent_widget_name
    return _call("duplicate_widget", params)


# ---------------------------------------------------------------------------
# Widget / Slot Properties
# ---------------------------------------------------------------------------


@mcp.tool()
def get_widget_properties(
    widget_blueprint_path: str,
    widget_name: str,
    filter: str = "",
    include_inherited: bool = True,
) -> str:
    """Read all property values from a widget in a Widget Blueprint.

    Uses the same property serialization as data asset tools — supports
    all FProperty types including structs, arrays, object refs, etc.

    Args:
        widget_blueprint_path: Full content path to the Widget Blueprint.
        widget_name: Name of the widget to inspect.
        filter: Optional substring to filter property names (case-insensitive).
        include_inherited: Include properties from parent classes (default True).
    """
    return _call("get_widget_properties", {
        "widget_blueprint_path": widget_blueprint_path,
        "widget_name":           widget_name,
        "filter":                filter,
        "include_inherited":     include_inherited,
    })


@mcp.tool()
def set_widget_properties(
    widget_blueprint_path: str,
    widget_name: str,
    properties: dict,
) -> str:
    """Set multiple properties on a widget in a Widget Blueprint.

    Supports all property types: primitives, structs, arrays, object refs,
    enums, etc. Same format as set_data_asset_properties.

    Args:
        widget_blueprint_path: Full content path to the Widget Blueprint.
        widget_name: Name of the widget to modify.
        properties: Dict mapping property_name to value.

    Example:
        set_widget_properties(
            "/Game/UI/WBP_HUD", "TXT_Score",
            {"Text": "Score: 0", "ColorAndOpacity": {"R": 1, "G": 1, "B": 0, "A": 1}}
        )
    """
    return _call("set_widget_properties", {
        "widget_blueprint_path": widget_blueprint_path,
        "widget_name":           widget_name,
        "properties":            properties,
    })


@mcp.tool()
def get_slot_properties(
    widget_blueprint_path: str,
    widget_name: str,
    filter: str = "",
) -> str:
    """Read layout slot properties for a widget in a Widget Blueprint.

    The slot type depends on the parent panel:
    - CanvasPanel parent  -> UCanvasPanelSlot (Anchors, Offsets, Alignment, ZOrder)
    - HorizontalBox parent -> UHorizontalBoxSlot (Padding, Size, HAlign, VAlign)
    - VerticalBox parent   -> UVerticalBoxSlot (Padding, Size, HAlign, VAlign)
    - Overlay parent       -> UOverlaySlot (Padding, HAlign, VAlign)

    The root widget has no slot.

    Args:
        widget_blueprint_path: Full content path to the Widget Blueprint.
        widget_name: Name of the widget whose slot to inspect.
        filter: Optional substring to filter property names (case-insensitive).
    """
    params: dict = {
        "widget_blueprint_path": widget_blueprint_path,
        "widget_name":           widget_name,
    }
    if filter:
        params["filter"] = filter
    return _call("get_slot_properties", params)


@mcp.tool()
def set_slot_properties(
    widget_blueprint_path: str,
    widget_name: str,
    properties: dict,
) -> str:
    """Set layout slot properties for a widget in a Widget Blueprint.

    The available properties depend on the parent panel type.

    Args:
        widget_blueprint_path: Full content path to the Widget Blueprint.
        widget_name: Name of the widget whose slot to modify.
        properties: Dict mapping slot property_name to value.

    Example (CanvasPanel slot):
        set_slot_properties(
            "/Game/UI/WBP_HUD", "TXT_Score",
            {
                "LayoutData": {
                    "Offsets": {"Left": 100, "Top": 50, "Right": 200, "Bottom": 30},
                    "Anchors": {"Minimum": {"X": 0, "Y": 0}, "Maximum": {"X": 0, "Y": 0}}
                }
            }
        )
    """
    return _call("set_slot_properties", {
        "widget_blueprint_path": widget_blueprint_path,
        "widget_name":           widget_name,
        "properties":            properties,
    })


# ---------------------------------------------------------------------------
# Utility
# ---------------------------------------------------------------------------


@mcp.tool()
def list_widget_types(
    filter: str = "",
    include_abstract: bool = False,
    panels_only: bool = False,
) -> str:
    """List all available UWidget subclasses in the editor.

    Useful for discovering which widget classes can be used with add_widget.

    IMPORTANT: Use the 'filter' parameter to avoid large responses.
    Without a filter, this can return hundreds of widget types.

    Args:
        filter: Case-insensitive substring filter on class name.
                e.g. "Text" finds TextBlock, RichTextBlock, EditableText, etc.
        include_abstract: Include abstract base classes (default False).
        panels_only: Only return panel widgets that can have children
                     (CanvasPanel, VerticalBox, HorizontalBox, etc.).
    """
    return _call("list_widget_types", {
        "filter":           filter,
        "include_abstract": include_abstract,
        "panels_only":      panels_only,
    })
