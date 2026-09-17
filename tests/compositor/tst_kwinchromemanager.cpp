// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridstackingorder.h"
#include "chromeplanlocalizer.h"
#include "kwinchromemanager_testfixture.h"

#include "qindaqt/hybrid_chrome/chromerenderer.h"

#include <QPainter>
#include "qindaqt/hybrid_chrome/chromeshadedbadge.h"
#include <QFontMetricsF>
#include <QtTest>

using namespace QindaQt;
using namespace QindaQt::Compositor::KWinIntegration;
using namespace QindaQt::Compositor::KWinIntegration::Test;

class KWinChromeManagerTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void reconcilesOneOverlayPerContainerAndTearsDownSafely();
    void mapsChromeHitRegionsToHybridTargets();
    void mapsWindowActionsAndHonorsStackingOrder();
    void supportsQindaMacAndStandardChromePlans();
    void sceneOverlayBoundaryRoutesNativeInputAndOwnsVisibility();
    void recreatesSceneOverlaysAfterSynchronousClear();
    void validatesPointerActivationAndRoutesHoverToPaint();
    void rejectsInvalidOrStaleSnapshotsAtomically();
    void publishesAShadedBadgeWithTabsButWithoutMemberGeometry();
    void localizesAndPaintsLiveContainerControls();
    void localizesTheRolledUpBadgeRectsSoItsLabelIsPainted();
};

void KWinChromeManagerTests::reconcilesOneOverlayPerContainerAndTearsDownSafely()
{
    FakeOverlayFactory factory;
    KWinChromeManager manager(factory);
    auto alpha = makeContainer(QStringLiteral("alpha"));
    const auto topology = makeTopology({alpha}, 4);
    QString error;
    QVERIFY2(manager.updateFromSnapshot(topology, plansFor(topology), {}, &error),
             qPrintable(error));
    QCOMPARE(manager.overlayCount(), 1);
    QCOMPARE(manager.topologyRevision(), std::optional<quint64>(4));
    const auto record = factory.records.value(QStringLiteral("container-alpha"));
    QCOMPARE(record->planCount, 1);
    QCOMPARE(record->showCount, 1);

    auto refreshed = plansFor(topology, HybridChrome::ChromeStyle::qindaMacOS({}));
    QVERIFY2(manager.updateFromSnapshot(topology, refreshed, {}, &error), qPrintable(error));
    QCOMPARE(factory.createCount, 1);
    QCOMPARE(record->planCount, 2);
    QCOMPARE(record->showCount, 2);

    bool observedPublishedEmpty = false;
    record->closing = [&] { observedPublishedEmpty = manager.overlayCount() == 0; };
    manager.quarantineContainer(QStringLiteral("container-alpha"));
    const auto empty = makeTopology({}, 5);
    QVERIFY2(manager.updateFromSnapshot(empty, {}, {}, &error), qPrintable(error));
    QVERIFY(observedPublishedEmpty);
    QCOMPARE(record->closeCount, 1);
    QVERIFY(record->destroyed);
    QCOMPARE(manager.overlayCount(), 0);
    QVERIFY(!manager.containerQuarantined(QStringLiteral("container-alpha")));
}

