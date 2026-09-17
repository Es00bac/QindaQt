// SPDX-License-Identifier: LGPL-3.0-or-later

#include "qindaqt/services/wiz_protocol/wiz_types.h"

#include "qindaqt/services/wiz_protocol/wiz_limits.h"

namespace QindaQt::Wiz
{
namespace
{

// Marketing families keyed by the module-name token the firmware reports.
// AGENT-NOTE: the token is the middle field of names such as
// "ESP25_SHRGB_01"; anything unrecognized falls back to the neutral "Light"
// rather than inventing a product name.
[[nodiscard]] QString productFamily(const QString &moduleName)
{
    const QString upper = moduleName.toUpper();
    if (upper.contains(QLatin1String("SHRGB")) || upper.contains(QLatin1String("RGB"))) {
        return QStringLiteral("Colour light");
    }
    if (upper.contains(QLatin1String("SHTW")) || upper.contains(QLatin1String("TW"))) {
        return QStringLiteral("Tunable white light");
    }
    if (upper.contains(QLatin1String("SHDW")) || upper.contains(QLatin1String("DW"))) {
        return QStringLiteral("Dimmable light");
    }
    if (upper.contains(QLatin1String("SOCKET")) || upper.contains(QLatin1String("PLUG"))) {
        return QStringLiteral("Smart plug");
    }
    return QStringLiteral("Light");
}

} // namespace

LightMode PilotState::mode() const noexcept
{
    if (sceneKnown && sceneId != 0) {
        return LightMode::Scene;
    }
    if (temperatureKnown && temperatureKelvin != 0) {
        return LightMode::White;
    }
    if (colorKnown && (red != 0 || green != 0 || blue != 0)) {
        return LightMode::Color;
    }
    if (temperatureKnown || colorKnown) {
        // The device answered a colour lane but every channel is zero, which
        // firmware uses for "white channels only".
        return LightMode::White;
    }
    return LightMode::Unknown;
}

QString normalizeMac(const QString &value)
{
    QString compact;
    compact.reserve(12);
    for (const QChar character : value) {
        if (character == QLatin1Char(':') || character == QLatin1Char('-')) {
            continue;
        }
        const QChar lowered = character.toLower();
        const bool hexDigit = (lowered >= QLatin1Char('0') && lowered <= QLatin1Char('9'))
            || (lowered >= QLatin1Char('a') && lowered <= QLatin1Char('f'));
        if (!hexDigit) {
            return {};
        }
        if (compact.size() == 12) {
            return {};
        }
        compact.append(lowered);
    }
    return compact.size() == 12 ? compact : QString();
}

QString derivedLabel(const DeviceIdentity &identity)
{
    const QString family = productFamily(identity.moduleName);
    const QString mac = normalizeMac(identity.mac);
    if (mac.isEmpty()) {
        return family;
    }
    // The last three octets are what the sticker on the luminaire shows.
    const QString suffix = mac.right(6).toUpper();
    return QStringLiteral("%1 %2").arg(family, suffix);
}

} // namespace QindaQt::Wiz
