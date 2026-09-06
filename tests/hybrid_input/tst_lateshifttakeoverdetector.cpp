// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/hybrid_input/lateshifttakeoverdetector.h"

#include <QTest>

using namespace QindaQt::HybridInput;

namespace {
int windowA = 0;
int windowB = 0;
} // namespace

class LateShiftTakeoverDetectorTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void triggersOnlyWhenShiftIsAddedMidDrag();
    void plainDragWithoutShiftNeverTriggers();
    void doesNotRetriggerAfterInitialSatisfaction();
    void dropAndReAddIsANewSatisfiedTransition();
    void aNewDragResetsTrackedModifiers();
    void noActiveDragNeverTriggers();
    void resizeCallerNeverPassesAnIdentitySoItNeverTriggers();
    void extraUnrelatedModifierNeverArmsTakeover();
};

void LateShiftTakeoverDetectorTest::triggersOnlyWhenShiftIsAddedMidDrag()
{
    LateShiftTakeoverDetector detector(Qt::MetaModifier | Qt::ShiftModifier);

    // Plain Meta-only drag starts (KWin resets tracked modifiers to empty at
    // the start of every native move, so the first observation here is
    // Qt::NoModifier even though Meta is what started the press).
    QVERIFY(!detector.observe(&windowA, Qt::NoModifier));
    QVERIFY(!detector.observe(&windowA, Qt::MetaModifier));
    QVERIFY(!detector.observe(&windowA, Qt::MetaModifier));

    // Shift is added mid-drag: this is the one event to react to.
    QVERIFY(detector.observe(&windowA, Qt::MetaModifier | Qt::ShiftModifier));
}

void LateShiftTakeoverDetectorTest::plainDragWithoutShiftNeverTriggers()
{
    LateShiftTakeoverDetector detector(Qt::MetaModifier | Qt::ShiftModifier);

    QVERIFY(!detector.observe(&windowA, Qt::NoModifier));
    QVERIFY(!detector.observe(&windowA, Qt::MetaModifier));
    QVERIFY(!detector.observe(&windowA, Qt::MetaModifier));
    QVERIFY(!detector.observe(nullptr, Qt::NoModifier));
}

void LateShiftTakeoverDetectorTest::doesNotRetriggerAfterInitialSatisfaction()
{
    LateShiftTakeoverDetector detector(Qt::MetaModifier | Qt::ShiftModifier);

    QVERIFY(!detector.observe(&windowA, Qt::MetaModifier));
    QVERIFY(detector.observe(&windowA, Qt::MetaModifier | Qt::ShiftModifier));
    // Still the same drag, chord still held: must not fire again on every
    // further event a caller happens to still route through (a real caller
    // stops - the native move it cancelled on the trigger no longer exists -
    // but the detector itself must be safe against being asked anyway).
    QVERIFY(!detector.observe(&windowA, Qt::MetaModifier | Qt::ShiftModifier));
    QVERIFY(!detector.observe(&windowA, Qt::MetaModifier | Qt::ShiftModifier));
}

void LateShiftTakeoverDetectorTest::dropAndReAddIsANewSatisfiedTransition()
{
    // Not a caller scenario this class needs to special-case (a real caller
    // stops reporting a window as the active drag once its native move ends
    // at the trigger) - this only pins down that dropping the chord and
    // re-adding it within the *same* tracked identity is treated as a fresh
    // transition, since the class has no notion of "already took over" of
    // its own beyond comparing this call's modifiers to the last one.
    LateShiftTakeoverDetector detector(Qt::MetaModifier | Qt::ShiftModifier);

    QVERIFY(!detector.observe(&windowA, Qt::MetaModifier));
    QVERIFY(detector.observe(&windowA, Qt::MetaModifier | Qt::ShiftModifier));
    QVERIFY(!detector.observe(&windowA, Qt::MetaModifier));
    QVERIFY(detector.observe(&windowA, Qt::MetaModifier | Qt::ShiftModifier));
}

void LateShiftTakeoverDetectorTest::aNewDragResetsTrackedModifiers()
{
    LateShiftTakeoverDetector detector(Qt::MetaModifier | Qt::ShiftModifier);

    QVERIFY(!detector.observe(&windowA, Qt::MetaModifier));
    QVERIFY(detector.observe(&windowA, Qt::MetaModifier | Qt::ShiftModifier));

    // A different window starting its own drag is a brand-new session, even
    // if the chord is already fully held from that drag's very first event -
    // a caller must still react (KWin's own claim on an exact-chord press
    // already prevents a native move from ever starting with the chord held
    // from the start; this only defends the assumption rather than relying
    // on it silently).
    QVERIFY(detector.observe(&windowB, Qt::MetaModifier | Qt::ShiftModifier));
}

void LateShiftTakeoverDetectorTest::noActiveDragNeverTriggers()
{
    LateShiftTakeoverDetector detector(Qt::MetaModifier | Qt::ShiftModifier);

    QVERIFY(!detector.observe(nullptr, Qt::NoModifier));
    QVERIFY(!detector.observe(nullptr, Qt::MetaModifier | Qt::ShiftModifier));
}

void LateShiftTakeoverDetectorTest::resizeCallerNeverPassesAnIdentitySoItNeverTriggers()
{
    // A caller must pass nullptr for a resize (or any non-competing native
    // operation) rather than a window identity - this only proves the
    // detector itself stays inert whenever the caller does so; the resize
    // vs. move distinction is the KWin glue's job, not this class's.
    LateShiftTakeoverDetector detector(Qt::MetaModifier | Qt::ShiftModifier);
    QVERIFY(!detector.observe(nullptr, Qt::MetaModifier));
    QVERIFY(!detector.observe(nullptr, Qt::MetaModifier | Qt::ShiftModifier));
}

void LateShiftTakeoverDetectorTest::extraUnrelatedModifierNeverArmsTakeover()
{
    // Regression: a native Meta drag that then gains Ctrl+Shift (an unrelated
    // accessibility/user chord layered on top of the still-held Meta) must
    // never arm the takeover, since the resulting modifiers are a superset
    // of the required Meta+Shift chord, not an exact match - mirroring
    // InteractionController::pointerBindingMatches's exact equality. A
    // subset test would wrongly consume this event and cancel/adopt the
    // native move.
    LateShiftTakeoverDetector detector(Qt::MetaModifier | Qt::ShiftModifier);

    QVERIFY(!detector.observe(&windowA, Qt::NoModifier));
    QVERIFY(!detector.observe(&windowA, Qt::MetaModifier));
    QVERIFY(!detector.observe(&windowA,
                              Qt::MetaModifier | Qt::ShiftModifier | Qt::ControlModifier));
    QVERIFY(!detector.observe(&windowA,
                              Qt::MetaModifier | Qt::ShiftModifier | Qt::ControlModifier));

    // Dropping the unrelated Control modifier down to the exact chord is
    // still a real, reachable late-Shift-style transition and must fire
    // normally - the exact-match fix must not break the ordinary case.
    QVERIFY(detector.observe(&windowA, Qt::MetaModifier | Qt::ShiftModifier));
}

QTEST_GUILESS_MAIN(LateShiftTakeoverDetectorTest)

#include "tst_lateshifttakeoverdetector.moc"
