// SPDX-License-Identifier: GPL-3.0-or-later
// Ordinary Wayland clipboard peer for the private remote-input journey. It
// only touches the private compositor named by WAYLAND_DISPLAY.
#include <QClipboard>
#include <QGuiApplication>
#include <QRasterWindow>
#include <QTimer>
#include <QKeyEvent>
#include <cstdio>

class ClipboardWindow final : public QRasterWindow
{
public:
    QString copyPayload;
    bool copyMode = false;
protected:
    bool event(QEvent *event) override
    {
        if (copyMode && event->type() == QEvent::KeyPress) {
            const auto *key = static_cast<QKeyEvent *>(event);
            if (key->key() == Qt::Key_C && key->modifiers().testFlag(Qt::ControlModifier)) {
                // Actual input provides the client serial required to replace
                // a newer remote clipboard selection; focus alone does not.
                QGuiApplication::clipboard()->setText(copyPayload);
                std::puts("COPIED"); std::fflush(stdout);
                return true;
            }
        }
        return QRasterWindow::event(event);
    }
};

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    const QString mode = app.arguments().value(1);
    const QString payload = app.arguments().value(2);
    ClipboardWindow window;
    window.copyMode = mode == QLatin1String("copy");
    window.copyPayload = payload;
    window.resize(240, 160);
    window.show();
    QObject::connect(&app, &QGuiApplication::focusWindowChanged, &app, [&](QWindow *focus) {
        if (focus != &window) return;
        QTimer::singleShot(200, &app, [&] {
            if (mode == QLatin1String("copy")) {
                std::puts("READY");
            } else {
                std::printf("PASTED %s\n", qPrintable(QGuiApplication::clipboard()->text()));
                QTimer::singleShot(0, &app, &QCoreApplication::quit);
            }
            std::fflush(stdout);
        });
    });
    return app.exec();
}
