/*
 * WS-PluginManager — Wireshark Plugin Manager
 *
 * Copyright (C) 2026 Walter Hofstetter
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "plugin_dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QInputDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QDir>
#include <QFont>
#include <QBrush>
#include <QColor>
#include <QFrame>
#include <QMenu>
#include <QAction>
#include <QProcess>
#include <QCoreApplication>

enum Column {
    COL_NAME    = 0,
    COL_LANG    = 1,
    COL_TYPE    = 2,
    COL_VERSION = 3,
    COL_DIR     = 4,
    COL_COUNT   = 5
};

static const int ENTRY_ROLE = Qt::UserRole;

static void setEntryData(QTreeWidgetItem *item, plugin_entry_t *e)
{
    item->setData(0, ENTRY_ROLE, QVariant((qulonglong)(uintptr_t)(void *)e));
}

/* ------------------------------------------------------------------ */

PluginDialog::PluginDialog(QWidget *parent)
    : QDialog(parent), m_inhibitSignals(false), m_plugins(nullptr)
{
    setWindowTitle("Wireshark Plugin Manager");
    setMinimumSize(740, 460);
    resize(920, 560);

    /* ---- Top bar: search + Open Dir + Delete ---- */
    m_search = new QLineEdit(this);
    m_search->setPlaceholderText("Search plugins…");
    m_search->setClearButtonEnabled(true);

    m_openDirBtn = new QPushButton("Open User Dir", this);
    m_openDirBtn->setToolTip("Open the personal plugin folder in the file manager");

    m_deleteBtn = new QPushButton("Delete…", this);
    m_deleteBtn->setToolTip("Permanently delete the selected plugin file");
    m_deleteBtn->setEnabled(false);

    QHBoxLayout *topBar = new QHBoxLayout;
    topBar->addWidget(m_search, 1);
    topBar->addWidget(m_openDirBtn);
    topBar->addWidget(m_deleteBtn);

    /* ---- Profiles bar ---- */
    QLabel *profileLabel = new QLabel("Profile:", this);

    m_profileCombo = new QComboBox(this);
    m_profileCombo->setMinimumWidth(180);
    m_profileCombo->setToolTip(
        "Select a saved profile to apply it.\n"
        "A profile records which plugins are enabled or disabled.");

    m_saveProfileBtn = new QPushButton("Save as…", this);
    m_saveProfileBtn->setToolTip("Save the current enable/disable state as a named profile");

    m_deleteProfileBtn = new QPushButton("Delete Profile", this);
    m_deleteProfileBtn->setToolTip("Delete the selected profile");
    m_deleteProfileBtn->setEnabled(false);

    QFrame *separator = new QFrame(this);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);

    QHBoxLayout *profileBar = new QHBoxLayout;
    profileBar->addWidget(profileLabel);
    profileBar->addWidget(m_profileCombo, 1);
    profileBar->addWidget(m_saveProfileBtn);
    profileBar->addWidget(m_deleteProfileBtn);

    /* ---- Restart banner (C plugins — hidden until toggled) ---- */
    m_restartBanner = new QWidget(this);
    m_restartBanner->setObjectName("restartBanner");
    {
        QHBoxLayout *bl = new QHBoxLayout(m_restartBanner);
        bl->setContentsMargins(8, 4, 8, 4);
        QLabel *txt = new QLabel(
            "⚠️  One or more C plugins changed — restart Wireshark to apply.",
            m_restartBanner);
        bl->addWidget(txt, 1);
        QPushButton *restartBtn = new QPushButton("Restart Now", m_restartBanner);
        QPushButton *laterBtn   = new QPushButton("Later",       m_restartBanner);
        bl->addWidget(restartBtn);
        bl->addWidget(laterBtn);
        connect(restartBtn, &QPushButton::clicked,
                this, &PluginDialog::onRestartClicked);
        connect(laterBtn, &QPushButton::clicked,
                [this]{ m_restartBanner->setVisible(false); });
    }
    m_restartBanner->hide();

    /* ---- Lua reload banner (hidden until a Lua plugin is toggled) ---- */
    m_luaBanner = new QWidget(this);
    m_luaBanner->setObjectName("luaBanner");
    {
        QHBoxLayout *bl = new QHBoxLayout(m_luaBanner);
        bl->setContentsMargins(8, 4, 8, 4);
        QLabel *txt = new QLabel(
            "✓  Lua plugin reloaded — changes are active immediately.",
            m_luaBanner);
        bl->addWidget(txt, 1);
        QPushButton *okBtn = new QPushButton("OK", m_luaBanner);
        bl->addWidget(okBtn);
        connect(okBtn, &QPushButton::clicked,
                [this]{ m_luaBanner->setVisible(false); });
    }
    m_luaBanner->hide();

    updateBannerStyle();

    /* ---- Plugin tree ---- */
    m_tree = new QTreeWidget(this);
    m_tree->setColumnCount(COL_COUNT);
    m_tree->setHeaderLabels({"Plugin Name", "Lang", "Type", "Version", "Directory"});
    m_tree->header()->setStretchLastSection(true);
    m_tree->header()->setSectionResizeMode(COL_NAME,    QHeaderView::Interactive);
    m_tree->header()->setSectionResizeMode(COL_LANG,    QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(COL_TYPE,    QHeaderView::Interactive);
    m_tree->header()->setSectionResizeMode(COL_VERSION, QHeaderView::ResizeToContents);
    m_tree->setRootIsDecorated(true);
    m_tree->setAlternatingRowColors(true);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tree->setColumnWidth(COL_NAME, 220);
    m_tree->setColumnWidth(COL_TYPE, 130);

    /* ---- Main layout ---- */
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(topBar);
    mainLayout->addWidget(separator);
    mainLayout->addLayout(profileBar);
    mainLayout->addWidget(m_restartBanner);
    mainLayout->addWidget(m_luaBanner);
    mainLayout->addWidget(m_tree, 1);

    /* ---- Connections ---- */
    connect(m_tree,             &QTreeWidget::itemChanged,
            this,               &PluginDialog::onItemChanged);
    connect(m_tree,             &QTreeWidget::itemSelectionChanged,
            this,               &PluginDialog::onSelectionChanged);
    connect(m_deleteBtn,        &QPushButton::clicked,
            this,               &PluginDialog::onDeleteClicked);
    connect(m_openDirBtn,       &QPushButton::clicked,
            this,               &PluginDialog::onOpenDirClicked);
    connect(m_search,           &QLineEdit::textChanged,
            this,               &PluginDialog::onSearchChanged);
    connect(m_tree,             &QTreeWidget::customContextMenuRequested,
            this,               &PluginDialog::onContextMenuRequested);
    connect(m_profileCombo,     QOverload<int>::of(&QComboBox::activated),
            this,               &PluginDialog::onProfileSelected);
    connect(m_saveProfileBtn,   &QPushButton::clicked,
            this,               &PluginDialog::onSaveProfileClicked);
    connect(m_deleteProfileBtn, &QPushButton::clicked,
            this,               &PluginDialog::onDeleteProfileClicked);
}

