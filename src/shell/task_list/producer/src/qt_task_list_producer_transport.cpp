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
constexpr auto kObjectPath = "/org/qindaqt/Compositor";
constexpr auto kInterfaceName = "org.qindaqt.Compositor1";
constexpr auto kWindowsMethod = "Windows";
constexpr auto kContainersMethod = "Containers";
constexpr auto kScopeMethod = "ShellVisibilitySnapshot";
constexpr auto kWindowsSignal = "WindowsChanged";
constexpr auto kContainerSignal = "ContainerCommitted";
constexpr auto kVisibilitySignal = "ShellVisibilityChanged";
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
    const char *slotNames[] = {SLOT(handleWindowsChanged()),
                           SLOT(handleContainerCommitted()),
                           SLOT(handleVisibilityChanged())};
    const char *signalNames[] = {kWindowsSignal, kContainerSignal,
                             kVisibilitySignal};
    for (int index = 0; index < 3; ++index) {
      m_connection.disconnect(m_uniqueOwner, QString::fromLatin1(kObjectPath),
                              QString::fromLatin1(kInterfaceName),
                              QString::fromLatin1(signalNames[index]), this,
                              slotNames[index]);
    }
  }
  m_uniqueOwner.clear();
  for (auto *pending : std::as_const(m_pendingCalls)) {
    if (pending) {
      pending->disconnect(this);
      pending->deleteLater();
    }
  }
  m_pendingCalls.clear();
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
  issueRead(token, uniqueOwner, kWindowsMethod);
  issueRead(token, uniqueOwner, kContainersMethod);
  issueRead(token, uniqueOwner, kScopeMethod);
}

void QtTaskListProducerTransport::handleWindowsChanged() {
  if (m_started && !m_uniqueOwner.isEmpty()) {
    Q_EMIT refreshInvalidated(m_uniqueOwner);
  }
}

void QtTaskListProducerTransport::handleContainerCommitted() {
  if (m_started && !m_uniqueOwner.isEmpty()) {
    Q_EMIT refreshInvalidated(m_uniqueOwner);
  }
}

void QtTaskListProducerTransport::handleVisibilityChanged() {
  if (m_started && !m_uniqueOwner.isEmpty()) {
    Q_EMIT refreshInvalidated(m_uniqueOwner);
  }
}

void QtTaskListProducerTransport::issueRead(quint64 token,
                                            const QString &uniqueOwner,
                                            const char *method) {
  const QDBusMessage message = QDBusMessage::createMethodCall(
      uniqueOwner, QString::fromLatin1(kObjectPath),
      QString::fromLatin1(kInterfaceName), QString::fromLatin1(method));
  auto *watcher = new QDBusPendingCallWatcher(
      m_connection.asyncCall(message, kDBusTimeoutMilliseconds), this);
  m_pendingCalls.append(watcher);
  connect(watcher, &QDBusPendingCallWatcher::finished, this,
          [this, watcher, token, uniqueOwner, method] {
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
            const QByteArray payload = reply.value();
            if (qstrcmp(method, kWindowsMethod) == 0) {
              Q_EMIT windowsRead(token, uniqueOwner, payload);
            } else if (qstrcmp(method, kContainersMethod) == 0) {
              Q_EMIT containersRead(token, uniqueOwner, payload);
            } else {
              Q_EMIT scopeRead(token, uniqueOwner, payload);
            }
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
  if (!m_started || uniqueOwner == m_uniqueOwner) {
    return;
  }
  const QString previous = m_uniqueOwner;
  if (!previous.isEmpty()) {
    for (const auto signal :
         {kWindowsSignal, kContainerSignal, kVisibilitySignal}) {
      m_connection.disconnect(previous, QString::fromLatin1(kObjectPath),
                              QString::fromLatin1(kInterfaceName),
                              QString::fromLatin1(signal), this, nullptr);
    }
  }
  m_uniqueOwner.clear();

  if (uniqueOwner.isEmpty()) {
    if (!previous.isEmpty()) {
      Q_EMIT serviceOwnerChanged({});
    }
    return;
  }

  // AGENT-GUARD: Subscribe to the unique owner before exposing it to the
  // client, so the client's immediate refresh can never miss an invalidation
  // between owner resolution and its first read.
  const char *slotNames[] = {SLOT(handleWindowsChanged()),
                         SLOT(handleContainerCommitted()),
                         SLOT(handleVisibilityChanged())};
  const char *signalNames[] = {kWindowsSignal, kContainerSignal, kVisibilitySignal};
  for (int index = 0; index < 3; ++index) {
    const bool connected = m_connection.connect(
        uniqueOwner, QString::fromLatin1(kObjectPath),
        QString::fromLatin1(kInterfaceName), QString::fromLatin1(signalNames[index]),
        this, slotNames[index]);
    if (!connected) {
      for (int rollback = 0; rollback < index; ++rollback) {
        m_connection.disconnect(uniqueOwner, QString::fromLatin1(kObjectPath),
                                QString::fromLatin1(kInterfaceName),
                                QString::fromLatin1(signalNames[rollback]), this,
                                slotNames[rollback]);
      }
      if (!previous.isEmpty()) {
        Q_EMIT serviceOwnerChanged({});
      }
      QTimer::singleShot(250, this, [this] { resolveInitialOwner(); });
      return;
    }
  }
  m_uniqueOwner = uniqueOwner;
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
