@echo off
setlocal enabledelayedexpansion

echo.
echo  ============================================
echo   UnrealMCP Setup
echo   Configure MCP for your AI tool
echo  ============================================
echo.

:: Check for Python
python --version >nul 2>&1
if errorlevel 1 (
    echo  [ERROR] Python not found. Please install Python 3.10+ and add it to PATH.
    echo  Download: https://www.python.org/downloads/
    echo.
    pause
    exit /b 1
)

echo  [OK] Python found
for /f "tokens=*" %%v in ('python --version 2^>^&1') do echo       %%v
echo.

:: Install pip package
echo  Installing unrealmcp Python package...
pip install unrealmcp >nul 2>&1
if errorlevel 1 (
    echo  [WARN] pip install failed. Trying with --user flag...
    pip install --user unrealmcp >nul 2>&1
)

where unrealmcp >nul 2>&1
if errorlevel 1 (
    echo  [ERROR] unrealmcp command not found after install.
    echo  You may need to add Python Scripts to your PATH.
    pause
    exit /b 1
)
echo  [OK] unrealmcp command ready
echo.

:: ============================================
:: Detect where we are
:: ============================================
set "SCOPE="
set "PROJECT_ROOT="
set "UPROJECT="

:: Search upward for .uproject (handles running from subfolders like Plugins\UnrealMCP)
set "SEARCH_DIR=%CD%"
:find_uproject
for %%f in ("!SEARCH_DIR!\*.uproject") do (
    set "UPROJECT=%%~nxf"
    set "PROJECT_ROOT=!SEARCH_DIR!"
)
if not defined UPROJECT (
    for %%d in ("!SEARCH_DIR!\..") do set "PARENT=%%~fd"
    if not "!PARENT!"=="!SEARCH_DIR!" (
        set "SEARCH_DIR=!PARENT!"
        goto :find_uproject
    )
)

:: Check if we're inside an Engine folder
set "IN_ENGINE=0"
set "CHECK_DIR=%CD%"
:check_engine
for %%d in ("!CHECK_DIR!") do set "DIRNAME=%%~nxd"
if /i "!DIRNAME!"=="Engine" set "IN_ENGINE=1"
if !IN_ENGINE!==0 (
    for %%d in ("!CHECK_DIR!\..") do set "CHECK_PARENT=%%~fd"
    if not "!CHECK_PARENT!"=="!CHECK_DIR!" (
        set "CHECK_DIR=!CHECK_PARENT!"
        goto :check_engine
    )
)

if defined UPROJECT if !IN_ENGINE!==0 goto :scope_project
if !IN_ENGINE!==1 goto :scope_engine
goto :scope_global

:scope_project
echo  Detected: UE project - !UPROJECT!
echo  Location: !PROJECT_ROOT!
echo.
echo  Where should the MCP config be created?
echo.
echo    1^) This project only (project scope^)
echo    2^) All projects (global scope^)
echo.
set /p SCOPE_CHOICE="  Enter choice (1-2): "
if "!SCOPE_CHOICE!"=="2" (
    set "SCOPE=GLOBAL"
) else (
    set "SCOPE=PROJECT"
)
goto :scope_done

:scope_engine
echo  Detected: Inside Unreal Engine folder
echo  MCP config will be set globally (user-level^).
echo.
set "SCOPE=GLOBAL"
goto :scope_done

:scope_global
echo  No UE project or engine detected.
echo  MCP config will be set globally (user-level^).
echo.
set "SCOPE=GLOBAL"

:scope_done

echo.

:: ============================================
:: Choose AI tool
:: ============================================
echo  Which AI tool do you want to configure?
echo.
echo    1) Claude Code
echo    2) Cursor
echo    3) VS Code / Copilot
echo    4) Windsurf
echo    5) Gemini CLI
echo    6) JetBrains / Rider
echo    7) Zed
echo    8) Amazon Q
echo    9) All of the above
echo    0) Skip
echo.
set /p TOOL_CHOICE="  Enter choice (0-9): "

if "%TOOL_CHOICE%"=="1" call :setup_claude
if "%TOOL_CHOICE%"=="2" call :setup_cursor
if "%TOOL_CHOICE%"=="3" call :setup_vscode
if "%TOOL_CHOICE%"=="4" call :setup_windsurf
if "%TOOL_CHOICE%"=="5" call :setup_gemini
if "%TOOL_CHOICE%"=="6" call :setup_jetbrains
if "%TOOL_CHOICE%"=="7" call :setup_zed
if "%TOOL_CHOICE%"=="8" call :setup_amazonq
if "%TOOL_CHOICE%"=="9" (
    call :setup_claude
    call :setup_cursor
    call :setup_vscode
    call :setup_windsurf
    call :setup_gemini
    call :setup_jetbrains
    call :setup_zed
    call :setup_amazonq
)
if "%TOOL_CHOICE%"=="0" echo  [SKIP] No config created

