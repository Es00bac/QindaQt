// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/tablet_devices/tablet_placement.h>

#include <QTest>

#include "support/kwin_tablet_pipeline.h"

using namespace QindaQt::Services::TabletDevices;
using namespace QindaQt::Tests::TabletPipeline;

// ADR-0285 planning: for every way the tablet lies and every way the screen
// is turned, the planned KWin values move the pen the way the user sees it,
// against the independent pipeline model in support/kwin_tablet_pipeline.h.

class TabletPlacementTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void upOnTheTabletIsUpOnTheScreenForEveryTurnAndRotation();
    void theWholeWorkspaceIsNeverCompensated();
    void areasMapSeenTabletOntoSeenScreenUnderEveryRotation();
    void aPlanIsAFixedPoint();
    void anUnknownRotationPlansAndRecordsNothing();
    void adoptionOfAnUncompensatedDeviceKeepsWhatTheUserSees();
    void libinputRotationIsPreferredAndClearsAStaleOrientation();
    void aTabletThatCannotTurnKeepsItsOrientation();
    void aPenDisplayLosesEveryStaleRotation();
};

void TabletPlacementTest::upOnTheTabletIsUpOnTheScreenForEveryTurnAndRotation() {
    // All 4 x 4 combinations of how the tablet lies and how the screen is
    // rotated: the seen top and the seen right of the tablet must land on
    // the seen top and the seen right of the screen.
    const std::array<Point, 3> probes{Point{0.5, 0.1}, Point{0.9, 0.5},
                                      Point{0.2, 0.7}};
    for (int user = 0; user < 4; ++user) {
        for (const Rotation output : AllRotations) {
            TabletDeviceSnapshot tool = deskTablet();
            TabletPlacementIntent intent;
            intent.rotation = AllRotations[static_cast<std::size_t>(user)];
            const TabletPlacementPlan plan =
                planDeskTabletPlacement(tool, intent, known(output));
            QVERIFY(plan.actionable);
            applyWrites(plan, &tool);
            const DeviceState state = stateOf(tool);
            for (const Point probe : probes) {
                const Point cursor = cursorFor(probe, user, state, output);
                QVERIFY2(nearPoint(cursor, probe),
                         qPrintable(QStringLiteral("user %1 output %2: %3 -> %4")
                                        .arg(user)
                                        .arg(rotationDegrees(output))
                                        .arg(describe(probe), describe(cursor))));
            }
        }
    }
}

void TabletPlacementTest::theWholeWorkspaceIsNeverCompensated() {
    const QList<TabletOutputCandidate> turned{
        screen(QStringLiteral("DP-1"), Rotation::Cw90)};
    const MappedRotation workspace =
        mappedRotation(TabletMapChoice::EntireWorkspace, {}, turned);
    for (int user = 0; user < 4; ++user) {
        TabletDeviceSnapshot tool = deskTablet();
        TabletPlacementIntent intent;
        intent.rotation = AllRotations[static_cast<std::size_t>(user)];
        applyWrites(planDeskTabletPlacement(tool, intent, workspace), &tool);
        // mapToWorkspace positions the pen with no output transform.
        QCOMPARE(tool.properties.value(QStringLiteral("orientationDBus")).toInt(),
                 kwinOrientationFor(*intent.rotation));
        const Point probe{0.3, 0.2};
        QVERIFY(nearPoint(cursorFor(probe, user, stateOf(tool), Rotation::None),
                          probe));
    }
}

