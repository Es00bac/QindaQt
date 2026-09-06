// SPDX-License-Identifier: GPL-3.0-or-later
#include "src/session_supervisor/src/activation_environment.h"
#include <QCoreApplication>
int main(int argc, char **argv) {
    QCoreApplication application(argc, argv);
    QindaQt::SessionSupervisor::publishActivationEnvironment(
        QDBusConnection::sessionBus(), QProcessEnvironment::systemEnvironment());
    return 0;
}
