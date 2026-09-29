// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/tablet_devices/display_tablet_outputs.h>
#include <qindaqt/services/tablet_devices/tablet_classification.h>

#include <QTest>

#include "support/kwin_tablet_pipeline.h"

using namespace QindaQt::Services::TabletDevices;
using namespace QindaQt::Tests::TabletPipeline;

// ADR-0285 values: the rotation algebra, KWin's orientation table, how an
// area turns with the square, which tablet is a screen, which rotation KWin
// applies for each mapping, and the Display1 join that supplies it.

class TabletOrientationValuesTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void rotationAlgebraIsTheGroupOfQuarterTurns();
    void kwinOrientationValuesFollowKWinsMatrixTable();
    void rotateAreaTurnsTheWholeSquare();
    void libinputsVerdictClassifiesBeforeAnyHeuristic();
    void theOutputMatcherDecidesOnlyWithoutLibinputsVerdict();
    void mappedRotationMirrorsKWinsThreeBranches();
    void displayTransformsDropTheFlipLikeKWin();
    void display1RotationsJoinByConnector();
};

void TabletOrientationValuesTest::rotationAlgebraIsTheGroupOfQuarterTurns() {
    for (const Rotation first : AllRotations) {
        QCOMPARE(composeRotations(first, inverseRotation(first)), Rotation::None);
        QCOMPARE(rotationFromDegrees(rotationDegrees(first)).value(), first);
        for (const Rotation second : AllRotations) {
            // Quarter turns of a square commute.
            QCOMPARE(composeRotations(first, second),
                     composeRotations(second, first));
        }
    }
    QCOMPARE(composeRotations(Rotation::Cw90, Rotation::Cw270), Rotation::None);
    QCOMPARE(composeRotations(Rotation::Cw180, Rotation::Cw270), Rotation::Cw90);
    QCOMPARE(inverseRotation(Rotation::Cw90), Rotation::Cw270);
    QVERIFY(swapsAxes(Rotation::Cw90) && swapsAxes(Rotation::Cw270));
    QVERIFY(!swapsAxes(Rotation::None) && !swapsAxes(Rotation::Cw180));
    // Only the four exact values are rotations a record may carry.
    QVERIFY(!rotationFromDegrees(360).has_value());
    QVERIFY(!rotationFromDegrees(-90).has_value());
    QVERIFY(!rotationFromDegrees(45).has_value());
}

void TabletOrientationValuesTest::kwinOrientationValuesFollowKWinsMatrixTable() {
    // Qt::ScreenOrientation: Primary 0, Portrait 1, Landscape 2,
    // InvertedPortrait 4, InvertedLandscape 8.
    QCOMPARE(kwinOrientationFor(Rotation::None), 0);
    QCOMPARE(kwinOrientationFor(Rotation::Cw90), 1);
    QCOMPARE(kwinOrientationFor(Rotation::Cw180), 8);
    QCOMPARE(kwinOrientationFor(Rotation::Cw270), 4);
    QCOMPARE(rotationFromKWinOrientation(2).value(), Rotation::None);
    for (const Rotation rotation : AllRotations) {
        QCOMPARE(rotationFromKWinOrientation(kwinOrientationFor(rotation)).value(),
                 rotation);
        // And each value turns the square the way its Rotation says.
        const Point corner = kwinOrientation(kwinOrientationFor(rotation),
                                             Point{0.0, 0.0});
        const TabletArea turned =
            rotateArea(TabletArea{0.0, 0.0, 0.0, 0.0}, rotation);
        QVERIFY2(nearPoint(corner, Point{turned.x, turned.y}),
                 qPrintable(describe(corner)));
    }
    // KWin would persist these and apply no rotation; never trusted.
    QVERIFY(!rotationFromKWinOrientation(3).has_value());
    QVERIFY(!rotationFromKWinOrientation(-1).has_value());
}

