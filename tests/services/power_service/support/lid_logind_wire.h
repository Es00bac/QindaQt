// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "fake_logind_service.h"
#include "lid_user_wire.h"
#include <QDBusMetaType>
#include <QDBusUnixFileDescriptor>
#include <QDBusVariant>
#include <optional>
#include <fcntl.h>
#include <unistd.h>
namespace QindaQt::Tests {
// Real descriptor transfer; only read peers survive in this actor. EOF proves
// every client/broker/Qt duplicate has closed. External pipe stays independent.
class LidLogindWire final : public QDBusVirtualObject {
public:
    explicit LidLogindWire(QDBusConnection bus) : domain(bus), m_bus(std::move(bus)) {
        qDBusRegisterMetaType<LidUserWire>();
        domain.setInhibitors({{QStringLiteral("sleep"), QStringLiteral("External app"), QStringLiteral("unchanged"), QStringLiteral("block"), quint32(::getuid()), quint32(::getpid())}});
        int pipe[2];
        if (::pipe2(pipe, O_CLOEXEC | O_NONBLOCK) == 0) { m_externalRead = pipe[0]; m_externalWrite = pipe[1]; }
    }
    ~LidLogindWire() override {
        stop();
        for (const int fd : peers) ::close(fd);
        if (m_externalRead >= 0) ::close(m_externalRead);
        if (m_externalWrite >= 0) ::close(m_externalWrite);
    }
    bool start() {
        return m_bus.registerVirtualObject(QStringLiteral("/org/freedesktop/login1"), this, QDBusConnection::SubPath)
            && m_bus.registerService(QStringLiteral("org.freedesktop.login1"));
    }
    void stop() { m_bus.unregisterService(QStringLiteral("org.freedesktop.login1")); m_bus.unregisterObject(QStringLiteral("/org/freedesktop/login1")); }
    void lid(bool closed, bool docked = false) { domain.setSessionTruth(closed, docked, false); domain.emitManagerPropertiesChanged(); }
    void activity(bool value) {
        active = value;
        auto message = QDBusMessage::createSignal(path, QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("PropertiesChanged"));
        message.setArguments({QStringLiteral("org.freedesktop.login1.Session"), QVariantMap{}, QStringList{QStringLiteral("Active")}});
        m_bus.send(message);
    }
    void removed() {
        auto message = QDBusMessage::createSignal(QStringLiteral("/org/freedesktop/login1"), QStringLiteral("org.freedesktop.login1.Manager"), QStringLiteral("SessionRemoved"));
        message.setArguments({QStringLiteral("private"), QVariant::fromValue(QDBusObjectPath(path))}); m_bus.send(message);
    }
    bool closedPeer(int index) const { char byte; return index >= 0 && index < peers.size() && ::read(peers[index], &byte, 1) == 0; }
    bool allClosed() const { for (int i = 0; i < peers.size(); ++i) if (!closedPeer(i)) return false; return true; }
    bool hasLiveOwned() const { return !peers.isEmpty() && !closedPeer(int(peers.size()) - 1); }
    bool externalAlive() const { char byte; return m_externalRead >= 0 && ::read(m_externalRead, &byte, 1) < 0 && errno == EAGAIN; }
    void replyInhibit() {
        if (!delayedInhibit) return;
        const auto message = *delayedInhibit; delayedInhibit.reset(); sendFd(message);
    }
    QString introspect(const QString &) const override { return QStringLiteral("<node/>"); }
    bool handleMessage(const QDBusMessage &message, const QDBusConnection &connection) override {
        const auto name = message.member();
        if (message.path() == path && message.interface() == QStringLiteral("org.freedesktop.DBus.Properties")) {
            if (name == QStringLiteral("Get")) { m_bus.send(message.createReply(QVariant::fromValue(QDBusVariant(active)))); return true; }
            if (name == QStringLiteral("GetAll")) {
                const quint32 uid = quint32(::getuid()) + (wrongUser ? 1u : 0u);
                QVariantMap properties{{QStringLiteral("Active"), malformedActive ? QVariant(QStringLiteral("true")) : QVariant(active)},
                    {QStringLiteral("Id"), QStringLiteral("private")},
                    {QStringLiteral("User"), QVariant::fromValue(LidUserWire{uid, QDBusObjectPath(QStringLiteral("/org/freedesktop/login1/user/_%1").arg(uid))})}};
                if (holdProperties) delayedProperties = message.createReply(properties);
                else m_bus.send(message.createReply(properties));
                return true;
            }
        }
        if (message.interface() == QStringLiteral("org.freedesktop.login1.Manager")) {
            if (name == QStringLiteral("GetSessionByPID")) {
                ++sessionQueries;
                queriedPid = message.arguments().first().toUInt();
                if (!pidAccepted || queriedPid != quint32(::getpid()))
                    m_bus.send(message.createErrorReply(QStringLiteral("org.freedesktop.login1.NoSessionForPID"), QStringLiteral("Private mismatched PID")));
                else m_bus.send(message.createReply(QVariant::fromValue(QDBusObjectPath(path))));
                return true;
            }
            if (name == QStringLiteral("Inhibit")) {
                ++inhibitCalls;
                arguments.append(message.arguments());
                if (denyInhibit) m_bus.send(message.createErrorReply(QStringLiteral("org.freedesktop.DBus.Error.AccessDenied"), QStringLiteral("Private denied FD")));
                else if (holdInhibit) delayedInhibit = message;
                else sendFd(message);
                return true;
            }
            if (name == QStringLiteral("Suspend") || name == QStringLiteral("Hibernate")) {
                ++directSleepCalls;
                m_bus.send(message.createErrorReply(QStringLiteral("org.freedesktop.DBus.Error.AccessDenied"), QStringLiteral("Direct sleep forbidden"))); return true;
            }
        }
        return domain.handleMessage(message, connection);
    }
    void replyProperties() { if (delayedProperties) { m_bus.send(*delayedProperties); delayedProperties.reset(); } }
    FakeLogindService domain;
    QList<int> peers;
    QList<QVariantList> arguments;
    const QString path = QStringLiteral("/org/freedesktop/login1/session/private");
    int inhibitCalls = 0, sessionQueries = 0, directSleepCalls = 0;
    quint32 queriedPid = 0;
    bool active = true, wrongUser = false, pidAccepted = true, malformedActive = false;
    bool denyInhibit = false, holdInhibit = false, holdProperties = false;
    std::optional<QDBusMessage> delayedInhibit, delayedProperties;
private:
    void sendFd(const QDBusMessage &message) {
        int pipe[2];
        if (::pipe2(pipe, O_CLOEXEC | O_NONBLOCK) != 0) {
            m_bus.send(message.createErrorReply(QStringLiteral("org.freedesktop.DBus.Error.LimitsExceeded"), QStringLiteral("Private pipe failed"))); return;
        }
        peers.append(pipe[0]);
        QDBusUnixFileDescriptor descriptor; descriptor.giveFileDescriptor(pipe[1]);
        m_bus.send(message.createReply(QVariant::fromValue(descriptor)));
    }
    QDBusConnection m_bus;
    int m_externalRead = -1, m_externalWrite = -1;
};
}
