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
//
// AGENT-CONTRACT: volatileRoles names fields that describe the SNAPSHOT a row
// was projected from rather than the row itself (the task list's
// generationRevision, which every publication advances for every row). A
// volatile field is dropped from the published rowData and never counts as a
// change, so a publication that alters one row emits dataChanged for exactly
// that row and an unchanged row emits nothing. The consumer reads the
// snapshot value from the facade that owns it (TaskListAppletController::
// generationRevision), never from a retained row, which would be stale by
// design. Identity roles are never volatile. See ADR-0390.
class KeyedRowModel : public QAbstractListModel {
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY(QVariantList sourceRows READ sourceRows WRITE setSourceRows NOTIFY sourceRowsChanged)
  Q_PROPERTY(QStringList identityRoles READ identityRoles WRITE setIdentityRoles NOTIFY identityRolesChanged)
  Q_PROPERTY(QStringList volatileRoles READ volatileRoles WRITE setVolatileRoles NOTIFY volatileRolesChanged)
public:
  explicit KeyedRowModel(QObject *parent = nullptr);
  [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
  [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
  [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
  [[nodiscard]] QVariantList sourceRows() const;
  [[nodiscard]] QStringList identityRoles() const;
  [[nodiscard]] QStringList volatileRoles() const;
  void setSourceRows(const QVariantList &rows);
  void setIdentityRoles(const QStringList &roles);
  void setVolatileRoles(const QStringList &roles);
Q_SIGNALS:
  void sourceRowsChanged();
  void identityRolesChanged();
  void volatileRolesChanged();
private:
  struct Row {
    QStringList key;
    QVariantMap value;
  };
  void reconcile();
  [[nodiscard]] QVariantMap published(const QVariantMap &source) const;
  QVariantList m_sourceRows;
  QStringList m_identityRoles;
  QStringList m_volatileRoles;
  QList<Row> m_rows;
};

} // namespace QindaQt::ShellTaskListApplet
