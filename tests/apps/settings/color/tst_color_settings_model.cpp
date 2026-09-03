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

namespace {

Display::Snapshot twoOutputSnapshot(const QString &epoch = QStringLiteral("dsep"),
                                    quint64 revision = 3) {
  Display::Output first;
  first.stableId = QStringLiteral("edid:dp1");
  first.connectorName = QStringLiteral("DP-1");
  first.label = QStringLiteral("Main Monitor");
  first.manufacturer = QStringLiteral("Dell");
  first.model = QStringLiteral("U2720Q");
  first.physicalSizeMillimeters = QSize(600, 340);
  first.enabled = true;
  first.primary = true;
  first.modeId = QStringLiteral("3840x2160@60");
  first.logicalSize = QSize(1920, 1080);
  first.modes = {{.id = QStringLiteral("3840x2160@60"),
                  .pixelSize = QSize(3840, 2160),
                  .refreshMilliHertz = 60'000,
                  .preferred = true}};
  Display::Output second;
  second.stableId = QStringLiteral("edid:hdmi1");
  second.connectorName = QStringLiteral("HDMI-1");
  second.manufacturer = QStringLiteral("LG");
  second.model = QStringLiteral("UltraFine");
  second.physicalSizeMillimeters = QSize(530, 300);
  second.enabled = true;
  second.modeId = QStringLiteral("1920x1080@60");
  second.logicalSize = QSize(1920, 1080);
  second.modes = {{.id = QStringLiteral("1920x1080@60"),
                   .pixelSize = QSize(1920, 1080),
                   .refreshMilliHertz = 60'000,
                   .preferred = true}};
  return {.protocolVersion = 1,
          .serviceEpoch = epoch,
          .revision = revision,
          .liveFingerprint = QByteArray(32, '\x01'),
          .outputs = {first, second},
          .transactions = {}};
}

struct RouteFixture {
  RouteFixture() {
    QVERIFY(userRoot.isValid());
    QVERIFY(systemRoot.isValid());
    writeFixtureProfile(QDir(systemRoot.path()),
                        QStringLiteral("vendor-srgb.icc"));
    discovery = std::make_unique<DisplayColor::ProfileDiscovery>(
        QList<DisplayColor::DiscoveryRoot>{
            {systemRoot.path(), DisplayColor::DiscoveryOrigin::System},
            {userRoot.path(), DisplayColor::DiscoveryOrigin::UserImported}});
    model = std::make_unique<ColorSettingsModel>(displayClient, settingsClient,
                                                 store, *discovery);
  }

  void bringReady(const QVariant &assignments = QVariantMap{}) {
    displayClient.start();
    displayTransport.publishOwner(QStringLiteral(":1.70"));
    QCOMPARE(displayTransport.fetches.size(), 1);
    displayTransport.replySnapshot(displayTransport.fetches.constLast(),
                                   twoOutputSnapshot());
    QVERIFY(settingsClient.start());
    Q_EMIT settingsTransport.ownerChanged(QStringLiteral(":1.42"));
    QTRY_VERIFY(!settingsTransport.snapshots.isEmpty());
    const auto request = settingsTransport.snapshots.takeLast();
    Q_EMIT settingsTransport.snapshotReceived(
        request.token, request.owner,
        snapshotWire(QStringLiteral("epoch-a"), 4, assignments));
    QTRY_VERIFY(settingsClient.state() == ClientState::Ready);
    model->setRouteActive(true);
  }

  FakeDisplayTransport displayTransport;
  DisplayClient::Client displayClient{&displayTransport};
  FakeSettingsTransport settingsTransport;
  SettingsClient settingsClient{
      settingsTransport, {QLatin1String(kAssignmentsKey)}, colorTestTiming()};
  DisplayColor::SettingsAssignmentStore store{settingsClient};
  QTemporaryDir userRoot;
  QTemporaryDir systemRoot;
  std::unique_ptr<DisplayColor::ProfileDiscovery> discovery;
  std::unique_ptr<ColorSettingsModel> model;
};

} // namespace

class ColorSettingsModelTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void projectsInventoryAssignmentsAndCatalog();
  void unavailableBeforeAuthority();
  void unusableDocumentIsDegradedAndClosed();
  void staleRetainedDocumentClosesAdmission();
  void selectionFencingAndInventoryChanges();
  void disconnectedRecordsStayVisibleReadOnly();
  void hasNoCompositorApplicationAuthority();
};

