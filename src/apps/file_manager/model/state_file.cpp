// SPDX-License-Identifier: GPL-3.0-or-later
#include "state_file.h"

#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QStringList>

#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <utility>

namespace QindaQt::Apps::FileManager {
namespace {

// Walks the absolute directory chain one component at a time with
// O_NOFOLLOW so a symlinked ancestor cannot redirect the state file. When
// create is true a missing component is made 0700 and reopened.
int openStateDirectory(const QString &path, const bool create, int *errorNumber) {
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

// QSaveFile needs a path, and the only path that still names the descriptor
// the O_NOFOLLOW walk proved safe is the /proc one.
QString descriptorFilePath(const int directoryDescriptor, const QByteArray &fileName) {
  return QStringLiteral("/proc/self/fd/%1/%2")
      .arg(directoryDescriptor)
      .arg(QString::fromLatin1(fileName));
}

bool finalEntryIsRegularOrAbsent(const int directoryDescriptor,
                                 const QByteArray &fileName) {
  struct stat status {};
  if (::fstatat(directoryDescriptor, fileName.constData(), &status,
                AT_SYMLINK_NOFOLLOW) == 0) {
    return S_ISREG(status.st_mode);
  }
  return errno == ENOENT;
}

StateFile::ReadResult readFailure(const StateFile::Error error,
                                  const QString &systemDiagnostic = {}) {
  return {.bytes = {}, .error = error, .systemDiagnostic = systemDiagnostic};
}

} // namespace

StateFile::StateFile(QString directory, QByteArray fileName, qint64 maximumBytes)
    : m_directory(QDir::cleanPath(std::move(directory))),
      m_fileName(std::move(fileName)), m_maximumBytes(maximumBytes) {}

QString StateFile::filePath() const {
  return QDir(m_directory).filePath(QString::fromLatin1(m_fileName));
}

StateFile::ReadResult StateFile::read() const {
  int directoryError = 0;
  const int directoryDescriptor =
      openStateDirectory(m_directory, false, &directoryError);
  if (directoryDescriptor < 0) {
    return readFailure(directoryError == ENOENT ? Error::Absent : Error::InvalidRoot);
  }
  const int descriptor = ::openat(directoryDescriptor, m_fileName.constData(),
                                  O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
  const int openError = errno;
  ::close(directoryDescriptor);
  if (descriptor < 0) {
    return readFailure(openError == ENOENT ? Error::Absent : Error::NotRegular);
  }
  struct stat status {};
  if (::fstat(descriptor, &status) != 0 || !S_ISREG(status.st_mode)) {
    ::close(descriptor);
    return readFailure(Error::NotRegular);
  }
  if (status.st_size > m_maximumBytes) {
    ::close(descriptor);
    return readFailure(Error::TooLarge);
  }
  QFile file;
  if (!file.open(descriptor, QIODevice::ReadOnly, QFileDevice::AutoCloseHandle)) {
    ::close(descriptor);
    return readFailure(Error::ReadFailed, file.errorString());
  }
  const QByteArray bytes = file.read(m_maximumBytes + 1);
  if (file.error() != QFileDevice::NoError) {
    return readFailure(Error::ReadFailed, file.errorString());
  }
  if (bytes.size() > m_maximumBytes) {
    return readFailure(Error::TooLarge);
  }
  return {.bytes = bytes, .error = Error::None, .systemDiagnostic = {}};
}

StateFile::WriteResult StateFile::write(const QByteArray &bytes) const {
  int directoryError = 0;
  const int directoryDescriptor =
      openStateDirectory(m_directory, true, &directoryError);
  if (directoryDescriptor < 0) {
    return {.error = Error::InvalidRoot, .systemDiagnostic = {}};
  }
  // AGENT-NOTE: the size bound is checked after the root is proven, not
  // before: an unsafe state root is the more important refusal to report
  // when an oversized payload would also have been rejected.
  if (bytes.size() > m_maximumBytes) {
    ::close(directoryDescriptor);
    return {.error = Error::TooLarge, .systemDiagnostic = {}};
  }
  // AGENT-GUARD: keep the directory descriptor open through commit so a
  // symlinked ancestor can never redirect the write outside the root.
  if (!finalEntryIsRegularOrAbsent(directoryDescriptor, m_fileName)) {
    ::close(directoryDescriptor);
    return {.error = Error::NotRegular, .systemDiagnostic = {}};
  }
  QSaveFile file(descriptorFilePath(directoryDescriptor, m_fileName));
  file.setDirectWriteFallback(false);
  if (!file.open(QIODevice::WriteOnly) ||
      !file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner) ||
      file.write(bytes) != bytes.size()) {
    file.cancelWriting();
    ::close(directoryDescriptor);
    return {.error = Error::WriteFailed, .systemDiagnostic = file.errorString()};
  }
  if (!file.commit()) {
    ::close(directoryDescriptor);
    return {.error = Error::WriteFailed, .systemDiagnostic = file.errorString()};
  }
  ::close(directoryDescriptor);
  return {};
}

} // namespace QindaQt::Apps::FileManager
