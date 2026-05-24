#!/bin/bash
# =============================================================================
# WS-PluginManager Installer for Linux (x86_64)
# =============================================================================
#
# Supports:
#   - Installing v.1.0.0 (current release)
#   - Auto-detecting Wireshark version (4.0.x, 4.2.x, 4.4.x, 4.6.x)
#   - Detecting an already-installed version
#   - Upgrading and uninstalling
#
# Binaries (next to this script):
#   v.1.0.0/ws_pluginmgr-ws40.so   Wireshark 4.0.x
#   v.1.0.0/ws_pluginmgr-ws42.so   Wireshark 4.2.x
#   v.1.0.0/ws_pluginmgr-ws44.so   Wireshark 4.4.x
#   v.1.0.0/ws_pluginmgr-ws46.so   Wireshark 4.6.x
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
GRAY='\033[0;90m'
NC='\033[0m'

printf "\n"
printf "${BLUE}╔══════════════════════════════════════════════════╗${NC}\n"
printf "${BLUE}║   WS-PluginManager Installer for Linux           ║${NC}\n"
printf "${BLUE}║   x86_64 (64-bit Intel/AMD)                      ║${NC}\n"
printf "${BLUE}║   Supports Wireshark 4.0.x / 4.2.x / 4.4.x / 4.6.x ║${NC}\n"
printf "${BLUE}╚══════════════════════════════════════════════════╝${NC}\n"
printf "\n"

# --- Architecture check ---
ARCH=$(uname -m)
if [ "$ARCH" != "x86_64" ]; then
    printf "${YELLOW}Warning: These binaries are built for x86_64 but you are running %s.${NC}\n" "$ARCH"
    printf "Continue anyway? [y/N]: "
    read -r CONTINUE
    if [ "$CONTINUE" != "y" ] && [ "$CONTINUE" != "Y" ]; then exit 1; fi
fi

# =============================================================================
# PREREQUISITES
# =============================================================================
printf "Checking prerequisites...\n\n"

# --- Verify installer binaries ---
printf "  Plugin binaries in this installer:\n"
BINARIES_OK=1
for ws in ws40 ws42 ws44 ws46; do
    f="$SCRIPT_DIR/v.${CURRENT_VERSION}/ws_pluginmgr-${ws}.so"
    if [ -f "$f" ]; then
        sz=$(ls -lh "$f" | awk '{print $5}')
        printf "    ${GREEN}[FOUND]${NC}   v.%s/ws_pluginmgr-%s.so  (%s)\n" "$CURRENT_VERSION" "$ws" "$sz"
    else
        printf "    ${GRAY}[missing]${NC} v.%s/ws_pluginmgr-%s.so\n" "$CURRENT_VERSION" "$ws"
        BINARIES_OK=0
    fi
done

INSTALL_ONLY_WARN=0
if [ "$BINARIES_OK" = "0" ]; then
    printf "\n${YELLOW}Warning: Some installer binaries are missing — uninstall still available.${NC}\n"
    INSTALL_ONLY_WARN=1
fi

# --- Detect Wireshark version ---
extract_dpkg_version() {
    sed 's/^[0-9]*://' | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' | head -1
}

WS_VERSION=""
printf "\n  Searching for Wireshark:\n"

