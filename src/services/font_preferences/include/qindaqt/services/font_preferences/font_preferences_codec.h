// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/services/font_preferences/font_preferences.h"

#include <QJsonObject>
#include <QVariantMap>

#include <optional>

namespace QindaQt::Services::FontPreferences {

// AGENT-CONTRACT: FontPreferencesCodec provides lossless conversion between
// FontPreferences and JSON / VariantMap structures, including the Settings1 schema keys.
class FontPreferencesCodec final {
public:
    FontPreferencesCodec() = delete;

    [[nodiscard]] static QJsonObject toJsonObject(const FontPreferences &prefs);
    [[nodiscard]] static std::optional<FontPreferences> fromJsonObject(
        const QJsonObject &json,
        QString *error = nullptr);

    [[nodiscard]] static QVariantMap toVariantMap(const FontPreferences &prefs);
    [[nodiscard]] static std::optional<FontPreferences> fromVariantMap(
        const QVariantMap &map,
        QString *error = nullptr);

    // AGENT-CONTRACT: Settings keys match the schema-v2 "fonts.*" keys:
    // "fonts.family", "fonts.monospaceFamily", "fonts.pointSize",
    // "fonts.antialiasing", "fonts.hinting", "fonts.subpixelOrder".
    [[nodiscard]] static QVariantMap toSettingsMap(const FontPreferences &prefs);
    [[nodiscard]] static std::optional<FontPreferences> fromSettingsMap(
        const QVariantMap &settings,
        QString *error = nullptr);
};

} // namespace QindaQt::Services::FontPreferences
