// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QColor>

namespace QindaQt::HybridChrome {

struct ChromePalette;

// WCAG 2.x contrast targets for identity chrome (ADR-0139). Borders and other
// large non-text cues must hold 3.0:1 against the surface they sit on; text
// and glyphs on identity fills must hold 4.5:1.
inline constexpr qreal IdentityBorderContrast = 3.0;
inline constexpr qreal IdentityTextContrast = 4.5;

// Every visible identity shade for one container, derived from exactly one
// identity color plus the theme palette (ADR-0139). The derivation never
// returns a shade below the contrast targets above, so consumers must paint
// these values verbatim instead of re-fading or re-blending them.
struct ChromeIdentityShades final
{
    // Resolved base color: the identity color itself, or the theme accent
    // when the identity input is invalid (CONTRACTS §2.4 default).
    QColor base;
    // Full-strength frame/stripe/underline color, >= 3.0:1 vs surface.
    QColor border;
    // Inactive-container border: a reduced-strength mix of `border` toward
    // the surface that still holds >= 3.0:1.
    QColor borderDimmed;
    // Soft focus glow: `border` at 28% alpha, painted as an inner ring.
    QColor glow;
    // Active-tab fill: `base` at 16% alpha composited over surfaceRaised.
    QColor tabTint;
    // Text over the title row / tinted tab fill, >= 4.5:1.
    QColor textOnFill;
    // Ink for text on the plain surface fill (badge label, overflow
    // counter), >= 4.5:1 against the theme surface.
    QColor badgeInk;
    // Ink for text on the identity border fill (index chip digits),
    // >= 4.5:1 against the border shade.
    QColor indexBadgeInk;
    // Focused member handlebar fill (the resolved base, unmodified) and an
    // ink for its glyphs with >= 4.5:1 against that fill.
    QColor handlebarFill;
    QColor handlebarInk;

    [[nodiscard]] bool isValid() const
    {
        return base.isValid() && border.isValid() && borderDimmed.isValid()
            && glow.isValid() && tabTint.isValid() && textOnFill.isValid()
            && badgeInk.isValid() && indexBadgeInk.isValid()
            && handlebarFill.isValid() && handlebarInk.isValid();
    }
    friend bool operator==(const ChromeIdentityShades &,
                           const ChromeIdentityShades &) = default;
};

// WCAG 2.x relative contrast ratio between two opaque colors.
[[nodiscard]] qreal identityContrastRatio(const QColor &first, const QColor &second);
// Alpha-composites `foreground` over `background` and returns the opaque
// result (background alpha is assumed opaque).
[[nodiscard]] QColor identityComposited(const QColor &foreground,
                                        const QColor &background);
// Whichever of `preferred`, white, and black contrasts most with `fill`;
// `preferred` wins ties so a passing theme text color keeps precedence.
[[nodiscard]] QColor identityInk(const QColor &fill, const QColor &preferred);
// Derives the complete shade set. An invalid `identityColor` derives every
// shade from `palette.accent`, which reproduces today's cue source.
[[nodiscard]] ChromeIdentityShades
resolveIdentityShades(const QColor &identityColor, const ChromePalette &palette);
// One rolled-up-badge pill tint (ADR-0139): `base` cycled over the badge
// tints 12%/18%/24%/30% by pill index, composited over `surface`. The active
// pill always takes the strongest tint of the cycle.
[[nodiscard]] QColor identityPillTint(const QColor &base, const QColor &surface,
                                      int pillIndex, bool active);

} // namespace QindaQt::HybridChrome
