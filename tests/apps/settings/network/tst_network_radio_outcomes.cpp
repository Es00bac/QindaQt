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
  qint64 now = 3'000;
  FakeNetworkTransport transport;
  NetworkClient client;
  NetworkSettingsModel model;
  Fixture()
      : client(transport, [this] { return now; }, fastTiming()),
        model(client, QDBusConnection(QStringLiteral("no-host-presence"))) {
    Snapshot initial = radioSnapshot();
    transport.setSnapshot(initial);
    Q_ASSERT(client.start());
    transport.announceOwner(initial.owner);
  }
  static Snapshot radioSnapshot(quint64 revision = 1) {
    Snapshot state = readySnapshot(QStringLiteral(":1.20"), 20, revision);
    state.capabilities |= Capability::RadioControl;
    state.radios[1] = {RadioKind::Wwan, true, true, false};
    return state;
  }
  QVariantMap radio(int index) const { return model.radios().at(index).toMap(); }
};
}

class NetworkRadioOutcomesTest final : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void dispatchesTypedWifiAndMobileChanges();
  void refusesAbsentHardwareCapabilityAndOccupiedLane();
  void waitsForNewerAuthoritativeReadback();
  void automaticallyRefetchesStaleReadback();
  void retiresUnconfirmedChangeWithoutReplay();
  void clearsOnOwnerReplacementAndShowsExternalState();
  void lostReplyDoesNotReplay();
  void ownerReplacementDuringReadbackRetiresIntent();
  void settledSuccessExpiresOnExternalTruthOrOwnerChange();
};

void NetworkRadioOutcomesTest::dispatchesTypedWifiAndMobileChanges() {
  Fixture f;
  QTRY_VERIFY(f.model.ready());
  QCOMPARE(f.radio(0).value(QStringLiteral("softwareEnabled")).toBool(), true);
  QCOMPARE(f.radio(1).value(QStringLiteral("softwareEnabled")).toBool(), false);
  QVERIFY(f.radio(0).value(QStringLiteral("controlAvailable")).toBool());
  QVERIFY(f.radio(1).value(QStringLiteral("controlAvailable")).toBool());

  QVERIFY(f.model.setRadio(0, false));
  QCOMPARE(f.transport.operations.size(), 1);
  QCOMPARE(f.transport.operations.last().kind, OperationKind::SetRadio);
  QCOMPARE(f.transport.operations.last().parameters.value(QStringLiteral("radioKind")).toUInt(), 0u);
  QCOMPARE(f.transport.operations.last().parameters.value(QStringLiteral("enable")).toBool(), false);
  QVERIFY(f.radio(0).value(QStringLiteral("pending")).toBool());
  QCOMPARE(f.radio(0).value(QStringLiteral("softwareEnabled")).toBool(), true);
  f.transport.setSnapshot(Fixture::radioSnapshot(1));
  f.transport.finishLast(operationResult(OperationKind::SetRadio, OperationStatus::Succeeded));
  QVERIFY(f.radio(0).value(QStringLiteral("pending")).toBool());
  Snapshot confirmed = Fixture::radioSnapshot(2);
  confirmed.radios[0].softwareEnabled = false;
  f.transport.setSnapshot(confirmed);
  QVERIFY(f.model.reload());
  QTRY_VERIFY(!f.radio(0).value(QStringLiteral("pending")).toBool());
  QCOMPARE(f.radio(0).value(QStringLiteral("softwareEnabled")).toBool(), false);
  QVERIFY(f.model.operationStatusText().contains(QStringLiteral("off")));

  QVERIFY(f.model.setRadio(1, true));
  QCOMPARE(f.transport.operations.size(), 2);
  QCOMPARE(f.transport.operations.last().parameters.value(QStringLiteral("radioKind")).toUInt(), 1u);
  QCOMPARE(f.transport.operations.last().parameters.value(QStringLiteral("enable")).toBool(), true);
  Snapshot mobile = confirmed;
  mobile.revision = 3;
  mobile.radios[1].softwareEnabled = true;
  f.transport.setSnapshot(mobile);
  f.transport.finishLast(operationResult(OperationKind::SetRadio, OperationStatus::Succeeded, 20, 2));
  QTRY_VERIFY(!f.radio(1).value(QStringLiteral("pending")).toBool());
  QCOMPARE(f.radio(1).value(QStringLiteral("softwareEnabled")).toBool(), true);
  QCOMPARE(f.transport.operations.size(), 2);
}

