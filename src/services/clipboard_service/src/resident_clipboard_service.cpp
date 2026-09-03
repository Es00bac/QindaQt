// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/clipboard_service/resident_clipboard_service.h>

#include "clipboard_service_object_p.h"

#include <qindaqt/services/clipboard_protocol/clipboard_dbus.h>

#include <QtDBus/QDBusConnectionInterface>

#include <utility>

namespace QindaQt::Services::Clipboard {

ResidentClipboardService::ResidentClipboardService(
    std::unique_ptr<ClipboardWayland::ClipboardWaylandAdapter> adapter,
    const QDBusConnection &connection, QString serviceName, quint64 epochSeed,
    QObject *parent)
    : QObject(parent), m_adapter(std::move(adapter)), m_connection(connection)
    , m_serviceName(serviceName.isEmpty() ? QString::fromLatin1(kServiceName)
                                          : std::move(serviceName))
{
    Q_ASSERT(m_adapter != nullptr);
    registerDBusTypes();
    m_host = std::make_unique<ClipboardHost>(m_adapter.get(), epochSeed);
    m_object = std::make_unique<ClipboardServiceObject>(m_host.get());
}

ResidentClipboardService::~ResidentClipboardService() { stop(); }

ServiceStartStatus ResidentClipboardService::start()
{
    if (isRunning()) {
        return ServiceStartStatus::Started;
    }
    if (!m_connection.isConnected() || m_connection.interface() == nullptr) {
        return ServiceStartStatus::InvalidConnection;
    }
    if (!m_ownerWatchInstalled) {
        const bool watched = m_connection.connect(
            QString{}, QStringLiteral("/org/freedesktop/DBus"),
            QStringLiteral("org.freedesktop.DBus"), QStringLiteral("NameOwnerChanged"),
            this, SLOT(onNameOwnerChanged(QString,QString,QString)));
        if (!watched) {
            return ServiceStartStatus::InvalidConnection;
        }
        m_ownerWatchInstalled = true;
    }
    if (!m_connection.registerObject(QString::fromLatin1(kObjectPath), m_object.get(),
                                     QDBusConnection::ExportScriptableSlots
                                         | QDBusConnection::ExportScriptableSignals)) {
        return ServiceStartStatus::ObjectRegistrationFailed;
    }
    m_objectRegistered = true;
    if (!m_connection.registerService(m_serviceName)) {
        const bool exists = m_connection.interface()->isServiceRegistered(m_serviceName);
        stop();
        return exists ? ServiceStartStatus::NameAlreadyOwned
                      : ServiceStartStatus::NameRegistrationFailed;
    }
    m_nameRegistered = true;
    return ServiceStartStatus::Started;
}

void ResidentClipboardService::stop()
{
    m_host->setUnlocked(false);
    m_host->setHistoryOptIn(false);
    m_adapter->stop();
    if (m_nameRegistered) {
        m_connection.unregisterService(m_serviceName);
        m_nameRegistered = false;
    }
    if (m_objectRegistered) {
        m_connection.unregisterObject(QString::fromLatin1(kObjectPath));
        m_objectRegistered = false;
    }
    if (m_ownerWatchInstalled) {
        m_connection.disconnect(QString{}, QStringLiteral("/org/freedesktop/DBus"),
                                QStringLiteral("org.freedesktop.DBus"),
                                QStringLiteral("NameOwnerChanged"), this,
                                SLOT(onNameOwnerChanged(QString,QString,QString)));
        m_ownerWatchInstalled = false;
    }
}

bool ResidentClipboardService::isRunning() const noexcept
{
    return m_nameRegistered && m_objectRegistered;
}

void ResidentClipboardService::onNameOwnerChanged(const QString &name,
                                                  const QString &oldOwner,
                                                  const QString &newOwner)
{
    if (name.startsWith(QLatin1Char(':')) && !oldOwner.isEmpty() && newOwner.isEmpty()) {
        m_object->forgetCaller(name);
    }
}

} // namespace QindaQt::Services::Clipboard
