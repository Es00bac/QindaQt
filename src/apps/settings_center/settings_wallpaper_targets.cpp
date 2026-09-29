// SPDX-License-Identifier: GPL-3.0-or-later
#include "settings_wallpaper_targets.h"

#include "qindaqt/apps/settings_appearance/appearance_settings_model.h"
#include "qindaqt/services/display_client/client.h"

#ifdef QINDAQT_SETTINGS_WORKSPACE_TRUTH
#include "qindaqt/shell/workspaces/qt_workspace_transport.h"
#include "qindaqt/shell/workspaces/workspace_controller.h"

#include <QDBusConnection>
#include <QDebug>
#endif

namespace QindaQt::Apps::SettingsCenter {

struct SettingsWallpaperTargets::Desktops {
#ifdef QINDAQT_SETTINGS_WORKSPACE_TRUTH
    // AGENT-NOTE: read grant only. Settings names desktops for wallpaper
    // choices and never switches, shows, or edits them; the compositor owner
    // is authenticated through QindaQt's compositor peer service, as for any
    // process without a supervisor-provided compositor PID.
    Desktops()
        : transport(QDBusConnection::sessionBus(), Shell::Workspaces::WorkspaceAuthority{}),
          controller(&transport,
                     Shell::Workspaces::WorkspaceGrants{.windowsRead = true,
                                                        .windowsManage = false})
    {
    }

    Shell::Workspaces::QtWorkspaceTransport transport;
    Shell::Workspaces::WorkspaceController controller;
#endif
};

SettingsWallpaperTargets::SettingsWallpaperTargets(
    DisplayClient::Client &displays, SettingsAppearance::AppearanceSettingsModel &appearance)
    : m_appearance(appearance)
{
    m_catalog.attachDisplayClient(&displays);
#ifdef QINDAQT_SETTINGS_WORKSPACE_TRUTH
    m_desktops = std::make_unique<Desktops>();
    QObject::connect(&m_desktops->controller,
                     &Shell::Workspaces::WorkspaceController::stateChanged, &m_catalog,
                     [this] { m_catalog.setDesktopRows(m_desktops->controller.rows()); });
    QString error;
    if (!m_desktops->transport.start(&error)) {
        qWarning().noquote() << "qindaqt-settings: virtual desktops unavailable for"
                                " wallpaper choices:"
                             << error;
    }
#endif
    m_appearance.setWallpaperTargets(&m_catalog);
}

SettingsWallpaperTargets::~SettingsWallpaperTargets()
{
    m_appearance.setWallpaperTargets(nullptr);
}

} // namespace QindaQt::Apps::SettingsCenter
