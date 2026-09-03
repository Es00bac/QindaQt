// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/services/font_preferences/font_fact.h"
#include "qindaqt/services/font_preferences/font_preferences.h"

#include <QList>
#include <QString>

namespace QindaQt::Services::FontPreferences {

// AGENT-CONTRACT: FontSettingsBootstrap is the pure half of the F1 pre-window
// bootstrap: validating confirmed preferences, applying them to Qt's default
// font, and gating application on the live-discovered catalog. It carries no
// transport dependency (the F0 boundary gate forbids Qt D-Bus in this module);
// FontDiscovery::FontSessionBootstrap is the production composition root that
// calls these helpers. applyPreferences() is safe before QGuiApplication
// construction: QGuiApplication::setFont() invoked pre-construction persists
// as the application default font (pinned by qindaqt.font-session-bootstrap).
class FontSettingsBootstrap final {
public:
    FontSettingsBootstrap() = delete;

    // Applies validated preferences to the application default font (family,
    // point size, hinting, and antialiasing strategy). Returns false and
    // changes nothing for invalid preferences.
    [[nodiscard]] static bool applyPreferences(const FontPreferences &preferences,
                                               QString *diagnostic = nullptr);

    // Case-insensitive gate: the confirmed family must resolve in the
    // live-discovered facts, otherwise the preference is left unapplied
    // (fail-closed). An empty fact list never resolves.
    [[nodiscard]] static bool confirmedFamilyResolves(const FontPreferences &preferences,
                                                      const QList<FontFact> &discoveredFacts) noexcept;
};

} // namespace QindaQt::Services::FontPreferences
