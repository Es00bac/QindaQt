// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_input/tablet_devices_model.h>
#include <qindaqt/apps/settings_input/tablet_placement_model.h>
#include <qindaqt/apps/settings_input/tablet_surface.h>

#include <qindaqt/services/tablet_devices/tablet_geometry.h>

#include <QSignalSpy>
#include <QTest>

#include <cmath>

#include "support/fake_tablet_ports.h"

using namespace QindaQt::Apps::SettingsInput;
using namespace QindaQt::Tests;
using QindaQt::Services::TabletDevices::Rotation;
using QindaQt::Services::TabletDevices::TabletArea;
using QindaQt::Services::TabletDevices::TabletDeviceSnapshot;
using QindaQt::Services::TabletDevices::TabletMapChoice;
using QindaQt::Services::TabletDevices::kwinOrientationFor;
using QindaQt::Services::TabletDevices::rotateArea;
using QindaQt::Services::TabletDevices::sameArea;

namespace {

constexpr auto kBamboo = "1386:221:Wacom Bamboo Connect";

QVariant lastWrite(const FakeTabletPort &port, const QString &property) {
    QVariant value;
    for (const auto &write : port.writes) {
        if (std::get<1>(write) == property) {
            value = std::get<2>(write);
        }
    }
    return value;
}

int writesOf(const FakeTabletPort &port, const QString &property) {
    int count = 0;
    for (const auto &write : port.writes) {
        if (std::get<1>(write) == property) {
            ++count;
        }
    }
    return count;
}

TabletArea areaOf(const QVariant &value) {
    bool ok = false;
    const TabletArea area = TabletArea::fromVariant(value, &ok);
    return ok ? area : TabletArea{-1.0, -1.0, -1.0, -1.0};
}

TabletArea presented(const QVariantList &list) {
    return areaOf(QVariant(list));
}

} // namespace

class TabletPlacementModelTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void aDeskTabletTurnsWithTheUserAndAgainstTheScreen();
    void areasAreWrittenInKWinsFramesAndPresentedInTheUsers();
    void unusableAreasAreRefusedBeforeTheWire();
    void keepProportionsLocksTheScreenAreaToTheTablet();
    void aPenDisplayFollowsItsScreenAndOffersNoRotation();
    void pickingARotatedScreenCompensatesAtOnce();
    void anUnknownScreenRotationIsNeverGuessedIntoKWin();
    void thePresentedIntentFollowsTheLedger();
    void aChangeThatCannotBeRememberedSaysSo();
    void surfacesDrawTheWorkspaceAndEachScreen();
};

void TabletPlacementModelTest::aDeskTabletTurnsWithTheUserAndAgainstTheScreen() {
    FakeTabletPort port;
    FakeTabletOutputs outputs;
    outputs.scripted = {fakeMonitor(QStringLiteral("DP-1"), Rotation::Cw90,
                                    QRectF(0, 0, 1080, 1920))};
    port.scripted = {fakeBambooPen(QStringLiteral("DP-1"))};
    FakeTabletMappingStore store;
    TabletDevicesModel model(port, outputs, &store);
    model.refresh();
    auto *placement = model.selection()->placement();

    QVERIFY(!placement->penDisplay());
    QVERIFY(placement->rotationAvailable());
    QCOMPARE(placement->rotation(), 0);
    // The user is told why the pen will be turned back.
    QVERIFY(placement->rotationNote().contains(QStringLiteral("90")));
    // Opening Settings never writes.
    QVERIFY(port.writes.isEmpty());

    // Turned upside down on the desk, on a monitor turned 90°: 90° net.
    QVERIFY(placement->setRotation(180));
    QCOMPARE(lastWrite(port, QStringLiteral("orientationDBus")).toInt(),
             kwinOrientationFor(Rotation::Cw90));
    QCOMPARE(placement->rotation(), 180);
    const auto record = store.held.record(QLatin1String(kBamboo));
    QVERIFY(record.placement.rotation.has_value());
    QCOMPARE(*record.placement.rotation, Rotation::Cw180);
    // Recording a rotation adopts KWin's mapping, never invents one.
    QCOMPARE(record.choice, TabletMapChoice::NamedOutput);
    QCOMPARE(record.outputName, QStringLiteral("DP-1"));
    QVERIFY(!record.userChosen);
    QVERIFY(model.selection()->statusText().isEmpty());

    // Left-handed is a desk-tablet control; the calibration wizard is a
    // pen-display one.
    QVERIFY(model.selection()->leftHandedAvailable());
    QVERIFY(!model.selection()->calibrationAvailable());
}

