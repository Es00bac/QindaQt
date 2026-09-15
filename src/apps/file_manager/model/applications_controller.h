// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

#include "qindaqt/application_catalog/application_directory_scan.h"
#include "qindaqt/application_catalog/category_tree.h"

namespace QindaQt::Apps::FileManager {

// Browses the installed-application tree (Finder-style: fixed main category
// folders, registered XDG additional categories nested inside) and launches
// entries. The scan comes from the shared ApplicationCatalog module over
// caller-injected XDG data roots; this controller never reads the
// environment itself (the composition root resolves the roots).
//
// Launch policy (ADR-0164): entries whose planned argv can run as a plain
// process start through QProcess::startDetached. Terminal-required and
// D-Bus-activatable entries report a typed message instead — the workspace
// picker route launches them through the compositor, which owns the full
// desktop-entry launch facility. All launching crosses ADR-0029's boundary
// explicitly; no local file URL is ever opened by this controller.
class ApplicationsController final : public QObject
{
    Q_OBJECT
    // False until the first completed scan; a scan with zero roots found
    // still becomes ready (an empty tree is a valid, degraded state).
    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)
    // QVariantMap rows: {id, label, entryCount}.
    Q_PROPERTY(QVariantList folders READ folders NOTIFY treeChanged)
    // QVariantMap rows: {id, name, iconName, genericName, comment,
    // launchable, message}.
    Q_PROPERTY(QVariantList entries READ entries NOTIFY treeChanged)
    // Human-readable breadcrumb of the open folder, empty at the root.
    Q_PROPERTY(QString breadcrumb READ breadcrumb NOTIFY treeChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    // Chooser mode (ADR-0165): activations become workspace picker choices
    // instead of direct launches.
    Q_PROPERTY(bool chooserMode READ chooserMode WRITE setChooserMode
               NOTIFY chooserModeChanged)

public:
    explicit ApplicationsController(QStringList dataRoots,
                                    QObject *parent = nullptr);
    ~ApplicationsController() override;

    [[nodiscard]] bool ready() const noexcept { return m_ready; }
    [[nodiscard]] QVariantList folders() const;
    [[nodiscard]] QVariantList entries() const;
    [[nodiscard]] QString breadcrumb() const;
    [[nodiscard]] const QString &lastError() const noexcept
    {
        return m_lastError;
    }

    [[nodiscard]] bool chooserMode() const noexcept { return m_chooserMode; }
    void setChooserMode(bool enabled);

    // Rescans the injected roots synchronously and rebuilds the tree. Scan
    // diagnostics (unreadable roots, ceiling hits) land in lastError as one
    // bounded summary, never as a hard failure.
    Q_INVOKABLE void refresh();
    // Opens a folder by "groupId/childToken" path relative to the root, or
    // the tree root when the id is empty. Unknown ids are a no-op.
    Q_INVOKABLE void openFolder(const QString &folderId);
    Q_INVOKABLE void openParentFolder();
    // Launches the entry when its document plans a plain process; otherwise
    // records the typed limitation in lastError.
    Q_INVOKABLE void activateEntry(const QString &entryId);
    // Chooser-mode activation: hands the entry to the compositor's picker
    // route, which launches it and swaps it into the waiting slot. The
    // chooserSucceeded signal fires on acceptance so the picker window can
    // close itself; a rejection lands in lastError and the picker stays.
    Q_INVOKABLE void chooseForWorkspace(const QString &entryId);

Q_SIGNALS:
    void readyChanged();
    void treeChanged();
    void lastErrorChanged();
    void chooserModeChanged();
    void chooserSucceeded();

private:
    struct OpenFolder final
    {
        QString groupId;
        QString childToken;
    };

    [[nodiscard]] const QindaQt::ApplicationCatalog::CategoryNode *
    openNode() const;
    void setLastError(QString message);

    QStringList m_dataRoots;
    QindaQt::ApplicationCatalog::DirectoryScan m_scan;
    QindaQt::ApplicationCatalog::CategoryNode m_tree;
    OpenFolder m_open;
    bool m_ready = false;
    bool m_chooserMode = false;
    QString m_lastError;
};

} // namespace QindaQt::Apps::FileManager
