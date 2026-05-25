/*
 * WS-PluginManager — Wireshark Plugin Manager
 *
 * Copyright (C) 2026 Walter Hofstetter
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef PLUGIN_DIALOG_H
#define PLUGIN_DIALOG_H

#include <QDialog>
#include <QTreeWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QWidget>
#include <QComboBox>
#include <QEvent>

extern "C" {
#include "plugin_scanner.h"
#include "plugin_actions.h"
#include "plugin_profiles.h"
}

class PluginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PluginDialog(QWidget *parent = nullptr);
    ~PluginDialog() override;

    /* Re-scan all plugin directories and repopulate the tree. */
    void refresh();

private slots:
    void onItemChanged(QTreeWidgetItem *item, int column);
    void onSelectionChanged();
    void onDeleteClicked();
    void onSearchChanged(const QString &text);
    void onOpenDirClicked();
    void onProfileSelected(int index);
    void onSaveProfileClicked();
    void onDeleteProfileClicked();
    void onContextMenuRequested(const QPoint &pos);
    void onRestartClicked();

protected:
    void changeEvent(QEvent *e) override;

private:
    void buildTree(GPtrArray *plugins);
    QTreeWidgetItem *addPluginItem(QTreeWidgetItem *parent, plugin_entry_t *entry);
    void applySearchFilter(const QString &text);
    void showRestartBanner(bool show);
    void updateBannerStyle();
    void refreshProfiles();
    plugin_entry_t *entryFromItem(QTreeWidgetItem *item) const;

    /* Plugin tree */
    QTreeWidget *m_tree;
    QLineEdit   *m_search;
    QPushButton *m_deleteBtn;
    QPushButton *m_openDirBtn;
    QWidget     *m_restartBanner;
    QWidget     *m_luaBanner;

    /* Profile bar */
    QComboBox   *m_profileCombo;
    QPushButton *m_saveProfileBtn;
    QPushButton *m_deleteProfileBtn;

    bool       m_inhibitSignals;
    GPtrArray *m_plugins;
};

#endif /* PLUGIN_DIALOG_H */
