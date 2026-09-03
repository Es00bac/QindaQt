// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QHash>
#include <QtCore/QString>
#include <QtCore/QStringList>

namespace QindaQt::StatusNotifier
{

// AGENT-CONTRACT: Deterministic icon-theme lookup over caller-injected theme
// roots. It implements a deliberately bounded freedesktop-icon-theme subset:
//
// 1. Themed directories declared by a root's `index.theme` (`Directories=`
//    with a per-directory `Size=`); the entry with the size nearest to the
//    request wins, roots and directories in declared order on ties.
// 2. A `hicolor` theme directory under any root is consulted last, parsed
//    the same way — hicolor is the protocol-mandated fallback theme.
// 3. A direct `<root>/<name>.<ext>` hit is the final fallback before failure.
//
// Lookups never escape an injected root: icon names containing a path
// separator, a parent reference, or characters outside [A-Za-z0-9._-] are
// refused, and resolved paths must remain under the canonical root. SVG files
// are deliberately not considered (this module decodes only what QImage
// decodes natively; adding QtSvg is a dependency decision for a later lane).
// No network access and no filesystem writes ever happen here.
//
// Threading: lookup is reentrant, stateless except for a small parsed-index
// cache; construct one instance per thread if concurrent lookups are needed.
class StatusNotifierIconLocator
{
public:
    explicit StatusNotifierIconLocator(QStringList themeRoots);

    // Returns the absolute path of the best match, or an empty string when
    // the name is refused or nothing matches under any injected root.
    [[nodiscard]] QString locate(const QString &iconName, int preferredSize) const;

private:
    struct ThemeIndex {
        bool parsed = false;
        // (subdirectory, declared size) pairs in declared order.
        QList<QPair<QString, int>> directories;
    };

    [[nodiscard]] ThemeIndex loadIndex(const QString &rootDirectory,
                                       const QString &themeSubDirectory) const;
    [[nodiscard]] QString probeDirectories(const QString &rootDirectory,
                                           const ThemeIndex &index,
                                           const QString &iconName,
                                           int preferredSize,
                                           qint64 *bestDistance) const;
    [[nodiscard]] static bool isAcceptableIconName(const QString &iconName);
    [[nodiscard]] static QStringList extensions();

    QStringList m_themeRoots;
    mutable QHash<QString, ThemeIndex> m_indexCache;
};

} // namespace QindaQt::StatusNotifier
