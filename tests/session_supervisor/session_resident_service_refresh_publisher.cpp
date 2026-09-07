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
    QStringList unitNames;
    for (int i = 1; i < argc; ++i) {
        unitNames.append(QString::fromLocal8Bit(argv[i]));
    }
    QindaQt::SessionSupervisor::refreshResidentWaylandServices(
        QDBusConnection::sessionBus(), unitNames);
    return 0;
}
