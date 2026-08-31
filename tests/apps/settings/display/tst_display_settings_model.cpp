// SPDX-License-Identifier: GPL-3.0-or-later

#include "tests/services/display_client/support/fake_display_transport.h"
#include <qindaqt/apps/settings_display/display_settings_model.h>
#include <qindaqt/services/display_client/client.h>
#include <qindaqt/services/display_client/display_coordinator.h>

#include <QSignalSpy>
#include <QtTest/QTest>

using namespace QindaQt::Apps::SettingsDisplay;
using namespace QindaQt::DisplayClient;
using namespace QindaQt::DisplayClient::TestSupport;

namespace {

QindaQt::Display::Snapshot createTwoOutputSnapshot(const QString &epoch = QStringLiteral("ep1"),
                                         quint64 revision = 1) {
  QindaQt::Display::Output out1{
      .stableId = QStringLiteral("edid:dp1"),
      .connectorName = QStringLiteral("DP-1"),
      .runtimeCompositorUuid = QStringLiteral("uuid1"),
      .label = QStringLiteral("Main Monitor"),
      .manufacturer = QStringLiteral("Dell"),
      .model = QStringLiteral("U2720Q"),
      .physicalSizeMillimeters = QSize(600, 340),
      .enabled = true,
      .primary = true,
      .modeId = QStringLiteral("3840x2160@60"),
      .position = QPoint(0, 0),
      .logicalSize = QSize(1920, 1080),
      .scale = 2.0,
      .transform = QindaQt::Display::Transform::Normal,
      .priority = 1,
      .replicationSourceStableId = {},
      .modes = {
          {.id = QStringLiteral("3840x2160@60"),
           .pixelSize = QSize(3840, 2160),
           .refreshMilliHertz = 60'000,
           .preferred = true},
          {.id = QStringLiteral("1920x1080@60"),
           .pixelSize = QSize(1920, 1080),
           .refreshMilliHertz = 60'000,
           .preferred = false},
      },
  };

  QindaQt::Display::Output out2{
      .stableId = QStringLiteral("edid:hdmi1"),
      .connectorName = QStringLiteral("HDMI-1"),
      .runtimeCompositorUuid = QStringLiteral("uuid2"),
      .label = QStringLiteral("Side Monitor"),
      .manufacturer = QStringLiteral("LG"),
      .model = QStringLiteral("UltraFine"),
      .physicalSizeMillimeters = QSize(530, 300),
      .enabled = true,
      .primary = false,
      .modeId = QStringLiteral("1920x1080@60"),
      .position = QPoint(3840, 0),
      .logicalSize = QSize(1920, 1080),
      .scale = 1.0,
      .transform = QindaQt::Display::Transform::Normal,
      .priority = 2,
      .replicationSourceStableId = {},
      .modes = {
          {.id = QStringLiteral("1920x1080@60"),
           .pixelSize = QSize(1920, 1080),
           .refreshMilliHertz = 60'000,
           .preferred = true},
      },
  };

  return {
      .protocolVersion = 1,
      .serviceEpoch = epoch,
      .revision = revision,
      .liveFingerprint = QByteArray(32, '\x01'),
      .outputs = {out1, out2},
      .transactions = {},
  };
}

} // namespace

class DisplaySettingsModelTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void testInitialSnapshotProjection();
  void testDraftModeSelection();
  void testDraftScaleAndTransform();
  void testDraftPositionAndPrimary();
  void testCancelDraftRestoresSnapshot();
  void testFullTransactionFlowConfirm();
  void testFullTransactionFlowRevert();
};

