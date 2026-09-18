// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_input/tablet_devices_model.h>

#include <QSignalSpy>
#include <QTest>

#include "support/fake_tablet_ports.h"

using namespace QindaQt::Apps::SettingsInput;
using namespace QindaQt::Tests;
using QindaQt::Services::TabletDevices::TabletDeviceSnapshot;

class TabletDevicesModelTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void groupsAPenAndItsPadIntoOneTablet();
    void anUnreachableAuthorityIsDegradedNotEmpty();
    void theSelectedTabletSurvivesARefresh();
    void theDeepLinkSelectsByDeviceGroup();
    void capabilityFlagsDecideWhatIsAvailable();
    void mappingWritesTheWorkspaceFlagAndOutputInOrder();
    void aRefusedWriteKeepsThePresentedValueAndSaysWhy();
    void areaHelpersComputeFromTheTabletsOwnSize();
    void resetPutsTheDeviceBackToItsOwnDefaults();
    void aMappingChosenInTheRouteIsRecordedInTheLedger();
};

void TabletDevicesModelTest::groupsAPenAndItsPadIntoOneTablet() {
    FakeTabletPort port;
    FakeTabletOutputs outputs;
    port.scripted = {fakeWacomPen(), fakeWacomPad()};
    FakeTabletMappingStore store;
    TabletDevicesModel model(port, outputs, &store);
    model.refresh();

    // One tablet, not two devices: the pen and its pad share a group.
    QCOMPARE(model.count(), 1);
    QCOMPARE(model.data(model.index(0, 0), TabletDevicesModel::LabelRole)
                 .toString(),
             QStringLiteral("Wacom One Pen Display 13"));
    QVERIFY(model.data(model.index(0, 0), TabletDevicesModel::HasPadRole)
                .toBool());
    QCOMPARE(model.data(model.index(0, 0),
                        TabletDevicesModel::MappedOutputRole)
                 .toString(),
             QStringLiteral("HDMI-A-1"));
    QVERIFY(model.selection()->hasPen());
    QVERIFY(model.selection()->hasPad());
    QCOMPARE(model.selection()->padButtonCount(), 4);
    QCOMPARE(model.selection()->padRingCount(), 1);
    // KWin's unsigned -1 means "the authority reports none", not zero.
    QCOMPARE(model.selection()->padStripCount(), -1);
}

void TabletDevicesModelTest::anUnreachableAuthorityIsDegradedNotEmpty() {
    FakeTabletPort port;
    FakeTabletOutputs outputs;
    port.listError = QStringLiteral("org.kde.KWin is not reachable");
    FakeTabletMappingStore store;
    TabletDevicesModel model(port, outputs, &store);
    model.refresh();
    QVERIFY(!model.available());
    QVERIFY(model.empty());

    // And an authority that answers with nothing is the honest empty state.
    port.listError.clear();
    model.refresh();
    QVERIFY(model.available());
    QVERIFY(model.empty());
}

void TabletDevicesModelTest::theSelectedTabletSurvivesARefresh() {
    FakeTabletPort port;
    FakeTabletOutputs outputs;
    TabletDeviceSnapshot other = fakeWacomPen(QStringLiteral("event25"));
    other.name = QStringLiteral("Second Tablet Pen");
    other.productId = 935;
    other.name = QStringLiteral("Second Tablet Pen");
    port.scripted = {fakeWacomPen(), other};
    FakeTabletMappingStore store;
    TabletDevicesModel model(port, outputs, &store);
    model.refresh();
    model.select(1);
    QCOMPARE(model.selection()->deviceGroupId(), QStringLiteral("1386:935:Second Tablet"));

    // A re-plug reorders the authority's list; the user's controls must not
    // jump to another device under their hand.
    port.scripted = {other, fakeWacomPen()};
    model.refresh();
    QCOMPARE(model.selectedIndex(), 0);
    QCOMPARE(model.selection()->deviceGroupId(), QStringLiteral("1386:935:Second Tablet"));
}

void TabletDevicesModelTest::theDeepLinkSelectsByDeviceGroup() {
    FakeTabletPort port;
    FakeTabletOutputs outputs;
    TabletDeviceSnapshot other = fakeWacomPen(QStringLiteral("event25"));
    other.productId = 935;
    other.name = QStringLiteral("Second Tablet Pen");
    port.scripted = {fakeWacomPen(), other};
    FakeTabletMappingStore store;
    TabletDevicesModel model(port, outputs, &store);
    model.refresh();

    QVERIFY(model.selectGroup(QStringLiteral("1386:935:Second Tablet")));
    QCOMPARE(model.selectedIndex(), 1);
    // A stale id from an old notification names no connected tablet; the
    // route must be able to say so rather than show a different device.
    QVERIFY(!model.selectGroup(QStringLiteral("1386:777:Unplugged Tablet")));
    QCOMPARE(model.selectedIndex(), 1);
}