PluginDialog::~PluginDialog()
{
    if (m_plugins) {
        plugin_scanner_free_all(m_plugins);
        m_plugins = nullptr;
    }
}

/* ------------------------------------------------------------------ */

void PluginDialog::refresh()
{
    if (m_plugins) {
        plugin_scanner_free_all(m_plugins);
        m_plugins = nullptr;
    }
    m_plugins = plugin_scanner_scan();

    m_inhibitSignals = true;
    m_tree->clear();
    buildTree(m_plugins);
    m_tree->expandAll();
    m_inhibitSignals = false;

    applySearchFilter(m_search->text());
    refreshProfiles();
}

/* ------------------------------------------------------------------ */

void PluginDialog::buildTree(GPtrArray *plugins)
{
    QTreeWidgetItem *userGroup   = new QTreeWidgetItem(m_tree);
    QTreeWidgetItem *systemGroup = new QTreeWidgetItem(m_tree);

    userGroup->setText(0,   "User Plugins");
    systemGroup->setText(0, "System Plugins");
    systemGroup->setToolTip(0,
        "Plugins installed system-wide. Shown for reference — "
        "use your package manager or admin rights to modify them.");

    QFont bold = userGroup->font(0);
    bold.setBold(true);
    userGroup->setFont(0, bold);
    systemGroup->setFont(0, bold);

    userGroup->setFlags(Qt::ItemIsEnabled);
    systemGroup->setFlags(Qt::ItemIsEnabled);

    bool hasUser = false, hasSystem = false;

    for (guint i = 0; i < plugins->len; i++) {
        plugin_entry_t *e = (plugin_entry_t *)g_ptr_array_index(plugins, i);
        if (e->dir == PLUGIN_DIR_USER) {
            addPluginItem(userGroup, e);
            hasUser = true;
        } else {
            addPluginItem(systemGroup, e);
            hasSystem = true;
        }
    }

    if (!hasUser) {
        QTreeWidgetItem *empty = new QTreeWidgetItem(userGroup, {"(none)"});
        empty->setFlags(Qt::ItemIsEnabled);
    }
    if (!hasSystem) {
        QTreeWidgetItem *empty = new QTreeWidgetItem(systemGroup, {"(none)"});
        empty->setFlags(Qt::ItemIsEnabled);
    }
}

