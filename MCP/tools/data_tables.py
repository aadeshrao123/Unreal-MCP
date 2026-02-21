"""Data Table tools — native C++ CRUD via the TCP bridge."""

import json

from _bridge import mcp
from _tcp_bridge import _call


@mcp.tool()
def get_data_table_rows(data_table_path: str) -> str:
    """Get all rows and their field data from a Data Table asset."""
    return _call("get_data_table_rows", {"data_table_path": data_table_path})


@mcp.tool()
def get_data_table_row(data_table_path: str, row_name: str) -> str:
    """Get a single row from a Data Table by its row name."""
    return _call("get_data_table_row", {
        "data_table_path": data_table_path,
        "row_name": row_name,
    })


@mcp.tool()
def get_data_table_schema(data_table_path: str) -> str:
    """Get column names and types defined by the Data Table's row struct."""
    return _call("get_data_table_schema", {"data_table_path": data_table_path})


@mcp.tool()
def add_data_table_row(
    data_table_path: str,
    row_name: str,
    data: str = "",
) -> str:
    """Add a new row, optionally setting initial field values.

    data is a JSON string mapping field names to values, e.g. '{"FieldA": 1}'.
    """
    params = {
        "data_table_path": data_table_path,
        "row_name": row_name,
    }
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
    """Update specific fields on an existing row (partial update).

    Only fields present in data are modified; everything else keeps its value.
    data is a JSON string, e.g. '{"DisplayName": "Miner"}'.
    """
    try:
        parsed = json.loads(data) if isinstance(data, str) else data
    except json.JSONDecodeError as e:
        return json.dumps({"status": "error", "error": f"Invalid JSON in data: {e}"})
    return _call("update_data_table_row", {
        "data_table_path": data_table_path,
        "row_name": row_name,
        "data": parsed,
    })


@mcp.tool()
def delete_data_table_row(data_table_path: str, row_name: str) -> str:
    """Delete a row from a Data Table by name."""
    return _call("delete_data_table_row", {
        "data_table_path": data_table_path,
        "row_name": row_name,
    })


@mcp.tool()
def rename_data_table_row(data_table_path: str, old_row_name: str, new_row_name: str) -> str:
    """Rename a row in-place (preserves row order)."""
    return _call("rename_data_table_row", {
        "data_table_path": data_table_path,
        "old_row_name": old_row_name,
        "new_row_name": new_row_name,
    })


@mcp.tool()
def duplicate_data_table_row(data_table_path: str, source_row_name: str, new_row_name: str) -> str:
    """Copy an existing row under a new name."""
    return _call("duplicate_data_table_row", {
        "data_table_path": data_table_path,
        "source_row_name": source_row_name,
        "new_row_name": new_row_name,
    })