void TabletPlacementTest::areasMapSeenTabletOntoSeenScreenUnderEveryRotation() {
    const TabletArea seenInput{0.1, 0.2, 0.5, 0.6};
    const TabletArea seenOutput{0.4, 0.1, 0.5, 0.3};
    const std::array<Point, 4> probes{Point{0.1, 0.2}, Point{0.6, 0.8},
                                      Point{0.35, 0.5}, Point{0.55, 0.25}};
    for (int user = 0; user < 4; ++user) {
        for (const Rotation output : AllRotations) {
            TabletDeviceSnapshot tool = deskTablet();
            TabletPlacementIntent intent;
            intent.rotation = AllRotations[static_cast<std::size_t>(user)];
            intent.inputArea = seenInput;
            intent.outputArea = seenOutput;
            const TabletPlacementPlan plan =
                planDeskTabletPlacement(tool, intent, known(output));
            applyWrites(plan, &tool);
            const DeviceState state = stateOf(tool);
            // Every written area is one libinput accepts: x2 <= 1 exactly.
            QVERIFY(state.inputArea.x + state.inputArea.width <= 1.0);
            QVERIFY(state.inputArea.y + state.inputArea.height <= 1.0);
            for (const Point probe : probes) {
                const Point expected = place(crop(probe, seenInput), seenOutput);
                const Point cursor = cursorFor(probe, user, state, output);
                QVERIFY2(nearPoint(cursor, expected),
                         qPrintable(QStringLiteral("user %1 output %2: %3 -> %4 "
                                                   "wanted %5")
                                        .arg(user)
                                        .arg(rotationDegrees(output))
                                        .arg(describe(probe), describe(cursor),
                                             describe(expected))));
            }
        }
    }
}

void TabletPlacementTest::aPlanIsAFixedPoint() {
    // The session re-plans on every output change and every ledger change;
    // a device that already matches must see no writes, or the two writers
    // would chatter forever.
    for (int user = 0; user < 4; ++user) {
        for (const Rotation output : AllRotations) {
            TabletDeviceSnapshot tool = deskTablet();
            TabletPlacementIntent intent;
            intent.rotation = AllRotations[static_cast<std::size_t>(user)];
            intent.inputArea = TabletArea{0.1, 0.1, 0.7, 0.3};
            intent.outputArea = TabletArea{0.3, 0.3, 0.6, 0.6};
            const TabletPlacementPlan first =
                planDeskTabletPlacement(tool, intent, known(output));
            applyWrites(first, &tool);
            const TabletPlacementPlan second =
                planDeskTabletPlacement(tool, first.intent, known(output));
            const QString rewritten = second.writes.isEmpty()
                                          ? QString()
                                          : second.writes.first().property;
            QVERIFY2(second.writes.isEmpty(),
                     qPrintable(QStringLiteral("user %1 output %2 rewrote %3")
                                    .arg(user)
                                    .arg(rotationDegrees(output))
                                    .arg(rewritten)));
            QCOMPARE(second.intent, first.intent);
        }
    }
}

void TabletPlacementTest::anUnknownRotationPlansAndRecordsNothing() {
    TabletPlacementIntent recorded;
    recorded.rotation = Rotation::Cw90;
    const TabletPlacementPlan plan =
        planDeskTabletPlacement(deskTablet(), recorded, MappedRotation{});
    QVERIFY(!plan.actionable);
    QVERIFY(plan.writes.isEmpty());
    // Nothing adopted: a guessed frame must not reach the ledger.
    QCOMPARE(plan.intent, recorded);
}

void TabletPlacementTest::adoptionOfAnUncompensatedDeviceKeepsWhatTheUserSees() {
    // A tablet set up before ADR-0285: upright, a custom screen area, mapped
    // to a monitor already turned 90°. Adoption keeps the screen area the
    // user sees and turns the pen so up is up.
    TabletDeviceSnapshot tool = deskTablet();
    tool.properties.insert(QStringLiteral("outputArea"),
                           QVariantList{0.0, 0.0, 0.5, 0.5});
    const TabletPlacementPlan plan =
        planDeskTabletPlacement(tool, TabletPlacementIntent{}, known(Rotation::Cw90));
    QVERIFY(plan.intent.rotation.has_value());
    QCOMPARE(*plan.intent.rotation, Rotation::None);
    // The native top-left quarter is what KWin showed on the turned screen.
    QVERIFY(sameArea(*plan.intent.outputArea,
                     rotateArea(TabletArea{0.0, 0.0, 0.5, 0.5}, Rotation::Cw90)));
    QVERIFY(sameArea(*plan.intent.inputArea, TabletArea{}));
    QCOMPARE(plan.writes.size(), 1);
    QCOMPARE(plan.writes.at(0).property, QStringLiteral("orientationDBus"));
    QCOMPARE(plan.writes.at(0).value.toInt(),
             kwinOrientationFor(Rotation::Cw270));

    // On an unrotated screen the same device needs no write at all.
    const TabletPlacementPlan upright = planDeskTabletPlacement(
        deskTablet(), TabletPlacementIntent{}, known(Rotation::None));
    QVERIFY(upright.writes.isEmpty());
    QVERIFY(sameArea(*upright.intent.outputArea, TabletArea{}));
}

