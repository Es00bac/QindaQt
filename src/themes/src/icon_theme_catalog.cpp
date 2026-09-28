// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/themes/icon_theme_catalog.h"
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QStandardPaths>
#include <algorithm>
#include <optional>

namespace QindaQt::Themes {
bool safeIconThemeId(const QString &id)
{
    if (id.isEmpty() || id.size() > 128 || id.contains(QStringLiteral(".."))) return false;
    for (const QChar c : id) {
        const auto n = c.unicode();
        if (!((n >= 'a' && n <= 'z') || (n >= 'A' && n <= 'Z')
              || (n >= '0' && n <= '9') || c == '-' || c == '_' || c == '.')) return false;
    }
    return true;
}

QStringList standardIconThemeRoots()
{
    QStringList roots;
    for (const auto &root : QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation))
        roots.append(QDir(root).filePath(QStringLiteral("icons")));
    roots.removeDuplicates();
    return roots;
}

namespace {
// One candidate theme directory under a canonical root: its display name when
// it is a readable, bounded, non-hidden XDG icon theme confined to the root.
std::optional<QString> iconThemeName(const QString &directory, const QString &canonicalRoot)
{
    const QFileInfo index(QDir(directory).filePath(QStringLiteral("index.theme")));
    if (!index.isFile() || !index.isReadable() || index.size() > 256 * 1024) return std::nullopt;
    const QString path = index.canonicalFilePath();
    if (!path.startsWith(canonicalRoot + QLatin1Char('/'))) return std::nullopt;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return std::nullopt;
    const QByteArray bytes = file.read(256 * 1024 + 1);
    if (bytes.size() > 256 * 1024) return std::nullopt;
    bool section = false, found = false, hidden = false;
    QString name;
    for (const auto &line : QString::fromUtf8(bytes).split(QLatin1Char('\n'))) {
        const QString text = line.trimmed();
        if (text.startsWith(QLatin1Char('['))) {
            section = text == QLatin1String("[Icon Theme]");
            found |= section;
        } else if (section && text.startsWith(QLatin1String("Name="))) {
            name = text.sliced(5).trimmed().left(128);
        } else if (section && text == QLatin1String("Hidden=true")) hidden = true;
    }
    if (!found || hidden) return std::nullopt;
    return name;
}

// Whether `id` names an installed theme under the roots, checked directly
// (not through the bounded listing, whose caps could hide a real theme).
bool isInstalledIconTheme(const QString &id, const QStringList &roots)
{
    if (!safeIconThemeId(id)) return false;
    for (const auto &root : roots.mid(0, 32)) {
        const QString canonical = QFileInfo(root).canonicalFilePath();
        if (canonical.isEmpty()) continue;
        const QString directory = QDir(root).filePath(id);
        if (QFileInfo(directory).isDir() && iconThemeName(directory, canonical)) return true;
    }
    return false;
}
} // namespace

QVector<IconThemeEntry> installedIconThemes(const QStringList &roots)
{
    QVector<IconThemeEntry> entries;
    QSet<QString> seen;
    int scanned = 0;
    for (const auto &root : roots.mid(0, 32)) {
        const QString canonical = QFileInfo(root).canonicalFilePath();
        if (canonical.isEmpty()) continue;
        QDirIterator it(root, QDir::Dirs | QDir::NoDotAndDotDot);
        while (it.hasNext() && scanned++ < 1024 && entries.size() < 128) {
            it.next();
            const QString id = it.fileName();
            if (!safeIconThemeId(id) || seen.contains(id)) continue;
            const auto name = iconThemeName(it.filePath(), canonical);
            if (!name) continue;
            seen.insert(id);
            entries.append({id, name->isEmpty() ? id : *name});
        }
    }
    std::sort(entries.begin(), entries.end(), [](const auto &a, const auto &b) {
        return a.name == b.name ? a.id < b.id : a.name < b.name;
    });
    return entries;
}

QString resolveIconTheme(const QString &preference, const QString &authored,
                         const QStringList &roots)
{
    // ADR-0280 order: the user's installed choice, then the theme's authored
    // family, then QindaQt. AGENT-GUARD: the authored id must be installed
    // too; handing Qt a missing theme name drops every icon to hicolor.
    if (isInstalledIconTheme(preference, roots)) return preference;
    if (isInstalledIconTheme(authored, roots)) return authored;
    return QStringLiteral("QindaQt");
}
}
