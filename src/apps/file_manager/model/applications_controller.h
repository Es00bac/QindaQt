// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "model/file_manager_types.h"
#include "model/workspace_chooser_client.h"

#include "qindaqt/application_catalog/application_directory_scan.h"

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <functional>

namespace QindaQt::Apps::FileManager {

// The two process-level effects an application activation can have. Tests
// inject recording fakes so no session bus is called and nothing is spawned;
// production() binds the real ones.
struct ApplicationLaunchSeams final
{
    // ADR-0165/ADR-0172: asks the compositor to launch the entry into the
    // active picker or docked member; a rejection is the ordinary undocked
    // case. Production: chooseApplicationOnCompositor().
    std::function<ChooserReply(const QString &entryId)> chooseOnCompositor;
    // Starts one planned argv detached. Production: QProcess::startDetached.
    std::function<bool(const QString &program, const QStringList &arguments)>
        startDetached;

    [[nodiscard]] static ApplicationLaunchSeams production();
};

// Owns the installed-application catalog behind the Applications place
// (ADR-0262): the scan of caller-injected XDG data roots (this controller
// never reads the environment; the composition root resolves the roots), the
// flat listing the window's NavigationController browses like a folder, the
// Get Info fields, and the launch policy.
//
// Launch policy (ADR-0164/ADR-0172): an activation asks the compositor first,
// so a docked window is replaced in place; a rejection falls back to a
// detached plain-process launch, and terminal-required or D-Bus-activatable
// entries report a typed message on that fallback. Chooser mode (ADR-0165)
// hands every activation to the compositor only. No local file URL is ever
// opened here.
//
// AGENT-CONTRACT: GUI-thread only. refresh() is synchronous and bounded by
// the catalog scan's ceilings; the listing and descriptions always read the
// last completed scan, so a row that disappeared reports "no longer
// installed" instead of launching something else.
class ApplicationsController final : public QObject
{
    Q_OBJECT
    // False until the first completed scan; a scan with zero roots found
    // still becomes ready (an empty catalog is a valid, degraded state).
    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)
    // ADR-0262: the Applications place's address, for Main.qml's navigation.
    Q_PROPERTY(QString location READ location CONSTANT)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    // Chooser mode (ADR-0165): activations become workspace picker choices
    // instead of direct launches.
    Q_PROPERTY(bool chooserMode READ chooserMode WRITE setChooserMode
               NOTIFY chooserModeChanged)

public:
    explicit ApplicationsController(QStringList dataRoots,
                                    QObject *parent = nullptr);
    ApplicationsController(QStringList dataRoots, ApplicationLaunchSeams seams,
                           QObject *parent = nullptr);
    ~ApplicationsController() override;

    [[nodiscard]] bool ready() const noexcept { return m_ready; }
    [[nodiscard]] static QString location();
    [[nodiscard]] const QString &lastError() const noexcept
    {
        return m_lastError;
    }

    [[nodiscard]] bool chooserMode() const noexcept { return m_chooserMode; }
    void setChooserMode(bool enabled);

    // Rescans the injected roots synchronously. Scan diagnostics (unreadable
    // roots, ceiling hits) land in lastError as one bounded summary, never as
    // a hard failure.
    Q_INVOKABLE void refresh();
    // Every visible application of the last scan as one Applications-place
    // row, in catalog order (NavigationController applies the window's sort,
    // filter and grouping). NoDisplay/Hidden entries never appear: the scan
    // keeps menu entries only. `truncated` reports a scan ceiling.
    [[nodiscard]] ListingResult listing() const;
    // Get Info fields for one entry (see ApplicationsListing::describe), or
    // an empty map for an unknown id.
    Q_INVOKABLE QVariantMap describe(const QString &entryId) const;
    // Opens the entry the way this window's mode requires: chooseForWorkspace
    // in chooser mode, activateEntry otherwise. Returns the failure it
    // recorded in lastError, or an empty string when the request was taken.
    Q_INVOKABLE QString open(const QString &entryId);
    Q_INVOKABLE void activateEntry(const QString &entryId);
    // Chooser-mode activation: hands the entry to the compositor's picker
    // route, which launches it and swaps it into the waiting slot. The
    // chooserSucceeded signal fires on acceptance so the picker window can
    // close itself; a rejection lands in lastError and the picker stays.
    Q_INVOKABLE void chooseForWorkspace(const QString &entryId);
    Q_INVOKABLE void clearLastError();

Q_SIGNALS:
    void readyChanged();
    void catalogChanged();
    void lastErrorChanged();
    void chooserModeChanged();
    void chooserSucceeded();

private:
    void setLastError(QString message);

    QStringList m_dataRoots;
    ApplicationLaunchSeams m_seams;
    QindaQt::ApplicationCatalog::DirectoryScan m_scan;
    // Entry id -> primary category label, rebuilt with every scan.
    QHash<QString, QString> m_categories;
    bool m_truncated = false;
    bool m_ready = false;
    bool m_chooserMode = false;
    QString m_lastError;
};

} // namespace QindaQt::Apps::FileManager
