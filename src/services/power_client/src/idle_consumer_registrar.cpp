// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/power_client/idle_consumer_registrar.h>
#include <qindaqt/services/power_protocol/power_limits.h>
#include <QDBusConnectionInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <utility>
namespace QindaQt::Power {
namespace {
QDBusMessage declaration(const QString &owner, quint64 epoch, quint32 scopes) {
    auto call = QDBusMessage::createMethodCall(owner, QString::fromLatin1(kObjectPath),
        QString::fromLatin1(kInterfaceName), QStringLiteral("RegisterIdleConsumers"));
    call.setArguments({QVariant::fromValue(epoch), scopes});
    return call;
}
constexpr quint32 CompleteScopes = 7;
}
IdleConsumerRegistrar::IdleConsumerRegistrar(QDBusConnection connection,
    PowerClient &power, QObject *parent)
    : QObject(parent), m_connection(std::move(connection)), m_power(power) {}
IdleConsumerRegistrar::~IdleConsumerRegistrar() { cancel(); }
quint64 IdleConsumerRegistrar::declareConsumers(const IdleInhibitorScopes scopes) {
    const auto mask = static_cast<quint32>(scopes.toInt());
    if ((mask != 0 && mask != CompleteScopes) || !m_power.hasSnapshot()
        || m_power.owner().isEmpty() || !m_connection.isConnected() || !m_connection.interface()) return 0;
    const auto sessionOwner = m_connection.interface()->serviceOwner(QStringLiteral("org.qindaqt.Session1"));
    if (!sessionOwner.isValid() || sessionOwner.value() != m_connection.baseService()) return 0;
    const auto snapshot = m_power.snapshot();
    if (snapshot.epoch == 0 || !snapshot.wireValid
        || snapshot.availability == Availability::Starting
        || snapshot.availability == Availability::Unavailable) return 0;
    const QString owner = m_power.owner();
    const quint64 epoch = snapshot.epoch;
    const quint64 serial = ++m_serial;
    m_addressedOwner = owner;
    m_addressedEpoch = epoch;
    auto *pending = new QDBusPendingCallWatcher(
        m_connection.asyncCall(declaration(owner, epoch, mask), 2000), this);
    connect(pending, &QDBusPendingCallWatcher::finished, this,
        [this, pending, owner, epoch, serial] {
            const QDBusPendingReply<bool> reply = *pending;
            pending->deleteLater();
            if (serial != m_serial) return;
            const bool sameLineage = m_power.owner() == owner && m_power.hasSnapshot()
                && m_power.snapshot().epoch == epoch;
            Q_EMIT requestFinished(serial, sameLineage && !reply.isError(),
                                   sameLineage && !reply.isError() && reply.value());
        });
    return serial;
}
void IdleConsumerRegistrar::cancel() {
    ++m_serial;
    if (!m_addressedOwner.isEmpty() && m_addressedEpoch != 0 && m_connection.isConnected()) {
        // Queued after any declaration on this same connection. This cannot
        // resurrect a late registration or mutate a replacement Power1 owner.
        static_cast<void>(m_connection.asyncCall(declaration(m_addressedOwner, m_addressedEpoch, 0), 2000));
    }
    m_addressedOwner.clear();
    m_addressedEpoch = 0;
}
}
