// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell/task_list/producer/task_list_producer_transport.h"

#include <QDBusConnection>
#include <QList>

class QDBusPendingCallWatcher;
class QDBusServiceWatcher;

namespace QindaQt::ShellTaskList::Producer {

// QtDBus binding of the producer seam to the public org.qindaqt.Compositor
// endpoint on an injected bus connection. All signal subscriptions and method
// calls bind to the exact current unique owner, mirroring the integrated
// compositor output authority pattern (integrated at 89557a0a); the injected
// connection lets tests run on a private bus and never the host session bus.
// AGENT-CONTRACT: Completing initial owner discovery publishes one observation
// even when the well-known name is unowned. Empty-before-resolution means
// unknown/loading; observed empty means known unavailable/degraded.
class QtTaskListProducerTransport final : public TaskListProducerTransport {
  Q_OBJECT

public:
  explicit QtTaskListProducerTransport(QDBusConnection connection,
                                       QObject *parent = nullptr);
  ~QtTaskListProducerTransport() override;

  [[nodiscard]] bool start(QString *error = nullptr) override;
  void stop() override;
  void requestRefresh(quint64 token, const QString &uniqueOwner) override;

private Q_SLOTS:
  void handleWindowsChanged();

private:
  void resolveInitialOwner();
  void bindOwner(const QString &uniqueOwner);
  void issueRead(quint64 token, const QString &uniqueOwner);
  void failRequest(quint64 token, const QString &uniqueOwner, QString message);

  QDBusConnection m_connection;
  QDBusServiceWatcher *m_serviceWatcher = nullptr;
  QList<QDBusPendingCallWatcher *> m_pendingCalls;
  QString m_uniqueOwner;
  quint64 m_resolutionGeneration = 0;
  bool m_ownerObservationPublished = false;
  bool m_started = false;
};

} // namespace QindaQt::ShellTaskList::Producer
