// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/themes/theme_spec.h"

#include <QString>
#include <QVariantMap>

namespace QindaQt::Themes {

// A decoration theme document (ADR-0207): the arrangement and material of
// application window chrome and container chrome, authored separately from
// a color theme so the two can be chosen independently in Appearance. Colors
// are optional: an invalid color defers to the color theme's own decoration
// colors, so one decoration document serves many palettes.
struct DecorationThemeSpec final {
    int schemaVersion = 1;
    QString id;
    QString name;
    QString description;
    // Arrangement and optional colors; `authored` is always true here.
    DecorationSpec decoration;
    // Title-bar / shared-row material (opacity, blur, highlight, border).
    SurfaceMaterialSpec titleMaterial;
    // Window frame radius, 0 to 32 logical pixels.
    int cornerRadius = 10;
    // Nine-patch shadow extent (0 to 48 logical pixels) and opacity (0 to 1).
    double shadowExtent = 12.0;
    double shadowOpacity = 0.30;
    // Contained-window handlebar grip: grip (bars), dots, or plain.
    QString memberHandleStyle = QStringLiteral("grip");
    // Rolled-up container badge: pill or square corners.
    QString containerBadgeStyle = QStringLiteral("pill");

    [[nodiscard]] QVariantMap toVariantMap() const;
    [[nodiscard]] bool hasAuthoredColors() const;
};

namespace DecorationThemeTokens {
// Every window-button style name a theme or document may author (ADR-0264).
// The decoration painter holds one spec per name; its tests pin the match.
[[nodiscard]] QStringList buttonStyles();
[[nodiscard]] QStringList memberHandleStyles();
[[nodiscard]] QStringList containerBadgeStyles();
} // namespace DecorationThemeTokens

} // namespace QindaQt::Themes