void TabletPlacementModelTest::areasAreWrittenInKWinsFramesAndPresentedInTheUsers() {
    FakeTabletPort port;
    FakeTabletOutputs outputs;
    outputs.scripted = {fakeMonitor(QStringLiteral("DP-1"), Rotation::Cw90,
                                    QRectF(0, 0, 1080, 1920))};
    port.scripted = {fakeBambooPen(QStringLiteral("DP-1"))};
    FakeTabletMappingStore store;
    TabletDevicesModel model(port, outputs, &store);
    model.refresh();
    auto *placement = model.selection()->placement();
    QVERIFY(placement->inputAreaAvailable());
    QVERIFY(placement->outputAreaAvailable());

    // The left half of the tablet as it lies on the desk.
    QVERIFY(placement->applyInputArea(0.0, 0.0, 0.5, 1.0));
    // The monitor is turned 90°, so the pen is turned back first, and the
    // area follows into KWin's post-orientation frame.
    QCOMPARE(lastWrite(port, QStringLiteral("orientationDBus")).toInt(),
             kwinOrientationFor(Rotation::Cw270));
    QVERIFY(sameArea(areaOf(lastWrite(port, QStringLiteral("inputArea"))),
                     rotateArea(TabletArea{0.0, 0.0, 0.5, 1.0}, Rotation::Cw270)));
    // What the user sees is what they drew.
    QVERIFY(sameArea(presented(placement->inputArea()),
                     TabletArea{0.0, 0.0, 0.5, 1.0}));

    // The top-right quarter of the screen as it appears.
    QVERIFY(placement->applyOutputArea(0.5, 0.0, 0.5, 0.5));
    QVERIFY(sameArea(areaOf(lastWrite(port, QStringLiteral("outputArea"))),
                     rotateArea(TabletArea{0.5, 0.0, 0.5, 0.5}, Rotation::Cw270)));
    QVERIFY(sameArea(presented(placement->outputArea()),
                     TabletArea{0.5, 0.0, 0.5, 0.5}));

    // Both are remembered as seen, so the next rotation can re-base them.
    const auto record = store.held.record(QLatin1String(kBamboo));
    QVERIFY(sameArea(*record.placement.inputArea, TabletArea{0.0, 0.0, 0.5, 1.0}));
    QVERIFY(sameArea(*record.placement.outputArea, TabletArea{0.5, 0.0, 0.5, 0.5}));

    // Reset puts the whole tablet back on the whole screen.
    QVERIFY(placement->resetAreas());
    QVERIFY(presented(placement->inputArea()).isWhole());
    QVERIFY(presented(placement->outputArea()).isWhole());
}

void TabletPlacementModelTest::unusableAreasAreRefusedBeforeTheWire() {
    FakeTabletPort port;
    FakeTabletOutputs outputs;
    outputs.scripted = {fakeMonitor(QStringLiteral("DP-1"), Rotation::None)};
    port.scripted = {fakeBambooPen(QStringLiteral("DP-1"))};
    FakeTabletMappingStore store;
    TabletDevicesModel model(port, outputs, &store);
    model.refresh();
    auto *placement = model.selection()->placement();

    // Outside the tablet, too small for a hand, or not a number: KWin would
    // store any of these and libinput would refuse them.
    QVERIFY(!placement->applyInputArea(0.9, 0.0, 0.2, 1.0));
    QVERIFY(!model.selection()->statusText().isEmpty());
    QVERIFY(!placement->applyInputArea(0.0, 0.0, 0.01, 0.5));
    QVERIFY(!placement->applyOutputArea(0.0, 0.0, std::nan(""), 0.5));
    QVERIFY(port.writes.isEmpty());
    QVERIFY(!store.held.contains(QLatin1String(kBamboo)));
}