void TabletDevicesModelTest::capabilityFlagsDecideWhatIsAvailable() {
    FakeTabletPort port;
    FakeTabletOutputs outputs;
    TabletDeviceSnapshot bare = fakeWacomPen();
    bare.properties.insert(QStringLiteral("supportsCalibrationMatrix"), false);
    bare.properties.insert(QStringLiteral("supportsRotation"), false);
    bare.properties.insert(QStringLiteral("supportsLeftHanded"), false);
    bare.properties.insert(QStringLiteral("supportsPressureRange"), false);
    bare.properties.insert(QStringLiteral("supportsInputArea"), false);
    bare.properties.insert(QStringLiteral("supportsDisableEvents"), false);
    port.scripted = {bare};
    FakeTabletMappingStore store;
    TabletDevicesModel model(port, outputs, &store);
    model.refresh();

    auto *selection = model.selection();
    QVERIFY(!selection->calibrationAvailable());
    QVERIFY(!selection->rotationAvailable());
    QVERIFY(!selection->leftHandedAvailable());
    QVERIFY(!selection->pressureRangeAvailable());
    QVERIFY(!selection->inputAreaAvailable());
    QVERIFY(!selection->deviceEnabledAvailable());
    // The pressure curve and the screen rectangle exist for every tool.
    QVERIFY(selection->pressureCurveAvailable());
    QVERIFY(selection->outputAreaAvailable());
    QVERIFY(selection->relativeModeAvailable());
}

void TabletDevicesModelTest::mappingWritesTheWorkspaceFlagAndOutputInOrder() {
    FakeTabletPort port;
    FakeTabletOutputs outputs;
    outputs.scripted = {fakeLaptopPanel(), fakePenDisplayOutput()};
    port.scripted = {fakeWacomPen()};
    FakeTabletMappingStore store;
    TabletDevicesModel model(port, outputs, &store);
    model.refresh();
    auto *selection = model.selection();
    QCOMPARE(selection->mapMode(), QStringLiteral("output"));
    QCOMPARE(selection->outputName(), QStringLiteral("HDMI-A-1"));
    QCOMPARE(selection->outputNames(),
             (QStringList{QStringLiteral("eDP-1"), QStringLiteral("HDMI-A-1")}));
    QVERIFY(selection->outputLabels().at(1).contains(QStringLiteral("Wacom")));

    port.writes.clear();
    QVERIFY(selection->applyMapping(QStringLiteral("workspace")));
    // AGENT-GUARD: outputName is cleared before the workspace flag is set;
    // the other order leaves the pen spanning every screen for a frame.
    QCOMPARE(port.writes.size(), 2);
    QCOMPARE(std::get<1>(port.writes.at(0)), QStringLiteral("outputName"));
    QVERIFY(std::get<2>(port.writes.at(0)).toString().isEmpty());
    QCOMPARE(std::get<1>(port.writes.at(1)), QStringLiteral("mapToWorkspace"));
    QCOMPARE(std::get<2>(port.writes.at(1)).toBool(), true);
    QCOMPARE(selection->mapMode(), QStringLiteral("workspace"));

    // "A specific screen" with no screen named writes nothing and says so.
    port.writes.clear();
    QVERIFY(!selection->applyMapping(QStringLiteral("output"), QString()));
    QCOMPARE(port.writes.size(), 0);
    QVERIFY(!selection->statusText().isEmpty());
}

void TabletDevicesModelTest::aRefusedWriteKeepsThePresentedValueAndSaysWhy() {
    FakeTabletPort port;
    FakeTabletOutputs outputs;
    port.scripted = {fakeWacomPen()};
    port.refuse = QStringLiteral("rotation");
    FakeTabletMappingStore store;
    TabletDevicesModel model(port, outputs, &store);
    model.refresh();
    auto *selection = model.selection();

    selection->setRotation(90);
    // Success alone moves the presented value; a refused write leaves the
    // route showing what the device actually is.
    QCOMPARE(selection->rotation(), 0);
    QVERIFY(selection->statusText().contains(QStringLiteral("refused")));
}

