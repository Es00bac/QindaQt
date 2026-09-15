// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridcontainerplacement.h"

#include "hybridshadestripgeometry.h"

#include <QtTest>

#include <limits>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

Hybrid::WindowTopology sampleTopology()
{
    Core::WindowContainer container(QStringLiteral("group"));
    QString error;
    if (!container.addPage(QStringLiteral("page"), QStringLiteral("left-leaf"),
                           QStringLiteral("left"), &error)
        || !container.splitWindow({.targetWindowId = QStringLiteral("left"),
                                   .newWindowId = QStringLiteral("right"),
                                   .newLeafNodeId = QStringLiteral("right-leaf"),
                                   .splitNodeId = QStringLiteral("divider"),
                                   .orientation = Core::SplitOrientation::Horizontal,
                                   .ratio = 0.5,
                                   .position = Core::InsertPosition::Second},
                                  &error)) {
        qFatal("could not build placement fixture: %s", qPrintable(error));
    }
    auto result = Hybrid::WindowTopology::create({}, {std::move(container)}, 3, &error);
    if (!result) {
        qFatal("could not build placement topology: %s", qPrintable(error));
    }
    return std::move(*result);
}

CommittedContainerLayout sampleLayout()
{
    HybridConstraints::ConstraintSolution solution{
        .outerFrame = QRect(100, 100, 800, 600),
        .contentFrame = QRect(101, 169, 798, 530),
        .requiredContentSize = {},
        .overflow = {},
        .members = {},
        .splits = {},
    };
    solution.splits.insert(
        QStringLiteral("divider"),
        {.frame = solution.contentFrame,
         .firstTileFrame = QRect(101, 169, 398, 530),
         .dividerFrame = QRect(499, 169, 2, 530),
         .secondTileFrame = QRect(501, 169, 398, 530),
         .preferredRatio = 0.5,
         .effectiveRatio = 0.5,
         .primaryMinimumsSatisfied = true});
    return {solution.outerFrame, std::move(solution)};
}

HybridInput::InteractionIntent moveIntent(HybridInput::IntentPhase phase,
                                          QPointF delta = {})
{
    return {.kind = HybridInput::InteractionKind::ContainerMove,
            .phase = phase,
            .source = {HybridInput::HitKind::OuterTitle,
                       QStringLiteral("group"), {}, {}},
            .target = {},
            .position = {},
            .delta = delta};
}

HybridInput::InteractionIntent resizeIntent(
    HybridInput::IntentPhase phase,
    QPointF delta = {},
    Qt::Edges edges = Qt::RightEdge | Qt::BottomEdge,
    QString containerId = QStringLiteral("group"))
{
    return {.kind = HybridInput::InteractionKind::ContainerResize,
            .phase = phase,
            .source = {HybridInput::HitKind::OuterResize,
                       std::move(containerId), {}, {}, edges},
            .target = {},
            .position = {},
            .delta = delta};
}

HybridChrome::ChromeDragEvent resizeEvent(HybridChrome::DragPhase phase,
                                          QPointF delta,
                                          Qt::Edges edges = Qt::RightEdge)
{
    return {.target = {.kind = HybridChrome::HitKind::OuterResize,
                       .stableId = {},
                       .logicalIndex = -1,
                       .action = std::nullopt,
                       .resizeEdges = edges,
                       .containerControl = std::nullopt},
            .phase = phase,
            .globalPosition = {},
            .delta = delta};
}

class Fixture final
{
public:
    Fixture()
        : topology(sampleTopology())
        , layout(sampleLayout())
        , controller(
              [this]() -> const Hybrid::WindowTopology & { return topology; },
              [this](const QString &id) -> std::optional<CommittedContainerLayout> {
                  return id == QStringLiteral("group")
                      ? std::optional<CommittedContainerLayout>(layout)
                      : std::nullopt;
              },
              [this](const Core::WindowContainer &, const QRect &frame) {
                  requestedFrames.append(frame);
                  if (failNext) {
                      failNext = false;
                      return Hybrid::SceneStepResult::failure(
                          QStringLiteral("reflow sentinel"));
                  }
                  const QPoint offset = frame.topLeft() - layout.outerFrame.topLeft();
                  layout.outerFrame = frame;
                  layout.activePage.outerFrame = frame;
                  layout.activePage.contentFrame.translate(offset);
                  for (auto iterator = layout.activePage.splits.begin();
                       iterator != layout.activePage.splits.end(); ++iterator) {
                      iterator->frame.translate(offset);
                      iterator->firstTileFrame.translate(offset);
                      iterator->dividerFrame.translate(offset);
                      iterator->secondTileFrame.translate(offset);
                  }
                  return Hybrid::SceneStepResult::ready();
              },
              [this](const QString &) { return workArea; },
              [this] { ++changedCount; })
    {
    }

