// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/design_tokens/token_deriver.h"

#include "color_math_p.h"
#include "qindaqt/design_tokens/design_tokens.h"
#include "qindaqt/themes/theme_spec.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace QindaQt::DesignTokens {
namespace {

constexpr std::array<const char *, 9> requiredColors = {
    "canvas", "surface", "surfaceRaised", "border", "text",
    "textMuted", "accent", "accentText", "danger"};

struct SemanticColors final {
    QColor canvas;
    QColor surface;
    QColor surfaceRaised;
    QColor border;
    QColor text;
    QColor textMuted;
    QColor accent;
    QColor accentText;
    QColor danger;
};

DerivationResult failure(DerivationError error, QString diagnostic)
{
    return {.tokens = {}, .error = error, .diagnostic = std::move(diagnostic)};
}

QColor overlay(const QColor &foreground,
               double alpha,
               const QColor &surface,
               bool reducedTransparency)
{
    const QColor translucent = Private::withAlpha(foreground, alpha);
    return reducedTransparency ? Private::compositeOver(translucent, surface) : translucent;
}

StatusPair pair(const QColor &background)
{
    return {.background = background,
            .foreground = Private::contrastForeground(background)};
}

QColor accessibleOrFallback(const QColor &candidate,
                            const QColor &background,
                            double minimumRatio,
                            const QColor &fallback)
{
    return DesignTokenDeriver::contrastRatio(candidate, background) >= minimumRatio
        ? candidate
        : fallback;
}

QColor opaqueOver(const QColor &foreground, const QColor &background)
{
    QColor flattened = Private::compositeOver(foreground, background);
    flattened.setAlpha(255);
    return flattened;
}

SemanticColors flattenTransparency(const SemanticColors &source)
{
    // AGENT-CONTRACT: Reduced transparency belongs entirely to QST-1. Flatten
    // authored schema-v1 alpha through this semantic stack so controls and
    // shell never need local opacity fallbacks. The canvas's RGB luminance
    // selects the nearest binary desktop backdrop, including when canvas alpha
    // is zero; this keeps the transform total and deterministic.
    const QColor desktopBackdrop = DesignTokenDeriver::relativeLuminance(source.canvas) < 0.5
        ? QColor(Qt::black)
        : QColor(Qt::white);
    SemanticColors flattened;
    flattened.canvas = opaqueOver(source.canvas, desktopBackdrop);
    flattened.surface = opaqueOver(source.surface, flattened.canvas);
    flattened.surfaceRaised = opaqueOver(source.surfaceRaised, flattened.surface);
    flattened.border = opaqueOver(source.border, flattened.surface);
    flattened.text = opaqueOver(source.text, flattened.surface);
    flattened.textMuted = opaqueOver(source.textMuted, flattened.surface);
    flattened.accent = opaqueOver(source.accent, flattened.surface);
    flattened.accentText = opaqueOver(source.accentText, flattened.accent);
    flattened.danger = opaqueOver(source.danger, flattened.surface);
    return flattened;
}

StatusTokens statusTokens(bool darkBackground)
{
    if (darkBackground) {
        return {
            .success = pair(QColor::fromRgb(0x78, 0xd6, 0x9b)),
            .warning = pair(QColor::fromRgb(0xf2, 0xc6, 0x6d)),
            .info = pair(QColor::fromRgb(0x83, 0xbd, 0xf2)),
        };
    }
    return {
        .success = pair(QColor::fromRgb(0x17, 0x6b, 0x43)),
        .warning = pair(QColor::fromRgb(0x75, 0x48, 0x00)),
        .info = pair(QColor::fromRgb(0x18, 0x58, 0x8a)),
    };
}

MotionTokens motionTokens(int baseDuration, bool reducedMotion)
{
    const int shortDuration = std::max(80, static_cast<int>(std::lround(baseDuration * 0.6)));
    const int longDuration = static_cast<int>(std::lround(baseDuration * 1.75));
    if (!reducedMotion) {
        return {.instant = 0,
                .shortDuration = shortDuration,
                .base = baseDuration,
                .longDuration = longDuration};
    }
    return {.instant = 0,
            .shortDuration = std::min(shortDuration, 80),
            .base = std::min(baseDuration, 80),
            .longDuration = std::min(longDuration, 80)};
}

ElevationTokens elevationTokens(bool themeBlurEnabled,
                                bool darkBackground,
                                bool reducedTransparency)
{
    const bool backgroundBlur = themeBlurEnabled && !reducedTransparency;
    const double opacityScale = reducedTransparency ? 0.0 : (darkBackground ? 1.15 : 1.0);
    return {
        .one = {.backgroundBlur = backgroundBlur,
                .blurRadius = backgroundBlur ? 12 : 0,
                .verticalOffset = 2,
                .shadowOpacity = 0.14 * opacityScale},
        .two = {.backgroundBlur = backgroundBlur,
                .blurRadius = backgroundBlur ? 20 : 0,
                .verticalOffset = 4,
                .shadowOpacity = 0.20 * opacityScale},
        .three = {.backgroundBlur = backgroundBlur,
                  .blurRadius = backgroundBlur ? 30 : 0,
                  .verticalOffset = 8,
                  .shadowOpacity = 0.26 * opacityScale},
    };
}

MotionEntryTokens motionEntry(const QindaQt::Themes::MotionSpec &spec, bool reducedMotion)
{
    return {.duration = reducedMotion ? std::min(spec.duration, 80) : spec.duration,
            .easing = spec.easing};
}

SurfaceMotionTokens surfaceMotionTokens(const QindaQt::Themes::ThemeSpec &theme,
                                        bool reducedMotion)
{
    namespace Names = QindaQt::Themes::MotionNames;
    return {.popup = motionEntry(theme.motion(QString(Names::Popup)), reducedMotion),
            .menu = motionEntry(theme.motion(QString(Names::Menu)), reducedMotion),
            .rollup = motionEntry(theme.motion(QString(Names::Rollup)), reducedMotion),
            .hover = motionEntry(theme.motion(QString(Names::Hover)), reducedMotion)};
}

// AGENT-GUARD: text on a translucent surface composites over unknown
// desktop pixels. The published opacity is the authored one raised until
// fg.default and fg.muted keep 4.5:1 against the surface over both a black
// and a white backdrop; an opaque surface is untouched.
double guardedOpacity(double requested, const QColor &surface, const ForegroundTokens &foreground)
{
    const auto passes = [&](double opacity) {
        for (const QColor &backdrop : {QColor(Qt::black), QColor(Qt::white)}) {
            const QColor composite = DesignTokenDeriver::compositeOver(surface, opacity, backdrop);
            if (DesignTokenDeriver::contrastRatio(foreground.defaultColor, composite) < 4.5
                || DesignTokenDeriver::contrastRatio(foreground.muted, composite) < 4.5) {
                return false;
            }
        }
        return true;
    };
    double opacity = std::clamp(requested, 0.0, 1.0);
    while (opacity < 1.0) {
        if (passes(opacity)) {
            return opacity;
        }
        opacity = std::min(1.0, opacity + 0.02);
    }
    return 1.0;
}

SurfaceMaterialTokens materialFor(const QindaQt::Themes::ThemeSpec &theme,
                                  const QString &name,
                                  const QColor &surfaceColor,
                                  const ForegroundTokens &foreground,
                                  const AccessibilityInputs &normalized)
{
    const auto spec = theme.surface(name);
    SurfaceMaterialTokens tokens;
    tokens.radius = static_cast<double>(theme.surfaceRadius(name));
    tokens.border = spec.border;
    tokens.highlight = spec.highlight;
    tokens.shadow = spec.shadow;
    // Reduce transparency and high contrast flatten every surface: opaque,
    // unblurred, untinted, exactly like a schema v1 theme.
    if (normalized.reducedTransparency || normalized.highContrast) {
        return tokens;
    }
    tokens.blur = spec.blur;
    tokens.tint = spec.tint;
    tokens.opacity = guardedOpacity(spec.opacity, surfaceColor, foreground);
    return tokens;
}

MaterialTokens materialTokens(const QindaQt::Themes::ThemeSpec &theme,
                              const BackgroundTokens &background,
                              const ForegroundTokens &foreground,
                              const AccessibilityInputs &normalized)
{
    namespace Names = QindaQt::Themes::SurfaceNames;
    // The color each surface paints its material with: panels, popups,
    // menus and container chrome paint bg.raised; window title bars paint
    // bg.highest; desktop icon plates paint bg.base.
    return {
        .panel = materialFor(theme, QString(Names::Panel), background.raised, foreground,
                             normalized),
        .popup = materialFor(theme, QString(Names::Popup), background.raised, foreground,
                             normalized),
        .menu = materialFor(theme, QString(Names::Menu), background.raised, foreground,
                            normalized),
        .containerChrome = materialFor(theme, QString(Names::ContainerChrome), background.raised,
                                       foreground, normalized),
        .decoration = materialFor(theme, QString(Names::Decoration), background.highest,
                                  foreground, normalized),
        .desktopIcons = materialFor(theme, QString(Names::DesktopIcons), background.base,
                                    foreground, normalized),
    };
}

double linearized(double component)
{
    return component <= 0.04045 ? component / 12.92
                                : std::pow((component + 0.055) / 1.055, 2.4);
}

} // namespace

