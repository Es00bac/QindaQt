// SPDX-License-Identifier: GPL-3.0-or-later
#include "panelvisibilityanimation.h"

#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QtTest/QtTest>

using namespace QindaQt;

// What the production fade actually moves. The port used to animate
// QWindow::opacity, which the Wayland platform does not implement: every
// transition logged "This plugin does not support setting window opacity" and
// nothing faded. These rows pin the target to the QQuickWindow scene root,
// where opacity is honoured. See ADR-0236.
class PanelVisibilityFadeTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void fadesTheSceneRootAndNotTheWindow();
    void restoreReturnsTheSceneRootToOpaque();
    void immediateDurationStillReportsCompletion();
};

void PanelVisibilityFadeTests::fadesTheSceneRootAndNotTheWindow()
{
    Shell::QtPanelVisibilityAnimation animation;
    QQuickWindow window;
    QVERIFY(window.contentItem() != nullptr);

    bool completed = false;
    // Zero duration settles synchronously, so the end state is observable
    // without driving an event loop.
    animation.animate(window, 1.0, 0.0, 0, [&completed] { completed = true; });
    QVERIFY(completed);

    QCOMPARE(window.contentItem()->opacity(), 0.0);
    // The window itself is deliberately untouched: setting it would only log
    // the unsupported-opacity warning once per frame and fade nothing.
    QCOMPARE(window.opacity(), 1.0);
}

void PanelVisibilityFadeTests::restoreReturnsTheSceneRootToOpaque()
{
    Shell::QtPanelVisibilityAnimation animation;
    QQuickWindow window;

    animation.animate(window, 1.0, 0.0, 0, {});
    QCOMPARE(window.contentItem()->opacity(), 0.0);

    // A transition abandoned mid-flight (lost compositor authority, a panel
    // leaving the admitted set) must not strand the panel part-transparent.
    animation.restore(window);
    QCOMPARE(window.contentItem()->opacity(), 1.0);
}

void PanelVisibilityFadeTests::immediateDurationStillReportsCompletion()
{
    Shell::QtPanelVisibilityAnimation animation;
    QQuickWindow window;

    // The visibility hold is released from this callback: dropping it would
    // leave the panel pinned visible forever.
    int completions = 0;
    animation.animate(window, 0.0, 1.0, 0, [&completions] { ++completions; });
    QCOMPARE(completions, 1);
    QCOMPARE(window.contentItem()->opacity(), 1.0);
}

QTEST_MAIN(PanelVisibilityFadeTests)
#include "tst_panelvisibilityfade.moc"
