// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/themes/icon_theme_catalog.h"
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QStandardPaths>
#include <algorithm>

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
            const QFileInfo index(QDir(it.filePath()).filePath(QStringLiteral("index.theme")));
            if (!index.isFile() || !index.isReadable() || index.size() > 256 * 1024) continue;
            const QString path = index.canonicalFilePath();
            if (!path.startsWith(canonical + QLatin1Char('/'))) continue;
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly)) continue;
            const QByteArray bytes = file.read(256 * 1024 + 1);
            if (bytes.size() > 256 * 1024) continue;
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
            if (!found || hidden) continue;
            seen.insert(id);
            entries.append({id, name.isEmpty() ? id : name});
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
    if (safeIconThemeId(preference)) {
        for (const auto &entry : installedIconThemes(roots))
            if (entry.id == preference) return entry.id;
    }
    return safeIconThemeId(authored) ? authored : QStringLiteral("QindaQt");
}
}