DerivationResult DesignTokenDeriver::derive(const QindaQt::Themes::ThemeSpec &theme,
                                            const AccessibilityInputs &inputs)
{
    if (theme.schemaVersion != 1 && theme.schemaVersion != 2) {
        return failure(DerivationError::InvalidSchemaVersion,
                       QStringLiteral("QST-1 requires theme schemaVersion 1 or 2"));
    }
    if (theme.id.isEmpty() || theme.name.isEmpty() || theme.variant.isEmpty()) {
        return failure(DerivationError::MissingIdentity,
                       QStringLiteral("QST-1 requires a loaded theme identity"));
    }
    for (const auto *name : requiredColors) {
        const QString key = QString::fromLatin1(name);
        if (!theme.colors.value(key).isValid()) {
            return failure(DerivationError::InvalidColor,
                           QStringLiteral("QST-1 missing valid theme color: %1").arg(key));
        }
    }
    if (theme.cornerRadius < 0 || theme.cornerRadius > 32 || theme.motionDuration < 0
        || theme.motionDuration > 1000) {
        return failure(DerivationError::InvalidMetric,
                       QStringLiteral("QST-1 received theme metrics outside schema-v1 bounds"));
    }

    const AccessibilityInputs normalized = inputs.normalized();
    SemanticColors colors = {
        .canvas = theme.colors.value(QStringLiteral("canvas")),
        .surface = theme.colors.value(QStringLiteral("surface")),
        .surfaceRaised = theme.colors.value(QStringLiteral("surfaceRaised")),
        .border = theme.colors.value(QStringLiteral("border")),
        .text = theme.colors.value(QStringLiteral("text")),
        .textMuted = theme.colors.value(QStringLiteral("textMuted")),
        .accent = theme.colors.value(QStringLiteral("accent")),
        .accentText = theme.colors.value(QStringLiteral("accentText")),
        .danger = theme.colors.value(QStringLiteral("danger")),
    };
    if (normalized.reducedTransparency) {
        colors = flattenTransparency(colors);
    }
    const bool darkBackground = relativeLuminance(colors.surface) < 0.5;

    const BackgroundTokens background = {.base = colors.canvas,
                                         .raised = colors.surface,
                                         .highest = colors.surfaceRaised};
    const ForegroundTokens foreground = {
        .defaultColor = colors.text,
        .muted = colors.textMuted,
        .disabled = overlay(colors.textMuted,
                            0.5,
                            colors.surface,
                            normalized.reducedTransparency),
    };
    const AccentTokens accentValues = {
        .defaultColor = colors.accent,
        .foreground = colors.accentText,
        .subtle = overlay(colors.accent,
                          0.12,
                          colors.surface,
                          normalized.reducedTransparency),
    };
    const StateTokens state = {
        .hover = overlay(colors.text, 0.08, colors.surface, normalized.reducedTransparency),
        .pressed = overlay(colors.text, 0.16, colors.surface, normalized.reducedTransparency),
    };
    const QColor focusRing = normalized.highContrast
        ? colors.text
        : accessibleOrFallback(colors.accent, colors.surface, 3.0, colors.text);
    const QColor divider = colors.border;
    const QColor mixedOutline = Private::mix(colors.border, colors.text, 0.10);
    const QColor strongOutline = normalized.highContrast
        ? colors.text
        : accessibleOrFallback(mixedOutline, colors.surface, 3.0, colors.text);
    const StatusTokens statuses = statusTokens(darkBackground);
    const DangerTokens dangerValues = {
        .defaultColor = colors.danger,
        .foreground = Private::contrastForeground(colors.danger),
    };
    const double radius = static_cast<double>(theme.cornerRadius);
    const RadiusTokens radii = {.small = radius / 2.0,
                                .medium = radius,
                                .large = std::min(32.0, radius * 1.5)};
    const SpacingTokens spacing;
    const double body = normalized.basePointSize * normalized.textScale;
    const TypeScaleTokens typeScale = {
        .fontFamily = theme.fontFamily,
        .monoFontFamily = theme.monoFontFamily,
        .caption = body * 0.85,
        .body = body,
        .subtitle = body * 1.25,
        .title = body * 1.5,
        .display = body * 2.0,
    };
    const MotionTokens motion = motionTokens(theme.motionDuration, normalized.reducedMotion);
    const ElevationTokens elevation = elevationTokens(
        theme.blurEnabled, darkBackground, normalized.reducedTransparency);
    const MaterialTokens material = materialTokens(theme, background, foreground, normalized);
    const SurfaceMotionTokens surfaceMotion = surfaceMotionTokens(theme, normalized.reducedMotion);

    // AGENT-GUARD: Construct a complete value only after validating every
    // ThemeSpec field used above. Publishing a partial map would force QML
    // controls to invent fallbacks and create a second token authority.
    auto *raw = new DesignTokens(theme.id,
                                 normalized,
                                 background,
                                 foreground,
                                 accentValues,
                                 state,
                                 focusRing,
                                 divider,
                                 strongOutline,
                                 statuses,
                                 dangerValues,
                                 radii,
                                 spacing,
                                 typeScale,
                                 motion,
                                 elevation,
                                 material,
                                 surfaceMotion);
    return {.tokens = std::shared_ptr<const DesignTokens>(raw),
            .error = DerivationError::None,
            .diagnostic = {}};
}

