// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/font_preferences/font_preferences_codec.h"

#include <QJsonValue>

namespace QindaQt::Services::FontPreferences {

namespace {

constexpr auto KeyFamily = "family";
constexpr auto KeyMonospaceFamily = "monospaceFamily";
constexpr auto KeyPointSize = "pointSize";
constexpr auto KeyAntialiasing = "antialiasing";
constexpr auto KeyHinting = "hinting";
constexpr auto KeySubpixelOrder = "subpixelOrder";
constexpr auto KeyLogicalDpi = "logicalDpi";

constexpr auto SettingKeyFamily = "fonts.family";
constexpr auto SettingKeyMonospaceFamily = "fonts.monospaceFamily";
constexpr auto SettingKeyPointSize = "fonts.pointSize";
constexpr auto SettingKeyAntialiasing = "fonts.antialiasing";
constexpr auto SettingKeyHinting = "fonts.hinting";
constexpr auto SettingKeySubpixelOrder = "fonts.subpixelOrder";

} // namespace

QJsonObject FontPreferencesCodec::toJsonObject(const FontPreferences &prefs)
{
    QJsonObject obj;
    obj.insert(QLatin1String(KeyFamily), prefs.family());
    obj.insert(QLatin1String(KeyMonospaceFamily), prefs.monospaceFamily());
    obj.insert(QLatin1String(KeyPointSize), prefs.pointSize());
    obj.insert(QLatin1String(KeyAntialiasing), fontAntialiasingToString(prefs.antialiasing()));
    obj.insert(QLatin1String(KeyHinting), fontHintingToString(prefs.hinting()));
    obj.insert(QLatin1String(KeySubpixelOrder), fontSubpixelOrderToString(prefs.subpixelOrder()));
    if (prefs.logicalDpi().has_value()) {
        obj.insert(QLatin1String(KeyLogicalDpi), *prefs.logicalDpi());
    }
    return obj;
}

std::optional<FontPreferences> FontPreferencesCodec::fromJsonObject(
    const QJsonObject &json,
    QString *error)
{
    FontPreferences prefs = FontPreferences::systemDefaults();

    if (json.contains(QLatin1String(KeyFamily))) {
        const auto val = json.value(QLatin1String(KeyFamily));
        if (!val.isString()) {
            if (error) *error = QStringLiteral("family must be a string");
            return std::nullopt;
        }
        const QString str = val.toString();
        if (!isValidFamilyName(str)) {
            if (error) *error = QStringLiteral("family contains invalid or unprintable characters");
            return std::nullopt;
        }
        prefs.setFamily(str);
    }

    if (json.contains(QLatin1String(KeyMonospaceFamily))) {
        const auto val = json.value(QLatin1String(KeyMonospaceFamily));
        if (!val.isString()) {
            if (error) *error = QStringLiteral("monospaceFamily must be a string");
            return std::nullopt;
        }
        const QString str = val.toString();
        if (!isValidFamilyName(str)) {
            if (error) *error = QStringLiteral("monospaceFamily contains invalid or unprintable characters");
            return std::nullopt;
        }
        prefs.setMonospaceFamily(str);
    }

    if (json.contains(QLatin1String(KeyPointSize))) {
        const auto val = json.value(QLatin1String(KeyPointSize));
        if (!val.isDouble()) {
            if (error) *error = QStringLiteral("pointSize must be a number");
            return std::nullopt;
        }
        const double pt = val.toDouble();
        if (!isValidPointSize(pt)) {
            if (error) *error = QStringLiteral("pointSize is out of valid bounds [6.0, 36.0]");
            return std::nullopt;
        }
        prefs.setPointSize(pt);
    }

    if (json.contains(QLatin1String(KeyAntialiasing))) {
        const auto val = json.value(QLatin1String(KeyAntialiasing));
        if (val.isBool()) {
            prefs.setAntialiasing(fontAntialiasingFromBool(val.toBool()));
        } else if (val.isString()) {
            const auto mode = fontAntialiasingFromString(val.toString());
            if (!mode) {
                if (error) *error = QStringLiteral("invalid antialiasing value: '%1'").arg(val.toString());
                return std::nullopt;
            }
            prefs.setAntialiasing(*mode);
        } else {
            if (error) *error = QStringLiteral("antialiasing must be boolean or string");
            return std::nullopt;
        }
    }

    if (json.contains(QLatin1String(KeyHinting))) {
        const auto val = json.value(QLatin1String(KeyHinting));
        if (!val.isString()) {
            if (error) *error = QStringLiteral("hinting must be a string");
            return std::nullopt;
        }
        const auto hint = fontHintingFromString(val.toString());
        if (!hint) {
            if (error) *error = QStringLiteral("invalid hinting value: '%1'").arg(val.toString());
            return std::nullopt;
        }
        prefs.setHinting(*hint);
    }

    if (json.contains(QLatin1String(KeySubpixelOrder))) {
        const auto val = json.value(QLatin1String(KeySubpixelOrder));
        if (!val.isString()) {
            if (error) *error = QStringLiteral("subpixelOrder must be a string");
            return std::nullopt;
        }
        const auto order = fontSubpixelOrderFromString(val.toString());
        if (!order) {
            if (error) *error = QStringLiteral("invalid subpixelOrder value: '%1'").arg(val.toString());
            return std::nullopt;
        }
        prefs.setSubpixelOrder(*order);
    }

    if (json.contains(QLatin1String(KeyLogicalDpi))) {
        const auto val = json.value(QLatin1String(KeyLogicalDpi));
        if (!val.isNull()) {
            if (!val.isDouble()) {
                if (error) *error = QStringLiteral("logicalDpi must be a number");
                return std::nullopt;
            }
            const double dpi = val.toDouble();
            if (!isValidLogicalDpi(dpi)) {
                if (error) *error = QStringLiteral("logicalDpi is out of valid bounds [48.0, 576.0]");
                return std::nullopt;
            }
            prefs.setLogicalDpi(dpi);
        }
    }

    return prefs;
}

QVariantMap FontPreferencesCodec::toVariantMap(const FontPreferences &prefs)
{
    QVariantMap map;
    map.insert(QLatin1String(KeyFamily), prefs.family());
    map.insert(QLatin1String(KeyMonospaceFamily), prefs.monospaceFamily());
    map.insert(QLatin1String(KeyPointSize), prefs.pointSize());
    map.insert(QLatin1String(KeyAntialiasing), fontAntialiasingToString(prefs.antialiasing()));
    map.insert(QLatin1String(KeyHinting), fontHintingToString(prefs.hinting()));
    map.insert(QLatin1String(KeySubpixelOrder), fontSubpixelOrderToString(prefs.subpixelOrder()));
    if (prefs.logicalDpi().has_value()) {
        map.insert(QLatin1String(KeyLogicalDpi), *prefs.logicalDpi());
    }
    return map;
}

std::optional<FontPreferences> FontPreferencesCodec::fromVariantMap(
    const QVariantMap &map,
    QString *error)
{
    FontPreferences prefs = FontPreferences::systemDefaults();

    if (map.contains(QLatin1String(KeyFamily))) {
        const QString str = map.value(QLatin1String(KeyFamily)).toString();
        if (!isValidFamilyName(str)) {
            if (error) *error = QStringLiteral("family contains invalid or unprintable characters");
            return std::nullopt;
        }
        prefs.setFamily(str);
    }

    if (map.contains(QLatin1String(KeyMonospaceFamily))) {
        const QString str = map.value(QLatin1String(KeyMonospaceFamily)).toString();
        if (!isValidFamilyName(str)) {
            if (error) *error = QStringLiteral("monospaceFamily contains invalid or unprintable characters");
            return std::nullopt;
        }
        prefs.setMonospaceFamily(str);
    }

    if (map.contains(QLatin1String(KeyPointSize))) {
        bool ok = false;
        const double pt = map.value(QLatin1String(KeyPointSize)).toDouble(&ok);
        if (!ok || !isValidPointSize(pt)) {
            if (error) *error = QStringLiteral("pointSize is invalid or out of valid bounds");
            return std::nullopt;
        }
        prefs.setPointSize(pt);
    }

    if (map.contains(QLatin1String(KeyAntialiasing))) {
        const QVariant val = map.value(QLatin1String(KeyAntialiasing));
        if (val.typeId() == QMetaType::Bool) {
            prefs.setAntialiasing(fontAntialiasingFromBool(val.toBool()));
        } else {
            const auto mode = fontAntialiasingFromString(val.toString());
            if (!mode) {
                if (error) *error = QStringLiteral("invalid antialiasing value: '%1'").arg(val.toString());
                return std::nullopt;
            }
            prefs.setAntialiasing(*mode);
        }
    }

    if (map.contains(QLatin1String(KeyHinting))) {
        const auto hint = fontHintingFromString(map.value(QLatin1String(KeyHinting)).toString());
        if (!hint) {
            if (error) *error = QStringLiteral("invalid hinting value");
            return std::nullopt;
        }
        prefs.setHinting(*hint);
    }

    if (map.contains(QLatin1String(KeySubpixelOrder))) {
        const auto order = fontSubpixelOrderFromString(map.value(QLatin1String(KeySubpixelOrder)).toString());
        if (!order) {
            if (error) *error = QStringLiteral("invalid subpixelOrder value");
            return std::nullopt;
        }
        prefs.setSubpixelOrder(*order);
    }

    if (map.contains(QLatin1String(KeyLogicalDpi))) {
        const QVariant dpiVar = map.value(QLatin1String(KeyLogicalDpi));
        if (!dpiVar.isNull() && dpiVar.isValid()) {
            bool ok = false;
            const double dpi = dpiVar.toDouble(&ok);
            if (!ok || !isValidLogicalDpi(dpi)) {
                if (error) *error = QStringLiteral("invalid logicalDpi value");
                return std::nullopt;
            }
            prefs.setLogicalDpi(dpi);
        }
    }

    return prefs;
}

QVariantMap FontPreferencesCodec::toSettingsMap(const FontPreferences &prefs)
{
    QVariantMap settings;
    settings.insert(QLatin1String(SettingKeyFamily), prefs.family());
    settings.insert(QLatin1String(SettingKeyMonospaceFamily), prefs.monospaceFamily());
    settings.insert(QLatin1String(SettingKeyPointSize), prefs.pointSize());
    settings.insert(QLatin1String(SettingKeyAntialiasing), fontAntialiasingToBool(prefs.antialiasing()));
    settings.insert(QLatin1String(SettingKeyHinting), fontHintingToString(prefs.hinting()));
    settings.insert(QLatin1String(SettingKeySubpixelOrder), fontSubpixelOrderToString(prefs.subpixelOrder()));
    return settings;
}

std::optional<FontPreferences> FontPreferencesCodec::fromSettingsMap(
    const QVariantMap &settings,
    QString *error)
{
    FontPreferences prefs = FontPreferences::systemDefaults();

    if (settings.contains(QLatin1String(SettingKeyFamily))) {
        const QString str = settings.value(QLatin1String(SettingKeyFamily)).toString();
        if (!isValidFamilyName(str)) {
            if (error) *error = QStringLiteral("fonts.family is invalid");
            return std::nullopt;
        }
        prefs.setFamily(str);
    }

    if (settings.contains(QLatin1String(SettingKeyMonospaceFamily))) {
        const QString str = settings.value(QLatin1String(SettingKeyMonospaceFamily)).toString();
        if (!isValidFamilyName(str)) {
            if (error) *error = QStringLiteral("fonts.monospaceFamily is invalid");
            return std::nullopt;
        }
        prefs.setMonospaceFamily(str);
    }

    if (settings.contains(QLatin1String(SettingKeyPointSize))) {
        bool ok = false;
        const double pt = settings.value(QLatin1String(SettingKeyPointSize)).toDouble(&ok);
        if (!ok || !isValidPointSize(pt)) {
            if (error) *error = QStringLiteral("fonts.pointSize is out of valid bounds");
            return std::nullopt;
        }
        prefs.setPointSize(pt);
    }

    if (settings.contains(QLatin1String(SettingKeyAntialiasing))) {
        const QVariant aa = settings.value(QLatin1String(SettingKeyAntialiasing));
        if (aa.typeId() == QMetaType::Bool) {
            prefs.setAntialiasing(fontAntialiasingFromBool(aa.toBool()));
        } else {
            const auto mode = fontAntialiasingFromString(aa.toString());
            if (!mode) {
                if (error) *error = QStringLiteral("fonts.antialiasing is invalid");
                return std::nullopt;
            }
            prefs.setAntialiasing(*mode);
        }
    }

    if (settings.contains(QLatin1String(SettingKeyHinting))) {
        const auto hint = fontHintingFromString(settings.value(QLatin1String(SettingKeyHinting)).toString());
        if (!hint) {
            if (error) *error = QStringLiteral("fonts.hinting is invalid");
            return std::nullopt;
        }
        prefs.setHinting(*hint);
    }

    if (settings.contains(QLatin1String(SettingKeySubpixelOrder))) {
        const auto order = fontSubpixelOrderFromString(settings.value(QLatin1String(SettingKeySubpixelOrder)).toString());
        if (!order) {
            if (error) *error = QStringLiteral("fonts.subpixelOrder is invalid");
            return std::nullopt;
        }
        prefs.setSubpixelOrder(*order);
    }

    return prefs;
}

} // namespace QindaQt::Services::FontPreferences
