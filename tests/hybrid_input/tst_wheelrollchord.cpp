// SPDX-License-Identifier: GPL-3.0-or-later
//
// ADR-0282: modifier + wheel rolls the container under the pointer up (away
// from the user) or down, once per scroll gesture.

#include "qindaqt/hybrid_input/wheelrollchord.h"

#include <QtTest>

using namespace QindaQt::HybridInput;

namespace {

// One classic wheel notch away from the user: KWin reports delta -15 and
// deltaV120 -120 unless natural scrolling inverted it.
WheelSample notchAway(qint64 timeMs, Qt::KeyboardModifiers modifiers = Qt::MetaModifier)
{
    return {.timeMs = timeMs, .modifiers = modifiers, .delta = -15.0, .deltaV120 = -120,
            .inverted = false, .targetUnderPointer = true};
}

WheelSample notchTowards(qint64 timeMs)
{
    return {.timeMs = timeMs, .modifiers = Qt::MetaModifier, .delta = 15.0,
            .deltaV120 = 120, .inverted = false, .targetUnderPointer = true};
}

} // namespace

class WheelRollChordTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void notchDirectionPicksRollUpOrDown();
    void naturalScrollingKeepsThePhysicalDirection();
    void firesOncePerGestureSoASpinDoesNotFlap();
    void highResolutionWheelAccumulatesToOneNotch();
    void touchpadAccumulatesAndItsStopEndsTheGesture();
    void jitterTheOtherWayNeverAddsUp();
    void plainWheelIsLeftToTheTabStripAndClients();
    void nothingUnderThePointerLeavesKWinItsMetaWheel();
    void rebindingOrDisablingTheModifier();
};

void WheelRollChordTest::notchDirectionPicksRollUpOrDown()
{
    WheelRollChord chord;
    const auto up = chord.feed(notchAway(0));
    QVERIFY(up.consumed);
    QCOMPARE(up.roll, std::optional(RollDirection::Up));
    const auto down = chord.feed(notchTowards(1000));
    QVERIFY(down.consumed);
    QCOMPARE(down.roll, std::optional(RollDirection::Down));
}

void WheelRollChordTest::naturalScrollingKeepsThePhysicalDirection()
{
    WheelRollChord chord;
    auto sample = notchTowards(0);
    sample.inverted = true;
    QCOMPARE(chord.feed(sample).roll, std::optional(RollDirection::Up));
}

void WheelRollChordTest::firesOncePerGestureSoASpinDoesNotFlap()
{
    WheelRollChord chord;
    QCOMPARE(chord.feed(notchAway(0)).roll, std::optional(RollDirection::Up));
    for (qint64 time = 40; time <= 200; time += 40) {
        const auto more = chord.feed(notchAway(time));
        QVERIFY(more.consumed);
        QVERIFY(!more.roll.has_value());
    }
    // Reversing inside the same gesture does not flap it back down...
    QVERIFY(!chord.feed(notchTowards(240)).roll.has_value());
    // ...but after a pause the next notch is a new gesture.
    QCOMPARE(chord.feed(notchTowards(240 + WheelRollChord::GesturePauseMs + 1)).roll,
             std::optional(RollDirection::Down));
}

void WheelRollChordTest::highResolutionWheelAccumulatesToOneNotch()
{
    WheelRollChord chord;
    for (int step = 0; step != 3; ++step) {
        const auto partial = chord.feed({.timeMs = step * 10, .modifiers = Qt::MetaModifier,
                                         .delta = -3.75, .deltaV120 = -30,
                                         .targetUnderPointer = true});
        QVERIFY(partial.consumed);
        QVERIFY(!partial.roll.has_value());
    }
    QCOMPARE(chord.feed({.timeMs = 30, .modifiers = Qt::MetaModifier, .delta = -3.75,
                         .deltaV120 = -30, .targetUnderPointer = true})
                 .roll,
             std::optional(RollDirection::Up));
}

void WheelRollChordTest::touchpadAccumulatesAndItsStopEndsTheGesture()
{
    WheelRollChord chord;
    // Finger scrolling: no v120, scroll units on the wheel-degree scale.
    const auto finger = [](qint64 time, qreal delta) {
        return WheelSample{.timeMs = time, .modifiers = Qt::MetaModifier, .delta = delta,
                           .deltaV120 = 0, .targetUnderPointer = true};
    };
    QVERIFY(!chord.feed(finger(0, -6.0)).roll.has_value());
    QVERIFY(!chord.feed(finger(10, -6.0)).roll.has_value());
    QCOMPARE(chord.feed(finger(20, -6.0)).roll, std::optional(RollDirection::Up));
    QVERIFY(!chord.feed(finger(30, -20.0)).roll.has_value());
    // The touchpad's zero-delta stop event ends the gesture at once.
    QVERIFY(chord.feed(finger(40, 0.0)).consumed);
    QCOMPARE(chord.feed(finger(50, 16.0)).roll, std::optional(RollDirection::Down));
}

void WheelRollChordTest::jitterTheOtherWayNeverAddsUp()
{
    WheelRollChord chord;
    const auto sample = [](qint64 time, qint32 v120) {
        return WheelSample{.timeMs = time, .modifiers = Qt::MetaModifier,
                           .delta = v120 / 8.0, .deltaV120 = v120,
                           .targetUnderPointer = true};
    };
    QVERIFY(!chord.feed(sample(0, -100)).roll.has_value());
    QVERIFY(!chord.feed(sample(10, 30)).roll.has_value());
    // The earlier -100 was discarded by the reversal, so -100 again is still
    // short of a notch.
    QVERIFY(!chord.feed(sample(20, -100)).roll.has_value());
    QCOMPARE(chord.feed(sample(30, -20)).roll, std::optional(RollDirection::Up));
}

// Lane 1 scrolls the Corner Bar's card-deck tabs with the plain wheel; the
// modifier wheel must win over it and the plain wheel must reach it.
void WheelRollChordTest::plainWheelIsLeftToTheTabStripAndClients()
{
    WheelRollChord chord;
    const auto plain = chord.feed(notchAway(0, Qt::NoModifier));
    QVERIFY(!plain.consumed);
    QVERIFY(!plain.roll.has_value());
    const auto withShift = chord.feed(notchAway(10, Qt::MetaModifier | Qt::ShiftModifier));
    QVERIFY(!withShift.consumed);
    QVERIFY(chord.feed(notchAway(20)).consumed);
}

void WheelRollChordTest::nothingUnderThePointerLeavesKWinItsMetaWheel()
{
    WheelRollChord chord;
    auto sample = notchAway(0);
    sample.targetUnderPointer = false;
    const auto decision = chord.feed(sample);
    QVERIFY(!decision.consumed);
    QVERIFY(!decision.roll.has_value());
}

void WheelRollChordTest::rebindingOrDisablingTheModifier()
{
    WheelRollChord chord(Qt::AltModifier);
    QVERIFY(!chord.feed(notchAway(0)).consumed);
    QCOMPARE(chord.feed(notchAway(1000, Qt::AltModifier)).roll,
             std::optional(RollDirection::Up));
    chord.setModifier(Qt::NoModifier);
    QVERIFY(!chord.feed(notchAway(2000, Qt::NoModifier)).consumed);
    QVERIFY(!chord.feed(notchAway(3000, Qt::AltModifier)).consumed);
}

QTEST_GUILESS_MAIN(WheelRollChordTest)
#include "tst_wheelrollchord.moc"
