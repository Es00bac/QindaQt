// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/session/native_sleep/sleep_service.h>
#include <QDBusConnectionInterface>
#include <QDBusError>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QPointer>
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
  connect(&m_coordinator, &SleepCoordinator::availabilityChanged, this, [this] {
    if (m_started) m_bus.send(QDBusMessage::createSignal(Path, Service, QStringLiteral("Changed")));
  });
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
      [this](const QString &, const QString &, const QString &replacementOwner) {
    if (replacementOwner != m_bus.baseService()) { m_coordinator.stop(); stop(); }
  });
  return true;
}
void SleepService::stop() {
  ++m_generation;
  // A retired public handoff cannot leave its accepted mutation alive. Stop
  // emits Refused before dispatch or Uncertain after dispatch, then fences it.
  if (m_pending) m_coordinator.stop();
  finish(SleepResult::Refused);
  for (const auto id : m_queries.keys()) finishQuery(id, false);
  if (m_started) { m_bus.unregisterService(Service); m_bus.unregisterObject(Path); }
  m_started = false;
  if (m_supervisorWatcher) {
    // AGENT-GUARD: Qt emits more than one watcher signal for owner loss. Retire
    // callbacks now, but do not destroy the sender within its metacall.
    m_supervisorWatcher->disconnect(this);
    m_supervisorWatcher->setWatchedServices({});
    m_supervisorWatcher->deleteLater();
    m_supervisorWatcher = nullptr;
  }
}
QString SleepService::introspect(const QString &) const {
  QString xml = QStringLiteral("<interface name=\"org.qindaqt.Sleep1\">");
  for (const auto mode : {SleepMode::Suspend, SleepMode::Hibernate,
                         SleepMode::HybridSleep, SleepMode::SuspendThenHibernate}) {
    for (const auto &method : {capabilityMethod(mode), actionMethod(mode)})
      xml += QStringLiteral("<method name=\"%1\"><arg type=\"b\" direction=\"out\"/></method>").arg(method);
  }
  return xml + QStringLiteral("<signal name=\"Changed\"/></interface>");
}
bool SleepService::handleMessage(const QDBusMessage &message, const QDBusConnection &) {
  if (message.path() != Path || message.interface() != Service) return false;
  const bool query = message.member().startsWith(QStringLiteral("Can"));
  const auto mode = methodMode(message.member(), query);
  if (!mode) return false;
  if (!message.signature().isEmpty()) {
    m_bus.send(message.createErrorReply(QDBusError::InvalidArgs,
        QStringLiteral("Sleep1 methods require no arguments"))); return true;
  }
  // Bound readonly in-flight work independently of the sole mutation slot.
  if (!m_started || m_pending || (query && m_queries.size() >= 16)) {
    m_bus.send(message.createReply(QVariant(false))); return true;
  }
  const auto id = ++m_querySerial;
  if (query) m_queries.insert(id, message); else m_pending = message;
  const auto generation = m_generation;
  auto call = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.DBus"),
      QStringLiteral("/org/freedesktop/DBus"), QStringLiteral("org.freedesktop.DBus"),
      QStringLiteral("GetConnectionUnixUser"));
  call.setArguments({message.service()});
  auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(call, 250), this);
  connect(watcher, &QDBusPendingCallWatcher::finished, this,
      [this, watcher, generation, id, query, mode = *mode] {
    const QDBusPendingReply<quint32> reply = *watcher;
    watcher->deleteLater();
    if (!m_started || generation != m_generation ||
        (query ? !m_queries.contains(id) : !m_pending.has_value())) return;
    const auto denied = [this, query, id] {
      if (query) finishQuery(id, false); else finish(SleepResult::Refused);
    };
    if (!m_bus.isConnected() || !m_bus.interface()) { denied(); return; }
    const auto owner = m_bus.interface()->serviceOwner(Session);
    if (!owner.isValid() || owner.value() != m_bus.baseService() || reply.isError() ||
        reply.value() != m_uid) { denied(); return; }
    if (query) {
      m_coordinator.queryCapability(mode, [self = QPointer<SleepService>(this), generation, id](bool capable) {
        if (self && self->m_started && generation == self->m_generation) self->finishQuery(id, capable);
      });
    } else if (!m_coordinator.requestSleep(mode)) denied();
  });
  return true;
}
void SleepService::finishQuery(quint64 id, bool capable) {
  const auto query = m_queries.find(id);
  if (query == m_queries.end()) return;
  const auto message = query.value(); m_queries.erase(query);
  m_bus.send(message.createReply(QVariant(capable)));
}
void SleepService::finish(SleepResult result) {
  if (!m_pending) return;
  const auto reply = result == SleepResult::Uncertain
      ? m_pending->createErrorReply(QStringLiteral("org.qindaqt.Sleep1.Uncertain"),
          QStringLiteral("Sleep dispatch could not be confirmed; it was not replayed"))
      : m_pending->createReply(QVariant(result == SleepResult::Confirmed));
  m_pending.reset(); m_bus.send(reply);
}
}
