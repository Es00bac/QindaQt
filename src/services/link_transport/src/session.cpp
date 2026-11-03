// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/link_transport/session.h>

#include <qindaqt/services/link_transport/desktop_pairing.h>
#include <qindaqt/services/link_transport/identity.h>

#include <QtCore/QJsonArray>
#include <QtCore/QLoggingCategory>

namespace QindaQt::Link {
namespace {

Q_LOGGING_CATEGORY(lcFrames, "qindaqt.link.frames", QtWarningMsg)

QString str(const QJsonObject &o, const char *key, bool *ok, int maxLen)
{
    const QJsonValue v = o.value(QLatin1String(key));
    if (!v.isString() || v.toString().size() > maxLen) {
        *ok = false;
        return {};
    }
    return v.toString();
}

bool allowedWhileUnpaired(const QString &t)
{
    return t.startsWith(QLatin1String("pair.")) || t == QLatin1String("ping") || t == QLatin1String("pong")
        || t == QLatin1String("error") || t == QLatin1String("bye") || t == QLatin1String("hello");
}

} // namespace

QString frameLogLine(const Envelope &e)
{
    if (e.t.startsWith(QLatin1String("vault."))) {
        return QStringLiteral("%1 (body redacted, %2 keys)").arg(e.t).arg(e.b.size());
    }
    return QStringLiteral("%1 id=%2 re=%3 keys=%4").arg(e.t).arg(e.id).arg(e.re).arg(e.b.keys().join(QLatin1Char(',')));
}

Session::Session(QSslSocket *socket, Origin origin, SessionTimings timings, SessionPolicy &policy,
                 QObject *parent)
    : QObject(parent), m_socket(socket), m_origin(origin), m_timings(timings), m_policy(policy)
{
    m_socket->setParent(this);
    m_deviceId = deviceIdFor(m_socket->peerCertificate());
    m_idleTimer.setSingleShot(true);
    m_idleTimer.setInterval(m_timings.pingIdleMs);
    m_deadTimer.setSingleShot(true);
    m_deadTimer.setInterval(m_timings.deadMs);
    connect(&m_idleTimer, &QTimer::timeout, this, [this] {
        Envelope ping;
        ping.t = QStringLiteral("ping");
        ping.id = m_nextId++;
        send(ping);
    });
    connect(&m_deadTimer, &QTimer::timeout, this, [this] { close(QString()); });
    // Bounds on unauthenticated connections (protocol page, "Connection lifecycle").
    m_helloTimer.setSingleShot(true);
    m_helloTimer.setInterval(m_timings.helloDeadlineMs);
    m_lifetimeTimer.setSingleShot(true);
    m_lifetimeTimer.setInterval(m_timings.unpairedLifetimeMs);
    connect(&m_helloTimer, &QTimer::timeout, this, [this] {
        if (m_state == SessionState::AwaitHello) close(QString());
    });
    connect(&m_lifetimeTimer, &QTimer::timeout, this, [this] {
        if (m_state != SessionState::Ready) close(QStringLiteral("pair-timeout"));
    });
    connect(m_socket, &QSslSocket::readyRead, this, &Session::onReadyRead);
    connect(m_socket, &QSslSocket::disconnected, this, [this] { close(QString()); });
    if (m_deviceId.isEmpty()) {
        QTimer::singleShot(0, this, [this] { close(QString()); });
        return;
    }
    m_deadTimer.start();
    m_helloTimer.start();
    m_lifetimeTimer.start();
    const PeerHello local = m_policy.localHello();
    Envelope hello;
    hello.t = QStringLiteral("hello");
    hello.b = QJsonObject{{QStringLiteral("proto"), 1},
                          {QStringLiteral("role"), QStringLiteral("desktop")},
                          {QStringLiteral("name"), local.name.left(128)},
                          {QStringLiteral("model"), local.model.left(128)},
                          {QStringLiteral("os"), local.os.left(128)},
                          {QStringLiteral("app"), local.app.left(64)},
                          {QStringLiteral("caps"), QJsonArray::fromStringList(m_policy.router()->caps())},
                          {QStringLiteral("trusted"), m_policy.isPinned(m_deviceId)}};
    send(hello);
    if (m_socket->bytesAvailable() > 0) {
        QTimer::singleShot(0, this, &Session::onReadyRead);
    }
}

Session::~Session() = default;

QHostAddress Session::sourceAddress() const
{
    QHostAddress a = m_socket->peerAddress();
    bool mapped = false;
    const quint32 v4 = a.toIPv4Address(&mapped);
    return mapped ? QHostAddress(v4) : a;
}

bool Session::pairingPending() const { return m_pairing != nullptr && m_pairing->active(); }
bool Session::awaitingLocalDecision() const
{
    return m_state != SessionState::Closed && m_pairing != nullptr && m_pairing->awaitingLocalDecision() && !m_code.isEmpty();
}

bool Session::send(const Envelope &envelope)
{
    if (m_state == SessionState::Closed) {
        return false;
    }
    const QByteArray bytes = encodeFrame(envelope);
    if (bytes.isEmpty()) {
        return false;
    }
    qCDebug(lcFrames) << "send" << frameLogLine(envelope);
    m_socket->write(bytes);
    restartIdleTimer();
    return true;
}

void Session::restartIdleTimer() { m_idleTimer.start(); }

void Session::request(Envelope envelope, std::function<void(const Envelope &)> reply)
{
    envelope.id = m_nextId++;
    const qint64 id = envelope.id;
    if (!send(envelope)) {
        reply(makeError(QStringLiteral("internal"), QStringLiteral("not connected")));
        return;
    }
    m_pending.insert(id, std::move(reply));
    QTimer::singleShot(m_timings.requestTimeoutMs, this, [this, id] {
        if (auto cb = m_pending.take(id)) {
            cb(makeError(QStringLiteral("timeout"), QStringLiteral("no reply"), id));
        }
    });
}

void Session::failPending()
{
    auto pending = std::move(m_pending);
    m_pending.clear();
    for (auto &cb : pending) {
        cb(makeError(QStringLiteral("internal"), QStringLiteral("connection closed")));
    }
}

void Session::close(const QString &byeReason)
{
    if (m_state == SessionState::Closed) {
        return;
    }
    if (!byeReason.isEmpty()) {
        send(makeBye(byeReason));
    }
    m_state = SessionState::Closed;
    m_idleTimer.stop();
    m_deadTimer.stop();
    m_helloTimer.stop();
    m_lifetimeTimer.stop();
    if (m_ownsPairingSlot) {
        m_ownsPairingSlot = false;
        m_policy.endPairing(this);
    }
    if (m_pairing != nullptr) {
        m_pairing->disconnect(this);
    }
    m_policy.router()->notifyClosed(m_deviceId);
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->disconnect(this);
        // AGENT-GUARD: a bye written from inside a readyRead slot is lost when
        // disconnectFromHost() runs before the TLS layer has flushed it. Wait
        // for the write buffer to drain (bounded) before closing.
        QSslSocket *socket = m_socket;
        socket->setParent(nullptr);  // outlives the Session; frees itself once closed
        connect(socket, &QSslSocket::disconnected, socket, &QObject::deleteLater);
        auto finish = [socket] {
            if (socket->state() != QAbstractSocket::UnconnectedState) {
                socket->disconnectFromHost();
            }
        };
        socket->flush();
        if (socket->bytesToWrite() == 0) {
            QTimer::singleShot(0, socket, finish);
        } else {
            connect(socket, &QSslSocket::bytesWritten, socket, [socket, finish] {
                if (socket->bytesToWrite() == 0) {
                    finish();
                }
            });
            QTimer::singleShot(1000, socket, finish);
        }
    }
    failPending();
    Q_EMIT closed();
}

