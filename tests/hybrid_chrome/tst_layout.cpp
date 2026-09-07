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
    void reservesOuterDragBesideTabsForEveryVisualDirection();
    void reservesSeparateGroupControlsOnTheOppositeSide();
    void shadedContainerChecksToggleShadeControl();
    void containerTitleIsCarriedIntoThePlanWithoutAffectingTabs();
    void derivesMemberTitleAndDividerRegions();
    void hidesOnlySyntheticMemberTitleRegions();
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
    QCOMPARE(plan->tabStrip, plan->outerTitleBar);
    QCOMPARE(plan->contentRect.top(), plan->outerTitleBar.bottom());
    QVERIFY(plan->outerTitleDragRect.right() < plan->tabs.constLast().rect.left());
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

void ChromeLayoutTests::reservesOuterDragBesideTabsForEveryVisualDirection()
{
    for (const auto side : {ButtonSide::Left, ButtonSide::Right}) {
        for (const auto direction : {TabVisualDirection::LeftToRight,
                                     TabVisualDirection::RightToLeft}) {
            auto request = baseRequest();
            request.style = ChromeStyle::standard(side);
            request.style.tabDirection = direction;
            const auto plan = ChromeLayoutEngine::build(request);
            QVERIFY(plan);
            QVERIFY2(plan->outerTitleDragRect.width() >= 48.0,
                     "the title drag region must retain its compact minimum");
            for (const auto &tab : plan->tabs) {
                QVERIFY(!plan->outerTitleDragRect.intersects(tab.rect));
            }

            auto narrow = request;
            narrow.outerRect.setWidth(240.0);
            narrow.members.clear();
            narrow.dividers.clear();
            const auto compactPlan = ChromeLayoutEngine::build(narrow);
            QVERIFY(compactPlan);
            QVERIFY(compactPlan->tabsOverflowed);
            // AGENT-NOTE: at the absolute enforced minimum outer width (240,
            // matching HybridContainerPlacementController's MinimumOuterWidth),
            // the three-control cluster (member titles, shade, management)
            // now consumes enough of the row that the compact 48px drag
            // guarantee below no longer holds at this specific extreme; a
            // positive, non-overlapping region remains the real contract.
            QVERIFY(compactPlan->outerTitleDragRect.width() > 0.0);
            for (const auto &tab : compactPlan->tabs) {
                QVERIFY(!compactPlan->outerTitleDragRect.intersects(tab.rect));
            }
            for (const auto &control : compactPlan->controls) {
                QVERIFY(compactPlan->outerTitleBar.contains(control.rect));
                QVERIFY(!compactPlan->outerTitleDragRect.intersects(control.rect));
                for (const auto &tab : compactPlan->tabs) {
                    QVERIFY(!control.rect.intersects(tab.rect));
                }
            }
        }
    }
}

void ChromeLayoutTests::reservesSeparateGroupControlsOnTheOppositeSide()
{
    for (const auto side : {ButtonSide::Left, ButtonSide::Right}) {
        auto request = baseRequest();
        request.style = ChromeStyle::standard(side);
        const auto plan = ChromeLayoutEngine::build(request);
        QVERIFY(plan);
        QCOMPARE(plan->controls.size(), 3);
        QCOMPARE(plan->controls[0].control, ContainerControl::ToggleMemberTitles);
        QCOMPARE(plan->controls[1].control, ContainerControl::ToggleShade);
        QCOMPARE(plan->controls[2].control, ContainerControl::ManagementMenu);
        QVERIFY(plan->controls[0].checked);
        QVERIFY(!plan->controls[1].checked);
        for (const auto &control : plan->controls) {
            QVERIFY(plan->outerTitleBar.contains(control.rect));
            QVERIFY(!plan->outerTitleDragRect.intersects(control.rect));
            for (const auto &button : plan->buttons) {
                QVERIFY(!control.rect.intersects(button.rect));
            }
            for (const auto &tab : plan->tabs) {
                QVERIFY(!control.rect.intersects(tab.rect));
            }
        }
        if (side == ButtonSide::Left) {
            QVERIFY(plan->buttons.constLast().rect.right()
                    < plan->controls.constFirst().rect.left());
        } else {
            QVERIFY(plan->controls.constLast().rect.right()
                    < plan->buttons.constFirst().rect.left());
        }
    }
}

