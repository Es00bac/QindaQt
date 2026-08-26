// SPDX-License-Identifier: GPL-3.0-or-later
#include "testfixtures.h"

#include "qindaqt/hybrid_chrome/chromehittest.h"
#include "qindaqt/hybrid_chrome/chromelayoutengine.h"

#include <QtTest>

using namespace QindaQt::HybridChrome;
using namespace QindaQt::HybridChrome::TestFixtures;

class ChromeHitTestTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void resolvesEveryStandardAction();
    void rtlTabHitPreservesLogicalIndex();
    void resolvesDragDividerResizeAndClientRegions();
    void maximizedFrameHasNoResizeTarget();
};

void ChromeHitTestTests::resolvesEveryStandardAction()
{
    const auto plan = ChromeLayoutEngine::build(qindaMacRequest());
    QVERIFY(plan);
    for (const auto &button : plan->buttons) {
        const auto hit = ChromeHitTester::hitTest(*plan, button.rect.center());
        QCOMPARE(hit.kind, HitKind::WindowButton);
        QVERIFY(hit.action);
        QCOMPARE(*hit.action, button.action);
        QCOMPARE(hit.stableId, plan->containerId);
    }

    auto maximizedRequest = qindaMacRequest();
    maximizedRequest.maximized = true;
    const auto maximizedPlan = ChromeLayoutEngine::build(maximizedRequest);
    QVERIFY(maximizedPlan);
    const auto restoreButton = maximizedPlan->buttons.constLast();
    QCOMPARE(restoreButton.action, WindowAction::Restore);
    const auto restoreHit = ChromeHitTester::hitTest(*maximizedPlan,
                                                     restoreButton.rect.center());
    QVERIFY(restoreHit.action);
    QCOMPARE(*restoreHit.action, WindowAction::Restore);
}

void ChromeHitTestTests::rtlTabHitPreservesLogicalIndex()
{
    const auto plan = ChromeLayoutEngine::build(qindaMacRequest());
    QVERIFY(plan);
    for (qsizetype index = 0; index < plan->tabs.size(); ++index) {
        const auto hit = ChromeHitTester::hitTest(*plan, plan->tabs[index].rect.center());
        QCOMPARE(hit.kind, HitKind::Tab);
        QCOMPARE(hit.logicalIndex, index);
        QCOMPARE(hit.stableId, plan->tabs[index].tabId);
    }
    QVERIFY(plan->tabs[0].rect.center().x() > plan->tabs[2].rect.center().x());
}

void ChromeHitTestTests::resolvesDragDividerResizeAndClientRegions()
{
    const auto plan = ChromeLayoutEngine::build(qindaMacRequest());
    QVERIFY(plan);

    const auto dividerHit = ChromeHitTester::hitTest(*plan,
                                                     plan->dividers[0].hitRect.center());
    QCOMPARE(dividerHit.kind, HitKind::Divider);
    QCOMPARE(dividerHit.stableId, QStringLiteral("divider-main"));

    const QPointF memberPoint(plan->members[0].titleDragRect.left() + 40.0,
                              plan->members[0].titleDragRect.center().y());
    const auto memberHit = ChromeHitTester::hitTest(*plan, memberPoint);
    QCOMPARE(memberHit.kind, HitKind::MemberTitleDrag);
    QCOMPARE(memberHit.stableId, QStringLiteral("member-a"));

    const auto titleHit = ChromeHitTester::hitTest(*plan, plan->outerTitleDragRect.center());
    QCOMPARE(titleHit.kind, HitKind::OuterTitleDrag);
    QCOMPARE(titleHit.stableId, plan->containerId);

    const auto resizeHit = ChromeHitTester::hitTest(*plan, plan->outerFrame.topLeft());
    QCOMPARE(resizeHit.kind, HitKind::OuterResize);
    QVERIFY(resizeHit.resizeEdges.testFlag(Qt::LeftEdge));
    QVERIFY(resizeHit.resizeEdges.testFlag(Qt::TopEdge));

    const QPointF clientPoint(plan->contentRect.left() + 100.0,
                              plan->contentRect.top() + 100.0);
    QCOMPARE(ChromeHitTester::hitTest(*plan, clientPoint).kind, HitKind::Client);
    QCOMPARE(ChromeHitTester::hitTest(*plan, QPointF(-50.0, -50.0)).kind, HitKind::None);
}

void ChromeHitTestTests::maximizedFrameHasNoResizeTarget()
{
    auto request = qindaMacRequest();
    request.maximized = true;
    const auto plan = ChromeLayoutEngine::build(request);
    QVERIFY(plan);
    const auto hit = ChromeHitTester::hitTest(*plan, plan->outerFrame.topLeft());
    QVERIFY(hit.kind != HitKind::OuterResize);
}

QTEST_GUILESS_MAIN(ChromeHitTestTests)
#include "tst_hittest.moc"
