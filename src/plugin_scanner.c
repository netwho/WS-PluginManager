/*
 * WS-PluginManager — Wireshark Plugin Manager
 *
 * Copyright (C) 2026 Walter Hofstetter
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "plugin_scanner.h"
#include <wsutil/filesystem.h>
#include <wsutil/plugins.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <string.h>
#include <stdlib.h>

#define MANAGER_PREFIX "ws_pluginmgr"

#ifdef _WIN32
static const char * const NATIVE_EXTS[] = { ".dll", NULL };
#elif defined(__APPLE__)
static const char * const NATIVE_EXTS[] = { ".dylib", ".so", NULL };
#else
static const char * const NATIVE_EXTS[] = { ".so", NULL };
#endif

/* ------------------------------------------------------------------ */

static gboolean has_native_ext(const char *name)
{
    for (int i = 0; NATIVE_EXTS[i]; i++)
        if (g_str_has_suffix(name, NATIVE_EXTS[i])) return TRUE;
    return FALSE;
}

static char *strip_native_ext(const char *name)
{
    for (int i = 0; NATIVE_EXTS[i]; i++) {
        if (g_str_has_suffix(name, NATIVE_EXTS[i])) {
            gsize keep = strlen(name) - strlen(NATIVE_EXTS[i]);
            return g_strndup(name, keep);
        }
    }
    return g_strdup(name);
}

/* ------------------------------------------------------------------ */

void plugin_entry_free(plugin_entry_t *entry)
{
    if (!entry) return;
    g_free(entry->display_name);
    g_free(entry->filename);
    g_free(entry->dirpath);
    g_free(entry->version);
    g_free(entry->type_flags);
    g_free(entry);
}

void plugin_scanner_free_all(GPtrArray *plugins)
{
    if (plugins) g_ptr_array_free(plugins, TRUE);
}

const char *plugin_scanner_get_user_dir(void)
{
    return get_plugins_pers_dir();
}

/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ */
/* Lua version extraction                                               */
/* ------------------------------------------------------------------ */

#define LUA_VERSION_SCAN_LINES 80

/*
 * Scan the first LUA_VERSION_SCAN_LINES lines of a Lua file for version info.
 *
 * Recognised patterns (tried in order, first match wins):
 *   version = "1.2.3"     (set_plugin_info style, single or double quotes)
 *   -- Version: 1.2.3     (comment header style, case-insensitive "Version:")
 *
 * Returns a newly allocated string or NULL.
 */
static char *lua_extract_version(const char *filepath)
{
    FILE *f = g_fopen(filepath, "r");
    if (!f) return NULL;

    char line[512];
    char *found = NULL;
    int n = 0;

    while (n++ < LUA_VERSION_SCAN_LINES && fgets(line, sizeof(line), f) && !found) {

        /* Pattern A: version = "x.y.z" or version = 'x.y.z' */
        const char *p = strstr(line, "version");
        if (p) {
            const char *q = p + 7;
            while (*q == ' ' || *q == '\t') q++;
            if (*q == '=') {
                q++;
                while (*q == ' ' || *q == '\t') q++;
                if (*q == '"' || *q == '\'') {
                    char delim = *q++;
                    const char *e = strchr(q, delim);
                    if (e && (e - q) >= 3 && (e - q) <= 20 && g_ascii_isdigit(*q))
                        found = g_strndup(q, (gsize)(e - q));
                }
            }
        }

        /* Pattern B: -- Version: x.y.z  (comment header) */
        if (!found) {
            const char *vp = strstr(line, "Version:");
            if (!vp) vp = strstr(line, "VERSION:");
            if (vp) {
                vp += 8;
                while (*vp == ' ' || *vp == '\t') vp++;
                if (g_ascii_isdigit(*vp)) {
                    const char *e = vp;
                    while (*e && !g_ascii_isspace(*e)) e++;
                    if ((e - vp) >= 3 && (e - vp) <= 20)
                        found = g_strndup(vp, (gsize)(e - vp));
                }
            }
        }
    }

    fclose(f);
    return found;
}

