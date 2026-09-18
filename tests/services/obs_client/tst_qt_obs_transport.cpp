// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/obs_client/obs_client.h>
#include <qindaqt/services/obs_client/qt_obs_transport.h>

#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTest>
#include <QWebSocket>
#include <QWebSocketServer>

using namespace QindaQt::Obs;

namespace {

// A real obs-websocket server, in miniature: it speaks the v5 handshake on a
// private loopback port and answers exactly what a row scripts. This is the
// only row that opens a socket; everything else drives the client through
// the transport seam.
class FakeObsServer final : public QObject {
    Q_OBJECT
public:
    explicit FakeObsServer(QObject *parent = nullptr)
        : QObject(parent),
          m_server(QStringLiteral("fake-obs"), QWebSocketServer::NonSecureMode,
                   this) {
        connect(&m_server, &QWebSocketServer::newConnection, this, [this] {
            m_client = m_server.nextPendingConnection();
            connect(m_client, &QWebSocket::textMessageReceived, this,
                    [this](const QString &text) {
                        received.append(text);
                        Q_EMIT textArrived(text);
                    });
            send(QStringLiteral(
                R"({"op":0,"d":{"obsWebSocketVersion":"5.6.2","rpcVersion":1}})"));
        });
    }

    bool listen() {
        return m_server.listen(QHostAddress::LocalHost, 0);
    }
    [[nodiscard]] QString url() const {
        return QStringLiteral("ws://127.0.0.1:%1").arg(m_server.serverPort());
    }
    void send(const QString &text) {
        if (m_client != nullptr) {
            m_client->sendTextMessage(text);
        }
    }
    void hangUp() {
        if (m_client != nullptr) {
            m_client->close();
        }
    }

    QStringList received;

Q_SIGNALS:
    void textArrived(const QString &text);

private:
    QWebSocketServer m_server;
    QWebSocket *m_client = nullptr;
};

} // namespace

class QtObsTransportTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void onlyLoopbackAddressesAreAccepted();
    void aRealSocketCarriesTheV5HandshakeAndFrames();
    void aServerThatHangsUpIsReportedAsADisconnect();
};

void QtObsTransportTest::onlyLoopbackAddressesAreAccepted() {
    // AGENT-GUARD: obs-websocket has no transport security. A host key that
    // pointed off the machine would put the password and every scene name on
    // the network in the clear.
    QVERIFY(QtObsTransport::isLoopbackWebSocketUrl(
        QStringLiteral("ws://127.0.0.1:4455")));
    QVERIFY(QtObsTransport::isLoopbackWebSocketUrl(
        QStringLiteral("ws://localhost:4455")));
    QVERIFY(QtObsTransport::isLoopbackWebSocketUrl(QStringLiteral("ws://[::1]:4455")));
    QVERIFY(!QtObsTransport::isLoopbackWebSocketUrl(
        QStringLiteral("ws://192.168.1.10:4455")));
    QVERIFY(!QtObsTransport::isLoopbackWebSocketUrl(
        QStringLiteral("ws://obs.example.com:4455")));
    QVERIFY(!QtObsTransport::isLoopbackWebSocketUrl(
        QStringLiteral("wss://127.0.0.1:4455")));
    QVERIFY(!QtObsTransport::isLoopbackWebSocketUrl(QStringLiteral("ws://127.0.0.1")));
    QVERIFY(!QtObsTransport::isLoopbackWebSocketUrl(QString()));

    // And the transport refuses to open one, rather than leaving the refusal
    // to whichever caller remembered to check.
    QtObsTransport transport;
    QSignalSpy errors(&transport, &ObsTransport::errorOccurred);
    QSignalSpy closed(&transport, &ObsTransport::disconnected);
    transport.open(QStringLiteral("ws://192.168.1.10:4455"));
    QCOMPARE(errors.size(), 1);
    QCOMPARE(closed.size(), 1);
    QVERIFY(!transport.isOpen());
}

void QtObsTransportTest::aRealSocketCarriesTheV5HandshakeAndFrames() {
    FakeObsServer server;
    QVERIFY(server.listen());

    QtObsTransport transport;
    ObsClient client(transport, ClientTiming{});
    client.start(server.url(), QString());

    // The client identifies against a real socket, and the server sees it.
    QTRY_VERIFY_WITH_TIMEOUT(!server.received.isEmpty(), 5000);
    const QJsonObject identify =
        QJsonDocument::fromJson(server.received.first().toUtf8()).object();
    QCOMPARE(identify.value(QStringLiteral("op")).toInt(), 1);

    server.send(QStringLiteral(R"({"op":2,"d":{"negotiatedRpcVersion":1}})"));
    QTRY_COMPARE_WITH_TIMEOUT(client.state(), ConnectionState::Ready, 5000);

    // And the initial reads arrive over the wire, not merely into a list.
    QTRY_VERIFY_WITH_TIMEOUT(server.received.size() > 1, 5000);
    bool sawSceneList = false;
    for (const QString &frame : server.received) {
        const QJsonObject d = QJsonDocument::fromJson(frame.toUtf8())
                                  .object()
                                  .value(QStringLiteral("d"))
                                  .toObject();
        if (d.value(QStringLiteral("requestType")).toString() ==
            QLatin1String("GetSceneList")) {
            sawSceneList = true;
        }
    }
    QVERIFY(sawSceneList);

    client.stop();
}

void QtObsTransportTest::aServerThatHangsUpIsReportedAsADisconnect() {
    FakeObsServer server;
    QVERIFY(server.listen());

    QtObsTransport transport;
    QSignalSpy connectedSpy(&transport, &ObsTransport::connected);
    QSignalSpy disconnectedSpy(&transport, &ObsTransport::disconnected);
    transport.open(server.url());
    QTRY_COMPARE_WITH_TIMEOUT(connectedSpy.size(), 1, 5000);
    QVERIFY(transport.isOpen());

    server.hangUp();
    QTRY_COMPARE_WITH_TIMEOUT(disconnectedSpy.size(), 1, 5000);
    QVERIFY(!transport.isOpen());
}

QTEST_MAIN(QtObsTransportTest)
#include "tst_qt_obs_transport.moc"
