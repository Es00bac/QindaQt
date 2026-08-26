// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/services/notification_presentation/presentation_access_token.h"
#include "qindaqt/services/notifications/notification_types.h"

#include <QDBusConnection>
#include <QDBusContext>
#include <QDBusServiceWatcher>
#include <QObject>
#include <QString>
#include <QVariantMap>

namespace QindaQt::Services::Notifications {
class NotificationService;
}

namespace QindaQt::Services::NotificationHost::Private {

class NotificationPresentationObject final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.NotificationPresentation1")

public:
    NotificationPresentationObject(
        QDBusConnection connection,
        Notifications::NotificationService &service,
        NotificationPresentation::PresentationAccessToken accessToken,
        QString epoch,
        QObject *parent = nullptr);

    void clearPresenter() noexcept;
    void publishRevision(quint64 revision);

public slots:
    [[nodiscard]] QVariantMap RegisterPresenter(const QString &accessToken);
    void ReleasePresenter();
    [[nodiscard]] QVariantMap GetSnapshot();
    [[nodiscard]] QVariantMap Dismiss(quint32 id);
    [[nodiscard]] QVariantMap InvokeAction(quint32 id,
                                           const QString &actionKey,
                                           const QString &activationToken);

private:
    [[nodiscard]] bool authorizeCall();
    [[nodiscard]] QVariantMap encodedSnapshot();
    [[nodiscard]] QVariantMap operationResult(
        const Notifications::NotificationOperationResult &result);
    void sendOperationError(const Notifications::NotificationOperationResult &result);

    QDBusConnection m_connection;
    Notifications::NotificationService &m_service;
    NotificationPresentation::PresentationAccessToken m_accessToken;
    QString m_epoch;
    QString m_presenter;
    QDBusServiceWatcher m_presenterWatcher;
};

} // namespace QindaQt::Services::NotificationHost::Private
