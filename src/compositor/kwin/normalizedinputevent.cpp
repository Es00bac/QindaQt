// SPDX-License-Identifier: GPL-3.0-or-later
#include "normalizedinputevent.h"

#include <core/inputdevice.h>
#include <input_event.h>

#include <utility>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

qint64 timestamp(std::chrono::microseconds value)
{
    return static_cast<qint64>(value.count());
}

NormalizedInputEvent baseEvent(NormalizedInputEventKind kind,
                               std::chrono::microseconds time,
                               QString deviceId = {})
{
    NormalizedInputEvent normalized;
    normalized.kind = kind;
    normalized.timestampUsec = timestamp(time);
    normalized.deviceId = std::move(deviceId);
    return normalized;
}

NormalizedInputState keyState(KWin::KeyboardKeyState state)
{
    switch (state) {
    case KWin::KeyboardKeyState::Pressed:
        return NormalizedInputState::Pressed;
    case KWin::KeyboardKeyState::Released:
        return NormalizedInputState::Released;
    case KWin::KeyboardKeyState::Repeated:
        return NormalizedInputState::Repeated;
    }
    return NormalizedInputState::None;
}

NormalizedInputState buttonState(KWin::PointerButtonState state)
{
    return state == KWin::PointerButtonState::Pressed ? NormalizedInputState::Pressed
                                                      : NormalizedInputState::Released;
}

NormalizedAxisSource axisSource(KWin::PointerAxisSource source)
{
    switch (source) {
    case KWin::PointerAxisSource::Unknown:
        return NormalizedAxisSource::Unknown;
    case KWin::PointerAxisSource::Wheel:
        return NormalizedAxisSource::Wheel;
    case KWin::PointerAxisSource::Finger:
        return NormalizedAxisSource::Finger;
    case KWin::PointerAxisSource::Continuous:
        return NormalizedAxisSource::Continuous;
    case KWin::PointerAxisSource::WheelTilt:
        return NormalizedAxisSource::WheelTilt;
    }
    return NormalizedAxisSource::Unknown;
}

NormalizedTabletToolType toolType(KWin::InputDeviceTabletTool::Type type)
{
    switch (type) {
    case KWin::InputDeviceTabletTool::Pen:
        return NormalizedTabletToolType::Pen;
    case KWin::InputDeviceTabletTool::Eraser:
        return NormalizedTabletToolType::Eraser;
    case KWin::InputDeviceTabletTool::Brush:
        return NormalizedTabletToolType::Brush;
    case KWin::InputDeviceTabletTool::Pencil:
        return NormalizedTabletToolType::Pencil;
    case KWin::InputDeviceTabletTool::Airbrush:
        return NormalizedTabletToolType::Airbrush;
    case KWin::InputDeviceTabletTool::Finger:
        return NormalizedTabletToolType::Finger;
    case KWin::InputDeviceTabletTool::Mouse:
        return NormalizedTabletToolType::Mouse;
    case KWin::InputDeviceTabletTool::Lens:
        return NormalizedTabletToolType::Lens;
    case KWin::InputDeviceTabletTool::Totem:
        return NormalizedTabletToolType::Totem;
    }
    return NormalizedTabletToolType::Unknown;
}

quint32 toolCapabilities(const KWin::InputDeviceTabletTool &tool)
{
    quint32 result = 0;
    for (const auto capability : tool.capabilities()) {
        switch (capability) {
        case KWin::InputDeviceTabletTool::Tilt:
            result |= TabletToolTilt;
            break;
        case KWin::InputDeviceTabletTool::Pressure:
            result |= TabletToolPressure;
            break;
        case KWin::InputDeviceTabletTool::Distance:
            result |= TabletToolDistance;
            break;
        case KWin::InputDeviceTabletTool::Rotation:
            result |= TabletToolRotation;
            break;
        case KWin::InputDeviceTabletTool::Slider:
            result |= TabletToolSlider;
            break;
        case KWin::InputDeviceTabletTool::Wheel:
            result |= TabletToolWheel;
            break;
        }
    }
    return result;
}