    Hybrid::WindowTopology topology;
    CommittedContainerLayout layout;
    QRect workArea{0, 0, 1920, 1040};
    QVector<QRect> requestedFrames;
    int changedCount = 0;
    bool failNext = false;
    HybridContainerPlacementController controller;
};

} // namespace

class HybridContainerPlacementTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void movesFromOneStableBaselineAndCancels();
    void computesDividerRatioFromCommittedSplitGeometry();
    void resizesFromOneStableBaselineAndCancels();
    void commitsKeyboardResizeWithoutReapplyingSameFrame();
    void resizesEdgesAndEnforcesMinimumFrame();
    void maximizesAndRestoresWholeContainer();
    void tracksVisibleAndHiddenWorkAreasWhileMaximized();
    void shadesAndUnshadesAnchoringTheBadgeStripAtTheLeftEdge();
    void rejectsShadeWhileMaximizedAndMaximizeWhileShaded();
    void rejectsOuterResizeWhileShadedButStillAllowsMove();
    void cancelledShadedMoveRestoresTheStripsPriorPosition();
    void forgettingContainerClearsShadeRestoreFrame();
    void reportsReflowFailureWithoutAdvancingAppliedFrame();
    void failedCommitReleasesPlacementBaseline();
    void rejectsUnavailableOrInconsistentResizeState();
    void aspectPinKeepsRatioOnEdgeAndCornerDrags();
    void aspectPinAnchorsTheFollowerEdgeOnTopLeftDrags();
    void aspectPinYieldsToMinimumFrameSizes();
    void aspectPinSurvivesMaximizeRestoreButMaximizeIgnoresIt();
    void aspectPinCancelRestoresBaselineAndUnpinRestoresFreeResize();
    void keyboardResizeComposesWithAspectPin();
    void aspectPinValidationContentRatioHelperAndForget();
};

void HybridContainerPlacementTest::movesFromOneStableBaselineAndCancels()
{
    Fixture fixture;
    QVERIFY(fixture.controller.handleMove(moveIntent(HybridInput::IntentPhase::Begin)).accepted);
    QVERIFY(fixture.controller.handleMove(
        moveIntent(HybridInput::IntentPhase::Update, QPointF(20, 10))).accepted);
    QVERIFY(fixture.controller.handleMove(
        moveIntent(HybridInput::IntentPhase::Update, QPointF(30, 15))).accepted);
    QCOMPARE(fixture.requestedFrames[0], QRect(120, 110, 800, 600));
    QCOMPARE(fixture.requestedFrames[1], QRect(130, 115, 800, 600));

    QVERIFY(fixture.controller.handleMove(
        moveIntent(HybridInput::IntentPhase::Cancel, QPointF(30, 15))).accepted);
    QCOMPARE(fixture.requestedFrames.constLast(), QRect(100, 100, 800, 600));
    QCOMPARE(fixture.layout.outerFrame, QRect(100, 100, 800, 600));
    QCOMPARE(fixture.changedCount, 3);
}

void HybridContainerPlacementTest::computesDividerRatioFromCommittedSplitGeometry()
{
    Fixture fixture;
    const HybridInput::InteractionIntent intent{
        .kind = HybridInput::InteractionKind::DividerResize,
        .phase = HybridInput::IntentPhase::Commit,
        .source = {HybridInput::HitKind::Divider,
                   QStringLiteral("group"), {}, QStringLiteral("divider")},
        .target = {},
        .position = {},
        .delta = QPointF(40, 0),
    };

    const auto result = fixture.controller.dividerRatio(intent);
    QVERIFY(result.ratio.has_value());
    QCOMPARE(*result.ratio, (398.0 + 40.0) / 796.0);
}