void KWinChromeManagerTests::mapsChromeHitRegionsToHybridTargets()
{
    FakeOverlayFactory factory;
    KWinChromeManager manager(factory);
    const auto topology = makeTopology({makeContainer(QStringLiteral("alpha"))}, 1);
    auto plans = plansFor(topology);
    const auto plan = plans.constBegin().value();
    QVERIFY(manager.updateFromSnapshot(topology, plans));

    QCOMPARE(manager.hitTestChrome(centerOf(plan.outerTitleDragRect)),
             HybridInput::HitTarget({HybridInput::HitKind::OuterTitle,
                                     QStringLiteral("container-alpha"), {}, {}}));
    QCOMPARE(manager.hitTestChrome(centerOf(plan.members[0].titleDragRect)),
             HybridInput::HitTarget({HybridInput::HitKind::MemberTitle,
                                     QStringLiteral("container-alpha"),
                                     QStringLiteral("window-alpha-a"), {}}));
    auto mainTab = HybridInput::HitTarget{HybridInput::HitKind::Tab,
                                          QStringLiteral("container-alpha"),
                                          QStringLiteral("window-alpha-a"), {}};
    mainTab.pageId = QStringLiteral("page-alpha-main");
    QCOMPARE(manager.hitTestChrome(centerOf(plan.tabs[0].rect)), mainTab);
    auto extraTab = HybridInput::HitTarget{HybridInput::HitKind::Tab,
                                           QStringLiteral("container-alpha"),
                                           QStringLiteral("window-alpha-c"), {}};
    extraTab.pageId = QStringLiteral("page-alpha-extra");
    QCOMPARE(manager.hitTestChrome(centerOf(plan.tabs[1].rect)), extraTab);
    QCOMPARE(manager.hitTestChrome(centerOf(plan.dividers[0].hitRect)),
             HybridInput::HitTarget({HybridInput::HitKind::Divider,
                                     QStringLiteral("container-alpha"), {},
                                     QStringLiteral("split-alpha-main")}));
    QVERIFY(!manager.hitTestChrome(centerOf(plan.buttons[0].rect)).isValid());
    QVERIFY(!manager.hitTestChrome(centerOf(plan.members[0].windowRect)).isValid());
}

void KWinChromeManagerTests::mapsWindowActionsAndHonorsStackingOrder()
{
    FakeOverlayFactory factory;
    KWinChromeManager manager(factory);
    auto topology = makeTopology({makeContainer(QStringLiteral("alpha")),
                                  makeContainer(QStringLiteral("beta"))}, 7);
    auto plans = plansFor(topology);
    const auto containerIds = topology.containerIds();
    const auto alphaTop = topmostMemberContainerOrder(
        {QStringLiteral("container-alpha"), QStringLiteral("container-beta"),
         QStringLiteral("container-alpha")},
        containerIds);
    QCOMPARE(alphaTop,
             QStringList({QStringLiteral("container-beta"),
                          QStringLiteral("container-alpha")}));
    QVERIFY(manager.updateFromSnapshot(topology, plans, alphaTop));
    const auto &topPlan = plans[QStringLiteral("container-alpha")];
    const auto action = manager.windowActionAt(centerOf(topPlan.buttons.constLast().rect));
    QVERIFY(action);
    QCOMPARE(action->containerId, QStringLiteral("container-alpha"));
    QCOMPARE(action->action, HybridChrome::WindowAction::Close);

    QSignalSpy requested(&manager, &KWinChromeManager::windowActionRequested);
    QVERIFY(manager.requestWindowActionAt(centerOf(topPlan.buttons.constFirst().rect)));
    QCOMPARE(requested.size(), 1);
    QCOMPARE(requested.constFirst().at(0).toString(), QStringLiteral("container-alpha"));
    QCOMPARE(requested.constFirst().at(1).value<HybridChrome::WindowAction>(),
             HybridChrome::WindowAction::Minimize);

    const auto betaTop = topmostMemberContainerOrder(
        {QStringLiteral("container-beta"), QStringLiteral("container-alpha"),
         QStringLiteral("container-beta")},
        containerIds);
    QCOMPARE(betaTop,
             QStringList({QStringLiteral("container-alpha"),
                          QStringLiteral("container-beta")}));
    QVERIFY(manager.updateFromSnapshot(topology, plans, betaTop));
    QCOMPARE(manager.windowActionAt(centerOf(topPlan.buttons.constLast().rect))->containerId,
             QStringLiteral("container-beta"));
}