if command -v tshark >/dev/null 2>&1; then
    TSHARK_PATH=$(command -v tshark)
    WS_VERSION=$(tshark --version 2>/dev/null | head -1 | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' | head -1)
    if [ -n "$WS_VERSION" ]; then
        printf "    ${GREEN}[FOUND]${NC}   tshark at %s  →  version %s\n" "$TSHARK_PATH" "$WS_VERSION"
    else
        printf "    ${YELLOW}[found]${NC}   tshark at %s  (could not parse version)\n" "$TSHARK_PATH"
    fi
else
    printf "    ${GRAY}[not found]${NC} tshark not on PATH\n"
fi

if [ -z "$WS_VERSION" ] && command -v wireshark >/dev/null 2>&1; then
    WS_PATH=$(command -v wireshark)
    WS_VERSION=$(wireshark --version 2>/dev/null | head -1 | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' | head -1)
    [ -n "$WS_VERSION" ] && \
        printf "    ${GREEN}[FOUND]${NC}   wireshark at %s  →  version %s\n" "$WS_PATH" "$WS_VERSION"
fi

if [ -z "$WS_VERSION" ] && command -v dpkg-query >/dev/null 2>&1; then
    for pkg in wireshark-common wireshark wireshark-qt libwireshark-data; do
        if dpkg-query -W -f='${Status}' "$pkg" 2>/dev/null | grep -q "install ok installed"; then
            WS_VERSION=$(dpkg-query -W -f='${Version}' "$pkg" 2>/dev/null | extract_dpkg_version)
            if [ -n "$WS_VERSION" ]; then
                printf "    ${GREEN}[FOUND]${NC}   dpkg: %-26s  →  version %s\n" "$pkg" "$WS_VERSION"
                break
            fi
        fi
    done
fi

if [ -z "$WS_VERSION" ] && command -v rpm >/dev/null 2>&1; then
    for pkg in wireshark wireshark-qt wireshark-cli; do
        WS_VERSION=$(rpm -q "$pkg" 2>/dev/null | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' | head -1)
        if [ -n "$WS_VERSION" ]; then
            printf "    ${GREEN}[FOUND]${NC}   rpm: %-28s  →  version %s\n" "$pkg" "$WS_VERSION"
            break
        fi
    done
fi

if [ -z "$WS_VERSION" ] && command -v pacman >/dev/null 2>&1; then
    WS_VERSION=$(pacman -Q wireshark-qt 2>/dev/null | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' | head -1)
    [ -n "$WS_VERSION" ] && \
        printf "    ${GREEN}[FOUND]${NC}   pacman: wireshark-qt  →  version %s\n" "$WS_VERSION"
fi

if [ -z "$WS_VERSION" ]; then
    for lib in /usr/lib/x86_64-linux-gnu/libwireshark.so \
               /usr/lib64/libwireshark.so \
               /usr/lib/libwireshark.so; do
        [ -L "$lib" ] || [ -f "$lib" ] || continue
        SONAME=$(readlink -f "$lib" 2>/dev/null | grep -oE 'libwireshark\.so\.[0-9]+' | grep -oE '[0-9]+$')
        case "$SONAME" in
            16) WS_VERSION="4.0.0" ;; 17) WS_VERSION="4.2.0" ;;
            18) WS_VERSION="4.4.0" ;; 19) WS_VERSION="4.6.0" ;;
        esac
        if [ -n "$WS_VERSION" ]; then
            printf "    ${GREEN}[FOUND]${NC}   %s (soname .%s)  →  %s\n" "$lib" "$SONAME" "$WS_VERSION"
            break
        fi
    done
fi

if [ -z "$WS_VERSION" ]; then
    printf "\n  ${YELLOW}[WARN] Could not detect Wireshark version automatically.${NC}\n"
    printf "  Enter Wireshark major.minor version (e.g., 4.0, 4.2, 4.6): "
    read -r WS_VERSION_INPUT
    WS_VERSION="${WS_VERSION_INPUT}.0"
fi

WS_MAJOR=$(printf "%s" "$WS_VERSION" | cut -d. -f1)
WS_MINOR=$(printf "%s" "$WS_VERSION" | cut -d. -f2)

# --- Select binary tag ---
case "$WS_MINOR" in
    0) WS_TAG="ws40"; WS_LABEL="Wireshark 4.0.x (built against 4.0.17)" ;;
    2) WS_TAG="ws42"; WS_LABEL="Wireshark 4.2.x (built against 4.2.14)" ;;
    4) WS_TAG="ws44"; WS_LABEL="Wireshark 4.4.x (built against 4.4.14)" ;;
    6) WS_TAG="ws46"; WS_LABEL="Wireshark 4.6.x (built against 4.6.5)"  ;;
    *)
        printf "\n${RED}Unsupported Wireshark version: %s.%s${NC}\n" "$WS_MAJOR" "$WS_MINOR"
        printf "Supported: 4.0.x, 4.2.x, 4.4.x, 4.6.x\n\n"
        printf "Force-install a binary anyway?\n"
        printf "  1) 4.0.x binary\n  2) 4.2.x binary\n  3) 4.4.x binary\n  4) 4.6.x binary\n  q) Quit\n"
        printf "Choice [q]: "
        read -r MC
        case "$MC" in
            1) WS_TAG="ws40"; WS_LABEL="Wireshark 4.0.x (FORCED)" ;;
            2) WS_TAG="ws42"; WS_LABEL="Wireshark 4.2.x (FORCED)" ;;
            3) WS_TAG="ws44"; WS_LABEL="Wireshark 4.4.x (FORCED)" ;;
            4) WS_TAG="ws46"; WS_LABEL="Wireshark 4.6.x (FORCED)" ;;
            *) printf "Installation cancelled.\n"; exit 1 ;;
        esac
        printf "${YELLOW}Warning: Installing binary for a non-matching version.${NC}\n"
        ;;
