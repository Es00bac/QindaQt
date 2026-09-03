// SPDX-License-Identifier: GPL-3.0-or-later

#include "network_settings_test_support.h"

#include <qindaqt/apps/settings_network/network_settings_model.h>

#include <QtTest>

using namespace QindaQt::Apps::SettingsNetwork;
using namespace QindaQt::Apps::SettingsNetwork::TestSupport;
using namespace QindaQt::Network::Client;

class NetworkSettingsAgentGateTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void absentAgentProjectionCannotBeBypassed();
};

void NetworkSettingsAgentGateTest::absentAgentProjectionCannotBeBypassed() {
  qint64 now = 1'000;
  FakeNetworkTransport transport;
  NetworkClient client(transport, [&now] { return now; }, fastTiming());
  const QDBusConnection absentPresence(QStringLiteral("absent-agent-bus"));
  NetworkSettingsModel model(client, absentPresence);
  transport.setSnapshot(readySnapshot());
  QVERIFY(client.start());
  transport.announceOwner(QStringLiteral(":1.20"));
  QTRY_VERIFY_WITH_TIMEOUT(model.ready(), 1'000);

  const QVariantMap secured = model.accessPoints().at(3).toMap();
  QVERIFY(!model.secretAgentRegistered());
  QVERIFY(!secured.value(QStringLiteral("connectAvailable")).toBool());
  QCOMPARE(secured.value(QStringLiteral("connectBlockedReason")).toString(),
           QStringLiteral("secret-agent-unavailable"));

  QSignalSpy rejected(&model, &NetworkSettingsModel::actionRejected);
  const qsizetype callsBefore = transport.operations.size();
  QVERIFY(!model.connectVisibleNetwork(
      secured.value(QStringLiteral("id")).toString()));
  QCOMPARE(transport.operations.size(), callsBefore);
  QCOMPARE(rejected.size(), 1);
  QCOMPARE(rejected.constFirst().constFirst().toString(),
           QStringLiteral("secret-agent-unavailable"));
}

QTEST_GUILESS_MAIN(NetworkSettingsAgentGateTest)
#include "tst_network_settings_agent_gate.moc"
