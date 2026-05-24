/*
 * WS-PluginManager — Wireshark Plugin Manager
 *
 * Copyright (C) 2026 Walter Hofstetter
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "plugin_actions.h"
#include <glib.h>
#include <glib/gstdio.h>
#include <string.h>

/* ---------------------------------------------------------------------------
 * Lua reload
 *
 * wslua_reload_plugins() is exported by epan when Wireshark is built with
 * Lua support.  We look it up at runtime via dlsym / GetProcAddress so the
 * plugin compiles and runs even against Lua-less Wireshark builds.
 * --------------------------------------------------------------------------- */

#ifdef _WIN32
#  include <windows.h>
static void reload_lua_if_available(void)
{
    typedef void (*reload_fn)(void *, void *);
    HMODULE h = GetModuleHandle(NULL);
    if (!h) return;
    reload_fn fn = (reload_fn)(void *)GetProcAddress(h, "wslua_reload_plugins");
    if (fn) fn(NULL, NULL);
}
#else
#  include <dlfcn.h>
static void reload_lua_if_available(void)
{
    typedef void (*reload_fn)(void *, void *);
    reload_fn fn = (reload_fn)dlsym(RTLD_DEFAULT, "wslua_reload_plugins");
    if (fn) fn(NULL, NULL);
}
#endif

/* ---------------------------------------------------------------------------
 * Path helpers
 * --------------------------------------------------------------------------- */

/* Returns the actual on-disk path for this entry (caller must g_free). */
static char *disk_path(const plugin_entry_t *entry)
{
    if (entry->enabled)
        return g_build_filename(entry->dirpath, entry->filename, NULL);

    /* Disabled: on-disk name has ".disabled" appended to the canonical name. */
    char *name_disabled = g_strconcat(entry->filename, ".disabled", NULL);
    char *path = g_build_filename(entry->dirpath, name_disabled, NULL);
    g_free(name_disabled);
    return path;
}

/* ---------------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------------- */

plugin_action_result_e plugin_action_toggle(plugin_entry_t *entry)
{
    char *old_path = disk_path(entry);
    char *new_name;

    if (entry->enabled)
        new_name = g_strconcat(entry->filename, ".disabled", NULL);
    else
        new_name = g_strdup(entry->filename);   /* strip .disabled */

    char *new_path = g_build_filename(entry->dirpath, new_name, NULL);
    g_free(new_name);

    int rc = g_rename(old_path, new_path);
    g_free(old_path);
    g_free(new_path);

    if (rc != 0) return PLUGIN_ACTION_ERROR;

    entry->enabled = !entry->enabled;

    if (entry->lang == PLUGIN_LANG_LUA)
        reload_lua_if_available();

    return PLUGIN_ACTION_OK;
}

plugin_action_result_e plugin_action_delete(plugin_entry_t *entry)
{
    char *path = disk_path(entry);
    int rc = g_remove(path);
    g_free(path);

    if (rc != 0) return PLUGIN_ACTION_ERROR;

    /* Reload Lua VM only when an *enabled* Lua plugin was removed; a disabled
     * one was already invisible to the interpreter. */
    if (entry->lang == PLUGIN_LANG_LUA && entry->enabled)
        reload_lua_if_available();

    return PLUGIN_ACTION_OK;
}
