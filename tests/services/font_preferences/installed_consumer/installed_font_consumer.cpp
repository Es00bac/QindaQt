// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/font_preferences/font_bootstrap.h>
#include <qindaqt/services/font_preferences/font_catalog.h>
#include <qindaqt/services/font_preferences/font_preferences.h>
#include <qindaqt/services/font_preferences/font_preferences_codec.h>
#include <qindaqt/services/font_preferences/font_preferences_coordinator.h>
#include <qindaqt/services/font_preferences/font_types.h>
#include <qindaqt/services/font_preferences/font_validation.h>

#include <iostream>

int main()
{
    using namespace QindaQt::Services::FontPreferences;

    FontPreferences prefs = FontPreferences::systemDefaults();
    if (!prefs.isValid()) {
        std::cerr << "Installed consumer: default preferences are invalid\n";
        return 1;
    }

    const QFont appFont = FontBootstrap::createApplicationFont(prefs);
    if (appFont.family() != QLatin1String("Noto Sans")) {
        std::cerr << "Installed consumer: unexpected default font family\n";
        return 2;
    }

    FontPreferencesCoordinator coordinator;
    if (coordinator.revision() != 1) {
        std::cerr << "Installed consumer: unexpected initial revision\n";
        return 3;
    }

    const QVariantMap settingsMap = FontPreferencesCodec::toSettingsMap(prefs);
    if (!settingsMap.contains(QStringLiteral("fonts.family"))) {
        std::cerr << "Installed consumer: settings map missing fonts.family\n";
        return 4;
    }

    std::cout << "Installed FontPreferences consumer verified successfully\n";
    return 0;
}
