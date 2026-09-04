// SPDX-License-Identifier: GPL-3.0-or-later

#include "bluetooth_settings_test_support.h"

#include <qindaqt/apps/settings_bluetooth/bluetooth_settings_model.h>

#include <QtTest>

using namespace QindaQt::Apps::SettingsBluetooth;
using namespace QindaQt::Apps::SettingsBluetooth::TestSupport;
using namespace QindaQt::Bluetooth;

class BluetoothSettingsModelTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void projectsBoundedAddressFreeInventoryAndAdmission();
  void serializesDiscoveryAndReleasesOnDeparture();
  void rejectsUnpairedAndFencesPendingOperations();
  void ownerReplacementClearsTruthAndRetiresLease();
  void pairingPromptRepliesWhilePairIsPending();

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

void BluetoothSettingsModelTest::projectsBoundedAddressFreeInventoryAndAdmission() {
  Fixture fixture;
  QVERIFY(fixture.model.ready());
  QVERIFY(fixture.model.pairingSupported());
  QCOMPARE(fixture.model.serviceOwner(), QStringLiteral(":1.42"));
  QCOMPARE(fixture.model.serviceEpoch(), qulonglong(61));
  QCOMPARE(fixture.model.serviceRevision(), qulonglong(5));

  const QVariantList adapters = fixture.model.adapters();
  QCOMPARE(adapters.size(), 1);
  const QVariantMap adapter = adapters.constFirst().toMap();
  QCOMPARE(adapter.value(QStringLiteral("label")).toString(),
           QStringLiteral("Bluetooth adapter 1"));
  QVERIFY(!adapter.contains(QStringLiteral("address")));
  QVERIFY(adapter.value(QStringLiteral("powerAvailable")).toBool());
  QVERIFY(adapter.value(QStringLiteral("startDiscoveryAvailable")).toBool());

  const QVariantList devices = fixture.model.devices();
  QCOMPARE(devices.size(), 3);
  QCOMPARE(devices.at(0).toMap().value(QStringLiteral("iconName")).toString(),
           QStringLiteral("audio-headphones"));
  QCOMPARE(devices.at(0).toMap().value(QStringLiteral("iconText")).toString(),
           QStringLiteral("HP"));
  QCOMPARE(devices.at(0).toMap().value(QStringLiteral("rssi")).toInt(), -42);
  QVERIFY(devices.at(0).toMap()
              .value(QStringLiteral("disconnectAvailable")).toBool());
  QVERIFY(devices.at(1).toMap()
              .value(QStringLiteral("connectAvailable")).toBool());
  QVERIFY(!devices.at(2).toMap()
               .value(QStringLiteral("connectAvailable")).toBool());
  QVERIFY(devices.at(2).toMap()
              .value(QStringLiteral("pairAvailable")).toBool());
  QVERIFY(devices.at(0).toMap()
              .value(QStringLiteral("forgetAvailable")).toBool());
  for (const QVariant &row : devices)
    QVERIFY(!row.toMap().contains(QStringLiteral("address")));
}

void BluetoothSettingsModelTest::serializesDiscoveryAndReleasesOnDeparture() {
  Fixture fixture;
  QVERIFY(fixture.model.requestDiscovery(QStringLiteral("adapter-61-400"), true));
  QCOMPARE(fixture.transport.submissions.size(), 1);
  QCOMPARE(fixture.transport.submissions.constLast().request.kind,
           OperationKind::AcquireDiscovery);
  QVERIFY(fixture.model.busy());
  QVERIFY(!fixture.model.adapters().constFirst().toMap()
               .value(QStringLiteral("startDiscoveryAvailable")).toBool());

  const auto acquire = fixture.transport.submissions.constLast();
  fixture.transport.finishOperation(acquire, successResult(acquire, 6));
  QTRY_VERIFY(fixture.model.discoveryLeaseHeld());
  QTRY_COMPARE(fixture.transport.fetches.size(), 2);
  fixture.transport.finishSnapshot(QStringLiteral(":1.42"),
                                   fixture.transport.fetches.constLast().second,
                                   readySnapshot(61, 6, true));
  QTRY_VERIFY(!fixture.model.busy());
  QVERIFY(fixture.model.adapters().constFirst().toMap()
              .value(QStringLiteral("stopDiscoveryAvailable")).toBool());

  fixture.model.setRouteActive(false);
  QVERIFY(fixture.model.departureReleasePending());
  QTRY_COMPARE(fixture.transport.submissions.size(), 2);
  const auto release = fixture.transport.submissions.constLast();
  QCOMPARE(release.request.kind, OperationKind::ReleaseDiscovery);
  fixture.transport.finishOperation(release, successResult(release, 7, 6));
  QTRY_VERIFY(!fixture.model.departureReleasePending());
  QTRY_COMPARE(fixture.transport.fetches.size(), 3);
  fixture.transport.finishSnapshot(QStringLiteral(":1.42"),
                                   fixture.transport.fetches.constLast().second,
                                   readySnapshot(61, 7, false));
  QTRY_VERIFY(!fixture.model.discoveryLeaseHeld());
}

