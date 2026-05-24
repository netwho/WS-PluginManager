/*
 * WS-PluginManager — Wireshark Plugin Manager
 *
 * Copyright (C) 2026 Walter Hofstetter
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef PLUGIN_MANAGER_H
#define PLUGIN_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

void proto_register_ws_pluginmgr(void);
void proto_reg_handoff_ws_pluginmgr(void);

#ifdef __cplusplus
}
#endif

#endif /* PLUGIN_MANAGER_H */
