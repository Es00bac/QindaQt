// SPDX-License-Identifier: GPL-3.0-or-later
//
// ADR-0384 output-removal rescue, model level. A nested KWin cannot place a
// client window on a virtual output and then unplug it under test today: the
// development output seam adds and removes outputs, but no harness probe can
// map a client onto the added output. So the rescue is tested where it lives:
// the pure frame policy every caller uses (independent restore states, the
// session's backstop pass, saved container frames) and the placement
// controller's saved-frame refresh, driven with the live failure's numbers.

#include "hybridactivationpolicy.h"
#include "hybridcontainerplacement_fixture.h"
#include "hybridoutputrescue.h"

#include <QtTest>

using namespace QindaQt;
using namespace QindaQt::Compositor::KWinIntegration;

namespace {

// The session the failure was found on: a 1536x864 eDP-1 alone, after an
// HDMI output at x = 1536 (2560x1440, 32 px top panel) was unplugged.
const QVector<RescueOutput> EdpOnly{{QRect(0, 0, 1536, 864), QRect(0, 32, 1536, 832)}};
const QRect HdmiMaximized(1536, 32, 2560, 1408);

} // namespace

class OutputRescueTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void bringsTheLiveStrandedWindowBack();
    void leavesAReachableFrameAlone();
    void keepsTheSizeWhenItFitsAndClampsWhenItCannot();
    void picksTheNearestRemainingOutput();
    void neverRescuesWithoutOutputs();
    void movesAMaximizeRestoreFrameOffTheRemovedOutput();
    void movesARolledStripOffTheRemovedOutput();
    void activatesAVisibleMemberInsteadOfAHiddenOne();
};

void OutputRescueTest::bringsTheLiveStrandedWindowBack()
{
    QVERIFY(isStranded(QRectF(HdmiMaximized), EdpOnly));
    const auto rescued = rescuedFrame(HdmiMaximized, EdpOnly);
    QVERIFY(rescued);
    // Too large for the remaining work area: clamped to it entirely.
    QCOMPARE(*rescued, QRect(0, 32, 1536, 832));
    QVERIFY(!isStranded(QRectF(*rescued), EdpOnly));
}

void OutputRescueTest::leavesAReachableFrameAlone()
{
    // Hanging off the right edge is the user's own arrangement.
    QVERIFY(!rescuedFrame(QRect(1400, 100, 800, 600), EdpOnly));
    QVERIFY(!rescuedFrame(QRectF(10.5, 40.25, 300.0, 200.0), EdpOnly));
    // Touching the edge without overlapping is not reachable.
    QVERIFY(rescuedFrame(QRect(1536, 100, 400, 300), EdpOnly));
}

void OutputRescueTest::keepsTheSizeWhenItFitsAndClampsWhenItCannot()
{
    const auto rescued = rescuedFrame(QRect(2000, 300, 800, 500), EdpOnly);
    QVERIFY(rescued);
    QCOMPARE(rescued->size(), QSize(800, 500));
    QCOMPARE(*rescued, QRect(736, 300, 800, 500));
    const auto tall = rescuedFrame(QRect(1600, -2000, 400, 3000), EdpOnly);
    QVERIFY(tall);
    QCOMPARE(*tall, QRect(1136, 32, 400, 832));
    const auto fractional = rescuedFrame(QRectF(1600.4, 100.6, 300.2, 200.2), EdpOnly);
    QVERIFY(fractional);
    QCOMPARE(*fractional, QRectF(1236.0, 101.0, 300.0, 200.0));
}

void OutputRescueTest::picksTheNearestRemainingOutput()
{
    const QVector<RescueOutput> twoLeft{
        {QRect(0, 0, 1920, 1080), QRect(0, 0, 1920, 1040)},
        {QRect(-1280, 0, 1280, 1024), QRect(-1280, 0, 1280, 1024)}};
    // Stranded far left: lands on the left output.
    const auto left = rescuedFrame(QRect(-4000, 200, 600, 400), twoLeft);
    QVERIFY(left);
    QCOMPARE(*left, QRect(-1280, 200, 600, 400));
    // Stranded far right: lands on the right edge of the primary output.
    const auto right = rescuedFrame(QRect(5000, 200, 600, 400), twoLeft);
    QVERIFY(right);
    QCOMPARE(*right, QRect(1320, 200, 600, 400));
    // An invalid work area falls back to the output geometry.
    const QVector<RescueOutput> noStruts{{QRect(0, 0, 1000, 800), QRect()}};
    QCOMPARE(*rescuedFrame(QRect(3000, 3000, 100, 100), noStruts), QRect(900, 700, 100, 100));
}