void HybridContainerPlacementTest::resizesFromOneStableBaselineAndCancels()
{
    Fixture fixture;
    QVERIFY(fixture.controller.handleResize(
        resizeIntent(HybridInput::IntentPhase::Begin)).accepted);
    QVERIFY(fixture.controller.handleResize(
        resizeIntent(HybridInput::IntentPhase::Update, QPointF(20, 10))).accepted);
    QVERIFY(fixture.controller.handleResize(
        resizeIntent(HybridInput::IntentPhase::Update, QPointF(30, 15))).accepted);
    QCOMPARE(fixture.requestedFrames[0], QRect(100, 100, 820, 610));
    QCOMPARE(fixture.requestedFrames[1], QRect(100, 100, 830, 615));

    const auto cancelled = fixture.controller.handleResize(
        resizeIntent(HybridInput::IntentPhase::Cancel, QPointF(30, 15)));
    QVERIFY(cancelled.accepted);
    QCOMPARE(fixture.requestedFrames.constLast(), QRect(100, 100, 800, 600));
    QCOMPARE(fixture.layout.outerFrame, QRect(100, 100, 800, 600));
    QCOMPARE(fixture.changedCount, 3);
}

void HybridContainerPlacementTest::commitsKeyboardResizeWithoutReapplyingSameFrame()
{
    Fixture fixture;
    const auto edges = Qt::LeftEdge | Qt::TopEdge;
    QVERIFY(fixture.controller.handleResize(
        resizeIntent(HybridInput::IntentPhase::Begin, {}, edges)).accepted);
    QVERIFY(fixture.controller.handleResize(
        resizeIntent(HybridInput::IntentPhase::Update, QPointF(-25, -15), edges))
                .accepted);
    QCOMPARE(fixture.layout.outerFrame, QRect(75, 85, 825, 615));
    QVERIFY(fixture.controller.handleResize(
        resizeIntent(HybridInput::IntentPhase::Commit, QPointF(-25, -15), edges))
                .accepted);
    QCOMPARE(fixture.requestedFrames.size(), qsizetype{1});

    const auto staleCommit = fixture.controller.handleResize(
        resizeIntent(HybridInput::IntentPhase::Commit, QPointF(-25, -15), edges));
    QVERIFY(!staleCommit.accepted);
    QVERIFY(staleCommit.message.contains(QStringLiteral("baseline")));
}

void HybridContainerPlacementTest::resizesEdgesAndEnforcesMinimumFrame()
{
    Fixture fixture;
    QString error;
    QVERIFY(fixture.controller.handleOuterResize(
        QStringLiteral("group"),
        resizeEvent(HybridChrome::DragPhase::Begin, {},
                    Qt::LeftEdge | Qt::TopEdge),
        &error));
    QVERIFY(fixture.controller.handleOuterResize(
        QStringLiteral("group"),
        resizeEvent(HybridChrome::DragPhase::Update, QPointF(700, 500),
                    Qt::LeftEdge | Qt::TopEdge),
        &error));
    QCOMPARE(fixture.layout.outerFrame, QRect(660, 540, 240, 160));
    QVERIFY(fixture.controller.handleOuterResize(
        QStringLiteral("group"),
        resizeEvent(HybridChrome::DragPhase::Commit, QPointF(700, 500),
                    Qt::LeftEdge | Qt::TopEdge),
        &error));
    QCOMPARE(fixture.requestedFrames.size(), 1);
}

void HybridContainerPlacementTest::maximizesAndRestoresWholeContainer()
{
    Fixture fixture;
    QString error;
    QVERIFY(fixture.controller.maximize(QStringLiteral("group"), &error));
    QVERIFY(fixture.controller.isMaximized(QStringLiteral("group")));
    QCOMPARE(fixture.layout.outerFrame, QRect(0, 0, 1920, 1040));
    QVERIFY(fixture.controller.restore(QStringLiteral("group"), &error));
    QVERIFY(!fixture.controller.isMaximized(QStringLiteral("group")));
    QCOMPARE(fixture.layout.outerFrame, QRect(100, 100, 800, 600));
}

