// SPDX-License-Identifier: LGPL-3.0-or-later

#include "status_notifier_watcher_object_p.h"

#include <qindaqt/shell/status_notifier/watcher/status_notifier_watcher_service.h>

#include <QDBusError>
#include <QDBusMessage>

namespace QindaQt::StatusNotifier
{

StatusNotifierWatcherObject::StatusNotifierWatcherObject(
    StatusNotifierWatcherService &service)
    : m_service(service)
{
}

QStringList StatusNotifierWatcherObject::registeredStatusNotifierItems() const
{
    return m_service.registeredItemServiceIds();
}

bool StatusNotifierWatcherObject::isStatusNotifierHostRegistered() const
{
    return !m_service.registeredHosts().isEmpty();
}

int StatusNotifierWatcherObject::protocolVersion() const
{
    // Protocol version 0 is the KDE StatusNotifier protocol revision served by
    // KStatusNotifierWatcher; hosts compare against this value.
    return 0;
}

void StatusNotifierWatcherObject::RegisterStatusNotifierItem(const QString &serviceOrPath)
{
    const auto attempt = m_service.registerItem(message().service(), serviceOrPath);
    if (!attempt.errorMessage.isEmpty()) {
        sendErrorReply(QDBusError::InvalidArgs, attempt.errorMessage);
    }
}

void StatusNotifierWatcherObject::RegisterStatusNotifierHost(const QString &service)
{
    const auto attempt = m_service.registerHost(service);
    if (!attempt.errorMessage.isEmpty()) {
        sendErrorReply(QDBusError::InvalidArgs, attempt.errorMessage);
    }
}

} // namespace QindaQt::StatusNotifier