void TabletOrientationValuesTest::rotateAreaTurnsTheWholeSquare() {
    const TabletArea leftStrip{0.0, 0.0, 0.25, 1.0};
    // A quarter turn clockwise carries the left edge to the top.
    const TabletArea turned = rotateArea(leftStrip, Rotation::Cw90);
    QVERIFY(near(turned.x, 0.0) && near(turned.y, 0.0) &&
            near(turned.width, 1.0) && near(turned.height, 0.25));
    const TabletArea half = rotateArea(leftStrip, Rotation::Cw180);
    QVERIFY(near(half.x, 0.75) && near(half.y, 0.0) && near(half.width, 0.25));
    const TabletArea three = rotateArea(leftStrip, Rotation::Cw270);
    QVERIFY(near(three.x, 0.0) && near(three.y, 0.75) &&
            near(three.width, 1.0) && near(three.height, 0.25));
    const TabletArea odd{0.1, 0.2, 0.3, 0.4};
    for (const Rotation rotation : AllRotations) {
        const TabletArea back =
            rotateArea(rotateArea(odd, rotation), inverseRotation(rotation));
        QVERIFY(sameArea(back, odd));
        // Whatever turns, it stays usable once normalized.
        QVERIFY(normalizedArea(rotateArea(odd, rotation)).has_value());
    }
}

void TabletOrientationValuesTest::libinputsVerdictClassifiesBeforeAnyHeuristic() {
    const QList<TabletOutputCandidate> wacomScreen{
        TabletOutputCandidate{QStringLiteral("HDMI-A-1"),
                              QStringLiteral("Wacom Technology Corp."),
                              QStringLiteral("Wacom One 13"),
                              QStringLiteral("Wacom One 13"), false, true}};
    // libinput offers an area only to an indirect tablet; that wins even
    // over a Wacom screen the name matcher would call its own.
    TabletDeviceSnapshot desk = deskTablet();
    const TabletClassification deskKind = classifyTablet(desk, wacomScreen);
    QCOMPARE(deskKind.kind, TabletKind::DeskTablet);
    QCOMPARE(deskKind.evidence, TabletKindEvidence::LibinputDirectness);

    const TabletClassification displayKind = classifyTablet(penDisplay(), {});
    QCOMPARE(displayKind.kind, TabletKind::PenDisplay);
    QCOMPARE(displayKind.evidence, TabletKindEvidence::LibinputDirectness);

    // A pad reports no area because it has none, not because it is a screen.
    TabletDeviceSnapshot pad = deskTablet();
    pad.tabletTool = false;
    pad.tabletPad = true;
    pad.properties.insert(QStringLiteral("supportsInputArea"), false);
    QCOMPARE(classifyTablet(pad, {}).kind, TabletKind::DeskTablet);
}

void TabletOrientationValuesTest::theOutputMatcherDecidesOnlyWithoutLibinputsVerdict() {
    TabletDeviceSnapshot older = penDisplay();
    older.properties.remove(QStringLiteral("supportsInputArea"));
    const QList<TabletOutputCandidate> wacomScreen{
        TabletOutputCandidate{QStringLiteral("HDMI-A-1"), QStringLiteral("WAC"),
                              QStringLiteral("One 13"), QStringLiteral("One 13"),
                              false, true}};
    const TabletClassification matched = classifyTablet(older, wacomScreen);
    QCOMPARE(matched.kind, TabletKind::PenDisplay);
    QCOMPARE(matched.evidence, TabletKindEvidence::OwnScreen);
    const TabletClassification unmatched =
        classifyTablet(older, {screen(QStringLiteral("DP-1"), Rotation::None)});
    QCOMPARE(unmatched.kind, TabletKind::DeskTablet);
    QCOMPARE(unmatched.evidence, TabletKindEvidence::Default);
}

