/*
 * WS-PluginManager — Wireshark Plugin Manager
 *
 * Copyright (C) 2026 Walter Hofstetter
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "plugin_manager.h"
#include "ui_bridge.h"
#include <epan/proto.h>
#include <epan/plugin_if.h>
#include <wsutil/wslog.h>

#ifdef __APPLE__
#include <dlfcn.h>
/*
 * macOS only: Wireshark's epan_cleanup() calls g_module_close() on the plugin
 * DSO before QApplication::~QApplication() runs (stack variable in main).
 * Qt's QAccessibleCache then accesses the unmapped vtable → SIGSEGV.
 * RTLD_NODELETE keeps the mapping alive through full Qt teardown.
 * Linux does not have this issue — its shutdown order is safe.
 */
static void pin_dso(void)
{
    Dl_info info;
    if (dladdr((void *)pin_dso, &info) && info.dli_fname)
        dlopen(info.dli_fname, RTLD_NOW | RTLD_NODELETE);
}
#endif

#define WS_LOG_DOMAIN "ws_pluginmgr"

/*
 * Version identification tag — searched by the installer's
 *   strings <plugin.so> | grep "ws_pluginmgr [0-9]"
 * to detect which version is currently installed.
 * Keep in sync with set_module_info() in CMakeLists.txt.
 */
const char ws_pluginmgr_version_tag[] = "ws_pluginmgr 1.0.1";

static int proto_pluginmgr = -1;
static ext_menu_t *pluginmgr_menu = NULL;

static void open_pluginmgr_cb(ext_menubar_gui_type gui_type,
                               void *gui_object, void *user_data)
{
    (void)gui_type;
    (void)gui_object;
    (void)user_data;

    pluginmgr_open_dialog();
}

void proto_register_ws_pluginmgr(void)
{
#ifdef __APPLE__
    pin_dso();
#endif

    /* Guard against double-registration (e.g. plugin reloaded). */
    if (proto_get_id_by_filter_name("ws_pluginmgr") != -1) {
        proto_pluginmgr = proto_get_id_by_filter_name("ws_pluginmgr");
        return;
    }

    proto_pluginmgr = proto_register_protocol(
        "Wireshark Plugin Manager (Author: Walter Hofstetter)",
        "Plugin Manager",
        "ws_pluginmgr"
    );

    pluginmgr_menu = ext_menubar_register_menu(
        proto_pluginmgr, "Plugin Manager", TRUE);
    ext_menubar_set_parentmenu(pluginmgr_menu, "Tools");
    ext_menubar_add_entry(pluginmgr_menu,
        "Manage Plugins\342\200\246",   /* "Manage Plugins…" in UTF-8 */
        "Open the Wireshark Plugin Manager",
        open_pluginmgr_cb, NULL);

    ws_log(WS_LOG_DOMAIN, LOG_LEVEL_DEBUG, "Plugin Manager registered");
}

void proto_reg_handoff_ws_pluginmgr(void)
{
    /* Nothing to hand off — this plugin adds UI only, no dissection. */
}
