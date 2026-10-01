// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/compositor_names/compositor_names.h"
#include "qindaqt/session_supervisor/direct_parent_process.h"
#include "qindaqt/session_supervisor/session_process_supervisor.h"
#include "qindaqt/session_supervisor/session_service.h"
#include "qindaqt/session_autostart/autostart_catalog.h"
#include "qindaqt/services/settings_client/qt_settings_transport.h"
#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/session/window_management/kwin_reconfigure_requester.h"
#include "qindaqt/session/window_management/kwin_window_management_writer.h"
#include "qindaqt/session/window_management/window_management_bridge.h"
#include "qindaqt/session/window_management/window_management_apply_state_service.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTextStream>
#include <QtDBus/QDBusConnection>

#include "qindaqt/session_supervisor/polkit_agent_selection.h"

#include "../src/activation_environment.h"
#include "../src/resident_service_refresh.h"
#include "../src/native_lock_composition.h"
#include "../src/replaced_activation_owner.h"

#include <utility>

using namespace QindaQt::SessionSupervisor;

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("qindaqt-session"));
    QCoreApplication::setApplicationVersion(QStringLiteral(QINDAQT_VERSION));
    QCoreApplication::setOrganizationDomain(QStringLiteral("qindaqt.org"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Supervise the essential QindaQt desktop session processes."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOptions({
        {QStringLiteral("notification-host"),
         QStringLiteral("Notification host executable."), QStringLiteral("path"),
         QStringLiteral("qindaqt-notification-host")},
        {QStringLiteral("shell"), QStringLiteral("Shell executable."),
         QStringLiteral("path"), QStringLiteral("qindaqt-shell")},
        {QStringLiteral("network-secret-agent"),
         QStringLiteral("Optional NetworkManager secret-agent executable."),
         QStringLiteral("path"), QStringLiteral("qindaqt-network-secret-agent")},
        {QStringLiteral("welcome"),
         QStringLiteral("Optional first-launch guide executable."),
         QStringLiteral("path"), QStringLiteral("qindaqt-welcome")},
        {QStringLiteral("removable-media"),
         QStringLiteral("Removable media insertion handler executable."),
         QStringLiteral("path"), QStringLiteral("qindaqt-removable-media")},
        {QStringLiteral("no-removable-media"),
         QStringLiteral("Disable physical removable-media handling in a private session.")},
        {QStringLiteral("desktop-controls"),
         QStringLiteral("Optional media-key/screenshot/idle-display helper."),
         QStringLiteral("path"), QStringLiteral("qindaqt-desktop-controls")},
        {QStringLiteral("powerdevil"),
         QStringLiteral("PowerDevil daemon executable."), QStringLiteral("path"),
         QStringLiteral("/usr/libexec/org_kde_powerdevil")},
        {QStringLiteral("no-powerdevil"),
         QStringLiteral("Disable the PowerDevil child for a private session.")},
        {QStringLiteral("global-shortcut-daemon"),
         QStringLiteral("KGlobalAccel daemon executable."), QStringLiteral("path"),
         QStringLiteral("/usr/libexec/kglobalacceld")},
        {QStringLiteral("no-global-shortcut-daemon"),
         QStringLiteral("Disable the KGlobalAccel child for a private session.")},
        {QStringLiteral("input-method-daemon"),
         QStringLiteral("Input method daemon executable."), QStringLiteral("path"),
         QStringLiteral("/usr/bin/ibus-daemon")},
        {QStringLiteral("no-input-method-daemon"),
         QStringLiteral("Disable the input method child for a private session.")},
        {QStringLiteral("polkit-agent"),
         QStringLiteral("Optional polkit authentication agent executable; "
                        "well-known locations are used when omitted."),
         QStringLiteral("path"), QStringLiteral("")},
        {QStringLiteral("no-polkit-agent"),
         QStringLiteral("Never start a polkit authentication agent, even when "
                        "well-known host locations exist. Private and "
                        "integration runs must pass this so a staged session "
                        "never launches host binaries.")},
        {QStringLiteral("no-keyring"),
         QStringLiteral("Disable native keyring ownership in private sessions.")},
        {QStringLiteral("night-light"),
         QStringLiteral("Optional resident night-light schedule service."),
         QStringLiteral("path"), QStringLiteral("/usr/bin/qindaqt-night-light-service")},
        {QStringLiteral("no-autostart"),
         QStringLiteral("Do not launch XDG autostart entries in a private or diagnostic session.")},
        {QStringLiteral("profile"), QStringLiteral("Shell profile id."),
         QStringLiteral("id")},
        {QStringLiteral("theme"), QStringLiteral("Shell theme id."),
         QStringLiteral("id")},
    });
    parser.process(application);

    QString error;
    const auto compositorProcessId =
        establishDirectParentProcessWitness(&error);
    if (!compositorProcessId.has_value()) {
        QTextStream(stderr) << QCoreApplication::applicationName() << ": "
                            << error << '\n';
        return 2;
    }

    const auto activationScope = witnessedSessionActivationScope(*compositorProcessId);
    publishActivationEnvironment(QDBusConnection::sessionBus(),
                                 QProcessEnvironment::systemEnvironment(), {}, activationScope);
    // AGENT-CONTRACT: before Audio1 is refreshed, so Audio1 and every recorder
    // reconnect to a PipeWire that answers (resident_service_refresh.h).
    static_cast<void>(refreshUnresponsivePipeWire(
        QDBusConnection::sessionBus(), [] { return pipeWireAnswers(); }, {}, activationScope));
    // AGENT-CONTRACT: must run after publishActivationEnvironment (so the
    // restarted unit reads the just-published environment) and before any
    // desktop consumer starts. See resident_service_refresh.h.
    refreshResidentServices(QDBusConnection::sessionBus(),
                            residentServiceRefreshUnits(), {}, activationScope);
    // Same boundary: an update may have replaced a D-Bus-activated service
    // that is still running from the previous login (replaced_activation_owner.h).
    static_cast<void>(retireReplacedActivationOwners(
        QDBusConnection::sessionBus(), replacedActivationServiceNames(), activationScope));

    SessionProcessOptions options;
    options.notificationHostExecutable = parser.value(QStringLiteral("notification-host"));
    options.shellExecutable = parser.value(QStringLiteral("shell"));
    options.networkSecretAgentExecutable =
        parser.value(QStringLiteral("network-secret-agent"));
    options.welcomeExecutable = parser.value(QStringLiteral("welcome"));
    if (!parser.isSet(QStringLiteral("no-removable-media")))
        options.removableMediaExecutable = parser.value(QStringLiteral("removable-media"));
    options.powerDevilExecutable = parser.isSet(QStringLiteral("no-powerdevil"))
        ? QString{} : parser.value(QStringLiteral("powerdevil"));
    options.globalShortcutDaemonExecutable =
        parser.isSet(QStringLiteral("no-global-shortcut-daemon"))
        ? QString{} : parser.value(QStringLiteral("global-shortcut-daemon"));
    options.inputMethodDaemonExecutable =
        parser.isSet(QStringLiteral("no-input-method-daemon"))
        ? QString{} : parser.value(QStringLiteral("input-method-daemon"));
    options.desktopControlsExecutable =
        parser.value(QStringLiteral("desktop-controls"));
    options.polkitAgentExecutable = resolvePolkitAgentExecutable(
        parser.isSet(QStringLiteral("no-polkit-agent")),
        parser.value(QStringLiteral("polkit-agent")));
    const auto portal = QCoreApplication::applicationDirPath() + QStringLiteral("/xdg-desktop-portal-qindaqt");
    if (QFileInfo(portal).isExecutable()) options.portalExecutable = portal;
    const auto keyring = QString::fromUtf8(QINDAQT_KEYRING_INSTALL_PATH);
    if (!parser.isSet(QStringLiteral("no-keyring")) && QFileInfo(keyring).isExecutable())
        options.keyringExecutable = keyring;
    options.nightLightExecutable = parser.value(QStringLiteral("night-light"));
    if (!parser.isSet(QStringLiteral("no-autostart"))) {
        options.autostart = QindaQt::SessionAutostart::ScanOptions::fromEnvironment();
        options.autostart.supersedeDistributionPolkitAgents =
            !options.polkitAgentExecutable.isEmpty();
        options.autostart.supersedeDistributionKeyringAgents = !options.keyringExecutable.isEmpty();
    }
    options.profileId = parser.value(QStringLiteral("profile"));
    options.themeId = parser.value(QStringLiteral("theme"));
    options.compositorProcessId = *compositorProcessId;
    SessionProcessSupervisor supervisor(std::move(options));
    if (!supervisor.start(&error)) {
        QTextStream(stderr) << QCoreApplication::applicationName() << ": "
                            << error << '\n';
        return 2;
    }
    SessionService sessionService(supervisor, QDBusConnection::sessionBus());
    if (!sessionService.start(&error)) {
        supervisor.stop();
        QTextStream(stderr) << QCoreApplication::applicationName() << ": "
                            << error << '\n';
        return 2;
    }
    // Native Lock1, the ScreenSaver facade and automatic idle/resume policy
    // share the supervisor's selected session owner and ordinary compositor
    // attachment. The lock path degrades closed without blocking login.
    NativeLockComposition nativeLock;
    if (!nativeLock.start(&error)) {
        QTextStream(stderr) << QCoreApplication::applicationName()
                            << ": native lock runtime unavailable: " << error << '\n';
    }
    QObject::connect(&application, &QCoreApplication::aboutToQuit,
                     &application, [&nativeLock] { nativeLock.stop(); });
    // The windowManagement.* live bridge (ADR-0209): confirmed Settings1
    // values become qindaqt/kwinrc entries plus one reconfigure. It rides the
    // supervisor's lifetime and never blocks it; without Settings1 the last
    // written qindaqt/kwinrc simply stands.
    QindaQt::Services::SettingsClient::QtSettingsTransport windowManagementTransport(
        QDBusConnection::sessionBus());
    QindaQt::Services::SettingsClient::SettingsClient windowManagementSettings(
        windowManagementTransport,
        QindaQt::Session::WindowManagement::WindowManagementPreferences::scopedKeys());
    const QindaQt::Session::WindowManagement::KWinWindowManagementWriter kwinWriter(
        QDir(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation))
            .filePath(QString(QindaQt::CompositorNames::configFile)));
    QindaQt::Session::WindowManagement::DBusKWinReconfigureRequester kwinReconfigure(
        QDBusConnection::sessionBus());
    QindaQt::Session::WindowManagement::WindowManagementBridge windowManagementBridge(
        windowManagementSettings, kwinWriter, kwinReconfigure);
    QString windowManagementError;
    QindaQt::Session::WindowManagement::WindowManagementApplyStateService
        windowManagementApplyState(windowManagementBridge,
                                   QDBusConnection::sessionBus());
    if (!windowManagementApplyState.start(&windowManagementError)) {
        QTextStream(stderr) << QCoreApplication::applicationName()
                            << ": windowManagement apply status is unavailable: "
                            << windowManagementError << '\n';
    }
    if (!windowManagementSettings.start(&windowManagementError)) {
        QTextStream(stderr) << QCoreApplication::applicationName()
                            << ": windowManagement bridge has no Settings1 scope yet: "
                            << windowManagementError << '\n';
    }
    QObject::connect(&supervisor, &SessionProcessSupervisor::finished,
                     &application, [&application](int exitCode,
                                                  const QString &message) {
                         QTextStream(stderr)
                             << QCoreApplication::applicationName() << ": "
                             << message << '\n';
                         application.exit(exitCode);
                     });
    QObject::connect(&application, &QCoreApplication::aboutToQuit,
                     &supervisor, &SessionProcessSupervisor::stop);
    return application.exec();
}
