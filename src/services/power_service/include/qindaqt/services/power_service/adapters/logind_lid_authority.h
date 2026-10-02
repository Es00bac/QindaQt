// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <qindaqt/services/power_service/lid_handling_authority.h>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <functional>
class QDBusServiceWatcher;
namespace QindaQt::Power::Upstream {
// Constructor-only expected UID supports a noninstalled private resident.
// Installed assembly fixes login1 UID0. Session1 UID/PID come from its current
// bus-daemon-authenticated owner; session identity never comes from an env var.
class LogindLidAuthority final : public LidHandlingAuthority {
    Q_OBJECT
public:
    LogindLidAuthority(QDBusConnection sessionBus, QDBusConnection systemBus,
                      quint32 expectedLogindUid = 0, QObject *parent = nullptr);
    ~LogindLidAuthority() override;
    void setEnabled(bool enabled) override;
    bool admitted() const override;
    quint64 generation() const override { return m_generation; }
private:
    using Completion = std::function<void(const QDBusMessage &)>;
    void call(const QDBusConnection &bus, const QString &owner, const QString &path,
              const QString &interface, const QString &method, const QVariantList &args,
              quint64 generation, Completion completion);
    void resolve();
    void resolveLogind(quint64 generation);
    void resolveSession(quint64 generation);
    void acquire(quint64 generation);
    void revoke(bool clearIdentity = true);
    void failed(bool uncertain = false);
    bool currentOwners() const;
    bool activeSession() const;
    void ownerChanged();
    QDBusConnection m_sessionBus, m_systemBus;
    const quint32 m_logindUid;
    QDBusServiceWatcher *m_sessionWatcher = nullptr, *m_logindWatcher = nullptr;
    QString m_sessionOwner, m_owner, m_path;
    quint32 m_sessionUid = 0, m_supervisorPid = 0;
    quint64 m_generation = 0;
    int m_fd = -1;
    bool m_enabled = false, m_ready = false, m_resolving = false, m_quarantined = false;
    bool m_subscribed = false;
private Q_SLOTS:
    void propertiesChanged(const QDBusMessage &message);
    void sessionRemoved(const QString &id, const QDBusObjectPath &path, const QDBusMessage &message);
    void disconnected();
};
}
