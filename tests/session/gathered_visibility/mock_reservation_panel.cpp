/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Private nested-session top panel. Its committed layer-shell exclusive zone
 * lets the gathered compositor row inspect real KWin maximize-area placement.
 */
#include <LayerShellQt/Window>

#include <QBackingStore>
#include <QFile>
#include <QGuiApplication>
#include <QPainter>
#include <QTimer>
#include <QWindow>

int main(int argc, char **argv)
{
    QGuiApplication application(argc, argv);
    if (argc != 2) {
        return 2;
    }

    QWindow panel;
    panel.setSurfaceType(QSurface::RasterSurface);
    panel.setTitle(QStringLiteral("Gather test work-area reservation"));
    panel.setFlags(Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);
    auto *const layer = LayerShellQt::Window::get(&panel);
    if (!layer) {
        return 3;
    }
    layer->setScope(QStringLiteral("dock"));
    layer->setLayer(LayerShellQt::Window::LayerTop);
    LayerShellQt::Window::Anchors anchors = LayerShellQt::Window::AnchorTop;
    anchors |= LayerShellQt::Window::AnchorLeft;
    anchors |= LayerShellQt::Window::AnchorRight;
    layer->setAnchors(anchors);
    layer->setDesiredSize(QSize(0, 40));
    layer->setExclusiveEdge(LayerShellQt::Window::AnchorTop);
    layer->setExclusiveZone(40);
    layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);

    QBackingStore store(&panel);
    panel.resize(640, 40);
    panel.show();
    QTimer::singleShot(350, &application, [&] {
        store.resize(panel.size());
        const QRect area(QPoint{}, panel.size());
        store.beginPaint(area);
        QPainter painter(store.paintDevice());
        painter.fillRect(area, QColor(QStringLiteral("#51412f")));
        painter.end();
        store.endPaint();
        store.flush(area);
        QFile ready(QString::fromLocal8Bit(argv[1]));
        if (ready.open(QIODevice::WriteOnly)) {
            ready.write("mapped");
        }
    });
    return application.exec();
}