/* ------------------------------------------------------------------ */

static plugin_entry_t *parse_plugin_file(const char *fname,
                                          const char *dirpath,
                                          plugin_dir_e dir_type)
{
    gboolean enabled = TRUE;
    plugin_lang_e lang;
    char *work = g_strdup(fname);

    if (g_str_has_suffix(work, ".disabled")) {
        enabled = FALSE;
        work[strlen(work) - strlen(".disabled")] = '\0';
    }

    if (g_str_has_suffix(work, ".lua")) {
        lang = PLUGIN_LANG_LUA;
    } else if (has_native_ext(work)) {
        lang = PLUGIN_LANG_C;
    } else {
        g_free(work);
        return NULL;
    }

    if (g_str_has_prefix(work, MANAGER_PREFIX)) {
        g_free(work);
        return NULL;
    }

    plugin_entry_t *e = g_new0(plugin_entry_t, 1);
    e->filename = g_strdup(work);
    e->dirpath  = g_strdup(dirpath);
    e->lang     = lang;
    e->dir      = dir_type;
    e->enabled  = enabled;

    if (lang == PLUGIN_LANG_LUA) {
        /* Extract version from the source file.
         * fname still contains the real on-disk name (including .disabled),
         * so g_build_filename(dirpath, fname) is the actual readable path. */
        char *lua_path = g_build_filename(dirpath, fname, NULL);
        e->version = lua_extract_version(lua_path);
        g_free(lua_path);

        char *dot = strrchr(work, '.');
        if (dot) *dot = '\0';
        e->display_name = g_strdup(work);
    } else {
        e->display_name = strip_native_ext(work);
    }

    g_free(work);
    return e;
}

/* ------------------------------------------------------------------ */

/*
 * Recursively scan a directory up to `depth` additional levels.
 * depth=0 : scan only the files directly in dirpath
 * depth=2 : handles <base>/<version>/<type>/<plugin>.so
 *
 * Directories are visited but NOT added to `seen` so that the same
 * version subdir can be reached from both the versioned and unversioned
 * API paths without skipping plugins.  Only file full-paths go into
 * `seen` to avoid listing the same binary twice.
 */
static void scan_dir_recursive(const char *dirpath, plugin_dir_e dir_type,
                                GPtrArray *result, GHashTable *seen, int depth)
{
    if (!dirpath || depth < 0) return;

    GDir *dir = g_dir_open(dirpath, 0, NULL);
    if (!dir) return;

    const char *fname;
    while ((fname = g_dir_read_name(dir)) != NULL) {
        char *fullpath = g_build_filename(dirpath, fname, NULL);

        if (g_file_test(fullpath, G_FILE_TEST_IS_DIR)) {
            if (depth > 0)
                scan_dir_recursive(fullpath, dir_type, result, seen, depth - 1);
            g_free(fullpath);
        } else {
            if (!g_hash_table_contains(seen, fullpath)) {
                g_hash_table_add(seen, fullpath); /* takes ownership */
                plugin_entry_t *entry = parse_plugin_file(fname, dirpath, dir_type);
                if (entry) g_ptr_array_add(result, entry);
            } else {
                g_free(fullpath);
            }
        }
    }
    g_dir_close(dir);
}

/* ------------------------------------------------------------------ */

typedef struct { GPtrArray *plugins; } desc_ctx_t;

/* WS_PLUGIN_DESC_* bitmask flags were introduced in Wireshark 4.4.
 * Earlier versions pass a pre-built human-readable type string instead. */
#ifdef WS_PLUGIN_DESC_DISSECTOR

