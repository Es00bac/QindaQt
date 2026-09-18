// SPDX-License-Identifier: GPL-3.0-or-later
#include "bookmarks_store.h"

#include "state_file.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QStringList>

#include <utility>

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

[[nodiscard]] StateFile stateFileFor(const QString &directory) {
  return StateFile(directory, QByteArray(bookmarksFileName),
                   BookmarksStore::maximumBytes);
}

// AGENT-GUARD: these two translations are the store's whole error surface.
// Keep the wording stable -- tst_bookmarks_store.cpp and the QML banner both
// read it, and Absent must stay diagnostic-free so a first run shows no
// error.
[[nodiscard]] BookmarksLoadResult loadFailureFor(const StateFile::ReadResult &read) {
  switch (read.error) {
  case StateFile::Error::Absent:
    return loadFailure(BookmarksError::Absent, QString());
  case StateFile::Error::InvalidRoot:
    return loadFailure(BookmarksError::InvalidRoot,
                       QStringLiteral("Bookmark state root is unsafe"));
  case StateFile::Error::NotRegular:
    return loadFailure(BookmarksError::Malformed,
                       QStringLiteral("Bookmark state is not a regular file"));
  case StateFile::Error::TooLarge:
    return loadFailure(BookmarksError::TooLarge,
                       QStringLiteral("Bookmark state exceeds 64 KiB"));
  case StateFile::Error::ReadFailed:
  case StateFile::Error::WriteFailed:
  case StateFile::Error::None:
    break;
  }
  return loadFailure(BookmarksError::ReadFailed, read.systemDiagnostic);
}

[[nodiscard]] BookmarksWriteResult
writeFailureFor(const StateFile::WriteResult &written) {
  switch (written.error) {
  case StateFile::Error::InvalidRoot:
    return writeFailure(
        BookmarksError::InvalidRoot,
        QStringLiteral("Bookmark state directory is unavailable or unsafe"));
  case StateFile::Error::NotRegular:
    return writeFailure(BookmarksError::InvalidRoot,
                        QStringLiteral("Bookmark state target is unsafe"));
  case StateFile::Error::TooLarge:
    return writeFailure(BookmarksError::TooLarge,
                        QStringLiteral("Bookmark state exceeds 64 KiB"));
  case StateFile::Error::WriteFailed:
  case StateFile::Error::ReadFailed:
  case StateFile::Error::Absent:
  case StateFile::Error::None:
    break;
  }
  return writeFailure(BookmarksError::WriteFailed, written.systemDiagnostic);
}

} // namespace

BookmarksStore::BookmarksStore(QString stateDirectory)
    : m_stateDirectory(QDir::cleanPath(std::move(stateDirectory))) {}

QString BookmarksStore::filePath() const {
  return stateFileFor(m_stateDirectory).filePath();
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
  const StateFile::ReadResult read = stateFileFor(m_stateDirectory).read();
  if (!read.ok()) {
    return loadFailureFor(read);
  }

  QJsonParseError parseError;
  const QJsonDocument document = QJsonDocument::fromJson(read.bytes, &parseError);
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
  QJsonArray entries;
  for (const Bookmark &bookmark : bookmarks) {
    entries.append(QJsonObject{{QStringLiteral("name"), bookmark.name},
                               {QStringLiteral("path"), bookmark.path}});
  }
  const QJsonDocument document(QJsonObject{
      {QStringLiteral("version"), 1},
      {QStringLiteral("bookmarks"), entries},
  });
  const StateFile::WriteResult written =
      stateFileFor(m_stateDirectory)
          .write(document.toJson(QJsonDocument::Compact));
  if (!written.ok()) {
    return writeFailureFor(written);
  }
  return {};
}

} // namespace QindaQt::Apps::FileManager
