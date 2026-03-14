# Contributing to UnrealMCP

Thanks for your interest in contributing! Whether it's a bug report, feature request, or code contribution — all help is welcome.

## Reporting Issues

Found a bug or have a feature request? [Open an issue](https://github.com/aadeshrao123/Unreal-MCP/issues/new) with:

- **Bug reports**: Steps to reproduce, expected vs actual behavior, UE version, and OS
- **Feature requests**: What you'd like to see and why it would be useful

## Development Setup

### Prerequisites

- Unreal Engine 5.7 (or the version you want to test against)
- Python 3.10+
- Git

### Getting Started

1. **Fork and clone** the repository:
   ```bash
   git clone https://github.com/YOUR_USERNAME/Unreal-MCP.git
   cd Unreal-MCP
   ```

2. **Install in editable mode** — this is the key step:
   ```bash
   pip install -e .
   ```
   This links the `unrealmcp` command to your local source files. Any changes you make to Python files take effect immediately — no need to reinstall or rebuild.

3. **Copy or symlink the plugin** into a UE project's `Plugins/` folder so you can test the C++ bridge.

4. **Open the UE project**, check the Output Log for `MCP Bridge initialized on port XXXXX`.

5. **Configure your AI tool** to use `"command": "unrealmcp"` (see [README](README.md#step-2-run-the-setup-script) or run `install.bat`/`install.sh`).

### Testing Your Changes

Since you installed with `pip install -e .`, edits to any file in `unrealmcp/` are live instantly:

- **Edit** a file in `unrealmcp/tools/` (e.g., add a new tool or fix a bug)
- **Restart** your AI tool (so it reconnects to the MCP server)
- **Test** the change — no reinstall needed

For C++ changes, you'll need to rebuild the plugin through your IDE (Rider, Visual Studio, etc.) and restart the editor.

### Running the Linter

```bash
pip install ruff
ruff check unrealmcp/ --select=E,F,W --ignore=E501,F401
```

### Running the CI Tests Locally

```bash
# Install and verify
pip install -e .
python -c "import unrealmcp; print(unrealmcp.__version__)"

# Check all tool modules import
python -c "
from unrealmcp._bridge import mcp
from unrealmcp.tools import core, assets, blueprints, materials
print('All imports OK')
"
```

## Pull Requests

1. **Fork** the repository
2. **Create a branch** from `main` (`git checkout -b my-feature`)
3. **Install in editable mode** (`pip install -e .`)
4. **Make your changes** — keep commits focused and well-described
5. **Test** your changes with a running UE5 editor
6. **Run the linter** (`ruff check unrealmcp/`)
7. **Open a Pull Request** against `main`

### Guidelines

- Follow the existing code style (Allman braces for C++, standard Python conventions)
- Keep PRs focused — one feature or fix per PR
- Add tool docstrings for any new Python MCP tools (the AI reads these)
- Test with at least one MCP client (Claude Code, Cursor, etc.)
- For new C++ commands, make sure they execute on the game thread when touching editor state

### Adding New Tools

1. Create a new file in `unrealmcp/tools/` (or add to an existing one):
   ```python
   from unrealmcp._bridge import mcp
   from unrealmcp._tcp_bridge import _call

   @mcp.tool()
   def my_custom_tool(param1: str, param2: int = 10) -> str:
       """Description shown to the AI assistant."""
       return _call("my_custom_command", {
           "param1": param1,
           "param2": param2,
       })
   ```

2. Register it in `unrealmcp/tools/__init__.py`:
   ```python
   from unrealmcp.tools import my_tools  # noqa: F401
   ```

3. Add the C++ command handler in `Source/UnrealMCPBridge/Private/Commands/`

4. Wire it up in `ExecuteCommand()`

Since you're in editable mode, the new tool is available as soon as you restart your AI tool — no pip install needed.

### What Makes a Good Contribution

- Bug fixes with clear reproduction steps
- New tools that expose useful UE5 editor functionality
- Documentation improvements
- Performance improvements to the TCP bridge or command handlers
- Support for additional UE5 versions or platforms

## Questions?

If you're not sure about something, [open an issue](https://github.com/aadeshrao123/Unreal-MCP/issues) and ask. We'd rather help you get started than have you struggle in silence.