void KWinChromeManagerTests::supportsQindaMacAndStandardChromePlans()
{
    FakeOverlayFactory factory;
    KWinChromeManager manager(factory);
    const auto topology = makeTopology({makeContainer(QStringLiteral("alpha"))}, 2);
    auto qindaPlans = plansFor(topology, HybridChrome::ChromeStyle::qindaMacOS({}));
    QVERIFY(manager.updateFromSnapshot(topology, qindaPlans));
    auto published = manager.plan(QStringLiteral("container-alpha"));
    QVERIFY(published);
    QCOMPARE(manager.tabRepresentatives(QStringLiteral("container-alpha")),
             (QMap<QString, QString>{
                 {QStringLiteral("page-alpha-main"),
                  QStringLiteral("window-alpha-a")},
                 {QStringLiteral("page-alpha-extra"),
                  QStringLiteral("window-alpha-c")},
             }));
    QVERIFY(manager.tabRepresentatives(QStringLiteral("missing-container")).isEmpty());
    QCOMPARE(published->style.buttonSide, HybridChrome::ButtonSide::Left);
    QCOMPARE(published->style.tabDirection, HybridChrome::TabVisualDirection::RightToLeft);
    QVERIFY(published->tabs[0].rect.center().x() > published->tabs[1].rect.center().x());

    auto standardLeft = plansFor(topology, HybridChrome::ChromeStyle::standard(
                                               HybridChrome::ButtonSide::Left));
    QVERIFY(manager.updateFromSnapshot(topology, standardLeft));
    QCOMPARE(manager.plan(QStringLiteral("container-alpha"))->style.buttonStyle,
             HybridChrome::ButtonStyle::Symbols);
    auto standardRight = plansFor(topology);
    QVERIFY(manager.updateFromSnapshot(topology, standardRight));
    QCOMPARE(manager.plan(QStringLiteral("container-alpha"))->style.buttonSide,
             HybridChrome::ButtonSide::Right);
}

void KWinChromeManagerTests::localizesAndPaintsLiveContainerControls()
{
    const auto topology = makeTopology({makeContainer(QStringLiteral("alpha"))}, 2);
    const QPointF globalOrigin(137.0, 83.0);
    const auto global = localizeChromeRenderPlan(
        plansFor(topology).constBegin().value(), -globalOrigin);
    QVERIFY(!global.controls.isEmpty());
    QCOMPARE(global.outerFrame.topLeft(), globalOrigin);

    const auto local = localizeChromeRenderPlan(global, global.outerFrame.topLeft());
    QCOMPARE(local.outerFrame.topLeft(), QPointF{});
    for (qsizetype index = 0; index < global.controls.size(); ++index) {
        QCOMPARE(local.controls.at(index).rect,
                 global.controls.at(index).rect.translated(-global.outerFrame.topLeft()));
    }

    const QSize size = local.outerFrame.size().toSize();
    QImage withControls(size, QImage::Format_ARGB32_Premultiplied);
    QImage withoutControls(size, QImage::Format_ARGB32_Premultiplied);
    withControls.fill(Qt::transparent);
    withoutControls.fill(Qt::transparent);
    {
        QPainter painter(&withControls);
        HybridChrome::ChromeRenderer::paint(painter, local);
    }
    auto stripped = local;
    stripped.controls.clear();
    {
        QPainter painter(&withoutControls);
        HybridChrome::ChromeRenderer::paint(painter, stripped);
    }

    bool controlPixelChanged = false;
    for (const auto &control : local.controls) {
        const QRect pixels = control.rect.toAlignedRect().intersected(withControls.rect());
        for (int y = pixels.top(); y <= pixels.bottom() && !controlPixelChanged; ++y) {
            for (int x = pixels.left(); x <= pixels.right(); ++x) {
                if (withControls.pixel(x, y) != withoutControls.pixel(x, y)) {
                    controlPixelChanged = true;
                    break;
                }
            }
        }
    }
    QVERIFY2(controlPixelChanged,
             "live scene-local rendering must produce visible container-control pixels");
}

