#!/usr/bin/env bash
# Bash/WSL equivalent of scripts/startdev.ps1.
#
# Opens the glbp dev environment: a WSL shell running Claude, a plain WSL
# shell, a PowerShell window, and CLion, all rooted in the repo - and
# records their process ids in .dev-pids.session.json for reference
# (informational only - there is no bash stopdev.sh; close windows to stop).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
DISTRO="${WSL_DISTRO_NAME:-Ubuntu-20.04}"

source "/mnt/c/Users/Ryan/source/repos/psmodules/devkit.sh"

CLAUDE_PID=""
if command -v wsl.exe >/dev/null 2>&1; then
    wsl.exe -d "$DISTRO" --cd "$REPO_ROOT" -- bash -ilc "srt claude" >/dev/null 2>&1 &
    disown
    CLAUDE_PID=$!
    echo "Started claude (PID $CLAUDE_PID)"
fi

WSLSHELL_PID=""
if command -v wsl.exe >/dev/null 2>&1; then
    wsl.exe -d "$DISTRO" --cd "$REPO_ROOT" >/dev/null 2>&1 &
    disown
    WSLSHELL_PID=$!
    echo "Started wsl shell (PID $WSLSHELL_PID)"
fi

PS_PID=""
if command -v powershell.exe >/dev/null 2>&1; then
    WIN_REPO_ROOT="$(wslpath -w "$REPO_ROOT")"
    powershell.exe -NoExit -Command "\$host.UI.RawUI.WindowTitle = 'powershell'; cd '$WIN_REPO_ROOT'" >/dev/null 2>&1 &
    disown
    PS_PID=$!
    echo "Started powershell (PID $PS_PID)"
fi

# CLion is launched fire-and-forget and deliberately not PID-tracked for
# teardown: CLion instances share a single process across open projects, so
# killing "the" CLion PID would risk force-closing unrelated projects open
# in the same IDE instance.
CLION="$(devkit_find_windows_exe clion64.exe \
    "/mnt/c/Users/Ryan/AppData/Local/Programs/CLion/bin/clion64.exe" \
    "/mnt/c/Program Files/JetBrains/CLion*/bin/clion64.exe" || true)"
if [ -n "$CLION" ]; then
    WIN_REPO_ROOT="${WIN_REPO_ROOT:-$(wslpath -w "$REPO_ROOT")}"
    "$CLION" "$WIN_REPO_ROOT" >/dev/null 2>&1 &
    disown
    echo "Started clion"
else
    echo "Could not locate CLion - open $REPO_ROOT manually"
fi

PID_FILE="$REPO_ROOT/.dev-pids.session.json"
PIDS=""
[ -n "$CLAUDE_PID" ] && PIDS="$PIDS
claude=$CLAUDE_PID"
[ -n "$WSLSHELL_PID" ] && PIDS="$PIDS
wslshell=$WSLSHELL_PID"
[ -n "$PS_PID" ] && PIDS="$PIDS
powershell=$PS_PID"
devkit_write_session "$PID_FILE" "$PIDS" ""

echo ""
echo "Session state: $PID_FILE"
