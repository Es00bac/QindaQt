// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/link_protocol/envelope.h>

#include <QtCore/QObject>
#include <QtCore/QTimer>

namespace QindaQt::Link {

// Desktop side of one SAS pairing (protocol page, "Pairing"). Pure logic: it
// consumes pair.* frames and emits frames to send; the Session owns the
// socket. One instance per pair.request.
//
// AGENT-GUARD: the commitment is sent before the phone's nonce is known and
// the reveal only after. Reordering lets a phone grind a nonce to a chosen
// code; the unit tests pin the order.
class DesktopPairing final : public QObject {
    Q_OBJECT
public:
    DesktopPairing(QByteArray spkiD, QByteArray spkiP, int timeoutMs, QObject *parent = nullptr);

    [[nodiscard]] bool awaitingLocalDecision() const { return m_state == State::Deciding && !m_localAccepted; }
    [[nodiscard]] bool active() const { return m_state != State::Idle && m_state != State::Done; }
    // Step 2: sends pair.commit as the reply to `requestId`; starts the timeout.
    void begin(qint64 requestId);
    // Steps 3 and 7. Returns false for an out-of-order or malformed frame.
    [[nodiscard]] bool handle(const Envelope &frame);
    // The local user's decision (step 7).
    void decide(bool accept);

Q_SIGNALS:
    void frame(const QindaQt::Link::Envelope &envelope);
    void codeReady(const QString &code);
    void completed();
    // `byeReason` is a protocol bye reason; the Session sends it and closes.
    void aborted(const QString &byeReason);

private:
    enum class State { Idle, AwaitNonce, Deciding, Done };
    void finishAbort(const QString &reason);
    void maybeComplete();

    QByteArray m_spkiD;
    QByteArray m_spkiP;
    QByteArray m_nd;
    State m_state = State::Idle;
    QTimer m_timeout;
    bool m_localAccepted = false;
    bool m_peerAccepted = false;
};

} // namespace QindaQt::Link