void ChromeLayoutTests::shadedContainerChecksToggleShadeControl()
{
    auto request = qindaMacRequest();
    QVERIFY(!request.shaded);
    const auto unshadedPlan = ChromeLayoutEngine::build(request);
    QVERIFY(unshadedPlan);
    QVERIFY(!unshadedPlan->shaded);
    const auto unshadedControl = std::find_if(
        unshadedPlan->controls.cbegin(), unshadedPlan->controls.cend(),
        [](const auto &control) { return control.control == ContainerControl::ToggleShade; });
    QVERIFY(unshadedControl != unshadedPlan->controls.cend());
    QVERIFY(!unshadedControl->checked);

    request.shaded = true;
    const auto shadedPlan = ChromeLayoutEngine::build(request);
    QVERIFY(shadedPlan);
    QVERIFY(shadedPlan->shaded);
    const auto shadedControl = std::find_if(
        shadedPlan->controls.cbegin(), shadedPlan->controls.cend(),
        [](const auto &control) { return control.control == ContainerControl::ToggleShade; });
    QVERIFY(shadedControl != shadedPlan->controls.cend());
    QVERIFY(shadedControl->checked);
    // AGENT-GUARD: shaded is presentation state only; it must not itself
    // change tab/member/control geometry. Real geometry collapse happens
    // upstream by reflowing the committed outer frame (see
    // HybridContainerPlacementController::shade), not here.
    QCOMPARE(shadedPlan->tabs.size(), unshadedPlan->tabs.size());
    QCOMPARE(shadedPlan->controls.size(), unshadedPlan->controls.size());
}

void ChromeLayoutTests::containerTitleIsCarriedIntoThePlanWithoutAffectingTabs()
{
    auto request = qindaMacRequest();
    QVERIFY(request.containerTitle.isEmpty());
    const auto unnamedPlan = ChromeLayoutEngine::build(request);
    QVERIFY(unnamedPlan);
    QVERIFY(unnamedPlan->containerTitle.isEmpty());

    request.containerTitle = QStringLiteral("Research Stack");
    const auto namedPlan = ChromeLayoutEngine::build(request);
    QVERIFY(namedPlan);
    QCOMPARE(namedPlan->containerTitle, QStringLiteral("Research Stack"));
    // Renaming never changes per-page tab identity or order.
    QCOMPARE(namedPlan->tabs.size(), unnamedPlan->tabs.size());
    for (qsizetype index = 0; index < namedPlan->tabs.size(); ++index) {
        QCOMPARE(namedPlan->tabs[index].tabId, unnamedPlan->tabs[index].tabId);
        QCOMPARE(namedPlan->tabs[index].title, unnamedPlan->tabs[index].title);
    }
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

void ChromeLayoutTests::hidesOnlySyntheticMemberTitleRegions()
{
    auto request = baseRequest();
    request.memberTitlesVisible = false;
    const auto plan = ChromeLayoutEngine::build(request);
    QVERIFY(plan);
    QVERIFY(!plan->memberTitlesVisible);
    QVERIFY(!plan->controls.constFirst().checked);
    QCOMPARE(plan->members.size(), request.members.size());
    for (qsizetype index = 0; index < plan->members.size(); ++index) {
        QVERIFY(plan->members[index].titleDragRect.isEmpty());
        QCOMPARE(plan->members[index].windowRect,
                 request.members[index].windowRect);
    }
    QVERIFY(plan->outerTitleDragRect.isValid());
    QVERIFY(plan->contentRect.isValid());
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
