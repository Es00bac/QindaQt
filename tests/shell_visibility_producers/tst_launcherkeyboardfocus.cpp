// SPDX-License-Identifier: GPL-3.0-or-later

#include "panelkeyboardfocus.h"

#include <QSignalSpy>
#include <QtTest>

using QindaQt::Shell::LauncherKeyboardFocusRelay;
using QindaQt::Shell::PanelFocusTarget;

namespace {

// A panel whose focus the compositor grants when the test says so, never
// synchronously: that asynchrony is the whole reason the relay exists.
class FakePanelFocusTarget final : public PanelFocusTarget {
    Q_OBJECT

public:
    void grantKeyboardFocus() override { ++grants; }
    void revokeKeyboardFocus() override
    {
        ++revokes;
        if (focused) {
            focused = false;
            Q_EMIT keyboardFocusChanged();
        }
    }
    [[nodiscard]] bool hasKeyboardFocus() const override { return focused; }

    void compositorGrantsFocus()
    {
        focused = true;
        Q_EMIT keyboardFocusChanged();
    }

    int grants = 0;
    int revokes = 0;
    bool focused = false;
};

} // namespace

class LauncherKeyboardFocusTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void opensOnlyOnceThePanelHasFocus();
    void aSecondPressWhileOpenDoesNotAskTwice();
    void closingTheBrowserGivesFocusBack();
    void focusThatNeverArrivesStillOpens();
    void everyCycleEndsWithTheGrantReleased();
};

void LauncherKeyboardFocusTest::opensOnlyOnceThePanelHasFocus() {
    FakePanelFocusTarget target;
    LauncherKeyboardFocusRelay relay(target);
    QSignalSpy opened(&relay, &LauncherKeyboardFocusRelay::openNow);

    relay.requestOpen();
    QCOMPARE(target.grants, 1);
    // AGENT-GUARD: opening before the panel is focused is exactly the bug this
    // relay exists for -- the popup is refused with "Failed to create grabbing
    // popup" and the launcher never appears.
    QCOMPARE(opened.count(), 0);

    target.compositorGrantsFocus();
    QCOMPARE(opened.count(), 1);
}

void LauncherKeyboardFocusTest::aSecondPressWhileOpenDoesNotAskTwice() {
    FakePanelFocusTarget target;
    LauncherKeyboardFocusRelay relay(target);
    QSignalSpy opened(&relay, &LauncherKeyboardFocusRelay::openNow);

    relay.requestOpen();
    relay.requestOpen();
    QCOMPARE(target.grants, 1);
    target.compositorGrantsFocus();
    QCOMPARE(opened.count(), 1);

    // Once the panel holds focus the open is immediate, with no second grant.
    relay.requestOpen();
    QCOMPARE(opened.count(), 2);
    QCOMPARE(target.grants, 1);
}

void LauncherKeyboardFocusTest::closingTheBrowserGivesFocusBack() {
    FakePanelFocusTarget target;
    LauncherKeyboardFocusRelay relay(target);

    relay.requestOpen();
    target.compositorGrantsFocus();
    relay.browserClosed();
    QCOMPARE(target.revokes, 1);
    QVERIFY(!target.hasKeyboardFocus());
}

void LauncherKeyboardFocusTest::focusThatNeverArrivesStillOpens() {
    FakePanelFocusTarget target;
    LauncherKeyboardFocusRelay relay(target);
    QSignalSpy opened(&relay, &LauncherKeyboardFocusRelay::openNow);

    relay.requestOpen();
    QCOMPARE(opened.count(), 0);
    // A compositor that never grants focus must not swallow the key press: a
    // launcher that appears without a grab beats one that never appears.
    QTRY_COMPARE_WITH_TIMEOUT(
        opened.count(), 1,
        LauncherKeyboardFocusRelay::focusWaitMilliseconds() * 4);
}

void LauncherKeyboardFocusTest::everyCycleEndsWithTheGrantReleased() {
    FakePanelFocusTarget target;
    LauncherKeyboardFocusRelay relay(target);

    for (int cycle = 0; cycle < 3; ++cycle) {
        relay.requestOpen();
        target.compositorGrantsFocus();
        relay.browserClosed();
    }
    // AGENT-GUARD: a panel left focusable keeps taking keystrokes meant for
    // the focused application, so grants and revokes must stay balanced.
    QCOMPARE(target.grants, target.revokes);
}

QTEST_MAIN(LauncherKeyboardFocusTest)
#include "tst_launcherkeyboardfocus.moc"
