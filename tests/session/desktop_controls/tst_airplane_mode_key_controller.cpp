// SPDX-License-Identifier: GPL-3.0-or-later

#include "network_protocol_test_data.h"

#include <qindaqt/services/network_client/network_client.h>
#include <qindaqt/services/network_client/network_transport.h>
#include <qindaqt/services/network_protocol/network_codec.h>
#include <qindaqt/session/desktop_controls/airplane_mode_key_controller.h>

#include <QtTest>

#include <memory>
#include <optional>

using namespace QindaQt::Network;
using namespace QindaQt::Network::Client;
using namespace QindaQt::Network::TestData;
using namespace QindaQt::Session::DesktopControls;

namespace {

class FakeNetworkTransport final : public NetworkTransport {
  Q_OBJECT

public:
  bool start(QString * /*error*/ = nullptr) override {
    m_running = true;
    return true;
  }
  void stop() override { m_running = false; }
  void requestSnapshot(const quint64 token, const QString &owner) override {
    if (m_autoSnapshot.has_value() && m_running) {
      Q_EMIT snapshotReceived(token, owner, m_autoSnapshot->second);
    }
  }
  void requestOperation(const quint64 token, const QString &owner,
                        const quint64 /*epoch*/, const quint64 /*revision*/,
                        const OperationKind kind,
                        const QVariantMap &parameters) override {
    operations.append(OperationCall{token, owner, kind, parameters});
  }

  void setAutoSnapshot(const Snapshot &snapshot) {
    const EncodeResult encoded = encodeSnapshot(snapshot);
    Q_ASSERT(encoded.succeeded());
    m_autoSnapshot = qMakePair(snapshot, encoded.payload);
  }
  void emitOwner(const QString &owner) { Q_EMIT ownerChanged(owner); }
  void completeLast(const OperationStatus status, const QString &reasonCode) {
    Q_ASSERT(!operations.isEmpty());
    const auto &call = operations.constLast();
    OperationResult result;
    result.kind = call.kind;
    result.status = status;
    result.initiatingEpoch = 41;
    result.initiatingRevision = 7;
    result.reasonCode = reasonCode;
    const EncodeResult encoded = encodeOperationResult(result);
    Q_ASSERT(encoded.succeeded());
    Q_EMIT operationReceived(call.token, call.owner, encoded.payload);
  }

  struct OperationCall {
    quint64 token = 0;
    QString owner;
    OperationKind kind = OperationKind::RequestScan;
    QVariantMap parameters;
  };
  QList<OperationCall> operations;

private:
  bool m_running = false;
  std::optional<QPair<Snapshot, QByteArray>> m_autoSnapshot;
};

} // namespace

class AirplaneModeKeyControllerTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void initTestCase() {
    qRegisterMetaType<OperationResult>("QindaQt::Network::OperationResult");
  }

  void togglingFromEnabledDisablesTheSoleWifiRadio();
  void togglingFromDisabledEnablesTheSoleWifiRadio();
  void presentWwanRadioIsNeverToggled();
  void absentWwanRadioIsNeverTouched();
  void missingSnapshotReportsUnavailableWithoutOperation();
  void missingWifiRadioReportsUnavailable();
  void rejectedOperationReportsUnavailable();
  void uncertainOperationStaysQuiet();

private:
  std::unique_ptr<FakeNetworkTransport> m_transport;
  std::unique_ptr<NetworkClient> makeClient(const Snapshot &snapshot) {
    m_transport = std::make_unique<FakeNetworkTransport>();
    m_transport->setAutoSnapshot(snapshot);
    auto client = std::make_unique<NetworkClient>(*m_transport);
    if (!client->start()) {
      return nullptr;
    }
    m_transport->emitOwner(QStringLiteral(":1.23"));
    const bool ready = QTest::qWaitFor(
        [c = client.get()] { return c->state() == ClientState::Ready; }, 2'000);
    if (!ready) {
      return nullptr;
    }
    return client;
  }
};

void AirplaneModeKeyControllerTest::
    togglingFromEnabledDisablesTheSoleWifiRadio() {
  std::unique_ptr<NetworkClient> client = makeClient(validSnapshot());
  QVERIFY(client != nullptr);

  AirplaneModeKeyController controller(*client);
  QSignalSpy feedback(&controller,
                      &AirplaneModeKeyController::airplaneModeFeedbackRequested);
  QSignalSpy unavailable(&controller,
                        &AirplaneModeKeyController::airplaneModeUnavailable);
  controller.toggleAirplaneMode();

  QCOMPARE(unavailable.count(), 0);
  QCOMPARE(feedback.count(), 1);
  QCOMPARE(feedback.at(0).at(0).toBool(), true);
  QCOMPARE(m_transport->operations.size(), 1);
  QCOMPARE(m_transport->operations.constFirst().kind, OperationKind::SetRadio);
  QCOMPARE(m_transport->operations.constFirst()
               .parameters.value(QStringLiteral("enable"))
               .toBool(),
           false);
  client->stop();
}

