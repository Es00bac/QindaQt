// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/link_mirror/input_mapper.h"

#include <QImage>
#include <QList>
#include <QRasterWindow>
#include <QString>

class QPainter;

// Presents decoded phone frames in an ordinary desktop toplevel and forwards its input.
//
// The window draws the newest frame letterboxed into ViewportMapper::contentRect() and turns
// window input into protocol frames through an InputTranslator over the same mapper. It owns
// neither the connection nor the decoder; it only emits frames for the controller to send.
//
// AGENT-NOTE: renderSnapshot() runs the same drawing code as paintEvent() so --grab evidence
// shows exactly what a visible window would show, without mapping a window on the desktop.
namespace qindaqt::link_mirror {

class MirrorWindow : public QRasterWindow {
    Q_OBJECT

public:
    explicit MirrorWindow(ViewportMapper &mapper);

    void setFrame(QImage frame);
    void setStatusText(const QString &text);
    // Paused: the phone's own screen is off. The last frame stays, dimmed under a notice, and
    // input is dropped (not queued) until resumed.
    void setPaused(bool paused);
    [[nodiscard]] bool paused() const { return m_paused; }
    [[nodiscard]] const QImage &frame() const { return m_frame; }
    // Draws the current presentation at a view size (logical pixels == image pixels).
    [[nodiscard]] QImage renderSnapshot(QSize viewSize) const;

Q_SIGNALS:
    void inputFrames(const QList<QByteArray> &frames);
    void viewResized(QSize logicalSize, qreal devicePixelRatio);
    void closeRequested();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    bool event(QEvent *event) override;

private:
    void render(QPainter &painter, const ViewportMapper &mapper, QSizeF viewSize) const;
    void forward(const QList<QByteArray> &frames);

    ViewportMapper &m_mapper;
    InputTranslator m_translator;
    QImage m_frame;
    QString m_status;
    bool m_paused = false;
};

} // namespace qindaqt::link_mirror