void TabletDevicesModelTest::areaHelpersComputeFromTheTabletsOwnSize() {
    FakeTabletPort port;
    FakeTabletOutputs outputs;
    port.scripted = {fakeWacomPen()};
    FakeTabletMappingStore store;
    TabletDevicesModel model(port, outputs, &store);
    model.refresh();
    auto *selection = model.selection();
    QVERIFY(selection->aspectRatioAvailable());

    port.writes.clear();
    QVERIFY(selection->fitWholeScreen());
    QCOMPARE(std::get<1>(port.writes.at(0)), QStringLiteral("outputArea"));
    QCOMPARE(std::get<2>(port.writes.at(0)).toList(),
             (QVariantList{0.0, 0.0, 1.0, 1.0}));

    // A 294 x 166 tablet on a 16:9 screen is very slightly narrower.
    port.writes.clear();
    QVERIFY(selection->keepTabletProportions(1920.0, 1080.0));
    const QVariantList area = std::get<2>(port.writes.at(0)).toList();
    QCOMPARE(area.size(), 4);
    QVERIFY(area.at(2).toDouble() < 1.0);
    QVERIFY(qFuzzyCompare(area.at(3).toDouble(), 1.0));

    // A tablet that reports no size cannot have its proportions matched, and
    // the route says so instead of guessing.
    TabletDeviceSnapshot sizeless = fakeWacomPen();
    sizeless.properties.insert(QStringLiteral("size"),
                               QVariantList{-1.0, -1.0});
    port.scripted = {sizeless};
    model.refresh();
    QVERIFY(!model.selection()->aspectRatioAvailable());
    QVERIFY(!model.selection()->keepTabletProportions(1920.0, 1080.0));
    QVERIFY(!model.selection()->statusText().isEmpty());
}

void TabletDevicesModelTest::resetPutsTheDeviceBackToItsOwnDefaults() {
    FakeTabletPort port;
    FakeTabletOutputs outputs;
    TabletDeviceSnapshot pen = fakeWacomPen();
    pen.properties.insert(QStringLiteral("calibrationMatrix"),
                          QStringLiteral("2,0,0,0,0,2,0,0,0,0,1,0,0,0,0,1"));
    pen.properties.insert(QStringLiteral("rotation"), 90u);
    port.scripted = {pen};
    FakeTabletMappingStore store;
    TabletDevicesModel model(port, outputs, &store);
    model.refresh();
    auto *selection = model.selection();
    QVERIFY(selection->calibrated());

    port.writes.clear();
    QVERIFY(selection->resetDevice());
    QStringList written;
    for (const auto &write : port.writes) {
        written.append(std::get<1>(write));
    }
    QVERIFY(written.contains(QStringLiteral("calibrationMatrix")));
    QVERIFY(written.contains(QStringLiteral("rotation")));
    QVERIFY(written.contains(QStringLiteral("outputArea")));
    QCOMPARE(selection->calibrationMatrix(),
             QStringLiteral("1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1"));
    QVERIFY(!selection->calibrated());
    QCOMPARE(selection->rotation(), 0);

    // A curve KWin could not read is refused before it reaches the wire.
    port.writes.clear();
    QVERIFY(!selection->applyPressureCurve(QStringLiteral("0.9,0;0.2,1;")));
    QCOMPARE(port.writes.size(), 0);
    QVERIFY(!selection->statusText().isEmpty());
}

void TabletDevicesModelTest::aMappingChosenInTheRouteIsRecordedInTheLedger() {
    // AGENT-GUARD: KWin obeying is not the desktop remembering. Without the
    // ledger write the session policy re-decides on its next pass and the
    // user's choice silently reverts, which reads as "Settings does nothing".
    FakeTabletPort port;
    FakeTabletOutputs outputs;
    outputs.scripted = {fakeLaptopPanel(), fakePenDisplayOutput()};
    port.scripted = {fakeWacomPen()};
    FakeTabletMappingStore store;
    TabletDevicesModel model(port, outputs, &store);
    model.refresh();
    auto *selection = model.selection();

    QVERIFY(selection->applyMapping(QStringLiteral("follow")));
    const auto record =
        store.held.record(QStringLiteral("1386:934:Wacom One Pen Display 13"));
    QCOMPARE(record.choice,
             QindaQt::Services::TabletDevices::TabletMapChoice::FollowActiveScreen);
    // It is the user's choice, so automatic matching may never override it.
    QVERIFY(record.userChosen);
    QVERIFY(record.announced);
    QVERIFY(selection->statusText().isEmpty());

    QVERIFY(selection->applyMapping(QStringLiteral("output"),
                                    QStringLiteral("HDMI-A-1")));
    const auto named =
        store.held.record(QStringLiteral("1386:934:Wacom One Pen Display 13"));
    QCOMPARE(named.choice,
             QindaQt::Services::TabletDevices::TabletMapChoice::NamedOutput);
    QCOMPARE(named.outputName, QStringLiteral("HDMI-A-1"));

    // A ledger that cannot be written says so rather than implying the
    // choice will be remembered.
    store.saveFails = true;
    QVERIFY(selection->applyMapping(QStringLiteral("workspace")));
    QVERIFY(selection->statusText().contains(QStringLiteral("remembered")));

    // And the key is never KWin's deviceGroupId.
    QVERIFY(!store.held.contains(QStringLiteral("5ggmGJ0A0G+Au+fGRi4fc0/veWc=")));
}

QTEST_MAIN(TabletDevicesModelTest)
#include "tst_tablet_devices_model.moc"
