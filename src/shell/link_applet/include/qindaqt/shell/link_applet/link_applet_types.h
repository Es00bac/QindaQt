// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtCore/QString>

namespace QindaQt::Shell::LinkApplet {

// The five presentation states (docs/wiki/shell/link-applet.md), closed by
// design: every input combination lands in exactly one of them.
enum class AppletState {
    Unavailable,    // service absent, or phone.read denied
    NoPhone,        // service present, nothing paired
    PairingRequest, // a six-digit code is waiting for the user
    Connected,      // paired phone, live and READY
    Offline,        // paired phone, not connected
};

[[nodiscard]] QString toString(AppletState state);

// A pairing request as the user sees it. AGENT-GUARD: name/model are
// attacker-influenced phone strings; they are sanitized here and QML renders
// them with Text.PlainText only.
struct PairingPresentation final {
    QString deviceId;
    QString name;
    QString model;
    QString code; // "123 456"

    friend bool operator==(const PairingPresentation &, const PairingPresentation &) = default;
};

struct LinkAppletModel final {
    AppletState state = AppletState::Unavailable;
    QString stateText;     // tooltip / accessible name
    QString iconColorRole; // "muted" | "success" | "warning" | "danger"
    QString deviceId;      // the primary paired device, if any
    QString deviceName;
    QString connectionText; // "USB" | "Wi-Fi" | ""
    int batteryPercent = -1;
    QString batteryText;    // "87%" | ""
    bool charging = false;
    QString networkText;    // "Wi-Fi, signal 3 of 4" | ""
    QString lastSeenText;   // "Last seen 5 min ago" | ""
    bool canRing = false;
    // SMS lane feeds this later; 0 hides the badge.
    int unreadCount = 0;
    bool hasPairing = false;
    PairingPresentation pairing;

    friend bool operator==(const LinkAppletModel &, const LinkAppletModel &) = default;
};

} // namespace QindaQt::Shell::LinkApplet