void addToolIdentity(NormalizedInputEvent &event, KWin::InputDeviceTabletTool *tool)
{
    if (!tool) {
        return;
    }
    event.tabletToolSerialId = tool->serialId();
    event.tabletToolUniqueId = tool->uniqueId();
    event.tabletToolType = toolType(tool->type());
    event.tabletToolCapabilities = toolCapabilities(*tool);
}

void addTabletAxes(NormalizedInputEvent &normalized,
                   const QPointF &position,
                   qreal pressure,
                   qreal rotation,
                   qreal sliderPosition,
                   qreal xTilt,
                   qreal yTilt,
                   qreal distance)
{
    normalized.position = position;
    normalized.hasPosition = true;
    normalized.pressure = pressure;
    normalized.rotation = rotation;
    normalized.sliderPosition = sliderPosition;
    normalized.xTilt = xTilt;
    normalized.yTilt = yTilt;
    normalized.distance = distance;
}

} // namespace

NormalizedInputEvent normalizeInputEvent(const KWin::PointerMotionEvent &event,
                                         QString deviceId)
{
    auto normalized = baseEvent(NormalizedInputEventKind::PointerMotion, event.timestamp,
                                std::move(deviceId));
    normalized.position = event.position;
    normalized.delta = event.delta;
    normalized.unacceleratedDelta = event.deltaUnaccelerated;
    normalized.hasPosition = true;
    normalized.hasDelta = true;
    normalized.hasUnacceleratedDelta = true;
    normalized.warp = event.warp;
    normalized.buttons = event.buttons;
    normalized.modifiers = event.modifiers;
    normalized.shortcutModifiers = event.modifiersRelevantForShortcuts;
    return normalized;
}

NormalizedInputEvent normalizeInputEvent(const KWin::PointerButtonEvent &event,
                                         QString deviceId)
{
    auto normalized = baseEvent(NormalizedInputEventKind::PointerButton, event.timestamp,
                                std::move(deviceId));
    normalized.state = buttonState(event.state);
    normalized.position = event.position;
    normalized.hasPosition = true;
    normalized.logicalCode = static_cast<quint32>(event.button);
    normalized.nativeCode = event.nativeButton;
    normalized.buttons = event.buttons;
    normalized.modifiers = event.modifiers;
    normalized.shortcutModifiers = event.modifiersRelevantForShortcuts;
    return normalized;
}

NormalizedInputEvent normalizeInputEvent(const KWin::PointerAxisEvent &event,
                                         QString deviceId)
{
    auto normalized = baseEvent(NormalizedInputEventKind::PointerAxis, event.timestamp,
                                std::move(deviceId));
    normalized.position = event.position;
    normalized.hasPosition = true;
    normalized.value = event.delta;
    normalized.discreteValue = event.deltaV120;
    normalized.orientation = event.orientation;
    normalized.hasOrientation = true;
    normalized.axisSource = axisSource(event.source);
    normalized.buttons = event.buttons;
    normalized.modifiers = event.modifiers;
    normalized.shortcutModifiers = event.modifiersRelevantForGlobalShortcuts;
    normalized.inverted = event.inverted;
    return normalized;
}

NormalizedInputEvent normalizeInputEvent(const KWin::PointerSwipeGestureBeginEvent &event)
{
    auto normalized = baseEvent(NormalizedInputEventKind::PointerSwipeBegin, event.time);
    normalized.fingerCount = event.fingerCount;
    return normalized;
}

NormalizedInputEvent normalizeInputEvent(const KWin::PointerSwipeGestureUpdateEvent &event)
{
    auto normalized = baseEvent(NormalizedInputEventKind::PointerSwipeUpdate, event.time);
    normalized.delta = event.delta;
    normalized.hasDelta = true;
    return normalized;
}

NormalizedInputEvent normalizeInputEvent(const KWin::PointerSwipeGestureEndEvent &event)
{
    return baseEvent(NormalizedInputEventKind::PointerSwipeEnd, event.time);
}

NormalizedInputEvent normalizeInputEvent(const KWin::PointerSwipeGestureCancelEvent &event)
{
    auto normalized = baseEvent(NormalizedInputEventKind::PointerSwipeCancel, event.time);
    normalized.state = NormalizedInputState::Cancelled;
    return normalized;
}

