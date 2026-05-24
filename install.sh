#!/bin/bash
# WS-PluginManager — build, fix dylib references, and install
# Mirrors the PacketCircle install.sh pattern.

set -euo pipefail

ARCH="${ARCH:-arm64}"   # override with: ARCH=x86_64 ./install.sh

WS_SRC="/Users/walterh/dev/wireshark-src/wireshark-4.6.5"
BUILD_DIR="$WS_SRC/build-$ARCH"
PLUGIN_SRC="$BUILD_DIR/run/Wireshark.app/Contents/PlugIns/wireshark/4-6/epan/ws_pluginmgr.so"
PLUGIN_DST="$HOME/.local/lib/wireshark/plugins/4-6/epan/ws_pluginmgr.so"

# ------------------------------------------------------------------
# 1. Ensure the plugin source tree is wired into the Wireshark build.
#
#    The CMakeLists.txt in this repo's src/ uses include(WiresharkPlugin)
#    which must be run from within the Wireshark CMake build system.
#    We create a symlink under plugins/epan/ and patch the parent
#    CMakeLists if it hasn't been done already.
# ------------------------------------------------------------------
PLUGINS_DIR="$WS_SRC/plugins/epan"
LINK="$PLUGINS_DIR/ws_pluginmgr"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ ! -e "$LINK" ]; then
    echo "=== Linking plugin source into Wireshark tree ==="
    ln -s "$SCRIPT_DIR/src" "$LINK"
fi

# The Wireshark main CMakeLists.txt drives the build via PLUGIN_SRC_DIRS —
# each custom plugin path must appear in that list.  We patch the list to
# add our entry before the existing ${CUSTOM_PLUGIN_SRC_DIR} placeholder.
MAIN_CMAKELISTS="$WS_SRC/CMakeLists.txt"
NEEDS_RECONFIGURE=false
if ! grep -q "plugins/epan/ws_pluginmgr" "$MAIN_CMAKELISTS"; then
    echo "=== Adding ws_pluginmgr to PLUGIN_SRC_DIRS in main CMakeLists.txt ==="
    sed -i '' 's|		\${CUSTOM_PLUGIN_SRC_DIR}|		plugins/epan/ws_pluginmgr\n		${CUSTOM_PLUGIN_SRC_DIR}|' "$MAIN_CMAKELISTS"
    NEEDS_RECONFIGURE=true
fi

# ------------------------------------------------------------------
# 2. Reconfigure CMake when we modified the plugin list.
#    Running cmake in-place is fast — it re-reads CMakeLists changes
#    without touching existing build objects.
# ------------------------------------------------------------------
if $NEEDS_RECONFIGURE; then
    echo "=== Reconfiguring CMake (picking up new plugin) ==="
    (cd "$BUILD_DIR" && cmake .)
fi

# ------------------------------------------------------------------
# 3. Build
# ------------------------------------------------------------------
echo "=== Building ws_pluginmgr ($ARCH) ==="
make -C "$BUILD_DIR" ws_pluginmgr -j"$(sysctl -n hw.logicalcpu)"

# ------------------------------------------------------------------
# 4. Fix dylib @rpath references (macOS only)
# ------------------------------------------------------------------
if [[ "$(uname)" == "Darwin" ]]; then
    echo "=== Fixing dylib references ==="

    for homebrew_glib in \
        /opt/homebrew/opt/glib/lib/libglib-2.0.0.dylib \
        /opt/homebrew/lib/libglib-2.0.0.dylib; do
        install_name_tool -change "$homebrew_glib" \
            @rpath/libglib-2.0.0.dylib "$PLUGIN_SRC" 2>/dev/null || true
    done

    for homebrew_xxhash in \
        /opt/homebrew/opt/xxhash/lib/libxxhash.0.dylib \
        @rpath/libxxhash.0.dylib; do
        install_name_tool -change "$homebrew_xxhash" \
            @rpath/libxxhash.0.8.3.dylib "$PLUGIN_SRC" 2>/dev/null || true
    done

    # Strip stale build-time rpaths — Wireshark's own rpath covers everything.
    while IFS= read -r rp; do
        install_name_tool -delete_rpath "$rp" "$PLUGIN_SRC" 2>/dev/null || true
    done < <(otool -l "$PLUGIN_SRC" | awk '/LC_RPATH/{getline;getline;print $2}')
fi

# ------------------------------------------------------------------
# 5. Install to personal plugin directory
# ------------------------------------------------------------------
echo "=== Installing to $PLUGIN_DST ==="
mkdir -p "$(dirname "$PLUGIN_DST")"
cp "$PLUGIN_SRC" "$PLUGIN_DST"

# ------------------------------------------------------------------
# 6. Verify
# ------------------------------------------------------------------
echo ""
echo "=== Installed: $PLUGIN_DST ==="
if [[ "$(uname)" == "Darwin" ]]; then
    echo ""
    echo "Dylib references:"
    otool -L "$PLUGIN_DST" \
        | grep -v "^$PLUGIN_DST" \
        | grep -v "/System/" \
        | grep -v "/usr/lib/" || true
    echo ""
    echo "Rpaths (should be empty):"
    otool -l "$PLUGIN_DST" | grep -A2 LC_RPATH || echo "  (none — good)"
fi
echo ""
echo "Restart Wireshark and look for  Tools → Manage Plugins…"
