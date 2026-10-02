// SPDX-License-Identifier: GPL-3.0-or-later
#include <QCoreApplication>
#include <QFile>
#include <QTimer>
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QFile marker(qEnvironmentVariable("QINDAQT_TEST_POWER_START_MARKER"));
    if (!marker.open(QIODevice::WriteOnly)) return 2;
    marker.write(QByteArray::number(QCoreApplication::applicationPid()));
    marker.close();
    QTimer::singleShot(30000, &app, &QCoreApplication::quit);
    return app.exec();
}