NormalizedInputEvent normalizeInputEvent(const KWin::PointerPinchGestureBeginEvent &event)
{
    auto normalized = baseEvent(NormalizedInputEventKind::PointerPinchBegin, event.time);
    normalized.fingerCount = event.fingerCount;
    return normalized;
}

NormalizedInputEvent normalizeInputEvent(const KWin::PointerPinchGestureUpdateEvent &event)
{
    auto normalized = baseEvent(NormalizedInputEventKind::PointerPinchUpdate, event.time);
    normalized.delta = event.delta;
    normalized.hasDelta = true;
    normalized.scale = event.scale;
    normalized.angleDelta = event.angleDelta;
    return normalized;
}

NormalizedInputEvent normalizeInputEvent(const KWin::PointerPinchGestureEndEvent &event)
{
    return baseEvent(NormalizedInputEventKind::PointerPinchEnd, event.time);
}

NormalizedInputEvent normalizeInputEvent(const KWin::PointerPinchGestureCancelEvent &event)
{
    auto normalized = baseEvent(NormalizedInputEventKind::PointerPinchCancel, event.time);
    normalized.state = NormalizedInputState::Cancelled;
    return normalized;
}

NormalizedInputEvent normalizeInputEvent(const KWin::PointerHoldGestureBeginEvent &event)
{
    auto normalized = baseEvent(NormalizedInputEventKind::PointerHoldBegin, event.time);
    normalized.fingerCount = event.fingerCount;
    return normalized;
}

NormalizedInputEvent normalizeInputEvent(const KWin::PointerHoldGestureEndEvent &event)
{
    return baseEvent(NormalizedInputEventKind::PointerHoldEnd, event.time);
}

NormalizedInputEvent normalizeInputEvent(const KWin::PointerHoldGestureCancelEvent &event)
{
    auto normalized = baseEvent(NormalizedInputEventKind::PointerHoldCancel, event.time);
    normalized.state = NormalizedInputState::Cancelled;
    return normalized;
}

NormalizedInputEvent normalizeInputEvent(const KWin::KeyboardKeyEvent &event,
                                         QString deviceId)
{
    auto normalized = baseEvent(NormalizedInputEventKind::KeyboardKey, event.timestamp,
                                std::move(deviceId));
    // AGENT-GUARD: Spies run before lock-screen and shortcut filters. Preserve
    // only the logical key/state needed by internal gestures; never copy text,
    // native scan codes, or serials into this longer-lived value.
    normalized.state = keyState(event.state);
    normalized.logicalCode = static_cast<quint32>(event.key);
    normalized.modifiers = event.modifiers;
    normalized.shortcutModifiers = event.modifiersRelevantForGlobalShortcuts;
    return normalized;
}

NormalizedInputEvent normalizeInputEvent(const KWin::TouchDownEvent &event)
{
    auto normalized = baseEvent(NormalizedInputEventKind::TouchDown, event.time);
    normalized.state = NormalizedInputState::Pressed;
    normalized.contactId = event.id;
    normalized.position = event.pos;
    normalized.hasPosition = true;
    return normalized;
}

NormalizedInputEvent normalizeInputEvent(const KWin::TouchMotionEvent &event)
{
    auto normalized = baseEvent(NormalizedInputEventKind::TouchMotion, event.time);
    normalized.contactId = event.id;
    normalized.position = event.pos;
    normalized.hasPosition = true;
    return normalized;
}

NormalizedInputEvent normalizeInputEvent(const KWin::TouchUpEvent &event)
{
    auto normalized = baseEvent(NormalizedInputEventKind::TouchUp, event.time);
    normalized.state = NormalizedInputState::Released;
    normalized.contactId = event.id;
    return normalized;
}

NormalizedInputEvent normalizeInputEvent(const KWin::SwitchEvent &event,
                                         QString deviceId)
{
    auto normalized = baseEvent(NormalizedInputEventKind::SwitchToggle, event.timestamp,
                                std::move(deviceId));
    normalized.state = event.state == KWin::SwitchState::On ? NormalizedInputState::On
                                                             : NormalizedInputState::Off;
    return normalized;
}