void AirplaneModeKeyControllerTest::
    togglingFromDisabledEnablesTheSoleWifiRadio() {
  Snapshot snapshot = validSnapshot();
  snapshot.radios = {Radio{RadioKind::Wifi, true, true, false}};
  std::unique_ptr<NetworkClient> client = makeClient(snapshot);
  QVERIFY(client != nullptr);

  AirplaneModeKeyController controller(*client);
  QSignalSpy feedback(&controller,
                      &AirplaneModeKeyController::airplaneModeFeedbackRequested);
  controller.toggleAirplaneMode();

  QCOMPARE(feedback.count(), 1);
  QCOMPARE(feedback.at(0).at(0).toBool(), false);
  QCOMPARE(m_transport->operations.constFirst()
               .parameters.value(QStringLiteral("enable"))
               .toBool(),
           true);
  client->stop();
}

void AirplaneModeKeyControllerTest::presentWwanRadioIsNeverToggled() {
  // NetworkClient admits one operation at a time; a second setRadio() call
  // issued synchronously right after the first would be rejected as busy,
  // not queued, so this controller deliberately toggles only Wi-Fi even
  // when a WWAN radio is present.
  Snapshot snapshot = validSnapshot();
  snapshot.radios = {Radio{RadioKind::Wifi, true, true, true},
                     Radio{RadioKind::Wwan, true, true, true}};
  std::unique_ptr<NetworkClient> client = makeClient(snapshot);
  QVERIFY(client != nullptr);

  AirplaneModeKeyController controller(*client);
  controller.toggleAirplaneMode();

  QCOMPARE(m_transport->operations.size(), 1);
  QCOMPARE(m_transport->operations.constFirst()
               .parameters.value(QStringLiteral("radioKind"))
               .toInt(),
           static_cast<qint32>(RadioKind::Wifi));
  client->stop();
}

void AirplaneModeKeyControllerTest::absentWwanRadioIsNeverTouched() {
  std::unique_ptr<NetworkClient> client = makeClient(validSnapshot());
  QVERIFY(client != nullptr);

  AirplaneModeKeyController controller(*client);
  controller.toggleAirplaneMode();

  // validSnapshot() carries only a Wi-Fi radio; a second call would mean
  // this controller invented a WWAN request the model never reported.
  QCOMPARE(m_transport->operations.size(), 1);
  client->stop();
}

void AirplaneModeKeyControllerTest::
    missingSnapshotReportsUnavailableWithoutOperation() {
  m_transport = std::make_unique<FakeNetworkTransport>();
  NetworkClient client(*m_transport);
  QVERIFY(client.start());

  AirplaneModeKeyController controller(client);
  QSignalSpy unavailable(&controller,
                        &AirplaneModeKeyController::airplaneModeUnavailable);
  controller.toggleAirplaneMode();

  QCOMPARE(unavailable.count(), 1);
  QCOMPARE(unavailable.at(0).at(0).toString(), QStringLiteral("no-snapshot"));
  QCOMPARE(m_transport->operations.size(), 0);
  client.stop();
}

void AirplaneModeKeyControllerTest::missingWifiRadioReportsUnavailable() {
  Snapshot snapshot = validSnapshot();
  snapshot.radios.clear();
  std::unique_ptr<NetworkClient> client = makeClient(snapshot);
  QVERIFY(client != nullptr);

  AirplaneModeKeyController controller(*client);
  QSignalSpy unavailable(&controller,
                        &AirplaneModeKeyController::airplaneModeUnavailable);
  controller.toggleAirplaneMode();

  QCOMPARE(unavailable.count(), 1);
  QCOMPARE(unavailable.at(0).at(0).toString(), QStringLiteral("no-wifi-radio"));
  QCOMPARE(m_transport->operations.size(), 0);
  client->stop();
}

void AirplaneModeKeyControllerTest::rejectedOperationReportsUnavailable() {
  std::unique_ptr<NetworkClient> client = makeClient(validSnapshot());
  QVERIFY(client != nullptr);

  AirplaneModeKeyController controller(*client);
  QSignalSpy unavailable(&controller,
                        &AirplaneModeKeyController::airplaneModeUnavailable);
  controller.toggleAirplaneMode();
  QCOMPARE(m_transport->operations.size(), 1);
  m_transport->completeLast(OperationStatus::Rejected,
                            QStringLiteral("radio-busy"));

  QTRY_COMPARE(unavailable.count(), 1);
  QCOMPARE(unavailable.at(0).at(0).toString(), QStringLiteral("radio-busy"));
  client->stop();
}

void AirplaneModeKeyControllerTest::uncertainOperationStaysQuiet() {
  std::unique_ptr<NetworkClient> client = makeClient(validSnapshot());
  QVERIFY(client != nullptr);

  AirplaneModeKeyController controller(*client);
  QSignalSpy unavailable(&controller,
                        &AirplaneModeKeyController::airplaneModeUnavailable);
  controller.toggleAirplaneMode();
  QCOMPARE(m_transport->operations.size(), 1);
  m_transport->completeLast(OperationStatus::Uncertain,
                            QStringLiteral("owner-replaced"));

  QTest::qWait(50);
  QCOMPARE(unavailable.count(), 0);
  client->stop();
}

QTEST_MAIN(AirplaneModeKeyControllerTest)
#include "tst_airplane_mode_key_controller.moc"
