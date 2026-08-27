// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/services/notification_presentation/presentation_access_token.h"
#include "qindaqt/session_supervisor/direct_parent_process.h"
#include "qindaqt/session_supervisor/tokenized_process_launcher.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QProcess>
#include <QTextStream>

#include <unistd.h>

using namespace QindaQt;

namespace {

int reportReady(qint64 processId)
{
    QTextStream(stdout) << "ready " << processId << '\n' << Qt::flush;
    return 0;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    QCommandLineParser parser;
    parser.addOptions({
        {QStringLiteral("witness"),
         QStringLiteral("Arm a direct-parent death witness and wait.")},
        {QStringLiteral("spawn-witness"),
         QStringLiteral("Spawn a witnessed copy of this helper and wait.")},
        {QStringLiteral("spawn-token-child"),
         QStringLiteral("Spawn a tokenized child and wait."),
         QStringLiteral("path")},
    });
    if (!parser.parse(application.arguments())) {
        return 2;
    }

    if (parser.isSet(QStringLiteral("witness"))) {
        QString error;
        const auto parent =
            SessionSupervisor::establishDirectParentProcessWitness(&error);
        if (!parent.has_value()) {
            QTextStream(stderr) << error << '\n';
            return 3;
        }
        reportReady(static_cast<qint64>(::getpid()));
        return application.exec();
    }

    QProcess child;
    child.setProcessChannelMode(QProcess::SeparateChannels);
    if (parser.isSet(QStringLiteral("spawn-witness"))) {
        child.start(QCoreApplication::applicationFilePath(),
                    {QStringLiteral("--witness")});
        if (!child.waitForStarted(5'000) || !child.waitForReadyRead(5'000)) {
            return 4;
        }
        const QByteArray readyLine = child.readLine();
        if (!readyLine.startsWith("ready ")) {
            return 5;
        }
        QTextStream(stdout) << readyLine << Qt::flush;
        return application.exec();
    }

    if (parser.isSet(QStringLiteral("spawn-token-child"))) {
        const auto token = Services::NotificationPresentation::
            PresentationAccessToken::generate();
        QString error;
        if (!SessionSupervisor::TokenizedProcessLauncher::start(
                child, parser.value(QStringLiteral("spawn-token-child")),
                {QStringLiteral("--hold")}, token, &error)) {
            QTextStream(stderr) << error << '\n';
            return 6;
        }
        reportReady(child.processId());
        return application.exec();
    }

    return 7;
}
