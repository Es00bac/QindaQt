// SPDX-License-Identifier: LGPL-3.0-or-later
#include "legacy_input_p.h"
#include <QSocketNotifier>
#include <libei.h>
#include <unistd.h>

namespace QindaQt::Services::Portal::RemoteInput {
namespace {
constexpr qsizetype kPendingLimit = 512;
// Portal discrete axis steps are whole detents; libei counts 120ths.
constexpr qint32 kDetent = 120;
}
LegacyInput::~LegacyInput() {
    for (auto *touch : std::as_const(m_touches)) ei_touch_unref(touch);
    for (auto *device : std::as_const(m_devices)) ei_device_unref(device);
    if (m_ei) ei_unref(m_ei);
}
bool LegacyInput::attach(int fd) {
    if (m_ei || fd < 0) { if (fd >= 0) ::close(fd); return false; }
    m_ei = ei_new_sender(nullptr);
    if (!m_ei) { ::close(fd); return false; }
    ei_configure_name(m_ei, "QindaQt RemoteDesktop legacy input");
    if (ei_setup_backend_fd(m_ei, fd) != 0) { ei_unref(m_ei); m_ei = nullptr; return false; }
    m_notifier = new QSocketNotifier(ei_get_fd(m_ei), QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, &LegacyInput::dispatch);
    dispatch();
    return m_ei != nullptr;
}
void LegacyInput::dispatch() {
    if (!m_ei) return;
    ei_dispatch(m_ei);
    bool disconnected = false;
    while (auto *event = ei_get_event(m_ei)) {
        auto *eventDevice = ei_event_get_device(event);
        switch (ei_event_get_type(event)) {
        case EI_EVENT_SEAT_ADDED:
            // The compositor offers only the consented capabilities.
            ei_seat_bind_capabilities(ei_event_get_seat(event), EI_DEVICE_CAP_POINTER, EI_DEVICE_CAP_POINTER_ABSOLUTE,
                EI_DEVICE_CAP_KEYBOARD, EI_DEVICE_CAP_TOUCH, EI_DEVICE_CAP_SCROLL, EI_DEVICE_CAP_BUTTON, nullptr);
            break;
        case EI_EVENT_DEVICE_RESUMED:
            if (!m_devices.contains(eventDevice)) {
                m_devices.append(ei_device_ref(eventDevice));
                ei_device_start_emulating(eventDevice, ++m_sequence);
            }
            break;
        case EI_EVENT_DEVICE_PAUSED:
        case EI_EVENT_DEVICE_REMOVED:
            forget(eventDevice);
            break;
        case EI_EVENT_DISCONNECT:
            disconnected = true;
            break;
        default:
            break;
        }
        ei_event_unref(event);
    }
    if (disconnected) {
        m_pending.clear();
        m_notifier->setEnabled(false);
        Q_EMIT lost();
        return;
    }
    flush();
}
void LegacyInput::forget(ei_device *device) {
    if (!m_devices.removeOne(device)) return;
    for (auto it = m_touches.begin(); it != m_touches.end();) {
        if (ei_touch_get_device(it.value()) == device) { ei_touch_unref(it.value()); it = m_touches.erase(it); }
        else ++it;
    }
    ei_device_unref(device);
}
ei_device *LegacyInput::device(Need need) const {
    for (auto *candidate : m_devices) {
        switch (need) {
        case Need::Motion: if (ei_device_has_capability(candidate, EI_DEVICE_CAP_POINTER)) return candidate; break;
        case Need::Absolute: if (ei_device_has_capability(candidate, EI_DEVICE_CAP_POINTER_ABSOLUTE)) return candidate; break;
        case Need::Button: if (ei_device_has_capability(candidate, EI_DEVICE_CAP_BUTTON)) return candidate; break;
        case Need::Scroll: if (ei_device_has_capability(candidate, EI_DEVICE_CAP_SCROLL)) return candidate; break;
        case Need::Keyboard: if (ei_device_has_capability(candidate, EI_DEVICE_CAP_KEYBOARD)) return candidate; break;
        case Need::Touch: if (ei_device_has_capability(candidate, EI_DEVICE_CAP_TOUCH)) return candidate; break;
        }
    }
    return nullptr;
}
void LegacyInput::emitOrQueue(Need need, std::function<void(ei_device *)> event) {
    // AGENT-GUARD: preserve caller order; a later event never overtakes one
    // still waiting for its device, and the FIFO is bounded.
    if (m_pending.size() >= kPendingLimit) return;
    m_pending.append({need, std::move(event)});
    flush();
}
void LegacyInput::flush() {
    while (m_ei && !m_pending.isEmpty()) {
        auto *target = device(m_pending.front().first);
        if (!target) return;
        const auto event = m_pending.takeFirst().second;
        event(target);
        ei_device_frame(target, ei_now(m_ei));
    }
}
void LegacyInput::motion(double dx, double dy) {
    emitOrQueue(Need::Motion, [dx, dy](ei_device *target) { ei_device_pointer_motion(target, dx, dy); });
}
void LegacyInput::absolute(double x, double y) {
    emitOrQueue(Need::Absolute, [x, y](ei_device *target) { ei_device_pointer_motion_absolute(target, x, y); });
}
void LegacyInput::button(quint32 code, bool pressed) {
    emitOrQueue(Need::Button, [code, pressed](ei_device *target) { ei_device_button_button(target, code, pressed); });
}
void LegacyInput::axis(double dx, double dy, bool finish) {
    emitOrQueue(Need::Scroll, [dx, dy, finish](ei_device *target) {
        if (finish) ei_device_scroll_stop(target, true, true);
        else ei_device_scroll_delta(target, dx, dy);
    });
}
void LegacyInput::discrete(quint32 axis, qint32 steps) {
    // Portal axis 0 is vertical, 1 horizontal.
    emitOrQueue(Need::Scroll, [axis, steps](ei_device *target) {
        ei_device_scroll_discrete(target, axis == 1 ? steps * kDetent : 0, axis == 0 ? steps * kDetent : 0);
    });
}
void LegacyInput::key(quint32 code, bool pressed) {
    emitOrQueue(Need::Keyboard, [code, pressed](ei_device *target) { ei_device_keyboard_key(target, code, pressed); });
}
void LegacyInput::touchDown(quint32 slot, double x, double y) {
    emitOrQueue(Need::Touch, [this, slot, x, y](ei_device *target) {
        if (m_touches.contains(slot)) return;
        auto *touch = ei_device_touch_new(target);
        ei_touch_down(touch, x, y);
        m_touches.insert(slot, touch);
    });
}
void LegacyInput::touchMotion(quint32 slot, double x, double y) {
    emitOrQueue(Need::Touch, [this, slot, x, y](ei_device *) {
        if (auto *touch = m_touches.value(slot)) ei_touch_motion(touch, x, y);
    });
}
void LegacyInput::touchUp(quint32 slot) {
    emitOrQueue(Need::Touch, [this, slot](ei_device *) {
        if (auto *touch = m_touches.take(slot)) { ei_touch_up(touch); ei_touch_unref(touch); }
    });
}
} // namespace QindaQt::Services::Portal::RemoteInput
