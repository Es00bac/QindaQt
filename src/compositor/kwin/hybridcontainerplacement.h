// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "hybridinteractionruntime.h"
#include "kwinhybridscene.h"

#include "qindaqt/hybrid/windowtopology.h"
#include "qindaqt/hybrid_chrome/chrometypes.h"

#include <QHash>
#include <QRect>
#include <QString>

#include <functional>
#include <optional>

namespace QindaQt::Compositor::KWinIntegration {

using HybridTopologyLookup = std::function<const Hybrid::WindowTopology &()>;
using HybridCommittedLayoutLookup =
    std::function<std::optional<CommittedContainerLayout>(const QString &containerId)>;
using HybridContainerReflow =
    std::function<Hybrid::SceneStepResult(const Core::WindowContainer &container,
                                          const QRect &outerFrame)>;
using HybridWorkAreaLookup = std::function<QRect(const QString &containerId)>;
using HybridPlacementChangedSink = std::function<void()>;

// Owns transient group-frame gesture state and maximize restore frames. KWin
// discovery and scene mutation remain injected boundaries, keeping pointer
// policy independently testable and the plugin lifetime graph explicit.
class HybridContainerPlacementController final
{
public:
    HybridContainerPlacementController(
        HybridTopologyLookup topology,
        HybridCommittedLayoutLookup layout,
        HybridContainerReflow reflow,
        HybridWorkAreaLookup workArea,
        HybridPlacementChangedSink changed = {});

    [[nodiscard]] DirectInteractionResult handleMove(
        const HybridInput::InteractionIntent &intent);
    [[nodiscard]] DirectInteractionResult handleResize(
        const HybridInput::InteractionIntent &intent);
    [[nodiscard]] DividerGeometryResult dividerRatio(
        const HybridInput::InteractionIntent &intent) const;
    [[nodiscard]] bool handleOuterResize(
        const QString &containerId,
        const HybridChrome::ChromeDragEvent &event,
        QString *error = nullptr);

    [[nodiscard]] bool maximize(const QString &containerId, QString *error = nullptr);
    [[nodiscard]] bool restore(const QString &containerId, QString *error = nullptr);
    [[nodiscard]] bool isMaximized(const QString &containerId) const noexcept;
    void forgetContainer(const QString &containerId) noexcept;
    void cancelAll() noexcept;

private:
    struct FrameDrag final
    {
        QRect baseline;
        QRect applied;
        Qt::Edges edges;
    };

    [[nodiscard]] const Core::WindowContainer *container(
        const QString &containerId) const;
    [[nodiscard]] bool reflow(const QString &containerId,
                              const QRect &frame,
                              QString *error = nullptr);
    [[nodiscard]] bool beginDrag(QHash<QString, FrameDrag> &drags,
                                 const QString &containerId,
                                 Qt::Edges edges,
                                 QString *error);
    [[nodiscard]] static QRect resizedFrame(const FrameDrag &drag,
                                            const QPointF &delta);
    static void assignError(QString *error, QString message);

    HybridTopologyLookup m_topology;
    HybridCommittedLayoutLookup m_layout;
    HybridContainerReflow m_reflow;
    HybridWorkAreaLookup m_workArea;
    HybridPlacementChangedSink m_changed;
    QHash<QString, FrameDrag> m_moveDrags;
    QHash<QString, FrameDrag> m_resizeDrags;
    QHash<QString, QRect> m_maximizeRestoreFrames;
};

inline bool HybridContainerPlacementController::isMaximized(
    const QString &containerId) const noexcept
{
    return m_maximizeRestoreFrames.contains(containerId);
}

} // namespace QindaQt::Compositor::KWinIntegration
