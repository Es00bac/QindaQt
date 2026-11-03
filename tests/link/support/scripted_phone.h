// SPDX-License-Identifier: GPL-3.0-or-later
// Scripted C++ phone peer for QindaLink tests: a throwaway identity, a real
// TLS 1.3 client socket and explicit frame-level control. Never connect it to
// port 47160 or to a real service; tests pass an ephemeral port.
#pragma once

#include <qindaqt/services/link_protocol/frame_codec.h>
#include <qindaqt/services/link_protocol/pairing_math.h>
#include <qindaqt/services/link_transport/identity.h>

#include <QtCore/QDeadlineTimer>
#include <QtCore/QJsonArray>
#include <QtCore/QObject>
#include <QtCore/QRandomGenerator>
#include <QtNetwork/QSslConfiguration>
#include <QtNetwork/QSslSocket>
#include <QtTest/QTest>

#include <deque>
#include <functional>

namespace QindaQt::Link::Test {

inline bool waitUntil(const std::function<bool()> &pred, int timeoutMs = 4000)
{
    QDeadlineTimer deadline(timeoutMs);
    while (!pred()) {
        if (deadline.hasExpired()) {
            return false;
        }
        QTest::qWait(5);
    }
    return true;
}

class ScriptedPhone : public QObject {
public:
    explicit ScriptedPhone(DeviceIdentity identity = generateIdentity(QStringLiteral("scripted-phone")))
        : m_identity(std::move(identity))
    {
    }

    [[nodiscard]] const DeviceIdentity &identity() const { return m_identity; }
    [[nodiscard]] QByteArray spkiP() const { return m_identity.spkiDigest(); }
    [[nodiscard]] QString deviceId() const { return QString::fromLatin1(spkiP().toHex()); }
    [[nodiscard]] QByteArray spkiD() const { return spkiDigest(m_socket.peerCertificate()); }  // valid while connected
    [[nodiscard]] bool autoPong() const { return m_autoPong; }
    void setAutoPong(bool on) { m_autoPong = on; }

    bool connectTo(quint16 port, const QString &host = QStringLiteral("127.0.0.1"))
    {
        Q_ASSERT(port != 47160);
        QSslConfiguration config = QSslConfiguration::defaultConfiguration();
        config.setProtocol(QSsl::TlsV1_3);
        config.setPeerVerifyMode(QSslSocket::VerifyNone);
        config.setLocalCertificate(m_identity.certificate);
        config.setPrivateKey(m_identity.key);
        m_socket.setSslConfiguration(config);
        connect(&m_socket, &QSslSocket::readyRead, this, [this] { pump(); });
        // Data that arrived together with the peer's close must still be read.
        connect(&m_socket, &QSslSocket::disconnected, this, [this] { pump(); });
        m_socket.connectToHostEncrypted(host, port);
        connect(&m_socket, &QSslSocket::encrypted, this, [this] { m_everEncrypted = true; });
        return waitUntil([this] { return m_everEncrypted; });
    }

    [[nodiscard]] bool isConnected() const { return m_socket.state() == QAbstractSocket::ConnectedState; }
    bool waitClosed(int ms = 4000)
    {
        return waitUntil([this] { return m_socket.state() == QAbstractSocket::UnconnectedState; }, ms);
    }

    void send(const Envelope &e) { m_socket.write(encodeFrame(e)); }
    void sendRaw(const QByteArray &bytes) { m_socket.write(bytes); }
    void sendHello(bool trusted, const QStringList &caps = {})
    {
        Envelope e;
        e.t = QStringLiteral("hello");
        e.b = QJsonObject{{QStringLiteral("proto"), 1}, {QStringLiteral("role"), QStringLiteral("phone")},
                          {QStringLiteral("name"), QStringLiteral("Test Phone")},
                          {QStringLiteral("model"), QStringLiteral("TP-1")},
                          {QStringLiteral("os"), QStringLiteral("Android 17")},
                          {QStringLiteral("app"), QStringLiteral("0.0.1")},
                          {QStringLiteral("caps"), QJsonArray::fromStringList(caps)},
                          {QStringLiteral("trusted"), trusted}};
        send(e);
    }

    // Waits for the next received frame of type `t`, discarding others.
    bool waitFor(const QString &t, Envelope *out = nullptr, int ms = 4000)
    {
        return waitUntil([&] {
            while (!m_received.empty()) {
                Envelope e = m_received.front();
                m_received.pop_front();
                if (e.t == t) {
                    if (out != nullptr) *out = e;
                    return true;
                }
            }
            return false;
        }, ms);
    }
    [[nodiscard]] int count(const QString &t) const
    {
        int n = 0;
        for (const Envelope &e : m_all) n += e.t == t ? 1 : 0;
        return n;
    }
    [[nodiscard]] QList<Envelope> all() const { return QList<Envelope>(m_all.begin(), m_all.end()); }

    // Drives steps 1..4 of pairing and returns the phone-side code.
    bool pairUntilCode(QString *code, QByteArray *nd = nullptr, bool tamperCommitCheck = false)
    {
        Envelope request;
        request.t = QStringLiteral("pair.request");
        request.id = 77;
        send(request);
        Envelope commit, reveal;
        if (!waitFor(QStringLiteral("pair.commit"), &commit) || commit.re != 77) return false;
        QByteArray np(32, Qt::Uninitialized);
        QRandomGenerator::system()->fillRange(reinterpret_cast<quint32 *>(np.data()), 8);
        Envelope nonce;
        nonce.t = QStringLiteral("pair.nonce");
        nonce.b = QJsonObject{{QStringLiteral("np"), QString::fromLatin1(np.toHex())}};
        send(nonce);
        if (!waitFor(QStringLiteral("pair.reveal"), &reveal)) return false;
        QByteArray revealed = QByteArray::fromHex(reveal.b.value(QStringLiteral("nd")).toString().toLatin1());
        if (tamperCommitCheck) revealed[0] = char(revealed[0] ^ 0x01);
        const QByteArray expected = QByteArray::fromHex(commit.b.value(QStringLiteral("c")).toString().toLatin1());
        if (pairingCommitment(revealed, spkiD(), spkiP()) != expected) {
            send(makeBye(QStringLiteral("pair-rejected")));  // step 5 of the protocol
            return false;
        }
        if (nd != nullptr) *nd = revealed;
        *code = pairingCode(spkiD(), spkiP(), revealed, np);
        return true;
    }

private:
    void pump()
    {
        m_decoder.append(m_socket.readAll());
        for (;;) {
            const DecodeResult r = m_decoder.next();
            if (r.status != DecodeStatus::Frame) return;
            m_all.push_back(r.envelope);
            if (m_autoPong && r.envelope.t == QLatin1String("ping")) {
                Envelope pong;
                pong.t = QStringLiteral("pong");
                pong.re = r.envelope.id;
                send(pong);
            }
            m_received.push_back(r.envelope);
        }
    }

    DeviceIdentity m_identity;
    QSslSocket m_socket;
    FrameDecoder m_decoder;
    std::deque<Envelope> m_received;
    std::deque<Envelope> m_all;
    bool m_autoPong = true;
    bool m_everEncrypted = false;
};

} // namespace QindaQt::Link::Test
