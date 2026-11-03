# CodeFizz Editor Agent for Claude Code

Drive the Unreal Engine 5 editor from Claude Code. Ask for a Blueprint, a material, a Niagara
system, a PCG graph, a Control Rig or a Sequencer shot, and Claude builds it in your open editor,
then checks its own work. More than 1,500 commands on UE 5.6, 5.7 and 5.8 (Windows x64).

## Install

```
/plugin marketplace add https://github.com/aadeshrao123/Unreal-MCP.git
/plugin install codefizz-editor-agent@codefizz
```

Then ask Claude to "set up CodeFizz" (or run `/codefizz-editor-agent:setup`). It installs the
`cfa` CLI, signs you in, installs the Unreal plugin and the full skill, and opens your project.

## What it contains

- An MCP server: `cfa mcp`, from the free `cfa` CLI.
- A setup skill that walks Claude through the install.

The Unreal plugin needs a CodeFizz account. New accounts get a free 3-day trial.
Website and docs: https://codefizz.dev
