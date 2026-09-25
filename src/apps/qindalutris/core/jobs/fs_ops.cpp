// SPDX-License-Identifier: GPL-3.0-or-later
#include "fs_ops.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/syscall.h>
#include <unistd.h>

#if defined(__linux__)
#include <linux/fs.h>
#endif

namespace QindaQt::QindaLutris {

bool renameNoReplace(const QString &from, const QString &to, QString *error) {
  const QByteArray source = QFile::encodeName(from);
  const QByteArray destination = QFile::encodeName(to);
#if defined(__linux__) && defined(SYS_renameat2) && defined(RENAME_NOREPLACE)
  const long rc = ::syscall(SYS_renameat2, AT_FDCWD, source.constData(), AT_FDCWD,
                            destination.constData(), RENAME_NOREPLACE);
  if (rc == 0) {
    return true;
  }
  if (error != nullptr) {
    *error = QString::fromLocal8Bit(std::strerror(errno));
  }
  return false;
#else
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

} // namespace QindaQt::QindaLutris
