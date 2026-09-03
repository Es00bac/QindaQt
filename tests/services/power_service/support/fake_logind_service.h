// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QtCore/QObject>
#include <QtCore/QStringList>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusVirtualObject>

namespace QindaQt::Tests {

// Fake org.freedesktop.login1.Manager implemented as a QDBusVirtualObject with
// wire-faithful property maps, a(ssssuu) inhibitor records (including UID and
// PID, which the adapter must drop), the Can* action answers, and action
// methods whose replies can be deferred to race upstream owner replacement.
class FakeLogindService final : public QDBusVirtualObject
{
public:
    struct InhibitorSpec {
        QString what;
        QString who;
        QString why;
        QString mode;
        quint32 uid = 0;
        quint32 pid = 0;
    };

    struct ActionCall {
        QString method;
        bool interactive = false;
    };

    FakeLogindService(const QDBusConnection &connection, QObject *parent = nullptr);
    ~FakeLogindService() override;

    bool registerService();
    void unregisterService();

    void setSessionTruth(bool lidClosed, bool docked, bool preparingForSleep);
    void setInhibitors(const QList<InhibitorSpec> &inhibitors);
    void setCanAnswers(const QString &powerOff, const QString &reboot,
                       const QString &suspend, const QString &hibernate);
    void setFailHibernate(bool fail);
    void setDeferNextActionReply(bool defer);
    void completeDeferredActionReply();
    void emitPrepareForSleep(bool start);
    void emitManagerPropertiesChanged();
    [[nodiscard]] int listInhibitorsCalls() const { return listInhibitorsCallsCount; }

    QString introspect(const QString &path) const override;
    bool handleMessage(const QDBusMessage &message,
                       const QDBusConnection &connection) override;

    QList<ActionCall> actionCalls;

private:
    void sendError(const QDBusMessage &message, const QString &name,
                   const QString &text);

    QDBusConnection m_connection;
    QList<InhibitorSpec> m_inhibitors;
    QString m_canPowerOff = QStringLiteral("yes");
    QString m_canReboot = QStringLiteral("yes");
    QString m_canSuspend = QStringLiteral("yes");
    QString m_canHibernate = QStringLiteral("yes");
    QDBusMessage m_deferredReply;
    bool m_lidClosed = false;
    bool m_docked = false;
    bool m_preparingForSleep = false;
    bool m_failHibernate = false;
    bool m_deferNextActionReply = false;
    int listInhibitorsCallsCount = 0;
};

} // namespace QindaQt::Tests
