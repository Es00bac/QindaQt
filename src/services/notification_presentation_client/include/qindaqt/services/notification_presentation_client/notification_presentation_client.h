// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/services/notification_presentation/presentation_access_token.h"
#include "qindaqt/services/notification_presentation/presentation_snapshot.h"

#include <QObject>
#include <QTimer>
#include <QVector>

#include <optional>

namespace QindaQt::Services::NotificationPresentationClient {

class PresentationTransport;

struct ClientTiming final {
    int debounceMilliseconds = 16;
    int requestTimeoutMilliseconds = 2'000;
    QVector<int> retryMilliseconds = {100, 250, 500, 1'000, 2'000, 5'000};

    [[nodiscard]] bool isValid() const noexcept;
};

enum class ClientState {
    Unavailable,
    Authenticating,
    Ready,
    Degraded,
};

// AGENT-CONTRACT: this policy object owns authentication, owner lineage,
// coalescing, retry, and decoding. It never performs a blocking D-Bus call.
class NotificationPresentationClient final : public QObject {
    Q_OBJECT

public:
    NotificationPresentationClient(
        PresentationTransport &transport,
        NotificationPresentation::PresentationAccessToken accessToken,
        ClientTiming timing = {}, QObject *parent = nullptr);
    ~NotificationPresentationClient() override;

    [[nodiscard]] bool start(QString *error = nullptr);
    void stop();

    [[nodiscard]] ClientState state() const noexcept;
    [[nodiscard]] const QString &lastError() const noexcept;
    [[nodiscard]] const std::optional<NotificationPresentation::PresentationSnapshot> &
    snapshot() const noexcept;
    [[nodiscard]] bool requestInFlight() const noexcept;
    [[nodiscard]] bool operationInFlight() const noexcept;

    [[nodiscard]] bool dismiss(quint32 id, QString *error = nullptr);
    [[nodiscard]] bool invokeAction(quint32 id, const QString &actionKey,
                                    const QString &activationToken,
                                    QString *error = nullptr);

Q_SIGNALS:
    void stateChanged();
    void operationSucceeded(quint32 notificationId);
    void operationRejected(quint32 notificationId, const QString &message);

private:
    enum class RequestKind { Register, Snapshot };
    struct InFlightRequest final {
        quint64 token = 0;
        QString owner;
        RequestKind kind = RequestKind::Register;
    };
    struct InFlightOperation final {
        quint64 token = 0;
        QString owner;
        quint32 notificationId = 0;
    };

    void handleOwnerChanged(const QString &uniqueOwner);
    void handleInvalidation(const QString &uniqueOwner, const QString &epoch,
                            quint64 revision);
    void handleSnapshot(quint64 token, const QString &uniqueOwner,
                        const QVariantMap &wire);
    void handleRequestFailure(quint64 token, const QString &uniqueOwner,
                              const QString &errorName, const QString &message);
    void handleOperationResult(quint64 token, const QString &uniqueOwner,
                               const QVariantMap &result);
    void handleOperationFailure(quint64 token, const QString &uniqueOwner,
                                const QString &errorName, const QString &message);
    void scheduleRequest(int milliseconds);
    void scheduleRetry();
    void requestNow();
    void failCurrentRequest(QString message, bool authenticationFailed);
    void publish(ClientState state, QString error = {});
    [[nodiscard]] bool validateOperation(quint32 id, const QString *actionKey,
                                         const QString *activationToken,
                                         QString *error) const;
    [[nodiscard]] quint64 nextToken();

    PresentationTransport &m_transport;
    NotificationPresentation::PresentationAccessToken m_accessToken;
    ClientTiming m_timing;
    QTimer m_refreshTimer;
    QTimer m_requestTimeout;
    QTimer m_operationTimeout;
    std::optional<InFlightRequest> m_request;
    std::optional<InFlightOperation> m_operation;
    std::optional<NotificationPresentation::PresentationSnapshot> m_snapshot;
    QString m_owner;
    QString m_lastError;
    ClientState m_state = ClientState::Unavailable;
    qsizetype m_retryIndex = 0;
    quint64 m_nextToken = 1;
    quint64 m_targetRevision = 0;
    bool m_authenticated = false;
    bool m_dirty = false;
    bool m_started = false;
};

} // namespace QindaQt::Services::NotificationPresentationClient
