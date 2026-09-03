// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/shell/status_notifier/icon/status_notifier_icon_locator.h>

#include <qindaqt/shell/status_notifier/status_notifier_limits.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace QindaQt::StatusNotifier
{
namespace
{

// The freedesktop index.theme keys this bounded subset understands.
[[nodiscard]] QString themeSectionName(const QString &themeSubDirectory)
{
    return themeSubDirectory.isEmpty() ? QStringLiteral("index.theme")
                                       : themeSubDirectory + QStringLiteral("/index.theme");
}

[[nodiscard]] int parseSizeValue(const QString &line)
{
    const QStringView value =
        QStringView(line).mid(line.indexOf(QLatin1Char('=')) + 1).trimmed();
    bool ok = false;
    const int size = value.toInt(&ok);
    return ok && size > 0 ? size : -1;
}

// Returns a canonical regular file only when it is contained by the canonical
// injected root. This closes both parent-directory and symlink escapes for
// index files and icon candidates.
[[nodiscard]] QString confinedFile(const QString &rootDirectory,
                                   const QString &candidate)
{
    const QString canonicalRoot = QDir(rootDirectory).canonicalPath();
    const QFileInfo candidateInfo(candidate);
    const QString canonicalCandidate = candidateInfo.canonicalFilePath();
    if (canonicalRoot.isEmpty() || canonicalCandidate.isEmpty()
        || !candidateInfo.isFile()) {
        return {};
    }
    const QString relative = QDir(canonicalRoot).relativeFilePath(canonicalCandidate);
    if (relative == QLatin1String("..")
        || relative.startsWith(QLatin1String("../"))
        || QDir::isAbsolutePath(relative)) {
        return {};
    }
    return canonicalCandidate;
}

} // namespace

StatusNotifierIconLocator::StatusNotifierIconLocator(QStringList themeRoots)
{
    m_themeRoots.reserve(themeRoots.size());
    for (const QString &root : std::as_const(themeRoots)) {
        const QString canonical = QDir(root).canonicalPath();
        m_themeRoots.append(canonical.isEmpty() ? QDir::cleanPath(root) : canonical);
    }
}

QString StatusNotifierIconLocator::locate(const QString &iconName, int preferredSize) const
{
    if (!isAcceptableIconName(iconName)) {
        return {};
    }
    const int target = preferredSize > 0 ? preferredSize : 16;

    // Each injected root is a freedesktop search path: its own index.theme
    // (a root that is itself one theme) plus every immediate subdirectory
    // that carries an index.theme, in sorted order for determinism. The
    // nearest declared size across all of the root's themes wins; hicolor is
    // therefore consulted whenever present without special-casing.
    for (const QString &root : std::as_const(m_themeRoots)) {
        qint64 bestDistance = -1;
        QString bestPath = probeDirectories(root, loadIndex(root, {}), iconName, target,
                                            &bestDistance);
        const QDir rootDir(root);
        const QStringList subDirectories =
            rootDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        qsizetype scanned = 0;
        for (const QString &subDirectory : subDirectories) {
            if (subDirectory == QLatin1String("hicolor")
                || scanned >= kMaxIconThemeDirectories) {
                continue; // Hicolor is consulted after every other theme of this root.
            }
            ++scanned;
            const QString candidate = probeDirectories(root,
                                                       loadIndex(root, subDirectory),
                                                       iconName, target, &bestDistance);
            if (!candidate.isEmpty()) {
                bestPath = candidate;
            }
        }
        const QString hicolor = probeDirectories(root, loadIndex(root, QStringLiteral("hicolor")),
                                                 iconName, target, &bestDistance);
        if (!hicolor.isEmpty()) {
            bestPath = hicolor;
        }
        if (!bestPath.isEmpty()) {
            return bestPath;
        }
    }
    // Direct root hits are the last resort before failure.
    for (const QString &root : std::as_const(m_themeRoots)) {
        for (const QString &extension : extensions()) {
            const QString candidate =
                root + QLatin1Char('/') + iconName + QLatin1Char('.') + extension;
            const QString confined = confinedFile(root, candidate);
            if (!confined.isEmpty()) {
                return confined;
            }
        }
    }
    return {};
}

StatusNotifierIconLocator::ThemeIndex
StatusNotifierIconLocator::loadIndex(const QString &rootDirectory,
                                     const QString &themeSubDirectory) const
{
    const QString cacheKey = themeSectionName(themeSubDirectory);
    const QString cacheHashKey = rootDirectory + QLatin1Char('|') + cacheKey;
    const auto cached = m_indexCache.constFind(cacheHashKey);
    if (cached != m_indexCache.cend()) {
        return cached.value();
    }

    ThemeIndex index;
    index.parsed = true;
    const QString indexPath = rootDirectory + QLatin1Char('/')
        + (themeSubDirectory.isEmpty() ? cacheKey
                                       : themeSubDirectory + QStringLiteral("/index.theme"));
    const QString confinedIndex = confinedFile(rootDirectory, indexPath);
    if (confinedIndex.isEmpty()) {
        m_indexCache.insert(cacheHashKey, index);
        return index;
    }
    QFile file(confinedIndex);
    if (file.open(QIODevice::ReadOnly)) {
        const QByteArray raw = file.read(kMaxIconThemeIndexBytes + 1);
        if (raw.size() <= kMaxIconThemeIndexBytes) {
            const QStringList lines = QString::fromUtf8(raw).split(QLatin1Char('\n'));
            QStringList declaredDirectories;
            QString currentSection;
            QHash<QString, int> directorySizes;
            for (const QString &rawLine : lines) {
                const QString line = rawLine.trimmed();
                if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']'))) {
                    currentSection = line.mid(1, line.size() - 2).trimmed();
                    continue;
                }
                if (line.startsWith(QLatin1Char('#')) || !line.contains(QLatin1Char('='))) {
                    continue;
                }
                if (currentSection == QLatin1String("Icon Theme")
                    && line.startsWith(QLatin1String("Directories"))) {
                    declaredDirectories =
                        line.mid(line.indexOf(QLatin1Char('=')) + 1)
                            .split(QLatin1Char(','), Qt::SkipEmptyParts);
                } else if (!currentSection.isEmpty()) {
                    const int size = parseSizeValue(line);
                    if (size > 0) {
                        directorySizes.insert(currentSection, size);
                    }
                }
            }
            for (const QString &directory : std::as_const(declaredDirectories)) {
                const QString cleaned = directory.trimmed();
                if (cleaned.isEmpty() || index.directories.size() >= kMaxIconThemeDirectories) {
                    continue;
                }
                const int size = directorySizes.value(cleaned, -1);
                if (size > 0) {
                    index.directories.append({cleaned, size});
                }
            }
        }
        // An unreadable or oversized index simply contributes no directories.
    }
    m_indexCache.insert(cacheHashKey, index);
    return index;
}

