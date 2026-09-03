// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/services/font_preferences/font_preferences.h"

#include <QString>

#include <optional>

class QGuiApplication;

namespace QindaQt::Services::SettingsClient {
class SettingsClient;
}

namespace QindaQt::Services::FontPreferences {

// AGENT-CONTRACT: Bounds the synchronous pre-window Settings1 read so a
// missing or slow settings service can delay application startup by at most
// this many milliseconds. See docs/wiki/architecture/font-preferences.md.
inline constexpr int DefaultBootstrapTimeoutMilliseconds = 750;

// AGENT-CONTRACT: FontSettingsBootstrap is the F1 pre-window helper derived
// from FontBootstrap. Each first-party application calls
// applyFromSessionSettings() once, immediately after constructing its
// QGuiApplication and before creating any window or QML engine (Qt D-Bus and
// QGuiApplication::setFont both require the application object, so a strictly
// pre-QGuiApplication call is impossible). Every path is guarded: a missing,
// unavailable, slow, or invalid preference source changes nothing and returns
// false with a bounded diagnostic.
class FontSettingsBootstrap final {
public:
    FontSettingsBootstrap() = delete;

    // Applies validated preferences to the application default font (family,
    // point size, hinting, and antialiasing strategy). Returns false and
    // changes nothing for invalid preferences.
    [[nodiscard]] static bool applyPreferences(QGuiApplication &application,
                                               const FontPreferences &preferences,
                                               QString *diagnostic = nullptr);

    // Bounded synchronous read of the confirmed fonts.* snapshot through an
    // injected, caller-owned client. Returns nullopt on any start, timeout, or
    // decode failure; never blocks longer than timeoutMilliseconds.
    [[nodiscard]] static std::optional<FontPreferences> readConfirmedPreferences(
        QindaQt::Services::SettingsClient::SettingsClient &client,
        int timeoutMilliseconds = DefaultBootstrapTimeoutMilliseconds,
        QString *diagnostic = nullptr);

    // Production composition: constructs a session-bus Settings1 transport and
    // scoped client internally, reads the confirmed snapshot, and applies it.
    [[nodiscard]] static bool applyFromSessionSettings(QGuiApplication &application,
                                                       QString *diagnostic = nullptr);
};

} // namespace QindaQt::Services::FontPreferences
