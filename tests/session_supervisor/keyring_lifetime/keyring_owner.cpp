// SPDX-License-Identifier: GPL-3.0-or-later
#include "src/session_supervisor/src/keyring_session_lifetime.h"
#include <QCoreApplication>
#include <QSocketNotifier>
#include <QTextStream>
#include <QTimer>
#include <unistd.h>
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QindaQt::SessionSupervisor::KeyringSessionLifetime session;
    session.start(QStringLiteral("/bin/true")); // adopt only explicitly started private daemon
    QSocketNotifier input(STDIN_FILENO, QSocketNotifier::Read);
    QObject::connect(&input, &QSocketNotifier::activated, &app, [&] {
        char command[16];
        if (::read(STDIN_FILENO, command, sizeof(command)) <= 0) app.quit();
        else { session.stop(); app.quit(); }
    });
    QTimer::singleShot(25'000, &app, &QCoreApplication::quit);
    QTextStream(stdout) << "ready\n" << Qt::flush;
    return app.exec();
}
