// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QAbstractListModel>
#include <QStringList>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

namespace QindaQt::ShellTaskListApplet {

// GUI-thread presentation adapter for bounded, authoritative row snapshots.
// Owns copied values only; no authority, persistence, or filesystem access.
// identityRoles names immutable string fields (at least one nonempty per row).
// Invalid/duplicate identities or >4096 rows clear presentation fail-closed.
// Consumers receive one rowData map role; retained keys emit dataChanged,
// insert/remove/move signals preserve delegate lifetime (ADR-0278).
class KeyedRowModel : public QAbstractListModel {
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY(QVariantList sourceRows READ sourceRows WRITE setSourceRows NOTIFY sourceRowsChanged)
  Q_PROPERTY(QStringList identityRoles READ identityRoles WRITE setIdentityRoles NOTIFY identityRolesChanged)
public:
  explicit KeyedRowModel(QObject *parent = nullptr);
  [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
  [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
  [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
  [[nodiscard]] QVariantList sourceRows() const;
  [[nodiscard]] QStringList identityRoles() const;
  void setSourceRows(const QVariantList &rows);
  void setIdentityRoles(const QStringList &roles);
Q_SIGNALS:
  void sourceRowsChanged();
  void identityRolesChanged();
private:
  struct Row {
    QStringList key;
    QVariantMap value;
  };
  void reconcile();
  QVariantList m_sourceRows;
  QStringList m_identityRoles;
  QList<Row> m_rows;
};

} // namespace QindaQt::ShellTaskListApplet
