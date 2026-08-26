// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/profiles/layout_profile.h"

#include <QString>
#include <QVector>

namespace QindaQt::Profiles {

struct LoadResult {
    bool ok = false;
    LayoutProfile profile;
    QString error;
};

class ProfileLoader final {
public:
    [[nodiscard]] static LoadResult fromFile(const QString &path);
    [[nodiscard]] static LoadResult fromJson(const QByteArray &json, const QString &origin);
    [[nodiscard]] static QVector<LoadResult> fromDirectory(const QString &path);
};

} // namespace QindaQt::Profiles
