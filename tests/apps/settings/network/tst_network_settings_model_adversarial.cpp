// SPDX-License-Identifier: GPL-3.0-or-later

#include "network_settings_test_support.h"

#include <qindaqt/apps/settings_network/network_settings_model.h>

#include <QtTest>

using namespace QindaQt::Apps::SettingsNetwork;
using namespace QindaQt::Apps::SettingsNetwork::TestSupport;
using namespace QindaQt::Network;
using namespace QindaQt::Network::Client;

namespace {

struct Fixture final {
  qint64 now = 2'000;
  FakeNetworkTransport transport;
  NetworkClient client;
  NetworkSettingsModel model;

  Fixture()
      : client(transport, [this] { return now; }, fastTiming()), model(client, QDBusConnection(QStringLiteral("no-host-presence"))) {
    transport.setSnapshot(readySnapshot());
    const bool started = client.start();
    Q_ASSERT(started);
    transport.announceOwner(QStringLiteral(":1.20"));
  }
};

} // namespace

class NetworkSettingsModelAdversarialTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void rejectsRetiredAbaLineageAndClearsTruth();
  void ignoresExactSameOwnerDuplicateWithoutMutatingTruth();
  void treatsMismatchedOperationLineageAsUncertainWithoutReplay();
  void redactsTransportDiagnosticsAndPublishesNoSecretRoles();
  void exposesNoRadioOrCredentialMutationSurface();
};

void NetworkSettingsModelAdversarialTest::rejectsRetiredAbaLineageAndClearsTruth() {
  Fixture fixture;
  QTRY_VERIFY_WITH_TIMEOUT(fixture.model.ready(), 1'000);

  fixture.transport.setSnapshot(
      readySnapshot(QStringLiteral(":1.21"), 21, 1));
  fixture.transport.announceOwner(QStringLiteral(":1.21"));
  QTRY_COMPARE_WITH_TIMEOUT(fixture.model.serviceOwner(),
                            QStringLiteral(":1.21"), 1'000);

  fixture.transport.setSnapshot(
      readySnapshot(QStringLiteral(":1.20"), 20, 99));
  fixture.transport.announceOwner(QStringLiteral(":1.20"));
  QTRY_VERIFY_WITH_TIMEOUT(fixture.model.degraded(), 1'000);
  QVERIFY(!fixture.model.stale());
  QVERIFY(fixture.model.serviceOwner().isEmpty());
  QVERIFY(fixture.model.devices().isEmpty());
  QVERIFY(fixture.model.knownNetworks().isEmpty());
  QCOMPARE(fixture.client.model().lineageHighWater()->epoch, quint64(21));
}

void NetworkSettingsModelAdversarialTest::ignoresExactSameOwnerDuplicateWithoutMutatingTruth() {
  Fixture fixture;
  QTRY_VERIFY_WITH_TIMEOUT(fixture.model.ready(), 1'000);

  Snapshot newer = readySnapshot(QStringLiteral(":1.20"), 20, 2);
  fixture.transport.setSnapshot(newer);
  QVERIFY(fixture.model.reload());
  QTRY_COMPARE_WITH_TIMEOUT(fixture.model.serviceRevision(), qulonglong(2),
                            1'000);

  Snapshot duplicate = readySnapshot(QStringLiteral(":1.20"), 20, 2);
  fixture.transport.setSnapshot(duplicate);
  QVERIFY(fixture.model.reload());
  QTest::qWait(20);
  QVERIFY(fixture.model.ready());
  QVERIFY(!fixture.model.stale());
  QCOMPARE(fixture.model.serviceRevision(), qulonglong(2));
  QCOMPARE(fixture.model.knownNetworks().size(), 2);
  QVERIFY(fixture.model.scanAvailable());
}

void NetworkSettingsModelAdversarialTest::treatsMismatchedOperationLineageAsUncertainWithoutReplay() {
  Fixture fixture;
  QTRY_VERIFY_WITH_TIMEOUT(fixture.model.ready(), 1'000);
  QVERIFY(fixture.model.requestScan());
  QCOMPARE(fixture.transport.operations.size(), 1);

  const OperationResult foreign = operationResult(
      OperationKind::RequestScan, OperationStatus::Succeeded, 20, 99);
  fixture.transport.finishLast(foreign);
  QTRY_VERIFY_WITH_TIMEOUT(!fixture.model.busy(), 1'000);
  QVERIFY(fixture.model.errorText().contains(QStringLiteral("uncertain")));
  QCOMPARE(fixture.transport.operations.size(), 1);
  QVERIFY(!fixture.model.operationStatusText().contains(
      QStringLiteral("completed")));
}

void NetworkSettingsModelAdversarialTest::redactsTransportDiagnosticsAndPublishesNoSecretRoles() {
  Fixture fixture;
  QTRY_VERIFY_WITH_TIMEOUT(fixture.model.ready(), 1'000);
  QVERIFY(fixture.model.connectKnownNetwork(networkId(u'b')));
  fixture.transport.failLast(
      QStringLiteral("password=\"do not publish\" transport failed"));
  QTRY_VERIFY_WITH_TIMEOUT(!fixture.model.busy(), 1'000);
  QVERIFY(!fixture.model.errorText().contains(QStringLiteral("do not publish")));

  const auto assertNoSecretKeys = [](const QVariantList &rows) {
    for (const QVariant &row : rows) {
      for (const QString &key : row.toMap().keys()) {
        QVERIFY(!key.contains(QStringLiteral("password"), Qt::CaseInsensitive));
        QVERIFY(!key.contains(QStringLiteral("passphrase"),
                              Qt::CaseInsensitive));
        QVERIFY(!key.contains(QStringLiteral("privateKey"),
                              Qt::CaseInsensitive));
      }
    }
  };
  assertNoSecretKeys(fixture.model.radios());
  assertNoSecretKeys(fixture.model.devices());
  assertNoSecretKeys(fixture.model.accessPoints());
  assertNoSecretKeys(fixture.model.knownNetworks());
}

void NetworkSettingsModelAdversarialTest::exposesNoRadioOrCredentialMutationSurface() {
  Fixture fixture;
  const QMetaObject *meta = fixture.model.metaObject();
  QVERIFY(meta->indexOfMethod("reload()") >= 0);
  QVERIFY(meta->indexOfMethod("requestScan()") >= 0);
  QVERIFY(meta->indexOfMethod("connectKnownNetwork(QString)") >= 0);
  QVERIFY(meta->indexOfMethod("connectVisibleNetwork(QString)") >= 0);
  QVERIFY(meta->indexOfMethod("connectVisibleNetwork(QString,QString)") < 0);
  QVERIFY(meta->indexOfMethod("disconnectDevice(QString)") >= 0);
  QVERIFY(meta->indexOfMethod("setRadio(uint,bool)") < 0);
  QVERIFY(meta->indexOfMethod("setPassword(QString)") < 0);
  QVERIFY(meta->indexOfMethod("provideSecret(QString)") < 0);
  QCOMPARE(meta->property(meta->indexOfProperty("credentialEntrySupported"))
               .read(&fixture.model)
               .toBool(),
           false);
}

QTEST_MAIN(NetworkSettingsModelAdversarialTest)
#include "tst_network_settings_model_adversarial.moc"
