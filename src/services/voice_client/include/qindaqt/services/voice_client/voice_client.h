// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/voice_client/voice_transport.h>

#include <QtCore/QTimer>

#include <optional>

namespace QindaQt::Services::Voice {

// Exact-owner asynchronous org.qindaqt.Voice1 consumer.
//
// The provider is a separate, independently released process, so this client
// assumes nothing about its liveness: no snapshot means no mutation authority,
// one intent is in flight at a time, and an intent whose reply never arrives is
// reported Uncertain and never replayed. Dictation is not idempotent — a
// silently retried StartDictation would open a second microphone session — so
// "uncertain" is a terminal answer the caller must surface, not retry.
class VoiceClient final : public QObject {
    Q_OBJECT
public:
    explicit VoiceClient(VoiceTransport *transport, QObject *parent = nullptr);
    void start();
    void stop();
    void setRequestTimeout(int milliseconds);

    [[nodiscard]] ClientState state() const noexcept { return m_state; }
    [[nodiscard]] QString reasonCode() const { return m_reasonCode; }
    [[nodiscard]] QString owner() const { return m_owner; }
    [[nodiscard]] bool hasSnapshot() const noexcept { return m_snapshot.has_value(); }
    [[nodiscard]] Snapshot snapshot() const { return m_snapshot.value_or(Snapshot{}); }
    [[nodiscard]] quint32 levelPercent() const noexcept { return m_levelPercent; }
    [[nodiscard]] bool operationPending() const noexcept { return m_operation.has_value(); }

    // Each returns the request id the completion will carry, or 0 when the
    // client can no longer mint one. A returned id always completes exactly
    // once through operationCompleted.
    [[nodiscard]] quint64 startDictation();
    [[nodiscard]] quint64 startCommand();
    [[nodiscard]] quint64 finish();
    [[nodiscard]] quint64 cancel();
    [[nodiscard]] quint64 retry();
    [[nodiscard]] quint64 undo();
    [[nodiscard]] quint64 copyLast();
    [[nodiscard]] quint64 setProvider(const QString &providerId);
    [[nodiscard]] quint64 setEnabled(bool enabled);

Q_SIGNALS:
    void stateChanged(QindaQt::Services::Voice::ClientState state,
                      const QString &reasonCode);
    void snapshotChanged(const QindaQt::Services::Voice::Snapshot &snapshot);
    void levelChanged(quint32 levelPercent);
    void operationCompleted(quint64 requestId,
                            const QindaQt::Services::Voice::OperationResult &result);

private:
    struct Pending {
        quint64 token = 0;
        OperationRequest request;
    };
    void acceptOwner(const QString &owner);
    void acceptInvalidation(const QString &owner, quint64 revision);
    void acceptLevel(const QString &owner, quint32 levelPercent);
    void acceptSnapshot(const QString &owner, quint64 token, bool transportSuccess,
                        const Snapshot &snapshot, const QString &reasonCode);
    void acceptOperation(const QString &owner, quint64 token, bool transportSuccess,
                         const OperationResult &result, const QString &reasonCode);
    void fetch();
    void publishLevel(quint32 levelPercent);
    [[nodiscard]] quint64 begin(OperationKind kind, const QString &providerId,
                                bool enable);
    void complete(OperationResult result);
    void completeUncertain(const QString &reasonCode);
    [[nodiscard]] OperationResult localResult(OperationKind kind, quint64 requestId,
                                              OperationStatus status,
                                              const QString &reasonCode) const;
    void publishState(ClientState state, const QString &reasonCode);

    VoiceTransport *m_transport = nullptr;
    ClientState m_state = ClientState::Stopped;
    QString m_reasonCode;
    QString m_owner;
    std::optional<Snapshot> m_snapshot;
    std::optional<Pending> m_operation;
    QTimer m_fetchTimer;
    QTimer m_operationTimer;
    quint64 m_nextToken = 1;
    quint64 m_fetchToken = 0;
    quint32 m_levelPercent = 0;
    int m_timeoutMs = kDefaultRequestTimeoutMs;
    bool m_fetching = false;
    bool m_dirty = false;
    // Whether the transport has answered the owner question at all. Voice is
    // the first QindaQt service whose provider is legitimately not installed,
    // so "no owner" is an answer that must resolve Starting, not silence.
    bool m_ownerResolved = false;
};

} // namespace QindaQt::Services::Voice
