// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "../core/game.h"

#include <QAbstractListModel>
#include <QSortFilterProxyModel>
#include <QVector>

namespace QindaQt::QindaLutris {

// The merged library as a flat list for the cover grid. Replaced wholesale
// on refresh (libraries are small and bounded by kMaxGames).
class GameListModel final : public QAbstractListModel {
  Q_OBJECT
public:
  enum Role {
    GameIdRole = Qt::UserRole + 1,
    TitleRole,
    SourceIdRole,
    SourceLabelRole,
    CoverUrlRole,
    IconNameRole,
  };
  Q_ENUM(Role)

  explicit GameListModel(QObject *parent = nullptr);

  int rowCount(const QModelIndex &parent = {}) const override;
  QVariant data(const QModelIndex &index, int role) const override;
  QHash<int, QByteArray> roleNames() const override;

  void setGames(const QVector<Game> &games);
  [[nodiscard]] const Game *game(const QString &gameId) const;

private:
  QVector<Game> m_games;
};

// Search text plus one source chip, both fail-open (empty matches all).
class GameFilterModel final : public QSortFilterProxyModel {
  Q_OBJECT
  Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged)
  Q_PROPERTY(QString sourceFilter READ sourceFilter WRITE setSourceFilter NOTIFY sourceFilterChanged)
  Q_PROPERTY(int shownCount READ shownCount NOTIFY shownCountChanged)
public:
  explicit GameFilterModel(QObject *parent = nullptr);

  [[nodiscard]] QString searchText() const { return m_searchText; }
  void setSearchText(const QString &text);
  [[nodiscard]] QString sourceFilter() const { return m_sourceFilter; }
  void setSourceFilter(const QString &sourceId);
  [[nodiscard]] int shownCount() const;

  // Grid needs the stable game id at a proxy row (for select/play).
  Q_INVOKABLE QString gameIdAt(int proxyRow) const;

Q_SIGNALS:
  void searchTextChanged();
  void sourceFilterChanged();
  void shownCountChanged();

protected:
  bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
  QString m_searchText;
  QString m_sourceFilter;
};

} // namespace QindaQt::QindaLutris
