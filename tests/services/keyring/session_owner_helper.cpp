// SPDX-License-Identifier: GPL-3.0-or-later
#include "src/session_supervisor/src/keyring_session_lifetime.h"
#include <QCoreApplication>
#include <QTimer>
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);
    QindaQt::SessionSupervisor::KeyringSessionLifetime lifetime;
    lifetime.start("/bin/true"); // An already activated private-bus owner is adopted.
    QTimer::singleShot(800,&app,&QCoreApplication::quit);
    return app.exec();
}
