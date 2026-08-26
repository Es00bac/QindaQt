// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/notification_host/notification_presentation_server.h"

#include "notification_presentation_object_p.h"

#include "qindaqt/services/notification_presentation/wire_contract.h"
#include "qindaqt/services/notifications/notification_service.h"

#include <QDBusError>
#include <QUuid>

#include <utility>

namespace QindaQt::Services::NotificationHost {

class NotificationPresentationServer::Private final {
public:
    Private(QDBusConnection selectedConnection,
            Notifications::NotificationService &service,
            NotificationPresentation::PresentationAccessToken accessToken)
        : connection(std::move(selectedConnection))
        , object(connection, service, std::move(accessToken),
                 QUuid::createUuid().toString(QUuid::WithoutBraces))
    {
    }

    QDBusConnection connection;
    NotificationHost::Private::NotificationPresentationObject object;
    QString lastError;
    bool running = false;
};

NotificationPresentationServer::NotificationPresentationServer(
    QDBusConnection connection,
    Notifications::NotificationService &service,
    NotificationPresentation::PresentationAccessToken accessToken)
    : d(std::make_unique<Private>(std::move(connection), service,
                                  std::move(accessToken)))
{
}

NotificationPresentationServer::~NotificationPresentationServer()
{
    stop();
}

bool NotificationPresentationServer::start()
{
    if (d->running) {
        return true;
    }
    d->lastError.clear();
    if (!d->connection.isConnected()) {
        d->lastError = QStringLiteral("notification presentation D-Bus connection is unavailable");
        return false;
    }
    d->running = d->connection.registerObject(
        QString::fromLatin1(NotificationPresentation::WireContract::ObjectPath),
        &d->object,
        QDBusConnection::ExportAllSlots);
    if (!d->running) {
        d->lastError = d->connection.lastError().message();
    }
    return d->running;
}

void NotificationPresentationServer::stop() noexcept
{
    d->object.clearPresenter();
    if (d->running) {
        d->connection.unregisterObject(
            QString::fromLatin1(NotificationPresentation::WireContract::ObjectPath));
        d->running = false;
    }
}

void NotificationPresentationServer::publishRevision(quint64 revision)
{
    if (d->running) {
        d->object.publishRevision(revision);
    }
}

bool NotificationPresentationServer::isRunning() const noexcept
{
    return d->running;
}

const QString &NotificationPresentationServer::lastError() const noexcept
{
    return d->lastError;
}

} // namespace QindaQt::Services::NotificationHost