esac

# --- Determine plugin directory ---
printf "\n  Searching for plugin directory:\n"
PLUGIN_PATH_ID=""
for dir in /usr/lib/x86_64-linux-gnu/wireshark/plugins/* \
           /usr/lib64/wireshark/plugins/* \
           /usr/lib/wireshark/plugins/* \
           "$HOME/.local/lib/wireshark/plugins"/*; do
    [ -d "$dir" ] || continue
    DIRNAME=$(basename "$dir")
    if printf "%s" "$DIRNAME" | grep -qE '^[0-9]+[-\.][0-9]+$'; then
        DIR_MINOR=$(printf "%s" "$DIRNAME" | sed 's/[^0-9]/ /g' | awk '{print $2}')
        if [ "$DIR_MINOR" = "$WS_MINOR" ]; then
            printf "    ${GREEN}[MATCH]${NC}   %s\n" "$dir"
            PLUGIN_PATH_ID="$DIRNAME"; break
        else
            printf "    ${GRAY}[skip]${NC}    %s\n" "$dir"
        fi
    fi
done
if [ -z "$PLUGIN_PATH_ID" ]; then
    PLUGIN_PATH_ID="${WS_MAJOR}.${WS_MINOR}"
    printf "    ${YELLOW}No existing plugin dir found; will create: %s${NC}\n" "$PLUGIN_PATH_ID"
fi

INSTALL_DIR="$HOME/.local/lib/wireshark/plugins/$PLUGIN_PATH_ID/epan"

# --- Detect currently installed ---
printf "\n  Checking for existing installation:\n"
INSTALLED_PATH=""
INSTALLED_VERSION=""
if [ -f "$INSTALL_DIR/$PLUGIN_NAME" ]; then
    INSTALLED_VERSION=$(strings "$INSTALL_DIR/$PLUGIN_NAME" 2>/dev/null \
        | grep -oE 'ws_pluginmgr [0-9]+\.[0-9]+\.[0-9]+' | head -1 \
        | grep -oE '[0-9]+\.[0-9]+\.[0-9]+')
    [ -z "$INSTALLED_VERSION" ] && INSTALLED_VERSION="unknown"
    INSTALLED_PATH="$INSTALL_DIR/$PLUGIN_NAME"
    printf "    ${GREEN}[FOUND]${NC}   v.%s at %s\n" "$INSTALLED_VERSION" "$INSTALLED_PATH"
else
    printf "    ${GRAY}[none]${NC}    %s  (not installed)\n" "$INSTALL_DIR/$PLUGIN_NAME"
fi

# --- Qt6 check ---
QT6_OK=0
for libdir in /usr/lib/x86_64-linux-gnu /usr/lib64 /usr/lib; do
    if [ -f "$libdir/libQt6Widgets.so.6" ] || [ -f "$libdir/libQt6Core.so.6" ]; then
        QT6_OK=1; break
    fi
done

# --- Prerequisites summary ---
SEP="-----------------------------------------------------------"
printf "\n%s\n" "$SEP"
printf "  Prerequisites Summary\n"
printf "%s\n" "$SEP"
printf "  Wireshark       : %s.%s  (plugin dir: %s)\n" "$WS_MAJOR" "$WS_MINOR" "$PLUGIN_PATH_ID"
printf "  Binary          : ws_pluginmgr-%s.so  (%s)\n" "$WS_TAG" "$WS_LABEL"
printf "  v.%s binaries: " "$CURRENT_VERSION"
if [ "$BINARIES_OK" = "1" ]; then printf "${GREEN}all present${NC}\n"; else printf "${YELLOW}some missing${NC}\n"; fi
printf "  Qt6 runtime     : "
if [ "$QT6_OK" = "1" ]; then printf "${GREEN}found${NC}\n"
else
    printf "${YELLOW}not found${NC}\n"
    printf "  ${YELLOW}Qt6 is required. Install with:${NC}\n"
    printf "    sudo apt-get install libqt6widgets6    # Debian/Ubuntu\n"
    printf "    sudo dnf install qt6-qtbase            # Fedora/RHEL\n"
    printf "    sudo pacman -S qt6-base                # Arch\n"
fi
printf "  Install dir     : %s\n" "$INSTALL_DIR"
printf "  Installed now   : "
if [ -n "$INSTALLED_VERSION" ]; then printf "${CYAN}v.%s${NC}\n" "$INSTALLED_VERSION"
else printf "${GRAY}none${NC}\n"; fi
printf "%s\n\n" "$SEP"

printf "Press Enter to continue, or Ctrl+C to abort: "
read -r _PAUSE

# --- Main menu ---
printf "\nWhat would you like to do?\n\n"
printf "  ${GREEN}i${NC}) Install / upgrade\n"
printf "  ${RED}u${NC}) Uninstall\n"
printf "  ${YELLOW}q${NC}) Quit\n\n"
printf "Choice [i]: "
read -r ACTION
ACTION=${ACTION:-i}

case "$ACTION" in
    u|U)
        # Uninstall: look in all likely dirs
        FOUND_ANY=0
        for search_dir in \
            "$HOME/.local/lib/wireshark/plugins"/*/epan \
            /usr/lib/x86_64-linux-gnu/wireshark/plugins/*/epan \
            /usr/lib64/wireshark/plugins/*/epan; do
            TARGET="$search_dir/$PLUGIN_NAME"
            if [ -f "$TARGET" ]; then
                printf "  Remove: ${CYAN}%s${NC}\n" "$TARGET"
                rm -f "$TARGET"
                printf "  ${GREEN}✓ Removed.${NC}\n"
                FOUND_ANY=1
            fi
        done
        if [ "$FOUND_ANY" = "0" ]; then
            printf "\n${YELLOW}WS-PluginManager is not currently installed.${NC}\n"
        else
            printf "\n${GREEN}✓ WS-PluginManager uninstalled. Restart Wireshark.${NC}\n"
        fi
        printf "\n"; exit 0
        ;;
    q|Q) printf "Bye.\n"; exit 0 ;;
    i|I|"")
        if [ "$INSTALL_ONLY_WARN" = "1" ]; then
            printf "\n${RED}Cannot install: binary for %s not found in this package.${NC}\n\n" "$WS_TAG"
            printf "  Download the full installer from the PacketCircle GitHub repository.\n\n"
            exit 1
        fi
        ;;
    *) printf "Invalid choice. Exiting.\n"; exit 1 ;;
esac

# --- Install ---
PLUGIN_FILE="$SCRIPT_DIR/v.${CURRENT_VERSION}/ws_pluginmgr-${WS_TAG}.so"
if [ ! -f "$PLUGIN_FILE" ]; then
    printf "\n${RED}Error: Binary not found: %s${NC}\n\n" "$PLUGIN_FILE"
    exit 1
fi

printf "\n${BLUE}Installing to: %s${NC}\n" "$INSTALL_DIR"
mkdir -p "$INSTALL_DIR"
cp "$PLUGIN_FILE" "$INSTALL_DIR/$PLUGIN_NAME"
chmod 644 "$INSTALL_DIR/$PLUGIN_NAME"

if [ -f "$INSTALL_DIR/$PLUGIN_NAME" ]; then
    printf "\n"
    printf "${GREEN}╔══════════════════════════════════════════════════╗${NC}\n"
    printf "${GREEN}║      Installation successful!                    ║${NC}\n"
    printf "${GREEN}╚══════════════════════════════════════════════════╝${NC}\n"
    printf "\n"
    printf "  Installed:  WS-PluginManager ${CYAN}v.%s${NC}  (%s)\n" "$CURRENT_VERSION" "$WS_LABEL"
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
