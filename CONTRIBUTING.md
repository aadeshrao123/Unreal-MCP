# Contributing to UnrealMCP

Thanks for your interest in contributing! Whether it's a bug report, feature request, or code contribution — all help is welcome.

## Reporting Issues

Found a bug or have a feature request? [Open an issue](https://github.com/aadeshrao123/Unreal-MCP/issues/new) with:

- **Bug reports**: Steps to reproduce, expected vs actual behavior, UE version, and OS
- **Feature requests**: What you'd like to see and why it would be useful

## Pull Requests

Want to contribute code? Great! Here's the workflow:

1. **Fork** the repository
2. **Create a branch** from `main` (`git checkout -b my-feature`)
3. **Make your changes** — keep commits focused and well-described
4. **Test** your changes with a running UE5 editor
5. **Open a Pull Request** against `main`

### Guidelines

- Follow the existing code style (Allman braces for C++, standard Python conventions)
- Keep PRs focused — one feature or fix per PR
- Add tool docstrings for any new Python MCP tools (the AI reads these)
- Test with at least one MCP client (Claude Code, Cursor, etc.)
- For new C++ commands, make sure they execute on the game thread when touching editor state

### Adding New Tools

See the [Adding Custom Tools](README.md#adding-custom-tools) section in the README for the pattern. In short:

1. Add a Python tool function in `MCP/tools/`
2. Register it in `MCP/tools/__init__.py`
3. Add the C++ command handler in `Source/UnrealMCPBridge/Private/Commands/`
4. Wire it up in `ExecuteCommand()`

### What Makes a Good Contribution

- Bug fixes with clear reproduction steps
- New tools that expose useful UE5 editor functionality
- Documentation improvements
- Performance improvements to the TCP bridge or command handlers
- Support for additional UE5 versions or platforms

## Questions?

If you're not sure about something, [open an issue](https://github.com/aadeshrao123/Unreal-MCP/issues) and ask. We'd rather help you get started than have you struggle in silence.
