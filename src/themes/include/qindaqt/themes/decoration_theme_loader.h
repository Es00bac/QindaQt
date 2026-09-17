// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/themes/decoration_theme_spec.h"

#include <QStringList>
#include <QVector>

#include <optional>

namespace QindaQt::Themes {

struct DecorationThemeLoadResult {
    bool ok = false;
    DecorationThemeSpec theme;
    QString error;
};

// Loads `data/decorations/*.json` documents (ADR-0207). Strict: an unknown
// enum, a color that does not parse, or a metric outside its bounds fails
// the document rather than degrading it.
class DecorationThemeLoader final {
public:
    [[nodiscard]] static DecorationThemeLoadResult fromFile(const QString &path);
    [[nodiscard]] static DecorationThemeLoadResult fromJson(const QByteArray &json,
                                                            const QString &origin);
    [[nodiscard]] static QVector<DecorationThemeLoadResult> fromDirectory(const QString &path);
    // Every valid document across the directories, earlier directories winning
    // duplicate ids; an invalid document fails the whole load (fail closed,
    // like the theme catalog).
    [[nodiscard]] static std::optional<QVector<DecorationThemeSpec>>
    loadDirectories(const QStringList &directories, QString *error = nullptr);
    [[nodiscard]] static std::optional<DecorationThemeSpec>
    find(const QVector<DecorationThemeSpec> &documents, const QString &id);
};

} // namespace QindaQt::Themes