double DesignTokenDeriver::relativeLuminance(const QColor &color)
{
    return 0.2126 * linearized(static_cast<double>(color.redF()))
        + 0.7152 * linearized(static_cast<double>(color.greenF()))
        + 0.0722 * linearized(static_cast<double>(color.blueF()));
}

QColor DesignTokenDeriver::compositeOver(const QColor &surface, double opacity,
                                         const QColor &backdrop)
{
    const double alpha = std::clamp(opacity, 0.0, 1.0) * static_cast<double>(surface.alphaF());
    const auto channel = [alpha](float over, float under) {
        return static_cast<float>(std::clamp(static_cast<double>(over) * alpha
                                                 + static_cast<double>(under) * (1.0 - alpha),
                                             0.0, 1.0));
    };
    return QColor::fromRgbF(channel(surface.redF(), backdrop.redF()),
                            channel(surface.greenF(), backdrop.greenF()),
                            channel(surface.blueF(), backdrop.blueF()));
}

double DesignTokenDeriver::contrastRatio(const QColor &foreground, const QColor &background)
{
    const QColor opaqueBackground = background.alphaF() < 1.0F
        ? Private::compositeOver(background, QColor(Qt::white))
        : background;
    const QColor opaqueForeground = foreground.alphaF() < 1.0F
        ? Private::compositeOver(foreground, opaqueBackground)
        : foreground;
    const double foregroundLuminance = relativeLuminance(opaqueForeground);
    const double backgroundLuminance = relativeLuminance(opaqueBackground);
    return (std::max(foregroundLuminance, backgroundLuminance) + 0.05)
        / (std::min(foregroundLuminance, backgroundLuminance) + 0.05);
}

} // namespace QindaQt::DesignTokens
