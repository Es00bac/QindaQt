// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QPoint>
#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QSizeF>
#include <QString>
#include <Qt>

#include <optional>

// Maps desktop window input onto the phone's virtual display.
//
// ViewportMapper is pure geometry: the decoded frame is drawn aspect-preserving and centred in
// the window (letterboxed), and window positions map back to virtual-display pixels through
// the same rectangle. The presenter draws with contentRect(), so drawing and input can never
// disagree about where the phone content is.
//
// InputTranslator turns window events into encoded protocol frames. It holds the pointer and
// key state needed to keep the phone's gesture streams well-formed (every down gets an up).
//
// Thread-safety: GUI-thread value objects.
namespace qindaqt::link_mirror {

class ViewportMapper {
public:
    void setDisplaySize(QSize displayPixels) { m_display = displayPixels; }
    void setViewSize(QSizeF viewSize) { m_view = viewSize; }
    [[nodiscard]] QSize displaySize() const { return m_display; }
    [[nodiscard]] QSizeF viewSize() const { return m_view; }

    // Where the frame is drawn, in view coordinates; empty until both sizes are known.
    [[nodiscard]] QRectF contentRect() const;
    // Display pixel under a view position, or nullopt outside the drawn frame.
    [[nodiscard]] std::optional<QPointF> toDisplay(QPointF viewPosition) const;
    // Like toDisplay but clamped to the display edge; used while a press is held so a drag
    // that leaves the frame still ends inside the phone display.
    [[nodiscard]] std::optional<QPointF> toDisplayClamped(QPointF viewPosition) const;

private:
    QSize m_display;
    QSizeF m_view;
};

// Android KeyEvent constants used by the translator (android.view.KeyEvent).
namespace android_key {
inline constexpr int Back = 4;
inline constexpr int MetaShiftOn = 0x1;
inline constexpr int MetaAltOn = 0x2;
inline constexpr int MetaCtrlOn = 0x1000;
inline constexpr int MetaMetaOn = 0x10000;
} // namespace android_key

// Android keycode for a Qt key, or 0 when it has no mapping.
[[nodiscard]] int androidKeycode(int qtKey);
// Android meta-state bits for Qt modifiers.
[[nodiscard]] int androidMetaState(Qt::KeyboardModifiers modifiers);

class InputTranslator {
public:
    explicit InputTranslator(const ViewportMapper &mapper) : m_mapper(mapper) {}

    // AGENT-NOTE: the primary button drives a finger (phone apps are touch-first), the
    // secondary and Back buttons send Android BACK, because a mirror display has no navigation bar.
    [[nodiscard]] QList<QByteArray> mousePress(QPointF viewPosition, Qt::MouseButton button);
    [[nodiscard]] QList<QByteArray> mouseMove(QPointF viewPosition);
    [[nodiscard]] QList<QByteArray> mouseRelease(QPointF viewPosition, Qt::MouseButton button);
    // angleDelta in eighths of a degree (120 per wheel notch), as QWheelEvent reports it.
    [[nodiscard]] QList<QByteArray> wheel(QPointF viewPosition, QPoint angleDelta);
    // Printable text without Ctrl/Alt/Meta becomes one text frame on press; other keys
    // become key frames.
    [[nodiscard]] QList<QByteArray> keyPress(int qtKey, Qt::KeyboardModifiers modifiers, const QString &text,
                                             bool autoRepeat);
    [[nodiscard]] QList<QByteArray> keyRelease(int qtKey, Qt::KeyboardModifiers modifiers, bool autoRepeat);
    // Ends a held finger, e.g. when the window loses focus mid-drag.
    [[nodiscard]] QList<QByteArray> cancel();
    // Cancels the finger and releases every key still down (focus loss, shutdown).
    [[nodiscard]] QList<QByteArray> releaseAll();

    [[nodiscard]] bool touching() const { return m_touching; }

private:
    const ViewportMapper &m_mapper;
    bool m_touching = false;
    QPointF m_lastTouch;
    QHash<int, int> m_downKeys; // Android keycode -> repeat count of its current press
};

} // namespace qindaqt::link_mirror
