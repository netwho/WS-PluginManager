/*
 * WS-PluginManager — Wireshark Plugin Manager
 *
 * Copyright (C) 2026 Walter Hofstetter
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef UI_BRIDGE_H
#define UI_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Called from the Wireshark menu callback (C code) to open the Qt dialog. */
void pluginmgr_open_dialog(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_BRIDGE_H */
