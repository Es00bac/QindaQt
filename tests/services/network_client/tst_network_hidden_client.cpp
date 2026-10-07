// SPDX-License-Identifier: GPL-3.0-or-later
#include "network_protocol_test_data.h"
#include <QtTest>
#include <qindaqt/services/network_client/network_client.h>
#include <qindaqt/services/network_client/network_transport.h>
#include <qindaqt/services/network_protocol/network_codec.h>
#include <qindaqt/services/network_protocol/network_redaction.h>
using namespace QindaQt::Network;
using namespace QindaQt::Network::Client;
using namespace QindaQt::Network::TestData;
class HiddenTransport final : public NetworkTransport {
  Q_OBJECT
public:
  bool start(QString *) override { return true; }
  void stop() override {}
  void requestSnapshot(quint64 requestToken,
                       const QString &requestOwner) override {
    Q_EMIT snapshotReceived(requestToken, requestOwner,
                            encodeSnapshot(validSnapshot()).payload);
  }
  void requestOperation(quint64 t, const QString &o, quint64 e, quint64 r,
                        OperationKind k, const QVariantMap &p) override {
    token = t;
    owner = o;
    epoch = e;
    revision = r;
    kind = k;
    parameters = p;
    ++calls;
  }
  quint64 token = 0, epoch = 0, revision = 0;
  QString owner;
  OperationKind kind{};
  QVariantMap parameters;
  int calls = 0;
};
class HiddenClientTests final : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void exactMetadataAndOldServiceRefusal();
  void singleflightAndOwnerLoss();
};
void HiddenClientTests::exactMetadataAndOldServiceRefusal() {
  HiddenTransport transport;
  NetworkClient client(transport);
  QSignalSpy completed(&client, &NetworkClient::operationFinished);
  QVERIFY(client.start());
  Q_EMIT transport.ownerChanged(validSnapshot().owner);
  QTRY_COMPARE(client.state(), ClientState::Ready);
  QVERIFY(client.connectHiddenNetwork({QStringLiteral("wlan0"),
                                       QStringLiteral("  <b>name</b>  "),
                                       SecuritySuite::Wpa3Personal}));
  QCOMPARE(transport.calls, 1);
  QCOMPARE(transport.kind, OperationKind::ConnectKnownNetwork);
  QCOMPARE(transport.parameters.keys(),
           QStringList({QStringLiteral("deviceInterface"),
                        QStringLiteral("security"), QStringLiteral("ssid")}));
  QCOMPARE(transport.parameters.value(QStringLiteral("ssid")).toString(),
           QStringLiteral("  <b>name</b>  "));
  QVERIFY(!wireContainsSecrets(transport.parameters));
  Q_EMIT transport.requestFailed(
      transport.token, transport.owner,
      QStringLiteral("hidden-network-control-unsupported"),
      QStringLiteral("private raw error"));
  QCOMPARE(completed.size(), 1);
  const auto result = completed.first().at(0).value<OperationResult>();
  QCOMPARE(result.kind, OperationKind::ConnectKnownNetwork);
  QCOMPARE(result.status, OperationStatus::Unsupported);
  QCOMPARE(result.initiatingEpoch, transport.epoch);
  QVERIFY(!result.diagnostic.contains(QStringLiteral("private")));
  QVERIFY(!client.operationInFlight());
  QCOMPARE(transport.calls, 1);
  Q_EMIT transport.requestFailed(
      transport.token, transport.owner,
      QStringLiteral("hidden-network-control-unsupported"), QString());
  QCOMPARE(completed.size(), 1);
}
void HiddenClientTests::singleflightAndOwnerLoss() {
  HiddenTransport transport;
  NetworkClient client(transport);
  QVERIFY(client.start());
  Q_EMIT transport.ownerChanged(validSnapshot().owner);
  QTRY_COMPARE(client.state(), ClientState::Ready);
  const ConnectHiddenIntent intent{QStringLiteral("wlan0"),
                                   QStringLiteral("hidden"),
                                   SecuritySuite::Wpa2Personal};
  QVERIFY(client.connectHiddenNetwork(intent));
  QVERIFY(!client.connectHiddenNetwork(intent));
  QCOMPARE(transport.calls, 1);
  QSignalSpy uncertain(&client, &NetworkClient::operationUncertain);
  Q_EMIT transport.ownerChanged(QString());
  QVERIFY(!client.operationInFlight());
  QCOMPARE(transport.calls, 1);
  QVERIFY(!client.connectHiddenNetwork(intent));
}
QTEST_GUILESS_MAIN(HiddenClientTests)
#include "tst_network_hidden_client.moc"
