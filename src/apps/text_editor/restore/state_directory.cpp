// SPDX-License-Identifier: GPL-3.0-or-later
#include "state_directory.h"

#include <QDir>
#include <QFile>

#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace QindaQt::Apps::TextEditor::StateDirectory {

int open(const QString &path, const bool create, int *errorNumber) {
  if (!QDir::isAbsolutePath(path) || !path.isValidUtf16() ||
      path.contains(QChar::Null)) {
    *errorNumber = EINVAL;
    return -1;
  }
  int current = ::open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  if (current < 0) {
    *errorNumber = errno;
    return -1;
  }
  const QStringList components =
      QDir::cleanPath(path).split(u'/', Qt::SkipEmptyParts);
  for (const QString &component : components) {
    const QByteArray name = QFile::encodeName(component);
    int next = ::openat(current, name.constData(),
                        O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (next < 0 && errno == ENOENT && create) {
      if (::mkdirat(current, name.constData(), 0700) != 0 && errno != EEXIST) {
        *errorNumber = errno;
        ::close(current);
        return -1;
      }
      next = ::openat(current, name.constData(),
                      O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    }
    if (next < 0) {
      *errorNumber = errno;
      ::close(current);
      return -1;
    }
    ::close(current);
    current = next;
  }
  return current;
}

QString descriptorFilePath(const int directoryDescriptor,
                           const char *fileName) {
  return QStringLiteral("/proc/self/fd/%1/%2")
      .arg(directoryDescriptor)
      .arg(QString::fromLatin1(fileName));
}

bool finalEntryIsRegularOrAbsent(const int directoryDescriptor,
                                 const char *fileName) {
  struct stat status {};
  if (::fstatat(directoryDescriptor, fileName, &status,
                AT_SYMLINK_NOFOLLOW) == 0) {
    return S_ISREG(status.st_mode);
  }
  return errno == ENOENT;
}

} // namespace QindaQt::Apps::TextEditor::StateDirectory
