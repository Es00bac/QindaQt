// SPDX-License-Identifier: GPL-3.0-or-later
#include "places_controller.h"

#include <QDir>
#include <QStandardPaths>
#include <QVariantMap>

namespace QindaQt::Apps::FileManager {

namespace {

[[nodiscard]] QVariantMap placeMap(const QString &id, const QString &name,
                                   const QString &path) {
  return QVariantMap{{QStringLiteral("id"), id},
                     {QStringLiteral("name"), name},
                     {QStringLiteral("path"), path}};
}

[[nodiscard]] QVariantMap bookmarkMap(const Bookmark &bookmark, int index) {
  return QVariantMap{{QStringLiteral("name"), bookmark.name},
                     {QStringLiteral("path"), bookmark.path},
                     {QStringLiteral("index"), index}};
}

} // namespace

PlacesController::PlacesController(std::unique_ptr<BookmarksStore> store,
                                   QObject *parent)
    : QObject(parent), m_store(std::move(store)) {
  Q_ASSERT(m_store);
  const BookmarksLoadResult loaded = m_store->load();
  if (loaded.ok()) {
    m_bookmarks = loaded.bookmarks;
  } else if (loaded.error != BookmarksError::Absent) {
    m_storeError = loaded.diagnostic.isEmpty()
                       ? QStringLiteral("Bookmarks could not be read")
                       : loaded.diagnostic;
  }
}

QVariantList PlacesController::places() const {
  QVariantList list;
  list.append(placeMap(QStringLiteral("home"), QStringLiteral("Home"),
                       QDir::homePath()));
  list.append(placeMap(QStringLiteral("root"), QStringLiteral("File System"),
                       QStringLiteral("/")));
  const QString dataHome =
      QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
  if (!dataHome.isEmpty()) {
    list.append(placeMap(QStringLiteral("trash"), QStringLiteral("Trash"),
                         QDir(dataHome).filePath(QStringLiteral("Trash/files"))));
  }
  return list;
}

QVariantList PlacesController::bookmarks() const {
  QVariantList list;
  list.reserve(m_bookmarks.size());
  for (qsizetype i = 0; i < m_bookmarks.size(); ++i) {
    list.append(bookmarkMap(m_bookmarks.at(i), static_cast<int>(i)));
  }
  return list;
}

QString PlacesController::storeError() const { return m_storeError; }

QVector<Bookmark> PlacesController::bookmarkValues() const {
  return m_bookmarks;
}

void PlacesController::addBookmark(const QString &name, const QString &path) {
  const QString cleaned = QDir::cleanPath(path);
  const QString trimmedName = name.trimmed();
  if (trimmedName.isEmpty() || !QDir::isAbsolutePath(cleaned)) {
    m_storeError = QStringLiteral("Choose a valid bookmark name and folder");
    emit storeErrorChanged();
    return;
  }
  for (const Bookmark &bookmark : m_bookmarks) {
    if (bookmark.path == cleaned) {
      return; // already bookmarked: no duplicate, no error
    }
  }
  if (m_bookmarks.size() >= BookmarksStore::maximumBookmarks) {
    m_storeError = QStringLiteral("The bookmark list is full");
    emit storeErrorChanged();
    return;
  }
  m_bookmarks.append(Bookmark{trimmedName, cleaned});
  persist();
  emit bookmarksChanged();
}

void PlacesController::removeBookmark(int index) {
  if (index < 0 || index >= m_bookmarks.size()) {
    return;
  }
  m_bookmarks.removeAt(index);
  persist();
  emit bookmarksChanged();
}

void PlacesController::clearStoreError() {
  if (m_storeError.isEmpty()) {
    return;
  }
  m_storeError.clear();
  emit storeErrorChanged();
}

void PlacesController::persist() {
  const BookmarksWriteResult written = m_store->store(m_bookmarks);
  if (written.ok()) {
    if (!m_storeError.isEmpty()) {
      m_storeError.clear();
      emit storeErrorChanged();
    }
    return;
  }
  m_storeError = written.diagnostic.isEmpty()
                     ? QStringLiteral("Bookmarks could not be saved")
                     : written.diagnostic;
  emit storeErrorChanged();
}

} // namespace QindaQt::Apps::FileManager
