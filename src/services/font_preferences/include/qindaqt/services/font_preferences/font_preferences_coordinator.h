// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/services/font_preferences/font_catalog.h"
#include "qindaqt/services/font_preferences/font_preferences.h"

#include <QList>
#include <QString>
#include <QVariantMap>

namespace QindaQt::Services::FontPreferences {

// AGENT-CONTRACT: FontPreferencesCoordinator coordinates atomic publication
// of font preferences and catalog discovery snapshots. Failed refresh or invalid
// preferences preserve the Last-Known-Good (LKG) state.
class FontPreferencesCoordinator final {
public:
    explicit FontPreferencesCoordinator(
        FontPreferences initialPreferences = FontPreferences::systemDefaults(),
        FontCatalog initialCatalog = FontCatalog::createDefaultFallback());

    [[nodiscard]] qint64 revision() const noexcept { return m_revision; }

    [[nodiscard]] const FontPreferences &preferences() const noexcept { return m_preferences; }
    [[nodiscard]] const FontCatalog &catalog() const noexcept { return m_catalog; }

    [[nodiscard]] const FontPreferences &lastKnownGoodPreferences() const noexcept { return m_lastKnownGoodPreferences; }
    [[nodiscard]] const FontCatalog &lastKnownGoodCatalog() const noexcept { return m_lastKnownGoodCatalog; }

    // AGENT-GUARD: If refresh fails due to invalid facts, the catalog is untouched,
    // revision is not advanced, and the last known good catalog is retained.
    bool refreshCatalog(
        const QList<FontFact> &facts,
        QString *errorMessage = nullptr);

    // AGENT-GUARD: If preference update fails validation, the current preferences
    // remain unchanged, revision is not advanced, and LKG is preserved.
    bool updatePreferences(
        const FontPreferences &newPreferences,
        QString *errorMessage = nullptr);

    bool updateFromSettings(
        const QVariantMap &settingsMap,
        QString *errorMessage = nullptr);

    void resetToDefaults();

private:
    qint64 m_revision = 1;
    FontPreferences m_preferences;
    FontCatalog m_catalog;
    FontPreferences m_lastKnownGoodPreferences;
    FontCatalog m_lastKnownGoodCatalog;
};

} // namespace QindaQt::Services::FontPreferences
