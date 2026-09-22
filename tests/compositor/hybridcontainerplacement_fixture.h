// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Shared fixture for the HybridContainerPlacementController test files.
//
// AGENT-NOTE: extracted when tst_hybridcontainerplacement.cpp reached the
// 600-line shape limit. AGENTS.md asks for tests split by behaviour rather
// than merged, and for setup not to be duplicated across the split, so the
// fixture lives here and each behaviour gets its own translation unit.

#include "hybridcontainerplacement.h"

#include <QPointF>
#include <QRect>
#include <QSize>
#include <QString>
#include <QVector>
#include <QtTest>

#include <optional>
#include <utility>

namespace QindaQt::Compositor::KWinIntegration {
namespace PlacementFixtures {


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


} // namespace PlacementFixtures
} // namespace QindaQt::Compositor::KWinIntegration
