// SPDX-License-Identifier: GPL-3.0-or-later
#include "shellruntimeapplication.h"

#include "audioappletcomposition.h"
#include "bluetoothappletcomposition.h"
#include "globalmenuappletcomposition.h"
#include "launcherappletcomposition.h"
#include "../common/shelliconconfiguration.h"
#include "launcher_persistence.h"
#include "powerappletcomposition.h"
#include "tasklistappletcomposition.h"

#include "qindaqt/services/settings_client/qt_settings_transport.h"
#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/shell_window_actions_client/qt_shell_window_actions_transport.h"
#include "qindaqt/shell_window_actions_client/shell_window_actions_client.h"
#include "qindaqt/shell/icons/icon_runtime.h"

#include <QDBusConnection>
#include <QDebug>
#include <QStandardPaths>

#include <utility>

namespace QindaQt::Shell {

bool ShellRuntimeApplication::initializeLauncherRuntime(QString *error)
{
    QStringList launcherRoots;
    if (!m_dataRoots.dataHome.isEmpty()) {
        launcherRoots.append(m_dataRoots.dataHome);
    }
    for (const QString &directory : m_dataRoots.dataDirectories) {
        if (!launcherRoots.contains(directory)) {
            launcherRoots.append(directory);
        }
    }
    m_settingsTransport =
        std::make_unique<Services::SettingsClient::QtSettingsTransport>(
            QDBusConnection::sessionBus());
    m_settingsClient = std::make_unique<Services::SettingsClient::SettingsClient>(
        *m_settingsTransport,
        QStringList{QStringLiteral("accessibility.reducedMotion"),
                    QStringLiteral("panels.autoHideDelayMs"),
                    QStringLiteral("services.clipboardHistory"),
                    Launcher::LauncherPersistenceController::pinnedKey(),
                    Launcher::LauncherPersistenceController::recentKey()});
    // AGENT-CONTRACT: Settings1 rejects an entire scoped snapshot when any
    // requested key is unknown. Notification quieting therefore owns a
    // purpose-scoped client so optional applet keys cannot turn a present,
    // defaulted services.doNotDisturb value into "unavailable".
    m_quietingSettingsTransport =
        std::make_unique<Services::SettingsClient::QtSettingsTransport>(
            QDBusConnection::sessionBus());
    m_quietingSettingsClient =
        std::make_unique<Services::SettingsClient::SettingsClient>(
            *m_quietingSettingsTransport,
            QStringList{QStringLiteral("services.doNotDisturb")});
    m_launcherApplet = std::make_unique<LauncherAppletComposition>(
        m_applets, m_appletPolicy, std::move(launcherRoots),
        *m_settingsClient, QDBusConnection::sessionBus());
    return m_launcherApplet->start(error);
}

void ShellRuntimeApplication::initializeServiceAppletCompositions()
{
    m_audioApplet =
        std::make_unique<AudioAppletComposition>(m_applets, m_appletPolicy);
    m_bluetoothApplet =
        std::make_unique<BluetoothAppletComposition>(m_applets, m_appletPolicy);
    m_powerApplet =
        std::make_unique<PowerAppletComposition>(m_applets, m_appletPolicy);
    const QDBusConnection sessionBus = QDBusConnection::sessionBus();
    m_clipboardApplet = std::make_unique<ClipboardAppletComposition>(
        m_applets, m_appletPolicy, *m_settingsClient, sessionBus);
    m_windowActionsTransport = std::make_unique<
        ShellWindowActionsClient::QtShellWindowActionsTransport>(sessionBus);
    m_windowActionsClient = std::make_unique<
        ShellWindowActionsClient::ShellWindowActionsClient>(
            *m_windowActionsTransport);
    m_globalMenuApplet = std::make_unique<GlobalMenuAppletComposition>(
        m_applets, m_appletPolicy, sessionBus, *m_windowActionsClient);
    m_taskListApplet = std::make_unique<TaskListAppletComposition>(
        m_applets, m_appletPolicy, sessionBus, *m_windowActionsClient,
        Icons::IconRuntime::freedesktopApplicationRoots(
            m_dataRoots.dataHome, m_dataRoots.dataDirectories),
        Icons::IconRuntime::freedesktopIconRoots(
            m_dataRoots.dataHome, m_dataRoots.dataDirectories),
        QStringList{[this] {
            QString themeName;
            QString ignoredError;
            const bool themeSelected = ShellIconConfiguration::selectedThemeName(
                m_themes, &themeName, &ignoredError);
            if (!themeSelected) {
                return QString{};
            }
            return themeName;
        }()});
    QString taskListError;
    if (!m_taskListApplet->start(&taskListError)) {
        qWarning().noquote()
            << "QindaQt shell could not start Task List facts:" << taskListError;
    }

    // The tray's icon-theme lookup roots are the freedesktop icon locations
    // beneath every generic data root; the renderer canonicalizes and
    // confines every candidate beneath these injected roots.
    QStringList statusNotifierIconRoots;
    const auto dataRoots =
        QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
    statusNotifierIconRoots.reserve(dataRoots.size());
    for (const QString &base : dataRoots) {
        statusNotifierIconRoots.append(base + QStringLiteral("/icons"));
    }
    m_statusNotifierApplet = std::make_unique<StatusNotifierAppletComposition>(
        m_applets, m_appletPolicy, sessionBus, statusNotifierIconRoots);
    m_globalMenuApplet->start();
    connect(m_windowActionsClient.get(),
            &ShellWindowActionsClient::ShellWindowActionsClient::identityChanged,
            this, [this] {
                if (m_windowActionsClient->identitySnapshot()) {
                    m_windowActionsRetry.stop();
                } else {
                    m_windowActionsRetry.start();
                }
            });
}

void ShellRuntimeApplication::startSettingsClients()
{
    QString settingsError;
    if (!m_settingsClient->start(&settingsError)) {
        qWarning().noquote()
            << "QindaQt shell could not start Settings1; launcher persistence"
               " and clipboard settings remain unavailable:"
            << settingsError;
    }
    QString quietingError;
    if (!m_quietingSettingsClient->start(&quietingError)) {
        qWarning().noquote()
            << "QindaQt shell could not start the Settings1 notification"
               " quieting scope:"
            << quietingError;
    }
}

} // namespace QindaQt::Shell
