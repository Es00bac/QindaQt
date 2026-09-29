// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/session/desktop_controls/tablet_mapping_policy.h>

#include <qindaqt/services/tablet_devices/tablet_geometry.h>

#include <QSignalSpy>
#include <QtTest>

#include "fake_tablet_authority.h"

using namespace QindaQt::Session::DesktopControls;
using namespace QindaQt::Services::TabletDevices;
using namespace QindaQt::Tests::TabletPolicy;

// ADR-0285 in the session: the policy keeps every tablet's rotation and areas
// right for the screen it reaches, on start, on every output change and on
// every mapping change, and records the intent it adopted.

namespace {

constexpr auto kBamboo = "1386:221:Wacom Bamboo Connect";
constexpr auto kWacomOne = "1386:934:Wacom One Pen Display 13";

// The owner's desk tablet as KWin 6.6.6 reported it (qinda-top, 2026-09-28).
TabletDeviceSnapshot bamboo(const QString &outputName = QStringLiteral("DP-1")) {
    TabletDeviceSnapshot pen;
    pen.deviceId = QStringLiteral("event3");
    pen.name = QStringLiteral("Wacom Bamboo Connect Pen");
    pen.deviceGroupId = QStringLiteral("Zm9vYmFyYmF6cXV4MTIzNDU2Nzg5MA==");
    pen.vendorId = 1386;
    pen.productId = 221;
    pen.tabletTool = true;
    pen.properties = QVariantMap{
        {QStringLiteral("outputName"), outputName},
        {QStringLiteral("mapToWorkspace"), false},
        {QStringLiteral("outputArea"), QVariantList{0.0, 0.0, 1.0, 1.0}},
        {QStringLiteral("inputArea"), QVariantList{0.0, 0.0, 1.0, 1.0}},
        {QStringLiteral("orientationDBus"), 0},
        {QStringLiteral("leftHanded"), false},
        {QStringLiteral("size"), QVariantList{147.2, 92.0}},
        {QStringLiteral("supportsCalibrationMatrix"), true},
        {QStringLiteral("supportsInputArea"), true},
        {QStringLiteral("supportsRotation"), false},
        {QStringLiteral("supportsLeftHanded"), true},
    };
    return pen;
}

TabletDeviceSnapshot wacomOne() {
    TabletDeviceSnapshot pen = bamboo(QStringLiteral("HDMI-A-1"));
    pen.deviceId = QStringLiteral("event19");
    pen.name = QStringLiteral("Wacom One Pen Display 13 Pen");
    pen.productId = 934;
    // libinput's verdict for a screen: no tablet area.
    pen.properties.insert(QStringLiteral("supportsInputArea"), false);
    return pen;
}

TabletOutputCandidate monitor(const QString &connector,
                              std::optional<Rotation> rotation) {
    TabletOutputCandidate output{connector, QStringLiteral("Dell Inc."),
                                 QStringLiteral("U2720Q"),
                                 QStringLiteral("U2720Q"), false, true};
    output.rotation = rotation;
    return output;
}

TabletOutputCandidate wacomScreen(std::optional<Rotation> rotation) {
    TabletOutputCandidate output{QStringLiteral("HDMI-A-1"),
                                 QStringLiteral("Wacom Technology Corp."),
                                 QStringLiteral("Wacom One 13"),
                                 QStringLiteral("Wacom One 13"), false, true};
    output.rotation = rotation;
    return output;
}

// The last value written to `property`, or an invalid QVariant.
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

bool areaIs(const QVariant &value, const TabletArea &expected) {
    bool ok = false;
    const TabletArea area = TabletArea::fromVariant(value, &ok);
    return ok && sameArea(area, expected);
}

} // namespace

class TabletOrientationPolicyTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void aDeskTabletOnARotatedScreenIsTurnedBackAndRecorded();
    void turningTheScreenRebasesTheRecordedIntent();
    void aRecordedRotationIsHonouredOnAnUprightScreen();
    void theWholeWorkspaceIsNeverCompensated();
    void mixedRotationsBehindTheActiveScreenAreLeftAlone();
    void anUnknownRotationWritesAndRecordsNothing();
    void aPenDisplayLosesAStaleRotationOfItsOwn();
    void aRefusedOrientationIsReportedAndRetried();
};

void TabletOrientationPolicyTest::aDeskTabletOnARotatedScreenIsTurnedBackAndRecorded() {
    FakeTabletPort port;
    port.scripted = {bamboo()};
    FakeWatcher watcher;
    FakeOutputs outputs;
    outputs.scripted = {monitor(QStringLiteral("DP-1"), Rotation::Cw90)};
    FakeStore store;
    TabletMappingPolicy policy(port, watcher, outputs, store);
    QVERIFY(policy.start());

    // The monitor turned 90°: the pen is turned back 90° (270° clockwise),
    // so up on the desk stays up on the screen. Whole areas need no write.
    QCOMPARE(writesOf(port, QStringLiteral("orientationDBus")), 1);
    QCOMPARE(lastWrite(port, QStringLiteral("orientationDBus")).toInt(),
             kwinOrientationFor(Rotation::Cw270));
    QCOMPARE(writesOf(port, QStringLiteral("inputArea")), 0);
    QCOMPARE(writesOf(port, QStringLiteral("outputArea")), 0);

    // The adopted intent is recorded with the (adopted) mapping, so the next
    // rotation re-bases it instead of guessing.
    const TabletMappingRecord record = store.held.record(QLatin1String(kBamboo));
    QCOMPARE(record.choice, TabletMapChoice::NamedOutput);
    QCOMPARE(record.outputName, QStringLiteral("DP-1"));
    QVERIFY(record.placement.rotation.has_value());
    QCOMPARE(*record.placement.rotation, Rotation::None);
    QVERIFY(record.placement.inputArea.has_value() &&
            record.placement.inputArea->isWhole());
    QVERIFY(record.placement.outputArea.has_value() &&
            record.placement.outputArea->isWhole());

    // A second pass finds the device already right and writes nothing.
    const qsizetype before = port.writes.size();
    policy.reconcile();
    QCOMPARE(port.writes.size(), before);
}

void TabletOrientationPolicyTest::turningTheScreenRebasesTheRecordedIntent() {
    FakeTabletPort port;
    port.scripted = {bamboo()};
    FakeWatcher watcher;
    FakeOutputs outputs;
    outputs.scripted = {monitor(QStringLiteral("DP-1"), Rotation::None)};
    FakeStore store;
    TabletMappingRecord record;
    record.choice = TabletMapChoice::NamedOutput;
    record.outputName = QStringLiteral("DP-1");
    record.userChosen = true;
    record.announced = true;
    // The left half of the tablet onto the top-right quarter of the screen.
    record.placement.rotation = Rotation::None;
    record.placement.inputArea = TabletArea{0.0, 0.0, 0.5, 1.0};
    record.placement.outputArea = TabletArea{0.5, 0.0, 0.5, 0.5};
    store.held.setRecord(QLatin1String(kBamboo), record);
    TabletMappingPolicy policy(port, watcher, outputs, store);
    QVERIFY(policy.start());
    QVERIFY(areaIs(lastWrite(port, QStringLiteral("inputArea")),
                   TabletArea{0.0, 0.0, 0.5, 1.0}));
    QVERIFY(areaIs(lastWrite(port, QStringLiteral("outputArea")),
                   TabletArea{0.5, 0.0, 0.5, 0.5}));

    // Displays turns the monitor 90°. KWin's transform would turn the pen's
    // directions and move the screen area; both are re-based from the intent.
    outputs.publish({monitor(QStringLiteral("DP-1"), Rotation::Cw90)});
    QCOMPARE(lastWrite(port, QStringLiteral("orientationDBus")).toInt(),
             kwinOrientationFor(Rotation::Cw270));
    QVERIFY(areaIs(lastWrite(port, QStringLiteral("inputArea")),
                   rotateArea(TabletArea{0.0, 0.0, 0.5, 1.0}, Rotation::Cw270)));
    QVERIFY(areaIs(lastWrite(port, QStringLiteral("outputArea")),
                   rotateArea(TabletArea{0.5, 0.0, 0.5, 0.5}, Rotation::Cw270)));
    // The intent itself did not move: it is what the user sees.
    const TabletMappingRecord after = store.held.record(QLatin1String(kBamboo));
    QVERIFY(sameArea(*after.placement.inputArea, TabletArea{0.0, 0.0, 0.5, 1.0}));
    QVERIFY(sameArea(*after.placement.outputArea, TabletArea{0.5, 0.0, 0.5, 0.5}));

    // And back upright: the original values return.
    outputs.publish({monitor(QStringLiteral("DP-1"), Rotation::None)});
    QCOMPARE(lastWrite(port, QStringLiteral("orientationDBus")).toInt(), 0);
    QVERIFY(areaIs(lastWrite(port, QStringLiteral("inputArea")),
                   TabletArea{0.0, 0.0, 0.5, 1.0}));
    QVERIFY(areaIs(lastWrite(port, QStringLiteral("outputArea")),
                   TabletArea{0.5, 0.0, 0.5, 0.5}));
}

