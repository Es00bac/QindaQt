// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/themes/theme_spec.h"

#include <QVector>

namespace QindaQt::Themes {

struct LoadResult {
    bool ok = false;
    ThemeSpec theme;
    QString error;
};

class ThemeLoader final {
public:
    [[nodiscard]] static LoadResult fromFile(const QString &path);
    [[nodiscard]] static LoadResult fromJson(const QByteArray &json, const QString &origin);
    [[nodiscard]] static QVector<LoadResult> fromDirectory(const QString &path);
};

} // namespace QindaQt::Themes
