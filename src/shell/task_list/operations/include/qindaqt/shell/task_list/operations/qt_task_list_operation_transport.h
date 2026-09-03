// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell/task_list/operations/task_list_operation_transport.h"

#include <QDBusConnection>
#include <QList>

class QDBusPendingCallWatcher;

namespace QindaQt::ShellTaskList::Operations {

// QtDBus sender for the operation seam on an injected bus connection. It owns
// no name watching itself: the facts producer supplies the exact unique owner
// at admission time, and every call is fenced by (token, owner) at reply.
class QtTaskListOperationTransport final : public TaskListOperationTransport {
  Q_OBJECT

public:
  explicit QtTaskListOperationTransport(QDBusConnection connection,
                                        QObject *parent = nullptr);
  ~QtTaskListOperationTransport() override;

  [[nodiscard]] quint64 allocateToken() override;

  bool submitTransaction(quint64 token, const QString &uniqueOwner,
                         const QByteArray &requestJson) override;
  bool releaseContainer(quint64 token, const QString &uniqueOwner,
                        const QString &containerId) override;
  bool dockWindows(quint64 token, const QString &uniqueOwner,
                   const QString &targetWindowId,
                   const QString &incomingWindowId, const QString &orientation,
                   const QString &position, double ratio) override;

private:
  bool issueCall(quint64 token, const QString &uniqueOwner,
                 const QDBusMessage &message);

  QDBusConnection m_connection;
  QList<QDBusPendingCallWatcher *> m_pendingCalls;
  quint64 m_nextToken = 1;
};

} // namespace QindaQt::ShellTaskList::Operations
