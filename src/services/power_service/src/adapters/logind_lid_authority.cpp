// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/power_service/adapters/logind_lid_authority.h>
#include <QDBusArgument>
#include <QDBusConnectionInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusReply>
#include <QDBusServiceWatcher>
#include <QDBusUnixFileDescriptor>
#include <QDBusVariant>
#include <QRegularExpression>
#include <fcntl.h>
#include <unistd.h>
namespace QindaQt::Power::Upstream {
namespace {
const QString SessionName = QStringLiteral("org.qindaqt.Session1");
const QString LogindName = QStringLiteral("org.freedesktop.login1");
const QString ManagerPath = QStringLiteral("/org/freedesktop/login1");
const QString Manager = QStringLiteral("org.freedesktop.login1.Manager");
const QString Session = QStringLiteral("org.freedesktop.login1.Session");
const QString Properties = QStringLiteral("org.freedesktop.DBus.Properties");
const QString Daemon = QStringLiteral("org.freedesktop.DBus");
bool valid(const QDBusMessage &reply, const QString &signature) {
    return reply.type() == QDBusMessage::ReplyMessage && reply.signature() == signature;
}
QString owner(const QDBusConnection &bus, const QString &name) {
    if (!bus.isConnected() || !bus.interface()) return {};
    bus.interface()->setTimeout(250);
    const QDBusReply<QString> result = bus.interface()->serviceOwner(name);
    return result.isValid() ? result.value() : QString{};
}
}
LogindLidAuthority::LogindLidAuthority(QDBusConnection sessionBus, QDBusConnection systemBus,
    quint32 expectedLogindUid, QObject *parent)
    : LidHandlingAuthority(parent), m_sessionBus(std::move(sessionBus)),
      m_systemBus(std::move(systemBus)), m_logindUid(expectedLogindUid) {
    // AGENT-CONTRACT: subscribe before resolving identity or accepting a FD.
    m_sessionWatcher = new QDBusServiceWatcher(SessionName, m_sessionBus,
        QDBusServiceWatcher::WatchForOwnerChange, this);
    m_logindWatcher = new QDBusServiceWatcher(LogindName, m_systemBus,
        QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(m_sessionWatcher, &QDBusServiceWatcher::serviceOwnerChanged, this, &LogindLidAuthority::ownerChanged);
    connect(m_logindWatcher, &QDBusServiceWatcher::serviceOwnerChanged, this, &LogindLidAuthority::ownerChanged);
    const auto localPath = QStringLiteral("/org/freedesktop/DBus/Local");
    const auto localInterface = QStringLiteral("org.freedesktop.DBus.Local");
    m_subscribed = m_systemBus.connect({}, {}, Properties, QStringLiteral("PropertiesChanged"),
        this, SLOT(propertiesChanged(QDBusMessage)))
        && m_systemBus.connect({}, ManagerPath, Manager, QStringLiteral("SessionRemoved"),
        this, SLOT(sessionRemoved(QString,QDBusObjectPath,QDBusMessage)))
        && m_systemBus.connect({}, localPath, localInterface, QStringLiteral("Disconnected"), this, SLOT(disconnected()))
        && m_sessionBus.connect({}, localPath, localInterface, QStringLiteral("Disconnected"), this, SLOT(disconnected()));
}
LogindLidAuthority::~LogindLidAuthority() { m_enabled = false; revoke(); }
void LogindLidAuthority::setEnabled(bool enabled) {
    if (m_enabled == enabled) return;
    m_enabled = enabled;
    revoke();
    if (enabled && !m_quarantined) resolve();
}
void LogindLidAuthority::revoke(bool clearIdentity) {
    ++m_generation;
    m_ready = false;
    m_resolving = false;
    if (m_fd >= 0) { ::close(m_fd); m_fd = -1; }
    if (clearIdentity) { m_sessionOwner.clear(); m_owner.clear(); m_path.clear(); }
    Q_EMIT admissionChanged();
}
void LogindLidAuthority::failed(bool uncertain) {
    m_quarantined = m_quarantined || uncertain;
    revoke(false);
}
void LogindLidAuthority::ownerChanged() {
    revoke();
    if (m_enabled && !m_quarantined) resolve();
}
void LogindLidAuthority::disconnected() { m_enabled = false; revoke(); }
bool LogindLidAuthority::currentOwners() const {
    if (!m_enabled || m_quarantined || m_sessionOwner.isEmpty() || m_owner.isEmpty()
        || owner(m_sessionBus, SessionName) != m_sessionOwner || owner(m_systemBus, LogindName) != m_owner) return false;
    const QDBusReply<bool> legacy = m_sessionBus.interface()->isServiceRegistered(QStringLiteral("org.kde.Solid.PowerManagement"));
    return legacy.isValid() && !legacy.value();
}
bool LogindLidAuthority::activeSession() const {
    if (!currentOwners() || m_path.isEmpty()) return false;
    auto message = QDBusMessage::createMethodCall(m_owner, m_path, Properties, QStringLiteral("Get"));
    message.setArguments({Session, QStringLiteral("Active")});
    const QDBusReply<QDBusVariant> reply = m_systemBus.call(message, QDBus::Block, 250);
    return reply.isValid() && reply.value().variant().metaType() == QMetaType::fromType<bool>()
        && reply.value().variant().toBool() && currentOwners();
}
bool LogindLidAuthority::admitted() const {
    return m_ready && m_fd >= 0 && ::fcntl(m_fd, F_GETFD) >= 0 && activeSession();
}
void LogindLidAuthority::call(const QDBusConnection &bus, const QString &destination,
    const QString &path, const QString &interface, const QString &method,
    const QVariantList &args, quint64 generation, Completion completion) {
    auto message = QDBusMessage::createMethodCall(destination, path, interface, method);
    message.setArguments(args);
    auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(message, 750), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
        [this, watcher, generation, completion = std::move(completion)] {
            const auto reply = watcher->reply();
            watcher->deleteLater();
            // Retired replies retain no FD. Qt owns and closes their descriptors,
            // including a late reply after disable/name/session loss.
            if (m_enabled && !m_quarantined && generation == m_generation) completion(reply);
        });
}
void LogindLidAuthority::resolve() {
    if (!m_enabled || m_quarantined || m_resolving || !m_subscribed) return;
    m_resolving = true;
    const auto generation = m_generation;
    m_sessionOwner = owner(m_sessionBus, SessionName);
    if (m_sessionOwner.isEmpty()) { failed(); return; }
    call(m_sessionBus, Daemon, QStringLiteral("/org/freedesktop/DBus"), Daemon,
        QStringLiteral("GetConnectionUnixUser"), {m_sessionOwner}, generation,
        [this, generation](const QDBusMessage &reply) {
            if (!valid(reply, QStringLiteral("u")) || reply.arguments().first().toUInt() != quint32(::getuid())) { failed(); return; }
            m_sessionUid = reply.arguments().first().toUInt();
            call(m_sessionBus, Daemon, QStringLiteral("/org/freedesktop/DBus"), Daemon,
                QStringLiteral("GetConnectionUnixProcessID"), {m_sessionOwner}, generation,
                [this, generation](const QDBusMessage &pidReply) {
                    if (!valid(pidReply, QStringLiteral("u")) || pidReply.arguments().first().toUInt() <= 1) { failed(); return; }
                    m_supervisorPid = pidReply.arguments().first().toUInt();
                    resolveLogind(generation);
                });
        });
}
void LogindLidAuthority::resolveLogind(quint64 generation) {
    m_owner = owner(m_systemBus, LogindName);
    if (m_owner.isEmpty()) { failed(); return; }
    call(m_systemBus, Daemon, QStringLiteral("/org/freedesktop/DBus"), Daemon,
        QStringLiteral("GetConnectionUnixUser"), {m_owner}, generation,
        [this, generation](const QDBusMessage &reply) {
            if (!valid(reply, QStringLiteral("u")) || reply.arguments().first().toUInt() != m_logindUid
                || !currentOwners()) { failed(); return; }
            resolveSession(generation);
        });
}
void LogindLidAuthority::resolveSession(quint64 generation) {
    call(m_systemBus, m_owner, ManagerPath, Manager, QStringLiteral("GetSessionByPID"), {m_supervisorPid}, generation,
        [this, generation](const QDBusMessage &reply) {
            static const QRegularExpression pattern(QStringLiteral("^/org/freedesktop/login1/session/[A-Za-z0-9_]{1,256}$"));
            if (!valid(reply, QStringLiteral("o")) || !currentOwners()) { failed(); return; }
            m_path = qvariant_cast<QDBusObjectPath>(reply.arguments().first()).path();
            if (!pattern.match(m_path).hasMatch()) { failed(); return; }
            call(m_systemBus, m_owner, m_path, Properties, QStringLiteral("GetAll"), {Session}, generation,
                [this, generation](const QDBusMessage &properties) {
                    if (!valid(properties, QStringLiteral("a{sv}")) || !currentOwners()) { failed(); return; }
                    const auto values = qdbus_cast<QVariantMap>(properties.arguments().first());
                    const auto active = values.value(QStringLiteral("Active"));
                    const auto userValue = values.value(QStringLiteral("User"));
                    if (userValue.metaType() != QMetaType::fromType<QDBusArgument>()) { failed(); return; }
                    const auto user = qvariant_cast<QDBusArgument>(userValue);
                    const auto id = values.value(QStringLiteral("Id"));
                    if (active.metaType() != QMetaType::fromType<bool>() || !active.toBool()
                        || user.currentSignature() != QStringLiteral("(uo)")
                        || id.metaType() != QMetaType::fromType<QString>() || id.toString().isEmpty() || id.toString().size() > 128) { failed(); return; }
                    quint32 uid = 0; QDBusObjectPath userPath;
                    user.beginStructure(); user >> uid >> userPath; user.endStructure();
                    if (uid != m_sessionUid || userPath.path() != QStringLiteral("/org/freedesktop/login1/user/_%1").arg(uid)) { failed(); return; }
                    acquire(generation);
                });
        });
}
void LogindLidAuthority::acquire(quint64 generation) {
    if (!activeSession()) { failed(); return; }
    call(m_systemBus, m_owner, ManagerPath, Manager, QStringLiteral("Inhibit"),
        {QStringLiteral("handle-lid-switch"), QStringLiteral("QindaQt"),
         QStringLiteral("Exclusive native lid policy"), QStringLiteral("block")}, generation,
        [this](const QDBusMessage &reply) {
            if (!valid(reply, QStringLiteral("h"))) {
                const auto error = reply.errorName();
                failed(error == QStringLiteral("org.freedesktop.DBus.Error.NoReply")
                    || error == QStringLiteral("org.freedesktop.DBus.Error.Timeout"));
                return;
            }
            if (!activeSession()) { failed(); return; }
            const auto descriptor = qvariant_cast<QDBusUnixFileDescriptor>(reply.arguments().first());
            if (!descriptor.isValid()) { failed(); return; }
            m_fd = ::fcntl(descriptor.fileDescriptor(), F_DUPFD_CLOEXEC, 3);
            if (m_fd < 0) { failed(); return; }
            m_resolving = false;
            m_ready = true;
            Q_EMIT admissionChanged();
        });
}
void LogindLidAuthority::propertiesChanged(const QDBusMessage &message) {
    if (!m_enabled || message.service() != m_owner || message.path() != m_path
        || message.signature() != QStringLiteral("sa{sv}as") || message.arguments().first().toString() != Session) return;
    // Payload is only invalidation. Fresh daemon/name/PID/session/Active checks
    // restore authority; no arbitrary Properties payload can grant admission.
    revoke(false);
    if (!m_quarantined) resolve();
}
void LogindLidAuthority::sessionRemoved(const QString &, const QDBusObjectPath &path, const QDBusMessage &message) {
    if (m_enabled && message.service() == m_owner && message.path() == ManagerPath
        && message.signature() == QStringLiteral("so") && path.path() == m_path) {
        revoke();
    }
}
}
