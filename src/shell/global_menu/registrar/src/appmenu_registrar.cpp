// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/shell/global_menu/registrar/appmenu_registrar.h>

#include "appmenu_registrar_object_p.h"

#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusServiceWatcher>
#include <utility>

namespace QindaQt::Shell::GlobalMenu::Registrar
{

AppMenuRegistrar::AppMenuRegistrar(QDBusConnection connection, QObject *parent)
    : QObject(parent)
    , m_connection(std::move(connection))
    , m_registry(std::make_unique<RegistrarRegistry>())
    , m_serviceObject(std::make_unique<AppMenuRegistrarObject>(*m_registry))
{
    registerRegistrarWireTypes();
    m_ownerWatcher = new QDBusServiceWatcher(
        {}, m_connection, QDBusServiceWatcher::WatchForUnregistration, this);
    connect(m_registry.get(), &RegistrarRegistry::ownerBecamePresent, this,
            [this](const QString &owner, quint64) { m_ownerWatcher->addWatchedService(owner); });
    connect(m_registry.get(), &RegistrarRegistry::ownerBecameAbsent, this,
            [this](const QString &owner) { m_ownerWatcher->removeWatchedService(owner); });
    connect(m_ownerWatcher, &QDBusServiceWatcher::serviceUnregistered, this,
            [this](const QString &owner) {
                const std::optional<quint64> generation = m_registry->ownerGeneration(owner);
                if (generation) {
                    (void)m_registry->retireOwner(owner, *generation);
                }
            });
}

AppMenuRegistrar::~AppMenuRegistrar()
{
    stop();
}

RegistrarStartStatus AppMenuRegistrar::start()
{
    if (isRunning()) {
        return RegistrarStartStatus::Started;
    }
    if (!m_connection.isConnected() || m_connection.interface() == nullptr) {
        return RegistrarStartStatus::InvalidConnection;
    }
    if (!m_connection.registerObject(
            QString::fromLatin1(kRegistrarObjectPath), m_serviceObject.get(),
            QDBusConnection::ExportScriptableSlots | QDBusConnection::ExportScriptableSignals)) {
        return RegistrarStartStatus::ObjectRegistrationFailed;
    }
    m_objectRegistered = true;
    if (!m_connection.registerService(QString::fromLatin1(kRegistrarServiceName))) {
        const QString owner = m_connection.interface()
                                  ->serviceOwner(QString::fromLatin1(kRegistrarServiceName))
                                  .value();
        stop();
        return owner.isEmpty() ? RegistrarStartStatus::NameRegistrationFailed
                               : RegistrarStartStatus::NameAlreadyOwned;
    }
    m_nameRegistered = true;
    return RegistrarStartStatus::Started;
}

void AppMenuRegistrar::stop()
{
    if (m_nameRegistered) {
        m_connection.unregisterService(QString::fromLatin1(kRegistrarServiceName));
        m_nameRegistered = false;
    }
    if (m_objectRegistered) {
        m_connection.unregisterObject(QString::fromLatin1(kRegistrarObjectPath));
        m_objectRegistered = false;
    }
}

bool AppMenuRegistrar::isRunning() const noexcept
{
    return m_nameRegistered && m_objectRegistered;
}

RegistrarRegistry *AppMenuRegistrar::registry() noexcept
{
    return m_registry.get();
}

} // namespace QindaQt::Shell::GlobalMenu::Registrar