void TabletOrientationValuesTest::mappedRotationMirrorsKWinsThreeBranches() {
    const QList<TabletOutputCandidate> mixed{
        screen(QStringLiteral("eDP-1"), Rotation::None),
        screen(QStringLiteral("DP-1"), Rotation::Cw90)};
    // The workspace has no transform at all.
    QCOMPARE(mappedRotation(TabletMapChoice::EntireWorkspace, {}, mixed),
             known(Rotation::None));
    // A named screen is that screen's rotation.
    QCOMPARE(mappedRotation(TabletMapChoice::NamedOutput, QStringLiteral("DP-1"),
                            mixed),
             known(Rotation::Cw90));
    // The active screen differs per pen event: no single answer.
    const MappedRotation follow =
        mappedRotation(TabletMapChoice::FollowActiveScreen, {}, mixed);
    QCOMPARE(follow.state, MappedRotation::State::Mixed);
    QCOMPARE(follow.compensation(), Rotation::None);
    // A named screen that is absent falls back to the active one, as KWin does.
    QCOMPARE(mappedRotation(TabletMapChoice::NamedOutput,
                            QStringLiteral("HDMI-A-9"), mixed)
                 .state,
             MappedRotation::State::Mixed);
    // Every candidate agreeing is known.
    const QList<TabletOutputCandidate> turned{
        screen(QStringLiteral("DP-1"), Rotation::Cw180),
        screen(QStringLiteral("DP-2"), Rotation::Cw180)};
    QCOMPARE(mappedRotation(TabletMapChoice::FollowActiveScreen, {}, turned),
             known(Rotation::Cw180));
    // One unknown rotation, or no screen at all, is unknown, never upright.
    const QList<TabletOutputCandidate> unknown{
        screen(QStringLiteral("DP-1"), Rotation::None),
        screen(QStringLiteral("DP-2"), std::nullopt)};
    QCOMPARE(mappedRotation(TabletMapChoice::FollowActiveScreen, {}, unknown)
                 .state,
             MappedRotation::State::Unknown);
    QCOMPARE(mappedRotation(TabletMapChoice::NamedOutput, QStringLiteral("DP-2"),
                            unknown)
                 .state,
             MappedRotation::State::Unknown);
    QCOMPARE(mappedRotation(TabletMapChoice::FollowActiveScreen, {}, {}).state,
             MappedRotation::State::Unknown);
    // A disabled output is not a candidate.
    TabletOutputCandidate off = screen(QStringLiteral("DP-3"), Rotation::Cw90);
    off.enabled = false;
    QCOMPARE(mappedRotation(TabletMapChoice::FollowActiveScreen, {},
                            {screen(QStringLiteral("DP-1"), Rotation::None), off}),
             known(Rotation::None));
}

void TabletOrientationValuesTest::displayTransformsDropTheFlipLikeKWin() {
    using QindaQt::Display::Transform;
    QCOMPARE(rotationForDisplayTransform(Transform::Normal), Rotation::None);
    QCOMPARE(rotationForDisplayTransform(Transform::Rotate90), Rotation::Cw90);
    QCOMPARE(rotationForDisplayTransform(Transform::Rotate180), Rotation::Cw180);
    QCOMPARE(rotationForDisplayTransform(Transform::Rotate270), Rotation::Cw270);
    QCOMPARE(rotationForDisplayTransform(Transform::FlipX), Rotation::None);
    QCOMPARE(rotationForDisplayTransform(Transform::FlipX90), Rotation::Cw90);
    QCOMPARE(rotationForDisplayTransform(Transform::FlipX180), Rotation::Cw180);
    QCOMPARE(rotationForDisplayTransform(Transform::FlipX270), Rotation::Cw270);
}

void TabletOrientationValuesTest::display1RotationsJoinByConnector() {
    TabletOutputCandidate hdmi = screen(QStringLiteral("HDMI-A-1"), std::nullopt,
                                        QRectF(0, 0, 1920, 1080));
    TabletOutputCandidate edp = screen(QStringLiteral("eDP-1"), std::nullopt,
                                       QRectF(1920, 0, 1280, 800));
    // Without a snapshot nothing is known and the screens are untouched.
    const QList<TabletOutputCandidate> bare =
        withDisplayRotations({hdmi, edp}, std::nullopt);
    QVERIFY(!bare.at(0).rotation.has_value());
    QCOMPARE(bare.at(1).logicalGeometry, QRectF(1920, 0, 1280, 800));

    QindaQt::Display::Snapshot snapshot;
    QindaQt::Display::Output turned;
    turned.connectorName = QStringLiteral("HDMI-A-1");
    turned.enabled = true;
    turned.transform = QindaQt::Display::Transform::Rotate90;
    turned.position = QPoint(0, 0);
    turned.logicalSize = QSize(1080, 1920);
    QindaQt::Display::Output off;
    off.connectorName = QStringLiteral("eDP-1");
    off.enabled = false;
    off.transform = QindaQt::Display::Transform::Rotate180;
    snapshot.outputs = {turned, off};

    const QList<TabletOutputCandidate> joined =
        withDisplayRotations({hdmi, edp}, snapshot);
    QVERIFY(joined.at(0).rotation.has_value());
    QCOMPARE(*joined.at(0).rotation, Rotation::Cw90);
    // Display1's logical size is the rotated shape the user sees.
    QCOMPARE(joined.at(0).logicalGeometry, QRectF(0, 0, 1080, 1920));
    // A connector Display1 does not list as enabled stays unknown.
    QVERIFY(!joined.at(1).rotation.has_value());
}

QTEST_MAIN(TabletOrientationValuesTest)
#include "tst_tablet_orientation_values.moc"
