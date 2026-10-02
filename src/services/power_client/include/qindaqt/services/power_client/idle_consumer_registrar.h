// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <qindaqt/services/power_client/power_client.h>
#include <QDBusConnection>
namespace QindaQt::Power {
// Supervisor-only consumer declaration over the same constructing bus as the
// borrowed PowerClient. Both objects live on one Qt thread; PowerClient must
// outlive this registrar. It never decides which stage is genuinely composed.
// All three scopes or zero are accepted, no partial registration or replay.
// A method-reported acceptance is not policy truth: consumers use PowerClient's
// authenticated current-owner inhibitor receipt before enabling inhibition.
// cancel()/destruction withdraw only the original addressed owner/epoch and
// fence callbacks; Session1 owner loss independently withdraws server scopes.
class IdleConsumerRegistrar final : public QObject {
    Q_OBJECT
public:
    IdleConsumerRegistrar(QDBusConnection connection, PowerClient &power,
                          QObject *parent = nullptr);
    ~IdleConsumerRegistrar() override;
    quint64 declareConsumers(IdleInhibitorScopes scopes);
    void cancel();
Q_SIGNALS:
    void requestFinished(quint64 requestId, bool transportSuccess, bool reportedAccepted);
private:
    QDBusConnection m_connection;
    PowerClient &m_power;
    QString m_addressedOwner;
    quint64 m_addressedEpoch = 0;
    quint64 m_serial = 0;
};
}
