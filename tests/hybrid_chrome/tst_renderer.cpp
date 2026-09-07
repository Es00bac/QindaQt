// SPDX-License-Identifier: GPL-3.0-or-later
#include "testfixtures.h"

#include "qindaqt/hybrid_chrome/chromelayoutengine.h"
#include "qindaqt/hybrid_chrome/chromerenderer.h"
#include "qindaqt/hybrid_chrome/chromewidget.h"

#include <QImage>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMouseEvent>
#include <QPainter>
#include <QSignalSpy>
#include <QtTest>

#include <algorithm>
#include <cmath>
#include <optional>

using namespace QindaQt::HybridChrome;
using namespace QindaQt::HybridChrome::TestFixtures;

namespace {

QImage render(const ChromeRenderPlan &plan, const ChromePaintState &state = {})
{
    const auto physicalSize = QSize(qCeil(plan.outerFrame.width() * plan.devicePixelRatio),
                                    qCeil(plan.outerFrame.height() * plan.devicePixelRatio));
    QImage image(physicalSize, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(plan.devicePixelRatio);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    ChromeRenderer::paint(painter, plan, state);
    return image;
}

QPoint physicalPoint(const QPointF &logical, qreal devicePixelRatio)
{
    return {qRound(logical.x() * devicePixelRatio),
            qRound(logical.y() * devicePixelRatio)};
}

std::optional<ChromePalette> paletteFromTheme(const QString &themeId)
{
    QFile file(QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/%1.json").arg(themeId));
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return std::nullopt;
    }
    const auto colors = document.object().value(QStringLiteral("colors")).toObject();
    ChromePalette palette;
    const auto color = [&colors](const QString &name) {
        return QColor(colors.value(name).toString());
    };
    palette.surface = color(QStringLiteral("surface"));
    palette.surfaceRaised = color(QStringLiteral("surfaceRaised"));
    palette.border = color(QStringLiteral("border"));
    palette.text = color(QStringLiteral("text"));
    palette.textMuted = color(QStringLiteral("textMuted"));
    palette.accent = color(QStringLiteral("accent"));
    palette.close = color(QStringLiteral("danger"));
    palette.minimize = palette.accent;
    palette.maximize = palette.accent;
    QString error;
    if (!palette.isValid(&error)) {
        return std::nullopt;
    }
    return palette;
}

void saveEvidence(const QImage &image, const QString &name)
{
    const auto directory = qEnvironmentVariable("QINDAQT_ACTIVE_FRAME_EVIDENCE_DIR");
    if (directory.isEmpty()) {
        return;
    }
    QVERIFY(QDir().mkpath(directory));
    QVERIFY(image.save(QDir(directory).filePath(name), "PNG"));
}

void sendMouse(ChromeWidget &widget,
               QEvent::Type type,
               const QPointF &localPosition,
               const QPointF &globalPosition,
               Qt::MouseButton button,
               Qt::MouseButtons buttons)
{
    QMouseEvent event(type, localPosition, globalPosition, button, buttons, Qt::NoModifier);
    QCoreApplication::sendEvent(&widget, &event);
}

} // namespace

class ChromeRendererTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void trafficLightGlyphsAppearOnControlHover();
    void groupControlsUsePlanGeometryAndPalette();
    void renamedContainerPaintsTitleInAccentColor();
    void activeTabGetsThemeAccentCue();
    void focusedContainerAndMemberGetThemeAccentCues();
    void rendersAtDevicePixelRatioWithoutChangingLogicalPlan();
    void leavesCompleteMemberFramesTransparent();
    void clearsPixelsThatBecomeMemberFramesAfterReflow();
    void widgetEmitsTypedActionTarget();
    void widgetEmitsThresholdedDragLifecycle();
    void widgetCancelsDragOnPointerUngrab();
};

void ChromeRendererTests::trafficLightGlyphsAppearOnControlHover()
{
    const auto plan = ChromeLayoutEngine::build(qindaMacRequest());
    QVERIFY(plan);
    const auto idle = render(*plan);
    ChromePaintState hovered;
    hovered.controlsHovered = true;
    const auto active = render(*plan, hovered);
    const auto center = physicalPoint(plan->buttons[0].rect.center(), plan->devicePixelRatio);
    QCOMPARE(idle.pixelColor(center), plan->style.palette.close);
    QVERIFY(active.pixelColor(center) != idle.pixelColor(center));
    QVERIFY(active != idle);
}

