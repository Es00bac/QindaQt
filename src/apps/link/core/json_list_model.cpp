// SPDX-License-Identifier: GPL-3.0-or-later
#include "json_list_model.h"

namespace QindaQt::LinkApp {

JsonListModel::JsonListModel(const QStringList &roles, QObject *parent) : QAbstractListModel(parent)
{
    int role = Qt::UserRole + 1;
    for (const QString &name : roles) {
        m_roleNames.insert(role, name.toUtf8());
        m_roleByName.insert(name, role);
        ++role;
    }
}

int JsonListModel::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : int(m_rows.size()); }

QVariant JsonListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size() || !m_roleNames.contains(role)) {
        return {};
    }
    return m_rows.at(index.row()).value(QString::fromUtf8(m_roleNames.value(role)));
}

QVariantMap JsonListModel::get(int row) const
{
    return row >= 0 && row < m_rows.size() ? m_rows.at(row) : QVariantMap();
}

void JsonListModel::setRows(QList<QVariantMap> rows)
{
    const int before = count();
    beginResetModel();
    m_rows = std::move(rows);
    endResetModel();
    if (before != count()) Q_EMIT countChanged();
}

void JsonListModel::append(const QList<QVariantMap> &rows)
{
    if (rows.isEmpty()) return;
    beginInsertRows({}, int(m_rows.size()), int(m_rows.size() + rows.size() - 1));
    m_rows += rows;
    endInsertRows();
    Q_EMIT countChanged();
}

void JsonListModel::prepend(const QList<QVariantMap> &rows)
{
    if (rows.isEmpty()) return;
    beginInsertRows({}, 0, int(rows.size() - 1));
    m_rows = rows + m_rows;
    endInsertRows();
    Q_EMIT countChanged();
}

void JsonListModel::replaceAt(int row, const QVariantMap &value)
{
    if (row < 0 || row >= m_rows.size()) return;
    m_rows[row] = value;
    Q_EMIT dataChanged(index(row), index(row));
}

void JsonListModel::removeAt(int row)
{
    if (row < 0 || row >= m_rows.size()) return;
    beginRemoveRows({}, row, row);
    m_rows.removeAt(row);
    endRemoveRows();
    Q_EMIT countChanged();
}

void JsonListModel::updateAt(int row, const QVariantMap &changes)
{
    if (row < 0 || row >= m_rows.size()) return;
    QList<int> roles;
    for (auto it = changes.cbegin(); it != changes.cend(); ++it) {
        m_rows[row].insert(it.key(), it.value());
        if (m_roleByName.contains(it.key())) roles.append(m_roleByName.value(it.key()));
    }
    Q_EMIT dataChanged(index(row), index(row), roles);
}

int JsonListModel::indexOf(const QString &role, const QVariant &value) const
{
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows.at(i).value(role) == value) return i;
    }
    return -1;
}

} // namespace QindaQt::LinkApp
