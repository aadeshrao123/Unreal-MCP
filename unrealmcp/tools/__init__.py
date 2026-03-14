"""
Tool registration — importing each module registers its @mcp.tool() decorators.
"""

from unrealmcp.tools import core       # noqa: F401
from unrealmcp.tools import assets     # noqa: F401
from unrealmcp.tools import materials  # noqa: F401
from unrealmcp.tools import blueprints # noqa: F401
from unrealmcp.tools import level      # noqa: F401
from unrealmcp.tools import data_tables # noqa: F401
from unrealmcp.tools import blueprint_graph  # noqa: F401 — C++ bridge: BP graph nodes
from unrealmcp.tools import editor_commands  # noqa: F401 — C++ bridge: actors, materials
from unrealmcp.tools import data_assets      # noqa: F401 — C++ bridge: data asset CRUD
from unrealmcp.tools import widgets          # noqa: F401 — C++ bridge: widget tree CRUD
from unrealmcp.tools import enhanced_input   # noqa: F401 — C++ bridge: Enhanced Input IA/IMC
from unrealmcp.tools import profiling        # noqa: F401 — C++ bridge: .utrace profiling analysis
from unrealmcp.tools import debug            # noqa: F401 — C++ bridge: token debug & response analysis