static void desc_cb(const char *name, const char *version,
                    uint32_t flags, const char *filepath, void *ud)
{
    (void)name;
    desc_ctx_t *ctx = (desc_ctx_t *)ud;
    char *base = g_path_get_basename(filepath);

    for (guint i = 0; i < ctx->plugins->len; i++) {
        plugin_entry_t *e = g_ptr_array_index(ctx->plugins, i);
        if (e->lang != PLUGIN_LANG_C) continue;
        if (strcmp(e->filename, base) != 0) continue;

        if (!e->version && version && *version)
            e->version = g_strdup(version);

        if (!e->type_flags) {
            GString *s = g_string_new(NULL);
#define AFLAG(bit, label) \
    if (flags & (bit)) { if (s->len) g_string_append(s, ", "); g_string_append(s, label); }
            AFLAG(WS_PLUGIN_DESC_DISSECTOR,    "dissector")
            AFLAG(WS_PLUGIN_DESC_TAP_LISTENER, "tap")
            AFLAG(WS_PLUGIN_DESC_FILE_TYPE,    "filetype")
            AFLAG(WS_PLUGIN_DESC_CODEC,        "codec")
            AFLAG(WS_PLUGIN_DESC_DFILTER,      "dfilter")
            AFLAG(WS_PLUGIN_DESC_EPAN,         "epan")
#undef AFLAG
            e->type_flags = g_string_free(s, FALSE);
        }
    }
    g_free(base);
}

#else /* WS < 4.4: callback delivers a pre-built type string, no bitmask */

static void desc_cb(const char *name, const char *version,
                    const char *types, const char *filepath, void *ud)
{
    (void)name;
    desc_ctx_t *ctx = (desc_ctx_t *)ud;
    char *base = g_path_get_basename(filepath);

    for (guint i = 0; i < ctx->plugins->len; i++) {
        plugin_entry_t *e = g_ptr_array_index(ctx->plugins, i);
        if (e->lang != PLUGIN_LANG_C) continue;
        if (strcmp(e->filename, base) != 0) continue;

        if (!e->version && version && *version)
            e->version = g_strdup(version);

        if (!e->type_flags && types && *types)
            e->type_flags = g_strdup(types);
    }
    g_free(base);
}

#endif /* WS_PLUGIN_DESC_DISSECTOR */

/* ------------------------------------------------------------------ */

GPtrArray *plugin_scanner_scan(void)
{
    GPtrArray *result = g_ptr_array_new_with_free_func(
                            (GDestroyNotify)plugin_entry_free);
    GHashTable *seen = g_hash_table_new_full(g_str_hash, g_str_equal,
                                             g_free, NULL);

    /* Scan personal plugin dirs recursively (depth=2) to pick up:
     *   <base>/foo.lua           - Lua at root
     *   <base>/dir/foo.lua       - Lua in named subdir
     *   <base>/<version>/foo.so  - C at version level
     *   <base>/<version>/epan/foo.so  - normal install (e.g. 4-6/epan/)
     *
     * Scanning the base (not the versioned variant) catches both
     * "4-6" and "4.6" subdir formats in one pass.
     */
    scan_dir_recursive(get_plugins_pers_dir(), PLUGIN_DIR_USER, result, seen, 2);

#ifdef __APPLE__
    /*
     * macOS Wireshark installers also drop plugins into the Application
     * Support directory which is separate from the XDG-compatible path
     * returned by get_plugins_pers_dir().  Check it explicitly.
     */
    {
        char *mac_path = g_build_filename(g_get_home_dir(),
                                           "Library", "Application Support",
                                           "Wireshark", "plugins", NULL);
        scan_dir_recursive(mac_path, PLUGIN_DIR_USER, result, seen, 2);
        g_free(mac_path);
    }
#endif

    /* System (read-only) plugin dir. */
    scan_dir_recursive(get_plugins_dir(), PLUGIN_DIR_SYSTEM, result, seen, 2);

    g_hash_table_destroy(seen);

    desc_ctx_t ctx = { result };
    plugins_get_descriptions(desc_cb, &ctx);

    return result;
}
