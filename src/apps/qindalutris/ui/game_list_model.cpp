// SPDX-License-Identifier: GPL-3.0-or-later
#include "game_list_model.h"

namespace QindaQt::QindaLutris {
namespace {

QString sourceLabel(GameSource source) {
  switch (source) {
  case GameSource::Steam: return QStringLiteral("Steam");
  case GameSource::Lutris: return QStringLiteral("Lutris");
  case GameSource::Desktop: return QStringLiteral("Native");
  case GameSource::Wine: return QStringLiteral("Wine");
  }
  Q_UNREACHABLE();
}

} // namespace

GameListModel::GameListModel(QObject *parent) : QAbstractListModel(parent) {}

int GameListModel::rowCount(const QModelIndex &parent) const {
  return parent.isValid() ? 0 : int(m_games.size());
}

QVariant GameListModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() >= m_games.size()) {
    return {};
  }
  const Game &game = m_games.at(index.row());
  switch (role) {
  case GameIdRole: return game.id;
  case TitleRole: return game.title;
  case SourceIdRole: return gameSourceId(game.source);
  case SourceLabelRole: return sourceLabel(game.source);
  case CoverUrlRole:
    // No art exists at all -> empty source (the tile's placeholder glyph);
    // art that exists but fails to decode is the tile's honest alert.
    if (game.coverPath.isEmpty() && game.iconName.isEmpty()) {
      return {};
    }
    return QStringLiteral("image://gameicon/") + game.id;
  case IconNameRole: return game.iconName;
  }
  return {};
}

QHash<int, QByteArray> GameListModel::roleNames() const {
  return {{GameIdRole, "gameId"},       {TitleRole, "title"},
          {SourceIdRole, "sourceId"},   {SourceLabelRole, "sourceLabel"},
          {CoverUrlRole, "coverUrl"},   {IconNameRole, "iconName"}};
}

void GameListModel::setGames(const QVector<Game> &games) {
  beginResetModel();
  m_games = games;
  endResetModel();
}

const Game *GameListModel::game(const QString &gameId) const {
  for (const Game &game : m_games) {
    if (game.id == gameId) {
      return &game;
    }
  }
  return nullptr;
}

GameFilterModel::GameFilterModel(QObject *parent) : QSortFilterProxyModel(parent) {
  setFilterCaseSensitivity(Qt::CaseInsensitive);
  connect(this, &QSortFilterProxyModel::rowsInserted, this,
          &GameFilterModel::shownCountChanged);
  connect(this, &QSortFilterProxyModel::rowsRemoved, this,
          &GameFilterModel::shownCountChanged);
  connect(this, &QSortFilterProxyModel::modelReset, this,
          &GameFilterModel::shownCountChanged);
}

void GameFilterModel::setSearchText(const QString &text) {
  const QString bounded = text.left(kMaxGameTitleChars);
  if (m_searchText == bounded) {
    return;
  }
  m_searchText = bounded;
  beginFilterChange();
  endFilterChange();
  Q_EMIT searchTextChanged();
}

void GameFilterModel::setSourceFilter(const QString &sourceId) {
  if (m_sourceFilter == sourceId) {
    return;
  }
  m_sourceFilter = sourceId;
  beginFilterChange();
  endFilterChange();
  Q_EMIT sourceFilterChanged();
}

int GameFilterModel::shownCount() const {
  return rowCount();
}

QString GameFilterModel::gameIdAt(int proxyRow) const {
  const QModelIndex proxy = index(proxyRow, 0);
  if (!proxy.isValid()) {
    return {};
  }
  return mapToSource(proxy).data(GameListModel::GameIdRole).toString();
}

bool GameFilterModel::filterAcceptsRow(int sourceRow,
                                       const QModelIndex &sourceParent) const {
  const QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
  if (!m_sourceFilter.isEmpty()
      && index.data(GameListModel::SourceIdRole).toString() != m_sourceFilter) {
    return false;
  }
  if (!m_searchText.isEmpty()
      && !index.data(GameListModel::TitleRole).toString().contains(
          m_searchText, Qt::CaseInsensitive)) {
    return false;
  }
  return true;
}

} // namespace QindaQt::QindaLutris
