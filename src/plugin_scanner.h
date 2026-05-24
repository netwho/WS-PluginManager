/*
 * WS-PluginManager — Wireshark Plugin Manager
 *
 * Copyright (C) 2026 Walter Hofstetter
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef PLUGIN_SCANNER_H
#define PLUGIN_SCANNER_H

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Source language of the plugin binary */
typedef enum {
    PLUGIN_LANG_C,
    PLUGIN_LANG_LUA
} plugin_lang_e;

/* Whether the plugin lives in the user personal dir or a system dir */
typedef enum {
    PLUGIN_DIR_USER,    /* get_plugins_pers_dir[_with_version]() */
    PLUGIN_DIR_SYSTEM   /* get_plugins_dir[_with_version]() — read-only in UI */
} plugin_dir_e;

typedef struct _plugin_entry {
    char         *display_name; /* basename without extension, shown in UI */
    char         *filename;     /* canonical filename on disk (no .disabled suffix) */
    char         *dirpath;      /* full path of the containing directory */
    plugin_lang_e lang;
    plugin_dir_e  dir;
    gboolean      enabled;      /* FALSE when the .disabled suffix is present */
    char         *version;      /* from plugins_get_descriptions(), or NULL */
    char         *type_flags;   /* comma-separated capability string, or NULL */
} plugin_entry_t;

/* Scan all four plugin directories and return a GPtrArray of plugin_entry_t*.
 * The array owns the entries; call plugin_scanner_free_all() when done. */
GPtrArray  *plugin_scanner_scan(void);

/* Convenience: returns get_plugins_pers_dir() for use from C++ without
 * pulling <wsutil/filesystem.h> into C++ translation units. */
const char *plugin_scanner_get_user_dir(void);

void        plugin_entry_free(plugin_entry_t *entry);
void        plugin_scanner_free_all(GPtrArray *plugins);

#ifdef __cplusplus
}
#endif

#endif /* PLUGIN_SCANNER_H */