NormalizedInputEvent normalizeInputEvent(const KWin::TabletToolProximityEvent &event,
                                         QString deviceId)
{
    auto normalized = baseEvent(NormalizedInputEventKind::TabletToolProximity,
                                event.timestamp, std::move(deviceId));
    normalized.state = event.type == KWin::TabletToolProximityEvent::EnterProximity
        ? NormalizedInputState::Entered
        : NormalizedInputState::Exited;
    addTabletAxes(normalized, event.position, 0.0, event.rotation, event.sliderPosition,
                  event.xTilt, event.yTilt, event.distance);
    addToolIdentity(normalized, event.tool);
    return normalized;
}

NormalizedInputEvent normalizeInputEvent(const KWin::TabletToolAxisEvent &event,
                                         QString deviceId)
{
    auto normalized = baseEvent(NormalizedInputEventKind::TabletToolAxis, event.timestamp,
                                std::move(deviceId));
    addTabletAxes(normalized, event.position, event.pressure, event.rotation,
                  event.sliderPosition, event.xTilt, event.yTilt, event.distance);
    normalized.buttons = event.buttons;
    addToolIdentity(normalized, event.tool);
    return normalized;
}

NormalizedInputEvent normalizeInputEvent(const KWin::TabletToolTipEvent &event,
                                         QString deviceId)
{
    auto normalized = baseEvent(NormalizedInputEventKind::TabletToolTip, event.timestamp,
                                std::move(deviceId));
    normalized.state = event.type == KWin::TabletToolTipEvent::Press
        ? NormalizedInputState::Pressed
        : NormalizedInputState::Released;
    addTabletAxes(normalized, event.position, event.pressure, event.rotation,
                  event.sliderPosition, event.xTilt, event.yTilt, event.distance);
    addToolIdentity(normalized, event.tool);
    return normalized;
}

NormalizedInputEvent normalizeInputEvent(const KWin::TabletToolButtonEvent &event,
                                         QString deviceId)
{
    auto normalized = baseEvent(NormalizedInputEventKind::TabletToolButton, event.time,
                                std::move(deviceId));
    normalized.state = event.pressed ? NormalizedInputState::Pressed
                                     : NormalizedInputState::Released;
    normalized.nativeCode = event.button;
    addToolIdentity(normalized, event.tool);
    return normalized;
}

NormalizedInputEvent normalizeInputEvent(const KWin::TabletPadButtonEvent &event,
                                         QString deviceId)
{
    auto normalized = baseEvent(NormalizedInputEventKind::TabletPadButton, event.time,
                                std::move(deviceId));
    normalized.state = event.pressed ? NormalizedInputState::Pressed
                                     : NormalizedInputState::Released;
    normalized.nativeCode = event.button;
    normalized.group = event.group;
    normalized.mode = event.mode;
    normalized.isModeSwitch = event.isModeSwitch;
    return normalized;
}

NormalizedInputEvent normalizeInputEvent(const KWin::TabletPadStripEvent &event,
                                         QString deviceId)
{
    auto normalized = baseEvent(NormalizedInputEventKind::TabletPadStrip, event.time,
                                std::move(deviceId));
    normalized.controlNumber = event.number;
    normalized.value = event.position;
    normalized.isFinger = event.isFinger;
    normalized.group = event.group;
    normalized.mode = event.mode;
    return normalized;
}

NormalizedInputEvent normalizeInputEvent(const KWin::TabletPadRingEvent &event,
                                         QString deviceId)
{
    auto normalized = baseEvent(NormalizedInputEventKind::TabletPadRing, event.time,
                                std::move(deviceId));
    normalized.controlNumber = event.number;
    normalized.value = event.position;
    normalized.isFinger = event.isFinger;
    normalized.group = event.group;
    normalized.mode = event.mode;
    return normalized;
}

NormalizedInputEvent normalizeInputEvent(const KWin::TabletPadDialEvent &event,
                                         QString deviceId)
{
    auto normalized = baseEvent(NormalizedInputEventKind::TabletPadDial, event.time,
                                std::move(deviceId));
    normalized.controlNumber = event.number;
    normalized.value = event.delta;
    normalized.group = event.group;
    return normalized;
}

} // namespace QindaQt::Compositor::KWinIntegration