void Session::onReadyRead()
{
    if (m_state == SessionState::Closed) {
        return;
    }
    m_deadTimer.start();
    m_decoder.append(m_socket->readAll());
    while (m_state != SessionState::Closed) {
        const DecodeResult r = m_decoder.next();
        if (r.status == DecodeStatus::NeedsMore) {
            return;
        }
        if (r.status != DecodeStatus::Frame) {
            close(QString());  // protocol error: close without replying
            return;
        }
        qCDebug(lcFrames) << "recv" << frameLogLine(r.envelope);
        dispatch(r.envelope);
    }
}

void Session::dispatch(const Envelope &e)
{
    if (m_state == SessionState::AwaitHello) {
        if (e.t == QLatin1String("hello")) {
            handleHello(e);
        } else if (e.t == QLatin1String("bye")) {
            close(QString());
        } else {
            close(QString());  // nothing may precede hello
        }
        return;
    }
    if (e.t == QLatin1String("hello")) {
        return;  // a repeated hello carries nothing new
    }
    if (e.t == QLatin1String("bye")) {
        close(QString());
        return;
    }
    if (e.t == QLatin1String("ping")) {
        Envelope pong;
        pong.t = QStringLiteral("pong");
        pong.re = e.id;
        send(pong);
        return;
    }
    if (e.t == QLatin1String("pong")) {
        return;
    }
    // AGENT-CONTRACT: a reply is `ok`, `error`, or a typed frame whose `re` names a request this side
    // made (sms.* replies carry data, protocol page "Messages"). Typed frames with an unknown `re`
    // fall through to their feature handler.
    if (e.re != 0 && (e.t == QLatin1String("ok") || e.t == QLatin1String("error") || m_pending.contains(e.re))) {
        if (auto cb = m_pending.take(e.re)) {
            cb(e);
        }
        return;
    }
    if (m_state == SessionState::Unpaired) {
        if (!allowedWhileUnpaired(e.t)) {
            close(QString());
            return;
        }
        handleUnpairedFrame(e);
        return;
    }
    if (FeatureHandler *h = m_policy.router()->find(e.t)) {
        h->handle(*this, e);
    } else if (e.id != 0) {
        send(makeError(QStringLiteral("unknown-type"), QStringLiteral("no handler"), e.id));
    }
}