// ADR-0189. The badge's own rectangles were the only painted geometry
// localizeChromeRenderPlan never translated, so a rolled-up container painted
// its controls and pills at frame-local coordinates and its label at *global*
// ones — outside the scene item's image, clipped away, invisible. That is why
// a rolled-up container never showed a title, and why ADR-0163 and ADR-0168
// both rewrote the label *text* without the user seeing any change: nothing
// was being drawn. This row fails if either badge rect is dropped again.
void KWinChromeManagerTests::localizesTheRolledUpBadgeRectsSoItsLabelIsPainted()
{
    const QString label = QStringLiteral("Quarterly revenue model, revision 12");
    const qreal labelWidth = HybridChrome::ChromeShadedBadge::labelWidthFor(
        label, QFontMetricsF(HybridChrome::ChromeShadedBadge::labelFont()));
    QVERIFY(labelWidth > 140.0);

    HybridChrome::ChromeLayoutRequest request;
    request.containerId = QStringLiteral("alpha");
    request.shaded = true;
    request.containerFocused = true;
    request.indexBadge = 3;
    request.badgeLabelText = label;
    request.badgeLabelWidth = labelWidth;
    request.tabs = {{QStringLiteral("page"), label, true}};
    const QPointF origin(137.0, 83.0);
    request.outerRect = QRectF(
        origin,
        QSizeF(HybridChrome::ChromeShadedBadge::badgeWidth(
                   HybridChrome::ChromeMetrics{}, 1, labelWidth)
                   + 2.0 * HybridChrome::ChromeMetrics{}.outerBorder,
               31.0));
    const auto global = HybridChrome::ChromeLayoutEngine::build(request);
    QVERIFY(global.has_value());
    QVERIFY(global->badgeLabelRect.isValid());
    // Built in global space, the label sits where the strip does.
    QVERIFY(global->badgeLabelRect.left() >= origin.x());

    const auto local = localizeChromeRenderPlan(*global, global->outerFrame.topLeft());
    QCOMPARE(local.outerFrame.topLeft(), QPointF{});
    QCOMPARE(local.badgeLabelRect,
             global->badgeLabelRect.translated(-global->outerFrame.topLeft()));
    if (global->indexBadgeRect.isValid()) {
        QCOMPARE(local.indexBadgeRect,
                 global->indexBadgeRect.translated(-global->outerFrame.topLeft()));
    }
    // AGENT-GUARD: the real consequence, not just the arithmetic — the label
    // has to land inside the image the scene item paints.
    QVERIFY2(local.outerFrame.contains(local.badgeLabelRect),
             qPrintable(QStringLiteral("label rect %1,%2 %3x%4 is outside the "
                                       "frame %5x%6")
                            .arg(local.badgeLabelRect.x())
                            .arg(local.badgeLabelRect.y())
                            .arg(local.badgeLabelRect.width())
                            .arg(local.badgeLabelRect.height())
                            .arg(local.outerFrame.width())
                            .arg(local.outerFrame.height())));

    QImage image(local.outerFrame.size().toSize(),
                 QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    {
        QPainter painter(&image);
        HybridChrome::ChromeRenderer::paint(painter, local);
    }
    const auto surface = local.style.palette.surface;
    int inkPixels = 0;
    const QRect probe = local.badgeLabelRect.toAlignedRect().intersected(image.rect());
    for (int x = probe.left(); x <= probe.right(); ++x) {
        for (int y = probe.top(); y <= probe.bottom(); ++y) {
            const auto pixel = image.pixelColor(x, y);
            if (pixel.alpha() > 0
                && (qAbs(pixel.red() - surface.red()) > 24
                    || qAbs(pixel.green() - surface.green()) > 24
                    || qAbs(pixel.blue() - surface.blue()) > 24)) {
                ++inkPixels;
            }
        }
    }
    QVERIFY2(inkPixels > 20,
             qPrintable(QStringLiteral("the badge label painted %1 ink pixels")
                            .arg(inkPixels)));
}

void KWinChromeManagerTests::sceneOverlayBoundaryRoutesNativeInputAndOwnsVisibility()
{
    FakeOverlayFactory factory;
    KWinChromeManager manager(factory);
    const auto topology = makeTopology({makeContainer(QStringLiteral("alpha"))}, 3);
    const auto plans = plansFor(topology);
    const auto plan = plans.constBegin().value();
    QSignalSpy visibility(&manager,
                          &KWinChromeManager::overlayVisibilityChanged);
    QVERIFY(manager.updateFromSnapshot(topology, plans));
    QCOMPARE(visibility.size(), 1);
    QCOMPARE(visibility.constFirst().at(0).toString(),
             QStringLiteral("container-alpha"));
    QVERIFY(visibility.constFirst().at(1).toBool());

    QCOMPARE(manager.overlayWidget(QStringLiteral("container-alpha")), nullptr);
    QVERIFY(manager.overlayVisible(QStringLiteral("container-alpha")));
    QCOMPARE(manager.overlayCount(), 1);
    QCOMPARE(manager.visibleOverlayCount(), 1);
    QCOMPARE(manager.anchoredOverlayCount(), 0);
    QCOMPARE(manager.visibleAnchoredOverlayCount(), 0);
    QString error;
    QVERIFY2(manager.setStackingAnchor(QStringLiteral("container-alpha"),
                                       QStringLiteral("window-alpha-b"), &error),
             qPrintable(error));
    QCOMPARE(factory.records.value(QStringLiteral("container-alpha"))->anchorId,
             QStringLiteral("window-alpha-b"));
    QCOMPARE(manager.visibleOverlayCount(), 1);
    QCOMPARE(manager.anchoredOverlayCount(), 1);
    QCOMPARE(manager.visibleAnchoredOverlayCount(), 1);
    QVERIFY2(manager.setStackingAnchor(QStringLiteral("container-alpha"),
                                       QStringLiteral("window-alpha-a"), &error),
             qPrintable(error));
    QCOMPARE(factory.records.value(QStringLiteral("container-alpha"))->anchorId,
             QStringLiteral("window-alpha-a"));

    const auto &divider = plans.constBegin().value().dividers.constFirst();
    const QPointF grabOnly(divider.hitRect.left() + 1.0, divider.hitRect.center().y());
    QVERIFY(!divider.visualRect.contains(grabOnly));
    const auto dividerHit = manager.pointerTargetAt(grabOnly);
    QVERIFY(dividerHit);
    QCOMPARE(dividerHit->target.kind, HybridChrome::HitKind::Divider);

    const auto &firstMember = plans.constBegin().value().members.constFirst();
    const QPoint nativeControl = (firstMember.titleDragRect.topLeft()
                                  + QPointF(19.0, 18.0)).toPoint();
    QVERIFY(firstMember.titleDragRect.contains(nativeControl));
    const auto nativeHit = manager.pointerTargetAt(nativeControl);
    QVERIFY(nativeHit);
    QCOMPARE(nativeHit->target.kind, HybridChrome::HitKind::MemberTitleDrag);
    QCOMPARE(nativeHit->target.stableId, firstMember.memberId);

    // Native decoration ownership overrides ChromeHitTester's widened-divider
    // precedence only at the production ordinary-input boundary.
    const QPoint dividerTitleOverlap = QPointF(
        divider.hitRect.center().x(),
        firstMember.titleDragRect.center().y()).toPoint();
    QVERIFY(divider.hitRect.contains(dividerTitleOverlap));
    QVERIFY(firstMember.titleDragRect.contains(dividerTitleOverlap));
    const auto overlapHit = manager.pointerTargetAt(dividerTitleOverlap);
    QVERIFY(overlapHit);
    QCOMPARE(overlapHit->target.kind, HybridChrome::HitKind::MemberTitleDrag);

    const auto clientHit = manager.pointerTargetAt(firstMember.windowRect.center());
    QVERIFY(clientHit);
    QCOMPARE(clientHit->target.kind, HybridChrome::HitKind::Client);
    manager.setOverlayVisible(QStringLiteral("container-alpha"), false);
    QVERIFY(!manager.overlayVisible(QStringLiteral("container-alpha")));
    QCOMPARE(manager.visibleOverlayCount(), 0);
    QCOMPARE(manager.anchoredOverlayCount(), 1);
    QCOMPARE(visibility.size(), 2);
    QVERIFY(!visibility.constLast().at(1).toBool());
    manager.quarantineContainer(QStringLiteral("container-alpha"));
    QVERIFY(manager.containerQuarantined(QStringLiteral("container-alpha")));
    QCOMPARE(manager.quarantinedContainerCount(), 1);
    QCOMPARE(visibility.size(), 2);
    QCOMPARE(manager.visibleAnchoredOverlayCount(), 0);
    QVERIFY(!manager.pointerTargetAt(plan.buttons.constFirst().rect.center()));
    manager.setOverlayVisible(QStringLiteral("container-alpha"), true);
    QCOMPARE(visibility.size(), 2);
    QVERIFY(!manager.overlayVisible(QStringLiteral("container-alpha")));
    QVERIFY2(manager.updateFromSnapshot(topology, plans, {}, &error), qPrintable(error));
    QVERIFY(!manager.overlayVisible(QStringLiteral("container-alpha")));
    manager.markContainerContextCoherent(QStringLiteral("container-alpha"));
    QCOMPARE(manager.quarantinedContainerCount(), 0);
    manager.setOverlayVisible(QStringLiteral("container-alpha"), true);
    QCOMPARE(visibility.size(), 3);
    QVERIFY(visibility.constLast().at(1).toBool());
    QVERIFY(manager.pointerTargetAt(plan.buttons.constFirst().rect.center()));
    QCOMPARE(manager.visibleAnchoredOverlayCount(), 1);
}

void KWinChromeManagerTests::recreatesSceneOverlaysAfterSynchronousClear()
{
    FakeOverlayFactory factory;
    KWinChromeManager manager(factory);
    const auto topology = makeTopology({makeContainer(QStringLiteral("alpha"))}, 3);
    const auto plans = plansFor(topology);
    QVERIFY(manager.updateFromSnapshot(topology, plans));
    QString error;
    QVERIFY2(manager.setStackingAnchor(QStringLiteral("container-alpha"),
                                       QStringLiteral("window-alpha-a"), &error),
             qPrintable(error));
    const auto first = factory.records.value(QStringLiteral("container-alpha"));
    QCOMPARE(manager.visibleAnchoredOverlayCount(), 1);

    manager.quarantineContainer(QStringLiteral("container-alpha"));
    manager.clear();
    QCOMPARE(first->closeCount, 1);
    QVERIFY(first->destroyed);

    // A compositor restart does not mutate topology, so the same revision
    // must be accepted after scene resources were synchronously discarded.
    QVERIFY2(manager.updateFromSnapshot(topology, plans, {}, &error), qPrintable(error));
    QCOMPARE(factory.createCount, 2);
    const auto replacement = factory.records.value(
        QStringLiteral("container-alpha"));
    QVERIFY(replacement != first);
    QCOMPARE(replacement->showCount, 0);
    QVERIFY(!manager.overlayVisible(QStringLiteral("container-alpha")));
    manager.markContainerContextCoherent(QStringLiteral("container-alpha"));
    manager.setOverlayVisible(QStringLiteral("container-alpha"), true);
    QVERIFY2(manager.setStackingAnchor(QStringLiteral("container-alpha"),
                                       QStringLiteral("window-alpha-b"), &error),
             qPrintable(error));
    QCOMPARE(manager.visibleAnchoredOverlayCount(), 1);
}

void KWinChromeManagerTests::validatesPointerActivationAndRoutesHoverToPaint()
{
    FakeOverlayFactory factory;
    KWinChromeManager manager(factory);
    const auto topology = makeTopology({makeContainer(QStringLiteral("alpha"))}, 3);
    const auto plans = plansFor(topology, HybridChrome::ChromeStyle::qindaMacOS({}));
    QVERIFY(manager.updateFromSnapshot(topology, plans));
    const auto plan = plans.constBegin().value();

    const auto buttonHit = manager.pointerTargetAt(plan.buttons.constFirst().rect.center());
    QVERIFY(buttonHit);
    manager.setPointerHover(buttonHit);
    QCOMPARE(factory.records.value(QStringLiteral("container-alpha"))->hoveredTarget,
             buttonHit->target);
    manager.setPointerHover(std::nullopt);
    QCOMPARE(factory.records.value(QStringLiteral("container-alpha"))->hoveredTarget,
             HybridChrome::ChromeHitTarget{});

    QSignalSpy actions(&manager, &KWinChromeManager::windowActionRequested);
    QVERIFY(manager.dispatchPointerActivation(*buttonHit));
    QCOMPARE(actions.size(), 1);

    auto staleAction = *buttonHit;
    staleAction.target.action = HybridChrome::WindowAction::Restore;
    QVERIFY(!manager.dispatchPointerActivation(staleAction));

    QSignalSpy controls(&manager, &KWinChromeManager::containerControlRequested);
    const auto controlHit = manager.pointerTargetAt(
        plan.controls.constLast().rect.center());
    QVERIFY(controlHit);
    QCOMPARE(controlHit->target.kind, HybridChrome::HitKind::ContainerControl);
    QVERIFY(manager.dispatchPointerActivation(*controlHit));
    QCOMPARE(controls.size(), 1);
    QCOMPARE(controls.constFirst().at(0).toString(),
             QStringLiteral("container-alpha"));
    QCOMPARE(controls.constFirst().at(1).value<HybridChrome::ContainerControl>(),
             HybridChrome::ContainerControl::ManagementMenu);
    auto staleControl = *controlHit;
    staleControl.target.containerControl =
        static_cast<HybridChrome::ContainerControl>(99);
    QVERIFY(!manager.dispatchPointerActivation(staleControl));

    QSignalSpy tabs(&manager, &KWinChromeManager::tabActivationRequested);
    const auto tabHit = manager.pointerTargetAt(plan.tabs.constFirst().rect.center());
    QVERIFY(tabHit);
    QVERIFY(manager.dispatchPointerActivation(*tabHit));
    QCOMPARE(tabs.size(), 1);
    auto staleTab = *tabHit;
    ++staleTab.target.logicalIndex;
    QVERIFY(!manager.dispatchPointerActivation(staleTab));
    manager.quarantineContainer(QStringLiteral("container-alpha"));
    QVERIFY(!manager.dispatchPointerActivation(*buttonHit));
}

void KWinChromeManagerTests::rejectsInvalidOrStaleSnapshotsAtomically()
{
    FakeOverlayFactory factory;
    KWinChromeManager manager(factory);
    auto alpha = makeContainer(QStringLiteral("alpha"));
    const auto initial = makeTopology({alpha}, 4);
    QVERIFY(manager.updateFromSnapshot(initial, plansFor(initial)));
    const auto record = factory.records.value(QStringLiteral("container-alpha"));

    QString error;
    const auto stale = makeTopology({alpha}, 3);
    QVERIFY(!manager.updateFromSnapshot(stale, plansFor(stale), {}, &error));
    QVERIFY(error.contains(QStringLiteral("stale")));
    QCOMPARE(record->planCount, 1);

    auto changed = alpha;
    QVERIFY(changed.activatePage(QStringLiteral("page-alpha-extra"), &error));
    const auto unrevisioned = makeTopology({changed}, 4);
    QVERIFY(!manager.updateFromSnapshot(unrevisioned, plansFor(unrevisioned), {}, &error));
    QVERIFY(error.contains(QStringLiteral("without advancing")));

    const auto next = makeTopology({alpha}, 5);
    QVERIFY(!manager.updateFromSnapshot(next, {}, {}, &error));
    QCOMPARE(manager.topologyRevision(), std::optional<quint64>(4));
    QCOMPARE(record->planCount, 1);

    auto expanded = makeTopology({alpha, makeContainer(QStringLiteral("beta"))}, 5);
    factory.failingContainerId = QStringLiteral("container-beta");
    QVERIFY(!manager.updateFromSnapshot(expanded, plansFor(expanded), {}, &error));
    QVERIFY(error.contains(QStringLiteral("failed to create")));
    QCOMPARE(manager.overlayCount(), 1);
    QCOMPARE(record->planCount, 1);

    auto malformed = plansFor(next);
    malformed[QStringLiteral("container-alpha")].members.removeLast();
    QVERIFY(!manager.updateFromSnapshot(next, malformed, {}, &error));
    QVERIFY(error.contains(QStringLiteral("members")));
    QCOMPARE(record->planCount, 1);
}

// AGENT-CONTRACT: regression for the live-run finding that a shaded
// container's chromeOverlayCount/publishedGroupStackingCount permanently
// dropped to 0 after the first shade. A shaded plan keeps topology-ordered
// tabs as compact badge pills, but must omit member and divider geometry.
void KWinChromeManagerTests::publishesAShadedBadgeWithTabsButWithoutMemberGeometry()
{
    FakeOverlayFactory factory;
    KWinChromeManager manager(factory);
    auto alpha = makeContainer(QStringLiteral("alpha"));
    QCOMPARE(alpha.pages().size(), 2);
    const auto topology = makeTopology({alpha}, 4);

    KWinChromeManager::ChromePlanMap shadedPlans;
    shadedPlans.insert(QStringLiteral("container-alpha"), makeShadedPlan(alpha));
    QString error;
    QVERIFY2(manager.updateFromSnapshot(topology, shadedPlans, {}, &error),
             qPrintable(error));
    QCOMPARE(manager.overlayCount(), 1);
    const auto record = factory.records.value(QStringLiteral("container-alpha"));
    QCOMPARE(record->planCount, 1);
    QCOMPARE(record->plan.tabs.size(), alpha.pages().size());
    QCOMPARE(record->plan.tabs.constFirst().tabId, alpha.pages().constFirst().id());
    QVERIFY(record->plan.members.isEmpty());
    QVERIFY(record->plan.dividers.isEmpty());

    auto malformedShaded = makeShadedPlan(alpha);
    malformedShaded.members.append(HybridChrome::MemberGeometry{
        .memberId = QStringLiteral("window-alpha-a"),
        .title = QStringLiteral("window-alpha-a"),
        .windowRect = QRectF(0.0, 0.0, 100.0, 26.0),
        .titleDragRect = {},
        .focused = false,
    });
    KWinChromeManager::ChromePlanMap malformedPlans;
    malformedPlans.insert(QStringLiteral("container-alpha"), malformedShaded);
    const auto next = makeTopology({alpha}, 5);
    QVERIFY(!manager.updateFromSnapshot(next, malformedPlans, {}, &error));
    QVERIFY(error.contains(QStringLiteral("members and dividers")));
    // Rejection is atomic: the previously published (valid) shaded entry
    // survives untouched, matching rejectsInvalidOrStaleSnapshotsAtomically.
    QCOMPARE(manager.overlayCount(), 1);
    QCOMPARE(record->planCount, 1);
}

QTEST_MAIN(KWinChromeManagerTests)
#include "tst_kwinchromemanager.moc"