echo.
echo  ============================================
echo   Setup complete!
echo  ============================================
echo.
echo  Next steps:
echo    1. Restart your AI tool (Claude Code, Cursor, etc.) to load the new config
echo    2. Open your project in Unreal Editor
echo    3. Check Output Log for: "MCP Bridge initialized on port XXXXX"
echo    4. Open your AI tool in the project directory
echo    5. Try: "Use the health_check tool"
echo.
echo  Docs: https://github.com/aadeshrao123/Unreal-MCP
echo  Help: https://github.com/aadeshrao123/Unreal-MCP/issues
echo.
pause
exit /b 0


:: ================================================================
:: Helper: Check if unrealmcp already configured in a file
:: Sets ALREADY_HAS=1 if "unrealmcp" found in file, else 0
:: ================================================================
:check_has_unrealmcp
set "ALREADY_HAS=0"
if exist "!TARGET!" (
    findstr /i "unrealmcp" "!TARGET!" >nul 2>nul && set "ALREADY_HAS=1"
)
exit /b

:: ================================================================
:: Helper: Merge unreal entry into existing mcpServers JSON
:: Uses PowerShell to safely add to existing config
:: ARG1 = server key path (mcpServers or servers)
:: ARG2 = extra fields (e.g. "type":"stdio",)
:: ================================================================
:merge_mcp_entry
set "ROOT_KEY=%~1"
set "EXTRA=%~2"
powershell -NoProfile -Command ^
    "$f='!TARGET!'; $j=Get-Content $f -Raw | ConvertFrom-Json; " ^
    "if (-not $j.'!ROOT_KEY!') { $j | Add-Member -Name '!ROOT_KEY!' -Value ([PSCustomObject]@{}) -MemberType NoteProperty }; " ^
    "$entry = [PSCustomObject]@{ !EXTRA! command='unrealmcp'; args=@(); env=[PSCustomObject]@{} }; " ^
    "$j.'!ROOT_KEY!' | Add-Member -Name 'unreal' -Value $entry -MemberType NoteProperty -Force; " ^
    "$j | ConvertTo-Json -Depth 10 | Set-Content $f -Encoding UTF8"
echo  [OK] Added unrealmcp to !TARGET!
exit /b

:: ================================================================
:: Config writers
:: PROJECT = write to PROJECT_ROOT, GLOBAL = user-level
:: ================================================================

:: ---- Claude Code ----
:setup_claude
if "!SCOPE!"=="PROJECT" (
    set "TARGET=!PROJECT_ROOT!\.mcp.json"
) else (
    set "TARGET=%USERPROFILE%\.claude.json"
)
call :check_has_unrealmcp
if !ALREADY_HAS!==1 (
    echo  [OK] Claude Code - already configured in !TARGET!
    exit /b
)
if exist "!TARGET!" (
    call :merge_mcp_entry "mcpServers" "'type'='stdio';"
    exit /b
)
(
echo {
echo   "mcpServers": {
echo     "unreal": {
echo       "type": "stdio",
echo       "command": "unrealmcp",
echo       "args": [],
echo       "env": {}
echo     }
echo   }
echo }
) > "!TARGET!"
echo  [OK] Claude Code - !TARGET!
exit /b

:: ---- Cursor ----
:setup_cursor
if "!SCOPE!"=="PROJECT" (
    set "TARGET=!PROJECT_ROOT!\.cursor\mcp.json"
    if not exist "!PROJECT_ROOT!\.cursor" mkdir "!PROJECT_ROOT!\.cursor"
) else (
    set "TARGET=%USERPROFILE%\.cursor\mcp.json"
    if not exist "%USERPROFILE%\.cursor" mkdir "%USERPROFILE%\.cursor"
)
call :check_has_unrealmcp
if !ALREADY_HAS!==1 (
    echo  [OK] Cursor - already configured in !TARGET!
    exit /b
)
if exist "!TARGET!" (
    call :merge_mcp_entry "mcpServers" ""
    exit /b
)
(
echo {
echo   "mcpServers": {
echo     "unreal": {
echo       "command": "unrealmcp",
echo       "args": [],
echo       "env": {}
echo     }
echo   }
echo }
) > "!TARGET!"
echo  [OK] Cursor - !TARGET!
exit /b

:: ---- VS Code / Copilot ----
:setup_vscode
if "!SCOPE!"=="PROJECT" (
    set "TARGET=!PROJECT_ROOT!\.vscode\mcp.json"
    if not exist "!PROJECT_ROOT!\.vscode" mkdir "!PROJECT_ROOT!\.vscode"
) else (
    set "TARGET=%USERPROFILE%\.vscode\mcp.json"
    if not exist "%USERPROFILE%\.vscode" mkdir "%USERPROFILE%\.vscode"
)
call :check_has_unrealmcp
if !ALREADY_HAS!==1 (
    echo  [OK] VS Code - already configured in !TARGET!
    exit /b
)
if exist "!TARGET!" (
    call :merge_mcp_entry "servers" ""
    exit /b
)
(
echo {
echo   "servers": {
echo     "unreal": {
echo       "command": "unrealmcp",
echo       "args": [],
echo       "env": {}
echo     }
echo   }
echo }
) > "!TARGET!"
echo  [OK] VS Code - !TARGET!
exit /b

