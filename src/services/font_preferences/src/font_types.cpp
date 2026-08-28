// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/font_preferences/font_types.h"

namespace QindaQt::Services::FontPreferences {

QString fontAntialiasingToString(FontAntialiasing mode)
{
    switch (mode) {
    case FontAntialiasing::None:
        return QStringLiteral("none");
    case FontAntialiasing::Grayscale:
        return QStringLiteral("grayscale");
    case FontAntialiasing::Subpixel:
        return QStringLiteral("subpixel");
    }
    return QStringLiteral("subpixel");
}

std::optional<FontAntialiasing> fontAntialiasingFromString(QStringView str)
{
    const auto trimmed = str.trimmed();
    if (trimmed.compare(QLatin1String("none"), Qt::CaseInsensitive) == 0 ||
        trimmed.compare(QLatin1String("false"), Qt::CaseInsensitive) == 0 ||
        trimmed.compare(QLatin1String("0"), Qt::CaseInsensitive) == 0) {
        return FontAntialiasing::None;
    }
    if (trimmed.compare(QLatin1String("grayscale"), Qt::CaseInsensitive) == 0 ||
        trimmed.compare(QLatin1String("gray"), Qt::CaseInsensitive) == 0) {
        return FontAntialiasing::Grayscale;
    }
    if (trimmed.compare(QLatin1String("subpixel"), Qt::CaseInsensitive) == 0 ||
        trimmed.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0 ||
        trimmed.compare(QLatin1String("1"), Qt::CaseInsensitive) == 0) {
        return FontAntialiasing::Subpixel;
    }
    return std::nullopt;
}

bool fontAntialiasingToBool(FontAntialiasing mode) noexcept
{
    return mode != FontAntialiasing::None;
}

FontAntialiasing fontAntialiasingFromBool(bool enabled) noexcept
{
    return enabled ? FontAntialiasing::Subpixel : FontAntialiasing::None;
}

QString fontHintingToString(FontHinting hinting)
{
    switch (hinting) {
    case FontHinting::None:
        return QStringLiteral("none");
    case FontHinting::Slight:
        return QStringLiteral("slight");
    case FontHinting::Medium:
        return QStringLiteral("medium");
    case FontHinting::Full:
        return QStringLiteral("full");
    }
    return QStringLiteral("slight");
}

std::optional<FontHinting> fontHintingFromString(QStringView str)
{
    const auto trimmed = str.trimmed();
    if (trimmed.compare(QLatin1String("none"), Qt::CaseInsensitive) == 0) {
        return FontHinting::None;
    }
    if (trimmed.compare(QLatin1String("slight"), Qt::CaseInsensitive) == 0) {
        return FontHinting::Slight;
    }
    if (trimmed.compare(QLatin1String("medium"), Qt::CaseInsensitive) == 0) {
        return FontHinting::Medium;
    }
    if (trimmed.compare(QLatin1String("full"), Qt::CaseInsensitive) == 0) {
        return FontHinting::Full;
    }
    return std::nullopt;
}

QString fontSubpixelOrderToString(FontSubpixelOrder order)
{
    switch (order) {
    case FontSubpixelOrder::None:
        return QStringLiteral("none");
    case FontSubpixelOrder::Rgb:
        return QStringLiteral("rgb");
    case FontSubpixelOrder::Bgr:
        return QStringLiteral("bgr");
    case FontSubpixelOrder::Vrgb:
        return QStringLiteral("vrgb");
    case FontSubpixelOrder::Vbgr:
        return QStringLiteral("vbgr");
    case FontSubpixelOrder::Unknown:
        return QStringLiteral("unknown");
    }
    return QStringLiteral("rgb");
}

std::optional<FontSubpixelOrder> fontSubpixelOrderFromString(QStringView str)
{
    const auto trimmed = str.trimmed();
    if (trimmed.compare(QLatin1String("none"), Qt::CaseInsensitive) == 0) {
        return FontSubpixelOrder::None;
    }
    if (trimmed.compare(QLatin1String("rgb"), Qt::CaseInsensitive) == 0) {
        return FontSubpixelOrder::Rgb;
    }
    if (trimmed.compare(QLatin1String("bgr"), Qt::CaseInsensitive) == 0) {
        return FontSubpixelOrder::Bgr;
    }
    if (trimmed.compare(QLatin1String("vrgb"), Qt::CaseInsensitive) == 0) {
        return FontSubpixelOrder::Vrgb;
    }
    if (trimmed.compare(QLatin1String("vbgr"), Qt::CaseInsensitive) == 0) {
        return FontSubpixelOrder::Vbgr;
    }
    if (trimmed.compare(QLatin1String("unknown"), Qt::CaseInsensitive) == 0) {
        return FontSubpixelOrder::Unknown;
    }
    return std::nullopt;
}

} // namespace QindaQt::Services::FontPreferences
