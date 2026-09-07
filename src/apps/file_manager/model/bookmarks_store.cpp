// SPDX-License-Identifier: GPL-3.0-or-later
#include "bookmarks_store.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>

#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace QindaQt::Apps::FileManager {
namespace {

constexpr auto bookmarksFileName = "bookmarks-v1.json";

BookmarksLoadResult loadFailure(const BookmarksError error,
                                const QString &diagnostic) {
  BookmarksLoadResult result;
  result.error = error;
  result.diagnostic = diagnostic.left(256);
  return result;
}

BookmarksWriteResult writeFailure(const BookmarksError error,
                                  const QString &diagnostic) {
  return {.error = error, .diagnostic = diagnostic.left(256)};
}

int openStateDirectory(const QString &path, const bool create,
                       int *errorNumber) {
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

QString descriptorFilePath(const int directoryDescriptor) {
  return QStringLiteral("/proc/self/fd/%1/%2")
      .arg(directoryDescriptor)
      .arg(QString::fromLatin1(bookmarksFileName));
}

bool finalEntryIsRegularOrAbsent(const int directoryDescriptor) {
  struct stat status {};
  if (::fstatat(directoryDescriptor, bookmarksFileName, &status,
                AT_SYMLINK_NOFOLLOW) == 0) {
    return S_ISREG(status.st_mode);
  }
  return errno == ENOENT;
}

} // namespace

BookmarksStore::BookmarksStore(QString stateDirectory)
    : m_stateDirectory(QDir::cleanPath(std::move(stateDirectory))) {}

QString BookmarksStore::filePath() const {
  return QDir(m_stateDirectory)
      .filePath(QString::fromLatin1(bookmarksFileName));
}

bool BookmarksStore::validate(const QVector<Bookmark> &bookmarks,
                              QString *diagnostic) {
  if (bookmarks.size() > maximumBookmarks) {
    *diagnostic = QStringLiteral("Bookmark inventory contains too many entries");
    return false;
  }
  QSet<QString> uniquePaths;
  for (const Bookmark &bookmark : bookmarks) {
    const QString cleaned = QDir::cleanPath(bookmark.path);
    if (bookmark.path.size() > maximumPathLength ||
        !bookmark.path.isValidUtf16() || bookmark.path.contains(QChar::Null) ||
        !QDir::isAbsolutePath(bookmark.path) || bookmark.path != cleaned ||
        uniquePaths.contains(cleaned)) {
      *diagnostic = QStringLiteral("Bookmark inventory contains an invalid path");
      return false;
    }
    if (bookmark.name.isEmpty() || bookmark.name.size() > maximumNameLength ||
        !bookmark.name.isValidUtf16() || bookmark.name.contains(QChar::Null)) {
      *diagnostic = QStringLiteral("Bookmark inventory contains an invalid name");
      return false;
    }
    uniquePaths.insert(cleaned);
  }
  return true;
}

BookmarksLoadResult BookmarksStore::load() const {
  int directoryError = 0;
  const int directoryDescriptor =
      openStateDirectory(m_stateDirectory, false, &directoryError);
  if (directoryDescriptor < 0) {
    return loadFailure(directoryError == ENOENT
                           ? BookmarksError::Absent
                           : BookmarksError::InvalidRoot,
                       directoryError == ENOENT
                           ? QString()
                           : QStringLiteral("Bookmark state root is unsafe"));
  }
  const int descriptor =
      ::openat(directoryDescriptor, bookmarksFileName,
               O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
  const int openError = errno;
  ::close(directoryDescriptor);
  if (descriptor < 0) {
    return loadFailure(
        openError == ENOENT ? BookmarksError::Absent : BookmarksError::Malformed,
        openError == ENOENT
            ? QString()
            : QStringLiteral("Bookmark state is not a regular file"));
  }
  struct stat status {};
  if (::fstat(descriptor, &status) != 0 || !S_ISREG(status.st_mode)) {
    ::close(descriptor);
    return loadFailure(BookmarksError::Malformed,
                       QStringLiteral("Bookmark state is not a regular file"));
  }
  if (status.st_size > maximumBytes) {
    ::close(descriptor);
    return loadFailure(BookmarksError::TooLarge,
                       QStringLiteral("Bookmark state exceeds 64 KiB"));
  }
  QFile file;
  if (!file.open(descriptor, QIODevice::ReadOnly, QFileDevice::AutoCloseHandle)) {
    ::close(descriptor);
    return loadFailure(BookmarksError::ReadFailed, file.errorString());
  }
  const QByteArray bytes = file.read(maximumBytes + 1);
  if (file.error() != QFileDevice::NoError) {
    return loadFailure(BookmarksError::ReadFailed, file.errorString());
  }
  if (bytes.size() > maximumBytes) {
    return loadFailure(BookmarksError::TooLarge,
                       QStringLiteral("Bookmark state exceeds 64 KiB"));
  }

  QJsonParseError parseError;
  const QJsonDocument document = QJsonDocument::fromJson(bytes, &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
    return loadFailure(BookmarksError::Malformed,
                       QStringLiteral("Bookmark state is malformed JSON"));
  }
  const QJsonObject object = document.object();
  const QSet<QString> expectedKeys{QStringLiteral("version"),
                                   QStringLiteral("bookmarks")};
  const QStringList objectKeys = object.keys();
  if (QSet<QString>(objectKeys.begin(), objectKeys.end()) != expectedKeys ||
      object.value(QStringLiteral("version")).toDouble() != 1.0 ||
      !object.value(QStringLiteral("bookmarks")).isArray()) {
    return loadFailure(BookmarksError::Malformed,
                       QStringLiteral("Bookmark state has an invalid schema"));
  }
  QVector<Bookmark> bookmarks;
  const QJsonArray entries = object.value(QStringLiteral("bookmarks")).toArray();
  if (entries.size() > maximumBookmarks) {
    return loadFailure(BookmarksError::Malformed,
                       QStringLiteral("Bookmark state contains too many entries"));
  }
  for (const QJsonValue &value : entries) {
    if (!value.isObject()) {
      return loadFailure(BookmarksError::Malformed,
                         QStringLiteral("Bookmark entry is not an object"));
    }
    const QJsonObject entry = value.toObject();
    const QJsonValue name = entry.value(QStringLiteral("name"));
    const QJsonValue path = entry.value(QStringLiteral("path"));
    if (!name.isString() || !path.isString()) {
      return loadFailure(BookmarksError::Malformed,
                         QStringLiteral("Bookmark entry has an invalid shape"));
    }
    bookmarks.append(Bookmark{name.toString(), path.toString()});
  }
  QString diagnostic;
  if (!validate(bookmarks, &diagnostic)) {
    return loadFailure(BookmarksError::Malformed, diagnostic);
  }
  BookmarksLoadResult result;
  result.bookmarks = std::move(bookmarks);
  return result;
}

BookmarksWriteResult
BookmarksStore::store(const QVector<Bookmark> &bookmarks) const {
  QString diagnostic;
  if (!validate(bookmarks, &diagnostic)) {
    return writeFailure(BookmarksError::Malformed, diagnostic);
  }
  int directoryError = 0;
  const int directoryDescriptor =
      openStateDirectory(m_stateDirectory, true, &directoryError);
  if (directoryDescriptor < 0) {
    return writeFailure(
        BookmarksError::InvalidRoot,
        QStringLiteral("Bookmark state directory is unavailable or unsafe"));
  }

  QJsonArray entries;
  for (const Bookmark &bookmark : bookmarks) {
    entries.append(QJsonObject{{QStringLiteral("name"), bookmark.name},
                               {QStringLiteral("path"), bookmark.path}});
  }
  const QJsonDocument document(QJsonObject{
      {QStringLiteral("version"), 1},
      {QStringLiteral("bookmarks"), entries},
  });
  const QByteArray bytes = document.toJson(QJsonDocument::Compact);
  if (bytes.size() > maximumBytes) {
    ::close(directoryDescriptor);
    return writeFailure(BookmarksError::TooLarge,
                        QStringLiteral("Bookmark state exceeds 64 KiB"));
  }
  // AGENT-GUARD: The directory descriptor is reached component-by-component
  // with O_NOFOLLOW. Keep it open through commit so a symlinked ancestor can
  // never redirect the inventory outside the selected state root.
  if (!finalEntryIsRegularOrAbsent(directoryDescriptor)) {
    ::close(directoryDescriptor);
    return writeFailure(BookmarksError::InvalidRoot,
                        QStringLiteral("Bookmark state target is unsafe"));
  }
  QSaveFile file(descriptorFilePath(directoryDescriptor));
  file.setDirectWriteFallback(false);
  if (!file.open(QIODevice::WriteOnly) ||
      !file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner) ||
      file.write(bytes) != bytes.size()) {
    file.cancelWriting();
    ::close(directoryDescriptor);
    return writeFailure(BookmarksError::WriteFailed, file.errorString());
  }
  if (!file.commit()) {
    ::close(directoryDescriptor);
    return writeFailure(BookmarksError::WriteFailed, file.errorString());
  }
  ::close(directoryDescriptor);
  return {};
}

} // namespace QindaQt::Apps::FileManager
