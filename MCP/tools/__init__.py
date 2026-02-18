"""
Tool registration — importing each module registers its @mcp.tool() decorators.
"""

from tools import core       # noqa: F401
from tools import assets     # noqa: F401
from tools import materials  # noqa: F401
from tools import blueprints # noqa: F401
from tools import level      # noqa: F401
from tools import data_tables # noqa: F401
from tools import blueprint_graph  # noqa: F401 — C++ bridge: BP graph nodes
from tools import editor_commands  # noqa: F401 — C++ bridge: actors, materials
from tools import data_assets      # noqa: F401 — C++ bridge: data asset CRUD
