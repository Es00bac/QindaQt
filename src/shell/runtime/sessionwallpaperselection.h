// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "wallpaperselectionsource.h"

#include "qindaqt/services/display_client/client.h"
#include "qindaqt/services/display_client/qt_display_transport.h"
#include "qindaqt/services/settings_client/qt_settings_transport.h"
#include "qindaqt/services/settings_client/settings_client.h"

#include <QDBusConnection>
#include <QHash>
#include <QPointer>

namespace QindaQt::Shell::Workspaces {
class WorkspaceController;
}

namespace QindaQt::Shell {

// Production WallpaperSelectionSource on the session bus (ADR-0286). It owns
// a purpose-scoped Settings1 client for `appearance.wallpaperAssignments`
// alone, so a Settings1 peer that predates the key costs only per-display
// wallpapers and never the shared shell preference scope. It owns a read-only
// public Display1 client for the connector -> stable-id join and borrows the
// desktop controls' workspace truth for the current desktop (may be null when
// no workspace applet is granted windows.read; desktops are then unknown).
//
// Nothing here writes: no Settings1 commit, Display1 transaction, or desktop
// switch. The last confirmed answers stay in force while a service is gone,
// like every other presentation projection in the shell.
//
// AGENT-GUARD: member order is the destruction contract. Each client is
// declared after, and so destroyed before, the transport it borrows.
class SessionWallpaperSelection final : public WallpaperSelectionSource {
    Q_OBJECT
public:
    SessionWallpaperSelection(const QDBusConnection &bus,
                              Workspaces::WorkspaceController *workspaces,
                              QObject *parent = nullptr);
    ~SessionWallpaperSelection() override;

    // Starts both clients. Failure to reach a service is not an error: the
    // wallpaper falls back until the service answers.
    void start();

    [[nodiscard]] const Services::WallpaperAssignments::WallpaperAssignments &
    assignments() const override;
    [[nodiscard]] QString displayIdForConnector(const QString &connectorName) const override;
    [[nodiscard]] QString currentDesktopId() const override;

private:
    void adoptAssignments();
    void adoptDisplays(const Display::Snapshot &snapshot);
    void adoptDesktop();

    Services::SettingsClient::QtSettingsTransport m_settingsTransport;
    Services::SettingsClient::SettingsClient m_settings;
    DisplayClient::QtDisplayTransport m_displayTransport;
    DisplayClient::Client m_display;
    QPointer<Workspaces::WorkspaceController> m_workspaces;
    Services::WallpaperAssignments::WallpaperAssignments m_assignments;
    QHash<QString, QString> m_displayIds;
    QString m_desktop;
    bool m_reportedUnreadable = false;
};

} // namespace QindaQt::Shell
