// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/apps/settings_login_screen/sddm_discovery.h>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QList>

#include <algorithm>

namespace QindaQt::Apps::SettingsLoginScreen {
namespace {

// Minimal [group] key=value reader for the two freedesktop-shaped files
// this module reads. Only named keys are returned; comments and blank
// lines are skipped, and the first occurrence of a key wins (these files
// never carry duplicates in practice, and first-wins matches what a
// hand-checking operator expects to see in the file).
[[nodiscard]] QString readIniValue(const QString &filePath,
                                   const QString &wantedGroup,
                                   const QString &wantedKey) {
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return QString();
  }
  QString group;
  const QString content = QString::fromUtf8(file.readAll());
  const QStringList lines = content.split(u'\n');
  for (const QString &rawLine : lines) {
    const QString line = rawLine.trimmed();
    if (line.isEmpty() || line.startsWith(u'#') || line.startsWith(u';')) {
      continue;
    }
    if (line.startsWith(u'[') && line.endsWith(u']')) {
      group = line.mid(1, line.size() - 2).trimmed();
      continue;
    }
    if (group != wantedGroup) {
      continue;
    }
    const qsizetype equals = line.indexOf(u'=');
    if (equals <= 0) {
      continue;
    }
    if (line.left(equals).trimmed() == wantedKey) {
      return line.mid(equals + 1).trimmed();
    }
  }
  return QString();
}

} // namespace

QList<SddmThemeEntry> listSddmThemes(const QString &themesDirectory) {
  QList<SddmThemeEntry> entries;
  const QDir root(themesDirectory);
  const QFileInfoList children =
      root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
  for (const QFileInfo &child : children) {
    const QDir themeDir(child.absoluteFilePath());
    // A directory without a Main.qml is not a theme SDDM can load.
    if (!QFile::exists(themeDir.absoluteFilePath(QStringLiteral("Main.qml")))) {
      continue;
    }
    SddmThemeEntry entry;
    entry.id = child.fileName();
    entry.name = readIniValue(themeDir.absoluteFilePath(QStringLiteral("metadata.desktop")),
                              QStringLiteral("SddmGreeterTheme"),
                              QStringLiteral("Name"));
    if (entry.name.isEmpty()) {
      entry.name = entry.id;
    }
    for (const QString &candidate :
         {QStringLiteral("preview.png"), QStringLiteral("screenshot.png")}) {
      const QString path = themeDir.absoluteFilePath(candidate);
      if (QFile::exists(path)) {
        entry.previewPath = path;
        break;
      }
    }
    entry.qindaqt =
        QFile::exists(themeDir.absoluteFilePath(QStringLiteral(".qindaqt-sddm-theme")));
    entries.append(entry);
  }

  const auto byName = [](const SddmThemeEntry &a, const SddmThemeEntry &b) {
    return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
  };
  std::stable_sort(entries.begin(), entries.end(), byName);
  std::stable_sort(entries.begin(), entries.end(),
                   [](const SddmThemeEntry &a, const SddmThemeEntry &b) {
                     return a.qindaqt && !b.qindaqt;
                   });
  return entries;
}

QList<SddmSessionEntry>
listSddmSessions(const QStringList &waylandSessionDirectories,
                 const QStringList &xSessionDirectories) {
  QList<SddmSessionEntry> entries;
  const auto scan = [&entries](const QStringList &directories, bool wayland) {
    for (const QString &directory : directories) {
      const QDir dir(directory);
      const QFileInfoList files = dir.entryInfoList(
          {QStringLiteral("*.desktop")}, QDir::Files, QDir::Name);
      for (const QFileInfo &file : files) {
        const QString hidden = readIniValue(
            file.absoluteFilePath(), QStringLiteral("Desktop Entry"),
            QStringLiteral("Hidden"));
        if (hidden == QLatin1String("true")) {
          continue;
        }
        const QString name = readIniValue(file.absoluteFilePath(),
                                          QStringLiteral("Desktop Entry"),
                                          QStringLiteral("Name"));
        const QString exec = readIniValue(file.absoluteFilePath(),
                                          QStringLiteral("Desktop Entry"),
                                          QStringLiteral("Exec"));
        if (name.isEmpty() || exec.isEmpty()) {
          continue;
        }
        SddmSessionEntry entry;
        entry.id = file.fileName();
        entry.name = name;
        entry.comment = readIniValue(file.absoluteFilePath(),
                                     QStringLiteral("Desktop Entry"),
                                     QStringLiteral("Comment"));
        entry.wayland = wayland;
        entries.append(entry);
      }
    }
  };
  scan(waylandSessionDirectories, true);
  scan(xSessionDirectories, false);

  std::stable_sort(entries.begin(), entries.end(),
                   [](const SddmSessionEntry &a, const SddmSessionEntry &b) {
                     return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
                   });
  // Wayland sessions group ahead of X11 ones, alphabetical inside each group;
  // same two-stable-sort shape as the qindaqt-first theme ordering above.
  std::stable_sort(entries.begin(), entries.end(),
                   [](const SddmSessionEntry &a, const SddmSessionEntry &b) {
                     return a.wayland && !b.wayland;
                   });
  return entries;
}

QStringList listSddmLoginUsers(const QString &passwdPath) {
  QStringList users;
  QFile file(passwdPath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return users;
  }
  const QStringList lines = QString::fromUtf8(file.readAll()).split(u'\n');
  for (const QString &line : lines) {
    if (line.isEmpty() || line.startsWith(u'#')) {
      continue;
    }
    const QStringList fields = line.split(u':');
    if (fields.size() < 7) {
      continue;
    }
    bool ok = false;
    const qlonglong uid = fields.at(2).toLongLong(&ok);
    if (!ok || uid < 1000) {
      continue;
    }
    const QString shell = QFileInfo(fields.at(6)).fileName();
    if (shell == QLatin1String("nologin") || shell == QLatin1String("false")) {
      continue;
    }
    users.append(fields.at(0));
  }
  users.removeDuplicates();
  users.sort(Qt::CaseInsensitive);
  return users;
}

} // namespace QindaQt::Apps::SettingsLoginScreen
