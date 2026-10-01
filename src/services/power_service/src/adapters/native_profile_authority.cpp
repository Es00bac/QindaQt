// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/power_service/adapters/native_profile_authority.h>
#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusServiceWatcher>
#include <QtDBus/QDBusReply>
namespace QindaQt::Power::Upstream {
namespace {
const QString legacyName = QStringLiteral("org.kde.Solid.PowerManagement");
}
NativeProfileAuthority::NativeProfileAuthority(QDBusConnection connection,
                                               bool exclusive, QObject *parent)
    : QObject(parent), m_connection(std::move(connection)), m_exclusive(exclusive) {}
NativeProfileAuthority::~NativeProfileAuthority() = default;
void NativeProfileAuthority::start()
{
    if (!m_exclusive || m_watcher) return;
    // AGENT-GUARD: Subscribe before querying, then resolve the current owner
    // for every notification. Never trust a queued old/new-owner payload.
    m_watcher = std::make_unique<QDBusServiceWatcher>(legacyName, m_connection,
        QDBusServiceWatcher::WatchForOwnerChange);
    connect(m_watcher.get(), &QDBusServiceWatcher::serviceOwnerChanged,
            this, &NativeProfileAuthority::resolve);
    if (!m_connection.connect({}, QStringLiteral("/org/freedesktop/DBus/Local"),
            QStringLiteral("org.freedesktop.DBus.Local"), QStringLiteral("Disconnected"),
            this, SLOT(disconnected()))) return;
    resolve();
}
void NativeProfileAuthority::resolve()
{
    if (!m_connection.isConnected() || !m_connection.interface()) {
        disconnected();
        return;
    }
    const QDBusReply<bool> owned = m_connection.interface()->isServiceRegistered(legacyName);
    const bool admitted = m_exclusive && owned.isValid() && !owned.value();
    if (m_admitted == admitted) return;
    m_admitted = admitted;
    Q_EMIT admissionChanged(admitted);
}
void NativeProfileAuthority::disconnected()
{
    if (!m_admitted) return;
    m_admitted = false;
    Q_EMIT admissionChanged(false);
}
}