void TabletPlacementModelTest::keepProportionsLocksTheScreenAreaToTheTablet() {
    FakeTabletPort port;
    FakeTabletOutputs outputs;
    outputs.scripted = {fakeMonitor(QStringLiteral("DP-1"), Rotation::None,
                                    QRectF(0, 0, 1920, 1080))};
    port.scripted = {fakeBambooPen(QStringLiteral("DP-1"))};
    FakeTabletMappingStore store;
    TabletDevicesModel model(port, outputs, &store);
    model.refresh();
    auto *placement = model.selection()->placement();
    QVERIFY(placement->proportionsAvailable());
    // The whole 147.2 x 92 mm tablet on a whole 16:9 screen does not match.
    QVERIFY(!placement->keepProportions());
    QCOMPARE(placement->outputAspectLock(), 0.0);

    placement->setKeepProportions(true);
    QVERIFY(placement->keepProportions());
    const double lock = placement->outputAspectLock();
    QVERIFY(std::abs(lock - (147.2 / 92.0) * (1080.0 / 1920.0)) < 1e-9);
    const TabletArea shaped = areaOf(lastWrite(port, QStringLiteral("outputArea")));
    // Same shape on screen as on the tablet: a circle stays round.
    QVERIFY(std::abs(shaped.width / shaped.height - lock) < 1e-9);
    QVERIFY(shaped.x + shaped.width <= 1.0 && shaped.y + shaped.height <= 1.0);

    // A new tablet area keeps the screen area in proportion to it.
    QVERIFY(placement->applyInputArea(0.0, 0.0, 0.5, 1.0));
    const TabletArea follows = presented(placement->outputArea());
    const double halfLock = (0.5 * 147.2 / 92.0) * (1080.0 / 1920.0);
    QVERIFY(std::abs(follows.width / follows.height - halfLock) < 1e-9);
    QVERIFY(std::abs(placement->outputAspectLock() - halfLock) < 1e-9);

    // The switch reflects the recorded areas when the tablet is shown again.
    model.refresh();
    QVERIFY(model.selection()->placement()->keepProportions());
}

void TabletPlacementModelTest::aPenDisplayFollowsItsScreenAndOffersNoRotation() {
    FakeTabletPort port;
    FakeTabletOutputs outputs;
    auto penScreen = fakePenDisplayOutput();
    penScreen.rotation = Rotation::Cw90;
    penScreen.logicalGeometry = QRectF(1920, 0, 1080, 1920);
    outputs.scripted = {fakeLaptopPanel(), penScreen};
    port.scripted = {fakeWacomPen()};
    FakeTabletMappingStore store;
    TabletDevicesModel model(port, outputs, &store);
    model.refresh();
    auto *selection = model.selection();
    auto *placement = selection->placement();

    QVERIFY(placement->penDisplay());
    QVERIFY(!placement->rotationAvailable());
    QVERIFY(placement->rotationNote().isEmpty());
    // It names the screen it turns with.
    QVERIFY(placement->followNote().contains(QStringLiteral("Wacom")));
    // Rotation, left-handed and the input area are desk-tablet controls;
    // calibration is a pen-display one.
    QVERIFY(!selection->leftHandedAvailable());
    QVERIFY(!placement->inputAreaAvailable());
    QVERIFY(selection->calibrationAvailable());
    // Its screen area is drawn as the turned screen shows it and written in
    // the panel's own frame.
    QVERIFY(placement->applyOutputArea(0.0, 0.0, 1.0, 0.5));
    QVERIFY(sameArea(areaOf(lastWrite(port, QStringLiteral("outputArea"))),
                     rotateArea(TabletArea{0.0, 0.0, 1.0, 0.5}, Rotation::Cw270)));
    QVERIFY(sameArea(presented(placement->outputArea()),
                     TabletArea{0.0, 0.0, 1.0, 0.5}));
    // A pen display records no desk-tablet intent.
    QVERIFY(!store.held.record(QStringLiteral("1386:934:Wacom One Pen Display 13"))
                 .placement.outputArea.has_value());
    // Mapped to the whole workspace it cannot follow one screen, and says so.
    QVERIFY(selection->applyMapping(QStringLiteral("workspace")));
    QVERIFY(placement->followNote().contains(QStringLiteral("every screen")));
}

