// SPDX-License-Identifier: LGPL-3.0-or-later

#include "appmenu_registrar_object_p.h"

#include <QtDBus/QDBusError>
#include <QtDBus/QDBusMessage>

namespace QindaQt::Shell::GlobalMenu::Registrar
{

namespace
{

constexpr auto kInvalidArgsError = "com.canonical.AppMenu.Registrar.Error.InvalidArguments";
constexpr auto kAccessDeniedError = "com.canonical.AppMenu.Registrar.Error.AccessDenied";
constexpr auto kLimitsError = "com.canonical.AppMenu.Registrar.Error.LimitsExceeded";

} // namespace

AppMenuRegistrarObject::AppMenuRegistrarObject(RegistrarRegistry &registry, QObject *parent)
    : QObject(parent)
    , m_registry(registry)
{
    connect(&m_registry, &RegistrarRegistry::windowRegistered, this,
            [this](const AppMenuRegistration &registration) {
                Q_EMIT WindowRegistered(registration.windowId, registration.ownerUniqueName,
                                        registration.menuObjectPath);
            });
    connect(&m_registry, &RegistrarRegistry::windowUnregistered, this,
            [this](quint32 windowId, const QString &) { Q_EMIT WindowUnregistered(windowId); });
}

void AppMenuRegistrarObject::RegisterWindow(quint32 windowId,
                                            const QDBusObjectPath &menuObjectPath)
{
    const QString caller = calledFromDBus() ? message().service() : QString{};
    const RegistrationResult result = m_registry.registerWindow(windowId, caller, menuObjectPath);
    switch (result.outcome) {
    case RegistrationOutcome::Registered:
    case RegistrationOutcome::Updated:
    case RegistrationOutcome::Unchanged:
        return;
    case RegistrationOutcome::Invalid:
        sendErrorReply(QString::fromLatin1(kInvalidArgsError),
                       QStringLiteral("window id, caller, or object path is invalid"));
        return;
    case RegistrationOutcome::OwnedByAnotherPeer:
        sendErrorReply(QString::fromLatin1(kAccessDeniedError),
                       QStringLiteral("window is owned by another unique peer"));
        return;
    case RegistrationOutcome::CapacityExceeded:
    case RegistrationOutcome::GenerationExhausted:
        sendErrorReply(QString::fromLatin1(kLimitsError),
                       QStringLiteral("registrar capacity or generation exhausted"));
        return;
    }
}

void AppMenuRegistrarObject::UnregisterWindow(quint32 windowId)
{
    const QString caller = calledFromDBus() ? message().service() : QString{};
    const RemovalOutcome outcome = m_registry.unregisterWindow(windowId, caller);
    if (outcome == RemovalOutcome::NotOwner) {
        sendErrorReply(QString::fromLatin1(kAccessDeniedError),
                       QStringLiteral("only the registering unique peer may unregister a window"));
    } else if (outcome == RemovalOutcome::NotFound) {
        sendErrorReply(QDBusError::UnknownObject, QStringLiteral("window is not registered"));
    }
}

void AppMenuRegistrarObject::GetMenuForWindow(quint32 windowId, QString &service,
                                              QDBusObjectPath &menuObjectPath)
{
    const std::optional<AppMenuRegistration> registration = m_registry.registrationFor(windowId);
    if (!registration) {
        service.clear();
        menuObjectPath = QDBusObjectPath(QStringLiteral("/"));
        return;
    }
    service = registration->ownerUniqueName;
    menuObjectPath = registration->menuObjectPath;
}

RegistrarMenuList AppMenuRegistrarObject::GetMenus()
{
    return m_registry.menus();
}

} // namespace QindaQt::Shell::GlobalMenu::Registrar
