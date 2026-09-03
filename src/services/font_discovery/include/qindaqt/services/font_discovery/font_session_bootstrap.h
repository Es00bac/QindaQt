// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/services/font_preferences/font_preferences.h"

#include <QString>

#include <optional>

class QDBusConnection;

namespace QindaQt::Services::FontDiscovery {

using QindaQt::Services::FontPreferences::FontPreferences;

// AGENT-CONTRACT: Bounds the synchronous pre-application Settings1 exchange
// (activation, initial owner lookup, snapshot read, and owner
// reauthentication) so a missing or slow preference source can delay
// application startup by at most this many milliseconds.
inline constexpr int DefaultBootstrapTimeoutMilliseconds = 750;

// AGENT-CONTRACT: FontSessionBootstrap is the Font F1 production composition
// root (review findings P1-1 and P1-6 of rejected candidate abc76f3). Each
// first-party application calls applyFromSessionSettings() exactly once,
// BEFORE QGuiApplication construction. Two Qt 6.11 behaviors make this safe:
// QGuiApplication::setFont() invoked pre-construction persists as the
// application default font, and blocking D-Bus calls work without an
// application object while QEventLoop does not. The read is therefore a
// bounded blocking exchange on a private connectToBus() connection -- the
// shared sessionBus() must never be created pre-application, or later
// in-process consumers of it would lose event-dispatcher integration.
//
// Fail-closed on every path: an unset or absent session bus (never
// autolaunched), an absent or slow Settings1 service, a malformed or
// wrong-typed snapshot, owner loss/replacement in flight, unavailable
// discovery, or a confirmed family that does not resolve in the live catalog
// all change nothing and return false with a bounded diagnostic.
class FontSessionBootstrap final {
public:
    FontSessionBootstrap() = delete;

    // Production composition: one guarded call per first-party main(). Reads
    // the confirmed fonts.* snapshot (exact-typed, wholesale rejection), runs
    // discovery with FontDiscoveryRequest::productionDefault() -- the only
    // request shape allowed to resolve the default fontconfig configuration --
    // and applies family, point size, hinting, and antialiasing to the
    // application default font only when the live catalog resolves the
    // confirmed family. The result is intentionally ignorable: every failure
    // path is fail-closed and reported only through the diagnostic.
    static bool applyFromSessionSettings(QString *diagnostic = nullptr);

    // Test seam: bounded blocking read of the confirmed fonts.* snapshot over
    // an injected, caller-owned connection. Never runs an event loop and
    // never blocks longer than timeoutMilliseconds.
    [[nodiscard]] static std::optional<FontPreferences> readConfirmedPreferences(
        const QDBusConnection &connection,
        int timeoutMilliseconds = DefaultBootstrapTimeoutMilliseconds,
        QString *diagnostic = nullptr);
};

} // namespace QindaQt::Services::FontDiscovery
