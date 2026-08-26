// SPDX-License-Identifier: LGPL-3.0-or-later
#include "notification_presentation_object_p.h"

#include "qindaqt/services/notification_presentation/presentation_snapshot.h"
#include "qindaqt/services/notification_presentation/wire_contract.h"
#include "qindaqt/services/notifications/notification_service.h"

#include <QDBusMessage>

#include <utility>

namespace QindaQt::Services::NotificationHost::Private {
namespace {

using namespace QindaQt::Services;

NotificationPresentation::PresentationSnapshot presentationSnapshot(
    const Notifications::NotificationModelSnapshot &source,
    const QString &epoch)
{
    NotificationPresentation::PresentationSnapshot result;
    result.epoch = epoch;
    result.revision = source.revision;
    result.notifications.reserve(source.notifications.size());
    for (const auto &notification : source.notifications) {
        NotificationPresentation::PresentationNotification item;
        item.id = notification.id;
        item.applicationName = notification.applicationName;
        item.applicationIcon = notification.applicationIcon;
        item.summary = notification.summary;
        item.body = notification.body;
        item.urgency = quint32(notification.hints.urgency);
        item.desktopEntry = notification.hints.desktopEntry;
        item.imagePath = notification.hints.imagePath;
        item.resident = notification.hints.resident;
        item.transient = notification.hints.transient;
        item.createdAtMs = notification.createdAtMs;
        item.updatedAtMs = notification.updatedAtMs;
        item.expiresAtMs = notification.expiresAtMs;
        item.actions.reserve(notification.actions.size());
        for (const auto &action : notification.actions) {
            item.actions.append({action.key, action.label});
        }
        result.notifications.append(std::move(item));
    }
    return result;
}

QString operationErrorName(Notifications::OperationStatus status)
{
    using Status = Notifications::OperationStatus;
    switch (status) {
    case Status::InvalidRequest:
    case Status::UnknownAction:
        return QStringLiteral("org.freedesktop.DBus.Error.InvalidArgs");
    case Status::NotFound:
        return QStringLiteral("org.qindaqt.NotificationPresentation1.Error.NotFound");
    case Status::CapacityReached:
    case Status::ReentrantOperation:
        return QStringLiteral("org.freedesktop.DBus.Error.LimitsExceeded");
    case Status::Applied:
    case Status::InvalidPolicy:
    case Status::NotOwner:
    case Status::RevisionExhausted:
    case Status::ClockFailure:
        return QStringLiteral("org.freedesktop.DBus.Error.Failed");
    }
    return QStringLiteral("org.freedesktop.DBus.Error.Failed");
}

} // namespace

NotificationPresentationObject::NotificationPresentationObject(
    QDBusConnection connection,
    Notifications::NotificationService &service,
    NotificationPresentation::PresentationAccessToken accessToken,
    QString epoch,
    QObject *parent)
    : QObject(parent)
    , m_connection(std::move(connection))
    , m_service(service)
    , m_accessToken(std::move(accessToken))
    , m_epoch(std::move(epoch))
    , m_presenterWatcher(QString{}, m_connection,
                         QDBusServiceWatcher::WatchForUnregistration,
                         this)
{
    connect(&m_presenterWatcher, &QDBusServiceWatcher::serviceUnregistered,
            this, [this](const QString &serviceName) {
                if (serviceName == m_presenter) {
                    clearPresenter();
                }
            });
}

void NotificationPresentationObject::clearPresenter() noexcept
{
    if (!m_presenter.isEmpty()) {
        m_presenterWatcher.removeWatchedService(m_presenter);
        m_presenter.clear();
    }
}

void NotificationPresentationObject::publishRevision(quint64 revision)
{
    if (m_presenter.isEmpty()) {
        return;
    }
    auto signal = QDBusMessage::createTargetedSignal(
        m_presenter,
        QString::fromLatin1(NotificationPresentation::WireContract::ObjectPath),
        QString::fromLatin1(NotificationPresentation::WireContract::InterfaceName),
        QStringLiteral("SnapshotChanged"));
    signal.setArguments({m_epoch, revision});
    const bool sent = m_connection.send(signal);
    Q_UNUSED(sent)
}

QVariantMap NotificationPresentationObject::RegisterPresenter(const QString &accessToken)
{
    const QString sender = calledFromDBus() ? message().service() : QString{};
    if (sender.isEmpty() || !sender.startsWith(QLatin1Char(':')) ||
        (!m_presenter.isEmpty() && m_presenter != sender) ||
        !m_accessToken.matches(accessToken)) {
        sendErrorReply(QStringLiteral("org.freedesktop.DBus.Error.AccessDenied"),
                       QStringLiteral("notification presenter authentication failed"));
        return {};
    }
    if (m_presenter.isEmpty()) {
        m_presenter = sender;
        m_presenterWatcher.addWatchedService(sender);
    }
    return encodedSnapshot();
}

void NotificationPresentationObject::ReleasePresenter()
{
    if (authorizeCall()) {
        clearPresenter();
    }
}

QVariantMap NotificationPresentationObject::GetSnapshot()
{
    return authorizeCall() ? encodedSnapshot() : QVariantMap{};
}

QVariantMap NotificationPresentationObject::Dismiss(quint32 id)
{
    if (!authorizeCall()) {
        return {};
    }
    const auto result = m_service.dismiss(id);
    if (!result.ok()) {
        sendOperationError(result);
        return {};
    }
    return operationResult(result);
}

QVariantMap NotificationPresentationObject::InvokeAction(
    quint32 id,
    const QString &actionKey,
    const QString &activationToken)
{
    if (!authorizeCall()) {
        return {};
    }
    const auto result = m_service.invokeAction(id, actionKey, activationToken);
    if (!result.ok()) {
        sendOperationError(result);
        return {};
    }
    return operationResult(result);
}

bool NotificationPresentationObject::authorizeCall()
{
    const QString sender = calledFromDBus() ? message().service() : QString{};
    if (!m_presenter.isEmpty() && sender == m_presenter) {
        return true;
    }
    sendErrorReply(QStringLiteral("org.freedesktop.DBus.Error.AccessDenied"),
                   QStringLiteral("caller is not the registered notification presenter"));
    return false;
}

QVariantMap NotificationPresentationObject::encodedSnapshot()
{
    const auto source = m_service.snapshot();
    if (!source) {
        sendErrorReply(QStringLiteral("org.freedesktop.DBus.Error.Failed"),
                       QStringLiteral("notification snapshot is unavailable"));
        return {};
    }
    const QVariantMap wire = NotificationPresentation::PresentationSnapshotCodec::encode(
        presentationSnapshot(*source, m_epoch));
    if (!NotificationPresentation::PresentationSnapshotCodec::decode(wire).ok()) {
        sendErrorReply(QStringLiteral("org.freedesktop.DBus.Error.LimitsExceeded"),
                       QStringLiteral("notification snapshot exceeds the presentation contract"));
        return {};
    }
    return wire;
}

QVariantMap NotificationPresentationObject::operationResult(
    const Notifications::NotificationOperationResult &result)
{
    return {{QStringLiteral("status"), Notifications::operationStatusName(result.status)},
            {QStringLiteral("revisionBefore"), result.revisionBefore},
            {QStringLiteral("revisionAfter"), result.revisionAfter},
            {QStringLiteral("notificationId"), result.notificationId}};
}

void NotificationPresentationObject::sendOperationError(
    const Notifications::NotificationOperationResult &result)
{
    sendErrorReply(operationErrorName(result.status), result.message);
}

} // namespace QindaQt::Services::NotificationHost::Private
