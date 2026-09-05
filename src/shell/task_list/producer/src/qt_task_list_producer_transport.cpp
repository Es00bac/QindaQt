// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell/task_list/producer/qt_task_list_producer_transport.h"

#include <QDBusError>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QTimer>

#include <utility>

namespace QindaQt::ShellTaskList::Producer {
namespace {

constexpr auto kServiceName = "org.qindaqt.Compositor";
constexpr auto kObjectPath = "/org/qindaqt/CompositorShell";
constexpr auto kInterfaceName = "org.qindaqt.CompositorShell1";
constexpr auto kFactsMethod = "TaskListSnapshot";
constexpr auto kFactsSignal = "TaskListSnapshotChanged";
constexpr auto kDBusService = "org.freedesktop.DBus";
constexpr auto kDBusPath = "/org/freedesktop/DBus";
constexpr auto kDBusInterface = "org.freedesktop.DBus";
constexpr auto kGetNameOwnerMethod = "GetNameOwner";
constexpr int kDBusTimeoutMilliseconds = 2000;

void setError(QString *error, QString message) {
  if (error) {
    *error = std::move(message);
  }
}

} // namespace

QtTaskListProducerTransport::QtTaskListProducerTransport(
    QDBusConnection connection, QObject *parent)
    : TaskListProducerTransport(parent), m_connection(std::move(connection)) {}

QtTaskListProducerTransport::~QtTaskListProducerTransport() { stop(); }

bool QtTaskListProducerTransport::start(QString *error) {
  if (m_started) {
    setError(error, {});
    return true;
  }
  if (!m_connection.isConnected()) {
    setError(error, QStringLiteral("session D-Bus is not connected"));
    return false;
  }

  m_serviceWatcher = new QDBusServiceWatcher(
      QString::fromLatin1(kServiceName), m_connection,
      QDBusServiceWatcher::WatchForOwnerChange, this);
  connect(m_serviceWatcher, &QDBusServiceWatcher::serviceOwnerChanged, this,
          [this](const QString &service, const QString &, const QString &newOwner) {
            if (!m_started || service != QLatin1StringView(kServiceName)) {
              return;
            }
            ++m_resolutionGeneration;
            bindOwner(newOwner);
          });
  m_started = true;
  resolveInitialOwner();
  setError(error, {});
  return true;
}

void QtTaskListProducerTransport::stop() {
  if (!m_started) {
    return;
  }
  m_started = false;
  ++m_resolutionGeneration;
  if (!m_uniqueOwner.isEmpty()) {
    m_connection.disconnect(m_uniqueOwner, QString::fromLatin1(kObjectPath),
                            QString::fromLatin1(kInterfaceName),
                            QString::fromLatin1(kFactsSignal), this,
                            SLOT(handleFactsChanged()));
  }
  m_uniqueOwner.clear();
  for (auto *pending : std::as_const(m_pendingCalls)) {
    if (pending) {
      pending->disconnect(this);
      pending->deleteLater();
    }
  }
  m_pendingCalls.clear();
  m_ownerObservationPublished = false;
  delete m_serviceWatcher;
  m_serviceWatcher = nullptr;
}

void QtTaskListProducerTransport::requestRefresh(quint64 token,
                                                 const QString &uniqueOwner) {
  if (!m_started || uniqueOwner.isEmpty() || uniqueOwner != m_uniqueOwner) {
    failRequest(token, uniqueOwner,
                QStringLiteral("refresh request owner is no longer current"));
    return;
  }
  issueRead(token, uniqueOwner);
}

void QtTaskListProducerTransport::handleFactsChanged() {
  if (m_started && !m_uniqueOwner.isEmpty()) {
    Q_EMIT refreshInvalidated(m_uniqueOwner);
  }
}

void QtTaskListProducerTransport::issueRead(quint64 token,
                                            const QString &uniqueOwner) {
  const QDBusMessage message = QDBusMessage::createMethodCall(
      uniqueOwner, QString::fromLatin1(kObjectPath),
      QString::fromLatin1(kInterfaceName), QString::fromLatin1(kFactsMethod));
  auto *watcher = new QDBusPendingCallWatcher(
      m_connection.asyncCall(message, kDBusTimeoutMilliseconds), this);
  m_pendingCalls.append(watcher);
  connect(watcher, &QDBusPendingCallWatcher::finished, this,
          [this, watcher, token, uniqueOwner] {
            m_pendingCalls.removeAll(watcher);
            QDBusPendingReply<QByteArray> reply = *watcher;
            watcher->deleteLater();
            if (!m_started) {
              return;
            }
            if (reply.isError()) {
              failRequest(token, uniqueOwner, reply.error().message());
              return;
            }
            Q_EMIT factsRead(token, uniqueOwner, reply.value());
          });
}

void QtTaskListProducerTransport::resolveInitialOwner() {
  if (!m_started) {
    return;
  }
  const quint64 generation = ++m_resolutionGeneration;
  QDBusMessage message = QDBusMessage::createMethodCall(
      QString::fromLatin1(kDBusService), QString::fromLatin1(kDBusPath),
      QString::fromLatin1(kDBusInterface),
      QString::fromLatin1(kGetNameOwnerMethod));
  message << QString::fromLatin1(kServiceName);
  auto *watcher = new QDBusPendingCallWatcher(
      m_connection.asyncCall(message, kDBusTimeoutMilliseconds), this);
  m_pendingCalls.append(watcher);
  connect(watcher, &QDBusPendingCallWatcher::finished, this,
          [this, watcher, generation] {
            m_pendingCalls.removeAll(watcher);
            QDBusPendingReply<QString> reply = *watcher;
            watcher->deleteLater();
            if (!m_started || generation != m_resolutionGeneration) {
              return;
            }
            if (reply.isError()) {
              bindOwner({});
              return;
            }
            bindOwner(reply.value());
          });
}

void QtTaskListProducerTransport::bindOwner(const QString &uniqueOwner) {
  if (!m_started ||
      (uniqueOwner == m_uniqueOwner && m_ownerObservationPublished)) {
    return;
  }
  const QString previous = m_uniqueOwner;
  if (!previous.isEmpty()) {
    m_connection.disconnect(previous, QString::fromLatin1(kObjectPath),
                            QString::fromLatin1(kInterfaceName),
                            QString::fromLatin1(kFactsSignal), this,
                            SLOT(handleFactsChanged()));
  }
  m_uniqueOwner.clear();

  if (uniqueOwner.isEmpty()) {
    // AGENT-GUARD: The first empty observation resolves Loading to known
    // unavailability. Suppressing it as an empty-to-empty transition strands
    // a cold-start producer in Loading (review finding P2-1 on 7b6bd8a).
    m_ownerObservationPublished = true;
    Q_EMIT serviceOwnerChanged({});
    return;
  }

  // AGENT-GUARD: Subscribe to the unique owner before exposing it to the
  // client, so the client's immediate refresh can never miss an invalidation
  // between owner resolution and its first read.
  const bool connected = m_connection.connect(
      uniqueOwner, QString::fromLatin1(kObjectPath),
      QString::fromLatin1(kInterfaceName), QString::fromLatin1(kFactsSignal),
      this, SLOT(handleFactsChanged()));
  if (!connected) {
    if (!m_ownerObservationPublished || !previous.isEmpty()) {
      Q_EMIT serviceOwnerChanged({});
    }
    m_ownerObservationPublished = true;
    QTimer::singleShot(250, this, [this] { resolveInitialOwner(); });
    return;
  }
  m_uniqueOwner = uniqueOwner;
  m_ownerObservationPublished = true;
  Q_EMIT serviceOwnerChanged(m_uniqueOwner);
}

void QtTaskListProducerTransport::failRequest(quint64 token,
                                              const QString &uniqueOwner,
                                              QString message) {
  if (message.trimmed().isEmpty()) {
    message = QStringLiteral("compositor task-list D-Bus read failed");
  }
  Q_EMIT refreshFailed(token, uniqueOwner, message);
}

} // namespace QindaQt::ShellTaskList::Producer
