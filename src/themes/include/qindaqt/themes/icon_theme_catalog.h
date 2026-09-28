// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QStringList>
#include <QVector>

namespace QindaQt::Themes {
struct IconThemeEntry {
    QString id;
    QString name;
};
// Read-only bounded discovery below explicit XDG icon roots. Symlink escapes,
// unsafe ids and oversized indexes are refused; first root wins. Returned
// values own their strings; no retained filesystem handles or global cache.
[[nodiscard]] bool safeIconThemeId(const QString &id);
[[nodiscard]] QStringList standardIconThemeRoots();
[[nodiscard]] QVector<IconThemeEntry> installedIconThemes(const QStringList &roots);
// ADR-0280 order: an installed user preference, else the color theme's
// authored id when that is installed too, else QindaQt. Empty, missing,
// invalid or uninstalled ids fall through; never interpret one as a path.
[[nodiscard]] QString resolveIconTheme(const QString &preference, const QString &authored,
                                       const QStringList &roots);
}
