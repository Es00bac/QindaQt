// SPDX-License-Identifier: GPL-3.0-or-later
#include <QCoreApplication>
#include <QFile>
#include <QTextStream>

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    if (application.arguments().size() != 2)
        return 2;
    QFile marker(application.arguments().at(1));
    if (!marker.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        return 3;
    QTextStream(&marker) << application.applicationPid() << '\n';
    marker.close();
    return application.exec();
}
