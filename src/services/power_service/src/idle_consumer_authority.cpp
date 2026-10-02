// SPDX-License-Identifier: GPL-3.0-or-later
#include "idle_consumer_authority_p.h"
#include <QDBusConnectionInterface>
#include <QDBusReply>
#include <unistd.h>
#include <utility>
namespace QindaQt::Power {
namespace {
const QString sessionName = QStringLiteral("org.qindaqt.Session1");
const QString legacyName = QStringLiteral("org.kde.Solid.PowerManagement");
}
IdleConsumerAuthority::IdleConsumerAuthority(QDBusConnection connection, QObject *parent)
    : QObject(parent), m_connection(std::move(connection)),
      m_watcher(QStringList{sessionName, legacyName}, m_connection, QDBusServiceWatcher::WatchForOwnerChange) {
    connect(&m_watcher, &QDBusServiceWatcher::serviceOwnerChanged,
            this, [this] { refresh(); });
    // Subscribe before resolution; queued owner-change payloads are not truth.
    refresh();
}
void IdleConsumerAuthority::setNativeAdmission(const bool admitted) {
    if (m_nativeAdmission == admitted) return;
    m_nativeAdmission = admitted;
    if (!admitted) Q_EMIT revoked();
    refresh();
}
void IdleConsumerAuthority::refresh() {
    QString current;
    if (m_connection.isConnected() && m_connection.interface()) {
        const QDBusReply<QString> owner = m_connection.interface()->serviceOwner(sessionName);
        if (owner.isValid() && !owner.value().isEmpty()) {
            const QDBusReply<uint> uid = m_connection.interface()->serviceUid(owner.value());
            if (uid.isValid() && uid.value() == static_cast<uint>(::getuid())) current = owner.value();
        }
    }
    bool absent = false;
    if (m_connection.isConnected() && m_connection.interface()) {
        const QDBusReply<bool> legacy = m_connection.interface()->isServiceRegistered(legacyName);
        absent = legacy.isValid() && !legacy.value();
    }
    const bool ownerChanged = current != m_sessionOwner;
    const bool legacyRevoked = m_legacyAbsent && !absent;
    m_sessionOwner = current;
    m_legacyAbsent = absent;
    if (ownerChanged || legacyRevoked) Q_EMIT revoked();
}
bool IdleConsumerAuthority::accepts(const QString &actualSender) {
    refresh();
    return m_nativeAdmission && m_legacyAbsent && !m_sessionOwner.isEmpty()
        && actualSender == m_sessionOwner;
}
}
