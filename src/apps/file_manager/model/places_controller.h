// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "bookmarks_store.h"

#include <QObject>
#include <QVariantList>

#include <memory>

namespace QindaQt::Apps::FileManager {

// AGENT-CONTRACT: This GUI-thread QObject owns the injected BookmarksStore
// and the fixed places list for one window. It publishes complete
// places/bookmarks snapshots plus a typed store-error string; it never
// navigates, lists directories, or shows UI. QML activates a place by calling
// NavigationController::navigateTo() with the published path, so a vanished
// bookmark lands on the existing navigation state card instead of a second
// error channel.
class PlacesController final : public QObject {
  Q_OBJECT

  Q_PROPERTY(QVariantList places READ places NOTIFY placesChanged FINAL)
  Q_PROPERTY(QVariantList bookmarks READ bookmarks NOTIFY bookmarksChanged FINAL)
  Q_PROPERTY(QString storeError READ storeError NOTIFY storeErrorChanged FINAL)

public:
  PlacesController(std::unique_ptr<BookmarksStore> store,
                   QObject *parent = nullptr);

  Q_INVOKABLE void addBookmark(const QString &name, const QString &path);
  Q_INVOKABLE void removeBookmark(int index);
  Q_INVOKABLE void clearStoreError();

  [[nodiscard]] QVariantList places() const;
  [[nodiscard]] QVariantList bookmarks() const;
  [[nodiscard]] QString storeError() const;

  // Test seam independent of QML's QVariantList marshalling.
  [[nodiscard]] QVector<Bookmark> bookmarkValues() const;

signals:
  void placesChanged();
  void bookmarksChanged();
  void storeErrorChanged();

private:
  void persist();

  std::unique_ptr<BookmarksStore> m_store;
  QVector<Bookmark> m_bookmarks;
  QString m_storeError;
};

} // namespace QindaQt::Apps::FileManager
