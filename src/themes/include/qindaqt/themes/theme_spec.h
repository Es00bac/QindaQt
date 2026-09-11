// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QColor>
#include <QHash>
#include <QString>
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
    // them, and decoration consumers fall back to the classic surfaceRaised
    // rendering, so the five original built-ins stay pixel-identical.
    QColor titleBarColor;
    QColor titleBarInactiveColor;
    QColor restoreColor;

    [[nodiscard]] QVariantMap toVariantMap() const;
};

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

    [[nodiscard]] QVariantMap toVariantMap() const;
};

} // namespace QindaQt::Themes
