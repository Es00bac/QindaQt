// SPDX-License-Identifier: GPL-3.0-or-later
#include "held_input_state_p.h"
#include <QtTest>
#include <linux/input-event-codes.h>
using namespace QindaQt::Controllers;
class HeldInputStateTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void mouseChordOrdersModifiersAroundDrag() {
        HeldInputState state;
        const QList<InputChange> press{{InputChange::Key, KEY_LEFTMETA, true},
            {InputChange::Key, KEY_LEFTSHIFT, true}, {InputChange::Mouse, BTN_LEFT, true}};
        QCOMPARE(state.press("pad/r2", {KEY_LEFTMETA, KEY_LEFTSHIFT}, BTN_LEFT), press);
        QVERIFY(state.press("pad/r2", {KEY_LEFTMETA, KEY_LEFTSHIFT}, BTN_LEFT).isEmpty());
        const QList<InputChange> release{{InputChange::Mouse, BTN_LEFT, false},
            {InputChange::Key, KEY_LEFTSHIFT, false}, {InputChange::Key, KEY_LEFTMETA, false}};
        QCOMPARE(state.release("pad/r2"), release);
        QVERIFY(state.tokens().isEmpty());
    }
    void simultaneousTriggersShareModifiersUntilLastRelease() {
        HeldInputState state;
        state.press("pad/r2", {KEY_LEFTMETA, KEY_LEFTSHIFT}, BTN_LEFT);
        QCOMPARE(state.press("pad/l2", {KEY_LEFTMETA, KEY_LEFTSHIFT}, BTN_RIGHT),
                 (QList<InputChange>{{InputChange::Mouse, BTN_RIGHT, true}}));
        QCOMPARE(state.release("pad/r2"), (QList<InputChange>{{InputChange::Mouse, BTN_LEFT, false}}));
        QCOMPARE(state.release("pad/l2"), (QList<InputChange>{{InputChange::Mouse, BTN_RIGHT, false},
            {InputChange::Key, KEY_LEFTSHIFT, false}, {InputChange::Key, KEY_LEFTMETA, false}}));
    }
    void arbitraryMultiKeyMultiMouseCombinationReleasesInReverseOrder() {
        HeldInputState state;
        const auto changes = state.press("pad/custom", {KEY_LEFTCTRL, KEY_LEFTMETA, KEY_K, KEY_J, KEY_LEFTCTRL}, QList<quint32>{BTN_LEFT, BTN_RIGHT, BTN_LEFT});
        QCOMPARE(changes, (QList<InputChange>{{InputChange::Key, KEY_LEFTCTRL, true}, {InputChange::Key, KEY_LEFTMETA, true},
            {InputChange::Key, KEY_K, true}, {InputChange::Key, KEY_J, true}, {InputChange::Mouse, BTN_LEFT, true}, {InputChange::Mouse, BTN_RIGHT, true}}));
        QCOMPARE(state.release("pad/custom"), (QList<InputChange>{{InputChange::Mouse, BTN_RIGHT, false}, {InputChange::Mouse, BTN_LEFT, false},
            {InputChange::Key, KEY_J, false}, {InputChange::Key, KEY_K, false}, {InputChange::Key, KEY_LEFTMETA, false}, {InputChange::Key, KEY_LEFTCTRL, false}}));
    }
    void duplicateClicksAndDisconnectReleaseOnlyOwnedTokens() {
        HeldInputState state;
        state.press("a/r1", {}, BTN_LEFT);
        QVERIFY(state.press("b/r1", {}, BTN_LEFT).isEmpty());
        QVERIFY(state.release("a/r1").isEmpty());
        QVERIFY(state.contains("b/r1"));
        QCOMPARE(state.release("b/r1"), (QList<InputChange>{{InputChange::Mouse, BTN_LEFT, false}}));
        QVERIFY(state.release("b/r1").isEmpty());
        QVERIFY(state.tokens().isEmpty());
    }
};
QTEST_GUILESS_MAIN(HeldInputStateTest)
#include "tst_held_input_state.moc"
