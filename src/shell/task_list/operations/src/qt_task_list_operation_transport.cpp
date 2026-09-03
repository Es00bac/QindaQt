// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell/task_list/operations/qt_task_list_operation_transport.h"

#include <QDBusError>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>

namespace QindaQt::ShellTaskList::Operations {
namespace {

constexpr auto kObjectPath = "/org/qindaqt/Compositor";
constexpr auto kInterfaceName = "org.qindaqt.Compositor1";
constexpr int kDBusTimeoutMilliseconds = 5000;

} // namespace

QtTaskListOperationTransport::QtTaskListOperationTransport(
    QDBusConnection connection, QObject *parent)
    : TaskListOperationTransport(parent), m_connection(std::move(connection)) {}

QtTaskListOperationTransport::~QtTaskListOperationTransport() {
  for (auto *pending : std::as_const(m_pendingCalls)) {
    if (pending) {
      pending->disconnect(this);
      pending->deleteLater();
    }
  }
  m_pendingCalls.clear();
}

bool QtTaskListOperationTransport::submitTransaction(
    quint64 token, const QString &uniqueOwner, const QByteArray &requestJson) {
  QDBusMessage message = QDBusMessage::createMethodCall(
      uniqueOwner, QString::fromLatin1(kObjectPath),
      QString::fromLatin1(kInterfaceName), QStringLiteral("Submit"));
  message << requestJson;
  return issueCall(token, uniqueOwner, message);
}

bool QtTaskListOperationTransport::releaseContainer(
    quint64 token, const QString &uniqueOwner, const QString &containerId) {
  QDBusMessage message = QDBusMessage::createMethodCall(
      uniqueOwner, QString::fromLatin1(kObjectPath),
      QString::fromLatin1(kInterfaceName), QStringLiteral("ReleaseContainer"));
  message << containerId;
  return issueCall(token, uniqueOwner, message);
}

bool QtTaskListOperationTransport::dockWindows(
    quint64 token, const QString &uniqueOwner, const QString &targetWindowId,
    const QString &incomingWindowId, const QString &orientation,
    const QString &position, double ratio) {
  QDBusMessage message = QDBusMessage::createMethodCall(
      uniqueOwner, QString::fromLatin1(kObjectPath),
      QString::fromLatin1(kInterfaceName), QStringLiteral("DockWindows"));
  message << targetWindowId << incomingWindowId << orientation << position
          << ratio;
  return issueCall(token, uniqueOwner, message);
}

bool QtTaskListOperationTransport::issueCall(quint64 token,
                                             const QString &uniqueOwner,
                                             const QDBusMessage &message) {
  if (!m_connection.isConnected() || uniqueOwner.isEmpty()) {
    return false;
  }
  const QDBusPendingCall pending =
      m_connection.asyncCall(message, kDBusTimeoutMilliseconds);
  // AGENT-GUARD: A synchronous invalid handle means the call never left the
  // process; reporting anything else would blur the sent/unsent line that the
  // adapter's exactly-once contract depends on.
  if (pending.isError()) {
    return false;
  }
  auto *watcher = new QDBusPendingCallWatcher(pending, this);
  m_pendingCalls.append(watcher);
  connect(watcher, &QDBusPendingCallWatcher::finished, this,
          [this, watcher, token, uniqueOwner] {
            m_pendingCalls.removeAll(watcher);
            QDBusPendingReply<QByteArray> reply = *watcher;
            watcher->deleteLater();
            if (reply.isError()) {
              Q_EMIT operationFailed(token, uniqueOwner,
                                     reply.error().message());
              return;
            }
            Q_EMIT operationReplied(token, uniqueOwner, reply.value());
          });
  return true;
}

} // namespace QindaQt::ShellTaskList::Operations
