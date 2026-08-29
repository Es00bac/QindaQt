// SPDX-License-Identifier: GPL-3.0-or-later

#include "tests/services/display_client/support/fake_display_transport.h"
#include <qindaqt/apps/settings_display/display_settings_model.h>
#include <qindaqt/services/display_client/client.h>
#include <qindaqt/services/display_client/display_coordinator.h>

#include <QtTest/QTest>

using namespace QindaQt::Apps::SettingsDisplay;
using namespace QindaQt::DisplayClient;
using namespace QindaQt::DisplayClient::TestSupport;

namespace {

QindaQt::Display::Snapshot createAdversarialSnapshot(const QString &epoch = QStringLiteral("ep1"),
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
      .modeId = QStringLiteral("1920x1080@60"),
      .position = QPoint(0, 0),
      .logicalSize = QSize(1920, 1080),
      .scale = 1.0,
      .transform = QindaQt::Display::Transform::Normal,
      .priority = 1,
      .replicationSourceStableId = {},
      .modes = {
          {.id = QStringLiteral("1920x1080@60"),
           .pixelSize = QSize(1920, 1080),
           .refreshMilliHertz = 60'000,
           .preferred = true},
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
      .position = QPoint(1920, 0),
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

class DisplaySettingsModelAdversarialTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void testAllOutputsDisabledValidation();
  void testOverlappingOutputsValidation();
  void testStaleLineageRevalidation();
  void testServiceCrashAndRecovery();
  void testCoordinatorStageRejected();
};

void DisplaySettingsModelAdversarialTest::testAllOutputsDisabledValidation() {
  FakeDisplayTransport transport;
  Client client(&transport);
  Coordinator coordinator(&client);
  DisplaySettingsModel model(client, coordinator);

  client.start();
  transport.publishOwner(QStringLiteral(":1.50"));
  transport.replySnapshot(transport.fetches.first(), createAdversarialSnapshot());

  // Disable both outputs
  model.setOutputEnabled(QStringLiteral("edid:dp1"), false);
  model.setOutputEnabled(QStringLiteral("edid:hdmi1"), false);

  QVERIFY(!model.draftValid());
  QVERIFY(!model.applyAvailable());
  QVERIFY(!model.draftErrorMessage().isEmpty());
}

void DisplaySettingsModelAdversarialTest::testOverlappingOutputsValidation() {
  FakeDisplayTransport transport;
  Client client(&transport);
  Coordinator coordinator(&client);
  DisplaySettingsModel model(client, coordinator);

  client.start();
  transport.publishOwner(QStringLiteral(":1.50"));
  transport.replySnapshot(transport.fetches.first(), createAdversarialSnapshot());

  // Set HDMI-1 to overlap DP-1 at (0, 0)
  model.setOutputPosition(QStringLiteral("edid:hdmi1"), 0, 0);

  QVERIFY(!model.draftValid());
  QVERIFY(!model.applyAvailable());
  QVERIFY(!model.draftErrorMessage().isEmpty());
}

void DisplaySettingsModelAdversarialTest::testStaleLineageRevalidation() {
  FakeDisplayTransport transport;
  Client client(&transport);
  Coordinator coordinator(&client);
  DisplaySettingsModel model(client, coordinator);

  client.start();
  transport.publishOwner(QStringLiteral(":1.50"));
  transport.replySnapshot(transport.fetches.first(), createAdversarialSnapshot(QStringLiteral("ep1"), 1));

  // Set scale
  model.setOutputScale(QStringLiteral("edid:dp1"), 1.5);
  QVERIFY(model.draftDirty());
  QVERIFY(model.draftValid());

  // External snapshot change (revision 2)
  transport.publishInvalidation(QStringLiteral(":1.50"), QStringLiteral("ep1"), 2);
  transport.replySnapshot(transport.fetches.last(), createAdversarialSnapshot(QStringLiteral("ep1"), 2));

  // Draft should be revalidated against revision 2
  QVERIFY(model.draftValid());
  QVERIFY(model.applyAvailable());
}

void DisplaySettingsModelAdversarialTest::testServiceCrashAndRecovery() {
  FakeDisplayTransport transport;
  Client client(&transport);
  Coordinator coordinator(&client);
  DisplaySettingsModel model(client, coordinator);

  client.start();
  transport.publishOwner(QStringLiteral(":1.50"));
  transport.replySnapshot(transport.fetches.first(), createAdversarialSnapshot());

  QVERIFY(model.ready());

  // Service drops owner
  transport.publishOwner(QString{});

  QVERIFY(model.unavailable());
  QCOMPARE(model.outputs().size(), 0);
  QVERIFY(!model.canEdit());

  // Service returns
  transport.publishOwner(QStringLiteral(":1.51"));
  QCOMPARE(transport.fetches.size(), 2);
  transport.replySnapshot(transport.fetches.last(), createAdversarialSnapshot(QStringLiteral("ep2"), 1));

  QVERIFY(model.ready());
  QCOMPARE(model.outputs().size(), 2);
  QVERIFY(model.canEdit());
}

void DisplaySettingsModelAdversarialTest::testCoordinatorStageRejected() {
  FakeDisplayTransport transport;
  Client client(&transport);
  Coordinator coordinator(&client);
  DisplaySettingsModel model(client, coordinator);

  client.start();
  transport.publishOwner(QStringLiteral(":1.50"));
  transport.replySnapshot(transport.fetches.first(), createAdversarialSnapshot());

  model.setOutputScale(QStringLiteral("edid:dp1"), 1.25);
  QVERIFY(model.applyDraft());

  const QString txId = transport.operations.last().transactionId;

  // Server rejects Stage
  transport.replyOperation(
      transport.operations.last(),
      operationResult(QindaQt::Display::OperationKind::Stage,
                      QindaQt::Display::OperationStatus::Rejected,
                      QStringLiteral("ep1"), 1, txId,
                      QStringLiteral("Hardware rejected requested mode")));
  QCoreApplication::processEvents();

  QVERIFY(!model.inTransaction());
  QVERIFY(!model.errorText().isEmpty() || !model.statusText().isEmpty());
}

QTEST_MAIN(DisplaySettingsModelAdversarialTest)
#include "tst_display_settings_model_adversarial.moc"
