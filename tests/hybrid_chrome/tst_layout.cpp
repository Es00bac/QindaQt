// SPDX-License-Identifier: GPL-3.0-or-later
#include "testfixtures.h"

#include "qindaqt/hybrid_chrome/chromelayoutengine.h"

#include <QtTest>

#include <algorithm>

using namespace QindaQt::HybridChrome;
using namespace QindaQt::HybridChrome::TestFixtures;

class ChromeLayoutTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void qindaMacUsesTrafficLightsAndVisualRtlTabs();
    void maximizedContainerOffersRestore();
    void standardButtonsHonorRequestedSide_data();
    void standardButtonsHonorRequestedSide();
    void logicalGeometryIsStableAcrossDpi();
    void derivesMemberTitleAndDividerRegions();
    void rejectsInvalidInput();
};

void ChromeLayoutTests::qindaMacUsesTrafficLightsAndVisualRtlTabs()
{
    QString error;
    const auto plan = ChromeLayoutEngine::build(qindaMacRequest(), &error);
    QVERIFY2(plan, qPrintable(error));
    QCOMPARE(plan->style.buttonSide, ButtonSide::Left);
    QCOMPARE(plan->style.buttonStyle, ButtonStyle::TrafficLights);
    QCOMPARE(plan->buttons.size(), 3);
    QCOMPARE(plan->buttons[0].action, WindowAction::Close);
    QCOMPARE(plan->buttons[1].action, WindowAction::Minimize);
    QCOMPARE(plan->buttons[2].action, WindowAction::Maximize);
    QCOMPARE(plan->buttons[0].hoverGlyph, QStringLiteral("x"));
    QCOMPARE(plan->buttons[1].hoverGlyph, QStringLiteral("_"));
    QCOMPARE(plan->buttons[2].hoverGlyph, QStringLiteral("[]"));
    QVERIFY(std::none_of(plan->buttons.cbegin(), plan->buttons.cend(),
                         [](const auto &button) { return button.glyphVisibleWhenIdle; }));
    QCOMPARE(plan->buttons[0].fillColor, plan->style.palette.close);
    QCOMPARE(plan->buttons[1].fillColor, plan->style.palette.minimize);
    QCOMPARE(plan->buttons[2].fillColor, plan->style.palette.maximize);

    // AGENT-GUARD: The vector remains logical order. Only its assigned visual
    // rectangles reverse, preserving persistence and keyboard traversal IDs.
    QCOMPARE(plan->tabs[0].tabId, QStringLiteral("page-a"));
    QCOMPARE(plan->tabs[1].tabId, QStringLiteral("page-b"));
    QCOMPARE(plan->tabs[2].tabId, QStringLiteral("page-c"));
    QVERIFY(plan->tabs[0].rect.center().x() > plan->tabs[1].rect.center().x());
    QVERIFY(plan->tabs[1].rect.center().x() > plan->tabs[2].rect.center().x());
}

void ChromeLayoutTests::maximizedContainerOffersRestore()
{
    auto request = qindaMacRequest();
    request.maximized = true;
    const auto plan = ChromeLayoutEngine::build(request);
    QVERIFY(plan);
    QVERIFY(plan->maximized);
    QCOMPARE(plan->buttons.constLast().action, WindowAction::Restore);
    QCOMPARE(plan->buttons.constLast().hoverGlyph, QStringLiteral("[]"));
}

void ChromeLayoutTests::standardButtonsHonorRequestedSide_data()
{
    QTest::addColumn<ButtonSide>("side");
    QTest::addColumn<WindowAction>("firstAction");
    QTest::newRow("left") << ButtonSide::Left << WindowAction::Close;
    QTest::newRow("right") << ButtonSide::Right << WindowAction::Minimize;
}

void ChromeLayoutTests::standardButtonsHonorRequestedSide()
{
    QFETCH(ButtonSide, side);
    QFETCH(WindowAction, firstAction);
    auto request = baseRequest();
    request.style = ChromeStyle::standard(side);
    const auto plan = ChromeLayoutEngine::build(request);
    QVERIFY(plan);
    QCOMPARE(plan->buttons.constFirst().action, firstAction);
    QVERIFY(std::all_of(plan->buttons.cbegin(), plan->buttons.cend(),
                        [](const auto &button) { return button.glyphVisibleWhenIdle; }));
    if (side == ButtonSide::Left) {
        QVERIFY(plan->buttons.constFirst().rect.center().x() < plan->outerFrame.center().x());
    } else {
        QVERIFY(plan->buttons.constFirst().rect.center().x() > plan->outerFrame.center().x());
        QCOMPARE(plan->buttons.constLast().action, WindowAction::Close);
    }
}

void ChromeLayoutTests::logicalGeometryIsStableAcrossDpi()
{
    auto oneX = qindaMacRequest();
    auto twoX = oneX;
    oneX.devicePixelRatio = 1.0;
    twoX.devicePixelRatio = 2.0;
    const auto first = ChromeLayoutEngine::build(oneX);
    const auto second = ChromeLayoutEngine::build(twoX);
    QVERIFY(first);
    QVERIFY(second);
    QCOMPARE(first->outerTitleBar, second->outerTitleBar);
    QCOMPARE(first->tabStrip, second->tabStrip);
    QCOMPARE(first->contentRect, second->contentRect);
    QCOMPARE(first->buttons[0].rect, second->buttons[0].rect);
    QCOMPARE(first->members[0].titleDragRect, second->members[0].titleDragRect);
    QCOMPARE(first->borderHairline, 1.0);
    QCOMPARE(second->borderHairline, 0.5);
}

void ChromeLayoutTests::derivesMemberTitleAndDividerRegions()
{
    const auto plan = ChromeLayoutEngine::build(baseRequest());
    QVERIFY(plan);
    QCOMPARE(plan->members.size(), 2);
    QCOMPARE(plan->members[0].titleDragRect.height(), plan->metrics.memberTitleHeight);
    QCOMPARE(plan->members[0].titleDragRect.top(), plan->members[0].windowRect.top());
    QCOMPARE(plan->dividers.size(), 1);
    QCOMPARE(plan->dividers[0].visualRect.width(), plan->metrics.dividerVisualThickness);
    QCOMPARE(plan->dividers[0].hitRect.width(), plan->metrics.dividerHitThickness);
    QVERIFY(plan->dividers[0].hitRect.contains(plan->dividers[0].visualRect));
}

void ChromeLayoutTests::rejectsInvalidInput()
{
    auto duplicate = baseRequest();
    duplicate.tabs[1].tabId = duplicate.tabs[0].tabId;
    QString error;
    QVERIFY(!ChromeLayoutEngine::build(duplicate, &error));
    QVERIFY(error.contains(QStringLiteral("duplicate tab")));

    auto outside = baseRequest();
    outside.members[0].windowRect.translate(-50.0, 0.0);
    QVERIFY(!ChromeLayoutEngine::build(outside, &error));
    QVERIFY(error.contains(QStringLiteral("outside")));

    auto badDpi = baseRequest();
    badDpi.devicePixelRatio = 0.0;
    QVERIFY(!ChromeLayoutEngine::build(badDpi, &error));
    QVERIFY(error.contains(QStringLiteral("pixel ratio")));

    auto badDivider = baseRequest();
    badDivider.dividers[0].position = badDivider.outerRect.right() + 10.0;
    QVERIFY(!ChromeLayoutEngine::build(badDivider, &error));
    QVERIFY(error.contains(QStringLiteral("divider")));
}

QTEST_GUILESS_MAIN(ChromeLayoutTests)
#include "tst_layout.moc"
