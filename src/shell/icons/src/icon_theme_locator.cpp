// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/shell/icons/icon_theme_locator.h>

#include <qindaqt/shell/icons/icon_theme_limits.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>

namespace QindaQt::Shell::Icons
{
namespace
{

[[nodiscard]] int clampedSize(int size)
{
    if (size < kMinIconLogicalSize || size > kMaxIconLogicalSize) {
        return qBound(kMinIconLogicalSize, size, kMaxIconLogicalSize);
    }
    return size;
}

[[nodiscard]] double clampedScale(double scale)
{
    if (!(scale >= kMinIconScale) || scale > kMaxIconScale) {
        // NaN and non-finite inputs land on the documented default.
        return scale == scale && scale > kMaxIconScale ? kMaxIconScale : kMinIconScale;
    }
    return scale;
}

[[nodiscard]] int parsePositiveInt(const QString &line, int fallback)
{
    const qsizetype separator = line.indexOf(QLatin1Char('='));
    if (separator < 0) {
        return fallback;
    }
    bool ok = false;
    const int value = QStringView(line).mid(separator + 1).trimmed().toInt(&ok);
    return ok && value > 0 ? value : fallback;
}

} // namespace

IconThemeLocator::IconThemeLocator(QStringList iconRoots, QStringList themeNames)
{
    QSet<QString> seenRoots;
    for (const QString &root : std::as_const(iconRoots)) {
        const QString canonical = QDir(root).canonicalPath();
        const QString cleaned = canonical.isEmpty() ? QDir::cleanPath(root) : canonical;
        if (!seenRoots.contains(cleaned)) {
            seenRoots.insert(cleaned);
            m_iconRoots.append(cleaned);
        }
    }
    QSet<QString> seenThemes;
    for (const QString &theme : std::as_const(themeNames)) {
        const QString trimmed = theme.trimmed();
        // AGENT-GUARD: Theme names become directory names beneath a root; the
        // icon-name grammar is reused so a hostile theme name cannot escape.
        if (!isAcceptableIconName(trimmed) || trimmed == QLatin1String("hicolor")
            || seenThemes.contains(trimmed)) {
            continue;
        }
        seenThemes.insert(trimmed);
        m_themeNames.append(trimmed);
    }
}

QString IconThemeLocator::locate(const QString &iconName, int size, double scale,
                                 bool symbolic) const
{
    if (!isAcceptableIconName(iconName)) {
        return {};
    }
    const int target = clampedSize(size);
    const double effectiveScale = clampedScale(scale);

    QStringList names;
    if (symbolic && !iconName.endsWith(QLatin1String("-symbolic"))) {
        names.append(iconName + QStringLiteral("-symbolic"));
    }
    names.append(iconName);

    const QStringList chain = themeChain();
    for (const QString &candidateName : std::as_const(names)) {
        for (const QString &theme : chain) {
            const QString found = findInTheme(candidateName, target, effectiveScale, theme);
            if (!found.isEmpty()) {
                return found;
            }
        }
    }
    // Unthemed direct root hits are the last resort before failure.
    for (const QString &candidateName : std::as_const(names)) {
        for (const QString &root : std::as_const(m_iconRoots)) {
            for (const QString &extension : extensions()) {
                const QString confined = confinedFile(
                    root, root + QLatin1Char('/') + candidateName + QLatin1Char('.') + extension);
                if (!confined.isEmpty()) {
                    return confined;
                }
            }
        }
    }
    return {};
}

bool IconThemeLocator::hasIcon(const QString &iconName, int size, double scale,
                               bool symbolic) const
{
    return !locate(iconName, size, scale, symbolic).isEmpty();
}

bool IconThemeLocator::isAcceptableIconName(const QString &iconName)
{
    if (iconName.isEmpty() || iconName.toUtf8().size() > kMaxIconNameUtf8Bytes
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

QStringList IconThemeLocator::themeChain() const
{
    if (!m_themeChainCache.isEmpty()) {
        return m_themeChainCache;
    }
    QStringList chain;
    for (const QString &theme : std::as_const(m_themeNames)) {
        expandTheme(theme, 0, chain);
    }
    // AGENT-GUARD: hicolor is the specification-mandated last resort. It must
    // remain after every injected theme and every inherited parent.
    if (!chain.contains(QLatin1String("hicolor"))) {
        if (chain.size() < kMaxThemeChainLength) {
            chain.append(QStringLiteral("hicolor"));
        }
    }
    m_themeChainCache = chain;
    return chain;
}

void IconThemeLocator::expandTheme(const QString &theme, int depth, QStringList &chain) const
{
    if (depth > kMaxThemeInheritDepth || chain.size() >= kMaxThemeChainLength
        || chain.contains(theme)) {
        return;
    }
    chain.append(theme);
    // Inherits lists from every root carrying this theme contribute parents;
    // the first root's order dominates because chain order is stable.
    QStringList parents;
    for (const QString &root : std::as_const(m_iconRoots)) {
        const ThemeIndex index = loadIndex(root + QLatin1Char('/') + theme);
        for (const QString &parent : index.inherits) {
            if (!parents.contains(parent)) {
                parents.append(parent);
            }
        }
    }
    for (const QString &parent : std::as_const(parents)) {
        if (isAcceptableIconName(parent)) {
            expandTheme(parent, depth + 1, chain);
        }
    }
}

IconThemeLocator::ThemeIndex IconThemeLocator::loadIndex(const QString &themeDirectory) const
{
    const auto cached = m_indexCache.constFind(themeDirectory);
    if (cached != m_indexCache.cend()) {
        return cached.value();
    }

    ThemeIndex index;
    index.parsed = true;
    const QString confinedIndex = confinedFile(themeDirectory,
                                               themeDirectory + QStringLiteral("/index.theme"));
    if (!confinedIndex.isEmpty()) {
        QFile file(confinedIndex);
        if (file.open(QIODevice::ReadOnly)) {
            const QByteArray raw = file.read(kMaxThemeIndexBytes + 1);
            if (raw.size() <= kMaxThemeIndexBytes) {
                const QStringList lines = QString::fromUtf8(raw).split(QLatin1Char('\n'));
                QString currentSection;
                QHash<QString, ThemeDirectory> directoryBySection;
                QStringList declaredDirectories;
                for (const QString &rawLine : lines) {
                    const QString line = rawLine.trimmed();
                    if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']'))) {
                        currentSection = line.mid(1, line.size() - 2).trimmed();
                        continue;
                    }
                    if (line.startsWith(QLatin1Char('#')) || !line.contains(QLatin1Char('='))) {
                        continue;
                    }
                    const QString key = line.left(line.indexOf(QLatin1Char('='))).trimmed();
                    if (currentSection == QLatin1String("Icon Theme")) {
                        if (key == QLatin1String("Directories")) {
                            declaredDirectories =
                                line.mid(line.indexOf(QLatin1Char('=')) + 1)
                                    .split(QLatin1Char(','), Qt::SkipEmptyParts);
                        } else if (key == QLatin1String("Inherits")) {
                            const QStringList parents =
                                line.mid(line.indexOf(QLatin1Char('=')) + 1)
                                    .split(QLatin1Char(','), Qt::SkipEmptyParts);
                            for (const QString &parent : parents) {
                                const QString cleaned = parent.trimmed();
                                if (!cleaned.isEmpty()
                                    && index.inherits.size() < kMaxThemeChainLength) {
                                    index.inherits.append(cleaned);
                                }
                            }
                        }
                        continue;
                    }
                    if (currentSection.isEmpty()) {
                        continue;
                    }
                    ThemeDirectory &directory = directoryBySection[currentSection];
                    directory.subDirectory = currentSection;
                    if (key == QLatin1String("Size")) {
                        directory.size = parsePositiveInt(line, 0);
                    } else if (key == QLatin1String("Scale")) {
                        directory.scale = qMin(parsePositiveInt(line, 1), 8);
                    } else if (key == QLatin1String("MinSize")) {
                        directory.minSize = parsePositiveInt(line, 0);
                    } else if (key == QLatin1String("MaxSize")) {
                        directory.maxSize = parsePositiveInt(line, 0);
                    } else if (key == QLatin1String("Threshold")) {
                        directory.threshold = qMin(parsePositiveInt(line, 2), 64);
                    } else if (key == QLatin1String("Type")) {
                        const QStringView value =
                            QStringView(line).mid(line.indexOf(QLatin1Char('=')) + 1).trimmed();
                        if (value == QLatin1String("Fixed")) {
                            directory.type = DirectoryType::Fixed;
                        } else if (value == QLatin1String("Scalable")) {
                            directory.type = DirectoryType::Scalable;
                        } else {
                            directory.type = DirectoryType::Threshold;
                        }
                    }
                }
                for (const QString &declared : std::as_const(declaredDirectories)) {
                    const QString cleaned = declared.trimmed();
                    if (cleaned.isEmpty() || index.directories.size() >= kMaxThemeDirectories) {
                        continue;
                    }
                    const auto found = directoryBySection.constFind(cleaned);
                    if (found != directoryBySection.cend() && found->size > 0) {
                        index.directories.append(found.value());
                    }
                }
            }
            // An unreadable or oversized index simply contributes no directories.
        }
    }

