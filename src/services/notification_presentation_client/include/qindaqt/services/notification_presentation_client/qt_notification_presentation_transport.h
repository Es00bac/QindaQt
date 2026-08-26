// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/services/notification_presentation_client/presentation_transport.h"

#include <QDBusConnection>
#include <QList>

class QDBusPendingCallWatcher;
class QDBusServiceWatcher;

namespace QindaQt::Services::NotificationPresentationClient {

class QtNotificationPresentationTransport final : public PresentationTransport {
    Q_OBJECT

public:
    explicit QtNotificationPresentationTransport(
        QDBusConnection connection = QDBusConnection::sessionBus(),
        QObject *parent = nullptr);
    ~QtNotificationPresentationTransport() override;

    [[nodiscard]] bool start(QString *error = nullptr) override;
    void stop() override;
    void registerPresenter(quint64 token, const QString &uniqueOwner,
                           const QString &accessToken) override;
    void requestSnapshot(quint64 token, const QString &uniqueOwner) override;
    void releasePresenter(const QString &uniqueOwner) override;
    void dismiss(quint64 token, const QString &uniqueOwner, quint32 id) override;
    void invokeAction(quint64 token, const QString &uniqueOwner, quint32 id,
                      const QString &actionKey,
                      const QString &activationToken) override;

private Q_SLOTS:
    void handleInvalidation(const QString &epoch, quint64 revision);

private:
    void resolveInitialOwner();
    void bindOwner(const QString &uniqueOwner);
    void requestMap(quint64 token, const QString &uniqueOwner,
                    const QString &method, const QVariantList &arguments,
                    bool operation);
    void failRequest(quint64 token, const QString &uniqueOwner,
                     QString errorName, QString message);

    QDBusConnection m_connection;
    QDBusServiceWatcher *m_serviceWatcher = nullptr;
    QList<QDBusPendingCallWatcher *> m_pendingCalls;
    QString m_uniqueOwner;
    quint64 m_resolutionGeneration = 0;
    bool m_started = false;
};

} // namespace QindaQt::Services::NotificationPresentationClient