QTreeWidgetItem *PluginDialog::addPluginItem(QTreeWidgetItem *parent,
                                              plugin_entry_t *entry)
{
    const char *lang_str = (entry->lang == PLUGIN_LANG_LUA) ? "Lua" : "C";
    QString ver  = entry->version    ? QString::fromUtf8(entry->version)    : "—";
    QString type = entry->type_flags ? QString::fromUtf8(entry->type_flags) : "—";

    QString dirShort = QString::fromUtf8(entry->dirpath);
    if (dirShort.startsWith(QDir::homePath()))
        dirShort.replace(0, QDir::homePath().length(), "~");

    QTreeWidgetItem *item = new QTreeWidgetItem(parent);
    item->setText(COL_NAME,    QString::fromUtf8(entry->display_name));
    item->setText(COL_LANG,    QString::fromUtf8(lang_str));
    item->setText(COL_TYPE,    type);
    item->setText(COL_VERSION, ver);
    item->setText(COL_DIR,     dirShort);
    item->setToolTip(COL_DIR,  QString::fromUtf8(entry->dirpath));

    setEntryData(item, entry);

    if (entry->dir == PLUGIN_DIR_USER) {
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable);
        item->setCheckState(COL_NAME, entry->enabled ? Qt::Checked : Qt::Unchecked);
        if (!entry->enabled) {
            for (int c = 0; c < COL_COUNT; c++)
                item->setForeground(c, QBrush(QColor(160, 160, 160)));
        }
    } else {
        item->setFlags((item->flags() | Qt::ItemIsSelectable) & ~Qt::ItemIsUserCheckable);
        item->setText(COL_NAME,
            QString::fromUtf8(entry->display_name) + "  \U0001F512");
        item->setToolTip(COL_NAME, "System plugin — read-only");
        for (int c = 0; c < COL_COUNT; c++)
            item->setForeground(c, QBrush(QColor(110, 110, 110)));
    }

    /* Lang badge colour */
    QColor langColor = (entry->lang == PLUGIN_LANG_LUA)
                       ? QColor(0, 130, 0) : QColor(0, 80, 200);
    item->setForeground(COL_LANG, QBrush(langColor));

    return item;
}

/* ------------------------------------------------------------------ */

void PluginDialog::onItemChanged(QTreeWidgetItem *item, int column)
{
    if (m_inhibitSignals || column != COL_NAME) return;

    plugin_entry_t *entry = entryFromItem(item);
    if (!entry || entry->dir == PLUGIN_DIR_SYSTEM) return;

    plugin_action_result_e rc = plugin_action_toggle(entry);
    if (rc != PLUGIN_ACTION_OK) {
        m_inhibitSignals = true;
        item->setCheckState(COL_NAME, entry->enabled ? Qt::Checked : Qt::Unchecked);
        m_inhibitSignals = false;
        QMessageBox::warning(this, "Plugin Manager",
            QString("Could not toggle '%1'.\n\nCheck file permissions.")
                .arg(QString::fromUtf8(entry->display_name)));
        return;
    }

    m_inhibitSignals = true;
    item->setCheckState(COL_NAME, entry->enabled ? Qt::Checked : Qt::Unchecked);
    for (int c = 0; c < COL_COUNT; c++) {
        if (entry->enabled)
            item->setData(c, Qt::ForegroundRole, QVariant());
        else
            item->setForeground(c, QBrush(QColor(160, 160, 160)));
    }
    QColor langColor = (entry->lang == PLUGIN_LANG_LUA)
                       ? QColor(0, 130, 0) : QColor(0, 80, 200);
    item->setForeground(COL_LANG, QBrush(langColor));
    m_inhibitSignals = false;

    if (entry->lang == PLUGIN_LANG_C)
        showRestartBanner(true);
    else if (entry->lang == PLUGIN_LANG_LUA)
        m_luaBanner->setVisible(true);
}

void PluginDialog::onSelectionChanged()
{
    QTreeWidgetItem *sel = m_tree->currentItem();
    plugin_entry_t  *e   = entryFromItem(sel);
    m_deleteBtn->setEnabled(e != nullptr && e->dir == PLUGIN_DIR_USER);
}