void NetworkRadioOutcomesTest::refusesAbsentHardwareCapabilityAndOccupiedLane() {
  Fixture f;
  QTRY_VERIFY(f.model.ready());
  Snapshot state = Fixture::radioSnapshot(2);
  state.radios[0].hardwareEnabled = false;
  state.radios[1].present = false;
  f.transport.setSnapshot(state);
  QVERIFY(f.model.reload());
  QTRY_COMPARE(f.model.serviceRevision(), qulonglong(2));
  QVERIFY(!f.radio(0).value(QStringLiteral("controlAvailable")).toBool());
  QVERIFY(!f.radio(1).value(QStringLiteral("controlAvailable")).toBool());
  QVERIFY(f.radio(0).value(QStringLiteral("detailText")).toString().contains(QStringLiteral("hardware")));
  QVERIFY(!f.model.setRadio(0, false));
  QVERIFY(!f.model.setRadio(1, true));
  QCOMPARE(f.transport.operations.size(), 0);

  state.revision = 3;
  state.radios[0].hardwareEnabled = true;
  state.radios[1].present = true;
  state.capabilities &= ~Capabilities(Capability::RadioControl);
  f.transport.setSnapshot(state);
  QVERIFY(f.model.reload());
  QTRY_COMPARE(f.model.serviceRevision(), qulonglong(3));
  QVERIFY(!f.radio(0).value(QStringLiteral("controlAvailable")).toBool());
  QVERIFY(!f.model.setRadio(0, false));
  QCOMPARE(f.transport.operations.size(), 0);

  state.revision = 4;
  state.capabilities |= Capability::RadioControl;
  f.transport.setSnapshot(state);
  QVERIFY(f.model.reload());
  QTRY_COMPARE(f.model.serviceRevision(), qulonglong(4));
  f.transport.clearSnapshot();
  QVERIFY(f.model.reload());
  QVERIFY(!f.radio(0).value(QStringLiteral("controlAvailable")).toBool());
  QVERIFY(!f.model.setRadio(0, false));
  QCOMPARE(f.transport.operations.size(), 0);
}

void NetworkRadioOutcomesTest::waitsForNewerAuthoritativeReadback() {
  Fixture f;
  QTRY_VERIFY(f.model.ready());
  f.transport.setSnapshot(Fixture::radioSnapshot(1));
  QVERIFY(f.model.setRadio(0, false));
  f.transport.finishLast(operationResult(OperationKind::SetRadio, OperationStatus::Succeeded));
  QTRY_VERIFY(f.transport.snapshotRequests.size() >= 3);
  QVERIFY(f.radio(0).value(QStringLiteral("pending")).toBool());
  QCOMPARE(f.radio(0).value(QStringLiteral("softwareEnabled")).toBool(), true);
  Snapshot conflict = Fixture::radioSnapshot(2);
  f.transport.setSnapshot(conflict);
  QVERIFY(f.model.reload());
  QTRY_VERIFY(!f.radio(0).value(QStringLiteral("pending")).toBool());
  QCOMPARE(f.radio(0).value(QStringLiteral("softwareEnabled")).toBool(), true);
  QVERIFY(f.model.errorText().contains(QStringLiteral("differs")));
  QCOMPARE(f.transport.operations.size(), 1);
}

