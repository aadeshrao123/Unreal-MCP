#!/usr/bin/env bash
set -e

echo ""
echo " ============================================"
echo "  UnrealMCP Setup"
echo "  Configure MCP for your AI tool"
echo " ============================================"
echo ""

# Check for Python
PYTHON=""
if command -v python3 &>/dev/null; then
    PYTHON="python3"
elif command -v python &>/dev/null; then
    PYTHON="python"
else
    echo " [ERROR] Python not found. Please install Python 3.10+."
    echo " macOS: brew install python"
    echo " Linux: sudo apt install python3 python3-pip"
    exit 1
fi

echo " [OK] Python found: $($PYTHON --version)"
echo ""

# Install pip package
echo " Installing unrealmcp Python package..."
$PYTHON -m pip install unrealmcp -q 2>/dev/null || $PYTHON -m pip install --user unrealmcp -q 2>/dev/null || true

if command -v unrealmcp &>/dev/null; then
    echo " [OK] unrealmcp command ready"
else
    echo " [ERROR] unrealmcp command not found after install."
    echo "        You may need to add Python scripts to your PATH."
    exit 1
fi
echo ""

# ============================================
# Detect where we are
# ============================================
SCOPE=""
PROJECT_ROOT=""
UPROJECT=""
IN_ENGINE=0

