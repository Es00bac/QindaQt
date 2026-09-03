// SPDX-License-Identifier: GPL-3.0-or-later

#include <QCoreApplication>
#include <QTimer>

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    bool valid = false;
    const int configured = qEnvironmentVariableIntValue(
        "QINDAQT_TEST_PLAIN_CHILD_MILLISECONDS", &valid);
    const int lifetime = valid && configured >= 0 ? configured : 20;
    QTimer::singleShot(lifetime, &application, &QCoreApplication::quit);
    return application.exec();
}
