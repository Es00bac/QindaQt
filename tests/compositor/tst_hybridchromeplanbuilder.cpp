// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridchromeplanbuilder.h"

#include "qindaqt/hybrid_constraints/constraint_solver.h"

#include <QHash>
#include <QImage>
#include <QPainter>
#include <QtTest>

#include "qindaqt/hybrid_chrome/chromerenderer.h"
#include "qindaqt/hybrid_chrome/chromeshadedbadge.h"

namespace QindaQt::Compositor::KWinIntegration {
namespace {

Core::WindowContainer sampleContainer()
{
    Core::WindowContainer container(QStringLiteral("group"));
    QString error;
    if (!container.addPage(QStringLiteral("work"), QStringLiteral("left-leaf"),
                           QStringLiteral("left"), &error)) {
        qFatal("test fixture could not add work page: %s", qPrintable(error));
    }
    if (!container.splitWindow({.targetWindowId = QStringLiteral("left"),
                                .newWindowId = QStringLiteral("right"),
                                .newLeafNodeId = QStringLiteral("right-leaf"),
                                .splitNodeId = QStringLiteral("main-divider"),
                                .orientation = Core::SplitOrientation::Horizontal,
                                .ratio = 0.5,
                                .position = Core::InsertPosition::Second},
                               &error)) {
        qFatal("test fixture could not split work page: %s", qPrintable(error));
    }
    if (!container.addPage(QStringLiteral("chat"), QStringLiteral("chat-leaf"),
                           QStringLiteral("chat-window"), &error)) {
        qFatal("test fixture could not add chat page: %s", qPrintable(error));
    }
    return container;
}

HybridConstraints::LayoutMetrics sceneMetrics()
{
    return {.contentInsets = QMargins(1, 29, 1, 1), .dividerThickness = 2};
}

HybridChromePlanOptions chromeOptions()
{
    HybridChromePlanOptions result;
    result.devicePixelRatio = 2.0;
    result.containerFocused = true;
    result.focusedMemberId = QStringLiteral("left");
    result.style = HybridChrome::ChromeStyle::qindaMacOS({});
    return result;
}

std::optional<HybridConstraints::ConstraintSolution> solve(
    const Core::WindowContainer &container,
    const QHash<QString, HybridConstraints::MemberSizeConstraints> &constraints = {})
{
    return HybridConstraints::ConstraintSolver::solve(
        container.page(container.activePageId())->root(),
        QRect(100, 60, 1000, 700), constraints, sceneMetrics());
}

} // namespace

class HybridChromePlanBuilderTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void productionShadedPlanPaintsItsLabel();
    void buildsQindaMacPlanInStableTopologyOrder();
    void forwardsSessionMemberTitleChoice();
    void usesActualWindowFramesForBoundedMembers();
    void rejectsChromeMetricsThatDisagreeWithScene();
    void rejectsStaleCommittedGeometry();
};

void HybridChromePlanBuilderTest::buildsQindaMacPlanInStableTopologyOrder()
{
    const auto container = sampleContainer();
    const auto solution = solve(container);
    QVERIFY(solution.has_value());

    QString error;
    const auto plan = HybridChromePlanBuilder::build(
        container, *solution, chromeOptions(),
        [](const QString &id) {
            const QHash<QString, QString> titles{
                {QStringLiteral("left"), QStringLiteral("Files")},
                {QStringLiteral("right"), QStringLiteral("Terminal")},
                {QStringLiteral("chat-window"), QStringLiteral("Chat")},
            };
            return titles.value(id);
        },
        &error);

    QVERIFY2(plan.has_value(), qPrintable(error));
    QCOMPARE(plan->style.buttonSide, HybridChrome::ButtonSide::Left);
    QCOMPARE(plan->style.buttonStyle, HybridChrome::ButtonStyle::TrafficLights);
    QCOMPARE(plan->style.tabDirection, HybridChrome::TabVisualDirection::RightToLeft);
    QVERIFY(plan->style.hoverGlyphs);
    QCOMPARE(plan->tabs.size(), 2);
    QCOMPARE(plan->tabs[0].tabId, QStringLiteral("work"));
    QCOMPARE(plan->tabs[0].title, QStringLiteral("Files +1"));
    QCOMPARE(plan->tabs[0].logicalIndex, 0);
    QVERIFY(plan->tabs[0].rect.left() > plan->tabs[1].rect.left());
    QCOMPARE(plan->members.size(), 2);
    QCOMPARE(plan->members[0].memberId, QStringLiteral("left"));
    QVERIFY(plan->containerFocused);
    QVERIFY(plan->memberTitlesVisible);
    QVERIFY(plan->members[0].focused);
    QVERIFY(!plan->members[1].focused);
    QCOMPARE(plan->members[1].memberId, QStringLiteral("right"));
    QCOMPARE(plan->dividers.size(), 1);
    QCOMPARE(plan->dividers[0].dividerId, QStringLiteral("main-divider"));
    QCOMPARE(plan->dividers[0].orientation, HybridChrome::DividerOrientation::Vertical);
    QCOMPARE(plan->contentRect, QRectF(solution->contentFrame));
}

void HybridChromePlanBuilderTest::forwardsSessionMemberTitleChoice()
{
    const auto container = sampleContainer();
    const auto solution = solve(container);
    QVERIFY(solution);
    auto options = chromeOptions();
    options.memberTitlesVisible = false;
    QString error;
    const auto plan = HybridChromePlanBuilder::build(
        container, *solution, options, {}, &error);
    QVERIFY2(plan, qPrintable(error));
    QVERIFY(!plan->memberTitlesVisible);
    for (const auto &member : plan->members) {
        QVERIFY(member.titleDragRect.isEmpty());
    }
}