QString StatusNotifierIconLocator::probeDirectories(const QString &rootDirectory,
                                                    const ThemeIndex &index,
                                                    const QString &iconName,
                                                    int preferredSize,
                                                    qint64 *bestDistance) const
{
    QString bestPath;
    qint64 best = *bestDistance;
    for (const auto &[directory, size] : index.directories) {
        for (const QString &extension : extensions()) {
            const QString candidate = rootDirectory + QLatin1Char('/') + directory
                + QLatin1Char('/') + iconName + QLatin1Char('.') + extension;
            // AGENT-GUARD: P1-7 regression: index.theme directory values are
            // hostile. Canonical containment is required after resolution so
            // both "../" and symlink escapes fail closed.
            const QString confined = confinedFile(rootDirectory, candidate);
            if (confined.isEmpty()) {
                continue;
            }
            const qint64 distance = qAbs(qint64(size) - qint64(preferredSize));
            if (best < 0 || distance < best) {
                best = distance;
                bestPath = confined;
            }
            break; // First existing extension for this directory wins.
        }
    }
    *bestDistance = best;
    return bestPath;
}

bool StatusNotifierIconLocator::isAcceptableIconName(const QString &iconName)
{
    if (iconName.isEmpty()
        || iconName.toUtf8().size() > kMaxIconNameUtf8Bytes
        || iconName.contains(QLatin1String(".."))) {
        return false;
    }
    for (const QChar character : iconName) {
        const bool acceptable = (character >= QLatin1Char('a') && character <= QLatin1Char('z'))
            || (character >= QLatin1Char('A') && character <= QLatin1Char('Z'))
            || (character >= QLatin1Char('0') && character <= QLatin1Char('9'))
            || character == QLatin1Char('_') || character == QLatin1Char('-')
            || character == QLatin1Char('.');
        if (!acceptable) {
            return false;
        }
    }
    return true;
}

QStringList StatusNotifierIconLocator::extensions()
{
    // PNG first: it is the only format the renderer's tests must rely on, and
    // the order makes resolution deterministic when several formats exist.
    return {QStringLiteral("png"), QStringLiteral("bmp"), QStringLiteral("xpm"),
            QStringLiteral("ico")};
}

} // namespace QindaQt::StatusNotifier
