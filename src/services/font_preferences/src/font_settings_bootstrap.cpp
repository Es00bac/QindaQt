// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/font_preferences/font_settings_bootstrap.h"

#include "qindaqt/services/font_preferences/font_bootstrap.h"

#include <QGuiApplication>

namespace QindaQt::Services::FontPreferences {

bool FontSettingsBootstrap::applyPreferences(const FontPreferences &preferences,
                                             QString *diagnostic)
{
    if (!preferences.isValid()) {
        if (diagnostic != nullptr) {
            *diagnostic = QStringLiteral("font preferences failed validation");
        }
        return false;
    }
    QGuiApplication::setFont(FontBootstrap::createApplicationFont(preferences));
    return true;
}

bool FontSettingsBootstrap::confirmedFamilyResolves(const FontPreferences &preferences,
                                                    const QList<FontFact> &discoveredFacts) noexcept
{
    const QString wanted = preferences.family().trimmed();
    for (const FontFact &fact : discoveredFacts) {
        if (fact.family.compare(wanted, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

} // namespace QindaQt::Services::FontPreferences
