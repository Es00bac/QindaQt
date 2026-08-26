// SPDX-License-Identifier: LGPL-3.0-or-later

#include "freedesktop_notification_object_p.h"

#include "freedesktop_notification_codec_p.h"

#include <QDBusMessage>

namespace QindaQt::Services::Notifications::Private {
namespace {

QString errorName(OperationStatus status)
{
    switch (status) {
    case OperationStatus::InvalidRequest:
        return QStringLiteral("org.freedesktop.DBus.Error.InvalidArgs");
    case OperationStatus::NotFound:
        return QStringLiteral("org.freedesktop.Notifications.Error.InvalidNotification");
    case OperationStatus::NotOwner:
        return QStringLiteral("org.freedesktop.DBus.Error.AccessDenied");
    case OperationStatus::CapacityReached:
        return QStringLiteral("org.freedesktop.DBus.Error.LimitsExceeded");
    case OperationStatus::ReentrantOperation:
        return QStringLiteral("org.freedesktop.DBus.Error.LimitsExceeded");
    case OperationStatus::Applied:
    case OperationStatus::InvalidPolicy:
    case OperationStatus::UnknownAction:
    case OperationStatus::RevisionExhausted:
    case OperationStatus::ClockFailure:
        return QStringLiteral("org.freedesktop.DBus.Error.Failed");
    }
    return QStringLiteral("org.freedesktop.DBus.Error.Failed");
}

} // namespace

FreedesktopNotificationObject::FreedesktopNotificationObject(
    NotificationService &service,
    const FreedesktopServerIdentity &identity,
    QObject *parent)
    : QObject(parent)
    , m_service(service)
    , m_identity(identity)
{
}

QStringList FreedesktopNotificationObject::GetCapabilities() const
{
    return m_identity.capabilities;
}

quint32 FreedesktopNotificationObject::Notify(const QString &applicationName,
                                              quint32 replacesId,
                                              const QString &applicationIcon,
                                              const QString &summary,
                                              const QString &body,
                                              const QStringList &actions,
                                              const QVariantMap &hints,
                                              int expireTimeoutMs)
{
    NotificationRequest request;
    QString error;
    const QString sourceService = calledFromDBus() ? message().service() : QString{};
    if (!decodeFreedesktopRequest(sourceService,
                                 applicationName,
                                 replacesId,
                                 applicationIcon,
                                 summary,
                                 body,
                                 actions,
                                 hints,
                                 expireTimeoutMs,
                                 &request,
                                 &error)) {
        sendErrorReply(QStringLiteral("org.freedesktop.DBus.Error.InvalidArgs"), error);
        return 0;
    }

    const auto result = m_service.submit(request);
    if (!result.ok()) {
        sendOperationError(result);
        return 0;
    }
    return result.notificationId;
}

void FreedesktopNotificationObject::CloseNotification(quint32 id)
{
    const QString sourceService = calledFromDBus() ? message().service() : QString{};
    const auto result = m_service.closeFromApplication(sourceService, id);
    if (!result.ok()) {
        sendOperationError(result);
    }
}

void FreedesktopNotificationObject::GetServerInformation(
    QString &name,
    QString &vendor,
    QString &version,
    QString &specificationVersion) const
{
    name = m_identity.name;
    vendor = m_identity.vendor;
    version = m_identity.version;
    specificationVersion = m_identity.specificationVersion;
}

void FreedesktopNotificationObject::sendOperationError(
    const NotificationOperationResult &result)
{
    if (result.status == OperationStatus::NotFound && calledFromDBus()) {
        // AGENT-CONTRACT: Notifications 1.3 requires missing-ID
        // CloseNotification errors to carry no body arguments. Qt's
        // sendErrorReply convenience always materializes a message string, so
        // construct and clear the reply explicitly.
        setDelayedReply(true);
        auto reply = message().createErrorReply(errorName(result.status), QString{});
        reply.setArguments({});
        const bool sent = connection().send(reply);
        Q_UNUSED(sent)
        return;
    }
    sendErrorReply(errorName(result.status), result.message);
}

} // namespace QindaQt::Services::Notifications::Private
