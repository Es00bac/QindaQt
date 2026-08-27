// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/audio_client/audio_transport.h>

#include <QtCore/QObject>
#include <QtCore/QTimer>

#include <optional>

namespace QindaQt::Audio
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
// mutations, and never replays a timed-out or owner-interrupted mutation. The
// borrowed transport must share this object's thread and outlive it; all
// completion/error reporting is asynchronous through signals.
class AudioClient : public QObject
{
    Q_OBJECT

public:
    explicit AudioClient(AudioTransport *transport, QObject *parent = nullptr);

    void start();
    void stop();
    [[nodiscard]] ClientState state() const noexcept;
    [[nodiscard]] QString reasonCode() const;
    [[nodiscard]] QString owner() const;
    [[nodiscard]] bool hasSnapshot() const noexcept;
    [[nodiscard]] Snapshot snapshot() const;
    [[nodiscard]] bool operationPending() const noexcept;

    void setRequestTimeout(int milliseconds);
    [[nodiscard]] quint64 setDefault(const Handle &device);
    [[nodiscard]] quint64 setVolume(const Handle &target, double volume);
    [[nodiscard]] quint64 setMute(const Handle &target, bool muted);
    [[nodiscard]] quint64 moveStream(const Handle &stream, const Handle &device);

Q_SIGNALS:
    void stateChanged(QindaQt::Audio::ClientState state, const QString &reasonCode);
    void snapshotChanged(const QindaQt::Audio::Snapshot &snapshot);
    void operationCompleted(quint64 requestId,
                            const QindaQt::Audio::OperationResult &result);

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
    void requestSnapshot();
    void scheduleRefetch();
    [[nodiscard]] quint64 beginOperation(const OperationRequest &request);
    void completeUncertain(const QString &reasonCode);
    [[nodiscard]] OperationResult localResult(const OperationRequest &request,
                                              OperationStatus status,
                                              const QString &reasonCode) const;

    AudioTransport *m_transport = nullptr;
    ClientState m_state = ClientState::Stopped;
    QString m_reasonCode;
    QString m_owner;
    std::optional<Snapshot> m_snapshot;
    std::optional<PendingOperation> m_operation;
    QTimer m_fetchTimer;
    QTimer m_operationTimer;
    QTimer m_retryTimer;
    quint64 m_nextRequestId = 1;
    quint64 m_fetchRequestId = 0;
    bool m_fetchInFlight = false;
    bool m_refetchNeeded = false;
    int m_requestTimeoutMs = 5000;
};

} // namespace QindaQt::Audio

Q_DECLARE_METATYPE(QindaQt::Audio::ClientState)