void TabletPlacementModelTest::pickingARotatedScreenCompensatesAtOnce() {
    FakeTabletPort port;
    FakeTabletOutputs outputs;
    outputs.scripted = {fakeMonitor(QStringLiteral("DP-1"), Rotation::Cw90,
                                    QRectF(0, 0, 1080, 1920))};
    TabletDeviceSnapshot pen = fakeBambooPen();
    pen.properties.insert(QStringLiteral("mapToWorkspace"), true);
    port.scripted = {pen};
    FakeTabletMappingStore store;
    TabletDevicesModel model(port, outputs, &store);
    model.refresh();
    auto *selection = model.selection();
    QCOMPARE(selection->mapMode(), QStringLiteral("workspace"));

    QVERIFY(selection->applyMapping(QStringLiteral("output"),
                                    QStringLiteral("DP-1")));
    // The mapping first (workspace off, then the name), then the rotation
    // that screen needs.
    QCOMPARE(std::get<1>(port.writes.at(0)), QStringLiteral("mapToWorkspace"));
    QCOMPARE(std::get<1>(port.writes.at(1)), QStringLiteral("outputName"));
    QCOMPARE(lastWrite(port, QStringLiteral("orientationDBus")).toInt(),
             kwinOrientationFor(Rotation::Cw270));
    QVERIFY(selection->placement()->rotationNote().contains(QStringLiteral("90")));
    QVERIFY(store.held.record(QLatin1String(kBamboo)).placement.rotation.has_value());

    // And back to the whole workspace, which has no rotation to undo.
    QVERIFY(selection->applyMapping(QStringLiteral("workspace")));
    QCOMPARE(lastWrite(port, QStringLiteral("orientationDBus")).toInt(), 0);
}

void TabletPlacementModelTest::anUnknownScreenRotationIsNeverGuessedIntoKWin() {
    FakeTabletPort port;
    FakeTabletOutputs outputs;
    // Display1 has not answered.
    outputs.scripted = {fakeMonitor(QStringLiteral("DP-1"), std::nullopt)};
    port.scripted = {fakeBambooPen()};
    FakeTabletMappingStore store;
    TabletDevicesModel model(port, outputs, &store);
    model.refresh();
    auto *selection = model.selection();
    QVERIFY(selection->placement()->rotationNote().contains(
        QStringLiteral("not known")));
    // Choosing a screen writes the mapping and nothing else.
    QVERIFY(selection->applyMapping(QStringLiteral("output"),
                                    QStringLiteral("DP-1")));
    QCOMPARE(writesOf(port, QStringLiteral("orientationDBus")), 0);
    // A rotation the user asks for is applied as if the screen were upright
    // and recorded, so the session corrects it once the rotation is known.
    QVERIFY(selection->placement()->setRotation(90));
    QCOMPARE(lastWrite(port, QStringLiteral("orientationDBus")).toInt(),
             kwinOrientationFor(Rotation::Cw90));
    const auto recorded = store.held.record(QLatin1String(kBamboo)).placement;
    QVERIFY(recorded.rotation.has_value());
    QCOMPARE(*recorded.rotation, Rotation::Cw90);
}