void ChromeRendererTests::groupControlsUsePlanGeometryAndPalette()
{
    auto request = baseRequest();
    const auto visiblePlan = ChromeLayoutEngine::build(request);
    QVERIFY(visiblePlan);
    QCOMPARE(visiblePlan->controls.size(), 3);
    const auto idle = render(*visiblePlan);
    for (const auto &control : visiblePlan->controls) {
        QVERIFY(idle.pixelColor(physicalPoint(control.rect.center(),
                                              visiblePlan->devicePixelRatio)).alpha() > 0);
    }

    ChromePaintState hovered;
    hovered.hoveredTarget = {
        HitKind::ContainerControl, visiblePlan->containerId, -1,
        std::nullopt, {}, ContainerControl::ManagementMenu};
    const auto active = render(*visiblePlan, hovered);
    QVERIFY(active != idle);

    request.memberTitlesVisible = false;
    const auto hiddenPlan = ChromeLayoutEngine::build(request);
    QVERIFY(hiddenPlan);
    QVERIFY(render(*hiddenPlan) != idle);
}

void ChromeRendererTests::renamedContainerPaintsTitleInAccentColor()
{
    auto request = baseRequest();
    const auto unnamedPlan = ChromeLayoutEngine::build(request);
    QVERIFY(unnamedPlan);
    QVERIFY(unnamedPlan->outerTitleDragRect.width() > 0.0);
    const auto dragCenter = physicalPoint(unnamedPlan->outerTitleDragRect.center(),
                                         unnamedPlan->devicePixelRatio);
    const auto unnamedImage = render(*unnamedPlan);

    request.containerTitle = QStringLiteral("Research Stack");
    const auto namedPlan = ChromeLayoutEngine::build(request);
    QVERIFY(namedPlan);
    QCOMPARE(namedPlan->outerTitleDragRect, unnamedPlan->outerTitleDragRect);
    const auto namedImage = render(*namedPlan);

    // Painting the rename text into the previously-empty drag region changes
    // at least one pixel there; an unnamed container leaves it exactly the
    // tab strip's fill (baseRequest() has tabs, so the tab strip covers the
    // whole outerTitleBar) with no separate text color.
    QVERIFY(namedImage != unnamedImage);
    QCOMPARE(unnamedImage.pixelColor(dragCenter), namedPlan->style.palette.surface);
}

void ChromeRendererTests::activeTabGetsThemeAccentCue()
{
    for (const auto &themeId : {QStringLiteral("qinda-dark"), QStringLiteral("qinda-light")}) {
        const auto palette = paletteFromTheme(themeId);
        QVERIFY2(palette.has_value(), qPrintable(themeId));
        auto request = baseRequest();
        request.style.palette = *palette;
        const auto plan = ChromeLayoutEngine::build(request);
        QVERIFY(plan);
        const auto image = render(*plan);
        const auto activeTab = std::find_if(plan->tabs.cbegin(), plan->tabs.cend(),
                                            [](const auto &tab) { return tab.active; });
        QVERIFY(activeTab != plan->tabs.cend());
        const auto inactiveTab = std::find_if(plan->tabs.cbegin(), plan->tabs.cend(),
                                              [](const auto &tab) { return !tab.active; });
        QVERIFY(inactiveTab != plan->tabs.cend());
        const auto activeCue = physicalPoint(
            QPointF(activeTab->rect.center().x(), activeTab->rect.bottom() - 1.0),
            plan->devicePixelRatio);
        const auto inactiveCue = physicalPoint(
            QPointF(inactiveTab->rect.center().x(), inactiveTab->rect.bottom() - 1.0),
            plan->devicePixelRatio);
        QCOMPARE(image.pixelColor(activeCue), plan->style.palette.accent);
        QVERIFY(image.pixelColor(inactiveCue) != plan->style.palette.accent);
        saveEvidence(image, themeId + QStringLiteral("-active-tab.png"));
    }
}

