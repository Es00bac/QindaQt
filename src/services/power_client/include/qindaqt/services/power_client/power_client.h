// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/power_client/power_transport.h>

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QTimer>

#include <optional>

namespace QindaQt::Power {

enum class PowerClientState {
    Stopped,
    Starting,
    Ready,
    Unavailable,
    Degraded,
};

// AGENT-CONTRACT: This QObject is owned and used on one Qt thread. It binds to
// an exact D-Bus owner, publishes only validateSnapshot-accepted snapshots,
// serializes mutations, and never replays a timed-out or owner-interrupted
// mutation: those complete exactly once as Uncertain and the caller must
// resnapshot. The borrowed transport must share this object's thread and
// outlive it; all completion/error reporting is asynchronous through signals.
// stop() cancels undelivered results, then schedules one client-stopped
// Uncertain result for a mutation that was still transport-backed;
// destruction safely drops queued delivery.
class PowerClient : public QObject
{
    Q_OBJECT

public:
    explicit PowerClient(PowerTransport *transport, QObject *parent = nullptr);

    void start();
    void stop();
    [[nodiscard]] PowerClientState state() const noexcept;
    [[nodiscard]] QString reasonCode() const;
    [[nodiscard]] QString owner() const;
    [[nodiscard]] bool hasSnapshot() const noexcept;
    [[nodiscard]] Snapshot snapshot() const;
    [[nodiscard]] bool operationPending() const noexcept;

    void setRequestTimeout(int milliseconds);
    [[nodiscard]] quint64 setProfile(const QString &profileId);
    [[nodiscard]] quint64 acquireProfileHold(const QString &profileId,
                                             const QString &applicationName,
                                             const QString &reason);
    [[nodiscard]] quint64 releaseProfileHold(const Handle &hold);
    [[nodiscard]] quint64 setKeyboardBrightness(const Handle &device, quint32 value);

Q_SIGNALS:
    void stateChanged(QindaQt::Power::PowerClientState state,
                      const QString &reasonCode);
    void snapshotChanged(const QindaQt::Power::Snapshot &snapshot);
    void operationCompleted(quint64 requestId,
                            const QindaQt::Power::OperationResult &result);

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
        PowerClientRequest request;
        quint64 epoch = 0;
        quint64 revision = 0;
    };

    void publishState(PowerClientState state, const QString &reasonCode);
    void publishSnapshotState(const Snapshot &snapshot);
    void requestSnapshot();
    void scheduleRefetch();
    [[nodiscard]] quint64 beginOperation(const PowerClientRequest &request);
    void queueOperationCompletion(quint64 requestId, OperationResult result);
    void cancelQueuedOperationCompletions();
    void completeUncertain(const QString &reasonCode);
    [[nodiscard]] OperationResult localResult(const PowerClientRequest &request,
                                              OperationStatus status,
                                              const QString &reasonCode) const;

    PowerTransport *m_transport = nullptr;
    PowerClientState m_state = PowerClientState::Stopped;
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
};

} // namespace QindaQt::Power

Q_DECLARE_METATYPE(QindaQt::Power::PowerClientState)
