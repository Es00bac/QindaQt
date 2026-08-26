// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/services/notification_presentation/presentation_snapshot.h"
#include "qindaqt/services/notification_presentation_client/notification_presentation_client.h"
#include "qindaqt/services/notification_presentation_client/presentation_transport.h"

#include <QVariantMap>
#include <QVector>

#include <utility>

namespace QindaQt::Tests::NotificationPresentationClientSupport {

enum class RequestType { Register, Snapshot };

struct RecordedRequest final {
    quint64 token = 0;
    QString owner;
    RequestType type = RequestType::Register;
};

struct RecordedOperation final {
    quint64 token = 0;
    QString owner;
    quint32 id = 0;
    QString actionKey;
    QString activationToken;
};

inline QVariantMap operationResult(quint64 before, quint64 after,
                                   quint32 notificationId = 7)
{
    return {{QStringLiteral("status"), QStringLiteral("applied")},
            {QStringLiteral("revisionBefore"), before},
            {QStringLiteral("revisionAfter"), after},
            {QStringLiteral("notificationId"), notificationId}};
}

class FakeTransport final
    : public Services::NotificationPresentationClient::PresentationTransport {
public:
    using PresentationTransport::PresentationTransport;

    bool start(QString *error) override
    {
        ++startCalls;
        if (!startSucceeds) {
            if (error != nullptr) {
                *error = QStringLiteral("injected transport failure");
            }
            return false;
        }
        running = true;
        return true;
    }

    void stop() override
    {
        ++stopCalls;
        running = false;
    }

    void registerPresenter(quint64 token, const QString &owner,
                           const QString &accessToken) override
    {
        requests.append({token, owner, RequestType::Register});
        observedAccessToken = accessToken;
    }

    void requestSnapshot(quint64 token, const QString &owner) override
    {
        requests.append({token, owner, RequestType::Snapshot});
    }

    void releasePresenter(const QString &owner) override
    {
        releasedOwners.append(owner);
    }

    void dismiss(quint64 token, const QString &owner, quint32 id) override
    {
        operations.append({token, owner, id, {}, {}});
    }

    void invokeAction(quint64 token, const QString &owner, quint32 id,
                      const QString &actionKey,
                      const QString &activationToken) override
    {
        operations.append({token, owner, id, actionKey, activationToken});
    }

    void announceOwner(const QString &owner)
    {
        Q_EMIT serviceOwnerChanged(owner);
    }

    void invalidate(const QString &owner, const QString &epoch,
                    quint64 revision)
    {
        Q_EMIT snapshotInvalidated(owner, epoch, revision);
    }

    void reply(const RecordedRequest &request, const QVariantMap &wire)
    {
        Q_EMIT snapshotReceived(request.token, request.owner, wire);
    }

    void fail(const RecordedRequest &request, const QString &errorName,
              const QString &message)
    {
        Q_EMIT requestFailed(request.token, request.owner, errorName, message);
    }

    void finish(const RecordedOperation &operation, quint64 before,
                quint64 after)
    {
        finishRaw(operation, operationResult(before, after, operation.id));
    }

    void finishRaw(const RecordedOperation &operation, const QVariantMap &result)
    {
        Q_EMIT operationFinished(operation.token, operation.owner, result);
    }

    void reject(const RecordedOperation &operation, const QString &errorName,
                const QString &message)
    {
        Q_EMIT operationFailed(operation.token, operation.owner,
                               errorName, message);
    }

    QVector<RecordedRequest> requests;
    QVector<RecordedOperation> operations;
    QVector<QString> releasedOwners;
    QString observedAccessToken;
    int startCalls = 0;
    int stopCalls = 0;
    bool startSucceeds = true;
    bool running = false;
};

inline Services::NotificationPresentation::PresentationAccessToken accessToken()
{
    QString error;
    auto result =
        Services::NotificationPresentation::PresentationAccessToken::fromHex(
            QString(64, QLatin1Char('a')), &error);
    Q_ASSERT(result.has_value());
    return std::move(*result);
}

inline QVariantMap snapshotWire(
    const QString &epoch, quint64 revision,
    QString summary = QStringLiteral("Build complete"), bool resident = false)
{
    Services::NotificationPresentation::PresentationNotification notification;
    notification.id = 7;
    notification.applicationName = QStringLiteral("Builder");
    notification.summary = std::move(summary);
    notification.body = QStringLiteral("The requested build passed.");
    notification.resident = resident;
    notification.createdAtMs = 100;
    notification.actions = {{QStringLiteral("open"), QStringLiteral("Open")}};
    return Services::NotificationPresentation::PresentationSnapshotCodec::encode(
        {epoch, revision, {notification}});
}

inline Services::NotificationPresentationClient::ClientTiming fastTiming()
{
    return {.debounceMilliseconds = 2,
            .requestTimeoutMilliseconds = 20,
            .retryMilliseconds = {3, 6, 12}};
}

} // namespace QindaQt::Tests::NotificationPresentationClientSupport