void TabletPlacementModelTest::thePresentedIntentFollowsTheLedger() {
    FakeTabletPort port;
    FakeTabletOutputs outputs;
    outputs.scripted = {fakeMonitor(QStringLiteral("DP-1"), Rotation::None)};
    port.scripted = {fakeBambooPen(QStringLiteral("DP-1"))};
    FakeTabletMappingStore store;
    TabletDevicesModel model(port, outputs, &store);
    model.refresh();
    auto *placement = model.selection()->placement();
    QCOMPARE(placement->rotation(), 0);
    QSignalSpy changed(placement, &TabletPlacementModel::placementChanged);

    // The session recorded a rotation (or another Settings window did).
    QindaQt::Services::TabletDevices::TabletMappingRecord record;
    record.choice = TabletMapChoice::NamedOutput;
    record.outputName = QStringLiteral("DP-1");
    record.placement.rotation = Rotation::Cw270;
    store.held.setRecord(QLatin1String(kBamboo), record);
    Q_EMIT store.ledgerChanged();
    QVERIFY(changed.size() >= 1);
    QCOMPARE(placement->rotation(), 270);
    // Presenting never writes.
    QVERIFY(port.writes.isEmpty());
}

void TabletPlacementModelTest::aChangeThatCannotBeRememberedSaysSo() {
    FakeTabletPort port;
    FakeTabletOutputs outputs;
    outputs.scripted = {fakeMonitor(QStringLiteral("DP-1"), Rotation::None)};
    port.scripted = {fakeBambooPen(QStringLiteral("DP-1"))};
    FakeTabletMappingStore store;
    store.saveFails = true;
    TabletDevicesModel model(port, outputs, &store);
    model.refresh();
    auto *placement = model.selection()->placement();
    // KWin obeys, the desktop cannot remember: the user is told.
    QVERIFY(placement->setRotation(90));
    QCOMPARE(lastWrite(port, QStringLiteral("orientationDBus")).toInt(),
             kwinOrientationFor(Rotation::Cw90));
    QVERIFY(model.selection()->statusText().contains(QStringLiteral("remembered")));
    // And with no ledger at all the same.
    TabletDevicesModel unstored(port, outputs, nullptr);
    unstored.refresh();
    QVERIFY(unstored.selection()->placement()->setRotation(180));
    QVERIFY(unstored.selection()->statusText().contains(
        QStringLiteral("remembered")));
}

void TabletPlacementModelTest::surfacesDrawTheWorkspaceAndEachScreen() {
    const QList<QindaQt::Services::TabletDevices::TabletOutputCandidate> outputs{
        fakeMonitor(QStringLiteral("DP-1"), Rotation::None, QRectF(0, 0, 1920, 1080)),
        fakeMonitor(QStringLiteral("DP-2"), Rotation::Cw90,
                    QRectF(1920, 0, 1080, 1920))};
    const TabletSurface workspace =
        tabletSurfaceFor(TabletMapChoice::EntireWorkspace, {}, outputs);
    QCOMPARE(workspace.width, 3000.0);
    QCOMPARE(workspace.height, 1920.0);
    QCOMPARE(workspace.screens.size(), 2);
    QCOMPARE(workspace.screens.at(1).area,
             QRectF(1920.0 / 3000.0, 0.0, 1080.0 / 3000.0, 1.0));
    QCOMPARE(workspace.screens.at(1).label, QStringLiteral("DP-2"));
    // A named screen is that screen, turned as it is.
    const TabletSurface named =
        tabletSurfaceFor(TabletMapChoice::NamedOutput, QStringLiteral("DP-2"), outputs);
    QCOMPARE(named.width, 1080.0);
    QCOMPARE(named.height, 1920.0);
    // An absent named screen and the active screen fall back to the largest.
    QCOMPARE(tabletSurfaceFor(TabletMapChoice::NamedOutput, QStringLiteral("HDMI-A-9"),
                              outputs)
                 .width,
             1920.0);
    // Nothing known is drawn as nothing known, never as a zero-sized screen.
    const TabletSurface none =
        tabletSurfaceFor(TabletMapChoice::FollowActiveScreen, {},
                         {fakeMonitor(QStringLiteral("DP-1"), std::nullopt, QRectF())});
    QCOMPARE(none.width, 0.0);
    QVERIFY(none.screens.isEmpty());
    // The label a user reads: EDID identity with the connector.
    QCOMPARE(tabletOutputLabel(outputs.at(0)),
             QStringLiteral("Dell Inc. U2720Q (DP-1)"));
}

QTEST_MAIN(TabletPlacementModelTest)
#include "tst_tablet_placement_model.moc"