void TabletOrientationPolicyTest::aRecordedRotationIsHonouredOnAnUprightScreen() {
    FakeTabletPort port;
    port.scripted = {bamboo()};
    FakeWatcher watcher;
    FakeOutputs outputs;
    outputs.scripted = {monitor(QStringLiteral("DP-1"), Rotation::None)};
    FakeStore store;
    TabletMappingRecord record;
    record.choice = TabletMapChoice::NamedOutput;
    record.outputName = QStringLiteral("DP-1");
    record.announced = true;
    record.placement.rotation = Rotation::Cw90;
    store.held.setRecord(QLatin1String(kBamboo), record);
    TabletMappingPolicy policy(port, watcher, outputs, store);
    QVERIFY(policy.start());
    // Turned 90° clockwise on the desk: KWin's Portrait matrix.
    QCOMPARE(lastWrite(port, QStringLiteral("orientationDBus")).toInt(),
             kwinOrientationFor(Rotation::Cw90));
}

void TabletOrientationPolicyTest::theWholeWorkspaceIsNeverCompensated() {
    FakeTabletPort port;
    TabletDeviceSnapshot pen = bamboo(QString());
    pen.properties.insert(QStringLiteral("mapToWorkspace"), true);
    port.scripted = {pen};
    FakeWatcher watcher;
    FakeOutputs outputs;
    outputs.scripted = {monitor(QStringLiteral("DP-1"), Rotation::Cw90)};
    FakeStore store;
    TabletMappingRecord record;
    record.choice = TabletMapChoice::EntireWorkspace;
    record.userChosen = true;
    record.announced = true;
    store.held.setRecord(QLatin1String(kBamboo), record);
    TabletMappingPolicy policy(port, watcher, outputs, store);
    QVERIFY(policy.start());
    // KWin maps the workspace with no output transform, so nothing turns.
    QCOMPARE(writesOf(port, QStringLiteral("orientationDBus")), 0);
    QVERIFY(store.held.record(QLatin1String(kBamboo)).placement.rotation.has_value());
}

void TabletOrientationPolicyTest::mixedRotationsBehindTheActiveScreenAreLeftAlone() {
    FakeTabletPort port;
    port.scripted = {bamboo(QString())};
    FakeWatcher watcher;
    FakeOutputs outputs;
    outputs.scripted = {monitor(QStringLiteral("DP-1"), Rotation::None),
                        monitor(QStringLiteral("DP-2"), Rotation::Cw90)};
    FakeStore store;
    TabletMappingPolicy policy(port, watcher, outputs, store);
    QVERIFY(policy.start());
    // The active screen changes per pen event; no single compensation is
    // right, so none is applied (Settings says so and suggests one screen).
    QCOMPARE(writesOf(port, QStringLiteral("orientationDBus")), 0);
    QCOMPARE(store.held.record(QLatin1String(kBamboo)).choice,
             TabletMapChoice::FollowActiveScreen);
}