    if (m_indexCacheOrder.size() >= kMaxCachedThemeIndexes) {
        const QString evicted = m_indexCacheOrder.takeFirst();
        m_indexCache.remove(evicted);
    }
    m_indexCache.insert(themeDirectory, index);
    m_indexCacheOrder.append(themeDirectory);
    return index;
}

QString IconThemeLocator::findInTheme(const QString &iconName, int size, double scale,
                                      const QString &theme) const
{
    // Pass 1: exact size/scale match, roots in injected order and directories
    // in declared order. Spec directories are relative to the theme
    // directory beneath each root.
    for (const QString &root : std::as_const(m_iconRoots)) {
        const QString themeDirectory = root + QLatin1Char('/') + theme;
        const ThemeIndex index = loadIndex(themeDirectory);
        for (const ThemeDirectory &directory : index.directories) {
            if (!directoryMatchesSize(directory, size, scale)) {
                continue;
            }
            const QString found = probeDirectory(root, themeDirectory, directory, iconName);
            if (!found.isEmpty()) {
                return found;
            }
        }
    }
    // Pass 2: smallest specification distance with the same tie order.
    qint64 bestDistance = -1;
    QString bestPath;
    for (const QString &root : std::as_const(m_iconRoots)) {
        const QString themeDirectory = root + QLatin1Char('/') + theme;
        const ThemeIndex index = loadIndex(themeDirectory);
        for (const ThemeDirectory &directory : index.directories) {
            const qint64 distance = directorySizeDistance(directory, size, scale);
            if (bestDistance >= 0 && distance >= bestDistance) {
                continue;
            }
            const QString found = probeDirectory(root, themeDirectory, directory, iconName);
            if (!found.isEmpty()) {
                bestDistance = distance;
                bestPath = found;
            }
        }
    }
    return bestPath;
}

