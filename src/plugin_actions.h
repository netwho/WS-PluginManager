/*
 * WS-PluginManager — Wireshark Plugin Manager
 *
 * Copyright (C) 2026 Walter Hofstetter
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef PLUGIN_ACTIONS_H
#define PLUGIN_ACTIONS_H

#include "plugin_scanner.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PLUGIN_ACTION_OK,
    PLUGIN_ACTION_ERROR
} plugin_action_result_e;

/*
 * Toggle enabled/disabled state by renaming the file:
 *   enabled  -> foo.dylib.disabled  (requires restart for C; reloads Lua immediately)
 *   disabled -> foo.dylib           (requires restart for C; reloads Lua immediately)
 * Updates entry->enabled on success.
 */
plugin_action_result_e plugin_action_toggle(plugin_entry_t *entry);

/*
 * Delete the plugin file from disk.
 * For C plugins the removal takes effect after restart.
 * For enabled Lua plugins the Lua VM is reloaded immediately.
 * The entry struct remains valid (still owned by the GPtrArray) but its
 * backing file is gone — the caller should refresh the scan afterwards.
 */
plugin_action_result_e plugin_action_delete(plugin_entry_t *entry);

#ifdef __cplusplus
}
#endif

#endif /* PLUGIN_ACTIONS_H */