void PluginDialog::onDeleteClicked()
{
    QTreeWidgetItem *sel = m_tree->currentItem();
    if (!sel) return;
    plugin_entry_t *entry = entryFromItem(sel);
    if (!entry || entry->dir == PLUGIN_DIR_SYSTEM) return;

    QString msg = QString(
        "Permanently delete plugin '%1'?\n\n"
        "File:  %2/%3\n\n"
        "This cannot be undone."
    ).arg(QString::fromUtf8(entry->display_name))
     .arg(QString::fromUtf8(entry->dirpath))
     .arg(QString::fromUtf8(entry->filename));

    if (entry->lang == PLUGIN_LANG_C)
        msg += "\n\nWireshark must be restarted to complete the removal.";

    auto ans = QMessageBox::warning(
        this, "Delete Plugin", msg,
        QMessageBox::Yes | QMessageBox::Cancel,
        QMessageBox::Cancel);
    if (ans != QMessageBox::Yes) return;

    bool was_c = (entry->lang == PLUGIN_LANG_C);
    plugin_action_result_e rc = plugin_action_delete(entry);
    if (rc != PLUGIN_ACTION_OK) {
        QMessageBox::critical(this, "Plugin Manager",
            QString("Could not delete '%1'.\n\nCheck file permissions.")
                .arg(QString::fromUtf8(entry->display_name)));
        return;
    }

    if (was_c) showRestartBanner(true);
    refresh();
}

void PluginDialog::onSearchChanged(const QString &text)
{
    applySearchFilter(text);
}

void PluginDialog::onOpenDirClicked()
{
    const char *dir = plugin_scanner_get_user_dir();
    if (dir)
        QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromUtf8(dir)));
}

void PluginDialog::onContextMenuRequested(const QPoint &pos)
{
    QTreeWidgetItem *item = m_tree->itemAt(pos);
    plugin_entry_t  *entry = entryFromItem(item);
    if (!entry) return; /* right-clicked on a group header or empty area */

#if defined(__APPLE__)
    const QString label = "Open in Finder";
#elif defined(_WIN32)
    const QString label = "Open in Explorer";
#else
    const QString label = "Open Containing Folder";
#endif

    QMenu menu(this);
    QAction *openAction = menu.addAction(label);

    if (menu.exec(m_tree->viewport()->mapToGlobal(pos)) == openAction) {
        QDesktopServices::openUrl(
            QUrl::fromLocalFile(QString::fromUtf8(entry->dirpath)));
    }
}

/* ------------------------------------------------------------------ */
/* Profile slots                                                        */
/* ------------------------------------------------------------------ */

void PluginDialog::onProfileSelected(int index)
{
    if (index <= 0) return; /* placeholder item */

    QString name = m_profileCombo->itemText(index);

    /* Apply the profile against the current scan before refresh(). */
    int c_toggled = plugin_profiles_apply(name.toUtf8().constData(), m_plugins);

    refresh(); /* rescan — Lua already reloaded inside apply */

    if (c_toggled > 0) showRestartBanner(true);

    /* Reset combo back to placeholder so the same profile can be re-applied. */
    m_profileCombo->blockSignals(true);
    m_profileCombo->setCurrentIndex(0);
    m_profileCombo->blockSignals(false);
    m_deleteProfileBtn->setEnabled(false);
}

void PluginDialog::onSaveProfileClicked()
{
    bool ok = false;
    QString name = QInputDialog::getText(
        this, "Save Profile",
        "Profile name:",
        QLineEdit::Normal, QString(), &ok);

    if (!ok || name.trimmed().isEmpty()) return;
    name = name.trimmed();

    /* If it already exists, confirm overwrite. */
    GPtrArray *existing = plugin_profiles_list_names();
    bool already_exists = false;
    for (guint i = 0; i < existing->len; i++) {
        if (name == QString::fromUtf8((const char *)g_ptr_array_index(existing, i))) {
            already_exists = true;
            break;
        }
    }
    plugin_profiles_free_names(existing);

    if (already_exists) {
        auto ans = QMessageBox::question(
            this, "Overwrite Profile",
            QString("Profile '%1' already exists.\nOverwrite it?").arg(name),
            QMessageBox::Yes | QMessageBox::Cancel,
            QMessageBox::Cancel);
        if (ans != QMessageBox::Yes) return;
    }

    plugin_profiles_save(name.toUtf8().constData(), m_plugins);
    refreshProfiles();

    /* Select the newly saved profile in the combo. */
    int idx = m_profileCombo->findText(name);
    if (idx >= 0) {
        m_profileCombo->setCurrentIndex(idx);
        m_deleteProfileBtn->setEnabled(true);
    }
}