void DisplaySettingsModelTest::testInitialSnapshotProjection() {
  FakeDisplayTransport transport;
  Client client(&transport);
  Coordinator coordinator(&client);
  DisplaySettingsModel model(client, coordinator);

  client.start();
  transport.publishOwner(QStringLiteral(":1.50"));
  QCOMPARE(transport.fetches.size(), 1);

  const auto snap = createTwoOutputSnapshot();
  transport.replySnapshot(transport.fetches.first(), snap);

  QVERIFY(model.ready());
  QVERIFY(!model.draftDirty());
  QVERIFY(model.draftValid());
  QCOMPARE(model.outputs().size(), 2);
  QCOMPARE(model.selectedOutputId(), QStringLiteral("edid:dp1"));

  const auto selected = model.selectedOutput();
  QCOMPARE(selected.value(QStringLiteral("connectorName")).toString(),
           QStringLiteral("DP-1"));
  QCOMPARE(selected.value(QStringLiteral("label")).toString(),
           QStringLiteral("Main Monitor"));
  QCOMPARE(selected.value(QStringLiteral("primary")).toBool(), true);
  QCOMPARE(selected.value(QStringLiteral("scale")).toDouble(), 2.0);

  const auto modes = selected.value(QStringLiteral("modes")).toList();
  QCOMPARE(modes.size(), 2);
  QCOMPARE(modes.first().toMap().value(QStringLiteral("id")).toString(),
           QStringLiteral("3840x2160@60"));
}

void DisplaySettingsModelTest::testDraftModeSelection() {
  FakeDisplayTransport transport;
  Client client(&transport);
  Coordinator coordinator(&client);
  DisplaySettingsModel model(client, coordinator);

  client.start();
  transport.publishOwner(QStringLiteral(":1.50"));
  transport.replySnapshot(transport.fetches.first(), createTwoOutputSnapshot());

  QSignalSpy draftSpy(&model, &DisplaySettingsModel::draftChanged);

  // Set valid mode
  QVERIFY(model.setOutputMode(QStringLiteral("edid:dp1"),
                              QStringLiteral("1920x1080@60")));
  QCOMPARE(draftSpy.count(), 1);
  QVERIFY(model.draftDirty());
  QVERIFY(model.draftValid());
  QCOMPARE(model.selectedOutput().value(QStringLiteral("modeId")).toString(),
           QStringLiteral("1920x1080@60"));

  // Try unknown mode -> rejected
  QVERIFY(!model.setOutputMode(QStringLiteral("edid:dp1"),
                               QStringLiteral("8000x4000@120")));
}

void DisplaySettingsModelTest::testDraftScaleAndTransform() {
  FakeDisplayTransport transport;
  Client client(&transport);
  Coordinator coordinator(&client);
  DisplaySettingsModel model(client, coordinator);

  client.start();
  transport.publishOwner(QStringLiteral(":1.50"));
  transport.replySnapshot(transport.fetches.first(), createTwoOutputSnapshot());

  // Change scale
  QVERIFY(model.setOutputScale(QStringLiteral("edid:dp1"), 1.5));
  QVERIFY(model.draftDirty());
  QVERIFY(model.draftValid());
  QCOMPARE(model.selectedOutput().value(QStringLiteral("scale")).toDouble(), 1.5);

  // Change transform
  QVERIFY(model.setOutputTransform(QStringLiteral("edid:dp1"),
                                   QStringLiteral("90")));
  QVERIFY(model.draftDirty());
  QVERIFY(model.draftValid());
  QCOMPARE(model.selectedOutput().value(QStringLiteral("transform")).toString(),
           QStringLiteral("90"));
}

void DisplaySettingsModelTest::testDraftPositionAndPrimary() {
  FakeDisplayTransport transport;
  Client client(&transport);
  Coordinator coordinator(&client);
  DisplaySettingsModel model(client, coordinator);

  client.start();
  transport.publishOwner(QStringLiteral(":1.50"));
  transport.replySnapshot(transport.fetches.first(), createTwoOutputSnapshot());

  // Switch primary to hdmi1
  QVERIFY(model.setOutputPrimary(QStringLiteral("edid:hdmi1")));
  QVERIFY(model.draftDirty());
  QVERIFY(model.draftValid());

  model.setSelectedOutputId(QStringLiteral("edid:hdmi1"));
  QCOMPARE(model.selectedOutput().value(QStringLiteral("primary")).toBool(),
           true);

  model.setSelectedOutputId(QStringLiteral("edid:dp1"));
  QCOMPARE(model.selectedOutput().value(QStringLiteral("primary")).toBool(),
           false);

  // Move DP-1 position
  QVERIFY(model.setOutputPosition(QStringLiteral("edid:dp1"), 0, 1080));
  QCOMPARE(model.selectedOutput().value(QStringLiteral("positionY")).toInt(),
           1080);
}

