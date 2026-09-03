// SPDX-License-Identifier: GPL-3.0-or-later

#include "color_settings_test_support.h"
#include "tests/services/display_client/support/fake_display_transport.h"

#include <qindaqt/apps/settings_color/color_settings_model.h>

#include <QtCore/QTemporaryDir>
#include <QtTest>

using namespace QindaQt;
using namespace QindaQt::Apps::SettingsColor;
using namespace QindaQt::Apps::SettingsColor::TestSupport;
using QindaQt::DisplayClient::TestSupport::FakeDisplayTransport;
using QindaQt::Services::SettingsClient::ClientState;
using QindaQt::Services::SettingsClient::SettingsClient;
using QindaQt::Services::SettingsProtocol::SettingsWireStatus;

namespace {

Display::Snapshot applyFixtureSnapshot(const QString &epoch = QStringLiteral("dsep"),
                                       quint64 revision = 3) {
  Display::Output first;
  first.stableId = QStringLiteral("edid:dp1");
  first.connectorName = QStringLiteral("DP-1");
  first.label = QStringLiteral("Main Monitor");
  first.physicalSizeMillimeters = QSize(600, 340);
  first.enabled = true;
  first.primary = true;
  first.modeId = QStringLiteral("3840x2160@60");
  first.logicalSize = QSize(1920, 1080);
  first.modes = {{.id = QStringLiteral("3840x2160@60"),
                  .pixelSize = QSize(3840, 2160),
                  .refreshMilliHertz = 60'000,
                  .preferred = true}};
  return {.protocolVersion = 1,
          .serviceEpoch = epoch,
          .revision = revision,
          .liveFingerprint = QByteArray(32, '\x01'),
          .outputs = {first},
          .transactions = {}};
}

struct ApplyFixture {
  ApplyFixture() {
    discovery = std::make_unique<DisplayColor::ProfileDiscovery>(
        QList<DisplayColor::DiscoveryRoot>{
            {systemRoot.path(), DisplayColor::DiscoveryOrigin::System},
            {userRoot.path(), DisplayColor::DiscoveryOrigin::UserImported}});
    model = std::make_unique<ColorSettingsModel>(displayClient, settingsClient,
                                                 store, *discovery);
  }

  void bringReady(const QVariant &assignments = QVariantMap{}) {
    writeFixtureProfile(QDir(systemRoot.path()),
                        QStringLiteral("vendor-srgb.icc"));
    displayClient.start();
    displayTransport.publishOwner(QStringLiteral(":1.70"));
    displayTransport.replySnapshot(displayTransport.fetches.constLast(),
                                   applyFixtureSnapshot());
    QVERIFY(settingsClient.start());
    Q_EMIT settingsTransport.ownerChanged(QStringLiteral(":1.42"));
    QTRY_VERIFY(!settingsTransport.snapshots.isEmpty());
    const auto request = settingsTransport.snapshots.takeLast();
    Q_EMIT settingsTransport.snapshotReceived(
        request.token, request.owner,
        snapshotWire(QStringLiteral("epoch-a"), 4, assignments));
    QTRY_VERIFY(settingsClient.state() == ClientState::Ready);
    model->setRouteActive(true);
    QVERIFY(model->ready());
    QVERIFY(model->selectOutput(QStringLiteral("edid:dp1")));
  }

  void finishCommit(SettingsWireStatus status, quint64 before, quint64 after,
                    const QVariant &authoritative, const QStringList &changed) {
    Q_EMIT settingsTransport.commitReceived(
        settingsTransport.commits.constLast().token,
        settingsTransport.commits.constLast().owner,
        commitWire(status, before, after, authoritative, changed));
  }

  void refreshDocument(quint64 revision, const QVariant &assignments) {
    QTRY_VERIFY(!settingsTransport.snapshots.isEmpty());
    const auto refresh = settingsTransport.snapshots.takeLast();
    Q_EMIT settingsTransport.snapshotReceived(
        refresh.token, refresh.owner,
        snapshotWire(QStringLiteral("epoch-a"), revision, assignments));
    QTRY_VERIFY(model->settingsRevision() == revision);
  }

  FakeDisplayTransport displayTransport;
  DisplayClient::Client displayClient{&displayTransport};
  FakeSettingsTransport settingsTransport;
  SettingsClient settingsClient{
      settingsTransport, {QLatin1String(kAssignmentsKey)}, colorTestTiming()};
  DisplayColor::SettingsAssignmentStore store{settingsClient};
  QTemporaryDir userRoot;
  QTemporaryDir systemRoot;
  QTemporaryDir sourceRoot;
  std::unique_ptr<DisplayColor::ProfileDiscovery> discovery;
  std::unique_ptr<ColorSettingsModel> model;
};

} // namespace

class ColorSettingsApplyTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void assignSendsFencedDraftAndConverges();
  void alreadyAssignedIsRefusedBeforeAnyWrite();
  void unassignRemovesTheRecord();
  void conflictIsSurfacedAndNeverReplayed();
  void uncertainTimeoutIsTerminalWithoutReplay();
  void inFlightWriteFencesFurtherActions();
  void ownerReplacementDuringApplyIsUncertain();
  void importRefreshesTheCatalog();
  void importRejectsHostileInputs();
};

void ColorSettingsApplyTest::assignSendsFencedDraftAndConverges() {
  ApplyFixture fixture;
  fixture.bringReady();

  QVERIFY(fixture.model->assignProfile(QStringLiteral("vendor-srgb")));
  QVERIFY(fixture.model->busy());
  QCOMPARE(fixture.settingsTransport.commits.size(), 1);
  const auto commit = fixture.settingsTransport.commits.constLast();
  QCOMPARE(commit.epoch, QStringLiteral("epoch-a"));
  QCOMPARE(commit.revision, quint64{4});
  // Displayed availability equals admission: while the write is fenced the
  // profile row closes.
  const QVariantList fencedRows = fixture.model->profileRows();
  QVERIFY(!fencedRows.first().toMap().value(QStringLiteral("available")).toBool());

  const QVariant merged = assignmentValue(QStringLiteral("edid:dp1"),
                                          QStringLiteral("vendor-srgb"),
                                          QString{});
  fixture.finishCommit(SettingsWireStatus::Applied, 4, 5, merged,
                       {QLatin1String(kAssignmentsKey)});
  QTRY_VERIFY(!fixture.model->busy());
  QVERIFY(fixture.model->errorText().isEmpty());
  QVERIFY(fixture.model->operationStatusText().contains(
      QStringLiteral("saved")));
  fixture.refreshDocument(5, merged);
  const QVariantList outputs = fixture.model->outputRows();
  QCOMPARE(outputs.first().toMap().value(QStringLiteral("assigned")).toBool(),
           true);
}

void ColorSettingsApplyTest::alreadyAssignedIsRefusedBeforeAnyWrite() {
  ApplyFixture fixture;
  fixture.bringReady(assignmentValue(QStringLiteral("edid:dp1"),
                                     QStringLiteral("vendor-srgb"),
                                     QString{}));
  const QVariantList rows = fixture.model->profileRows();
  QVERIFY(!rows.first().toMap().value(QStringLiteral("available")).toBool());
  QVERIFY(!fixture.model->assignProfile(QStringLiteral("vendor-srgb")));
  QVERIFY(fixture.model->errorText().contains(
      QStringLiteral("already assigned")));
  QVERIFY(fixture.settingsTransport.commits.isEmpty());
}

void ColorSettingsApplyTest::unassignRemovesTheRecord() {
  ApplyFixture fixture;
  fixture.bringReady(assignmentValue(QStringLiteral("edid:dp1"),
                                     QStringLiteral("vendor-srgb"),
                                     QString{}));
  QVERIFY(fixture.model->unassignAvailable());
  QVERIFY(fixture.model->unassignSelected());
  QCOMPARE(fixture.settingsTransport.commits.size(), 1);

  const QVariant emptyDocument = QVariantMap{};
  fixture.finishCommit(SettingsWireStatus::Applied, 4, 5, emptyDocument,
                       {QLatin1String(kAssignmentsKey)});
  QTRY_VERIFY(!fixture.model->busy());
  fixture.refreshDocument(5, emptyDocument);
  QCOMPARE(fixture.model->outputRows()
               .first()
               .toMap()
               .value(QStringLiteral("assigned"))
               .toBool(),
           false);
  QVERIFY(!fixture.model->unassignAvailable());
}

void ColorSettingsApplyTest::conflictIsSurfacedAndNeverReplayed() {
  ApplyFixture fixture;
  fixture.bringReady();
  QVERIFY(fixture.model->assignProfile(QStringLiteral("vendor-srgb")));
  QCOMPARE(fixture.settingsTransport.commits.size(), 1);

  fixture.finishCommit(SettingsWireStatus::Conflict, 5, 5, QVariantMap{}, {});
  QTRY_VERIFY(!fixture.model->busy());
  QVERIFY(fixture.model->errorText().contains(QStringLiteral("changed elsewhere")));
  QVERIFY(fixture.model->errorText().contains(QStringLiteral("not replayed")));
  QTest::qWait(80);
  QCOMPARE(fixture.settingsTransport.commits.size(), 1);
}

