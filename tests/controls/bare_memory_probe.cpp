// SPDX-License-Identifier: GPL-3.0-or-later
#include <QGuiApplication>
#include <QQuickItem>
#include <QQuickView>
#include <QSizeF>
#include <QTextStream>
#include <QTimer>

int main(int argc, char **argv)
{
    QGuiApplication application(argc, argv);
    QQuickView view;
    view.resize(720, 840);
    auto *root = new QQuickItem(view.contentItem());
    root->setSize(QSizeF(720, 840));
    view.show();

    QTimer::singleShot(200, &application, [&application]() {
        QTextStream(stdout) << "READY " << application.applicationPid() << Qt::endl;
    });
    QTimer::singleShot(30000, &application, &QCoreApplication::quit);
    return application.exec();
}
