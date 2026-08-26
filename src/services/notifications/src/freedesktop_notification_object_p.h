// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/services/notifications/freedesktop_notification_server.h"

#include <QDBusContext>
#include <QObject>
#include <QVariantMap>

namespace QindaQt::Services::Notifications::Private {

class FreedesktopNotificationObject final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Notifications")

public:
    FreedesktopNotificationObject(NotificationService &service,
                                  const FreedesktopServerIdentity &identity,
                                  QObject *parent = nullptr);

public slots:
    [[nodiscard]] QStringList GetCapabilities() const;
    [[nodiscard]] quint32 Notify(const QString &applicationName,
                                 quint32 replacesId,
                                 const QString &applicationIcon,
                                 const QString &summary,
                                 const QString &body,
                                 const QStringList &actions,
                                 const QVariantMap &hints,
                                 int expireTimeoutMs);
    void CloseNotification(quint32 id);
    void GetServerInformation(QString &name,
                              QString &vendor,
                              QString &version,
                              QString &specificationVersion) const;

private:
    void sendOperationError(const NotificationOperationResult &result);

    NotificationService &m_service;
    const FreedesktopServerIdentity &m_identity;
};

} // namespace QindaQt::Services::Notifications::Private
