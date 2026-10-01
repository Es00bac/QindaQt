// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/session/native_sleep/sleep_service.h>
#include <QDBusConnectionInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
namespace QindaQt::Session::NativeSleep {
namespace {
const QString Service = QStringLiteral("org.qindaqt.Sleep1");
const QString Path = QStringLiteral("/org/qindaqt/Sleep1");
const QString Session = QStringLiteral("org.qindaqt.Session1");
}
SleepService::SleepService(QDBusConnection bus, SleepCoordinator &coordinator,
    quint32 uid, QObject *parent)
    : QDBusVirtualObject(parent), m_bus(std::move(bus)), m_coordinator(coordinator), m_uid(uid) {
  connect(&m_coordinator, &SleepCoordinator::suspendFinished, this, &SleepService::finish);
}
SleepService::~SleepService() { stop(); }
bool SleepService::start() {
  if (m_started) return true;
  if (!m_bus.interface()) return false;
  m_bus.interface()->setTimeout(250);
  const auto owner = m_bus.interface()->serviceOwner(Session);
  if (!owner.isValid() || owner.value() != m_bus.baseService() ||
      !m_bus.registerVirtualObject(Path, this)) return false;
  if (!m_bus.registerService(Service)) { m_bus.unregisterObject(Path); return false; }
  m_started = true;
  m_supervisorWatcher = new QDBusServiceWatcher(Session, m_bus,
      QDBusServiceWatcher::WatchForOwnerChange, this);
  connect(m_supervisorWatcher, &QDBusServiceWatcher::serviceOwnerChanged, this,
      [this](const QString &, const QString &, const QString &owner) {
    if (owner != m_bus.baseService()) { m_coordinator.stop(); stop(); }
  });
  return true;
}
void SleepService::stop() {
  ++m_generation;
  finish(false);
  if (m_started) { m_bus.unregisterService(Service); m_bus.unregisterObject(Path); }
  m_started = false;
  delete m_supervisorWatcher; m_supervisorWatcher = nullptr;
}
QString SleepService::introspect(const QString &) const {
  return QStringLiteral("<interface name=\"org.qindaqt.Sleep1\">"
      "<method name=\"CanSuspend\"><arg type=\"b\" direction=\"out\"/></method>"
      "<method name=\"Suspend\"><arg type=\"b\" direction=\"out\"/></method></interface>");
}
bool SleepService::handleMessage(const QDBusMessage &message, const QDBusConnection &) {
  if (message.path() != Path || message.interface() != Service ||
      !message.signature().isEmpty()) return false;
  if (message.member() == QStringLiteral("CanSuspend")) {
    m_bus.send(message.createReply({m_started && m_coordinator.canSuspend()})); return true;
  }
  if (message.member() != QStringLiteral("Suspend")) return false;
  if (!m_started || m_pending) {
    m_bus.send(message.createReply({false})); return true;
  }
  m_pending = message;
  const auto generation = m_generation;
  auto call = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.DBus"),
      QStringLiteral("/org/freedesktop/DBus"), QStringLiteral("org.freedesktop.DBus"),
      QStringLiteral("GetConnectionUnixUser"));
  call.setArguments({message.service()});
  auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(call, 250), this);
  connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, generation] {
    const QDBusPendingReply<quint32> reply = *watcher;
    watcher->deleteLater();
    if (!m_started || generation != m_generation || !m_pending) return;
    const auto owner = m_bus.interface()->serviceOwner(Session);
    if (!owner.isValid() || owner.value() != m_bus.baseService() || reply.isError() ||
        reply.value() != m_uid || !m_coordinator.requestSuspend()) finish(false);
  });
  return true;
}
void SleepService::finish(bool confirmed) {
  if (!m_pending) return;
  const auto reply = m_pending->createReply({confirmed});
  m_pending.reset(); m_bus.send(reply);
}
}