void DisplaySettingsModelTest::testCancelDraftRestoresSnapshot() {
  FakeDisplayTransport transport;
  Client client(&transport);
  Coordinator coordinator(&client);
  DisplaySettingsModel model(client, coordinator);

  client.start();
  transport.publishOwner(QStringLiteral(":1.50"));
  transport.replySnapshot(transport.fetches.first(), createTwoOutputSnapshot());

  QVERIFY(model.setOutputScale(QStringLiteral("edid:dp1"), 1.25));
  QVERIFY(model.draftDirty());

  QVERIFY(model.cancelDraft());
  QVERIFY(!model.draftDirty());
  QCOMPARE(model.selectedOutput().value(QStringLiteral("scale")).toDouble(), 2.0);
}

void DisplaySettingsModelTest::testFullTransactionFlowConfirm() {
  FakeDisplayTransport transport;
  Client client(&transport);
  Coordinator coordinator(&client);
  DisplaySettingsModel model(client, coordinator);

  client.start();
  transport.publishOwner(QStringLiteral(":1.50"));
  transport.replySnapshot(transport.fetches.first(), createTwoOutputSnapshot());

  QVERIFY(model.setOutputScale(QStringLiteral("edid:dp1"), 1.0));
  QVERIFY(model.applyAvailable());

  QVERIFY(model.applyDraft());
  QVERIFY(model.inTransaction());
  QCOMPARE(transport.operations.size(), 1);
  QCOMPARE(transport.operations.last().kind, QindaQt::Display::OperationKind::Stage);

  const QString txId = transport.operations.last().transactionId;

  // Server replies to Stage
  transport.replyOperation(
      transport.operations.last(),
      operationResult(QindaQt::Display::OperationKind::Stage,
                      QindaQt::Display::OperationStatus::Accepted,
                      QStringLiteral("ep1"), 1, txId));
  QCoreApplication::processEvents();

  // Coordinator moves to Preview
  QCOMPARE(transport.operations.size(), 2);
  QCOMPARE(transport.operations.last().kind, QindaQt::Display::OperationKind::Preview);

  // Server replies to Preview
  transport.replyOperation(
      transport.operations.last(),
      operationResult(QindaQt::Display::OperationKind::Preview,
                      QindaQt::Display::OperationStatus::Accepted,
                      QStringLiteral("ep1"), 1, txId));
  QCoreApplication::processEvents();

  // Server publishes snapshot with transaction summary awaiting confirmation
  auto previewSnap = createTwoOutputSnapshot(QStringLiteral("ep1"), 1);
  previewSnap.transactions = {{
      .transactionId = txId,
      .state = QindaQt::Display::TransactionState::AwaitingConfirmation,
      .reason = QindaQt::Display::TransactionReason::None,
      .initiatingEpoch = QStringLiteral("ep1"),
      .baseRevision = 1,
      .observedRevision = 1,
      .deadlineMonotonicMilliseconds = 50'000,
      .revertAttempt = 0,
  }};
  transport.publishInvalidation(QStringLiteral(":1.50"), QStringLiteral("ep1"), 1);
  transport.replySnapshot(transport.fetches.last(), previewSnap);
  QCoreApplication::processEvents();

  // Coordinator is now AwaitingConfirmation
  QVERIFY(model.awaitingConfirmation());
  QVERIFY(model.transactionRemainingSeconds() > 0);

  // User confirms
  QVERIFY(model.confirmTransaction());
  QCOMPARE(transport.operations.size(), 3);
  QCOMPARE(transport.operations.last().kind, QindaQt::Display::OperationKind::Confirm);

  // Server replies to Confirm
  transport.replyOperation(
      transport.operations.last(),
      operationResult(QindaQt::Display::OperationKind::Confirm,
                      QindaQt::Display::OperationStatus::Accepted,
                      QStringLiteral("ep1"), 2, txId));
  QCoreApplication::processEvents();

  // Snapshot updated with confirmed revision
  auto updatedSnap = createTwoOutputSnapshot(QStringLiteral("ep1"), 2);
  updatedSnap.outputs[0].scale = 1.0;
  transport.publishInvalidation(QStringLiteral(":1.50"), QStringLiteral("ep1"),
                               2);
  transport.replySnapshot(transport.fetches.last(), updatedSnap);
  QCoreApplication::processEvents();

  QVERIFY(!model.inTransaction());
  QVERIFY(!model.draftDirty());
  QCOMPARE(model.selectedOutput().value(QStringLiteral("scale")).toDouble(), 1.0);
}