void Session::handleHello(const Envelope &e)
{
    bool ok = true;
    PeerHello h;
    h.role = str(e.b, "role", &ok, 16);
    h.name = str(e.b, "name", &ok, 128);
    h.model = str(e.b, "model", &ok, 128);
    h.os = str(e.b, "os", &ok, 128);
    h.app = str(e.b, "app", &ok, 64);
    const QJsonValue proto = e.b.value(QStringLiteral("proto"));
    const QJsonValue trusted = e.b.value(QStringLiteral("trusted"));
    const QJsonValue caps = e.b.value(QStringLiteral("caps"));
    if (!ok || !proto.isDouble() || !trusted.isBool() || !caps.isArray() || h.role != QLatin1String("phone")) {
        close(QStringLiteral("error"));
        return;
    }
    if (proto.toInt(-1) != 1) {
        close(QStringLiteral("protocol-version"));
        return;
    }
    h.trusted = trusted.toBool();
    for (const QJsonValue &c : caps.toArray()) {
        if (c.isString()) {
            h.caps.append(c.toString());
        }
    }
    m_peer = h;
    m_state = SessionState::Unpaired;
    m_helloTimer.stop();
    Q_EMIT helloReceived();
    // READY needs a pin on both sides. A mismatch (stale pin) stays UNPAIRED
    // and keeps whatever pin we hold; the phone UI offers "pair again".
    if (h.trusted && m_policy.isPinned(m_deviceId)) {
        enterReady();
    }
}

void Session::enterReady()
{
    m_state = SessionState::Ready;
    m_lifetimeTimer.stop();
    Q_EMIT readyEntered();
}

void Session::handleUnpairedFrame(const Envelope &e)
{
    if (e.t == QLatin1String("pair.request")) {
        handlePairRequest(e);
        return;
    }
    if (e.t.startsWith(QLatin1String("pair."))) {
        if (m_pairing == nullptr || !m_pairing->handle(e)) {
            send(makeError(QStringLiteral("bad-request"), QStringLiteral("unexpected pairing frame"), e.id));
        }
    }
}

void Session::handlePairRequest(const Envelope &e)
{
    if (e.id == 0) {
        send(makeError(QStringLiteral("bad-request"), QStringLiteral("pair.request needs an id")));
        return;
    }
    if (m_origin == Origin::Lan && !m_policy.pairingWindowOpen()) {
        send(makeError(QStringLiteral("denied"), QStringLiteral("pairing window closed"), e.id));
        return;
    }
    if (pairingPending()) {
        send(makeError(QStringLiteral("busy"), QStringLiteral("pairing in progress"), e.id));
        return;
    }
    if (!m_policy.beginPairing(this)) {
        send(makeError(QStringLiteral("busy"), QStringLiteral("another pairing is in progress"), e.id));
        return;
    }
    m_ownsPairingSlot = true;
    delete m_pairing;
    m_pairing = new DesktopPairing(m_socket->localCertificate().isNull() ? QByteArray()
                                                                        : spkiDigest(m_socket->localCertificate()),
                                   spkiDigest(m_socket->peerCertificate()), m_timings.pairTimeoutMs, this);
    connect(m_pairing, &DesktopPairing::frame, this, [this](const Envelope &f) { send(f); });
    connect(m_pairing, &DesktopPairing::codeReady, this, [this](const QString &code) {
        m_code = code;
        Q_EMIT pairingCodeReady(code);
    });
    connect(m_pairing, &DesktopPairing::aborted, this, [this](const QString &reason) { close(reason); });
    connect(m_pairing, &DesktopPairing::completed, this, [this] {
        m_ownsPairingSlot = false;
        m_policy.endPairing(this);
        // AGENT-GUARD: never enter READY unless the pin is durable, or the
        // two trust stores disagree after a restart.
        if (!m_policy.persistPairing(*this)) {
            close(QStringLiteral("error"));
            return;
        }
        Q_EMIT pairingCompleted();
        enterReady();
    });
    m_pairing->begin(e.id);
}

void Session::confirmPairing(bool accept)
{
    if (m_pairing != nullptr) {
        m_pairing->decide(accept);
    }
}

} // namespace QindaQt::Link
