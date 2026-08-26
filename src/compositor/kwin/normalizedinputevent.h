// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QMetaType>
#include <QPointF>
#include <QString>
#include <Qt>

namespace KWin {
struct KeyboardKeyEvent;
struct PointerAxisEvent;
struct PointerButtonEvent;
struct PointerHoldGestureBeginEvent;
struct PointerHoldGestureCancelEvent;
struct PointerHoldGestureEndEvent;
struct PointerMotionEvent;
struct PointerPinchGestureBeginEvent;
struct PointerPinchGestureCancelEvent;
struct PointerPinchGestureEndEvent;
struct PointerPinchGestureUpdateEvent;
struct PointerSwipeGestureBeginEvent;
struct PointerSwipeGestureCancelEvent;
struct PointerSwipeGestureEndEvent;
struct PointerSwipeGestureUpdateEvent;
struct SwitchEvent;
struct TabletPadButtonEvent;
struct TabletPadDialEvent;
struct TabletPadRingEvent;
struct TabletPadStripEvent;
struct TabletToolAxisEvent;
struct TabletToolButtonEvent;
struct TabletToolProximityEvent;
struct TabletToolTipEvent;
struct TouchDownEvent;
struct TouchMotionEvent;
struct TouchUpEvent;
}

namespace QindaQt::Compositor::KWinIntegration {

enum class NormalizedInputEventKind : quint8 {
    PointerMotion,
    PointerButton,
    PointerAxis,
    PointerSwipeBegin,
    PointerSwipeUpdate,
    PointerSwipeEnd,
    PointerSwipeCancel,
    PointerPinchBegin,
    PointerPinchUpdate,
    PointerPinchEnd,
    PointerPinchCancel,
    PointerHoldBegin,
    PointerHoldEnd,
    PointerHoldCancel,
    KeyboardKey,
    TouchDown,
    TouchMotion,
    TouchUp,
    SwitchToggle,
    TabletToolProximity,
    TabletToolAxis,
    TabletToolTip,
    TabletToolButton,
    TabletPadButton,
    TabletPadStrip,
    TabletPadRing,
    TabletPadDial,
};

enum class NormalizedInputState : quint8 {
    None,
    Pressed,
    Released,
    Repeated,
    Entered,
    Exited,
    Cancelled,
    On,
    Off,
};

enum class NormalizedAxisSource : quint8 {
    Unknown,
    Wheel,
    Finger,
    Continuous,
    WheelTilt,
};

enum class NormalizedTabletToolType : quint8 {
    Unknown,
    Pen,
    Eraser,
    Brush,
    Pencil,
    Airbrush,
    Finger,
    Mouse,
    Lens,
    Totem,
};

enum NormalizedTabletToolCapability : quint32 {
    TabletToolTilt = 1U << 0U,
    TabletToolPressure = 1U << 1U,
    TabletToolDistance = 1U << 2U,
    TabletToolRotation = 1U << 3U,
    TabletToolSlider = 1U << 4U,
    TabletToolWheel = 1U << 5U,
};

struct NormalizedInputEvent final
{
    // AGENT-CONTRACT: Positions are KWin logical compositor coordinates and
    // timestamps are monotonic microseconds with no wall-clock epoch.
    NormalizedInputEventKind kind = NormalizedInputEventKind::PointerMotion;
    NormalizedInputState state = NormalizedInputState::None;
    NormalizedAxisSource axisSource = NormalizedAxisSource::Unknown;
    NormalizedTabletToolType tabletToolType = NormalizedTabletToolType::Unknown;
    QString deviceId;
    qint64 timestampUsec = 0;
    QPointF position;
    QPointF delta;
    QPointF unacceleratedDelta;
    Qt::KeyboardModifiers modifiers = Qt::NoModifier;
    Qt::KeyboardModifiers shortcutModifiers = Qt::NoModifier;
    Qt::MouseButtons buttons = Qt::NoButton;
    Qt::Orientation orientation = Qt::Horizontal;
    quint32 logicalCode = 0;
    quint32 nativeCode = 0;
    quint32 group = 0;
    quint32 mode = 0;
    quint32 tabletToolCapabilities = 0;
    quint64 tabletToolSerialId = 0;
    quint64 tabletToolUniqueId = 0;
    qint32 contactId = -1;
    qint32 discreteValue = 0;
    int fingerCount = 0;
    int controlNumber = -1;
    qreal value = 0.0;
    qreal scale = 1.0;
    qreal angleDelta = 0.0;
    qreal pressure = 0.0;
    qreal rotation = 0.0;
    qreal sliderPosition = 0.0;
    qreal xTilt = 0.0;
    qreal yTilt = 0.0;
    qreal distance = 0.0;
    bool hasPosition = false;
    bool hasDelta = false;
    bool hasUnacceleratedDelta = false;
    bool hasOrientation = false;
    bool warp = false;
    bool inverted = false;
    bool isFinger = false;
    bool isModeSwitch = false;
};

[[nodiscard]] NormalizedInputEvent normalizeInputEvent(const KWin::PointerMotionEvent &event,
                                                       QString deviceId);
[[nodiscard]] NormalizedInputEvent normalizeInputEvent(const KWin::PointerButtonEvent &event,
                                                       QString deviceId);
[[nodiscard]] NormalizedInputEvent normalizeInputEvent(const KWin::PointerAxisEvent &event,
                                                       QString deviceId);
[[nodiscard]] NormalizedInputEvent normalizeInputEvent(
    const KWin::PointerSwipeGestureBeginEvent &event);
[[nodiscard]] NormalizedInputEvent normalizeInputEvent(
    const KWin::PointerSwipeGestureUpdateEvent &event);
[[nodiscard]] NormalizedInputEvent normalizeInputEvent(
    const KWin::PointerSwipeGestureEndEvent &event);
[[nodiscard]] NormalizedInputEvent normalizeInputEvent(
    const KWin::PointerSwipeGestureCancelEvent &event);
[[nodiscard]] NormalizedInputEvent normalizeInputEvent(
    const KWin::PointerPinchGestureBeginEvent &event);
[[nodiscard]] NormalizedInputEvent normalizeInputEvent(
    const KWin::PointerPinchGestureUpdateEvent &event);
[[nodiscard]] NormalizedInputEvent normalizeInputEvent(
    const KWin::PointerPinchGestureEndEvent &event);
[[nodiscard]] NormalizedInputEvent normalizeInputEvent(
    const KWin::PointerPinchGestureCancelEvent &event);
[[nodiscard]] NormalizedInputEvent normalizeInputEvent(
    const KWin::PointerHoldGestureBeginEvent &event);
[[nodiscard]] NormalizedInputEvent normalizeInputEvent(
    const KWin::PointerHoldGestureEndEvent &event);
[[nodiscard]] NormalizedInputEvent normalizeInputEvent(
    const KWin::PointerHoldGestureCancelEvent &event);
[[nodiscard]] NormalizedInputEvent normalizeInputEvent(const KWin::KeyboardKeyEvent &event,
                                                       QString deviceId);
[[nodiscard]] NormalizedInputEvent normalizeInputEvent(const KWin::TouchDownEvent &event);
[[nodiscard]] NormalizedInputEvent normalizeInputEvent(const KWin::TouchMotionEvent &event);
[[nodiscard]] NormalizedInputEvent normalizeInputEvent(const KWin::TouchUpEvent &event);
[[nodiscard]] NormalizedInputEvent normalizeInputEvent(const KWin::SwitchEvent &event,
                                                       QString deviceId);
[[nodiscard]] NormalizedInputEvent normalizeInputEvent(
    const KWin::TabletToolProximityEvent &event, QString deviceId);
[[nodiscard]] NormalizedInputEvent normalizeInputEvent(const KWin::TabletToolAxisEvent &event,
                                                       QString deviceId);
[[nodiscard]] NormalizedInputEvent normalizeInputEvent(const KWin::TabletToolTipEvent &event,
                                                       QString deviceId);
[[nodiscard]] NormalizedInputEvent normalizeInputEvent(const KWin::TabletToolButtonEvent &event,
                                                       QString deviceId);
[[nodiscard]] NormalizedInputEvent normalizeInputEvent(const KWin::TabletPadButtonEvent &event,
                                                       QString deviceId);
[[nodiscard]] NormalizedInputEvent normalizeInputEvent(const KWin::TabletPadStripEvent &event,
                                                       QString deviceId);
[[nodiscard]] NormalizedInputEvent normalizeInputEvent(const KWin::TabletPadRingEvent &event,
                                                       QString deviceId);
[[nodiscard]] NormalizedInputEvent normalizeInputEvent(const KWin::TabletPadDialEvent &event,
                                                       QString deviceId);

} // namespace QindaQt::Compositor::KWinIntegration

Q_DECLARE_METATYPE(QindaQt::Compositor::KWinIntegration::NormalizedInputEvent)