void TabletPlacementTest::libinputRotationIsPreferredAndClearsAStaleOrientation() {
    TabletDeviceSnapshot tool = deskTablet();
    tool.properties.insert(QStringLiteral("supportsRotation"), true);
    tool.properties.insert(QStringLiteral("orientationDBus"), 1);
    QCOMPARE(rotationMechanism(tool), RotationMechanism::LibinputRotation);
    TabletPlacementIntent intent;
    intent.rotation = Rotation::Cw90;
    const TabletPlacementPlan plan =
        planDeskTabletPlacement(tool, intent, known(Rotation::Cw180));
    QCOMPARE(plan.writes.size(), 2);
    QCOMPARE(plan.writes.at(0).property, QStringLiteral("orientationDBus"));
    QCOMPARE(plan.writes.at(0).value.toInt(), 0);
    QCOMPARE(plan.writes.at(1).property, QStringLiteral("rotation"));
    // 90° turned on the desk, 180° screen: libinput turns 270°.
    QCOMPARE(plan.writes.at(1).value.toUInt(), 270U);
}

void TabletPlacementTest::aTabletThatCannotTurnKeepsItsOrientation() {
    TabletDeviceSnapshot tool = deskTablet();
    // libinput with libwacom offers a desk tablet no calibration matrix.
    tool.properties.insert(QStringLiteral("supportsCalibrationMatrix"), false);
    QCOMPARE(rotationMechanism(tool), RotationMechanism::None);
    const TabletPlacementPlan plan = planDeskTabletPlacement(
        tool, TabletPlacementIntent{}, known(Rotation::Cw90));
    for (const TabletPropertyWrite &write : plan.writes) {
        QVERIFY(write.property != QLatin1String("orientationDBus"));
        QVERIFY(write.property != QLatin1String("rotation"));
    }
}

void TabletPlacementTest::aPenDisplayLosesEveryStaleRotation() {
    TabletDeviceSnapshot tool = penDisplay();
    QVERIFY(planPenDisplayPlacement(tool).writes.isEmpty());
    tool.properties.insert(QStringLiteral("orientationDBus"), 1);
    tool.properties.insert(QStringLiteral("leftHanded"), true);
    const TabletPlacementPlan plan = planPenDisplayPlacement(tool);
    QVERIFY(plan.actionable);
    QCOMPARE(plan.writes.size(), 2);
    QCOMPARE(plan.writes.at(0).property, QStringLiteral("orientationDBus"));
    QCOMPARE(plan.writes.at(0).value.toInt(), 0);
    QCOMPARE(plan.writes.at(1).property, QStringLiteral("leftHanded"));
    QCOMPARE(plan.writes.at(1).value.toBool(), false);
    // A value KWin stored but no rotation corresponds to is stale too.
    tool.properties.insert(QStringLiteral("orientationDBus"), 3);
    tool.properties.insert(QStringLiteral("leftHanded"), false);
    QCOMPARE(planPenDisplayPlacement(tool).writes.size(), 1);
}

QTEST_MAIN(TabletPlacementTest)
#include "tst_tablet_placement.moc"
