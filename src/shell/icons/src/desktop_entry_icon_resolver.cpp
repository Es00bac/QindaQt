// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/shell/icons/desktop_entry_icon_resolver.h>

#include <qindaqt/shell/icons/icon_theme_limits.h>
#include <qindaqt/shell/icons/icon_theme_locator.h>

#include <qindaqt/shell_launcher/desktop_entry_parser.h>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>

namespace QindaQt::Shell::Icons
{

DesktopEntryIconResolver::DesktopEntryIconResolver(QStringList applicationRoots)
{
    for (const QString &root : std::as_const(applicationRoots)) {
        if (m_entries.size() >= kMaxDesktopEntries) {
            break;
        }
        scanRoot(root);
    }
}

QString DesktopEntryIconResolver::iconNameForDesktopId(const QString &desktopEntryId) const
{
    if (desktopEntryId.isEmpty()
        || desktopEntryId.toUtf8().size() > kMaxDesktopEntryIdUtf8Bytes) {
        return {};
    }
    for (const Entry &entry : m_entries) {
        if (entry.id == desktopEntryId) {
            return entry.iconName;
        }
    }
    return {};
}

QString DesktopEntryIconResolver::iconNameForAppId(const QString &appId) const
{
    if (appId.isEmpty() || appId.toUtf8().size() > kMaxDesktopEntryIdUtf8Bytes) {
        return {};
    }
    QString candidate = appId;
    if (candidate.endsWith(QLatin1String(".desktop"), Qt::CaseInsensitive)) {
        candidate.chop(8);
    }
    if (candidate.isEmpty()) {
        return {};
    }
    const QString exact = iconNameForDesktopId(candidate);
    if (!exact.isEmpty()) {
        return exact;
    }
    for (const Entry &entry : m_entries) {
        if (entry.id.compare(candidate, Qt::CaseInsensitive) == 0) {
            return entry.iconName;
        }
    }
    // AGENT-NOTE: Compositor app ids are frequently the bare final component
    // of a reverse-DNS desktop id (e.g. Wayland app_id "dolphin" for
    // org.kde.dolphin.desktop). The tail match keeps that mapping working;
    // scan order makes collisions deterministic.
    const QString tail = QLatin1Char('.') + candidate;
    for (const Entry &entry : m_entries) {
        if (entry.id.endsWith(tail, Qt::CaseInsensitive)) {
            return entry.iconName;
        }
    }
    return {};
}

void DesktopEntryIconResolver::scanRoot(const QString &root)
{
    const QString canonicalRoot = QDir(root).canonicalPath();
    if (canonicalRoot.isEmpty()) {
        return;
    }
    QDirIterator iterator(canonicalRoot, {QStringLiteral("*.desktop")},
                          QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    QStringList relativePaths;
    while (iterator.hasNext()) {
        const QString absolute = iterator.next();
        const QString relative = QDir(canonicalRoot).relativeFilePath(absolute);
        // Bound the tree walk itself: deep or oversized scans stop instead of
        // growing shell memory. Depth counts path separators in the relative
        // desktop-entry path.
        if (relative.size() > kMaxDesktopEntryIdUtf8Bytes + 8
            || relative.count(QLatin1Char('/')) > kMaxDesktopEntryDepth) {
            continue;
        }
        relativePaths.append(relative);
        if (relativePaths.size() >= kMaxDesktopEntries) {
            break;
        }
    }
    relativePaths.sort(Qt::CaseSensitive);
    for (const QString &relative : std::as_const(relativePaths)) {
        if (m_entries.size() >= kMaxDesktopEntries) {
            return;
        }
        admitFile(canonicalRoot, relative, canonicalRoot + QLatin1Char('/') + relative);
    }
}

void DesktopEntryIconResolver::admitFile(const QString &canonicalRoot,
                                         const QString &relativePath,
                                         const QString &canonicalFile)
{
    const QFileInfo fileInfo(canonicalFile);
    const QString canonicalTarget = fileInfo.canonicalFilePath();
    // AGENT-GUARD: Canonical containment after resolution is the only escape
    // defense; a desktop file reached through a symlink leaving the injected
    // root must never be opened.
    if (canonicalTarget.isEmpty() || !fileInfo.isFile()) {
        return;
    }
    const QString relative = QDir(canonicalRoot).relativeFilePath(canonicalTarget);
    if (relative == QLatin1String("..") || relative.startsWith(QLatin1String("../"))
        || QDir::isAbsolutePath(relative)) {
        return;
    }

    QString id = relativePath;
    if (id.endsWith(QLatin1String(".desktop"))) {
        id.chop(8);
    }
    id.replace(QLatin1Char('/'), QLatin1Char('-'));
    for (const Entry &entry : m_entries) {
        if (entry.id == id) {
            return; // First claim wins: earlier roots shadow later ones.
        }
    }

    QFile file(canonicalTarget);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    const QByteArray raw = file.read(kMaxDesktopEntryBytes + 1);
    if (raw.size() > kMaxDesktopEntryBytes) {
        return;
    }
    const QindaQt::ShellLauncher::DesktopEntryParseResult parsed =
        QindaQt::ShellLauncher::DesktopEntryParser::parse(QString::fromUtf8(raw));
    // Hidden entries (Hidden=true or NoDisplay=true in the parser's combined
    // flag) contribute no icon; malformed documents fail closed. An `Icon=`
    // value that is not a plain icon name (absolute paths, separators, or
    // other hostiles) is refused here so downstream consumers only ever see
    // names the confined locator grammar accepts.
    if (!parsed.ok() || parsed.entry->hidden
        || !IconThemeLocator::isAcceptableIconName(parsed.entry->iconName)) {
        return;
    }
    m_entries.append({id, parsed.entry->iconName});
}

} // namespace QindaQt::Shell::Icons
