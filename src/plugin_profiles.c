/*
 * WS-PluginManager — Wireshark Plugin Manager
 *
 * Copyright (C) 2026 Walter Hofstetter
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "plugin_profiles.h"
#include "plugin_actions.h"
#include <wsutil/filesystem.h>
#include <glib.h>
#include <string.h>

#define PROFILE_GROUP_PREFIX "Profile:"

/* Profiles file lives next to the personal plugin directory so it travels
 * with the user's plugin install. */
static char *profiles_file_path(void)
{
    const char *base = get_plugins_pers_dir();
    if (!base) return g_strdup("ws_pluginmgr_profiles.ini");
    return g_build_filename(base, "ws_pluginmgr_profiles.ini", NULL);
}

static GKeyFile *load_keyfile(void)
{
    GKeyFile *kf = g_key_file_new();
    char *path = profiles_file_path();
    g_key_file_load_from_file(kf, path, G_KEY_FILE_KEEP_COMMENTS, NULL);
    g_free(path);
    return kf;
}

static void save_keyfile(GKeyFile *kf)
{
    char *path = profiles_file_path();
    gsize len;
    char *data = g_key_file_to_data(kf, &len, NULL);
    if (data) {
        char *dir = g_path_get_dirname(path);
        g_mkdir_with_parents(dir, 0700);
        g_free(dir);
        g_file_set_contents(path, data, (gssize)len, NULL);
        g_free(data);
    }
    g_free(path);
}

/* ------------------------------------------------------------------ */

GPtrArray *plugin_profiles_list_names(void)
{
    GKeyFile *kf = load_keyfile();
    gsize n = 0;
    gchar **groups = g_key_file_get_groups(kf, &n);
    GPtrArray *names = g_ptr_array_new_with_free_func(g_free);

    for (gsize i = 0; i < n; i++) {
        if (g_str_has_prefix(groups[i], PROFILE_GROUP_PREFIX))
            g_ptr_array_add(names, g_strdup(groups[i] + strlen(PROFILE_GROUP_PREFIX)));
    }
    g_strfreev(groups);
    g_key_file_free(kf);

    /* Sort alphabetically for stable display. */
    g_ptr_array_sort(names, (GCompareFunc)g_strcmp0);
    return names;
}

void plugin_profiles_free_names(GPtrArray *names)
{
    if (names) g_ptr_array_free(names, TRUE);
}

void plugin_profiles_save(const char *name, GPtrArray *plugins)
{
    GKeyFile *kf = load_keyfile();
    char *group = g_strconcat(PROFILE_GROUP_PREFIX, name, NULL);

    g_key_file_remove_group(kf, group, NULL);

    for (guint i = 0; i < plugins->len; i++) {
        plugin_entry_t *e = (plugin_entry_t *)g_ptr_array_index(plugins, i);
        if (e->dir != PLUGIN_DIR_USER) continue;
        g_key_file_set_string(kf, group, e->display_name,
                              e->enabled ? "enabled" : "disabled");
    }

    g_free(group);
    save_keyfile(kf);
    g_key_file_free(kf);
}

int plugin_profiles_apply(const char *name, GPtrArray *plugins)
{
    GKeyFile *kf = load_keyfile();
    char *group = g_strconcat(PROFILE_GROUP_PREFIX, name, NULL);
    int c_toggled = 0;

    for (guint i = 0; i < plugins->len; i++) {
        plugin_entry_t *e = (plugin_entry_t *)g_ptr_array_index(plugins, i);
        if (e->dir != PLUGIN_DIR_USER) continue;

        GError *err = NULL;
        char *val = g_key_file_get_string(kf, group, e->display_name, &err);
        if (err) { g_error_free(err); continue; } /* not in profile — skip */

        gboolean want_enabled = (g_strcmp0(val, "enabled") == 0);
        g_free(val);

        if (e->enabled != want_enabled) {
            if (plugin_action_toggle(e) == PLUGIN_ACTION_OK &&
                e->lang == PLUGIN_LANG_C)
                c_toggled++;
        }
    }

    g_free(group);
    g_key_file_free(kf);
    return c_toggled;
}

void plugin_profiles_delete(const char *name)
{
    GKeyFile *kf = load_keyfile();
    char *group = g_strconcat(PROFILE_GROUP_PREFIX, name, NULL);
    g_key_file_remove_group(kf, group, NULL);
    g_free(group);
    save_keyfile(kf);
    g_key_file_free(kf);
}