void ColorSettingsApplyTest::uncertainTimeoutIsTerminalWithoutReplay() {
  ApplyFixture fixture;
  fixture.bringReady();
  QVERIFY(fixture.model->assignProfile(QStringLiteral("vendor-srgb")));
  QCOMPARE(fixture.settingsTransport.commits.size(), 1);

  // The reply never arrives; the client times the write out as uncertain.
  QTRY_VERIFY(!fixture.model->busy());
  QVERIFY(fixture.model->errorText().contains(QStringLiteral("not replayed")));
  QTest::qWait(150);
  QCOMPARE(fixture.settingsTransport.commits.size(), 1);

  // After resync to fresh authority an explicit new apply works.
  fixture.refreshDocument(4, QVariantMap{});
  QVERIFY(fixture.model->assignProfile(QStringLiteral("vendor-srgb")));
  QCOMPARE(fixture.settingsTransport.commits.size(), 2);
}

void ColorSettingsApplyTest::inFlightWriteFencesFurtherActions() {
  ApplyFixture fixture;
  fixture.bringReady();
  QVERIFY(fixture.model->assignProfile(QStringLiteral("vendor-srgb")));
  QVERIFY(!fixture.model->assignProfile(QStringLiteral("vendor-srgb")));
  QVERIFY(!fixture.model->unassignSelected());
  QVERIFY(!fixture.model->retry());
  QCOMPARE(fixture.settingsTransport.commits.size(), 1);
}

void ColorSettingsApplyTest::ownerReplacementDuringApplyIsUncertain() {
  ApplyFixture fixture;
  fixture.bringReady();
  QVERIFY(fixture.model->assignProfile(QStringLiteral("vendor-srgb")));
  QCOMPARE(fixture.settingsTransport.commits.size(), 1);

  Q_EMIT fixture.settingsTransport.ownerChanged(QString());
  QTRY_VERIFY(!fixture.model->busy());
  QVERIFY(fixture.model->errorText().contains(QStringLiteral("not replayed")));
  QCOMPARE(fixture.settingsTransport.commits.size(), 1);
}

void ColorSettingsApplyTest::importRefreshesTheCatalog() {
  ApplyFixture fixture;
  fixture.bringReady();
  QCOMPARE(fixture.model->profileRows().size(), 1);

  const QString source = writeFixtureProfile(QDir(fixture.sourceRoot.path()),
                                             QStringLiteral("custom.icc"));
  QVERIFY(!source.isEmpty());
  QVERIFY(fixture.model->importProfile(QUrl::fromLocalFile(source)));
  QVERIFY(fixture.model->importStatusText().contains(QStringLiteral("Imported")));
  const QVariantList rows = fixture.model->profileRows();
  QCOMPARE(rows.size(), 2);
  bool found = false;
  bool assignedOrigin = false;
  for (const QVariant &row : rows) {
    const QVariantMap map = row.toMap();
    if (map.value(QStringLiteral("id")).toString() == QStringLiteral("custom")) {
      found = true;
      assignedOrigin = map.value(QStringLiteral("originText")).toString()
          == QStringLiteral("Imported profile");
    }
  }
  QVERIFY(found);
  QVERIFY(assignedOrigin);

  // The imported profile carries its SHA-256 lineage into the draft record.
  QVERIFY(fixture.model->assignProfile(QStringLiteral("custom")));
  QCOMPARE(fixture.settingsTransport.commits.size(), 1);
  const auto operations = fixture.settingsTransport.commits.constLast().operations;
  const QVariant value = operations.first().toMap()
                             .value(QLatin1StringView(WC::FieldValue));
  const QString lineage = value.toMap()
                              .value(QStringLiteral("edid:dp1"))
                              .toMap()
                              .value(QStringLiteral("lineage"))
                              .toString();
  QCOMPARE(lineage.size(), 64);

  // A byte-identical re-import is an idempotent AlreadyPresent.
  QVERIFY(fixture.model->importProfile(QUrl::fromLocalFile(source)));
  QVERIFY(fixture.model->importStatusText().contains(
      QStringLiteral("already imported")));
}

void ColorSettingsApplyTest::importRejectsHostileInputs() {
  ApplyFixture fixture;
  fixture.bringReady();

  // Remote URLs never reach the import seam.
  QVERIFY(!fixture.model->importProfile(
      QUrl(QStringLiteral("https://example.invalid/p.icc"))));
  QVERIFY(fixture.model->importStatusText().contains(
      QStringLiteral("local profile files")));

  // A garbage file is refused by C1 validation before any mutation.
  const QString garbage = writeFixtureProfile(
      QDir(fixture.sourceRoot.path()), QStringLiteral("garbage.icc"),
      QByteArray(256, '\x07'));
  QVERIFY(!garbage.isEmpty());
  QVERIFY(!fixture.model->importProfile(QUrl::fromLocalFile(garbage)));
  QVERIFY(fixture.model->importStatusText().contains(
      QStringLiteral("not a valid ICC profile")));
  QCOMPARE(fixture.model->profileRows().size(), 1);
  QVERIFY(fixture.settingsTransport.commits.isEmpty());
}

QTEST_GUILESS_MAIN(ColorSettingsApplyTest)
#include "tst_color_settings_apply.moc"
