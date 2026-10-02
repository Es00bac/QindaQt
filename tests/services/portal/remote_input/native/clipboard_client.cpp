// SPDX-License-Identifier: GPL-3.0-or-later
// Ordinary Wayland clipboard peer for the private remote-input journey. It
// only touches the private compositor named by WAYLAND_DISPLAY.
#include <QClipboard>
#include <QGuiApplication>
#include <QRasterWindow>
#include <QTimer>
#include <cstdio>

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    const QString mode = app.arguments().value(1);
    const QString payload = app.arguments().value(2);
    QRasterWindow window;
    window.resize(240, 160);
    window.show();
    QObject::connect(&app, &QGuiApplication::focusWindowChanged, &app, [&](QWindow *focus) {
        if (focus != &window) return;
        QTimer::singleShot(200, &app, [&] {
            if (mode == QLatin1String("copy")) {
                QGuiApplication::clipboard()->setText(payload);
                std::puts("COPIED");
            } else {
                std::printf("PASTED %s\n", qPrintable(QGuiApplication::clipboard()->text()));
                QTimer::singleShot(0, &app, &QCoreApplication::quit);
            }
            std::fflush(stdout);
        });
    });
    return app.exec();
}
