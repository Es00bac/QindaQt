// SPDX-License-Identifier: GPL-3.0-or-later
#include "fs_ops.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

#if defined(__linux__)
#include <linux/fs.h>
#endif

namespace QindaQt::QindaLutris {

bool renameNoReplace(const QString &from, const QString &to, QString *error, int *errnoOut) {
  const QByteArray source = QFile::encodeName(from);
  const QByteArray destination = QFile::encodeName(to);
#if defined(__linux__) && defined(SYS_renameat2) && defined(RENAME_NOREPLACE)
  const long rc = ::syscall(SYS_renameat2, AT_FDCWD, source.constData(), AT_FDCWD,
                            destination.constData(), RENAME_NOREPLACE);
  if (rc == 0) {
    return true;
  }
  const int code = errno;
  if (errnoOut != nullptr) {
    *errnoOut = code;
  }
  if (error != nullptr) {
    *error = QString::fromLocal8Bit(std::strerror(code));
  }
  return false;
#else
  if (errnoOut != nullptr) {
    *errnoOut = ENOSYS;
  }
  Q_UNUSED(source);
  Q_UNUSED(destination);
  if (error != nullptr) {
    *error = QStringLiteral("atomic no-replace rename is unavailable");
  }
  return false;
#endif
}

QString nearestExistingDirectory(const QString &path) {
  if (path.isEmpty() || QDir::isRelativePath(path)) {
    return {};
  }
  QString current = QDir::cleanPath(path);
  for (int guard = 0; guard < 256; ++guard) {
    const QFileInfo info(current);
    if (info.exists() && info.isDir()) {
      return current;
    }
    const QString parent = info.absolutePath();
    if (parent == current) {
      break;
    }
    current = parent;
  }
  return {};
}

bool isSymlink(const QString &path) { return QFileInfo(path).isSymLink(); }

bool removeTreeForcibly(const QString &path, QString *error) {
  constexpr int kMaxDirectories = 200000;
  const QFileInfo top(path);
  if (!top.exists() && !top.isSymLink()) {
    return true;
  }
  if (top.isSymLink() || !top.isDir()) {
    if (!QFile::remove(path) && error != nullptr) {
      *error = QStringLiteral("could not remove %1").arg(path);
    }
    return !QFileInfo::exists(path) && !QFileInfo(path).isSymLink();
  }
  // Make every real directory writable/searchable by its owner first.
  QStringList pending{path};
  int visited = 0;
  while (!pending.isEmpty() && visited < kMaxDirectories) {
    const QString directory = pending.takeLast();
    ++visited;
    const QByteArray encoded = QFile::encodeName(directory);
    struct stat info {};
    if (::lstat(encoded.constData(), &info) != 0 || !S_ISDIR(info.st_mode)) {
      continue;
    }
    if ((info.st_mode & S_IRWXU) != S_IRWXU) {
      ::chmod(encoded.constData(), (info.st_mode & 07777) | S_IRWXU);
    }
    const QFileInfoList children = QDir(directory).entryInfoList(
        QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    for (const QFileInfo &child : children) {
      if (!child.isSymLink()) {
        pending.append(child.filePath());
      }
    }
  }
  QDir(path).removeRecursively();
  if (!QFileInfo::exists(path)) {
    return true;
  }
  if (error != nullptr) {
    *error = QStringLiteral("%1 could not be removed completely").arg(path);
  }
  return false;
}

} // namespace QindaQt::QindaLutris
