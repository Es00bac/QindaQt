// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/session/native_sleep/logind_sleep_transport.h>
#include <QDBusArgument>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QDBusUnixFileDescriptor>
#include <QDBusVariant>
#include <QLoggingCategory>
#include <QRegularExpression>
#include <fcntl.h>
#include <unistd.h>
namespace QindaQt::Session::NativeSleep {
Q_LOGGING_CATEGORY(lcNativeSleep, "qindaqt.native-sleep", QtInfoMsg)
namespace {
const QString Service = QStringLiteral("org.freedesktop.login1");
const QString ManagerPath = QStringLiteral("/org/freedesktop/login1");
const QString Manager = QStringLiteral("org.freedesktop.login1.Manager");
const QString Session = QStringLiteral("org.freedesktop.login1.Session");
const QString Daemon = QStringLiteral("org.freedesktop.DBus");
const QString DaemonPath = QStringLiteral("/org/freedesktop/DBus");
constexpr int CallTimeoutMs = 2000;
constexpr int CapabilityTimeoutMs = 1000;
bool validReply(const QDBusMessage &reply, const QString &signature) {
  return reply.type() == QDBusMessage::ReplyMessage && reply.signature() == signature;
}
bool errorReply(const QDBusMessage &reply) {
  return reply.type() == QDBusMessage::ErrorMessage;
}
QString describe(const QDBusMessage &reply, const QString &what) {
  return errorReply(reply) ? QStringLiteral("%1 failed: %2").arg(what, reply.errorName())
                           : QStringLiteral("%1 returned a malformed reply").arg(what);
}
}
LogindSleepTransport::LogindSleepTransport(QDBusConnection bus, QString id,
    quint32 uid, quint32 pid, std::function<bool()> admission, quint32 logindUid,
    QObject *parent)
    : LogindSleepTransport(std::move(bus), std::move(id), uid, pid, std::move(admission),
                           logindUid, SleepTransportRecovery{}, parent) {}
LogindSleepTransport::LogindSleepTransport(QDBusConnection bus, QString id,
    quint32 uid, quint32 pid, std::function<bool()> admission, quint32 logindUid,
    SleepTransportRecovery recovery, QObject *parent)
    : QObject(parent), m_bus(std::move(bus)), m_id(std::move(id)), m_uid(uid),
      m_pid(pid), m_logindUid(logindUid), m_admission(std::move(admission)),
      m_bindRetry(recovery.policy, recovery.timers),
      m_maintenanceRetry(recovery.policy, recovery.timers) {
  connect(&m_bindRetry, &Platform::Recovery::RecoveryBackoff::retry, this, [this] {
    if (m_started && !m_ready && !m_sessionRemoved) resolve();
  });
  connect(&m_maintenanceRetry, &Platform::Recovery::RecoveryBackoff::retry, this,
          &LogindSleepTransport::retryMaintenance);
}
LogindSleepTransport::~LogindSleepTransport() { stop(); }
void LogindSleepTransport::start() {
  if (m_started) return;
  m_started = true;
  m_sessionRemoved = false;
  m_watcher = new QDBusServiceWatcher(Service, m_bus,
      QDBusServiceWatcher::WatchForOwnerChange, this);
  connect(m_watcher, &QDBusServiceWatcher::serviceOwnerChanged, this,
      [this](const QString &, const QString &, const QString &newOwner) {
    // An advertisement of the bound (or refused) owner is not a change.
    if (!m_started || m_sessionRemoved || newOwner == m_owner) return;
    if (!m_owner.isEmpty())
      qCInfo(lcNativeSleep).noquote() << "logind owner changed; rebinding";
    unbind();
    m_bindRetry.reset();
    if (newOwner.isEmpty()) m_bindRetry.schedule(); // safety net only
    else bind(newOwner);
  });
  resolve();
}
void LogindSleepTransport::stop() {
  m_started = false;
  m_bindRetry.reset();
  m_maintenanceRetry.reset();
  unbind();
  if (m_watcher) {
    // AGENT-GUARD: stop may run inside a watcher-originated signal chain.
    m_watcher->disconnect(this);
    m_watcher->deleteLater();
    m_watcher = nullptr;
  }
}
bool LogindSleepTransport::available() const {
  return m_ready && current(m_generation) && admitted();
}
bool LogindSleepTransport::hasDelayInhibitor() const { return available() && m_delayFd >= 0; }
bool LogindSleepTransport::current(quint64 generation) const {
  return m_started && generation == m_generation && m_bus.isConnected();
}
void LogindSleepTransport::call(QString destination, QString path, QString interface,
    QString method, QVariantList arguments, quint64 generation, Completion completion) {
  auto message = QDBusMessage::createMethodCall(destination, path, interface, method);
  message.setArguments(arguments);
  auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(message, CallTimeoutMs), this);
  connect(watcher, &QDBusPendingCallWatcher::finished, this,
      [this, watcher, generation, completion = std::move(completion)] {
    const auto reply = watcher->reply();
    watcher->deleteLater();
    // Reply's FD is owned by Qt until accepted and duplicated below. A retired
    // generation does not retain descriptors or mutate a restarted authority.
    if (current(generation)) completion(reply);
  });
}
void LogindSleepTransport::resolve() {
  static const QRegularExpression idPattern(QStringLiteral("^[A-Za-z0-9_-]{1,128}$"));
  if (!m_started || m_ready || m_sessionRemoved || !m_bus.isConnected() || m_pid <= 1 ||
      !idPattern.match(m_id).hasMatch()) return;
  const auto generation = m_generation;
  call(Daemon, DaemonPath, Daemon, QStringLiteral("GetNameOwner"), {Service}, generation,
      [this, generation](const QDBusMessage &reply) {
    if (!validReply(reply, QStringLiteral("s")) ||
        !reply.arguments().first().toString().startsWith(u':')) {
      bindingFailed(generation, describe(reply, QStringLiteral("logind owner query")), true);
      return;
    }
    bind(reply.arguments().first().toString());
  });
}
void LogindSleepTransport::bind(const QString &owner) {
  if (!m_started || m_ready || m_sessionRemoved) return;
  ++m_generation; // fence a chain still validating an earlier owner
  m_owner = owner;
  m_path.clear();
  const auto generation = m_generation;
  call(Daemon, DaemonPath, Daemon, QStringLiteral("GetConnectionUnixUser"),
      {owner}, generation, [this, generation](const QDBusMessage &uidReply) {
    if (errorReply(uidReply)) {
      bindingFailed(generation, describe(uidReply, QStringLiteral("logind UID query")), true);
      return;
    }
    if (!validReply(uidReply, QStringLiteral("u")) ||
        uidReply.arguments().first().toUInt() != m_logindUid) {
      bindingFailed(generation, QStringLiteral("logind owner has an unexpected UID"), false);
      return;
    }
    validateSession(generation);
  });
}
void LogindSleepTransport::validateSession(quint64 generation) {
  call(m_owner, ManagerPath, Manager, QStringLiteral("GetSession"), {m_id}, generation,
      [this, generation](const QDBusMessage &reply) {
    if (errorReply(reply)) { bindingFailed(generation, describe(reply, QStringLiteral("GetSession")), true); return; }
    const auto path = validReply(reply, QStringLiteral("o"))
        ? qvariant_cast<QDBusObjectPath>(reply.arguments().first()).path() : QString{};
    static const QRegularExpression pathPattern(
        QStringLiteral("^/org/freedesktop/login1/session/[A-Za-z0-9_]{1,256}$"));
    if (!pathPattern.match(path).hasMatch()) {
      bindingFailed(generation, QStringLiteral("selected session path is malformed"), false);
      return;
    }
    m_path = path;
    call(m_owner, ManagerPath, Manager, QStringLiteral("GetSessionByPID"),
        {m_pid}, generation, [this, generation](const QDBusMessage &pidReply) {
      if (errorReply(pidReply)) {
        bindingFailed(generation, describe(pidReply, QStringLiteral("GetSessionByPID")), true);
        return;
      }
      if (!validReply(pidReply, QStringLiteral("o")) ||
          qvariant_cast<QDBusObjectPath>(pidReply.arguments().first()).path() != m_path) {
        bindingFailed(generation, QStringLiteral("supervisor belongs to another session"), false);
        return;
      }
      call(m_owner, m_path, QStringLiteral("org.freedesktop.DBus.Properties"),
          QStringLiteral("GetAll"), {Session}, generation,
          [this, generation](const QDBusMessage &propertiesReply) {
        if (errorReply(propertiesReply)) {
          bindingFailed(generation, describe(propertiesReply, QStringLiteral("session properties")), true);
          return;
        }
        bool valid = validReply(propertiesReply, QStringLiteral("a{sv}"));
        const auto properties = valid ? qdbus_cast<QVariantMap>(propertiesReply.arguments().first())
                                      : QVariantMap{};
        const auto id = properties.value(QStringLiteral("Id"));
        const auto user = properties.value(QStringLiteral("User"));
        valid = valid && id.metaType() == QMetaType::fromType<QString>() && id.toString() == m_id &&
                user.metaType() == QMetaType::fromType<QDBusArgument>();
        quint32 uid = 0; QDBusObjectPath userPath;
        if (valid) {
          const auto wire = qvariant_cast<QDBusArgument>(user);
          valid = wire.currentSignature() == QStringLiteral("(uo)");
          if (valid) { wire.beginStructure(); wire >> uid >> userPath; wire.endStructure(); }
        }
        if (!valid || uid != m_uid || userPath.path() !=
            QStringLiteral("/org/freedesktop/login1/user/_%1").arg(m_uid)) {
          bindingFailed(generation, QStringLiteral("selected session is not this user's"), false);
          return;
        }
        subscribe(generation);
      });
    });
  });
}
void LogindSleepTransport::subscribe(quint64 generation) {
  if (!m_bus.connect(m_owner, m_path, Session, QStringLiteral("Lock"),
                     this, SLOT(receiveLock(QDBusMessage))) ||
      !m_bus.connect(m_owner, m_path, Session, QStringLiteral("Unlock"),
                     this, SLOT(receiveUnlock(QDBusMessage))) ||
      !m_bus.connect(m_owner, ManagerPath, Manager, QStringLiteral("PrepareForSleep"),
                     this, SLOT(receivePrepare(bool,QDBusMessage))) ||
      !m_bus.connect(m_owner, ManagerPath, Manager, QStringLiteral("SessionRemoved"),
                     this, SLOT(receiveRemoved(QString,QDBusObjectPath,QDBusMessage)))) {
    bindingFailed(generation, QStringLiteral("logind signal subscription failed"), true);
    return;
  }
  m_ready = true;
  qCInfo(lcNativeSleep).noquote() << "logind sleep binding established for session" << m_id;
  publishAvailability();
  if (!current(generation)) return;
  call(m_owner, ManagerPath, QStringLiteral("org.freedesktop.DBus.Properties"),
      QStringLiteral("Get"), {Manager, QStringLiteral("PreparingForSleep")}, generation,
      [this, generation](const QDBusMessage &preparingReply) {
    const auto value = validReply(preparingReply, QStringLiteral("v"))
        ? qvariant_cast<QDBusVariant>(preparingReply.arguments().first()).variant() : QVariant{};
    if (value.metaType() != QMetaType::fromType<bool>()) {
      bindingFailed(generation, describe(preparingReply, QStringLiteral("PreparingForSleep")), true);
      return;
    }
    m_bindRetry.reset();
    // A signal arriving during this snapshot query supersedes its value.
    if (!m_preparationSeen) {
      m_preparing = value.toBool();
      if (available()) Q_EMIT prepareForSleep(m_preparing);
    }
    if (!current(generation)) return;
    if (!m_preparing) acquireDelayInhibitor();
    sendLockedHint();
    publishAvailability();
  });
}
void LogindSleepTransport::bindingFailed(quint64 generation, const QString &reason, bool transient) {
  if (!current(generation)) return;
  const auto owner = m_owner;
  unbind();
  if (transient) {
    if (m_bindRetry.attempts() == 0)
      qCInfo(lcNativeSleep).noquote() << "logind sleep binding retrying:" << reason;
    m_bindRetry.schedule();
    return;
  }
  // Refused until the owner changes; the watcher rebinds a new owner.
  m_owner = owner;
  qCWarning(lcNativeSleep).noquote() << "logind sleep binding refused:" << reason;
}
void LogindSleepTransport::closeDelay() {
  // AGENT-GUARD: Linux releases the FD even on EINTR; retrying close could
  // close a reused descriptor. Qt retains ownership of unaccepted wire FDs.
  if (m_delayFd >= 0) { ::close(m_delayFd); m_delayFd = -1; }
}
void LogindSleepTransport::unbind() {
  ++m_generation;
  m_ready = false;
  m_preparing = false;
  m_preparationSeen = false;
  m_acquiring = false;
  m_hintPending = false;
  m_hintConfirmed = false;
  closeDelay();
  m_maintenanceRetry.reset();
  if (!m_owner.isEmpty()) {
    m_bus.disconnect(m_owner, m_path, Session, QStringLiteral("Lock"), this, SLOT(receiveLock(QDBusMessage)));
    m_bus.disconnect(m_owner, m_path, Session, QStringLiteral("Unlock"), this, SLOT(receiveUnlock(QDBusMessage)));
    m_bus.disconnect(m_owner, ManagerPath, Manager, QStringLiteral("PrepareForSleep"), this, SLOT(receivePrepare(bool,QDBusMessage)));
    m_bus.disconnect(m_owner, ManagerPath, Manager, QStringLiteral("SessionRemoved"), this, SLOT(receiveRemoved(QString,QDBusObjectPath,QDBusMessage)));
  }
  m_owner.clear(); m_path.clear();
  cancelSleep();
  publishAvailability();
}
void LogindSleepTransport::reevaluateAdmission() {
  if (!m_started) return;
  if (!admitted()) {
    // The lock cannot be applied without admission; never delay sleep for it.
    closeDelay();
    m_maintenanceRetry.reset();
  } else if (m_ready && !m_preparing) {
    acquireDelayInhibitor();
    sendLockedHint();
  }
  publishAvailability();
}
void LogindSleepTransport::acquireDelayInhibitor() {
  if (!available() || m_preparing || m_acquiring || m_delayFd >= 0) return;
  m_acquiring = true;
  call(m_owner, ManagerPath, Manager, QStringLiteral("Inhibit"),
      {QStringLiteral("sleep"), QStringLiteral("QindaQt"),
       QStringLiteral("Protect the selected native session before sleep"), QStringLiteral("delay")},
      m_generation, [this](const QDBusMessage &reply) {
    m_acquiring = false;
    if (validReply(reply, QStringLiteral("h"))) {
      const auto descriptor = qvariant_cast<QDBusUnixFileDescriptor>(reply.arguments().first());
      if (descriptor.isValid() && !m_preparing && admitted() && m_delayFd < 0)
        m_delayFd = ::fcntl(descriptor.fileDescriptor(), F_DUPFD_CLOEXEC, 3);
      if (m_delayFd >= 0) m_maintenanceRetry.reset();
    } else {
      qCInfo(lcNativeSleep).noquote() << describe(reply, QStringLiteral("delay inhibitor"));
      if (!m_preparing) m_maintenanceRetry.schedule();
    }
    publishAvailability();
  });
}
void LogindSleepTransport::releaseDelayInhibitor() { closeDelay(); publishAvailability(); }
bool LogindSleepTransport::setLockedHint(bool locked) {
  m_desiredHint = locked;
  m_hasDesiredHint = true;
  if (!available()) return false;
  sendLockedHint();
  return true;
}
void LogindSleepTransport::sendLockedHint() {
  if (!available() || !m_hasDesiredHint || m_hintPending ||
      (m_hintConfirmed && m_confirmedHint == m_desiredHint)) return;
  m_hintPending = true;
  const bool value = m_desiredHint;
  call(m_owner, m_path, Session, QStringLiteral("SetLockedHint"), {value}, m_generation,
      [this, value](const QDBusMessage &reply) {
    m_hintPending = false;
    if (!validReply(reply, QString{})) {
      m_hintConfirmed = false;
      m_maintenanceRetry.schedule();
      return;
    }
    m_hintConfirmed = true;
    m_confirmedHint = value;
    sendLockedHint(); // the desired value may have moved meanwhile
  });
}
void LogindSleepTransport::retryMaintenance() {
  if (!m_started || !m_ready) return;
  if (m_delayFd < 0 && !m_preparing) acquireDelayInhibitor();
  sendLockedHint();
}
void LogindSleepTransport::publishAvailability() {
  const int snapshot = (available() ? 1 : 0) | (hasDelayInhibitor() ? 2 : 0) |
                       (preparingForSleep() ? 4 : 0);
  if (snapshot == m_published) return;
  m_published = snapshot;
  Q_EMIT availabilityChanged();
}
void LogindSleepTransport::queryCapability(SleepMode mode, std::function<void(bool)> completion) {
  const auto method = capabilityMethod(mode);
  if (method.isEmpty() || !hasDelayInhibitor() || m_preparing) { completion(false); return; }
  const auto generation = m_generation;
  auto message = QDBusMessage::createMethodCall(m_owner, ManagerPath, Manager, method);
  auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(message, CapabilityTimeoutMs), this);
  connect(watcher, &QDBusPendingCallWatcher::finished, this,
      [this, watcher, generation, completion = std::move(completion)] {
    const auto reply = watcher->reply(); watcher->deleteLater();
    completion(current(generation) && hasDelayInhibitor() && !m_preparing &&
        validReply(reply, QStringLiteral("s")) &&
        reply.arguments().first().toString() == QStringLiteral("yes"));
  });
}
void LogindSleepTransport::cancelSleep() {
  ++m_requestSerial;
  if (!m_suspendPending) return;
  const auto result = m_mutationDispatched ? SleepResult::Uncertain : SleepResult::Refused;
  m_suspendPending = false; m_mutationDispatched = false;
  Q_EMIT suspendFinished(result);
}
bool LogindSleepTransport::requestSleep(SleepMode mode, std::function<bool()> protectedAdmission) {
  const auto action = actionMethod(mode);
  if (action.isEmpty() || !hasDelayInhibitor() || m_preparing || m_suspendPending ||
      !protectedAdmission || !protectedAdmission()) return false;
  m_suspendPending = true;
  m_mutationDispatched = false;
  const auto serial = ++m_requestSerial;
  call(m_owner, ManagerPath, Manager, capabilityMethod(mode), {}, m_generation,
      [this, serial, action, protectedAdmission = std::move(protectedAdmission)](const QDBusMessage &reply) {
    if (!m_suspendPending || serial != m_requestSerial) return;
    if (!hasDelayInhibitor() || m_preparing || !protectedAdmission() ||
        !validReply(reply, QStringLiteral("s")) ||
        reply.arguments().first().toString() != QStringLiteral("yes")) {
      cancelSleep(); return;
    }
    m_mutationDispatched = true;
    call(m_owner, ManagerPath, Manager, action, {false}, m_generation,
        [this, serial](const QDBusMessage &result) {
      if (!m_suspendPending || serial != m_requestSerial) return;
      m_suspendPending = false;
      m_mutationDispatched = false;
      Q_EMIT suspendFinished(validReply(result, QString{})
          ? SleepResult::Confirmed : SleepResult::Uncertain);
    });
  });
  return true;
}
bool LogindSleepTransport::signalFromOwner(const QDBusMessage &message, const QString &path) const {
  return m_ready && current(m_generation) && message.type() == QDBusMessage::SignalMessage &&
         message.service() == m_owner && message.path() == path;
}
void LogindSleepTransport::receiveLock(const QDBusMessage &message) {
  if (signalFromOwner(message, m_path) && available() && message.signature().isEmpty())
    Q_EMIT lockRequested();
}
void LogindSleepTransport::receiveUnlock(const QDBusMessage &message) {
  if (signalFromOwner(message, m_path) && available() && message.signature().isEmpty())
    Q_EMIT unlockRequested();
}
void LogindSleepTransport::receivePrepare(bool preparing, const QDBusMessage &message) {
  if (!signalFromOwner(message, ManagerPath) || message.signature() != QStringLiteral("b")) return;
  // Track logind's phase even while unadmitted so re-admission never takes a
  // delay lock in the middle of a sleep transition.
  m_preparationSeen = true;
  m_preparing = preparing;
  if (preparing) m_maintenanceRetry.cancel();
  if (available()) Q_EMIT prepareForSleep(preparing);
  if (!preparing) acquireDelayInhibitor();
  publishAvailability();
}
void LogindSleepTransport::receiveRemoved(const QString &id, const QDBusObjectPath &path,
    const QDBusMessage &message) {
  if (!signalFromOwner(message, ManagerPath) || message.signature() != QStringLiteral("so") ||
      (id != m_id && path.path() != m_path)) return;
  qCInfo(lcNativeSleep).noquote() << "logind removed the selected session";
  unbind();
  m_sessionRemoved = true;
  m_bindRetry.reset();
}

}