void ColorSettingsModelTest::projectsInventoryAssignmentsAndCatalog() {
  RouteFixture fixture;
  fixture.bringReady(assignmentValue(QStringLiteral("edid:dp1"),
                                     QStringLiteral("vendor-srgb"),
                                     QString{}));

  QVERIFY(fixture.model->ready());
  QVERIFY(fixture.model->catalogScanned());
  QCOMPARE(fixture.model->displayEpoch(), QStringLiteral("dsep"));
  QCOMPARE(fixture.model->displayRevision(), qulonglong(3));
  QCOMPARE(fixture.model->settingsRevision(), qulonglong(4));

  const QVariantList outputs = fixture.model->outputRows();
  QCOMPARE(outputs.size(), 2);
  const QVariantMap first = outputs.at(0).toMap();
  QCOMPARE(first.value(QStringLiteral("id")).toString(),
           QStringLiteral("edid:dp1"));
  QCOMPARE(first.value(QStringLiteral("name")).toString(),
           QStringLiteral("Main Monitor"));
  QCOMPARE(first.value(QStringLiteral("assigned")).toBool(), true);
  QCOMPARE(first.value(QStringLiteral("assignmentText")).toString(),
           QStringLiteral("vendor-srgb"));
  const QVariantMap second = outputs.at(1).toMap();
  QCOMPARE(second.value(QStringLiteral("name")).toString(),
           QStringLiteral("LG UltraFine"));
  QCOMPARE(second.value(QStringLiteral("assigned")).toBool(), false);

  QVERIFY(fixture.model->selectOutput(QStringLiteral("edid:hdmi1")));
  QCOMPARE(fixture.model->selectedOutputId(), QStringLiteral("edid:hdmi1"));
  QCOMPARE(fixture.model->selectedOutputName(),
           QStringLiteral("LG UltraFine"));
  const QVariantList profiles = fixture.model->profileRows();
  QCOMPARE(profiles.size(), 1);
  const QVariantMap profile = profiles.first().toMap();
  QCOMPARE(profile.value(QStringLiteral("id")).toString(),
           QStringLiteral("vendor-srgb"));
  QCOMPARE(profile.value(QStringLiteral("originText")).toString(),
           QStringLiteral("System profile"));
  QVERIFY(profile.value(QStringLiteral("available")).toBool());
  QVERIFY(!fixture.model->unassignAvailable());

  QVERIFY(fixture.model->catalogSummaryText().contains(
      QStringLiteral("1 color profiles discovered")));
}

void ColorSettingsModelTest::unavailableBeforeAuthority() {
  RouteFixture fixture;
  // No client has any truth: the page is unavailable, not connecting.
  QVERIFY(!fixture.model->loading());
  QVERIFY(fixture.model->unavailable());
  QVERIFY(fixture.model->statusText().contains(QStringLiteral("unavailable")));
  QVERIFY(fixture.model->outputRows().isEmpty());
  QVERIFY(!fixture.model->assignProfile(QStringLiteral("vendor-srgb")));
  QVERIFY(!fixture.model->unassignSelected());
  QVERIFY(!fixture.model->selectOutput(QStringLiteral("edid:dp1")));
  QVERIFY(fixture.settingsTransport.commits.isEmpty());
  QVERIFY(fixture.model->retryAvailable());

  // A started display client without an owner keeps the page connecting.
  fixture.displayClient.start();
  QVERIFY(fixture.model->loading());
  QVERIFY(fixture.model->statusText().contains(QStringLiteral("Connecting")));
}