void TabletOrientationPolicyTest::anUnknownRotationWritesAndRecordsNothing() {
    FakeTabletPort port;
    port.scripted = {bamboo()};
    FakeWatcher watcher;
    FakeOutputs outputs;
    // Display1 has not answered: no rotation is known.
    outputs.scripted = {monitor(QStringLiteral("DP-1"), std::nullopt)};
    FakeStore store;
    TabletMappingPolicy policy(port, watcher, outputs, store);
    QVERIFY(policy.start());
    QVERIFY(port.writes.isEmpty());
    const TabletMappingRecord record = store.held.record(QLatin1String(kBamboo));
    QVERIFY(!record.placement.rotation.has_value());
    QVERIFY(!record.placement.inputArea.has_value());
    QVERIFY(!record.placement.outputArea.has_value());

    // Once Display1 answers, the pass acts.
    outputs.publish({monitor(QStringLiteral("DP-1"), Rotation::Cw180)});
    QCOMPARE(lastWrite(port, QStringLiteral("orientationDBus")).toInt(),
             kwinOrientationFor(Rotation::Cw180));
}

void TabletOrientationPolicyTest::aPenDisplayLosesAStaleRotationOfItsOwn() {
    FakeTabletPort port;
    TabletDeviceSnapshot pen = wacomOne();
    pen.properties.insert(QStringLiteral("orientationDBus"), 1);
    pen.properties.insert(QStringLiteral("leftHanded"), true);
    port.scripted = {pen};
    FakeWatcher watcher;
    FakeOutputs outputs;
    outputs.scripted = {wacomScreen(Rotation::Cw90)};
    FakeStore store;
    TabletMappingPolicy policy(port, watcher, outputs, store);
    QVERIFY(policy.start());
    // It turns with its own screen through KWin's output transform; any
    // rotation of its own would turn it twice.
    QCOMPARE(lastWrite(port, QStringLiteral("orientationDBus")).toInt(), 0);
    QCOMPARE(lastWrite(port, QStringLiteral("leftHanded")).toBool(), false);
    // It stays mapped to its own screen (ADR-0197) and records no intent.
    const TabletMappingRecord record = store.held.record(QLatin1String(kWacomOne));
    QCOMPARE(record.choice, TabletMapChoice::NamedOutput);
    QCOMPARE(record.outputName, QStringLiteral("HDMI-A-1"));
    QVERIFY(!record.placement.rotation.has_value());
}

void TabletOrientationPolicyTest::aRefusedOrientationIsReportedAndRetried() {
    FakeTabletPort port;
    port.scripted = {bamboo()};
    port.refuse = QStringLiteral("orientationDBus");
    FakeWatcher watcher;
    FakeOutputs outputs;
    outputs.scripted = {monitor(QStringLiteral("DP-1"), Rotation::Cw90)};
    FakeStore store;
    TabletMappingPolicy policy(port, watcher, outputs, store);
    QSignalSpy failed(&policy, &TabletMappingPolicy::mappingFailed);
    QVERIFY(policy.start());
    QCOMPARE(failed.size(), 1);
    QVERIFY(failed.at(0).at(0).toString().contains(QStringLiteral("orientationDBus")));
    // Nothing adopted on a failed pass...
    QVERIFY(!store.held.record(QLatin1String(kBamboo)).placement.rotation.has_value());
    // ...so the next pass tries again once the authority accepts.
    port.refuse.clear();
    policy.reconcile();
    QCOMPARE(lastWrite(port, QStringLiteral("orientationDBus")).toInt(),
             kwinOrientationFor(Rotation::Cw270));
}

QTEST_MAIN(TabletOrientationPolicyTest)
#include "tst_tablet_orientation_policy.moc"
