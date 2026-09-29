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
    // `--own <name>`: stand in for a D-Bus-activated service by owning
    // <name> until terminated (replaced_activation_owner tests).
    if (argc == 3 && QString::fromLocal8Bit(argv[1]) == QStringLiteral("--own")) {
        if (!QDBusConnection::sessionBus().registerService(QString::fromLocal8Bit(argv[2]))) {
            return 4;
        }
        return application.exec();
    }
    // Optional leading `--socket <path>`: explicit systemd private-socket
    // path for the hermetic lane; everything else stays unit names, exactly
    // like the session supervisor call.
    using QindaQt::SessionSupervisor::SessionActivationScope;
    auto scope = SessionActivationScope::PhysicalDesktop;
    QString socketPath;
    QStringList unitNames;
    // `--pipewire-hung` / `--pipewire-answers`: run the PipeWire liveness
    // refresh with a probe that reports that outcome (exit 0 = restart
    // requested, 6 = nothing requested).
    bool pipeWireProbe = false;
    bool pipeWireHung = false;
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == QStringLiteral("--socket") && i + 1 < argc) {
            socketPath = QString::fromLocal8Bit(argv[++i]);
        } else if (arg == QStringLiteral("--private")) {
            scope = SessionActivationScope::Private;
        } else if (arg == QStringLiteral("--pipewire-hung")) {
            pipeWireProbe = true;
            pipeWireHung = true;
        } else if (arg == QStringLiteral("--pipewire-answers")) {
            pipeWireProbe = true;
        } else {
            unitNames.append(arg);
        }
    }
    if (pipeWireProbe) {
        const bool requested = QindaQt::SessionSupervisor::refreshUnresponsivePipeWire(
            QDBusConnection::sessionBus(), [pipeWireHung] { return !pipeWireHung; },
            socketPath, scope);
        return requested ? 0 : 6;
    }
    return QindaQt::SessionSupervisor::refreshResidentServices(
        QDBusConnection::sessionBus(), unitNames, socketPath, scope) ? 0 : 3;
}
