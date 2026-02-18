"""Data Table tools — native C++ commands via the TCP bridge."""

import json

from _bridge import mcp
from _tcp_bridge import _tcp_send_raw


def _call(command: str, params: dict) -> str:
    resp = _tcp_send_raw(command, params)
    return json.dumps(resp, default=str, indent=2)


@mcp.tool()
def get_data_table_rows(data_table_path: str) -> str:
    """Get all rows and their field data from a Data Table asset.

    Args:
        data_table_path: Full content-browser path (e.g. "/Game/Data/DT_Items")
    """
    return _call("get_data_table_rows", {"data_table_path": data_table_path})


@mcp.tool()
def get_data_table_row(data_table_path: str, row_name: str) -> str:
    """Get a single row from a Data Table by its row name.

    Args:
        data_table_path: Full content-browser path to the data table
        row_name: Exact name of the row to retrieve
    """
    return _call("get_data_table_row", {
        "data_table_path": data_table_path,
        "row_name": row_name,
    })


@mcp.tool()
def get_data_table_schema(data_table_path: str) -> str:
    """Get the column names and types defined by a Data Table's row struct.

    Args:
        data_table_path: Full content-browser path to the data table
    """
    return _call("get_data_table_schema", {"data_table_path": data_table_path})


@mcp.tool()
def add_data_table_row(
    data_table_path: str,
    row_name: str,
    data: str = "",
) -> str:
    """Add a new row to a Data Table, optionally setting initial field values.

    Args:
        data_table_path: Full content-browser path to the data table
        row_name: Name for the new row (must be unique)
        data: JSON string mapping field names to initial values, e.g. '{"FieldA": 1}'
    """
    params: dict = {"data_table_path": data_table_path, "row_name": row_name}
    if data:
        try:
            params["data"] = json.loads(data) if isinstance(data, str) else data
        except json.JSONDecodeError as e:
            return json.dumps({"status": "error", "error": f"Invalid JSON in data: {e}"})
    return _call("add_data_table_row", params)


@mcp.tool()
def update_data_table_row(
    data_table_path: str,
    row_name: str,
    data: str,
) -> str:
    """Update specific fields on an existing Data Table row.

    Only the fields present in *data* are modified; all other fields
    keep their current values.

    Args:
        data_table_path: Full content-browser path to the data table
        row_name: Name of the row to update
        data: JSON string mapping field names to new values, e.g. '{"DisplayName": "Miner"}'
    """
    try:
        data_dict = json.loads(data) if isinstance(data, str) else data
    except json.JSONDecodeError as e:
        return json.dumps({"status": "error", "error": f"Invalid JSON in data: {e}"})
    return _call("update_data_table_row", {
        "data_table_path": data_table_path,
        "row_name": row_name,
        "data": data_dict,
    })


@mcp.tool()
def delete_data_table_row(data_table_path: str, row_name: str) -> str:
    """Delete a row from a Data Table by name.

    Args:
        data_table_path: Full content-browser path to the data table
        row_name: Name of the row to delete
    """
    return _call("delete_data_table_row", {
        "data_table_path": data_table_path,
        "row_name": row_name,
    })


@mcp.tool()
def rename_data_table_row(
    data_table_path: str,
    old_row_name: str,
    new_row_name: str,
) -> str:
    """Rename a row in a Data Table in-place (preserves row order).

    Args:
        data_table_path: Full content-browser path to the data table
        old_row_name: Current name of the row
        new_row_name: New name for the row (must be unique)
    """
    return _call("rename_data_table_row", {
        "data_table_path": data_table_path,
        "old_row_name": old_row_name,
        "new_row_name": new_row_name,
    })


@mcp.tool()
def duplicate_data_table_row(
    data_table_path: str,
    source_row_name: str,
    new_row_name: str,
) -> str:
    """Copy an existing Data Table row under a new name.

    Args:
        data_table_path: Full content-browser path to the data table
        source_row_name: Name of the row to copy
        new_row_name: Name for the new duplicated row (must be unique)
    """
    return _call("duplicate_data_table_row", {
        "data_table_path": data_table_path,
        "source_row_name": source_row_name,
        "new_row_name": new_row_name,
    })
