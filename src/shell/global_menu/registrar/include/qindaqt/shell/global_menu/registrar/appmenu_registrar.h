// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/shell/global_menu/registrar/registrar_registry.h>

#include <QtCore/QObject>
#include <QtDBus/QDBusConnection>

#include <memory>

class QDBusServiceWatcher;

namespace QindaQt::Shell::GlobalMenu::Registrar
{

class AppMenuRegistrarObject;

enum class RegistrarStartStatus {
    Started,
    InvalidConnection,
    ObjectRegistrationFailed,
    NameAlreadyOwned,
    NameRegistrationFailed,
};

// Composition root for the standard registrar name and object. It requests
// name ownership only from explicit start(), rolls back partial startup, and
// releases both name and object on stop/destruction. The named connection and
// this object must live on one Qt thread.
class AppMenuRegistrar final : public QObject
{
    Q_OBJECT

public:
    explicit AppMenuRegistrar(QDBusConnection connection, QObject *parent = nullptr);
    ~AppMenuRegistrar() override;

    [[nodiscard]] RegistrarStartStatus start();
    void stop();
    [[nodiscard]] bool isRunning() const noexcept;
    [[nodiscard]] RegistrarRegistry *registry() noexcept;
    void setHostedMenu(const QString &providerUniqueName,
                       const QString &objectPath, bool hosted);
    void clearHostedMenus();

private:
    QDBusConnection m_connection;
    std::unique_ptr<RegistrarRegistry> m_registry;
    std::unique_ptr<AppMenuRegistrarObject> m_serviceObject;
    QDBusServiceWatcher *m_ownerWatcher = nullptr;
    bool m_objectRegistered = false;
    bool m_nameRegistered = false;
};

} // namespace QindaQt::Shell::GlobalMenu::Registrar