void HybridChromePlanBuilderTest::usesActualWindowFramesForBoundedMembers()
{
    const auto container = sampleContainer();
    const QHash<QString, HybridConstraints::MemberSizeConstraints> constraints{
        {QStringLiteral("left"),
         {.minimumSize = QSize(80, 60),
          .maximumSize = std::nullopt,
          .fixedSize = QSize(260, 220)}},
        {QStringLiteral("right"),
         {.minimumSize = QSize(80, 60),
          .maximumSize = QSize(320, 280),
          .fixedSize = std::nullopt}},
    };
    const auto solution = solve(container, constraints);
    QVERIFY(solution);
    QVERIFY(solution->members.value(QStringLiteral("left")).windowFrame
            != solution->members.value(QStringLiteral("left")).tileFrame);
    QVERIFY(solution->members.value(QStringLiteral("right")).windowFrame
            != solution->members.value(QStringLiteral("right")).tileFrame);

    QString error;
    const auto plan = HybridChromePlanBuilder::build(
        container, *solution, chromeOptions(), {}, &error);
    QVERIFY2(plan, qPrintable(error));

    QCOMPARE(plan->members[0].windowRect,
             QRectF(solution->members.value(QStringLiteral("left")).windowFrame));
    QCOMPARE(plan->members[1].windowRect,
             QRectF(solution->members.value(QStringLiteral("right")).windowFrame));
    QCOMPARE(plan->members[0].titleDragRect.topLeft(),
             plan->members[0].windowRect.topLeft());
    QCOMPARE(plan->members[1].titleDragRect.width(),
             plan->members[1].windowRect.width());
}

void HybridChromePlanBuilderTest::rejectsChromeMetricsThatDisagreeWithScene()
{
    const auto container = sampleContainer();
    const auto solution = solve(container);
    QVERIFY(solution.has_value());
    auto options = chromeOptions();
    options.metrics.titleBarHeight += 4.0;

    QString error;
    const auto plan = HybridChromePlanBuilder::build(
        container, *solution, options, {}, &error);

    QVERIFY(!plan.has_value());
    QCOMPARE(error, QStringLiteral("scene and chrome content frames do not match"));
}

void HybridChromePlanBuilderTest::rejectsStaleCommittedGeometry()
{
    const auto container = sampleContainer();
    auto solution = solve(container);
    QVERIFY(solution.has_value());
    solution->members.insert(QStringLiteral("stale-window"), {});

    QString error;
    const auto plan = HybridChromePlanBuilder::build(
        container, *solution, chromeOptions(), {}, &error);

    QVERIFY(!plan.has_value());
    QCOMPARE(error, QStringLiteral("committed solution contains stale active-page geometry"));
}


// ADR-0189, end to end through the compositor's own builder. The pure
// badge rows build a request by hand; this one builds the *production*
// shaded plan and paints it, because the two halves agreeing separately is
// exactly what let a correctly-sized strip ship with an unpainted label.
void HybridChromePlanBuilderTest::productionShadedPlanPaintsItsLabel()
{
    Core::WindowContainer container(QStringLiteral("group"));
    QString error;
    QVERIFY2(container.addPage(QStringLiteral("work"), QStringLiteral("leaf"),
                               QStringLiteral("window-a"), &error),
             qPrintable(error));

    HybridChromePlanOptions options;
    options.shaded = true;
    options.containerTitle = QStringLiteral("Container 7");
    options.containerTitleIsGenerated = true;
    const QString caption =
        QStringLiteral("Quarterly revenue model, revision 12");
    const auto titleLookup = [&caption](const QString &) { return caption; };

    const auto label =
        HybridChromePlanBuilder::shadedLabel(container, options, titleLookup);
    QCOMPARE(label.text, caption);
    QVERIFY(label.width > 140.0);
    options.shadedOuterFrame =
        QRectF(0.0, 0.0,
               HybridChrome::ChromeShadedBadge::badgeWidth(options.metrics, 1, label.width)
                   + 2.0 * options.metrics.outerBorder,
               31.0);

    const auto plan = HybridChromePlanBuilder::build(
        container, {}, options, titleLookup, &error);
    QVERIFY2(plan.has_value(), qPrintable(error));
    QCOMPARE(plan->badgeLabelText, caption);
    QVERIFY(plan->badgeLabelRect.width() > 140.0);

    QImage image(plan->outerFrame.size().toSize(),
                 QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    {
        QPainter painter(&image);
        HybridChrome::ChromeRenderer::paint(painter, *plan, {});
    }

    // Glyph ink only.
    //
    // AGENT-GUARD: bound *both* axes to the label rect, inset past the
    // identity frame. ChromeRenderer strokes a 2 px full-perimeter frame in
    // identity.border on shaded plans too, so scanning the whole image height
    // counts border pixels at every column and a blank label passes. That
    // mistake was made here and caught by blanking the label as a negative
    // control.
    const auto surface = plan->style.palette.surface;
    const QRectF band = plan->badgeLabelRect.adjusted(4.0, 3.0, -4.0, -3.0);
    QVERIFY(band.isValid() && !band.isEmpty());
    int inkPixels = 0;
    for (int x = qMax(0, qRound(band.left()));
         x <= qMin(image.width() - 1, qRound(band.right())); ++x) {
        for (int y = qMax(0, qRound(band.top()));
             y <= qMin(image.height() - 1, qRound(band.bottom())); ++y) {
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
             qPrintable(QStringLiteral("label rect %1 wide painted %2 ink pixels")
                            .arg(plan->badgeLabelRect.width())
                            .arg(inkPixels)));
}

} // namespace QindaQt::Compositor::KWinIntegration

QTEST_MAIN(QindaQt::Compositor::KWinIntegration::HybridChromePlanBuilderTest)

#include "tst_hybridchromeplanbuilder.moc"
