// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QList>
#include <functional>
#include <utility>
#include <libei.h>
// libei peer driven from QTRY expressions; never touches host devices.
struct Ei {
    Ei(bool sender, int fd) : context(sender ? ei_new_sender(nullptr) : ei_new_receiver(nullptr)) {
        ei_configure_name(context, "qindaqt-native-remote-input");
        ok = ei_setup_backend_fd(context, fd) == 0;
    }
    ~Ei() {
        for (auto *device : std::as_const(devices)) ei_device_unref(device);
        ei_unref(context);
    }
    bool pump() {
        ei_dispatch(context);
        while (auto *event = ei_get_event(context)) {
            const auto type = ei_event_get_type(event);
            seen << type;
            if (type == EI_EVENT_SEAT_ADDED)
                ei_seat_bind_capabilities(ei_event_get_seat(event), EI_DEVICE_CAP_POINTER, EI_DEVICE_CAP_POINTER_ABSOLUTE,
                                          EI_DEVICE_CAP_KEYBOARD, EI_DEVICE_CAP_BUTTON, EI_DEVICE_CAP_SCROLL, nullptr);
            else if (type == EI_EVENT_DEVICE_ADDED) devices << ei_device_ref(ei_event_get_device(event));
            else if (type == EI_EVENT_DEVICE_RESUMED) resumed << ei_event_get_device(event);
            else if (type == EI_EVENT_KEYBOARD_KEY && ei_event_keyboard_get_key_is_press(event)) keys << ei_event_keyboard_get_key(event);
            ei_event_unref(event);
        }
        return true;
    }
    ei_device *device(ei_device_capability capability) const {
        for (auto *candidate : devices)
            if (resumed.contains(candidate) && ei_device_has_capability(candidate, capability)) return candidate;
        return nullptr;
    }
    void emulate(ei_device *device, const std::function<void()> &events) {
        if (!started.contains(device)) { ei_device_start_emulating(device, ++sequence); started << device; }
        events();
        ei_device_frame(device, ei_now(context));
    }
    bool disconnected() { pump(); return seen.contains(EI_EVENT_DISCONNECT); }
    ei *context;
    bool ok = false;
    QList<ei_event_type> seen;
    QList<ei_device *> devices, resumed, started;
    QList<uint32_t> keys;
    uint32_t sequence = 0;
};
