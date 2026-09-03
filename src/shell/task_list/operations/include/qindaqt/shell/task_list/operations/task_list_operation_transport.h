// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QtTypes>

namespace QindaQt::ShellTaskList::Operations {

// Asynchronous mutation seam for the public org.qindaqt.Compositor1
// authority. Implementations send each call to the exact unique owner passed
// by the adapter.
//
// AGENT-CONTRACT: A method returning false means the request never left the
// process (TransportFailure). Once a call returns true, exactly one terminal
// signal may follow for that token; any bus-level failure after sending is
// operationFailed and the adapter reports it as Uncertain because the
// transaction may still have committed.
class TaskListOperationTransport : public QObject {
  Q_OBJECT

public:
  using QObject::QObject;
  ~TaskListOperationTransport() override = default;

  // Allocates lineage across every adapter sharing this transport. Pending
  // calls are transport-owned, so the transport is the smallest lifetime that
  // can guarantee a destroyed adapter's token is never recycled.
  [[nodiscard]] virtual quint64 allocateToken() = 0;

  virtual bool submitTransaction(quint64 token, const QString &uniqueOwner,
                                 const QByteArray &requestJson) = 0;
  virtual bool releaseContainer(quint64 token, const QString &uniqueOwner,
                                const QString &containerId) = 0;
  virtual bool dockWindows(quint64 token, const QString &uniqueOwner,
                           const QString &targetWindowId,
                           const QString &incomingWindowId,
                           const QString &orientation, const QString &position,
                           double ratio) = 0;

Q_SIGNALS:
  void operationReplied(quint64 token, const QString &uniqueOwner,
                        const QByteArray &payload);
  void operationFailed(quint64 token, const QString &uniqueOwner,
                       const QString &message);
};

} // namespace QindaQt::ShellTaskList::Operations