void OutputRescueTest::neverRescuesWithoutOutputs()
{
    QVERIFY(!isStranded(QRectF(HdmiMaximized), {}));
    QVERIFY(!rescuedFrame(HdmiMaximized, {}));
    QVERIFY(!rescuedFrame(QRect(), EdpOnly));
}

void OutputRescueTest::movesAMaximizeRestoreFrameOffTheRemovedOutput()
{
    PlacementFixtures::Fixture fixture;
    // The container lives on the HDMI output and is maximized there; its
    // restore frame is HDMI-only too.
    fixture.layout = PlacementFixtures::sampleLayout();
    fixture.layout.outerFrame.moveTo(1700, 200);
    fixture.layout.activePage.outerFrame = fixture.layout.outerFrame;
    fixture.workArea = QRect(1536, 32, 2560, 1408);
    QVERIFY(fixture.controller.maximize(QStringLiteral("group")));
    QVERIFY(fixture.controller.isMaximized(QStringLiteral("group")));

    const auto moved = fixture.controller.refreshStrandedSavedFrames(EdpOnly);
    QCOMPARE(moved, QStringList{QStringLiteral("group")});
    // Restore now lands on eDP-1, at the restore frame's own size.
    fixture.workArea = QRect(0, 32, 1536, 832);
    fixture.requestedFrames.clear();
    QVERIFY(fixture.controller.restore(QStringLiteral("group")));
    QCOMPARE(fixture.requestedFrames.size(), 1);
    QVERIFY(!isStranded(QRectF(fixture.requestedFrames.constFirst()), EdpOnly));
    QCOMPARE(fixture.requestedFrames.constFirst().size(), QSize(800, 600));
    // A second pass has nothing left to move.
    QVERIFY(fixture.controller.refreshStrandedSavedFrames(EdpOnly).isEmpty());
}

void OutputRescueTest::movesARolledStripOffTheRemovedOutput()
{
    PlacementFixtures::Fixture fixture;
    fixture.layout.outerFrame.moveTo(2000, 300);
    fixture.layout.activePage.outerFrame = fixture.layout.outerFrame;
    QVERIFY(fixture.controller.shade(QStringLiteral("group")));
    const auto strip = fixture.controller.shadedFrame(QStringLiteral("group"));
    QVERIFY(strip);
    QVERIFY(isStranded(QRectF(*strip), EdpOnly));
    const int changesBefore = fixture.changedCount;

    QCOMPARE(fixture.controller.refreshStrandedSavedFrames(EdpOnly),
             QStringList{QStringLiteral("group")});
    const auto rescued = fixture.controller.shadedFrame(QStringLiteral("group"));
    QVERIFY(rescued);
    QVERIFY(!isStranded(QRectF(*rescued), EdpOnly));
    QCOMPARE(rescued->size(), strip->size());
    // Chrome follows the strip through the changed sink.
    QVERIFY(fixture.changedCount > changesBefore);
}

void OutputRescueTest::activatesAVisibleMemberInsteadOfAHiddenOne()
{
    const QStringList page{QStringLiteral("left"), QStringLiteral("right")};
    QSet<QString> shown{QStringLiteral("right")};
    const auto isShown = [&shown](const QString &id) { return shown.contains(id); };
    // The anchor is preferred while it is shown...
    QCOMPARE(visibleMemberToActivate(page, QStringLiteral("right"), isShown),
             std::optional<QString>(QStringLiteral("right")));
    // ...otherwise the first shown member of the active page.
    QCOMPARE(visibleMemberToActivate(page, QStringLiteral("left"), isShown),
             std::optional<QString>(QStringLiteral("right")));
    // A container that shows nothing hands focus elsewhere.
    shown.clear();
    QVERIFY(!visibleMemberToActivate(page, QStringLiteral("left"), isShown));
    // A preferred id outside the active page is never chosen.
    shown = {QStringLiteral("other"), QStringLiteral("left")};
    QCOMPARE(visibleMemberToActivate(page, QStringLiteral("other"), isShown),
             std::optional<QString>(QStringLiteral("left")));
}

QTEST_GUILESS_MAIN(OutputRescueTest)
#include "tst_outputrescue.moc"
