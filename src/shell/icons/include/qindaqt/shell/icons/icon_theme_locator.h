// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QStringList>

namespace QindaQt::Shell::Icons
{

// AGENT-CONTRACT: XDG Icon Theme Specification lookup over an injected,
// ordered list of icon roots. Production composition injects
// `XDG_DATA_HOME/icons` followed by each `XDG_DATA_DIRS/icons` entry and a
// theme chain of `breeze` then `hicolor`; tests inject fixture roots. The
// locator never reads the environment and never adds `~/.icons` on its own.
//
// Resolution contract (docs/wiki/shell/iconography.md):
//
// 1. The theme chain is the injected theme names in order, each expanded by
//    its `Inherits=` parents (depth-capped, cycle-guarded), with `hicolor`
//    always appended last per the specification's fallback rule.
// 2. Within one theme, directories from every root carrying that theme are
//    matched per the spec: an exact `Size`/`Scale` (Fixed), `Threshold`
//    window, or `Scalable` MinSize/MaxSize match wins in root-then-declared
//    order; otherwise the smallest spec distance wins with the same tie
//    order. Scale-aware: the requested scale is matched against `Scale=`.
// 3. With `symbolic` requested, `<name>-symbolic` is tried through the whole
//    chain before `<name>` itself.
// 4. Unthemed `<root>/<name>.<ext>` hits are the final fallback.
//
// Confinement: every candidate (indexes and icons) is canonicalized and must
// remain beneath its canonical injected root, so `../` declarations and
// symlink escapes fail closed. Icon names outside [A-Za-z0-9._-], containing
// `..`, or over the byte ceiling are refused. Extension probing order is
// fixed (png, svg, xpm) for determinism. No network access and no filesystem
// writes ever happen here.
//
// Threading: an instance is confined to the thread that owns it (the parsed
// -index cache is mutable). The image provider serializes its own instance
// behind a mutex; the QML seam holds a separate GUI-thread instance.
class IconThemeLocator
{
public:
    // iconRoots and themeNames are used in the given order; `hicolor` need
    // not be listed, it is always the final themed fallback.
    IconThemeLocator(QStringList iconRoots, QStringList themeNames);

    // Returns the canonical absolute path of the best match, or an empty
    // string when the name is refused or nothing resolves. `size` is a
    // logical pixel size clamped to [1, 512]; `scale` is clamped to [1, 4].
    [[nodiscard]] QString locate(const QString &iconName, int size, double scale = 1.0,
                                 bool symbolic = false) const;

    // True when locate() would resolve; same validation and confinement.
    [[nodiscard]] bool hasIcon(const QString &iconName, int size, double scale = 1.0,
                               bool symbolic = false) const;

    // The name grammar every entry point enforces.
    [[nodiscard]] static bool isAcceptableIconName(const QString &iconName);

private:
    enum class DirectoryType { Fixed, Scalable, Threshold };

    struct ThemeDirectory {
        QString subDirectory;
        int size = 0;
        int scale = 1;
        DirectoryType type = DirectoryType::Threshold;
        int minSize = 0;
        int maxSize = 0;
        int threshold = 2;
    };

    struct ThemeIndex {
        bool parsed = false;
        QStringList inherits;
        QList<ThemeDirectory> directories;
    };

    [[nodiscard]] QStringList themeChain() const;
    void expandTheme(const QString &theme, int depth, QStringList &chain) const;
    [[nodiscard]] ThemeIndex loadIndex(const QString &themeDirectory) const;
    [[nodiscard]] QString findInTheme(const QString &iconName, int size, double scale,
                                      const QString &theme) const;
    [[nodiscard]] QString probeDirectory(const QString &root, const QString &themeDirectory,
                                         const ThemeDirectory &directory,
                                         const QString &iconName) const;
    [[nodiscard]] static bool directoryMatchesSize(const ThemeDirectory &directory, int size,
                                                   double scale);
    [[nodiscard]] static qint64 directorySizeDistance(const ThemeDirectory &directory, int size,
                                                      double scale);
    [[nodiscard]] QString confinedFile(const QString &root, const QString &candidate) const;
    [[nodiscard]] static QStringList extensions();

    QStringList m_iconRoots;   // canonicalized, order-preserving
    QStringList m_themeNames;  // injected order, without hicolor
    mutable QHash<QString, ThemeIndex> m_indexCache;
    mutable QStringList m_indexCacheOrder;
    mutable QStringList m_themeChainCache;
};

} // namespace QindaQt::Shell::Icons
