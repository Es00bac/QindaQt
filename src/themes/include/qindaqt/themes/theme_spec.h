// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QColor>
#include <QHash>
#include <QLatin1String>
#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace QindaQt::Themes {

struct DecorationSpec {
    // True when the theme file declares a `decoration` object. Container
    // chrome follows an authored block and keeps the Qinda macOS arrangement
    // otherwise (ADR-0129). Never serialized: the object's presence is the
    // source, so the strict JSON round trip is unchanged.
    bool authored = false;
    QString buttonPlacement = QStringLiteral("right");
    QString tabDirection = QStringLiteral("left-to-right");
    QString buttonStyle = QStringLiteral("symbols");
    bool hoverGlyphs = false;
    QColor closeColor = QColor(QStringLiteral("#f07c76"));
    QColor minimizeColor = QColor(QStringLiteral("#e8bf63"));
    QColor maximizeColor = QColor(QStringLiteral("#71bd8a"));
    // Optional Luna-style title chrome. An invalid color means "not authored":
    // the loader leaves these unset for every theme that does not declare
    // them, and third-party decoration consumers fall back to the classic
    // surfaceRaised rendering. Every bundled base theme authors a distinct
    // decoration; imported legacy themes may still omit these values.
    QColor titleBarColor;
    QColor titleBarInactiveColor;
    QColor restoreColor;
    // Title-bar behaviour a theme may author (ADR-0268), so a desktop
    // experience can carry it. Empty titleDoubleClick leaves KWin's own
    // double-click; minimizeAction `roll-up` puts the ADR-0203 roll-up to the
    // icon in minimize's place; titleWear false paints an authored title bar
    // clean instead of weathered (ADR-0124). A user's Appearance choice still
    // wins where one exists (appearance.windowTitleDoubleClick).
    QString titleDoubleClick;
    QString minimizeAction = QStringLiteral("minimize");
    bool titleWear = true;

    [[nodiscard]] QVariantMap toVariantMap() const;
};

// Schema v2 (ADR-0206): the material one surface class is painted with.
// Every default reproduces schema v1, so a v1 document and an unauthored v2
// surface render byte-identically to what shipped before.
struct SurfaceMaterialSpec {
    // True when the document declares this surface. Never serialized.
    bool authored = false;
    // 0.0 (fully translucent) to 1.0 (opaque). QST raises it as far as the
    // text-contrast guardrail requires; the authored value is the floor of
    // the design intent, not a promise.
    double opacity = 1.0;
    // Ask the compositor to blur what lies behind the surface.
    bool blur = false;
    // Optional vibrancy tint painted over the blurred backdrop before the
    // surface color; invalid means none.
    QColor tint;
    // Hairline border strength, 0.0 (no border) to 1.0 (the theme border).
    double border = 1.0;
    // One-pixel inner highlight under the top edge, the "glass" catch light.
    bool highlight = false;
    // Shadow strength multiplier over the elevation default, 0.0 to 2.0.
    double shadow = 1.0;

    [[nodiscard]] QVariantMap toVariantMap() const;
    [[nodiscard]] bool operator==(const SurfaceMaterialSpec &) const = default;
};

// Schema v2: one named motion (duration in milliseconds plus an easing name).
struct MotionSpec {
    bool authored = false;
    int duration = 160;
    QString easing = QStringLiteral("standard");

    [[nodiscard]] QVariantMap toVariantMap() const;
    [[nodiscard]] bool operator==(const MotionSpec &) const = default;
};

// The surface classes and motions a schema-v2 document may author. Unknown
// names are rejected by the loader so a typo can never silently fall back.
namespace SurfaceNames {
inline constexpr QLatin1String Panel{"panel"};
inline constexpr QLatin1String Popup{"popup"};
inline constexpr QLatin1String Menu{"menu"};
inline constexpr QLatin1String ContainerChrome{"containerChrome"};
inline constexpr QLatin1String Decoration{"decoration"};
inline constexpr QLatin1String DesktopIcons{"desktopIcons"};
[[nodiscard]] QStringList all();
} // namespace SurfaceNames

namespace MotionNames {
inline constexpr QLatin1String Popup{"popup"};
inline constexpr QLatin1String Menu{"menu"};
inline constexpr QLatin1String Rollup{"rollup"};
inline constexpr QLatin1String Hover{"hover"};
[[nodiscard]] QStringList all();
[[nodiscard]] QStringList easings();
} // namespace MotionNames

class ThemeSpec final {
public:
    int schemaVersion = 1;
    QString id;
    QString name;
    QString variant;
    QString fontFamily = QStringLiteral("Inter");
    QString monoFontFamily = QStringLiteral("JetBrains Mono");
    // Optional non-token metadata consumed by shell and application composition. The
    // loader validates the bounded XDG theme-name grammar before this value
    // enters a catalog, so downstream consumers never re-parse theme files.
    QString iconTheme;
    QHash<QString, QColor> colors;
    int cornerRadius = 10;
    int motionDuration = 160;
    bool blurEnabled = false;
    DecorationSpec decoration;

    // Schema v2 (ADR-0206). Only authored entries are stored; the accessors
    // below fill defaults so consumers never branch on the schema version.
    QHash<QString, SurfaceMaterialSpec> surfaces;
    QHash<QString, int> radii;
    QHash<QString, MotionSpec> motions;
    // `fixed` keeps the authored accent; `wallpaper` asks the shell to derive
    // it from the current wallpaper (reserved: no consumer samples yet).
    QString accentMode = QStringLiteral("fixed");
    // Decoration theme document (data/decorations/<id>.json) this color theme
    // pairs with; empty keeps the inline `decoration` block.
    QString decorationTheme;

    // The material for one surface class: the authored entry, or a v1-shaped
    // default whose blur follows `blurEnabled` for panels, popups, and menus
    // (the ADR-0120 rule) and is off for everything else.
    [[nodiscard]] SurfaceMaterialSpec surface(const QString &surfaceName) const;
    // The authored per-surface radius, or `cornerRadius`.
    [[nodiscard]] int surfaceRadius(const QString &surfaceName) const;
    // The authored motion, or `motionDuration` with the standard easing.
    [[nodiscard]] MotionSpec motion(const QString &motionName) const;

    [[nodiscard]] QVariantMap toVariantMap() const;
};

} // namespace QindaQt::Themes
