// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtCore/QAbstractListModel>
#include <QtCore/QVariantMap>

namespace QindaQt::LinkApp {

// A list model over maps, with the role names fixed at construction. It is the one model type the
// app's lists (threads, messages, apps, pairings, steps) share, so QML sees plain role names and
// the view models keep their rows as QVariantMap.
//
// Ownership: QObject parent. Threading: GUI thread. Error behavior: out-of-range access returns
// an empty map / invalid variant; nothing throws.
// AGENT-GUARD: rows hold personal content (message bodies, names); never print a row or role value.
class JsonListModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged FINAL)
public:
    explicit JsonListModel(const QStringList &roles, QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override { return m_roleNames; }
    [[nodiscard]] int count() const { return int(m_rows.size()); }
    [[nodiscard]] const QList<QVariantMap> &rows() const { return m_rows; }

    Q_INVOKABLE QVariantMap get(int row) const;
    void setRows(QList<QVariantMap> rows);
    void append(const QList<QVariantMap> &rows);
    void prepend(const QList<QVariantMap> &rows);
    void replaceAt(int row, const QVariantMap &value);
    void removeAt(int row);
    // Merges `changes` into row `row` (only the named roles change).
    void updateAt(int row, const QVariantMap &changes);
    // First row whose `role` equals `value`, else -1.
    [[nodiscard]] int indexOf(const QString &role, const QVariant &value) const;

Q_SIGNALS:
    void countChanged();

private:
    QHash<int, QByteArray> m_roleNames;
    QHash<QString, int> m_roleByName;
    QList<QVariantMap> m_rows;
};

} // namespace QindaQt::LinkApp
