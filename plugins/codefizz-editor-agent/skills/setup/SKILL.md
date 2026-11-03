---
name: setup
description: Set up the CodeFizz Editor Agent so Claude Code can drive the Unreal Engine 5 editor. Use when the user wants to connect Claude to Unreal, when the cfa MCP server fails to start, or when the cfa command is not found.
---

# Set up the CodeFizz Editor Agent

The plugin starts an MCP server with `cfa mcp`, so the `cfa` CLI must be installed first.
Windows x64 only. Run each step, read its output, and stop on an error.

1. Check for the CLI: `cfa --version`. If it prints a version, go to step 3.
2. Install the CLI (no Node needed):
   `powershell -c "irm https://codefizz.dev/install.ps1 | iex"`
   It installs to `%LOCALAPPDATA%\CodeFizz\bin\cfa.exe` and adds that folder to the user PATH.
   Until a new terminal opens, call it by that full path.
3. Sign in: `cfa login`. It opens the browser. The user signs in with their CodeFizz account;
   a new account starts a free 3-day trial at https://codefizz.dev/pricing. Never type the
   user's password for them.
4. Install the Unreal plugin into an engine. List engines with `cfa engines`, then run
   `cfa install-plugin "<engine folder>"`, for example `cfa install-plugin "E:/Unreal/UE_5.8"`.
   Supported: UE 5.6, 5.7 and 5.8.
5. Install the full skill, which teaches every command and workflow: `cfa skill install`.
   Do not run `cfa install claude`; this plugin already provides the MCP server.
6. Open the project and check the connection:
   `cfa launch "<path to .uproject>"`, then `cfa health_check`.
7. Ask the user to run `/reload-plugins` (or restart Claude Code) so the `cfa` MCP server connects.

When a step fails, run `cfa doctor` and show its output. Docs: https://codefizz.dev/docs/quick-start