void NetworkRadioOutcomesTest::automaticallyRefetchesStaleReadback() {
  Fixture f;
  QTRY_VERIFY(f.model.ready());
  f.transport.setSnapshot(Fixture::radioSnapshot(1));
  QVERIFY(f.model.setRadio(0, false));
  f.transport.finishLast(operationResult(OperationKind::SetRadio, OperationStatus::Succeeded));
  QTRY_VERIFY(f.transport.snapshotRequests.size() >= 2);
  QVERIFY(f.radio(0).value(QStringLiteral("pending")).toBool());
  Snapshot confirmed = Fixture::radioSnapshot(2);
  confirmed.radios[0].softwareEnabled = false;
  f.transport.setSnapshot(confirmed);
  QTRY_VERIFY_WITH_TIMEOUT(!f.radio(0).value(QStringLiteral("pending")).toBool(), 2'000);
  QCOMPARE(f.radio(0).value(QStringLiteral("softwareEnabled")).toBool(), false);
  QCOMPARE(f.transport.operations.size(), 1);
  QVERIFY(f.transport.snapshotRequests.size() >= 3);
}

void NetworkRadioOutcomesTest::retiresUnconfirmedChangeWithoutReplay() {
  Fixture f;
  QTRY_VERIFY(f.model.ready());
  QVERIFY(f.model.setRadio(0, false));
  f.transport.finishLast(operationResult(OperationKind::SetRadio, OperationStatus::Rejected,
                                         20, 1, QStringLiteral("radio-hardware-disabled")));
  QTRY_VERIFY(!f.radio(0).value(QStringLiteral("pending")).toBool());
  QVERIFY(f.model.errorText().contains(QStringLiteral("hardware")));
  QCOMPARE(f.transport.operations.size(), 1);

  QTRY_VERIFY(f.client.operationAdmissionReady());
  QVERIFY(f.model.setRadio(0, false));
  f.transport.setSnapshot(Fixture::radioSnapshot(1));
  f.transport.finishLast(operationResult(OperationKind::SetRadio, OperationStatus::Succeeded));
  QTest::qWait(5'300);
  QVERIFY(!f.radio(0).value(QStringLiteral("pending")).toBool());
  QVERIFY(f.model.errorText().contains(QStringLiteral("confirmed in time")));
  QCOMPARE(f.transport.operations.size(), 2);
}

void NetworkRadioOutcomesTest::clearsOnOwnerReplacementAndShowsExternalState() {
  Fixture f;
  QTRY_VERIFY(f.model.ready());
  QVERIFY(f.model.setRadio(1, true));
  Snapshot replacement = Fixture::radioSnapshot(1);
  replacement.owner = QStringLiteral(":1.21");
  replacement.epoch = 21;
  replacement.radios[1].softwareEnabled = true;
  f.transport.setSnapshot(replacement);
  f.transport.announceOwner(replacement.owner);
  QTRY_COMPARE(f.model.serviceOwner(), replacement.owner);
  QVERIFY(!f.radio(1).value(QStringLiteral("pending")).toBool());
  QVERIFY(!f.model.errorText().isEmpty());
  QCOMPARE(f.radio(1).value(QStringLiteral("softwareEnabled")).toBool(), true);
  QCOMPARE(f.transport.operations.size(), 1);
}

void NetworkRadioOutcomesTest::lostReplyDoesNotReplay() {
  Fixture f;
  QTRY_VERIFY(f.model.ready());
  QVERIFY(f.model.setRadio(0, false));
  QTRY_VERIFY_WITH_TIMEOUT(!f.radio(0).value(QStringLiteral("pending")).toBool(), 1'000);
  QVERIFY(f.model.errorText().contains(QStringLiteral("uncertain")));
  QCOMPARE(f.transport.operations.size(), 1);
}

void NetworkRadioOutcomesTest::ownerReplacementDuringReadbackRetiresIntent() {
  Fixture f;
  QTRY_VERIFY(f.model.ready());
  f.transport.setSnapshot(Fixture::radioSnapshot(1));
  QVERIFY(f.model.setRadio(0, false));
  f.transport.finishLast(operationResult(OperationKind::SetRadio, OperationStatus::Succeeded));
  QVERIFY(f.radio(0).value(QStringLiteral("pending")).toBool());
  Snapshot replacement = Fixture::radioSnapshot(1);
  replacement.owner = QStringLiteral(":1.21");
  replacement.epoch = 21;
  f.transport.setSnapshot(replacement);
  f.transport.announceOwner(replacement.owner);
  QTRY_COMPARE(f.model.serviceOwner(), replacement.owner);
  QVERIFY(!f.radio(0).value(QStringLiteral("pending")).toBool());
  QVERIFY(!f.model.errorText().isEmpty());
  QCOMPARE(f.transport.operations.size(), 1);
}

void NetworkRadioOutcomesTest::settledSuccessExpiresOnExternalTruthOrOwnerChange() {
  {
    Fixture f;
    QTRY_VERIFY(f.model.ready());
    QVERIFY(f.model.setRadio(0, false));
    Snapshot off = Fixture::radioSnapshot(2);
    off.radios[0].softwareEnabled = false;
    f.transport.setSnapshot(off);
    f.transport.finishLast(operationResult(OperationKind::SetRadio, OperationStatus::Succeeded));
    QTRY_VERIFY(!f.radio(0).value(QStringLiteral("pending")).toBool());
    QVERIFY(f.model.operationStatusText().contains(QStringLiteral("off")));
    Snapshot external = Fixture::radioSnapshot(3);
    f.transport.setSnapshot(external);
    f.transport.invalidate();
    QTRY_COMPARE(f.model.serviceRevision(), qulonglong(3));
    QCOMPARE(f.radio(0).value(QStringLiteral("softwareEnabled")).toBool(), true);
    QVERIFY(f.model.operationStatusText().isEmpty());
    QCOMPARE(f.transport.operations.size(), 1);
  }
  {
    Fixture f;
    QTRY_VERIFY(f.model.ready());
    QVERIFY(f.model.setRadio(0, false));
    Snapshot off = Fixture::radioSnapshot(2);
    off.radios[0].softwareEnabled = false;
    f.transport.setSnapshot(off);
    f.transport.finishLast(operationResult(OperationKind::SetRadio, OperationStatus::Succeeded));
    QTRY_VERIFY(!f.radio(0).value(QStringLiteral("pending")).toBool());
    QVERIFY(f.model.operationStatusText().contains(QStringLiteral("off")));
    Snapshot replacement = Fixture::radioSnapshot(1);
    replacement.owner = QStringLiteral(":1.21");
    replacement.epoch = 21;
    f.transport.setSnapshot(replacement);
    f.transport.announceOwner(replacement.owner);
    QTRY_COMPARE(f.model.serviceOwner(), replacement.owner);
    QVERIFY(f.model.operationStatusText().isEmpty());
    QCOMPARE(f.radio(0).value(QStringLiteral("softwareEnabled")).toBool(), true);
    QCOMPARE(f.transport.operations.size(), 1);
  }
}

QTEST_MAIN(NetworkRadioOutcomesTest)
#include "tst_network_radio_outcomes.moc"