void BluetoothSettingsModelTest::rejectsUnpairedAndFencesPendingOperations() {
  Fixture fixture;
  QVERIFY(!fixture.model.requestDeviceConnection(
      QStringLiteral("device-61-702"), true));
  QCOMPARE(fixture.transport.submissions.size(), 0);
  QVERIFY(fixture.model.errorText().contains(QStringLiteral("not-paired")));

  QVERIFY(fixture.model.requestDeviceConnection(
      QStringLiteral("device-61-701"), true));
  QCOMPARE(fixture.transport.submissions.size(), 1);
  QVERIFY(fixture.model.busy());
  for (const QVariant &row : fixture.model.devices()) {
    QVERIFY(!row.toMap().value(QStringLiteral("connectAvailable")).toBool());
    QVERIFY(!row.toMap().value(QStringLiteral("disconnectAvailable")).toBool());
  }
  QVERIFY(!fixture.model.requestAdapterPower(
      QStringLiteral("adapter-61-400"), false));
  QCOMPARE(fixture.transport.submissions.size(), 1);
}

void BluetoothSettingsModelTest::ownerReplacementClearsTruthAndRetiresLease() {
  Fixture fixture;
  QVERIFY(fixture.model.requestDiscovery(QStringLiteral("adapter-61-400"), true));
  const auto acquire = fixture.transport.submissions.constLast();
  fixture.transport.finishOperation(acquire, successResult(acquire, 6));
  QTRY_VERIFY(fixture.model.discoveryLeaseHeld());

  fixture.transport.announceOwner(QStringLiteral(":1.84"));
  QTRY_VERIFY(!fixture.model.ready());
  QVERIFY(fixture.model.adapters().isEmpty());
  QVERIFY(fixture.model.devices().isEmpty());
  QVERIFY(!fixture.model.discoveryLeaseHeld());
  QVERIFY(fixture.model.errorText().contains(QStringLiteral("authority"),
                                             Qt::CaseInsensitive));
  QCOMPARE(fixture.transport.submissions.size(), 1);
}

void BluetoothSettingsModelTest::pairingPromptRepliesWhilePairIsPending() {
  Fixture fixture;
  QVERIFY(fixture.model.requestPairing(QStringLiteral("device-61-702")));
  QCOMPARE(fixture.transport.submissions.size(), 1);
  const auto pairing = fixture.transport.submissions.constFirst();
  QCOMPARE(pairing.request.kind, OperationKind::Pair);

  fixture.transport.announceOwner(QStringLiteral(":1.42"));
  Q_EMIT fixture.transport.invalidated(QStringLiteral(":1.42"), 61, 6);
  QTRY_COMPARE(fixture.transport.fetches.size(), 2);
  Snapshot prompted = readySnapshot(61, 6);
  prompted.pairingPrompt = {
      .promptId = 102,
      .kind = PairingPromptKind::EnterPasskey,
      .device = prompted.devices.at(2).handle,
      .detail = {},
      .serviceUuid = {},
      .entered = 0,
  };
  fixture.transport.finishSnapshot(QStringLiteral(":1.42"),
                                   fixture.transport.fetches.constLast().second,
                                   prompted);
  QTRY_VERIFY(fixture.model.pairingPrompt().value(QStringLiteral("active")).toBool());
  QVERIFY(fixture.model.replyPasskey(QStringLiteral("654321")));
  QCOMPARE(fixture.transport.submissions.size(), 2);
  const auto reply = fixture.transport.submissions.constLast();
  QCOMPARE(reply.request.kind, OperationKind::ReplyPasskey);
  QCOMPARE(pairingInputString(reply.request.input, reply.request.inputSize),
           QStringLiteral("654321"));
  fixture.transport.finishOperation(reply, successResult(reply, 7, 6));
  QTRY_VERIFY(!fixture.model.pairingReplyPending());

  fixture.transport.finishOperation(pairing, successResult(pairing, 7));
  Snapshot paired = readySnapshot(61, 7);
  paired.devices[2].paired = true;
  fixture.transport.finishSnapshot(QStringLiteral(":1.42"),
                                   fixture.transport.fetches.constLast().second,
                                   paired);
  QTRY_VERIFY(!fixture.model.busy());
  if (fixture.transport.fetches.size() == 3) {
    fixture.transport.finishSnapshot(QStringLiteral(":1.42"),
                                     fixture.transport.fetches.constLast().second,
                                     paired);
  }
}

QTEST_MAIN(BluetoothSettingsModelTest)
#include "tst_bluetooth_settings_model.moc"