void ChromeRendererTests::focusedContainerAndMemberGetThemeAccentCues()
{
    for (const auto &themeId : {QStringLiteral("qinda-dark"), QStringLiteral("qinda-light")}) {
        const auto palette = paletteFromTheme(themeId);
        QVERIFY2(palette.has_value(), qPrintable(themeId));

        auto unfocusedRequest = baseRequest();
        unfocusedRequest.style.palette = *palette;
        const auto unfocusedPlan = ChromeLayoutEngine::build(unfocusedRequest);
        QVERIFY(unfocusedPlan);
        const auto unfocusedImage = render(*unfocusedPlan);

        auto focusedRequest = unfocusedRequest;
        focusedRequest.containerFocused = true;
        focusedRequest.members[0].focused = true;
        const auto focusedPlan = ChromeLayoutEngine::build(focusedRequest);
        QVERIFY(focusedPlan);
        const auto focusedImage = render(*focusedPlan);

        const auto outerFrameSample = physicalPoint(QPointF(500.0, 0.0),
                                                    focusedPlan->devicePixelRatio);
        QCOMPARE(focusedImage.pixelColor(outerFrameSample), focusedPlan->style.palette.accent);
        QVERIFY(unfocusedImage.pixelColor(outerFrameSample)
                != unfocusedPlan->style.palette.accent);

        const auto focusedMemberEdge = physicalPoint(QPointF(0.0, 350.0),
                                                     focusedPlan->devicePixelRatio);
        const auto siblingEdge = physicalPoint(QPointF(999.0, 350.0),
                                               focusedPlan->devicePixelRatio);
        QCOMPARE(focusedImage.pixelColor(focusedMemberEdge), focusedPlan->style.palette.accent);
        QVERIFY(focusedImage.pixelColor(siblingEdge) != focusedPlan->style.palette.accent);
        QCOMPARE(focusedImage.pixelColor(physicalPoint(focusedPlan->members[0].windowRect.center(),
                                                        focusedPlan->devicePixelRatio)),
                 QColor(Qt::transparent));

        auto siblingFocusedRequest = focusedRequest;
        siblingFocusedRequest.members[0].focused = false;
        siblingFocusedRequest.members[1].focused = true;
        const auto siblingFocusedPlan = ChromeLayoutEngine::build(siblingFocusedRequest);
        QVERIFY(siblingFocusedPlan);
        const auto siblingFocusedImage = render(*siblingFocusedPlan);
        QCOMPARE(siblingFocusedImage.pixelColor(siblingEdge),
                 siblingFocusedPlan->style.palette.accent);
        QVERIFY(siblingFocusedImage.pixelColor(focusedMemberEdge)
                != siblingFocusedPlan->style.palette.accent);
        saveEvidence(focusedImage, themeId + QStringLiteral("-focused-frame.png"));
        saveEvidence(unfocusedImage, themeId + QStringLiteral("-unfocused-frame.png"));
    }
}

void ChromeRendererTests::rendersAtDevicePixelRatioWithoutChangingLogicalPlan()
{
    auto request = qindaMacRequest();
    request.devicePixelRatio = 2.0;
    const auto plan = ChromeLayoutEngine::build(request);
    QVERIFY(plan);
    const auto image = render(*plan);
    QCOMPARE(image.size(), QSize(2000, 1400));
    QCOMPARE(image.devicePixelRatio(), 2.0);
    QCOMPARE(plan->outerFrame, QRectF(0.0, 0.0, 1000.0, 700.0));
    QCOMPARE(plan->borderHairline, 0.5);
}

void ChromeRendererTests::leavesCompleteMemberFramesTransparent()
{
    const auto plan = ChromeLayoutEngine::build(baseRequest());
    QVERIFY(plan);
    const auto image = render(*plan);
    for (const auto &member : plan->members) {
        const QVector<QPointF> samples{
            member.titleDragRect.center(),
            member.windowRect.center(),
            member.windowRect.bottomRight() - QPointF(2.0, 2.0),
        };
        for (const auto &sample : samples) {
            QCOMPARE(image.pixelColor(physicalPoint(sample, plan->devicePixelRatio)),
                     QColor(Qt::transparent));
        }
    }
    QVERIFY(image.pixelColor(physicalPoint(plan->outerTitleBar.center(),
                                           plan->devicePixelRatio)).alpha() > 0);
    QVERIFY(image.pixelColor(physicalPoint(plan->dividers.constFirst().visualRect.center(),
                                           plan->devicePixelRatio)).alpha() > 0);
}

