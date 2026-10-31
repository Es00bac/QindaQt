// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtCore/QString>

namespace QindaQt::Shell::FirewallApplet {

// The six presentation states (docs/wiki/shell/firewall-applet.md,
// ADR-0431), closed by design: every input combination from
// `projectFirewallApplet` lands in exactly one of these, never a seventh
// "unhandled" bucket.
enum class AppletState {
    Unavailable,
    Blocked,
    Secure,
    Watching,
    Attention,
    Alarm,
};

[[nodiscard]] QString toString(AppletState state);

// Presentation-ready copy of one SecurityMonitor alert's plain-English
// fields (QindaFirewall docs/monitor.md), plus the id `Dismiss`/
// `ConfirmMine`/`OpenWindow` key on. Title/summary/meaning/advice are shown
// exactly as QindaFirewall worded them; this layer adds no technical terms.
struct AlertPresentation final {
    QString id;
    QString title;
    QString summary; // "What happened?"
    QString meaning; // "Is this dangerous?"
    QString advice;  // "What should I do?"
    bool canConfirm = false;

    friend bool operator==(const AlertPresentation &, const AlertPresentation &) = default;
};

struct FirewallAppletModel final {
    AppletState state = AppletState::Unavailable;
    // Panel tooltip and popup state text, e.g. "All quiet. Nobody is
    // trying to get in." or the headline title for Attention/Alarm.
    QString stateText;
    // A closed theme-role vocabulary QML maps to QindaTK tokens
    // ("muted" | "success" | "warning" | "danger"); never a literal colour.
    QString iconColorRole;
    quint32 alertCount = 0;
    // AGENT-CONTRACT: true only when state == Alarm. QML renders a
    // persistent (non-dismissing-on-its-own) attention treatment exactly
    // while this is true, and never otherwise.
    bool persistentAttention = false;
    bool hasAlert = false;
    AlertPresentation alert;

    friend bool operator==(const FirewallAppletModel &, const FirewallAppletModel &) = default;
};

} // namespace QindaQt::Shell::FirewallApplet
