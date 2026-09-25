// SPDX-License-Identifier: GPL-3.0-or-later
#include "staged_tree_check.h"

#include "link_resolution.h"

#include <QDir>
#include <QFile>
#include <QHash>
#include <QList>
#include <QPair>

#include <sys/stat.h>
#include <unistd.h>

namespace QindaQt::QindaLutris {

namespace {

struct Inode {
  qint64 links = 0;
  qint64 seen = 0;
  QString example;
};

QString joined(const QString &base, const QStringList &components) {
  return base + QLatin1Char('/') + components.join(QLatin1Char('/'));
}

std::optional<QString> readLinkAt(const QString &path) {
  const QByteArray encoded = QFile::encodeName(path);
  struct stat info {};
  if (::lstat(encoded.constData(), &info) != 0 || !S_ISLNK(info.st_mode)) {
    return std::nullopt;
  }
  char buffer[4097];
  const ssize_t length = ::readlink(encoded.constData(), buffer, sizeof(buffer) - 1);
  if (length < 0) {
    return QString(); // unreadable link: an empty target is refused
  }
  return QFile::decodeName(QByteArray(buffer, static_cast<qsizetype>(length)));
}

StagedTreeVerdict refuse(const QString &reason, qsizetype entries) {
  StagedTreeVerdict verdict;
  verdict.reason = reason;
  verdict.entries = entries;
  return verdict;
}

} // namespace

StagedTreeVerdict verifyStagedBuild(const QString &extractDir, const QString &toolName,
                                    qsizetype maxEntries) {
  const QString base = QDir::cleanPath(extractDir);
  struct stat topInfo {};
  if (::lstat(QFile::encodeName(joined(base, {toolName})).constData(), &topInfo) != 0 ||
      !S_ISDIR(topInfo.st_mode)) {
    return refuse(QStringLiteral("The unpacked build is not a real folder."), 0);
  }
  QList<QStringList> pending{{toolName}};
  QList<QPair<QStringList, QString>> symlinks;
  QHash<QPair<quint64, quint64>, Inode> inodes;
  qsizetype entries = 0;
  while (!pending.isEmpty()) {
    const QStringList directory = pending.takeLast();
    if (::access(QFile::encodeName(joined(base, directory)).constData(), R_OK | X_OK) != 0) {
      return refuse(QStringLiteral("Cannot read the unpacked folder %1")
                        .arg(directory.join(QLatin1Char('/'))),
                    entries);
    }
    const QStringList names = QDir(joined(base, directory))
                                  .entryList(QDir::AllEntries | QDir::Hidden | QDir::System |
                                             QDir::NoDotAndDotDot);
    for (const QString &name : names) {
      if (++entries > maxEntries) {
        return refuse(QStringLiteral("The unpacked build has too many files."), entries);
      }
      const QStringList path = directory + QStringList{name};
      const QString full = joined(base, path);
      struct stat info {};
      if (::lstat(QFile::encodeName(full).constData(), &info) != 0) {
        return refuse(QStringLiteral("Cannot inspect %1").arg(full), entries);
      }
      if (S_ISDIR(info.st_mode)) {
        pending.append(path);
      } else if (S_ISLNK(info.st_mode)) {
        symlinks.append({path, readLinkAt(full).value_or(QString())});
      } else if (S_ISREG(info.st_mode)) {
        if ((info.st_mode & (S_ISUID | S_ISGID)) != 0) {
          return refuse(QStringLiteral("The unpacked build has a setuid/setgid file: %1")
                            .arg(path.join(QLatin1Char('/'))),
                        entries);
        }
        if (info.st_nlink > 1) {
          Inode &inode = inodes[{quint64(info.st_dev), quint64(info.st_ino)}];
          inode.links = qint64(info.st_nlink);
          ++inode.seen;
          inode.example = path.join(QLatin1Char('/'));
        }
      } else {
        return refuse(QStringLiteral("The unpacked build has a special file: %1")
                          .arg(path.join(QLatin1Char('/'))),
                      entries);
      }
    }
  }
  const LinkReader readLink = [&base](const QStringList &path) { return readLinkAt(joined(base, path)); };
  for (const auto &[path, target] : std::as_const(symlinks)) {
    const auto resolved = resolveConfined(path.mid(0, path.size() - 1), target, readLink);
    if (!resolved || resolved->isEmpty() || resolved->first() != toolName) {
      return refuse(QStringLiteral("The unpacked build has a link leading outside it: %1 -> %2")
                        .arg(path.join(QLatin1Char('/')), target),
                    entries);
    }
  }
  for (auto it = inodes.cbegin(); it != inodes.cend(); ++it) {
    if (it->seen != it->links) {
      return refuse(QStringLiteral("The unpacked build shares a file with something outside "
                                   "it (hard link): %1")
                        .arg(it->example),
                    entries);
    }
  }
  StagedTreeVerdict verdict;
  verdict.ok = true;
  verdict.entries = entries;
  return verdict;
}

} // namespace QindaQt::QindaLutris
