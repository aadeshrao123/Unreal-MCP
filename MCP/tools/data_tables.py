"""Data Table tools — inspect rows and data."""

import textwrap

from _bridge import mcp, _send


@mcp.tool()
def get_data_table_rows(data_table_path: str) -> str:
    """Get all row names and data from a Data Table asset.

    Args:
        data_table_path: Full path to data table (e.g. "/Game/Data/DT_Items")
    """
    code = textwrap.dedent(f"""\
        import unreal
        dt = unreal.EditorAssetLibrary.load_asset('{data_table_path}')
        if not dt:
            result = 'Error: Data table not found'
        else:
            row_names = unreal.DataTableFunctionLibrary.get_data_table_column_as_string(dt, 'Name')
            result = {{'table': '{data_table_path}', 'row_names': list(dt.get_editor_property('row_map').keys()) if hasattr(dt, 'get_editor_property') else row_names}}
    """)
    return _send(code)
