// SPDX-License-Identifier: GPL-3.0-or-later
#include <QApplication>
#include <QPainter>
#include <QTimer>
#include <QWidget>
#include <QWindow>
#include <QtGui/qpa/qplatformnativeinterface.h>
#include <QtGui/qguiapplication_platform.h>
#include <wayland-client.h>
#include <atomic>
#include <cstdio>
class Pixels final : public QWidget {
public:
    Pixels() {
        setWindowTitle("Private changing capture pixels"); timer.setInterval(150);
        connect(&timer, &QTimer::timeout, this, [this] { alternate = !alternate; update(); }); timer.start();
    }
    int paintCount() const { return paints; }
protected: void paintEvent(QPaintEvent *) override { QPainter painter(this); painter.fillRect(rect(), alternate ? QColor(220, 30, 50) : QColor(20, 200, 70)); ++paints; }
private: QTimer timer; bool alternate = false; int paints = 0;
};
int main(int argc, char **argv) {
    if (qEnvironmentVariable("WAYLAND_SOCKET").isEmpty() || !qEnvironmentVariable("WAYLAND_DISPLAY").isEmpty()) return 2;
    QApplication app(argc, argv);
    // AGENT-CONTRACT: run_native_capture.py supplies this real task-only desktop
    // entry. Qt registers with the host portal when its frontend appears; an
    // empty identity produces a fatal warning after initial frame readiness.
    QGuiApplication::setDesktopFileName(QStringLiteral("org.test.CapturePixels"));
    Pixels pixels; pixels.showFullScreen();
    // Callback data survives Qt display teardown after a bounded early exit.
    static std::atomic_bool frameCompleted = false;
    QTimer mapped; mapped.setInterval(20); QObject::connect(&mapped, &QTimer::timeout, &app, [&] {
        if (!pixels.windowHandle() || !pixels.windowHandle()->isExposed() || pixels.paintCount() == 0) return;
        // AGENT-CONTRACT: exposure is not a compositor frame. After a real
        // widget paint, request the public Wayland callback on its actual Qt
        // surface and commit only existing state. No buffer/pixel is fabricated.
        auto *native = QGuiApplication::platformNativeInterface();
        auto *surface = static_cast<wl_surface *>(native->nativeResourceForWindow(QByteArrayLiteral("surface"), pixels.windowHandle()));
        auto *wayland = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>();
        if (!surface || !wayland) { app.exit(2); return; }
        auto *callback = wl_surface_frame(surface);
        static const wl_callback_listener completed{[](void *data, wl_callback *frame, uint32_t timestamp) {
            wl_callback_destroy(frame);
            static_cast<std::atomic_bool *>(data)->store(true);
            printf("completed private pixels frame %u\n", timestamp); fflush(stdout);
        }};
        if (wl_callback_add_listener(callback, &completed, &frameCompleted) != 0) { wl_callback_destroy(callback); app.exit(2); return; }
        wl_surface_commit(surface); wl_display_flush(wayland->display());
        mapped.stop(); printf("mapped private pixels; actual paints %d\n", pixels.paintCount()); fflush(stdout);
    }); mapped.start();
    QTimer::singleShot(8000, &app, [&] { if (!frameCompleted.load()) app.exit(3); });
    return app.exec();
}
