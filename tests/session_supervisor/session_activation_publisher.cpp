// SPDX-License-Identifier: GPL-3.0-or-later
#include "src/session_supervisor/src/activation_environment.h"
#include <QCoreApplication>
int main(int argc, char **argv) {
    QCoreApplication application(argc, argv);
    // Optional argv[1]: explicit systemd private-socket path (hermetic lane);
    // empty means the computed default, exactly like the session supervisor.
    const QString socketPath = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QString();
    QindaQt::SessionSupervisor::publishActivationEnvironment(
        QDBusConnection::sessionBus(), QProcessEnvironment::systemEnvironment(), socketPath);
    return 0;
}
