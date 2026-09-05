// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QtTypes>

namespace QindaQt::ShellTaskList::Producer {

// Asynchronous transport seam for the authenticated CompositorShell1 task-fact
// authority. Implementations must bind every signal subscription and method
// call to the exact unique owner passed to requestRefresh(); replies carrying
// a different token or owner are fencing input for the client.
//
// AGENT-CONTRACT: One refresh token covers exactly one TaskListSnapshot read;
// implementations never issue any secondary inventory read or join.
class TaskListProducerTransport : public QObject {
  Q_OBJECT

public:
  using QObject::QObject;
  ~TaskListProducerTransport() override = default;

  [[nodiscard]] virtual bool start(QString *error = nullptr) = 0;
  virtual void stop() = 0;
  virtual void requestRefresh(quint64 token, const QString &uniqueOwner) = 0;

Q_SIGNALS:
  // Empty owner means the well-known name is currently unowned.
  void serviceOwnerChanged(const QString &uniqueOwner);
  // TaskListSnapshotChanged is an invalidation hint for a complete re-read.
  void refreshInvalidated(const QString &uniqueOwner);
  void factsRead(quint64 token, const QString &uniqueOwner,
                 const QByteArray &payload);
  void refreshFailed(quint64 token, const QString &uniqueOwner,
                     const QString &message);
};

} // namespace QindaQt::ShellTaskList::Producer
