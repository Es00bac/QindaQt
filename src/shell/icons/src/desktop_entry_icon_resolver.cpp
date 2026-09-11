// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/shell/icons/desktop_entry_icon_resolver.h>

#include <qindaqt/shell/icons/icon_theme_limits.h>
#include <qindaqt/shell/icons/icon_theme_locator.h>

#include <qindaqt/shell_launcher/desktop_entry_parser.h>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>

#include <algorithm>

namespace QindaQt::Shell::Icons
{
namespace
{

bool containsId(const auto &entries, const QString &id)
{
    return std::any_of(entries.cbegin(), entries.cend(),
                       [&id](const auto &entry) { return entry.id == id; });
}

bool isIdSegment(const QString &segment)
{
    if (segment.isEmpty() || !segment.front().isLetter()) {
        return false;
    }
    return std::all_of(segment.cbegin(), segment.cend(), [](QChar character) {
        return character.isLetterOrNumber() || character == QLatin1Char('_')
            || character == QLatin1Char('-');
    });
}

} // namespace

DesktopEntryIconResolver::DesktopEntryIconResolver(QStringList applicationRoots)
{
    for (const QString &root : std::as_const(applicationRoots)) {
        if (full()) {
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
            return entry.value;
        }
    }
    return {};
}

QString DesktopEntryIconResolver::iconNameForAppId(const QString &appId) const
{
    const Entry *entry = matchAppId(m_entries, appId);
    return entry != nullptr ? entry->value : QString{};
}

QString DesktopEntryIconResolver::displayNameForAppId(const QString &appId) const
{
    const Entry *entry = matchAppId(m_names, appId);
    return entry != nullptr ? entry->value : QString{};
}

QString DesktopEntryIconResolver::applicationDisplayName(const QString &appId,
                                                         const QString &reportedName) const
{
    QString name = displayNameForAppId(appId);
    if (name.isEmpty() && reportedName != appId) {
        name = displayNameForAppId(reportedName);
    }
    if (!name.isEmpty()) {
        return name;
    }
    return prettifiedApplicationId(reportedName.isEmpty() ? appId : reportedName);
}

QString DesktopEntryIconResolver::prettifiedApplicationId(const QString &applicationId)
{
    QString name = applicationId.trimmed();
    if (name.endsWith(QLatin1String(".desktop"), Qt::CaseInsensitive)) {
        name.chop(8);
    }
    const QStringList segments = name.split(QLatin1Char('.'));
    if (segments.size() > 1 && std::all_of(segments.cbegin(), segments.cend(), isIdSegment)) {
        name = segments.constLast();
    }
    if (!name.isEmpty() && name.front().isLower()) {
        name.front() = name.front().toUpper();
    }
    return name;
}

const DesktopEntryIconResolver::Entry *
DesktopEntryIconResolver::matchAppId(const QList<Entry> &entries, const QString &appId)
{
    if (appId.isEmpty() || appId.toUtf8().size() > kMaxDesktopEntryIdUtf8Bytes) {
        return nullptr;
    }
    QString candidate = appId;
    if (candidate.endsWith(QLatin1String(".desktop"), Qt::CaseInsensitive)) {
        candidate.chop(8);
    }
    if (candidate.isEmpty()) {
        return nullptr;
    }
    const auto first = [&entries](const auto &matches) -> const Entry * {
        const auto found = std::find_if(entries.cbegin(), entries.cend(), matches);
        return found != entries.cend() ? &*found : nullptr;
    };
    if (const Entry *exact = first([&candidate](const Entry &entry) {
            return entry.id == candidate;
        })) {
        return exact;
    }
    if (const Entry *folded = first([&candidate](const Entry &entry) {
            return entry.id.compare(candidate, Qt::CaseInsensitive) == 0;
        })) {
        return folded;
    }
    // AGENT-NOTE: X11 clients report WM_CLASS, which desktop entries declare
    // as StartupWMClass (the first-party entries set it, e.g. qindaqt-editor);
    // the icon and the name of one window therefore come from one entry.
    if (const Entry *wmClass = first([&candidate](const Entry &entry) {
            return !entry.startupWmClass.isEmpty()
                && entry.startupWmClass.compare(candidate, Qt::CaseInsensitive) == 0;
        })) {
        return wmClass;
    }
    // AGENT-NOTE: Compositor app ids are frequently the bare final component
    // of a reverse-DNS desktop id (e.g. Wayland app_id "dolphin" for
    // org.kde.dolphin.desktop). The tail match keeps that mapping working;
    // scan order makes collisions deterministic.
    const QString tail = QLatin1Char('.') + candidate;
    return first([&tail](const Entry &entry) {
        return entry.id.endsWith(tail, Qt::CaseInsensitive);
    });
}

bool DesktopEntryIconResolver::full() const
{
    return m_entries.size() >= kMaxDesktopEntries && m_names.size() >= kMaxDesktopEntries;
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
        if (full()) {
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
    // First claim wins: earlier roots shadow later ones, per claim kind.
    const bool iconClaimed = containsId(m_entries, id) || m_entries.size() >= kMaxDesktopEntries;
    const bool nameClaimed = containsId(m_names, id) || m_names.size() >= kMaxDesktopEntries;
    if (iconClaimed && nameClaimed) {
        return;
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
    // flag) contribute nothing; malformed documents fail closed. The parser
    // guarantees a non-blank, bounded Name for every accepted entry.
    if (!parsed.ok() || parsed.entry->hidden) {
        return;
    }
    if (!nameClaimed) {
        m_names.append({id, parsed.entry->name, parsed.entry->startupWmClass});
    }
    // An `Icon=` value that is not a plain icon name (absolute paths,
    // separators, or other hostiles) is refused here so downstream consumers
    // only ever see names the confined locator grammar accepts.
    if (!iconClaimed && IconThemeLocator::isAcceptableIconName(parsed.entry->iconName)) {
        m_entries.append({id, parsed.entry->iconName, parsed.entry->startupWmClass});
    }
}

} // namespace QindaQt::Shell::Icons
