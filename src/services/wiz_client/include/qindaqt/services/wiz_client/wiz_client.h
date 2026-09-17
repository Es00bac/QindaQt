// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/wiz_client/wiz_transport.h>
#include <qindaqt/services/wiz_model/wiz_model.h>
#include <qindaqt/services/wiz_protocol/wiz_types.h>

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QTimer>

namespace QindaQt::Wiz
{

enum class ClientState : quint32 {
    Stopped = 0,
    Starting = 1,
    Ready = 2,
    Unavailable = 3,
};

// Owns the conversation with every luminaire on the local network: discovery,
// capability interrogation, polling, and serialized control.
//
// AGENT-CONTRACT: this QObject is owned and used on one Qt thread. The
// borrowed transport and clock must share that thread and outlive it. Control
// intents are queued and executed one at a time so a burst of slider
// movements cannot flood the network or interleave on one device.
//
// AGENT-GUARD: a control operation that is not answered is completed as
// Uncertain and never replayed automatically. A luminaire may have applied a
// request whose acknowledgement was lost; silently repeating it would flicker
// the room and could re-apply a state the user has since changed.
class WizClient : public QObject
{
    Q_OBJECT

public:
    WizClient(WizTransport *transport, WizClock *clock, QObject *parent = nullptr);
    ~WizClient() override;

    void start();
    void stop();

    [[nodiscard]] ClientState state() const noexcept { return m_state; }
    [[nodiscard]] Snapshot snapshot() const { return m_model.snapshot(); }
    [[nodiscard]] bool operationPending() const noexcept;
    [[nodiscard]] int queuedOperationCount() const noexcept
    {
        return static_cast<int>(m_queue.size());
    }

    // Timing policy. Defaults suit a quiet home network: one poll round every
    // few seconds, a request deadline generous enough for a sleepy Wi-Fi radio.
    void setPollIntervalMilliseconds(int interval);
    void setRequestTimeoutMilliseconds(int timeout);
    // Drives polling, deadlines, and retries. Automatic mode runs it from a
    // timer; tests call it directly with a fake clock.
    void setAutomaticPolling(bool automatic);
    void tick();

    // Seeds an endpoint learned from stored configuration so a known light can
    // be reached at startup without waiting for a broadcast round.
    void seedDevice(const QString &mac, const QString &address);
    void applyStoredLabel(const QString &mac, const QString &label);
    void forgetDevice(const QString &mac);

    // Every control intent goes through here. The returned request id is
    // reported back by operationCompleted, including for intents rejected
    // before any datagram was sent.
    [[nodiscard]] quint64 dispatch(const OperationRequest &request);

Q_SIGNALS:
    void stateChanged(QindaQt::Wiz::ClientState state, const QString &reasonCode);
    void snapshotChanged();
    void operationCompleted(quint64 requestId,
                            const QindaQt::Wiz::OperationResult &result);

private:
    // One queued or in-flight control intent.
    struct PendingOperation {
        quint64 requestId = 0;
        OperationKind kind = OperationKind::Refresh;
        QString mac;
        QByteArray datagram;
        QString address;
        quint64 deadline = 0;
        int attempt = 0;
        quint64 initiatingEpoch = 0;
        quint64 initiatingRevision = 0;
    };

    void setState(ClientState state, const QString &reasonCode);
    void publishIfChanged(bool changed);
    void sendDiscovery();
    void pollDevice(const QString &mac);
    void interrogate(const QString &mac);
    void beginNextOperation();
    void completeCurrent(OperationStatus status, const QString &reasonCode,
                         const QString &diagnostic = QString());
    void deliver(const OperationResult &result, quint64 requestId);
    // Delivers a result that needs no datagram (a refusal, or an intent that
    // is already satisfied) on the next event-loop turn.
    void completeAsynchronously(quint64 requestId, const OperationRequest &request,
                                OperationStatus status, const QString &reasonCode);
    [[nodiscard]] bool transmit(const QString &mac, const QByteArray &datagram);

private Q_SLOTS:
    void acceptDatagram(const QString &address, quint16 port, const QByteArray &datagram);
    void acceptTransportFailure(const QString &reason);

private:
    WizTransport *m_transport = nullptr;
    WizClock *m_clock = nullptr;
    WizModel m_model;
    ClientState m_state = ClientState::Stopped;
    QTimer m_ticker;
    bool m_automaticPolling = true;
    int m_pollInterval = 4000;
    int m_requestTimeout = 1500;
    quint64 m_nextRequestId = 1;
    quint64 m_lastPollRound = 0;
    quint64 m_lastDiscovery = 0;
    QList<PendingOperation> m_queue;
    bool m_operationInFlight = false;
    // Devices with a getPilot poll outstanding, and when it was sent.
    QHash<QString, quint64> m_outstandingPolls;
    // Devices whose capability interrogation has been sent at least once, so a
    // silent model-configuration reply is not re-asked on every poll round.
    QHash<QString, int> m_interrogations;
};

} // namespace QindaQt::Wiz
