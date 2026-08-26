// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/hybrid_chrome/chrometypes.h"

#include <array>
#include <cmath>
#include <utility>

namespace QindaQt::HybridChrome {
namespace {

bool reject(QString *error, QString message)
{
    if (error) {
        *error = std::move(message);
    }
    return false;
}

bool validMetric(qreal value)
{
    return std::isfinite(value) && value >= 0.0;
}

} // namespace

bool ChromePalette::isValid(QString *error) const
{
    const std::array<std::pair<const char *, QColor>, 9> colors{{
        {"surface", surface},
        {"surfaceRaised", surfaceRaised},
        {"border", border},
        {"text", text},
        {"textMuted", textMuted},
        {"accent", accent},
        {"close", close},
        {"minimize", minimize},
        {"maximize", maximize},
    }};
    for (const auto &[name, color] : colors) {
        if (!color.isValid()) {
            return reject(error, QStringLiteral("chrome palette color '%1' is invalid")
                                     .arg(QString::fromLatin1(name)));
        }
    }
    return true;
}

ChromeStyle ChromeStyle::qindaMacOS(ChromePalette palette)
{
    ChromeStyle result;
    result.buttonSide = ButtonSide::Left;
    result.tabDirection = TabVisualDirection::RightToLeft;
    result.buttonStyle = ButtonStyle::TrafficLights;
    result.hoverGlyphs = true;
    result.palette = std::move(palette);
    return result;
}

ChromeStyle ChromeStyle::standard(ButtonSide side, ChromePalette palette)
{
    ChromeStyle result;
    result.buttonSide = side;
    result.tabDirection = TabVisualDirection::LeftToRight;
    result.buttonStyle = ButtonStyle::Symbols;
    result.hoverGlyphs = false;
    result.palette = std::move(palette);
    return result;
}

bool ChromeMetrics::isValid(QString *error) const
{
    const std::array<std::pair<const char *, qreal>, 16> values{{
        {"outerBorder", outerBorder},
        {"outerResizeMargin", outerResizeMargin},
        {"cornerRadius", cornerRadius},
        {"titleBarHeight", titleBarHeight},
        {"tabStripHeight", tabStripHeight},
        {"titleHorizontalInset", titleHorizontalInset},
        {"buttonExtent", buttonExtent},
        {"buttonSpacing", buttonSpacing},
        {"buttonClusterInset", buttonClusterInset},
        {"tabHorizontalInset", tabHorizontalInset},
        {"tabSpacing", tabSpacing},
        {"tabMinimumWidth", tabMinimumWidth},
        {"tabMaximumWidth", tabMaximumWidth},
        {"memberTitleHeight", memberTitleHeight},
        {"dividerVisualThickness", dividerVisualThickness},
        {"dividerHitThickness", dividerHitThickness},
    }};
    for (const auto &[name, value] : values) {
        if (!validMetric(value)) {
            return reject(error, QStringLiteral("chrome metric '%1' must be finite and non-negative")
                                     .arg(QString::fromLatin1(name)));
        }
    }
    if (titleBarHeight <= 0.0 || buttonExtent <= 0.0 || tabMaximumWidth <= 0.0
        || memberTitleHeight <= 0.0 || dividerVisualThickness <= 0.0) {
        return reject(error, QStringLiteral("chrome extents must be positive"));
    }
    if (tabMinimumWidth > tabMaximumWidth) {
        return reject(error, QStringLiteral("tab minimum width exceeds its maximum"));
    }
    if (dividerHitThickness < dividerVisualThickness) {
        return reject(error, QStringLiteral("divider hit thickness must cover its visual thickness"));
    }
    return true;
}

qreal ChromeMetrics::physicalHairline(qreal devicePixelRatio) const
{
    return std::isfinite(devicePixelRatio) && devicePixelRatio > 0.0
        ? 1.0 / devicePixelRatio
        : 1.0;
}

} // namespace QindaQt::HybridChrome
