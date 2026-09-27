// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell/task_list/applet/keyed_row_model.h"

#include "qindaqt/shell/task_list/applet/task_list_applet_types.h"

#include <QSet>

namespace QindaQt::ShellTaskListApplet {
namespace {
constexpr int RowDataRole = Qt::UserRole + 1;
}

KeyedRowModel::KeyedRowModel(QObject *parent) : QAbstractListModel(parent) {}

int KeyedRowModel::rowCount(const QModelIndex &parent) const {
  return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QVariant KeyedRowModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()
      || role != RowDataRole)
    return {};
  return m_rows.at(index.row()).value;
}

QHash<int, QByteArray> KeyedRowModel::roleNames() const {
  return {{RowDataRole, QByteArrayLiteral("rowData")}};
}

QVariantList KeyedRowModel::sourceRows() const { return m_sourceRows; }
QStringList KeyedRowModel::identityRoles() const { return m_identityRoles; }

void KeyedRowModel::setSourceRows(const QVariantList &rows) {
  // AGENT-GUARD: upstream owns admission; never retain an unbounded QML list
  // even when declining to present it. A malformed snapshot clears old rows.
  const QVariantList bounded = rows.size() <= kMaxPresentedDockEntries ? rows : QVariantList{};
  if (m_sourceRows == bounded)
    return;
  m_sourceRows = bounded;
  reconcile();
  Q_EMIT sourceRowsChanged();
}

void KeyedRowModel::setIdentityRoles(const QStringList &roles) {
  const QStringList bounded = roles.size() <= 8 ? roles : QStringList{};
  if (m_identityRoles == bounded)
    return;
  m_identityRoles = bounded;
  reconcile();
  Q_EMIT identityRolesChanged();
}

void KeyedRowModel::reconcile() {
  QList<Row> next;
  QSet<QStringList> keys;
  for (const QVariant &value : std::as_const(m_sourceRows)) {
    const QVariantMap map = value.toMap();
    QStringList key;
    bool nonempty = false;
    for (const QString &role : std::as_const(m_identityRoles)) {
      const QString part = map.value(role).toString();
      key.append(part);
      nonempty = nonempty || !part.isEmpty();
    }
    if (!nonempty || keys.contains(key)) {
      next.clear();
      keys.clear();
      break;
    }
    keys.insert(key);
    next.append({key, map});
  }

  // Remove obsolete identities first; all surviving delegates retain their
  // object, hover, focus and in-progress animations through the following diff.
  for (int index = static_cast<int>(m_rows.size()) - 1; index >= 0; --index) {
    if (!keys.contains(m_rows.at(index).key)) {
      beginRemoveRows({}, index, index);
      m_rows.removeAt(index);
      endRemoveRows();
    }
  }
  for (int index = 0; index < next.size(); ++index) {
    int existing = index;
    while (existing < m_rows.size() && m_rows.at(existing).key != next.at(index).key)
      ++existing;
    if (existing == m_rows.size()) {
      beginInsertRows({}, index, index);
      m_rows.insert(index, next.at(index));
      endInsertRows();
    } else {
      if (existing != index) {
        beginMoveRows({}, existing, existing, {}, index);
        m_rows.move(existing, index);
        endMoveRows();
      }
      if (m_rows.at(index).value != next.at(index).value) {
        m_rows[index].value = next.at(index).value;
        Q_EMIT dataChanged(this->index(index), this->index(index), {RowDataRole});
      }
    }
  }
}

} // namespace QindaQt::ShellTaskListApplet
