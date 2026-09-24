// SPDX-License-Identifier: GPL-3.0-or-later
#include "src/session_supervisor/src/resident_service_refresh.h"
#include <QCoreApplication>
#include <QStringList>

// Standalone process (mirrors session_activation_publisher.cpp) so the test
// can observe exactly the D-Bus calls one refresh invocation makes, isolated
// from the rest of qindaqt-session's startup.
int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    // Optional leading `--socket <path>`: explicit systemd private-socket
    // path for the hermetic lane; everything else stays unit names, exactly
    // like the session supervisor call.
    QString socketPath;
    QStringList unitNames;
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == QStringLiteral("--socket") && i + 1 < argc) {
            socketPath = QString::fromLocal8Bit(argv[++i]);
        } else {
            unitNames.append(arg);
        }
    }
    return QindaQt::SessionSupervisor::refreshResidentServices(
        QDBusConnection::sessionBus(), unitNames, socketPath) ? 0 : 3;
}
