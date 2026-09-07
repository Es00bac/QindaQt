// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "hybridinteractionruntime.h"
#include "kwinhybridscene.h"

#include "qindaqt/hybrid/windowtopology.h"
#include "qindaqt/hybrid_chrome/chrometypes.h"

#include <QHash>
#include <QRect>
#include <QSize>
#include <QString>
#include <QStringList>

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
    // Re-resolves KWin's current maximize area for every maximized container.
    // Successful refreshes preserve the independent restore frame; failures
    // remain maximized so a later work-area transition can retry them.
    [[nodiscard]] QStringList refreshMaximizedAreas();
    [[nodiscard]] bool isMaximized(const QString &containerId) const noexcept;

    // Rolls the container up to a compact, still-visible, still-movable strip
    // at the frame's current position and width. Unlike maximize/restore,
    // shading never reflows or resizes any member window: the real committed
    // layout is left completely untouched (members keep their exact frame),
    // and this controller instead tracks an independent "strip frame" used
    // only for the shared chrome plan and for dragging the strip around.
    // Content/decoration/shadow/input hiding for members is a KWin-adapter
    // responsibility (see HybridShadeMemberController) driven by isShaded();
    // this controller owns no KWin object and no visibility state.
    [[nodiscard]] bool shade(const QString &containerId, QString *error = nullptr);
    [[nodiscard]] bool unshade(const QString &containerId, QString *error = nullptr);
    [[nodiscard]] bool isShaded(const QString &containerId) const noexcept;
    // The strip's current logical frame while shaded (moves under drag);
    // nullopt when the container is not shaded.
    [[nodiscard]] std::optional<QRect> shadedFrame(const QString &containerId) const;
    // Diagnostics-only enumeration (see KWinHybridSession::diagnostics());
    // production code should use isShaded()/shadedFrame() per container.
    [[nodiscard]] QStringList shadedContainerIds() const;
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
    [[nodiscard]] DirectInteractionResult handleShadedMove(
        const QString &containerId,
        const HybridInput::InteractionIntent &intent);
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
    QHash<QString, QRect> m_shadeStripFrames;
    // Original (pre-shade) size only; position is not tracked here because
    // the strip's own current position (which may have moved under drag) is
    // exactly the position the container reflows back to on unshade.
    QHash<QString, QSize> m_shadeRestoreSizes;
};

inline bool HybridContainerPlacementController::isMaximized(
    const QString &containerId) const noexcept
{
    return m_maximizeRestoreFrames.contains(containerId);
}

inline bool HybridContainerPlacementController::isShaded(
    const QString &containerId) const noexcept
{
    return m_shadeStripFrames.contains(containerId);
}

} // namespace QindaQt::Compositor::KWinIntegration