void HybridContainerPlacementTest::tracksVisibleAndHiddenWorkAreasWhileMaximized()
{
    Fixture fixture;
    QCOMPARE(fixture.controller.refreshMaximizedAreas(), QStringList{});
    QVERIFY(fixture.requestedFrames.isEmpty());

    QString error;
    QVERIFY(fixture.controller.maximize(QStringLiteral("group"), &error));
    QCOMPARE(fixture.layout.outerFrame, QRect(0, 0, 1920, 1040));

    fixture.workArea = QRect(0, 0, 1920, 1080);
    QCOMPARE(fixture.controller.refreshMaximizedAreas(), QStringList{});
    QCOMPARE(fixture.layout.outerFrame, fixture.workArea);
    QCOMPARE(fixture.requestedFrames.size(), qsizetype{2});
    QCOMPARE(fixture.controller.refreshMaximizedAreas(), QStringList{});
    QCOMPARE(fixture.requestedFrames.size(), qsizetype{2});

    fixture.workArea = QRect(0, 0, 1920, 1040);
    QCOMPARE(fixture.controller.refreshMaximizedAreas(), QStringList{});
    QCOMPARE(fixture.layout.outerFrame, fixture.workArea);

    QVERIFY(fixture.controller.restore(QStringLiteral("group"), &error));
    QCOMPARE(fixture.layout.outerFrame, QRect(100, 100, 800, 600));
    fixture.workArea = QRect(0, 0, 1920, 1080);
    QCOMPARE(fixture.controller.refreshMaximizedAreas(), QStringList{});
    QCOMPARE(fixture.layout.outerFrame, QRect(100, 100, 800, 600));
}

void HybridContainerPlacementTest::shadesAndUnshadesAnchoringTheBadgeStripAtTheLeftEdge()
{
    Fixture fixture;
    QString error;
    QVERIFY(fixture.controller.shade(QStringLiteral("group"), &error));
    QVERIFY(fixture.controller.isShaded(QStringLiteral("group")));
    // AGENT-GUARD: shade never reflows the real committed layout (see
    // ADR-0099's follow-up correction) -- no live member window is resized
    // to fake being hidden. The strip is purely this controller's own
    // bookkeeping, tracked independently of fixture.layout.
    QCOMPARE(fixture.layout.outerFrame, QRect(100, 100, 800, 600));
    QCOMPARE(fixture.requestedFrames, QVector<QRect>{});

    const auto strip = fixture.controller.shadedFrame(QStringLiteral("group"));
    QVERIFY(strip.has_value());
    QCOMPARE(strip->topLeft(), QPoint(100, 100));
    // ADR-0139: the strip is the content-sized rolled-up badge anchored at
    // the frame's left edge -- same top-left, badge width, compact height.
    const auto container = sampleTopology().container(QStringLiteral("group"));
    const auto tabCount = container ? container->pages().size() : qsizetype{1};
    QCOMPARE(strip->width(),
             HybridShadeStripGeometry::stripWidth(tabCount, QRect(100, 100, 800, 600)));
    QVERIFY(strip->width() < 800);
    QVERIFY(strip->height() < 600);
    QVERIFY(strip->height() > 28);

    // Idempotent: shading an already-shaded container is a no-op success and
    // never touches the real layout or the strip frame.
    QVERIFY(fixture.controller.shade(QStringLiteral("group"), &error));
    QCOMPARE(fixture.controller.shadedFrame(QStringLiteral("group")), strip);
    QCOMPARE(fixture.layout.outerFrame, QRect(100, 100, 800, 600));

    // Unshade performs exactly one real reflow, back to the exact original
    // size at the strip's current (here, unmoved) position.
    QVERIFY(fixture.controller.unshade(QStringLiteral("group"), &error));
    QVERIFY(!fixture.controller.isShaded(QStringLiteral("group")));
    QCOMPARE(fixture.layout.outerFrame, QRect(100, 100, 800, 600));
    QCOMPARE(fixture.requestedFrames, QVector<QRect>{QRect(100, 100, 800, 600)});

    // Unshading a never-shaded (or already unshaded) container fails cleanly.
    QVERIFY(!fixture.controller.unshade(QStringLiteral("group"), &error));
    QVERIFY(!error.isEmpty());
}

