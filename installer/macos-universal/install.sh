#!/bin/bash
# =============================================================================
# WS-PluginManager Installer for macOS (Universal Binary)
# =============================================================================
#
# Supports:
#   - Installing v.1.0.0 (current release)
#   - Detecting an already-installed version
#   - Upgrading and uninstalling
#
# The universal binary works on both Intel (x86_64) and Apple Silicon (arm64).
# Requires Wireshark 4.6.x (macOS binary is built against WS 4.6.x).
#
# Plugin directory:
#   Personal:   ~/.local/lib/wireshark/plugins/<version>/epan/
#   App bundle: /Applications/Wireshark.app/Contents/PlugIns/wireshark/<version>/epan/
#
# Usage:
#   chmod +x install.sh && ./install.sh
# =============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PLUGIN_NAME="ws_pluginmgr.so"
CURRENT_VERSION="1.0.0"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

printf "\n"
printf "${BLUE}╔══════════════════════════════════════════════════╗${NC}\n"
printf "${BLUE}║   WS-PluginManager Installer for macOS           ║${NC}\n"
printf "${BLUE}║   Universal Binary (Intel + Apple Silicon)       ║${NC}\n"
printf "${BLUE}║   v.1.0.0  —  requires Wireshark 4.6.x           ║${NC}\n"
printf "${BLUE}╚══════════════════════════════════════════════════╝${NC}\n"
printf "\n"

# --- Detect Wireshark ---
WIRESHARK_APP=""
WS_VERSION=""

if [ -d "/Applications/Wireshark.app" ]; then
    WIRESHARK_APP="/Applications/Wireshark.app"
elif [ -d "$HOME/Applications/Wireshark.app" ]; then
    WIRESHARK_APP="$HOME/Applications/Wireshark.app"
fi

if [ -n "$WIRESHARK_APP" ]; then
    WS_VERSION=$(/usr/libexec/PlistBuddy -c "Print :CFBundleShortVersionString" \
        "$WIRESHARK_APP/Contents/Info.plist" 2>/dev/null || true)
fi

if [ -z "$WS_VERSION" ] && command -v tshark >/dev/null 2>&1; then
    WS_VERSION=$(tshark --version 2>/dev/null | head -1 | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' | head -1)
fi

if [ -z "$WS_VERSION" ]; then
    printf "${YELLOW}Warning: Could not detect Wireshark version.${NC}\n"
    printf "Enter Wireshark major.minor version (e.g., 4.6): "
    read -r WS_VERSION_INPUT
    WS_VERSION="${WS_VERSION_INPUT}.0"
fi

WS_MAJOR=$(printf "%s" "$WS_VERSION" | cut -d. -f1)
WS_MINOR=$(printf "%s" "$WS_VERSION" | cut -d. -f2)
printf "${GREEN}✓${NC} Wireshark version: ${CYAN}%s${NC}\n" "$WS_VERSION"

if [ "$WS_MAJOR" != "4" ] || [ "$WS_MINOR" != "6" ]; then
    printf "\n${YELLOW}Warning: This binary was built against Wireshark 4.6.x.${NC}\n"
    printf "  Your version: ${YELLOW}%s${NC} — the plugin may fail to load.\n" "$WS_VERSION"
    printf "  Install anyway? [y/N]: "
    read -r FORCE
    if [ "$FORCE" != "y" ] && [ "$FORCE" != "Y" ]; then
        printf "Installation cancelled.\n"; exit 1
    fi
fi

# --- Determine plugin directory ---
PLUGIN_PATH_ID=""

if [ -n "$WIRESHARK_APP" ]; then
    for dir in "$WIRESHARK_APP/Contents/PlugIns/wireshark"/*/; do
        b=$(basename "$dir" 2>/dev/null) || continue
        if printf "%s" "$b" | grep -qE "^${WS_MAJOR}[.-]${WS_MINOR}$"; then
            PLUGIN_PATH_ID="$b"; break
        fi
    done
fi

if [ -z "$PLUGIN_PATH_ID" ]; then
    for base in "$HOME/.local/lib/wireshark/plugins" \
                "$HOME/Library/Application Support/Wireshark/plugins"; do
        [ -d "$base" ] || continue
        for dir in "$base"/*/; do
            b=$(basename "$dir" 2>/dev/null) || continue
            if printf "%s" "$b" | grep -qE "^${WS_MAJOR}[.-]${WS_MINOR}$"; then
                PLUGIN_PATH_ID="$b"; break 2
            fi
        done
    done
fi

[ -z "$PLUGIN_PATH_ID" ] && PLUGIN_PATH_ID="${WS_MAJOR}-${WS_MINOR}"

PERSONAL_PLUGIN_DIR="$HOME/.local/lib/wireshark/plugins/$PLUGIN_PATH_ID/epan"
SYSTEM_PLUGIN_DIR=""
[ -n "$WIRESHARK_APP" ] && \
    SYSTEM_PLUGIN_DIR="$WIRESHARK_APP/Contents/PlugIns/wireshark/$PLUGIN_PATH_ID/epan"

# --- Detect currently installed version ---
INSTALLED_VERSION=""
INSTALLED_PATH=""
for dir in "$PERSONAL_PLUGIN_DIR" "$SYSTEM_PLUGIN_DIR"; do
    [ -n "$dir" ] || continue
    [ -f "$dir/$PLUGIN_NAME" ] || continue
    INSTALLED_VERSION=$(strings "$dir/$PLUGIN_NAME" 2>/dev/null \
        | grep -oE 'ws_pluginmgr [0-9]+\.[0-9]+\.[0-9]+' | head -1 \
        | grep -oE '[0-9]+\.[0-9]+\.[0-9]+')
    [ -z "$INSTALLED_VERSION" ] && INSTALLED_VERSION="unknown"
    INSTALLED_PATH="$dir/$PLUGIN_NAME"
    break
done

if [ -n "$INSTALLED_PATH" ]; then
    printf "${GREEN}✓${NC} Currently installed: ${CYAN}v.%s${NC}\n" "$INSTALLED_VERSION"
    printf "  Location: %s\n" "$INSTALLED_PATH"
else
    printf "  No existing installation found.\n"
fi

# --- Main menu ---
printf "\n"
printf "What would you like to do?\n\n"
printf "  ${GREEN}i${NC}) Install / upgrade\n"
printf "  ${RED}u${NC}) Uninstall\n"
printf "  ${YELLOW}q${NC}) Quit\n\n"
printf "Choice [i]: "
read -r ACTION
ACTION=${ACTION:-i}

case "$ACTION" in
    u|U)
        if [ -z "$INSTALLED_PATH" ]; then
            printf "\n${YELLOW}WS-PluginManager is not currently installed.${NC}\n\n"
            exit 0
        fi
        printf "\nRemove: ${CYAN}%s${NC}\n" "$INSTALLED_PATH"
        printf "Confirm uninstall? [y/N]: "
        read -r CONFIRM
        if [ "$CONFIRM" = "y" ] || [ "$CONFIRM" = "Y" ]; then
            rm -f "$INSTALLED_PATH"
            printf "\n${GREEN}✓ WS-PluginManager uninstalled.${NC}\n\n"
        else
            printf "Uninstall cancelled.\n"
        fi
        exit 0
        ;;
    q|Q) printf "Bye.\n"; exit 0 ;;
    i|I|"") ;;
    *) printf "Invalid choice. Exiting.\n"; exit 1 ;;
