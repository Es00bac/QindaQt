// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QColor>
#include <QHash>
#include <QString>
#include <QVariantMap>

namespace QindaQt::Themes {

struct DecorationSpec {
    QString buttonPlacement = QStringLiteral("right");
    QString tabDirection = QStringLiteral("left-to-right");
    QString buttonStyle = QStringLiteral("symbols");
    bool hoverGlyphs = false;
    QColor closeColor = QColor(QStringLiteral("#f07c76"));
    QColor minimizeColor = QColor(QStringLiteral("#e8bf63"));
    QColor maximizeColor = QColor(QStringLiteral("#71bd8a"));

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
    QHash<QString, QColor> colors;
    int cornerRadius = 10;
    int motionDuration = 160;
    bool blurEnabled = false;
    DecorationSpec decoration;

    [[nodiscard]] QVariantMap toVariantMap() const;
};

} // namespace QindaQt::Themes
