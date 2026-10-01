// SPDX-License-Identifier: GPL-3.0-or-later
#include <QApplication>
#include <QPainter>
#include <QTimer>
#include <QWidget>
#include <QWindow>
#include <cstdio>
class Pixels final : public QWidget {
public:
    Pixels() {
        setWindowTitle("Private changing capture pixels"); timer.setInterval(150);
        connect(&timer, &QTimer::timeout, this, [this] { alternate = !alternate; update(); }); timer.start();
    }
protected: void paintEvent(QPaintEvent *) override { QPainter painter(this); painter.fillRect(rect(), alternate ? QColor(220, 30, 50) : QColor(20, 200, 70)); }
private: QTimer timer; bool alternate = false;
};
int main(int argc, char **argv) {
    if (qEnvironmentVariable("WAYLAND_SOCKET").isEmpty() || !qEnvironmentVariable("WAYLAND_DISPLAY").isEmpty()) return 2;
    QApplication app(argc, argv); Pixels pixels; pixels.showFullScreen();
    QTimer mapped; mapped.setInterval(20); QObject::connect(&mapped, &QTimer::timeout, &app, [&] {
        if (pixels.windowHandle() && pixels.windowHandle()->isExposed()) { mapped.stop(); puts("mapped private pixels"); fflush(stdout); }
    }); mapped.start(); return app.exec();
}