esac

# --- Binary check ---
PLUGIN_FILE="$SCRIPT_DIR/v.${CURRENT_VERSION}/$PLUGIN_NAME"
if [ ! -f "$PLUGIN_FILE" ]; then
    printf "${RED}Error: Binary not found: %s${NC}\n" "$PLUGIN_FILE"
    exit 1
fi

printf "\n${GREEN}✓${NC} Binary: %s\n" "$PLUGIN_FILE"
printf "  Architecture: "
file "$PLUGIN_FILE" | grep -o "universal binary.*" || file "$PLUGIN_FILE" | grep -o "Mach-O.*"

# --- Choose install location ---
printf "\n"
printf "Where would you like to install?\n\n"
printf "  ${GREEN}1${NC}) Personal directory (recommended — survives Wireshark updates)\n"
printf "     %s\n" "$PERSONAL_PLUGIN_DIR"
if [ -n "$SYSTEM_PLUGIN_DIR" ]; then
    printf "\n  ${YELLOW}2${NC}) Application bundle (wiped on Wireshark updates)\n"
    printf "     %s\n" "$SYSTEM_PLUGIN_DIR"
fi
printf "\nChoice [1]: "
read -r LOC_CHOICE
LOC_CHOICE=${LOC_CHOICE:-1}

if [ "$LOC_CHOICE" = "2" ] && [ -n "$SYSTEM_PLUGIN_DIR" ]; then
    INSTALL_DIR="$SYSTEM_PLUGIN_DIR"
    NEED_SUDO=true
else
    INSTALL_DIR="$PERSONAL_PLUGIN_DIR"
    NEED_SUDO=false
fi

# --- Install ---
printf "\n${BLUE}Installing to: %s${NC}\n" "$INSTALL_DIR"
if [ "$NEED_SUDO" = true ]; then
    sudo mkdir -p "$INSTALL_DIR"
    sudo cp "$PLUGIN_FILE" "$INSTALL_DIR/$PLUGIN_NAME"
    sudo chmod 644 "$INSTALL_DIR/$PLUGIN_NAME"
else
    mkdir -p "$INSTALL_DIR"
    cp "$PLUGIN_FILE" "$INSTALL_DIR/$PLUGIN_NAME"
    chmod 644 "$INSTALL_DIR/$PLUGIN_NAME"
fi

# --- Done ---
if [ -f "$INSTALL_DIR/$PLUGIN_NAME" ]; then
    printf "\n"
    printf "${GREEN}╔══════════════════════════════════════════════════╗${NC}\n"
    printf "${GREEN}║      Installation successful!                    ║${NC}\n"
    printf "${GREEN}╚══════════════════════════════════════════════════╝${NC}\n"
    printf "\n"
    printf "  Installed:  WS-PluginManager ${CYAN}v.%s${NC}\n" "$CURRENT_VERSION"
    printf "  Location:   ${BLUE}%s/%s${NC}\n" "$INSTALL_DIR" "$PLUGIN_NAME"
    printf "\n"
    printf "  Next steps:\n"
    printf "  1. Restart Wireshark (if running)\n"
    printf "  2. Open Tools menu — Plugin Manager appears at the bottom\n"
    printf "  3. Click 'Manage Plugins…' to open the plugin browser\n"
    printf "\n"
    printf "  To uninstall, run this script again and choose 'u'.\n\n"
else
    printf "${RED}Error: Installation failed.${NC}\n"; exit 1
fi
