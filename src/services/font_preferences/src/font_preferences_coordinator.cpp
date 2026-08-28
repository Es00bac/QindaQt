// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/font_preferences/font_preferences_coordinator.h"
#include "qindaqt/services/font_preferences/font_preferences_codec.h"

namespace QindaQt::Services::FontPreferences {

FontPreferencesCoordinator::FontPreferencesCoordinator(
    FontPreferences initialPreferences,
    FontCatalog initialCatalog)
    : m_preferences(std::move(initialPreferences))
    , m_catalog(std::move(initialCatalog))
    , m_lastKnownGoodPreferences(m_preferences)
    , m_lastKnownGoodCatalog(m_catalog)
{
}

bool FontPreferencesCoordinator::refreshCatalog(
    const QList<FontFact> &facts,
    QString *errorMessage)
{
    QString localError;
    FontCatalog newCatalog = FontCatalog::create(facts, &localError);
    if (newCatalog.isEmpty()) {
        if (errorMessage) {
            *errorMessage = localError.isEmpty()
                                ? QStringLiteral("Failed to generate valid catalog from provided facts")
                                : localError;
        }
        // AGENT-GUARD: Retain last known good catalog on failure
        return false;
    }

    m_catalog = std::move(newCatalog);
    m_lastKnownGoodCatalog = m_catalog;
    ++m_revision;
    return true;
}

bool FontPreferencesCoordinator::updatePreferences(
    const FontPreferences &newPreferences,
    QString *errorMessage)
{
    if (!newPreferences.isValid()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Proposed font preferences failed validation");
        }
        return false;
    }

    if (m_preferences == newPreferences) {
        return true;
    }

    m_preferences = newPreferences;
    m_lastKnownGoodPreferences = m_preferences;
    ++m_revision;
    return true;
}

bool FontPreferencesCoordinator::updateFromSettings(
    const QVariantMap &settingsMap,
    QString *errorMessage)
{
    QString codecError;
    const auto parsed = FontPreferencesCodec::fromSettingsMap(settingsMap, &codecError);
    if (!parsed) {
        if (errorMessage) {
            *errorMessage = codecError.isEmpty()
                                ? QStringLiteral("Failed to decode settings map into font preferences")
                                : codecError;
        }
        return false;
    }

    return updatePreferences(*parsed, errorMessage);
}

void FontPreferencesCoordinator::resetToDefaults()
{
    m_preferences = FontPreferences::systemDefaults();
    m_catalog = FontCatalog::createDefaultFallback();
    m_lastKnownGoodPreferences = m_preferences;
    m_lastKnownGoodCatalog = m_catalog;
    ++m_revision;
}

} // namespace QindaQt::Services::FontPreferences
