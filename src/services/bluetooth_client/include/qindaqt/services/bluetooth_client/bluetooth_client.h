// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/bluetooth_client/bluetooth_transport.h>

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QTimer>

#include <optional>

namespace QindaQt::Bluetooth
{

enum class ClientState {
    Stopped,
    Starting,
    Ready,
    Unavailable,
    Degraded,
};

// AGENT-CONTRACT: This QObject is owned and used on one Qt thread. It binds to
// an exact D-Bus owner, publishes only validated snapshots, serializes
// mutations, and never replays a timed-out or owner-interrupted mutation. A
// fetch failure or timeout revokes mutation authority: the retained snapshot
// is dropped and any dispatched operation completes as Uncertain rather than
// remaining authorized by stale state. The borrowed transport must share this
// object's thread and outlive it; all completion/error reporting is
// asynchronous through signals. stop() cancels undelivered results, then
// schedules one client-stopped result for a mutation that was still
// transport-backed; destruction safely drops queued delivery.
class BluetoothClient : public QObject
{
    Q_OBJECT

public:
    explicit BluetoothClient(BluetoothTransport *transport, QObject *parent = nullptr);

    void start();
    void stop();
    [[nodiscard]] ClientState state() const noexcept;
    [[nodiscard]] QString reasonCode() const;
    [[nodiscard]] QString owner() const;
    [[nodiscard]] bool hasSnapshot() const noexcept;
    [[nodiscard]] Snapshot snapshot() const;
    [[nodiscard]] bool operationPending() const noexcept;

    void setRequestTimeout(int milliseconds);
    [[nodiscard]] quint64 setAdapterPower(const Handle &adapter, bool powered);
    [[nodiscard]] quint64 acquireDiscovery(const Handle &adapter);
    [[nodiscard]] quint64 releaseDiscovery(const Handle &adapter);
    [[nodiscard]] quint64 connectDevice(const Handle &device);
    [[nodiscard]] quint64 disconnectDevice(const Handle &device);

Q_SIGNALS:
    void stateChanged(QindaQt::Bluetooth::ClientState state, const QString &reasonCode);
    void snapshotChanged(const QindaQt::Bluetooth::Snapshot &snapshot);
    void operationCompleted(quint64 requestId,
                            const QindaQt::Bluetooth::OperationResult &result);

private Q_SLOTS:
    void acceptOwner(const QString &owner);
    void acceptInvalidation(const QString &owner, quint64 epoch, quint64 revision);
    void acceptSnapshotReply(const QString &owner, quint64 requestId,
                             bool transportSuccess, const Snapshot &snapshot,
                             const QString &reasonCode);
    void acceptOperationReply(const QString &owner, quint64 requestId,
                              bool transportSuccess, const OperationResult &result,
                              const QString &reasonCode);
    void onFetchTimeout();
    void onOperationTimeout();

private:
    struct PendingOperation {
        quint64 requestId = 0;
        OperationRequest request;
        quint64 epoch = 0;
        quint64 revision = 0;
    };

    void publishState(ClientState state, const QString &reasonCode);
    void publishSnapshotState(const Snapshot &snapshot);
    void requestSnapshot();
    void scheduleRefetch();
    [[nodiscard]] quint64 beginOperation(const OperationRequest &request);
    void queueOperationCompletion(quint64 requestId, OperationResult result);
    void cancelQueuedOperationCompletions();
    void completeUncertain(const QString &reasonCode);
    [[nodiscard]] OperationResult localResult(const OperationRequest &request,
                                              OperationStatus status,
                                              const QString &reasonCode) const;

    BluetoothTransport *m_transport = nullptr;
    ClientState m_state = ClientState::Stopped;
    QString m_reasonCode;
    QString m_owner;
    std::optional<Snapshot> m_snapshot;
    std::optional<PendingOperation> m_operation;
    QHash<quint64, OperationResult> m_queuedOperationCompletions;
    QTimer m_fetchTimer;
    QTimer m_operationTimer;
    QTimer m_retryTimer;
    quint64 m_nextRequestId = 1;
    quint64 m_fetchRequestId = 0;
    bool m_fetchInFlight = false;
    bool m_refetchNeeded = false;
    int m_requestTimeoutMs = 5000;
    // Bounded retry backoff: the refetch interval doubles from 200 ms to a
    // 2 s cap and resets to the minimum whenever a snapshot is accepted.
    int m_retryIntervalMs = 200;
};

} // namespace QindaQt::Bluetooth

Q_DECLARE_METATYPE(QindaQt::Bluetooth::ClientState)
