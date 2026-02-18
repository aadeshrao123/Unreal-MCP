"""Asset management tools — search, inspect, modify, delete, save, import, duplicate, rename."""

import json

from _bridge import mcp
from _tcp_bridge import _tcp_send_raw


def _call(command: str, params: dict) -> str:
    resp = _tcp_send_raw(command, params)
    return json.dumps(resp, default=str, indent=2)


@mcp.tool()
def find_assets(
    class_type: str = "",
    path: str = "",
    name_pattern: str = "",
    recursive: bool = True,
    max_results: int = 200,
) -> str:
    """Search the Asset Registry by class, path, and/or name pattern.

    Args:
        class_type: Shortcut ("material", "blueprint", "static_mesh", "texture",
            "data_table", "niagara_system", etc.) or "PackagePath.ClassName"
        path: Content path filter (e.g. "/Game/Materials")
        name_pattern: Substring match on asset name (case-insensitive)
        recursive: Search subdirectories (default True)
        max_results: Maximum results to return (default 200)
    """
    return _call("find_assets", {
        "class_type":   class_type,
        "path":         path,
        "name_pattern": name_pattern,
        "recursive":    recursive,
        "max_results":  max_results,
    })


@mcp.tool()
def list_assets(
    path: str = "/Game",
    class_filter: str = "",
    recursive: bool = True,
) -> str:
    """List assets in a Content Browser directory.

    Args:
        path: Content path (e.g. "/Game", "/Game/Materials")
        class_filter: Optional class shortcut (e.g. "material", "blueprint")
        recursive: Search subdirectories
    """
    return _call("list_assets", {
        "path":         path,
        "class_filter": class_filter,
        "recursive":    recursive,
    })


@mcp.tool()
def open_asset(asset_path: str) -> str:
    """Open an asset in the UE5 editor (blueprint, material, data table, widget, etc.).

    Args:
        asset_path: Full asset path (e.g. "/Game/Materials/M_Base")
    """
    return _call("open_asset", {"asset_path": asset_path})


@mcp.tool()
def get_asset_info(asset_path: str) -> str:
    """Get comprehensive asset metadata: class, package, and key properties.

    Args:
        asset_path: Full asset path (e.g. "/Game/Materials/M_Base")
    """
    return _call("get_asset_info", {"asset_path": asset_path})


@mcp.tool()
def get_asset_properties(asset_path: str) -> str:
    """Get all editable properties of an asset.

    Args:
        asset_path: Full asset path (e.g. "/Game/Materials/M_Base")
    """
    return _call("get_asset_properties", {"asset_path": asset_path})


@mcp.tool()
def set_asset_property(
    asset_path: str,
    property_name: str,
    property_value: str,
) -> str:
    """Set a property on an asset.

    Args:
        asset_path: Full asset path
        property_name: Property name (e.g. "two_sided", "blend_mode")
        property_value: JSON-encoded value (e.g. "true", "0.5", '"translucent"')
    """
    # Parse so C++ receives the correct JSON type (bool, number, string, etc.)
    try:
        parsed = json.loads(property_value)
    except (json.JSONDecodeError, TypeError):
        parsed = property_value
    return _call("set_asset_property", {
        "asset_path":     asset_path,
        "property_name":  property_name,
        "property_value": parsed,
    })


@mcp.tool()
def find_references(
    asset_path: str,
    direction: str = "both",
) -> str:
    """Find all assets that reference or are referenced by a given asset.

    Args:
        asset_path: Full asset path (e.g. "/Game/Materials/M_Base")
        direction: "dependents" (who uses this), "dependencies" (what this uses), or "both"
    """
    return _call("find_references", {
        "asset_path": asset_path,
        "direction":  direction,
    })


@mcp.tool()
def duplicate_asset(source_path: str, dest_path: str, dest_name: str) -> str:
    """Duplicate an asset to a new location.

    Args:
        source_path: Full path to source asset (e.g. "/Game/Materials/M_Base")
        dest_path: Destination directory (e.g. "/Game/Materials/Copies")
        dest_name: Name for the duplicate (e.g. "M_Base_Copy")
    """
    return _call("duplicate_asset", {
        "source_path": source_path,
        "dest_path":   dest_path,
        "dest_name":   dest_name,
    })


@mcp.tool()
def rename_asset(source_path: str, dest_path: str) -> str:
    """Rename or move an asset. UE5 automatically fixes all references.

    Args:
        source_path: Current full asset path (e.g. "/Game/Materials/M_Old")
        dest_path: New full asset path (e.g. "/Game/Materials/M_New")
    """
    return _call("rename_asset", {
        "source_path": source_path,
        "dest_path":   dest_path,
    })


@mcp.tool()
def delete_asset(asset_path: str, force: bool = False) -> str:
    """Delete an asset or directory from the Content Browser.

    When deleting a single asset, checks for references first (unless force=True).
    If asset_path has no dot, treats it as a directory.

    Args:
        asset_path: Full asset path (e.g. "/Game/Materials/M_Test") or directory
        force: Skip reference check and force delete (default False)
    """
    return _call("delete_asset", {
        "asset_path": asset_path,
        "force":      force,
    })


@mcp.tool()
def save_asset(asset_path: str) -> str:
    """Save a specific asset to disk.

    Args:
        asset_path: Full asset path
    """
    return _call("save_asset", {"asset_path": asset_path})


@mcp.tool()
def save_all() -> str:
    """Save all unsaved (dirty) assets."""
    return _call("save_all", {})


@mcp.tool()
def import_asset(source_file: str, destination_path: str) -> str:
    """Import an external file into the Content Browser.

    Args:
        source_file: Absolute path to file on disk (e.g. "C:/textures/wood.png")
        destination_path: Content Browser path (e.g. "/Game/Textures")
    """
    return _call("import_asset", {
        "source_file":      source_file,
        "destination_path": destination_path,
    })


@mcp.tool()
def get_selected_assets() -> str:
    """Get currently selected assets in the Content Browser."""
    return _call("get_selected_assets", {})


@mcp.tool()
def sync_browser(asset_path: str) -> str:
    """Navigate the Content Browser to show a specific asset.

    Args:
        asset_path: Full asset path (e.g. "/Game/Materials/M_Base")
    """
    return _call("sync_browser", {"asset_path": asset_path})
