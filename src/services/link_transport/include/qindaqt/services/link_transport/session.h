// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/link_protocol/frame_codec.h>
#include <qindaqt/services/link_transport/feature.h>

#include <QtCore/QObject>
#include <QtCore/QTimer>
#include <QtNetwork/QSslSocket>

#include <functional>
#include <memory>

namespace QindaQt::Link {

class DesktopPairing;

enum class Origin { Loopback, Lan };
enum class SessionState { AwaitHello, Unpaired, Ready, Closed };

// AGENT-NOTE: timings are injectable so tests drive keepalive/pairing timeouts
// in milliseconds; production uses the defaults from the protocol page.
struct SessionTimings {
    int pingIdleMs = 30'000;
    int deadMs = 90'000;
    int pairTimeoutMs = 120'000;
    int requestTimeoutMs = 10'000;
};

struct PeerHello {
    QString role, name, model, os, app;
    QStringList caps;
    bool trusted = false;
};

// What a Session needs from the hub that owns it.
class SessionPolicy {
public:
    virtual ~SessionPolicy() = default;
    [[nodiscard]] virtual bool isPinned(const QString &deviceId) const = 0;
    [[nodiscard]] virtual bool pairingWindowOpen() const = 0;
    // name, model, os, app of this desktop.
    [[nodiscard]] virtual PeerHello localHello() const = 0;
    [[nodiscard]] virtual FeatureRouter *router() = 0;
};

// Log-safe one-line description of a frame.
//
// AGENT-GUARD: vault.* bodies are never logged, at any level; only the type
// and body size. A debug flag must not lift this.
[[nodiscard]] QString frameLogLine(const Envelope &envelope);

// One TLS connection to a phone, from handshake to close: hello exchange, the
// UNPAIRED gate, desktop-side pairing, keepalive, request/reply tracking and
// dispatch to FeatureHandlers.
//
// Ownership: the Session owns `socket` (it must already be encrypted and have
// a peer certificate). Threading: single thread. Error behavior: protocol
// errors close the connection; the Session never throws. After closed() is
// emitted the object is inert and may be deleted by its owner.
class Session final : public QObject {
    Q_OBJECT
public:
    Session(QSslSocket *socket, Origin origin, SessionTimings timings, SessionPolicy &policy,
            QObject *parent = nullptr);
    ~Session() override;

    [[nodiscard]] const QString &deviceId() const { return m_deviceId; }
    [[nodiscard]] Origin origin() const { return m_origin; }
    [[nodiscard]] SessionState state() const { return m_state; }
    [[nodiscard]] const PeerHello &peer() const { return m_peer; }
    [[nodiscard]] bool peerHasCap(const QString &cap) const { return m_peer.caps.contains(cap); }

    bool send(const Envelope &envelope);
    // Sends `envelope` with a fresh id; `reply` gets the ok/error answer, or a
    // synthetic error{timeout|internal} when none arrives or the session ends.
    void request(Envelope envelope, std::function<void(const Envelope &)> reply);
    // The local user's accept/reject of the pending pairing code.
    void confirmPairing(bool accept);
    // Sends bye{reason} (when non-empty) and closes after flushing.
    void close(const QString &byeReason);
    [[nodiscard]] bool pairingPending() const;

Q_SIGNALS:
    void helloReceived();
    void pairingCodeReady(const QString &code);
    // Emitted before readyEntered(); the hub pins the key here.
    void pairingCompleted();
    void readyEntered();
    void closed();

private:
    void onReadyRead();
    void dispatch(const Envelope &e);
    void handleHello(const Envelope &e);
    void handleUnpairedFrame(const Envelope &e);
    void handlePairRequest(const Envelope &e);
    void enterReady();
    void failPending();
    void restartIdleTimer();

    QSslSocket *m_socket;
    Origin m_origin;
    SessionTimings m_timings;
    SessionPolicy &m_policy;
    QString m_deviceId;
    SessionState m_state = SessionState::AwaitHello;
    PeerHello m_peer;
    FrameDecoder m_decoder;
    QTimer m_idleTimer;
    QTimer m_deadTimer;
    qint64 m_nextId = 1;
    QHash<qint64, std::function<void(const Envelope &)>> m_pending;
    DesktopPairing *m_pairing = nullptr;
};

} // namespace QindaQt::Link
