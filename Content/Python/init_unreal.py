"""
UnrealMCP Editor Bridge — init_unreal.py

The HTTP server previously hosted here (port 8765) has been replaced by
the C++ UnrealMCPBridge module which runs a TCP server on port 55557.
All Python execution is now routed through the C++ bridge's
``execute_python`` command, which calls IPythonScriptPlugin internally.

This file is intentionally kept as a no-op so the PythonScriptPlugin
still recognises the plugin's Content/Python directory.
"""

import unreal

unreal.log("[UnrealMCP] Python bridge handled by C++ UnrealMCPBridge (TCP 55557)")
