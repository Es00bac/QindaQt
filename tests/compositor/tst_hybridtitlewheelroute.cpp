// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridtitlewheelroute.h"

#include <QtTest>

namespace QindaQt::Compositor::KWinIntegration {

class HybridTitleWheelRouteTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void independentTitleRollsTheWindowToItsIcon();
    void zoomedMemberTitleRollsUpItsWholeContainer();
    void visibleContainerChromeKeepsTheHandlebarWheel();
    void nonTitleAndIneligibleWindowsNeverRoute();
};

void HybridTitleWheelRouteTest::independentTitleRollsTheWindowToItsIcon()
{
    QCOMPARE(titleWheelRoute({.onTitleBar = true, .normalWindow = true}),
             TitleWheelRoute::IconifyWindow);
}

// ADR-0282: a member maximize hides the group's shared chrome, so the zoomed
// member's own title bar is the only title the group has. Its wheel did
// nothing, and the group could not be rolled up from the pointer at all.
void HybridTitleWheelRouteTest::zoomedMemberTitleRollsUpItsWholeContainer()
{
    QCOMPARE(titleWheelRoute({.onTitleBar = true,
                              .normalWindow = true,
                              .grouped = true,
                              .containerChromeVisible = false}),
             TitleWheelRoute::RollUpContainer);
}

void HybridTitleWheelRouteTest::visibleContainerChromeKeepsTheHandlebarWheel()
{
    QCOMPARE(titleWheelRoute({.onTitleBar = true,
                              .normalWindow = true,
                              .grouped = true,
                              .containerChromeVisible = true}),
             TitleWheelRoute::None);
}

void HybridTitleWheelRouteTest::nonTitleAndIneligibleWindowsNeverRoute()
{
    QCOMPARE(titleWheelRoute({.onTitleBar = false, .normalWindow = true}),
             TitleWheelRoute::None);
    QCOMPARE(titleWheelRoute({.onTitleBar = true, .normalWindow = false}),
             TitleWheelRoute::None);
    QCOMPARE(titleWheelRoute({.onTitleBar = true, .normalWindow = true, .iconified = true}),
             TitleWheelRoute::None);
    QCOMPARE(titleWheelRoute({.onTitleBar = false, .normalWindow = true, .grouped = true}),
             TitleWheelRoute::None);
}

} // namespace QindaQt::Compositor::KWinIntegration

QTEST_GUILESS_MAIN(QindaQt::Compositor::KWinIntegration::HybridTitleWheelRouteTest)
#include "tst_hybridtitlewheelroute.moc"
