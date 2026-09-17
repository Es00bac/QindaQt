// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridcontainerplacement.h"

#include "hybridshadestripgeometry.h"
#include "qindaqt/hybrid_chrome/chromeshadedbadge.h"

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

class HybridContainerShadeTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void shadesAndUnshadesAnchoringTheBadgeStripAtTheLeftEdge();
    void shadeStripFollowsItsBadgeLabel();
    void rejectsShadeWhileMaximizedAndMaximizeWhileShaded();
    void rejectsOuterResizeWhileShadedButStillAllowsMove();
    void cancelledShadedMoveRestoresTheStripsPriorPosition();
    void forgettingContainerClearsShadeRestoreFrame();
};

void HybridContainerShadeTest::shadesAndUnshadesAnchoringTheBadgeStripAtTheLeftEdge()
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
    // ADR-0189: shade() reserves the label minimum because it has no page
    // titles and no font; the session widens the strip to the measured label
    // through resizeShadeStrip() as soon as it synchronizes chrome.
    QCOMPARE(strip->width(),
             HybridShadeStripGeometry::stripWidth(
                 tabCount, QRect(100, 100, 800, 600),
                 HybridChrome::ChromeShadedBadge::LabelMinimumWidth));
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

// ADR-0189: the strip must follow the badge label, because a page title
// changes while a container stays rolled up. Without this the strip keeps the
// width it was shaded at and a longer title is elided into it.
void HybridContainerShadeTest::shadeStripFollowsItsBadgeLabel()
{
    Fixture fixture;
    QString error;
    QVERIFY(fixture.controller.shade(QStringLiteral("group"), &error));
    const auto initial = fixture.controller.shadedFrame(QStringLiteral("group"));
    QVERIFY(initial.has_value());

    // AGENT-GUARD: this suite is QTEST_GUILESS_MAIN on purpose — the
    // placement controller owns no GUI state — so the width is passed in
    // explicitly rather than measured. Measuring a font here needs a
    // QGuiApplication and aborts the run. Measurement belongs to the session
    // (KWinHybridSession) and to the badge's own suite.
    constexpr qreal wide = HybridChrome::ChromeShadedBadge::LabelMaximumWidth;
    static_assert(wide > HybridChrome::ChromeShadedBadge::LabelMinimumWidth);
    QVERIFY(fixture.controller.resizeShadeStrip(QStringLiteral("group"), wide));
    const auto widened = fixture.controller.shadedFrame(QStringLiteral("group"));
    QVERIFY(widened.has_value());
    QVERIFY2(widened->width() > initial->width(),
             qPrintable(QStringLiteral("strip did not widen: %1 -> %2")
                            .arg(initial->width())
                            .arg(widened->width())));
    // The top-left is untouched: the badge stays anchored where it was.
    QCOMPARE(widened->topLeft(), initial->topLeft());
    QCOMPARE(widened->height(), initial->height());
    // Never wider than the frame it was shaded from.
    QVERIFY(widened->width() <= 800);

    // A shorter title shrinks it back, and re-applying the same width is a
    // no-op so chrome is not republished for nothing.
    QVERIFY(fixture.controller.resizeShadeStrip(
        QStringLiteral("group"), HybridChrome::ChromeShadedBadge::LabelMinimumWidth));
    QCOMPARE(fixture.controller.shadedFrame(QStringLiteral("group")), initial);
    QVERIFY(!fixture.controller.resizeShadeStrip(
        QStringLiteral("group"), HybridChrome::ChromeShadedBadge::LabelMinimumWidth));

    // An unshaded container has no strip to resize.
    QVERIFY(fixture.controller.unshade(QStringLiteral("group"), &error));
    QVERIFY(!fixture.controller.resizeShadeStrip(QStringLiteral("group"), wide));
}

void HybridContainerShadeTest::rejectsShadeWhileMaximizedAndMaximizeWhileShaded()
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

void HybridContainerShadeTest::rejectsOuterResizeWhileShadedButStillAllowsMove()
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

void HybridContainerShadeTest::cancelledShadedMoveRestoresTheStripsPriorPosition()
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

void HybridContainerShadeTest::forgettingContainerClearsShadeRestoreFrame()
{
    Fixture fixture;
    QString error;
    QVERIFY(fixture.controller.shade(QStringLiteral("group"), &error));
    fixture.controller.forgetContainer(QStringLiteral("group"));
    QVERIFY(!fixture.controller.isShaded(QStringLiteral("group")));
    QVERIFY(!fixture.controller.shadedFrame(QStringLiteral("group")).has_value());
}

} // namespace QindaQt::Compositor::KWinIntegration

QTEST_GUILESS_MAIN(QindaQt::Compositor::KWinIntegration::HybridContainerShadeTest)

#include "tst_hybridcontainershade.moc"