void ColorSettingsModelTest::unusableDocumentIsDegradedAndClosed() {
  RouteFixture fixture;
  const QVariant hostile = QVariantMap{
      {QStringLiteral("edid:dp1"),
       QVariantMap{{QStringLiteral("profile"), QStringLiteral("p")},
                   {QStringLiteral("lineage"), QStringLiteral("zz")},
                   {QStringLiteral("extra"), true}}}};
  fixture.bringReady(hostile);

  QVERIFY(fixture.model->degraded());
  QVERIFY(!fixture.model->ready());
  QVERIFY(fixture.model->statusText().contains(
      QStringLiteral("cannot be read safely")));
  QCOMPARE(fixture.model->outputRows().size(), 2);
  QVERIFY(fixture.model->selectOutput(QStringLiteral("edid:dp1")));
  const QVariantList profiles = fixture.model->profileRows();
  QCOMPARE(profiles.size(), 1);
  QVERIFY(!profiles.first().toMap().value(QStringLiteral("available")).toBool());
  QVERIFY(!fixture.model->assignProfile(QStringLiteral("vendor-srgb")));
  QVERIFY(fixture.model->errorText().contains(
      QStringLiteral("cannot be read safely")));
  QVERIFY(fixture.settingsTransport.commits.isEmpty());
}

void ColorSettingsModelTest::staleRetainedDocumentClosesAdmission() {
  RouteFixture fixture;
  fixture.bringReady(assignmentValue(QStringLiteral("edid:dp1"),
                                     QStringLiteral("vendor-srgb"),
                                     QString{}));
  QVERIFY(fixture.model->ready());

  Q_EMIT fixture.settingsTransport.ownerChanged(QString());
  QTRY_VERIFY(fixture.settingsClient.state() == ClientState::Unavailable);
  QVERIFY(fixture.model->stale());
  QVERIFY(fixture.model->statusText().contains(
      QStringLiteral("stale")));
  QVERIFY(!fixture.model->assignProfile(QStringLiteral("vendor-srgb")));
  QVERIFY(fixture.model->errorText().contains(QStringLiteral("stale")));
  QVERIFY(fixture.settingsTransport.commits.isEmpty());
}

void ColorSettingsModelTest::selectionFencingAndInventoryChanges() {
  RouteFixture fixture;
  fixture.bringReady();

  QVERIFY(!fixture.model->selectOutput(QStringLiteral("edid:gone")));
  QVERIFY(fixture.model->selectOutput(QStringLiteral("edid:dp1")));
  QCOMPARE(fixture.model->profileRows().size(), 1);

  // A hotplug snapshot without the selected output clears the selection.
  Display::Snapshot reduced = twoOutputSnapshot(QStringLiteral("dsep"), 4);
  reduced.outputs.removeFirst();
  QCOMPARE(reduced.outputs.size(), 1);
  reduced.outputs.first().primary = true;
  Q_EMIT fixture.displayTransport.invalidated(QStringLiteral(":1.70"),
                                              QStringLiteral("dsep"), 4, true);
  fixture.displayTransport.replySnapshot(
      fixture.displayTransport.fetches.constLast(), reduced);
  QTRY_VERIFY(fixture.model->selectedOutputId().isEmpty());
  QVERIFY(fixture.model->profileRows().isEmpty());
}

void ColorSettingsModelTest::disconnectedRecordsStayVisibleReadOnly() {
  RouteFixture fixture;
  QVariantMap assignments = assignmentValue(
      QStringLiteral("edid:dp1"), QStringLiteral("vendor-srgb"), QString{})
                                  .toMap();
  assignments.insert(QStringLiteral("edid:removed"),
                     QVariantMap{{QStringLiteral("profile"),
                                  QStringLiteral("vendor-srgb")},
                                 {QStringLiteral("lineage"), QString{}}});
  fixture.bringReady(assignments);

  QVERIFY(fixture.model->ready());
  const QVariantList inactive = fixture.model->inactiveAssignmentRows();
  QCOMPARE(inactive.size(), 1);
  QCOMPARE(inactive.first().toMap().value(QStringLiteral("id")).toString(),
           QStringLiteral("edid:removed"));
  QCOMPARE(
      inactive.first().toMap().value(QStringLiteral("profileText")).toString(),
      QStringLiteral("vendor-srgb"));
}

void ColorSettingsModelTest::hasNoCompositorApplicationAuthority() {
  RouteFixture fixture;
  QVERIFY(!fixture.model->compositorApplicationSupported());
  const QMetaObject *meta = fixture.model->metaObject();
  QCOMPARE(meta->indexOfMethod("applyToCompositor()"), -1);
  QCOMPARE(meta->indexOfMethod("applyProfile()"), -1);
  QCOMPARE(meta->indexOfMethod("setOutputProfile()"), -1);
}

QTEST_GUILESS_MAIN(ColorSettingsModelTest)
#include "tst_color_settings_model.moc"