void HybridContainerPlacementTest::rejectsShadeWhileMaximizedAndMaximizeWhileShaded()
{
    Fixture fixture;
    QString error;
    QVERIFY(fixture.controller.maximize(QStringLiteral("group"), &error));
    QVERIFY(!fixture.controller.shade(QStringLiteral("group"), &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!fixture.controller.isShaded(QStringLiteral("group")));
    QVERIFY(fixture.controller.restore(QStringLiteral("group"), &error));

    QVERIFY(fixture.controller.shade(QStringLiteral("group"), &error));
    QVERIFY(!fixture.controller.maximize(QStringLiteral("group"), &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!fixture.controller.isMaximized(QStringLiteral("group")));
}

void HybridContainerPlacementTest::rejectsOuterResizeWhileShadedButStillAllowsMove()
{
    Fixture fixture;
    QString error;
    QVERIFY(fixture.controller.shade(QStringLiteral("group"), &error));
    const auto initialStrip = *fixture.controller.shadedFrame(QStringLiteral("group"));

    const auto rejectedResize = fixture.controller.handleResize(
        resizeIntent(HybridInput::IntentPhase::Begin));
    QVERIFY(!rejectedResize.accepted);
    QVERIFY(rejectedResize.message.contains(QStringLiteral("shaded")));

    // Moving the strip updates only this controller's tracked strip frame:
    // no reflow, no real member-window movement.
    QVERIFY(fixture.controller.handleMove(
        moveIntent(HybridInput::IntentPhase::Begin)).accepted);
    QVERIFY(fixture.controller.handleMove(
        moveIntent(HybridInput::IntentPhase::Commit, QPointF(50, 0))).accepted);
    QVERIFY(fixture.requestedFrames.isEmpty());
    QCOMPARE(*fixture.controller.shadedFrame(QStringLiteral("group")),
             initialStrip.translated(50, 0));
    QCOMPARE(fixture.layout.outerFrame, QRect(100, 100, 800, 600));

    // Unrolling now reflows to the original size at the strip's moved
    // position: dragging the strip really does relocate the restored group.
    QVERIFY(fixture.controller.unshade(QStringLiteral("group"), &error));
    QCOMPARE(fixture.layout.outerFrame, QRect(150, 100, 800, 600));
}

void HybridContainerPlacementTest::cancelledShadedMoveRestoresTheStripsPriorPosition()
{
    Fixture fixture;
    QString error;
    QVERIFY(fixture.controller.shade(QStringLiteral("group"), &error));
    const auto initialStrip = *fixture.controller.shadedFrame(QStringLiteral("group"));

    QVERIFY(fixture.controller.handleMove(
        moveIntent(HybridInput::IntentPhase::Begin)).accepted);
    QVERIFY(fixture.controller.handleMove(
        moveIntent(HybridInput::IntentPhase::Update, QPointF(50, 20))).accepted);
    QCOMPARE(*fixture.controller.shadedFrame(QStringLiteral("group")),
             initialStrip.translated(50, 20));
    QVERIFY(fixture.controller.handleMove(
        moveIntent(HybridInput::IntentPhase::Cancel, QPointF(50, 20))).accepted);
    QCOMPARE(*fixture.controller.shadedFrame(QStringLiteral("group")), initialStrip);
    QVERIFY(fixture.requestedFrames.isEmpty());
}

void HybridContainerPlacementTest::forgettingContainerClearsShadeRestoreFrame()
{
    Fixture fixture;
    QString error;
    QVERIFY(fixture.controller.shade(QStringLiteral("group"), &error));
    fixture.controller.forgetContainer(QStringLiteral("group"));
    QVERIFY(!fixture.controller.isShaded(QStringLiteral("group")));
    QVERIFY(!fixture.controller.shadedFrame(QStringLiteral("group")).has_value());
}

void HybridContainerPlacementTest::reportsReflowFailureWithoutAdvancingAppliedFrame()
{
    Fixture fixture;
    QVERIFY(fixture.controller.handleMove(moveIntent(HybridInput::IntentPhase::Begin)).accepted);
    fixture.failNext = true;
    const auto failed = fixture.controller.handleMove(
        moveIntent(HybridInput::IntentPhase::Update, QPointF(20, 20)));
    QVERIFY(!failed.accepted);
    QCOMPARE(failed.message, QStringLiteral("reflow sentinel"));
    QCOMPARE(fixture.layout.outerFrame, QRect(100, 100, 800, 600));

    const auto retried = fixture.controller.handleMove(
        moveIntent(HybridInput::IntentPhase::Commit, QPointF(20, 20)));
    QVERIFY(retried.accepted);
    QCOMPARE(fixture.layout.outerFrame, QRect(120, 120, 800, 600));
}

void HybridContainerPlacementTest::failedCommitReleasesPlacementBaseline()
{
    Fixture fixture;
    QVERIFY(fixture.controller.handleMove(
        moveIntent(HybridInput::IntentPhase::Begin)).accepted);
    fixture.failNext = true;
    const auto failedMove = fixture.controller.handleMove(
        moveIntent(HybridInput::IntentPhase::Commit, QPointF(20, 20)));
    QVERIFY(!failedMove.accepted);
    QVERIFY(fixture.controller.handleMove(
        moveIntent(HybridInput::IntentPhase::Begin)).accepted);
    QVERIFY(fixture.controller.handleMove(
        moveIntent(HybridInput::IntentPhase::Cancel)).accepted);

    QVERIFY(fixture.controller.handleResize(
        resizeIntent(HybridInput::IntentPhase::Begin)).accepted);
    fixture.failNext = true;
    const auto failedResize = fixture.controller.handleResize(
        resizeIntent(HybridInput::IntentPhase::Commit, QPointF(20, 20)));
    QVERIFY(!failedResize.accepted);
    QVERIFY(fixture.controller.handleResize(
        resizeIntent(HybridInput::IntentPhase::Begin)).accepted);
    QVERIFY(fixture.controller.handleResize(
        resizeIntent(HybridInput::IntentPhase::Cancel)).accepted);
}

void HybridContainerPlacementTest::rejectsUnavailableOrInconsistentResizeState()
{
    Fixture fixture;
    auto invalid = resizeIntent(HybridInput::IntentPhase::Begin, {}, {});
    QVERIFY(!fixture.controller.handleResize(invalid).accepted);
    QVERIFY(!fixture.controller.handleResize(
        resizeIntent(HybridInput::IntentPhase::Update, QPointF(10, 10))).accepted);
    QVERIFY(!fixture.controller.handleResize(
        resizeIntent(HybridInput::IntentPhase::Begin, {}, Qt::RightEdge,
                     QStringLiteral("missing"))).accepted);

    QVERIFY(fixture.controller.handleResize(
        resizeIntent(HybridInput::IntentPhase::Begin)).accepted);
    const auto duplicate = fixture.controller.handleResize(
        resizeIntent(HybridInput::IntentPhase::Begin));
    QVERIFY(!duplicate.accepted);
    QVERIFY(duplicate.message.contains(QStringLiteral("active")));
    const auto changedEdges = fixture.controller.handleResize(
        resizeIntent(HybridInput::IntentPhase::Update, QPointF(5, 5),
                     Qt::LeftEdge | Qt::TopEdge));
    QVERIFY(!changedEdges.accepted);
    QVERIFY(changedEdges.message.contains(QStringLiteral("edges")));
    QVERIFY(fixture.controller.handleResize(
        resizeIntent(HybridInput::IntentPhase::Cancel)).accepted);

    QString error;
    QVERIFY(fixture.controller.maximize(QStringLiteral("group"), &error));
    const auto maximized = fixture.controller.handleResize(
        resizeIntent(HybridInput::IntentPhase::Begin));
    QVERIFY(!maximized.accepted);
    QVERIFY(maximized.message.contains(QStringLiteral("maximized")));
}

void HybridContainerPlacementTest::aspectPinKeepsRatioOnEdgeAndCornerDrags()
{
    // The fixture baseline outer frame is 800x600, so its content-area ratio
    // is (800-2)/(600-30) = 798/570 -- exactly what "Lock current" captures.
    Fixture fixture;
    const double lockedRatio = 798.0 / 570.0;
    QString error;
    QVERIFY(fixture.controller.setAspectRatioPin(QStringLiteral("group"),
                                                  lockedRatio, &error));

    // Corner drag: width leads, height follows the ratio from the dragged
    // width; the top-left corner stays anchored.
    QVERIFY(fixture.controller.handleResize(
        resizeIntent(HybridInput::IntentPhase::Begin)).accepted);
    QVERIFY(fixture.controller.handleResize(
        resizeIntent(HybridInput::IntentPhase::Update, QPointF(160, 0)))
                .accepted);
    QCOMPARE(fixture.layout.outerFrame, QRect(100, 100, 960, 714));
    // Cumulative deltas that only inflate the follower axis change nothing.
    QVERIFY(fixture.controller.handleResize(
        resizeIntent(HybridInput::IntentPhase::Update, QPointF(160, 60)))
                .accepted);
    QCOMPARE(fixture.requestedFrames.size(), qsizetype{1});
    QVERIFY(fixture.controller.handleResize(
        resizeIntent(HybridInput::IntentPhase::Commit, QPointF(160, 60)))
                .accepted);

    // A pure vertical-edge drag makes height lead instead; a fresh fixture
    // keeps the original 800x600 baseline.
    Fixture fresh;
    QVERIFY(fresh.controller.setAspectRatioPin(QStringLiteral("group"),
                                                798.0 / 570.0, &error));
    QVERIFY(fresh.controller.handleResize(
        resizeIntent(HybridInput::IntentPhase::Begin, {}, Qt::BottomEdge))
                .accepted);
    QVERIFY(fresh.controller.handleResize(
        resizeIntent(HybridInput::IntentPhase::Update, QPointF(0, 95),
                     Qt::BottomEdge)).accepted);
    QCOMPARE(fresh.layout.outerFrame, QRect(100, 100, 933, 695));
}

void HybridContainerPlacementTest::aspectPinAnchorsTheFollowerEdgeOnTopLeftDrags()
{
    Fixture fixture;
    QString error;
    QVERIFY(fixture.controller.setAspectRatioPin(QStringLiteral("group"),
                                                  798.0 / 570.0, &error));
    QVERIFY(fixture.controller.handleResize(
        resizeIntent(HybridInput::IntentPhase::Begin, {},
                     Qt::LeftEdge | Qt::TopEdge)).accepted);
    QVERIFY(fixture.controller.handleResize(
        resizeIntent(HybridInput::IntentPhase::Update, QPointF(-30, -10),
                     Qt::LeftEdge | Qt::TopEdge)).accepted);
    // Width leads; the height follower is anchored at the baseline bottom
    // edge because the drag moves the top edge.
    QCOMPARE(fixture.layout.outerFrame, QRect(70, 79, 830, 621));
}

void HybridContainerPlacementTest::aspectPinYieldsToMinimumFrameSizes()
{
    // A very wide pin: shrinking past the minimum width re-derives the height
    // from the clamped width, which then violates the minimum height, so the
    // clamp cascades and the frame rests at the ratio-exact minimum pair.
    Fixture fixture;
    QString error;
    QVERIFY(fixture.controller.setAspectRatioPin(QStringLiteral("group"),
                                                 21.0 / 9.0, &error));
    QVERIFY(fixture.controller.handleResize(
        resizeIntent(HybridInput::IntentPhase::Begin, {}, Qt::RightEdge))
                .accepted);
    QVERIFY(fixture.controller.handleResize(
        resizeIntent(HybridInput::IntentPhase::Update, QPointF(-700, 0),
                     Qt::RightEdge)).accepted);
    QCOMPARE(fixture.layout.outerFrame, QRect(100, 100, 305, 160));

    // A very tall pin with height leading: the width follower grows to keep
    // the ratio instead of dropping below the minimum height.
    Fixture tall;
    QVERIFY(tall.controller.setAspectRatioPin(QStringLiteral("group"), 12.0,
                                              &error));
    QVERIFY(tall.controller.handleResize(
        resizeIntent(HybridInput::IntentPhase::Begin, {}, Qt::BottomEdge))
                .accepted);
    QVERIFY(tall.controller.handleResize(
        resizeIntent(HybridInput::IntentPhase::Update, QPointF(0, -500),
                     Qt::BottomEdge)).accepted);
    QCOMPARE(tall.layout.outerFrame, QRect(100, 100, 1562, 160));
}

void HybridContainerPlacementTest::aspectPinSurvivesMaximizeRestoreButMaximizeIgnoresIt()
{
    Fixture fixture;
    QString error;
    QVERIFY(fixture.controller.setAspectRatioPin(QStringLiteral("group"), 1.0,
                                                 &error));
    QVERIFY(fixture.controller.maximize(QStringLiteral("group"), &error));
    QCOMPARE(fixture.layout.outerFrame, QRect(0, 0, 1920, 1040));
    QVERIFY(fixture.controller.restore(QStringLiteral("group"), &error));
    QCOMPARE(fixture.layout.outerFrame, QRect(100, 100, 800, 600));
    QVERIFY(fixture.controller.aspectRatioPin(QStringLiteral("group")).has_value());
}

void HybridContainerPlacementTest::aspectPinCancelRestoresBaselineAndUnpinRestoresFreeResize()
{
    Fixture fixture;
    QString error;
    QVERIFY(fixture.controller.setAspectRatioPin(QStringLiteral("group"),
                                                  798.0 / 570.0, &error));
    QVERIFY(fixture.controller.handleResize(
        resizeIntent(HybridInput::IntentPhase::Begin)).accepted);
    QVERIFY(fixture.controller.handleResize(
        resizeIntent(HybridInput::IntentPhase::Update, QPointF(160, 0)))
                .accepted);
    QVERIFY(fixture.controller.handleResize(
        resizeIntent(HybridInput::IntentPhase::Cancel, QPointF(160, 0)))
                .accepted);
    QCOMPARE(fixture.layout.outerFrame, QRect(100, 100, 800, 600));

    QVERIFY(fixture.controller.setAspectRatioPin(QStringLiteral("group"),
                                                  std::nullopt, &error));
    QVERIFY(!fixture.controller.aspectRatioPin(QStringLiteral("group"))
                 .has_value());
    QVERIFY(fixture.controller.handleResize(
        resizeIntent(HybridInput::IntentPhase::Begin)).accepted);
    QVERIFY(fixture.controller.handleResize(
        resizeIntent(HybridInput::IntentPhase::Update, QPointF(20, 10)))
                .accepted);
    QCOMPARE(fixture.layout.outerFrame, QRect(100, 100, 820, 610));
}

void HybridContainerPlacementTest::keyboardResizeComposesWithAspectPin()
{
    Fixture fixture;
    QString error;
    QVERIFY(fixture.controller.setAspectRatioPin(QStringLiteral("group"),
                                                  798.0 / 570.0, &error));
    const auto edges = Qt::LeftEdge | Qt::TopEdge;
    QVERIFY(fixture.controller.handleResize(
        resizeIntent(HybridInput::IntentPhase::Begin, {}, edges)).accepted);
    QVERIFY(fixture.controller.handleResize(
        resizeIntent(HybridInput::IntentPhase::Update, QPointF(-25, -15), edges))
                .accepted);
    QCOMPARE(fixture.layout.outerFrame, QRect(75, 82, 825, 618));
}

void HybridContainerPlacementTest::aspectPinValidationContentRatioHelperAndForget()
{
    Fixture fixture;
    QString error;
    QVERIFY(!fixture.controller.setAspectRatioPin(QStringLiteral("missing"),
                                                   1.0, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!fixture.controller.setAspectRatioPin(QStringLiteral("group"), -1.0,
                                                  &error));
    QVERIFY(!fixture.controller.setAspectRatioPin(
        QStringLiteral("group"),
        std::numeric_limits<double>::quiet_NaN(), &error));
    QVERIFY(!fixture.controller.setAspectRatioPin(
        QStringLiteral("group"),
        std::numeric_limits<double>::infinity(), &error));
    QVERIFY(!fixture.controller.aspectRatioPin(QStringLiteral("group"))
                 .has_value());

    QVERIFY(fixture.controller.setAspectRatioPin(QStringLiteral("group"), 2.0,
                                                 &error));
    QCOMPARE(fixture.controller.aspectRatioPin(QStringLiteral("group")),
             std::optional<double>(2.0));
    // Clearing an unlocked container is an idempotent success.
    QVERIFY(fixture.controller.setAspectRatioPin(QStringLiteral("group"),
                                                  std::nullopt, &error));
    QVERIFY(fixture.controller.setAspectRatioPin(QStringLiteral("group"),
                                                  std::nullopt, &error));

    QVERIFY(fixture.controller.setAspectRatioPin(QStringLiteral("group"), 3.0,
                                                 &error));
    fixture.controller.forgetContainer(QStringLiteral("group"));
    QVERIFY(!fixture.controller.aspectRatioPin(QStringLiteral("group"))
                 .has_value());

    QCOMPARE(HybridContainerPlacementController::contentAspectRatioForOuterFrame(
                 QRect(100, 100, 800, 600)),
             798.0 / 570.0);
    QCOMPARE(HybridContainerPlacementController::contentAspectRatioForOuterFrame(
                 QRect(0, 0, 2, 30)),
             0.0);
}

} // namespace QindaQt::Compositor::KWinIntegration

QTEST_GUILESS_MAIN(QindaQt::Compositor::KWinIntegration::HybridContainerPlacementTest)

#include "tst_hybridcontainerplacement.moc"