void DisplaySettingsModelTest::testFullTransactionFlowRevert() {
  FakeDisplayTransport transport;
  Client client(&transport);
  Coordinator coordinator(&client);
  DisplaySettingsModel model(client, coordinator);

  client.start();
  transport.publishOwner(QStringLiteral(":1.50"));
  transport.replySnapshot(transport.fetches.first(), createTwoOutputSnapshot());

  QVERIFY(model.setOutputScale(QStringLiteral("edid:dp1"), 1.0));
  QVERIFY(model.applyDraft());

  const QString txId = transport.operations.last().transactionId;

  // Stage accepted
  transport.replyOperation(
      transport.operations.last(),
      operationResult(QindaQt::Display::OperationKind::Stage,
                      QindaQt::Display::OperationStatus::Accepted,
                      QStringLiteral("ep1"), 1, txId));
  QCoreApplication::processEvents();

  // Preview accepted
  transport.replyOperation(
      transport.operations.last(),
      operationResult(QindaQt::Display::OperationKind::Preview,
                      QindaQt::Display::OperationStatus::Accepted,
                      QStringLiteral("ep1"), 1, txId));
  QCoreApplication::processEvents();

  // Server publishes snapshot with transaction summary awaiting confirmation
  auto previewSnap = createTwoOutputSnapshot(QStringLiteral("ep1"), 1);
  previewSnap.transactions = {{
      .transactionId = txId,
      .state = QindaQt::Display::TransactionState::AwaitingConfirmation,
      .reason = QindaQt::Display::TransactionReason::None,
      .initiatingEpoch = QStringLiteral("ep1"),
      .baseRevision = 1,
      .observedRevision = 1,
      .deadlineMonotonicMilliseconds = 50'000,
      .revertAttempt = 0,
  }};
  transport.publishInvalidation(QStringLiteral(":1.50"), QStringLiteral("ep1"), 1);
  transport.replySnapshot(transport.fetches.last(), previewSnap);
  QCoreApplication::processEvents();

  QVERIFY(model.awaitingConfirmation());

  // User reverts / cancels
  QVERIFY(model.revertTransaction());
  QCOMPARE(transport.operations.last().kind, QindaQt::Display::OperationKind::Cancel);

  transport.replyOperation(
      transport.operations.last(),
      operationResult(QindaQt::Display::OperationKind::Cancel,
                      QindaQt::Display::OperationStatus::Accepted,
                      QStringLiteral("ep1"), 1, txId));
  QCoreApplication::processEvents();

  auto revertSnap = createTwoOutputSnapshot(QStringLiteral("ep1"), 1);
  revertSnap.transactions = {};
  transport.publishInvalidation(QStringLiteral(":1.50"), QStringLiteral("ep1"), 1);
  transport.replySnapshot(transport.fetches.last(), revertSnap);
  QCoreApplication::processEvents();

  QVERIFY(!model.inTransaction());
  QVERIFY(!model.draftDirty());
  QCOMPARE(model.selectedOutput().value(QStringLiteral("scale")).toDouble(), 2.0);
}

QTEST_MAIN(DisplaySettingsModelTest)
#include "tst_display_settings_model.moc"
