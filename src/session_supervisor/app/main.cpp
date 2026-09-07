// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/session_supervisor/direct_parent_process.h"
#include "qindaqt/session_supervisor/session_process_supervisor.h"
#include "qindaqt/session_supervisor/session_service.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QTextStream>
#include <QtDBus/QDBusConnection>

#include "qindaqt/session_supervisor/polkit_agent_selection.h"

#include "../src/activation_environment.h"
#include "../src/resident_service_refresh.h"

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
        {QStringLiteral("desktop-controls"),
         QStringLiteral("Optional media-key/screenshot/idle-display helper."),
         QStringLiteral("path"), QStringLiteral("qindaqt-desktop-controls")},
        {QStringLiteral("powerdevil"),
         QStringLiteral("PowerDevil daemon executable."), QStringLiteral("path"),
         QStringLiteral("/usr/libexec/org_kde_powerdevil")},
        {QStringLiteral("no-powerdevil"),
         QStringLiteral("Disable the PowerDevil child for a private session.")},
        {QStringLiteral("polkit-agent"),
         QStringLiteral("Optional polkit authentication agent executable; "
                        "well-known locations are used when omitted."),
         QStringLiteral("path"), QStringLiteral("")},
        {QStringLiteral("no-polkit-agent"),
         QStringLiteral("Never start a polkit authentication agent, even when "
                        "well-known host locations exist. Private and "
                        "integration runs must pass this so a staged session "
                        "never launches host binaries.")},
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

    publishActivationEnvironment(QDBusConnection::sessionBus(),
                                 QProcessEnvironment::systemEnvironment());
    // AGENT-CONTRACT: must run after publishActivationEnvironment (so the
    // restarted unit reads the just-published environment) and before any
    // desktop consumer starts. See resident_service_refresh.h.
    refreshResidentServices(QDBusConnection::sessionBus(),
                            residentServiceRefreshUnits());

    SessionProcessOptions options;
    options.notificationHostExecutable = parser.value(QStringLiteral("notification-host"));
    options.shellExecutable = parser.value(QStringLiteral("shell"));
    options.networkSecretAgentExecutable =
        parser.value(QStringLiteral("network-secret-agent"));
    options.welcomeExecutable = parser.value(QStringLiteral("welcome"));
    options.powerDevilExecutable = parser.isSet(QStringLiteral("no-powerdevil"))
        ? QString{} : parser.value(QStringLiteral("powerdevil"));
    options.desktopControlsExecutable =
        parser.value(QStringLiteral("desktop-controls"));
    options.polkitAgentExecutable = resolvePolkitAgentExecutable(
        parser.isSet(QStringLiteral("no-polkit-agent")),
        parser.value(QStringLiteral("polkit-agent")));
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