void ChromeRendererTests::clearsPixelsThatBecomeMemberFramesAfterReflow()
{
    auto firstRequest = baseRequest();
    const auto first = ChromeLayoutEngine::build(firstRequest);
    QVERIFY(first);

    auto second = *first;
    second.members[0].windowRect.setTop(first->outerTitleBar.top());

    QImage image(first->outerFrame.size().toSize(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    ChromeRenderer::paint(painter, *first);
    const QPointF newlyCovered = second.members[0].windowRect.topLeft()
        + QPointF(8.0, 8.0);
    QVERIFY(image.pixelColor(newlyCovered.toPoint()).alpha() > 0);
    ChromeRenderer::paint(painter, second);
    QCOMPARE(image.pixelColor(newlyCovered.toPoint()), QColor(Qt::transparent));
}

void ChromeRendererTests::widgetEmitsTypedActionTarget()
{
    const auto plan = ChromeLayoutEngine::build(qindaMacRequest());
    QVERIFY(plan);
    ChromeWidget widget;
    widget.resize(1000, 700);
    widget.setRenderPlan(*plan);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QSignalSpy spy(&widget, &ChromeWidget::targetActivated);
    const auto closeCenter = plan->buttons[0].rect.center().toPoint();
    QTest::mouseClick(&widget, Qt::LeftButton, Qt::NoModifier, closeCenter);
    QCOMPARE(spy.size(), 1);
    const auto target = qvariant_cast<ChromeHitTarget>(spy.constFirst().constFirst());
    QCOMPARE(target.kind, HitKind::WindowButton);
    QVERIFY(target.action);
    QCOMPARE(*target.action, WindowAction::Close);
}

void ChromeRendererTests::widgetEmitsThresholdedDragLifecycle()
{
    const auto plan = ChromeLayoutEngine::build(baseRequest());
    QVERIFY(plan);
    const QVector<QPair<HitKind, QPointF>> regions = {
        {HitKind::MemberTitleDrag, plan->members[0].titleDragRect.center()},
        {HitKind::OuterTitleDrag, plan->outerTitleDragRect.center()},
        {HitKind::Divider, plan->dividers[0].hitRect.center()},
    };
    for (const auto &[expectedKind, pressLocal] : regions) {
        ChromeWidget widget;
        widget.resize(1000, 700);
        widget.setRenderPlan(*plan);
        QSignalSpy drags(&widget, &ChromeWidget::dragLifecycle);
        QSignalSpy activations(&widget, &ChromeWidget::targetActivated);
        const QPointF pressGlobal = pressLocal + QPointF(100.0, 100.0);
        sendMouse(widget, QEvent::MouseButtonPress, pressLocal, pressGlobal,
                  Qt::LeftButton, Qt::LeftButton);
        sendMouse(widget, QEvent::MouseMove, pressLocal + QPointF(40.0, 0.0),
                  pressGlobal + QPointF(40.0, 0.0), Qt::NoButton, Qt::LeftButton);
        sendMouse(widget, QEvent::MouseMove, pressLocal + QPointF(50.0, 5.0),
                  pressGlobal + QPointF(50.0, 5.0), Qt::NoButton, Qt::LeftButton);
        sendMouse(widget, QEvent::MouseButtonRelease, pressLocal + QPointF(55.0, 5.0),
                  pressGlobal + QPointF(55.0, 5.0), Qt::LeftButton, Qt::NoButton);

        QCOMPARE(drags.size(), 3);
        const auto begin = drags[0][0].value<ChromeDragEvent>();
        const auto update = drags[1][0].value<ChromeDragEvent>();
        const auto commit = drags[2][0].value<ChromeDragEvent>();
        QCOMPARE(begin.target.kind, expectedKind);
        QCOMPARE(begin.phase, DragPhase::Begin);
        QCOMPARE(begin.delta, QPointF(40.0, 0.0));
        QCOMPARE(update.phase, DragPhase::Update);
        QCOMPARE(update.delta, QPointF(50.0, 5.0));
        QCOMPARE(commit.phase, DragPhase::Commit);
        QCOMPARE(commit.delta, QPointF(55.0, 5.0));
        QCOMPARE(activations.size(), 0);
    }
}

void ChromeRendererTests::widgetCancelsDragOnPointerUngrab()
{
    const auto plan = ChromeLayoutEngine::build(baseRequest());
    QVERIFY(plan);
    ChromeWidget widget;
    widget.resize(1000, 700);
    widget.setRenderPlan(*plan);
    QSignalSpy drags(&widget, &ChromeWidget::dragLifecycle);
    QSignalSpy activations(&widget, &ChromeWidget::targetActivated);
    const auto local = plan->outerTitleDragRect.center();
    const auto global = local + QPointF(100.0, 100.0);
    sendMouse(widget, QEvent::MouseButtonPress, local, global,
              Qt::LeftButton, Qt::LeftButton);
    sendMouse(widget, QEvent::MouseMove, local + QPointF(40.0, 0.0),
              global + QPointF(40.0, 0.0), Qt::NoButton, Qt::LeftButton);
    QEvent ungrab(QEvent::UngrabMouse);
    QCoreApplication::sendEvent(&widget, &ungrab);
    sendMouse(widget, QEvent::MouseButtonRelease, local + QPointF(40.0, 0.0),
              global + QPointF(40.0, 0.0), Qt::LeftButton, Qt::NoButton);

    QCOMPARE(drags.size(), 2);
    QCOMPARE(drags[0][0].value<ChromeDragEvent>().phase, DragPhase::Begin);
    QCOMPARE(drags[1][0].value<ChromeDragEvent>().phase, DragPhase::Cancel);
    QCOMPARE(activations.size(), 0);
}

QTEST_MAIN(ChromeRendererTests)
#include "tst_renderer.moc"
