/*
 * WS-PluginManager — Wireshark Plugin Manager
 *
 * Copyright (C) 2026 Walter Hofstetter
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "ui_bridge.h"
#include "plugin_dialog.h"
#include <QApplication>
#include <QMainWindow>
#include <QMenuBar>
#include <QMenu>
#include <QAction>

/* Single dialog instance — re-used across repeated menu invocations. */
static PluginDialog *s_dialog  = nullptr;
static bool          s_menu_ok = false;

/*
 * Move our Tools menu entry to the very end and insert a separator above it.
 * Called once, the first time the dialog is opened (Qt event loop is running
 * by then). ext_menubar has no positioning or cross-separator API, so we do
 * this directly through Qt.
 */
static void arrange_tools_menu(void)
{
    QMainWindow *mainWin = nullptr;
    for (QWidget *w : QApplication::topLevelWidgets()) {
        if ((mainWin = qobject_cast<QMainWindow *>(w)))
            break;
    }
    if (!mainWin) return;

    QMenu *toolsMenu = nullptr;
    for (QAction *a : mainWin->menuBar()->actions()) {
        if (a->menu() && a->text().remove('&').trimmed() == "Tools") {
            toolsMenu = a->menu();
            break;
        }
    }
    if (!toolsMenu) return;

    /* Find the action that owns our "Plugin Manager" submenu. */
    QAction *pmAction = nullptr;
    for (QAction *a : toolsMenu->actions()) {
        if (a->menu() &&
            a->menu()->title().contains("Plugin Manager", Qt::CaseInsensitive)) {
            pmAction = a;
            break;
        }
    }
    if (!pmAction) return;

    /* Already last with a separator above it — nothing to do. */
    QList<QAction *> acts = toolsMenu->actions();
    int idx = static_cast<int>(acts.indexOf(pmAction));
    if (idx == acts.size() - 1 && idx > 0 && acts[idx - 1]->isSeparator())
        return;

    /* Pull it out, append separator, re-append our entry. */
    toolsMenu->removeAction(pmAction);
    toolsMenu->addSeparator();
    toolsMenu->addAction(pmAction);
}

#ifdef __cplusplus
extern "C" {
#endif

void pluginmgr_open_dialog(void)
{
    /* Wireshark always provides a QApplication when a plugin callback fires;
     * never create a second one — that causes QAccessibleCache corruption on
     * macOS Qt 6 and a SIGSEGV at exit. */
    if (!s_dialog)
        s_dialog = new PluginDialog(nullptr);

    if (!s_menu_ok) {
        arrange_tools_menu();
        s_menu_ok = true;
    }

    s_dialog->refresh();
    s_dialog->show();
    s_dialog->raise();
    s_dialog->activateWindow();
}

#ifdef __cplusplus
}
#endif