void PluginDialog::onDeleteProfileClicked()
{
    int idx = m_profileCombo->currentIndex();
    if (idx <= 0) return;

    QString name = m_profileCombo->itemText(idx);
    auto ans = QMessageBox::question(
        this, "Delete Profile",
        QString("Delete profile '%1'?").arg(name),
        QMessageBox::Yes | QMessageBox::Cancel,
        QMessageBox::Cancel);
    if (ans != QMessageBox::Yes) return;

    plugin_profiles_delete(name.toUtf8().constData());
    refreshProfiles();
}

/* ------------------------------------------------------------------ */

void PluginDialog::refreshProfiles()
{
    m_profileCombo->blockSignals(true);
    m_profileCombo->clear();
    m_profileCombo->addItem("— Select Profile —"); /* placeholder at index 0 */

    GPtrArray *names = plugin_profiles_list_names();
    for (guint i = 0; i < names->len; i++)
        m_profileCombo->addItem(
            QString::fromUtf8((const char *)g_ptr_array_index(names, i)));
    plugin_profiles_free_names(names);

    m_profileCombo->setCurrentIndex(0);
    m_profileCombo->blockSignals(false);
    m_deleteProfileBtn->setEnabled(false);

    /* Enable delete when a real profile is selected (not the placeholder). */
    connect(m_profileCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
                m_deleteProfileBtn->setEnabled(idx > 0);
            });
}

void PluginDialog::applySearchFilter(const QString &text)
{
    QString lower = text.toLower();
    for (int g = 0; g < m_tree->topLevelItemCount(); g++) {
        QTreeWidgetItem *group = m_tree->topLevelItem(g);
        for (int i = 0; i < group->childCount(); i++) {
            QTreeWidgetItem *child = group->child(i);
            bool match = text.isEmpty()
                || child->text(COL_NAME).toLower().contains(lower)
                || child->text(COL_LANG).toLower().contains(lower)
                || child->text(COL_TYPE).toLower().contains(lower);
            child->setHidden(!match);
        }
    }
}

void PluginDialog::showRestartBanner(bool show)
{
    m_restartBanner->setVisible(show);
}

/* Apply palette-aware colours to both banners.
 * Called at construction and whenever Qt fires a PaletteChange event
 * (e.g. the user switches between Light and Dark mode at the OS level). */
void PluginDialog::updateBannerStyle()
{
    const bool dark = palette().window().color().lightness() < 128;

    /* C restart banner — amber/warning */
    m_restartBanner->setStyleSheet(dark
        ? "QWidget#restartBanner { background-color: #3d2800;"
          " border: 1px solid #cc8800; border-radius: 4px; }"
          "QWidget#restartBanner QLabel { color: #ffd966;"
          " background: transparent; border: none; }"
        : "QWidget#restartBanner { background-color: #fff3cd;"
          " border: 1px solid #856404; border-radius: 4px; }"
          "QWidget#restartBanner QLabel { color: #5c4800;"
          " background: transparent; border: none; }");

    /* Lua reload banner — green/info */
    m_luaBanner->setStyleSheet(dark
        ? "QWidget#luaBanner { background-color: #0b2e18;"
          " border: 1px solid #2da05e; border-radius: 4px; }"
          "QWidget#luaBanner QLabel { color: #7dde96;"
          " background: transparent; border: none; }"
        : "QWidget#luaBanner { background-color: #d4edda;"
          " border: 1px solid #28a745; border-radius: 4px; }"
          "QWidget#luaBanner QLabel { color: #155724;"
          " background: transparent; border: none; }");
}

void PluginDialog::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::PaletteChange)
        updateBannerStyle();
    QDialog::changeEvent(e);
}

/* Spawn a fresh Wireshark instance then exit the current one.
 * Used by the "Restart Now" button on the C-plugin banner. */
void PluginDialog::onRestartClicked()
{
    close();
    QProcess::startDetached(QCoreApplication::applicationFilePath(), {});
    QCoreApplication::instance()->quit();
}

plugin_entry_t *PluginDialog::entryFromItem(QTreeWidgetItem *item) const
{
    if (!item) return nullptr;
    QVariant v = item->data(0, ENTRY_ROLE);
    if (!v.isValid()) return nullptr;
    return reinterpret_cast<plugin_entry_t *>(
        static_cast<uintptr_t>(v.toULongLong()));
}
