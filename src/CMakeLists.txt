# CMakeLists.txt for WS-PluginManager (Wireshark 4.6.x compatible)
# Wireshark plugin that provides a GUI for managing user-installed plugins.

include(WiresharkPlugin)

# Plugin name and version info (major minor micro extra)
set_module_info(ws_pluginmgr 1 0 1 0)

SET(CMAKE_AUTOMOC ON)
SET(CMAKE_AUTORCC ON)

if(USE_qt6)
    set(qtver "6")
else()
    set(qtver "5")
endif()

find_package(Qt${qtver} REQUIRED COMPONENTS Core Widgets Gui)

set(PLUGIN_NAME ws_pluginmgr)

set(DISSECTOR_SRC
    plugin_manager.c
    plugin_scanner.c
    plugin_actions.c
    plugin_profiles.c
    ui_bridge.cpp
    plugin_dialog.cpp
)

set_source_files_properties(
    ui_bridge.cpp
    plugin_dialog.cpp
    PROPERTIES LANGUAGE CXX
)

# glibc 2.38 compatibility shim (Linux only) — same pattern as PacketCircle.
if(UNIX AND NOT APPLE)
    list(APPEND DISSECTOR_SRC glibc_compat.c)
    set_source_files_properties(glibc_compat.c PROPERTIES
        COMPILE_OPTIONS  "-U_GNU_SOURCE;-D_POSIX_C_SOURCE=200809L"
        LANGUAGE         C
        SKIP_AUTOGEN     ON
    )
endif()

set(PLUGIN_FILES
    plugin.c
    ${DISSECTOR_SRC}
)

set(PLUGIN_HEADERS
    plugin_manager.h
    plugin_scanner.h
    plugin_actions.h
    plugin_profiles.h
    ui_bridge.h
    plugin_dialog.h
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/utf-8)
endif()

# Allow the plugin to load with any Qt 6.x runtime (Wireshark bundles its own Qt).
add_definitions(-DQT_NO_VERSION_TAGGING)

set_source_files_properties(
    plugin.c
    PROPERTIES
    SKIP_AUTOGEN ON
    LANGUAGE C
)

set_source_files_properties(
    ${PLUGIN_FILES}
    PROPERTIES
    COMPILE_FLAGS "${WERROR_COMMON_FLAGS}"
)

register_plugin_files(plugin.c
    plugin
    ${DISSECTOR_SRC}
)

add_wireshark_plugin_library(${PLUGIN_NAME} epan)

target_link_libraries(${PLUGIN_NAME}
    epan
    wiretap
    Qt${qtver}::Core
    Qt${qtver}::Widgets
    Qt${qtver}::Gui
    # dl is needed on Linux for dlsym(RTLD_DEFAULT, ...) used to call
    # wslua_reload_plugins at runtime without a hard Lua dependency.
    ${CMAKE_DL_LIBS}
)

install_plugin(${PLUGIN_NAME} epan)