# Search upward for .uproject
SEARCH_DIR="$(pwd)"
while [ "$SEARCH_DIR" != "/" ]; do
    FOUND=$(ls "$SEARCH_DIR"/*.uproject 2>/dev/null | head -1)
    if [ -n "$FOUND" ]; then
        UPROJECT="$(basename "$FOUND")"
        PROJECT_ROOT="$SEARCH_DIR"
        break
    fi
    SEARCH_DIR="$(dirname "$SEARCH_DIR")"
done

# Check if we're inside an Engine folder
CHECK_DIR="$(pwd)"
while [ "$CHECK_DIR" != "/" ]; do
    DIRNAME="$(basename "$CHECK_DIR")"
    if [ "$DIRNAME" = "Engine" ] || [ "$DIRNAME" = "engine" ]; then
        IN_ENGINE=1
        break
    fi
    CHECK_DIR="$(dirname "$CHECK_DIR")"
done

# Decide scope
if [ -n "$UPROJECT" ] && [ $IN_ENGINE -eq 0 ]; then
    echo " Detected: UE project - $UPROJECT"
    echo " Location: $PROJECT_ROOT"
    echo ""
    echo "  Where should the MCP config be created?"
    echo ""
    echo "   1) This project only (project scope)"
    echo "   2) All projects (global scope)"
    echo ""
    read -p "  Enter choice (1-2): " SCOPE_CHOICE
    if [ "$SCOPE_CHOICE" = "2" ]; then
        SCOPE="GLOBAL"
    else
        SCOPE="PROJECT"
    fi
elif [ $IN_ENGINE -eq 1 ]; then
    echo " Detected: Inside Unreal Engine folder"
    echo " MCP config will be set globally (user-level)."
    echo ""
    SCOPE="GLOBAL"
else
    echo " No UE project or engine detected."
    echo " MCP config will be set globally (user-level)."
    echo ""
    SCOPE="GLOBAL"
fi

# ============================================
# Helper: check if unrealmcp already in a file
# ============================================
check_has_unrealmcp() {
    local file="$1"
    if [ -f "$file" ] && grep -qi "unrealmcp" "$file" 2>/dev/null; then
        return 0
    fi
    return 1
}

# ============================================
# Helper: merge unreal entry into existing JSON
# Uses python since it's already installed
# ============================================
merge_mcp_entry() {
    local file="$1"
    local root_key="$2"
    local extra="$3"
    $PYTHON -c "
import json, sys
f = '$file'
with open(f) as fh:
    data = json.load(fh)
if '$root_key' not in data:
    data['$root_key'] = {}
entry = {'command': 'unrealmcp', 'args': [], 'env': {}}
${extra}
data['$root_key']['unreal'] = entry
with open(f, 'w') as fh:
    json.dump(data, fh, indent=2)
    fh.write('\n')
"
    echo " [OK] Added unrealmcp to $file"
}

# ============================================
# Config writers
# ============================================

setup_claude() {
    local target
    if [ "$SCOPE" = "PROJECT" ]; then
        target="$PROJECT_ROOT/.mcp.json"
    else
        target="$HOME/.claude.json"
    fi
    if check_has_unrealmcp "$target"; then
        echo " [OK] Claude Code - already configured in $target"
        return
    fi
    if [ -f "$target" ]; then
        merge_mcp_entry "$target" "mcpServers" "entry['type'] = 'stdio'"
        return
    fi
    cat > "$target" << 'EOF'
{
  "mcpServers": {
    "unreal": {
      "type": "stdio",
      "command": "unrealmcp",
      "args": [],
      "env": {}
    }
  }
}
EOF
    echo " [OK] Claude Code - $target"
}

setup_cursor() {
    local target
    if [ "$SCOPE" = "PROJECT" ]; then
        target="$PROJECT_ROOT/.cursor/mcp.json"
        mkdir -p "$PROJECT_ROOT/.cursor"
    else
        target="$HOME/.cursor/mcp.json"
        mkdir -p "$HOME/.cursor"
    fi
    if check_has_unrealmcp "$target"; then
        echo " [OK] Cursor - already configured in $target"
        return
    fi
    if [ -f "$target" ]; then
        merge_mcp_entry "$target" "mcpServers" ""
        return
    fi
    cat > "$target" << 'EOF'
{
  "mcpServers": {
    "unreal": {
      "command": "unrealmcp",
      "args": [],
      "env": {}
    }
  }
}
EOF
    echo " [OK] Cursor - $target"
}

setup_vscode() {
    local target
    if [ "$SCOPE" = "PROJECT" ]; then
        target="$PROJECT_ROOT/.vscode/mcp.json"
        mkdir -p "$PROJECT_ROOT/.vscode"
    else
        target="$HOME/.vscode/mcp.json"
        mkdir -p "$HOME/.vscode"
    fi
    if check_has_unrealmcp "$target"; then
        echo " [OK] VS Code - already configured in $target"
        return
    fi
    if [ -f "$target" ]; then
        merge_mcp_entry "$target" "servers" ""
        return
    fi
    cat > "$target" << 'EOF'
{
  "servers": {
    "unreal": {
      "command": "unrealmcp",
      "args": [],
      "env": {}
    }
  }
}
EOF
    echo " [OK] VS Code - $target"
}

setup_windsurf() {
    local target="$HOME/.codeium/windsurf/mcp_config.json"
    mkdir -p "$HOME/.codeium/windsurf"
    if check_has_unrealmcp "$target"; then
        echo " [OK] Windsurf - already configured in $target"
        return
    fi
    if [ -f "$target" ]; then
        merge_mcp_entry "$target" "mcpServers" ""
        return
    fi
    cat > "$target" << 'EOF'
{
  "mcpServers": {
    "unreal": {
      "command": "unrealmcp",
      "args": [],
      "env": {}
    }
  }
}
EOF
    echo " [OK] Windsurf - $target"
}

setup_gemini() {
    local target
    if [ "$SCOPE" = "PROJECT" ]; then
        target="$PROJECT_ROOT/.gemini/settings.json"
        mkdir -p "$PROJECT_ROOT/.gemini"
    else
        target="$HOME/.gemini/settings.json"
        mkdir -p "$HOME/.gemini"
    fi
    if check_has_unrealmcp "$target"; then
        echo " [OK] Gemini CLI - already configured in $target"
        return
    fi
    if [ -f "$target" ]; then
        merge_mcp_entry "$target" "mcpServers" ""
        return
    fi
    cat > "$target" << 'EOF'
{
  "mcpServers": {
    "unreal": {
      "command": "unrealmcp",
      "args": [],
      "env": {}
    }
  }
}
EOF
    echo " [OK] Gemini CLI - $target"
}

setup_jetbrains() {
    local target
    if [ "$SCOPE" = "PROJECT" ]; then
        target="$PROJECT_ROOT/.junie/mcp/mcp.json"
        mkdir -p "$PROJECT_ROOT/.junie/mcp"
    else
        echo " [INFO] JetBrains: Configure via Settings > Tools > MCP Server in your IDE"
        echo "        Command: unrealmcp"
        return
    fi
    if check_has_unrealmcp "$target"; then
        echo " [OK] JetBrains - already configured in $target"
        return
    fi
    if [ -f "$target" ]; then
        echo " [INFO] JetBrains uses array format. Please add manually:"
        echo '        { "name": "unreal", "command": "unrealmcp", "args": [] }'
        return
    fi
    cat > "$target" << 'EOF'
{
  "servers": [
    {
      "name": "unreal",
      "command": "unrealmcp",
      "args": [],
      "env": {}
    }
  ]
}
EOF
    echo " [OK] JetBrains - $target"
}

setup_zed() {
    local target="$HOME/.config/zed/settings.json"
    if check_has_unrealmcp "$target"; then
        echo " [OK] Zed - already configured in $target"
        return
    fi
    if [ -f "$target" ]; then
        echo " [INFO] Zed settings.json exists. Please add manually:"
        echo '        "context_servers": { "unreal": { "source": "custom", "command": { "path": "unrealmcp" } } }'
        return
    fi
    mkdir -p "$HOME/.config/zed"
    cat > "$target" << 'EOF'
{
  "context_servers": {
    "unreal": {
      "source": "custom",
      "command": {
        "path": "unrealmcp",
        "args": [],
        "env": {}
      }
    }
  }
}
EOF
    echo " [OK] Zed - $target"
}

setup_amazonq() {
    local target
    if [ "$SCOPE" = "PROJECT" ]; then
        target="$PROJECT_ROOT/.amazonq/mcp.json"
        mkdir -p "$PROJECT_ROOT/.amazonq"
    else
        target="$HOME/.aws/amazonq/mcp.json"
        mkdir -p "$HOME/.aws/amazonq"
    fi
    if check_has_unrealmcp "$target"; then
        echo " [OK] Amazon Q - already configured in $target"
        return
    fi
    if [ -f "$target" ]; then
        merge_mcp_entry "$target" "mcpServers" ""
        return
    fi
    cat > "$target" << 'EOF'
{
  "mcpServers": {
    "unreal": {
      "command": "unrealmcp",
      "args": [],
      "env": {}
    }
  }
}
EOF
    echo " [OK] Amazon Q - $target"
}

# ============================================
# Choose AI tool
# ============================================
echo ""
echo " Which AI tool do you want to configure?"
echo ""
echo "   1) Claude Code"
echo "   2) Cursor"
echo "   3) VS Code / Copilot"
echo "   4) Windsurf"
echo "   5) Gemini CLI"
echo "   6) JetBrains / Rider"
echo "   7) Zed"
echo "   8) Amazon Q"
echo "   9) All of the above"
echo "   0) Skip"
echo ""
read -p "  Enter choice (0-9): " TOOL_CHOICE

case "${TOOL_CHOICE:-0}" in
    1) setup_claude ;;
    2) setup_cursor ;;
    3) setup_vscode ;;
    4) setup_windsurf ;;
    5) setup_gemini ;;
    6) setup_jetbrains ;;
    7) setup_zed ;;
    8) setup_amazonq ;;
    9)
        setup_claude
        setup_cursor
        setup_vscode
        setup_windsurf
        setup_gemini
        setup_jetbrains
        setup_zed
        setup_amazonq
        ;;
    0) echo " [SKIP] No config created" ;;
esac

echo ""
echo " ============================================"
echo "  Setup complete!"
echo " ============================================"
echo ""
echo " Next steps:"
echo "   1. Restart your AI tool (Claude Code, Cursor, etc.) to load the new config"
echo "   2. Open your project in Unreal Editor"
echo "   3. Check Output Log for: \"MCP Bridge initialized on port XXXXX\""
echo "   4. Open your AI tool in the project directory"
echo "   5. Try: \"Use the health_check tool\""
echo ""
echo " Docs: https://github.com/aadeshrao123/Unreal-MCP"
echo " Help: https://github.com/aadeshrao123/Unreal-MCP/issues"
echo ""
