// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/network_model/network_model.h>
#include <qindaqt/services/network_model/network_model_state.h>
#include <qindaqt/services/network_protocol/network_types.h>

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QTimer>
#include <QtCore/QVector>

#include <optional>

namespace QindaQt::Network::Client {

class NetworkTransport;

enum class ClientState { Unavailable, Connecting, Ready, Degraded };

struct ClientTiming final {
    int requestTimeoutMilliseconds = 5'000;
    QVector<int> retryMilliseconds{250, 1'000, 5'000};
    [[nodiscard]] bool isValid() const noexcept;
};

// Owns one exact Network owner/epoch lineage and its NetworkModel. All calls
// are asynchronous; timeouts, owner replacement, and teardown fence late
// replies by token, exact decoded owner, and a lineage high-water that survives
// owner loss. A timed-out or interrupted mutation is reported uncertain and
// never automatically replayed. Thread-confined to the Qt main loop; the
// client owns no platform objects beyond its timers.
class NetworkClient final : public QObject {
    Q_OBJECT
public:
    NetworkClient(NetworkTransport &transport,
                  Model::MonotonicClock clock = {}, ClientTiming timing = {},
                  QObject *parent = nullptr);
    ~NetworkClient() override;

    [[nodiscard]] bool start(QString *error = nullptr);
    void stop();
    void refresh();

    // Intent-admitted mutations. Each returns false with a stable reason when
    // the model rejects the intent, another operation is in flight, or the
    // client is not Ready; a false return changes no state.
    [[nodiscard]] bool requestScan(qint64 deadlineMilliseconds,
                                   QString *error = nullptr);
    [[nodiscard]] bool connectKnownNetwork(const QString &knownNetworkId,
                                           QString *error = nullptr);
    [[nodiscard]] bool disconnectDevice(const QString &deviceInterface,
                                        QString *error = nullptr);
    [[nodiscard]] bool setRadio(RadioKind kind, bool enable,
                                QString *error = nullptr);

    [[nodiscard]] ClientState state() const noexcept { return m_state; }
    [[nodiscard]] const QString &lastError() const noexcept { return m_lastError; }
    [[nodiscard]] bool operationInFlight() const noexcept {
        return m_operation.has_value();
    }
    [[nodiscard]] const Model::NetworkModel &model() const noexcept {
        return m_model;
    }
    // Busy-aware consumer projection of the current model state.
    [[nodiscard]] Model::ModelState projection() const {
        return m_model.projection(operationInFlight());
    }

Q_SIGNALS:
    void stateChanged();
    void snapshotChanged();
    void operationInFlightChanged();
    void operationFinished(const QindaQt::Network::OperationResult &result);
    void operationUncertain(const QString &redactedMessage);

private:
    enum class RequestKind { Snapshot, Operation };
    struct Request final {
        quint64 token = 0;
        QString owner;
        RequestKind kind = RequestKind::Snapshot;
        quint64 epoch = 0;
        quint64 revision = 0;
        OperationKind operationKind = OperationKind::RequestScan;
    };
    struct Operation final {
        OperationKind kind = OperationKind::RequestScan;
        quint64 epoch = 0;
        quint64 revision = 0;
    };

    void handleOwnerChanged(const QString &owner);
    void handleInvalidation(const QString &owner);
    void handleSnapshot(quint64 token, const QString &owner,
                        const QByteArray &payload);
    void handleOperation(quint64 token, const QString &owner,
                         const QByteArray &payload);
    void handleFailure(quint64 token, const QString &owner,
                       const QString &errorName, const QString &message);
    void handleBusDisconnected();
    void handleRefreshTimer();
    void requestSnapshotNow();
    bool beginOperation(OperationKind kind, const QVariantMap &parameters,
                        QString *error);
    void finishOperationAsUncertain(const QString &message);
    void scheduleRetry();
    void publish(ClientState state, QString error = {});
    void setError(QString *output, QString message) const;
    void abortInFlight(const QString &reason);
    [[nodiscard]] quint64 nextToken();

    NetworkTransport &m_transport;
    Model::NetworkModel m_model;
    ClientTiming m_timing;
    QTimer m_refreshTimer;
    QTimer m_timeout;
    std::optional<Request> m_request;
    std::optional<Operation> m_operation;
    QString m_owner;
    QString m_lastError;
    ClientState m_state = ClientState::Unavailable;
    qsizetype m_retryIndex = 0;
    quint64 m_nextToken = 1;
    bool m_started = false;
    bool m_transportStarted = false;
    bool m_dirty = false;
};

} // namespace QindaQt::Network::Client
