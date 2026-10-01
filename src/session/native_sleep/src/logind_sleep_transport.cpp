// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/session/native_sleep/logind_sleep_transport.h>
#include <QDBusArgument>
#include <QDBusConnectionInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QDBusUnixFileDescriptor>
#include <QDBusVariant>
#include <QRegularExpression>
#include <fcntl.h>
#include <unistd.h>
namespace QindaQt::Session::NativeSleep {
namespace {
const QString Service = QStringLiteral("org.freedesktop.login1");
const QString ManagerPath = QStringLiteral("/org/freedesktop/login1");
const QString Manager = QStringLiteral("org.freedesktop.login1.Manager");
const QString Session = QStringLiteral("org.freedesktop.login1.Session");
const QString Daemon = QStringLiteral("org.freedesktop.DBus");
const QString DaemonPath = QStringLiteral("/org/freedesktop/DBus");
bool validReply(const QDBusMessage &reply, const QString &signature) {
  return reply.type() == QDBusMessage::ReplyMessage && reply.signature() == signature;
}
}
LogindSleepTransport::LogindSleepTransport(QDBusConnection bus, QString id,
    quint32 uid, quint32 pid, std::function<bool()> admission, quint32 logindUid,
    QObject *parent)
    : QObject(parent), m_bus(std::move(bus)), m_id(std::move(id)), m_uid(uid),
      m_pid(pid), m_logindUid(logindUid), m_admission(std::move(admission)) {
  if (m_bus.interface()) m_bus.interface()->setTimeout(250);
  m_lifetime.setInterval(250);
  connect(&m_lifetime, &QTimer::timeout, this, [this] {
    if (m_started && (!m_bus.isConnected() || !m_admission || !m_admission())) revoke();
  });
}
LogindSleepTransport::~LogindSleepTransport() { stop(); }
void LogindSleepTransport::start() {
  if (m_started) return;
  m_started = true;
  m_watcher = new QDBusServiceWatcher(Service, m_bus,
      QDBusServiceWatcher::WatchForOwnerChange, this);
  connect(m_watcher, &QDBusServiceWatcher::serviceOwnerChanged, this,
      [this] { revoke(); if (m_started) resolve(); });
  m_lifetime.start();
  resolve();
}
void LogindSleepTransport::stop() {
  m_started = false;
  m_lifetime.stop();
  revoke();
  delete m_watcher;
  m_watcher = nullptr;
}
bool LogindSleepTransport::available() const {
  return m_ready && current(m_generation);
}
bool LogindSleepTransport::hasDelayInhibitor() const { return available() && m_delayFd >= 0; }
bool LogindSleepTransport::current(quint64 generation) const {
  if (!m_started || generation != m_generation || !m_bus.isConnected() ||
      !m_admission || !m_admission()) return false;
  if (m_owner.isEmpty()) return true; // Startup has not resolved a target yet.
  if (!m_bus.interface()) return false;
  const auto owner = m_bus.interface()->serviceOwner(Service);
  return owner.isValid() && owner.value() == m_owner;
}
void LogindSleepTransport::call(QString destination, QString path, QString interface,
    QString method, QVariantList arguments, quint64 generation, Completion completion) {
  auto message = QDBusMessage::createMethodCall(destination, path, interface, method);
  message.setArguments(arguments);
  auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(message, 750), this);
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
  if (!current(m_generation) || m_pid <= 1 || !idPattern.match(m_id).hasMatch()) return;
  const auto generation = m_generation;
  call(Daemon, DaemonPath, Daemon, QStringLiteral("GetNameOwner"), {Service}, generation,
      [this, generation](const QDBusMessage &reply) {
    if (!validReply(reply, QStringLiteral("s"))) return;
    const auto owner = reply.arguments().first().toString();
    if (!owner.startsWith(u':')) return;
    m_owner = owner;
    call(Daemon, DaemonPath, Daemon, QStringLiteral("GetConnectionUnixUser"),
        {owner}, generation, [this, generation](const QDBusMessage &uidReply) {
      if (!validReply(uidReply, QStringLiteral("u")) ||
          uidReply.arguments().first().toUInt() != m_logindUid) { revoke(); return; }
      validateSession(generation);
    });
  });
}
void LogindSleepTransport::validateSession(quint64 generation) {
  call(m_owner, ManagerPath, Manager, QStringLiteral("GetSession"), {m_id}, generation,
      [this, generation](const QDBusMessage &reply) {
    if (!validReply(reply, QStringLiteral("o"))) { revoke(); return; }
    const auto path = qvariant_cast<QDBusObjectPath>(reply.arguments().first()).path();
    static const QRegularExpression pathPattern(
        QStringLiteral("^/org/freedesktop/login1/session/[A-Za-z0-9_]{1,256}$"));
    if (!pathPattern.match(path).hasMatch()) { revoke(); return; }
    m_path = path;
    call(m_owner, ManagerPath, Manager, QStringLiteral("GetSessionByPID"),
        {m_pid}, generation, [this, generation](const QDBusMessage &pidReply) {
      if (!validReply(pidReply, QStringLiteral("o")) ||
          qvariant_cast<QDBusObjectPath>(pidReply.arguments().first()).path() != m_path) {
        revoke(); return;
      }
      call(m_owner, m_path, QStringLiteral("org.freedesktop.DBus.Properties"),
          QStringLiteral("GetAll"), {Session}, generation,
          [this, generation](const QDBusMessage &propertiesReply) {
        if (!validReply(propertiesReply, QStringLiteral("a{sv}"))) { revoke(); return; }
        const auto properties = qdbus_cast<QVariantMap>(propertiesReply.arguments().first());
        const auto id = properties.value(QStringLiteral("Id"));
        const auto user = properties.value(QStringLiteral("User"));
        if (id.metaType() != QMetaType::fromType<QString>() || id.toString() != m_id ||
            user.metaType() != QMetaType::fromType<QDBusArgument>()) { revoke(); return; }
        const auto wire = qvariant_cast<QDBusArgument>(user);
        if (wire.currentSignature() != QStringLiteral("(uo)")) { revoke(); return; }
        quint32 uid = 0; QDBusObjectPath userPath;
        wire.beginStructure(); wire >> uid >> userPath; wire.endStructure();
        if (uid != m_uid || userPath.path() !=
            QStringLiteral("/org/freedesktop/login1/user/_%1").arg(m_uid)) { revoke(); return; }
        if (!m_bus.connect(m_owner, m_path, Session, QStringLiteral("Lock"),
                           this, SLOT(receiveLock(QDBusMessage))) ||
            !m_bus.connect(m_owner, m_path, Session, QStringLiteral("Unlock"),
                           this, SLOT(receiveUnlock(QDBusMessage))) ||
            !m_bus.connect(m_owner, ManagerPath, Manager, QStringLiteral("PrepareForSleep"),
                           this, SLOT(receivePrepare(bool,QDBusMessage))) ||
            !m_bus.connect(m_owner, ManagerPath, Manager, QStringLiteral("SessionRemoved"),
                           this, SLOT(receiveRemoved(QString,QDBusObjectPath,QDBusMessage)))) { revoke(); return; }
        m_ready = true;
        Q_EMIT availabilityChanged();
        if (!current(generation)) return;
        call(m_owner, ManagerPath, QStringLiteral("org.freedesktop.DBus.Properties"),
            QStringLiteral("Get"), {Manager, QStringLiteral("PreparingForSleep")}, generation,
            [this](const QDBusMessage &preparingReply) {
          if (!validReply(preparingReply, QStringLiteral("v"))) { revoke(); return; }
          const auto value = qvariant_cast<QDBusVariant>(preparingReply.arguments().first()).variant();
          if (value.metaType() != QMetaType::fromType<bool>()) { revoke(); return; }
          // A signal arriving during this snapshot query supersedes its value.
          if (!m_preparationSeen) {
            m_preparing = value.toBool();
            Q_EMIT prepareForSleep(m_preparing);
          }
          if (!m_preparing) acquireDelayInhibitor();
        });
      });
    });
  });
}
void LogindSleepTransport::closeDelay() {
  // AGENT-GUARD: Linux releases the FD even on EINTR; retrying close could
  // close a reused descriptor. Qt retains ownership of unaccepted wire FDs.
  if (m_delayFd >= 0) { ::close(m_delayFd); m_delayFd = -1; }
}
void LogindSleepTransport::revoke() {
  ++m_generation;
  m_ready = false;
  m_preparing = false;
  m_preparationSeen = false;
  m_acquiring = false;
  closeDelay();
  if (!m_owner.isEmpty()) {
    m_bus.disconnect(m_owner, m_path, Session, QStringLiteral("Lock"), this, SLOT(receiveLock(QDBusMessage)));
    m_bus.disconnect(m_owner, m_path, Session, QStringLiteral("Unlock"), this, SLOT(receiveUnlock(QDBusMessage)));
    m_bus.disconnect(m_owner, ManagerPath, Manager, QStringLiteral("PrepareForSleep"), this, SLOT(receivePrepare(bool,QDBusMessage)));
    m_bus.disconnect(m_owner, ManagerPath, Manager, QStringLiteral("SessionRemoved"), this, SLOT(receiveRemoved(QString,QDBusObjectPath,QDBusMessage)));
  }
  m_owner.clear(); m_path.clear();
  cancelSleep();
  Q_EMIT availabilityChanged();
}
void LogindSleepTransport::acquireDelayInhibitor() {
  if (!available() || m_preparing || m_acquiring || m_delayFd >= 0) return;
  m_acquiring = true;
  call(m_owner, ManagerPath, Manager, QStringLiteral("Inhibit"),
      {QStringLiteral("sleep"), QStringLiteral("QindaQt"),
       QStringLiteral("Protect the selected native session before sleep"), QStringLiteral("delay")},
      m_generation, [this](const QDBusMessage &reply) {
    m_acquiring = false;
    if (validReply(reply, QStringLiteral("h")) && !m_preparing) {
      const auto descriptor = qvariant_cast<QDBusUnixFileDescriptor>(reply.arguments().first());
      if (descriptor.isValid()) m_delayFd = ::fcntl(descriptor.fileDescriptor(), F_DUPFD_CLOEXEC, 3);
    }
    Q_EMIT availabilityChanged();
  });
}
void LogindSleepTransport::releaseDelayInhibitor() { closeDelay(); Q_EMIT availabilityChanged(); }
bool LogindSleepTransport::setLockedHint(bool locked) {
  if (!available()) return false;
  call(m_owner, m_path, Session, QStringLiteral("SetLockedHint"), {locked}, m_generation,
      [this](const QDBusMessage &reply) {
    if (!validReply(reply, QString{})) revoke();
  });
  return true;
}
void LogindSleepTransport::queryCapability(SleepMode mode, std::function<void(bool)> completion) {
  const auto method = capabilityMethod(mode);
  if (method.isEmpty() || !hasDelayInhibitor() || m_preparing) { completion(false); return; }
  const auto generation = m_generation;
  auto message = QDBusMessage::createMethodCall(m_owner, ManagerPath, Manager, method);
  auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(message, 250), this);
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
bool LogindSleepTransport::signalAdmitted(const QDBusMessage &message, const QString &path) const {
  return available() && message.type() == QDBusMessage::SignalMessage &&
         message.service() == m_owner && message.path() == path;
}
void LogindSleepTransport::receiveLock(const QDBusMessage &message) {
  if (signalAdmitted(message, m_path) && message.signature().isEmpty()) Q_EMIT lockRequested();
}
void LogindSleepTransport::receiveUnlock(const QDBusMessage &message) {
  if (signalAdmitted(message, m_path) && message.signature().isEmpty()) Q_EMIT unlockRequested();
}
void LogindSleepTransport::receivePrepare(bool preparing, const QDBusMessage &message) {
  if (!signalAdmitted(message, ManagerPath) || message.signature() != QStringLiteral("b")) return;
  m_preparationSeen = true;
  m_preparing = preparing;
  Q_EMIT prepareForSleep(preparing);
  if (!preparing) acquireDelayInhibitor();
}
void LogindSleepTransport::receiveRemoved(const QString &id, const QDBusObjectPath &path,
    const QDBusMessage &message) {
  if (signalAdmitted(message, ManagerPath) && message.signature() == QStringLiteral("so") &&
      (id == m_id || path.path() == m_path)) revoke();
}

}
