// SPDX-License-Identifier: GPL-3.0-or-later

#include "bluetooth_settings_test_support.h"

#include <qindaqt/apps/settings_bluetooth/bluetooth_settings_model.h>

#include <QtTest>

using namespace QindaQt::Apps::SettingsBluetooth;
using namespace QindaQt::Apps::SettingsBluetooth::TestSupport;
using namespace QindaQt::Bluetooth;

class BluetoothSettingsAdversarialTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void unsuccessfulAcquireAfterDepartureReleasesCloseFence_data();
  void unsuccessfulAcquireAfterDepartureReleasesCloseFence();
  void ownerChangeDuringAcquireReleasesCloseFence_data();
  void ownerChangeDuringAcquireReleasesCloseFence();
  void malformedSnapshotNeverBecomesActionable_data();
  void malformedSnapshotNeverBecomesActionable();

private:
  struct Fixture {
    FakeBluetoothTransport transport;
    BluetoothClient client{&transport};
    BluetoothSettingsModel model{client};

    Fixture() {
      client.start();
      transport.announceOwner(QStringLiteral(":1.42"));
      Q_ASSERT(transport.fetches.size() == 1);
      transport.finishSnapshot(QStringLiteral(":1.42"),
                               transport.fetches.constFirst().second,
                               readySnapshot());
      model.setRouteActive(true);
    }
  };
};

void BluetoothSettingsAdversarialTest::
    unsuccessfulAcquireAfterDepartureReleasesCloseFence_data() {
  QTest::addColumn<OperationStatus>("status");
  QTest::addColumn<bool>("exactInitiator");
  QTest::addColumn<QString>("reasonCode");

  QTest::newRow("rejected-too-many-leases")
      << OperationStatus::Rejected << true
      << QStringLiteral("too-many-leases");
  QTest::newRow("failed") << OperationStatus::Failed << true
                           << QStringLiteral("backend-malformed");
  QTest::newRow("uncertain") << OperationStatus::Uncertain << true
                              << QStringLiteral("operation-timeout");
  QTest::newRow("inexact-wire-result")
      << OperationStatus::Rejected << false
      << QStringLiteral("too-many-leases");
}

void BluetoothSettingsAdversarialTest::
    unsuccessfulAcquireAfterDepartureReleasesCloseFence() {
  QFETCH(OperationStatus, status);
  QFETCH(bool, exactInitiator);
  QFETCH(QString, reasonCode);
  Fixture fixture;

  QVERIFY(fixture.model.requestDiscovery(QStringLiteral("adapter-61-400"),
                                         true));
  QCOMPARE(fixture.transport.submissions.size(), 1);
  const auto acquire = fixture.transport.submissions.constLast();
  fixture.model.setRouteActive(false);
  QVERIFY(fixture.model.departureReleasePending());

  const OperationResult result{
      .kind = OperationKind::AcquireDiscovery,
      .status = status,
      .initiatingEpoch = 61,
      .initiatingRevision = exactInitiator ? quint64{5} : quint64{4},
      .observedEpoch = 61,
      .observedRevision = 5,
      .reasonCode = reasonCode,
      .diagnostic = {},
      .wireValid = true,
  };
  fixture.transport.finishOperation(acquire, result);

  QTRY_VERIFY(!fixture.model.busy());
  QVERIFY(!fixture.model.discoveryLeaseHeld());
  QVERIFY(!fixture.model.departureReleasePending());
  QCOMPARE(fixture.transport.submissions.size(), 1);

  QTRY_COMPARE(fixture.transport.fetches.size(), 2);
  fixture.transport.finishSnapshot(QStringLiteral(":1.42"),
                                   fixture.transport.fetches.constLast().second,
                                   readySnapshot(61, 8, false));
  QVERIFY(!fixture.model.departureReleasePending());
  QCOMPARE(fixture.transport.submissions.size(), 1);
}

void BluetoothSettingsAdversarialTest::
    ownerChangeDuringAcquireReleasesCloseFence_data() {
  QTest::addColumn<QString>("replacementOwner");
  QTest::newRow("owner-loss") << QString{};
  QTest::newRow("owner-replacement") << QStringLiteral(":1.84");
}

void BluetoothSettingsAdversarialTest::
    ownerChangeDuringAcquireReleasesCloseFence() {
  QFETCH(QString, replacementOwner);
  Fixture fixture;
  QVERIFY(fixture.model.requestDiscovery(QStringLiteral("adapter-61-400"),
                                         true));
  fixture.model.setRouteActive(false);
  QVERIFY(fixture.model.departureReleasePending());

  fixture.transport.announceOwner(replacementOwner);
  QTRY_VERIFY(!fixture.model.departureReleasePending());
  QVERIFY(!fixture.model.discoveryLeaseHeld());
  QCOMPARE(fixture.transport.submissions.size(), 1);

  if (!replacementOwner.isEmpty()) {
    QTRY_COMPARE(fixture.transport.fetches.size(), 2);
    fixture.transport.finishSnapshot(
        replacementOwner, fixture.transport.fetches.constLast().second,
        readySnapshot(72, 1, false));
    QTRY_VERIFY(fixture.model.ready());
    QVERIFY(!fixture.model.departureReleasePending());
  }
}

void BluetoothSettingsAdversarialTest::
    malformedSnapshotNeverBecomesActionable_data() {
  QTest::addColumn<Snapshot>("snapshot");

  Snapshot duplicateId = readySnapshot();
  duplicateId.devices[1].handle = duplicateId.devices[0].handle;
  QTest::newRow("duplicate-id") << duplicateId;

  Snapshot overlongName = readySnapshot();
  overlongName.devices[0].name = QString(257, QLatin1Char('x'));
  QTest::newRow("overlong-name") << overlongName;

  Snapshot invalidRssi = readySnapshot();
  invalidRssi.devices[0].rssi = -129;
  QTest::newRow("out-of-range-rssi") << invalidRssi;

  Snapshot invalidClass = readySnapshot();
  invalidClass.devices[0].deviceClass = static_cast<DeviceClass>(999);
  QTest::newRow("out-of-range-class") << invalidClass;
}

void BluetoothSettingsAdversarialTest::malformedSnapshotNeverBecomesActionable() {
  QFETCH(Snapshot, snapshot);
  FakeBluetoothTransport transport;
  BluetoothClient client{&transport};
  BluetoothSettingsModel model{client};

  client.start();
  transport.announceOwner(QStringLiteral(":1.42"));
  QCOMPARE(transport.fetches.size(), 1);
  transport.finishSnapshot(QStringLiteral(":1.42"),
                           transport.fetches.constFirst().second, snapshot);

  QCOMPARE(client.state(), ClientState::Unavailable);
  QCOMPARE(client.reasonCode(), QStringLiteral("malformed-snapshot"));
  QVERIFY(!client.hasSnapshot());
  QVERIFY(!model.ready());
  QVERIFY(model.adapters().isEmpty());
  QVERIFY(model.devices().isEmpty());
  model.setRouteActive(true);
  QVERIFY(!model.requestDiscovery(QStringLiteral("adapter-61-400"), true));
  QVERIFY(transport.submissions.isEmpty());
}

QTEST_MAIN(BluetoothSettingsAdversarialTest)
#include "tst_bluetooth_settings_adversarial.moc"
