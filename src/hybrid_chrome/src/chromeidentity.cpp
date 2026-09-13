// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/hybrid_chrome/chromeidentity.h"

#include "qindaqt/hybrid_chrome/chrometypes.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace QindaQt::HybridChrome {
namespace {

// All luminance math runs in double; the float conversions happen only at
// the QColor boundaries (this build compiles qreal as float, and the strict
// warning set rejects implicit narrowing).
double channelLuminance(double channel)
{
    return channel <= 0.03928
        ? channel / 12.92
        : std::pow((channel + 0.055) / 1.055, 2.4);
}

double relativeLuminance(const QColor &color)
{
    return 0.2126 * channelLuminance(color.redF())
        + 0.7152 * channelLuminance(color.greenF())
        + 0.0722 * channelLuminance(color.blueF());
}

// Linear RGB mix; t = 0 returns `from`, t = 1 returns `to`.
QColor mixColors(const QColor &from, const QColor &to, double t)
{
    const auto mix = [t](double a, double b) { return a + (b - a) * t; };
    return QColor::fromRgbF(static_cast<float>(mix(from.redF(), to.redF())),
                            static_cast<float>(mix(from.greenF(), to.greenF())),
                            static_cast<float>(mix(from.blueF(), to.blueF())));
}

QColor opaque(const QColor &color)
{
    auto solid = color;
    solid.setAlpha(255);
    return solid;
}

QColor tinted(const QColor &base, float alpha)
{
    return QColor::fromRgbF(base.redF(), base.greenF(), base.blueF(), alpha);
}

// AGENT-GUARD: The adjustment order is part of the visual contract
// (ADR-0139): a minimal lightness nudge toward whichever extreme reaches the
// target with the smallest change. Reordering changes which shade every
// theme test pins.
QColor strengthenContrast(const QColor &color, const QColor &against, double target)
{
    if (identityContrastRatio(opaque(color), against) >= target) {
        return opaque(color);
    }
    const QColor white(Qt::white);
    const QColor black(Qt::black);
    QColor best;
    double bestDistance = 2.0;
    for (const QColor *extreme : {&white, &black}) {
        double low = 0.0;
        double high = 1.0;
        QColor reached;
        for (int step = 0; step < 24; ++step) {
            const double mid = (low + high) / 2.0;
            const auto candidate = mixColors(color, *extreme, mid);
            if (identityContrastRatio(candidate, against) >= target) {
                reached = candidate;
                high = mid;
            } else {
                low = mid;
            }
        }
        if (reached.isValid() && high < bestDistance) {
            bestDistance = high;
            best = reached;
        }
    }
    // One of white/black always reaches 3.0:1 (the worst mid-gray case still
    // exceeds it), so this is a belt-and-braces fallback, not a live path.
    return best.isValid() ? best : white;
}

} // namespace

qreal identityContrastRatio(const QColor &first, const QColor &second)
{
    const double firstLuminance = relativeLuminance(opaque(first));
    const double secondLuminance = relativeLuminance(opaque(second));
    const double lighter = std::max(firstLuminance, secondLuminance);
    const double darker = std::min(firstLuminance, secondLuminance);
    return static_cast<qreal>((lighter + 0.05) / (darker + 0.05));
}

QColor identityComposited(const QColor &foreground, const QColor &background)
{
    const float alpha = std::clamp(foreground.alphaF(), 0.0f, 1.0f);
    return mixColors(background, foreground, alpha);
}

QColor identityInk(const QColor &fill, const QColor &preferred)
{
    const auto solidFill = opaque(fill);
    const QColor white(Qt::white);
    const QColor black(Qt::black);
    const std::array<const QColor *, 3> candidates{&preferred, &white, &black};
    QColor best = white;
    qreal bestRatio = -1.0;
    for (const auto *candidate : candidates) {
        if (!candidate->isValid()) {
            continue;
        }
        const auto solid = opaque(*candidate);
        const auto ratio = identityContrastRatio(solid, solidFill);
        // Strictly greater: the earlier candidate wins ties, so a theme text
        // color that already passes keeps precedence over white/black.
        if (ratio > bestRatio) {
            bestRatio = ratio;
            best = solid;
        }
    }
    return best;
}

ChromeIdentityShades resolveIdentityShades(const QColor &identityColor,
                                           const ChromePalette &palette)
{
    // AGENT-CONTRACT: An unset identity color must resolve through the theme
    // accent (CONTRACTS §2.4) so an uncolored container reads exactly like
    // today's accent cues.
    const auto base = identityColor.isValid() ? opaque(identityColor) : opaque(palette.accent);
    const auto surface = opaque(palette.surface);

    ChromeIdentityShades shades;
    shades.base = base;
    shades.border = strengthenContrast(base, surface, IdentityBorderContrast);
    // Reduced strength: the border at 72% alpha over the surface, then
    // contrast-raised so the inactive frame reads quieter but never drops
    // below the border threshold.
    auto dimmedOverSurface = shades.border;
    dimmedOverSurface.setAlphaF(0.72f);
    shades.borderDimmed = strengthenContrast(
        identityComposited(dimmedOverSurface, surface), surface, IdentityBorderContrast);
    auto glow = shades.border;
    glow.setAlphaF(0.28f);
    shades.glow = glow;

    shades.tabTint = identityComposited(tinted(base, 0.16f),
                                        opaque(palette.surfaceRaised));
    shades.textOnFill = identityInk(shades.tabTint, palette.text);
    shades.badgeInk = identityInk(surface, palette.text);
    shades.indexBadgeInk = identityInk(shades.border, palette.text);
    shades.handlebarFill = base;
    shades.handlebarInk = identityInk(base, palette.text);
    return shades;
}

QColor identityPillTint(const QColor &base, const QColor &surface,
                        int pillIndex, bool active)
{
    static constexpr std::array<float, 4> TintAlphas{0.12f, 0.18f, 0.24f, 0.30f};
    const auto clampedIndex = ((pillIndex % 4) + 4) % 4;
    const float alpha = active ? TintAlphas.back()
                               : TintAlphas[static_cast<std::size_t>(clampedIndex)];
    return identityComposited(tinted(base, alpha), opaque(surface));
}

} // namespace QindaQt::HybridChrome
