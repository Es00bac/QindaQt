// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/session_supervisor/direct_parent_process.h"
#include "qindaqt/session_supervisor/session_process_supervisor.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QTextStream>

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

    SessionProcessOptions options;
    options.notificationHostExecutable = parser.value(QStringLiteral("notification-host"));
    options.shellExecutable = parser.value(QStringLiteral("shell"));
    options.profileId = parser.value(QStringLiteral("profile"));
    options.themeId = parser.value(QStringLiteral("theme"));
    options.compositorProcessId = *compositorProcessId;
    SessionProcessSupervisor supervisor(std::move(options));
    if (!supervisor.start(&error)) {
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