:: ---- Windsurf (always user-level) ----
:setup_windsurf
set "TARGET=%USERPROFILE%\.codeium\windsurf\mcp_config.json"
if not exist "%USERPROFILE%\.codeium\windsurf" mkdir "%USERPROFILE%\.codeium\windsurf"
call :check_has_unrealmcp
if !ALREADY_HAS!==1 (
    echo  [OK] Windsurf - already configured in !TARGET!
    exit /b
)
if exist "!TARGET!" (
    call :merge_mcp_entry "mcpServers" ""
    exit /b
)
(
echo {
echo   "mcpServers": {
echo     "unreal": {
echo       "command": "unrealmcp",
echo       "args": [],
echo       "env": {}
echo     }
echo   }
echo }
) > "!TARGET!"
echo  [OK] Windsurf - !TARGET!
exit /b

:: ---- Gemini CLI ----
:setup_gemini
if "!SCOPE!"=="PROJECT" (
    set "TARGET=!PROJECT_ROOT!\.gemini\settings.json"
    if not exist "!PROJECT_ROOT!\.gemini" mkdir "!PROJECT_ROOT!\.gemini"
) else (
    set "TARGET=%USERPROFILE%\.gemini\settings.json"
    if not exist "%USERPROFILE%\.gemini" mkdir "%USERPROFILE%\.gemini"
)
call :check_has_unrealmcp
if !ALREADY_HAS!==1 (
    echo  [OK] Gemini CLI - already configured in !TARGET!
    exit /b
)
if exist "!TARGET!" (
    call :merge_mcp_entry "mcpServers" ""
    exit /b
)
(
echo {
echo   "mcpServers": {
echo     "unreal": {
echo       "command": "unrealmcp",
echo       "args": [],
echo       "env": {}
echo     }
echo   }
echo }
) > "!TARGET!"
echo  [OK] Gemini CLI - !TARGET!
exit /b

:: ---- JetBrains / Rider ----
:setup_jetbrains
if "!SCOPE!"=="PROJECT" (
    set "TARGET=!PROJECT_ROOT!\.junie\mcp\mcp.json"
    if not exist "!PROJECT_ROOT!\.junie\mcp" mkdir "!PROJECT_ROOT!\.junie\mcp"
) else (
    echo  [INFO] JetBrains: Configure via Settings ^> Tools ^> MCP Server in your IDE
    echo         Command: unrealmcp
    exit /b
)
call :check_has_unrealmcp
if !ALREADY_HAS!==1 (
    echo  [OK] JetBrains - already configured in !TARGET!
    exit /b
)
if exist "!TARGET!" (
    echo  [INFO] JetBrains uses array format. Please add manually:
    echo         { "name": "unreal", "command": "unrealmcp", "args": [] }
    exit /b
)
(
echo {
echo   "servers": [
echo     {
echo       "name": "unreal",
echo       "command": "unrealmcp",
echo       "args": [],
echo       "env": {}
echo     }
echo   ]
echo }
) > "!TARGET!"
echo  [OK] JetBrains - !TARGET!
exit /b

:: ---- Zed (always user-level) ----
:setup_zed
set "TARGET=%APPDATA%\Zed\settings.json"
call :check_has_unrealmcp
if !ALREADY_HAS!==1 (
    echo  [OK] Zed - already configured in !TARGET!
    exit /b
)
if exist "!TARGET!" (
    echo  [INFO] Zed settings.json exists. Please add manually:
    echo         "context_servers": { "unreal": { "source": "custom", "command": { "path": "unrealmcp" } } }
    exit /b
)
if not exist "%APPDATA%\Zed" mkdir "%APPDATA%\Zed"
(
echo {
echo   "context_servers": {
echo     "unreal": {
echo       "source": "custom",
echo       "command": {
echo         "path": "unrealmcp",
echo         "args": [],
echo         "env": {}
echo       }
echo     }
echo   }
echo }
) > "!TARGET!"
echo  [OK] Zed - !TARGET!
exit /b

:: ---- Amazon Q ----
:setup_amazonq
if "!SCOPE!"=="PROJECT" (
    set "TARGET=!PROJECT_ROOT!\.amazonq\mcp.json"
    if not exist "!PROJECT_ROOT!\.amazonq" mkdir "!PROJECT_ROOT!\.amazonq"
) else (
    set "TARGET=%USERPROFILE%\.aws\amazonq\mcp.json"
    if not exist "%USERPROFILE%\.aws\amazonq" mkdir "%USERPROFILE%\.aws\amazonq"
)
call :check_has_unrealmcp
if !ALREADY_HAS!==1 (
    echo  [OK] Amazon Q - already configured in !TARGET!
    exit /b
)
if exist "!TARGET!" (
    call :merge_mcp_entry "mcpServers" ""
    exit /b
)
(
echo {
echo   "mcpServers": {
echo     "unreal": {
echo       "command": "unrealmcp",
echo       "args": [],
echo       "env": {}
echo     }
echo   }
echo }
) > "!TARGET!"
echo  [OK] Amazon Q - !TARGET!
exit /b
