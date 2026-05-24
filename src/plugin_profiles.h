/*
 * WS-PluginManager — Wireshark Plugin Manager
 *
 * Copyright (C) 2026 Walter Hofstetter
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef PLUGIN_PROFILES_H
#define PLUGIN_PROFILES_H

#include <glib.h>
#include "plugin_scanner.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Profiles are stored as a GKeyFile (.ini) next to the personal plugin dir:
 *   <get_plugins_pers_dir()>/ws_pluginmgr_profiles.ini
 *
 * Each profile is a [Profile:<name>] group where every key is a plugin
 * display_name and the value is "enabled" or "disabled".  Only user-dir
 * plugins are recorded; system plugins are not touched by profiles.
 */

/* Return a GPtrArray of g_strdup'd profile names (sorted).
 * Caller must call plugin_profiles_free_names() when done. */
GPtrArray  *plugin_profiles_list_names(void);
void        plugin_profiles_free_names(GPtrArray *names);

/* Save the current user-plugin enable/disable state as a named profile.
 * Overwrites any existing profile with the same name. */
void        plugin_profiles_save(const char *name, GPtrArray *plugins);

/* Apply a named profile to the given plugin array.
 * For each plugin whose display_name appears in the profile, toggle it if
 * its current state differs from the saved state.
 * Returns the number of C plugins toggled (requires Wireshark restart);
 * Lua plugins are reloaded immediately inside plugin_action_toggle(). */
int         plugin_profiles_apply(const char *name, GPtrArray *plugins);

/* Delete a named profile.  No-op if the profile does not exist. */
void        plugin_profiles_delete(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* PLUGIN_PROFILES_H */