QString IconThemeLocator::probeDirectory(const QString &root, const QString &themeDirectory,
                                         const ThemeDirectory &directory,
                                         const QString &iconName) const
{
    for (const QString &extension : extensions()) {
        const QString candidate = themeDirectory + QLatin1Char('/') + directory.subDirectory
            + QLatin1Char('/') + iconName + QLatin1Char('.') + extension;
        // AGENT-GUARD: index.theme directory values are hostile. Canonical
        // containment after resolution closes both "../" and symlink escapes.
        const QString confined = confinedFile(root, candidate);
        if (!confined.isEmpty()) {
            return confined;
        }
    }
    return {};
}

bool IconThemeLocator::directoryMatchesSize(const ThemeDirectory &directory, int size,
                                            double scale)
{
    if (double(directory.scale) != scale) {
        return false;
    }
    switch (directory.type) {
    case DirectoryType::Fixed:
        return directory.size == size;
    case DirectoryType::Threshold:
        return size >= directory.size - directory.threshold
            && size <= directory.size + directory.threshold;
    case DirectoryType::Scalable: {
        const int minimum = directory.minSize > 0 ? directory.minSize : directory.size;
        const int maximum = directory.maxSize > 0 ? directory.maxSize : directory.size;
        return size >= minimum && size <= maximum;
    }
    }
    return false;
}

qint64 IconThemeLocator::directorySizeDistance(const ThemeDirectory &directory, int size,
                                               double scale)
{
    const qint64 scaledSize = qint64(size * 1024 * scale);
    const qint64 scaledDirectory = qint64(directory.size) * 1024 * directory.scale;
    switch (directory.type) {
    case DirectoryType::Fixed:
        return qAbs(scaledDirectory - scaledSize);
    case DirectoryType::Threshold: {
        const qint64 window = qint64(directory.threshold) * 1024 * directory.scale;
        if (scaledSize < scaledDirectory - window) {
            return scaledDirectory - window - scaledSize;
        }
        if (scaledSize > scaledDirectory + window) {
            return scaledSize - scaledDirectory - window;
        }
        return 0;
    }
    case DirectoryType::Scalable: {
        const qint64 minimum =
            qint64(directory.minSize > 0 ? directory.minSize : directory.size) * 1024
            * directory.scale;
        const qint64 maximum =
            qint64(directory.maxSize > 0 ? directory.maxSize : directory.size) * 1024
            * directory.scale;
        if (scaledSize < minimum) {
            return minimum - scaledSize;
        }
        if (scaledSize > maximum) {
            return scaledSize - maximum;
        }
        return 0;
    }
    }
    return 0;
}

QString IconThemeLocator::confinedFile(const QString &root, const QString &candidate) const
{
    const QString canonicalRoot = QDir(root).canonicalPath();
    const QFileInfo candidateInfo(candidate);
    const QString canonicalCandidate = candidateInfo.canonicalFilePath();
    if (canonicalRoot.isEmpty() || canonicalCandidate.isEmpty() || !candidateInfo.isFile()
        || !candidateInfo.isReadable()) {
        return {};
    }
    const QString relative = QDir(canonicalRoot).relativeFilePath(canonicalCandidate);
    if (relative == QLatin1String("..") || relative.startsWith(QLatin1String("../"))
        || QDir::isAbsolutePath(relative)) {
        return {};
    }
    return canonicalCandidate;
}

QStringList IconThemeLocator::extensions()
{
    // Fixed order keeps resolution deterministic when several formats of one
    // icon exist in the same directory.
    return {QStringLiteral("png"), QStringLiteral("svg"), QStringLiteral("xpm")};
}

} // namespace QindaQt::Shell::Icons
