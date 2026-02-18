"""Data Asset tools — create and modify any UDataAsset / UPrimaryDataAsset subclass."""

import json

from _bridge import mcp
from _tcp_bridge import _tcp_send_raw


def _call(command: str, params: dict) -> str:
    resp = _tcp_send_raw(command, params)
    return json.dumps(resp, default=str, indent=2)


@mcp.tool()
def list_data_asset_classes(
    filter: str = "",
    include_abstract: bool = False,
) -> str:
    """List all UDataAsset subclasses that are currently loaded in the editor.

    Returns name, full class path, parent class, whether it is a
    UPrimaryDataAsset, and the number of editable properties.

    Args:
        filter: Optional substring to filter class names (case-insensitive).
        include_abstract: Include abstract base classes (default False).
    """
    return _call("list_data_asset_classes", {
        "filter":           filter,
        "include_abstract": include_abstract,
    })


@mcp.tool()
def create_data_asset(
    class_name: str,
    asset_path: str,
    initial_properties: dict | None = None,
) -> str:
    """Create a new data asset of any UDataAsset subclass.

    Args:
        class_name: Short class name (e.g. "UItemData", "ItemData") or full
                    path (e.g. "/Script/Jiggify.UItemData").
        asset_path: Full content path for the new asset, including name
                    (e.g. "/Game/Data/Items/DA_IronOre").
        initial_properties: Optional dict of property_name → value to set
                            immediately after creation. Supports all types:
                            primitives, structs {"tagName": "…"}, arrays,
                            object paths "/Game/…", etc.

    Example:
        create_data_asset(
            class_name="UItemData",
            asset_path="/Game/Data/Items/DA_IronOre",
            initial_properties={
                "ItemName": "Iron Ore",
                "MaxStackSize": 100,
                "ItemTag": {"tagName": "Resource.Base.IronOre"},
                "ItemMesh": "/Game/Meshes/SM_IronOre",
            }
        )
    """
    params: dict = {
        "class_name": class_name,
        "asset_path": asset_path,
    }
    if initial_properties:
        params["initial_properties"] = initial_properties
    return _call("create_data_asset", params)


@mcp.tool()
def get_data_asset_properties(
    asset_path: str,
    filter: str = "",
    include_inherited: bool = True,
) -> str:
    """Read ALL property values from a data asset — every FProperty with no
    visibility filter, including inherited C++ properties.

    Args:
        asset_path: Full content path (e.g. "/Game/Data/Items/DA_IronOre").
        filter: Optional substring to filter property names (case-insensitive).
        include_inherited: Include properties from parent C++ classes (default True).
    """
    return _call("get_data_asset_properties", {
        "asset_path":        asset_path,
        "filter":            filter,
        "include_inherited": include_inherited,
    })


@mcp.tool()
def set_data_asset_property(
    asset_path: str,
    property_name: str,
    property_value,
) -> str:
    """Set a single property on a data asset. Supports all property types:

    - Primitives:   42, 3.14, True, "hello"
    - Enums:        "TG_PostPhysics" or 2
    - FName:        "MyName"
    - FText:        "Display Text"
    - Structs:      {"tagName": "Resource.Base.IronOre"}
                    {"x": 1.0, "y": 2.0, "z": 3.0}
    - Arrays:       [1, 2, 3]  or  [{"tagName": "Tag.A"}, {"tagName": "Tag.B"}]
    - Object refs:  "/Game/Meshes/SM_IronOre"  (loaded by path)
    - Soft refs:    "/Game/Textures/T_IronOre" (stored as soft path)
    - Null:         None  (clears object reference)

    Args:
        asset_path:     Full content path to the data asset.
        property_name:  Exact FProperty name (case-sensitive).
        property_value: Value to set (Python type, auto-converted to JSON).
    """
    return _call("set_data_asset_property", {
        "asset_path":     asset_path,
        "property_name":  property_name,
        "property_value": property_value,
    })


@mcp.tool()
def set_data_asset_properties(
    asset_path: str,
    properties: dict,
) -> str:
    """Set multiple properties on a data asset in a single call.

    More efficient than calling set_data_asset_property repeatedly — loads
    the asset once, sets all properties, then saves once.

    Args:
        asset_path:  Full content path to the data asset.
        properties:  Dict mapping property_name → value.
                     Same value format as set_data_asset_property.

    Example:
        set_data_asset_properties(
            asset_path="/Game/Data/DA_IronOre",
            properties={
                "DisplayName": "Iron Ore",
                "MaxStackSize": 100,
                "ResourceTag": {"tagName": "Resource.Base.IronOre"},
                "bIsRefined": False,
            }
        )
    """
    return _call("set_data_asset_properties", {
        "asset_path":  asset_path,
        "properties":  properties,
    })


@mcp.tool()
def list_data_assets(
    path: str = "/Game",
    class_filter: str = "",
    recursive: bool = True,
    max_results: int = 200,
) -> str:
    """List data assets in a content path, optionally filtered by class.

    Args:
        path:         Content Browser path to search (e.g. "/Game/Data").
        class_filter: Optional class name to filter by (e.g. "UItemData").
                      Matches any UDataAsset subclass if omitted.
        recursive:    Search subdirectories (default True).
        max_results:  Cap on results returned (default 200).
    """
    return _call("list_data_assets", {
        "path":         path,
        "class_filter": class_filter,
        "recursive":    recursive,
        "max_results":  max_results,
    })
