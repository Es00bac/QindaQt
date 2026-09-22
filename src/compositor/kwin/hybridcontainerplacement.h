// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "hybridinteractionruntime.h"
#include "kwinhybridscene.h"

#include "qindaqt/hybrid/windowtopology.h"
#include "qindaqt/hybrid_chrome/chrometypes.h"

#include <QHash>
#include <QRect>
#include <QSet>
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

    // Aspect-ratio lock (ADR-0162). The pin holds the wanted content-area
    // ratio (outer frame minus shared chrome) and constrains every subsequent
    // outer pointer/keyboard resize. Process-local like the maximize restore
    // frame; cleared by forgetContainer(). nullopt clears the lock; a pinned
    // ratio must be finite and positive, otherwise the store is unchanged and
    // *error is set. Maximize deliberately ignores the lock; unmaximize
    // restores the saved (locked-shape) frame unchanged.
    [[nodiscard]] bool setAspectRatioPin(const QString &containerId,
                                         std::optional<double> contentRatio,
                                         QString *error = nullptr);
    [[nodiscard]] std::optional<double> aspectRatioPin(
        const QString &containerId) const noexcept;
    // The content-area ratio an outer frame currently has, using the same
    // chrome offsets as the lock itself; 0.0 when the frame cannot express
    // one. The session's "Lock current" menu action reads this.
    [[nodiscard]] static double contentAspectRatioForOuterFrame(const QRect &outerFrame) noexcept;
    // Re-resolves KWin's current maximize area for every maximized container.
    // Successful refreshes preserve the independent restore frame; failures
    // remain maximized so a later work-area transition can retry them.
    [[nodiscard]] QStringList refreshMaximizedAreas();
    // Brings back any container whose frame lies entirely outside its work
    // area, which is what removing an output leaves behind.
    //
    // AGENT-GUARD: refreshMaximizedAreas() only re-fits containers that are
    // maximized, so an ordinary container stranded by a display going away was
    // never relocated -- and the grouped geometry reconciler then reasserts its
    // stale plan, so moving a member at the KWin level is undone. The user is
    // left with a container whose title bar is off-screen and therefore cannot
    // be dragged back, and every visibility snapshot is rejected forever
    // because a managed window lies outside its own output. Call this from the
    // same work-area transition, before the grouped reconciler runs.
    //
    // Moves rather than resizes: the container keeps its size unless it no
    // longer fits, so a user's layout survives the display change.
    [[nodiscard]] QStringList refreshStrandedContainers();
    // True while a gesture whose Begin this controller refused is still
    // sending phases. Exposed so the behaviour is testable; callers have no
    // reason to consult it.
    [[nodiscard]] bool gestureRefused(const QString &containerId) const noexcept;
    [[nodiscard]] bool isMaximized(const QString &containerId) const noexcept;

    // Rolls the container up to a compact, still-visible, still-movable badge
    // at the frame's current position with a metric/tab-derived width. Unlike maximize/restore,
    // shading never reflows or resizes any member window: the real committed
    // layout is left completely untouched (members keep their exact frame),
    // and this controller instead tracks an independent "strip frame" used
    // only for the shared chrome plan and for dragging the badge around.
    // Content/decoration/shadow/input hiding for members is a KWin-adapter
    // responsibility (see HybridShadeMemberController) driven by isShaded();
    // this controller owns no KWin object and no visibility state.
    [[nodiscard]] bool shade(const QString &containerId, QString *error = nullptr);
    [[nodiscard]] bool unshade(const QString &containerId, QString *error = nullptr);
    [[nodiscard]] bool isShaded(const QString &containerId) const noexcept;
    // The strip's current logical frame while shaded (moves under drag);
    // nullopt when the container is not shaded.
    [[nodiscard]] std::optional<QRect> shadedFrame(const QString &containerId) const;
    // Re-sizes a shaded container's strip for a badge label of labelWidth
    // logical pixels, keeping its current top-left. Returns true when the
    // width actually changed, so the caller can skip republishing chrome.
    //
    // AGENT-CONTRACT: shade() sizes the strip for the label minimum because it
    // cannot see page titles; the session calls this from its chrome
    // synchronization, where the resolved label is already in hand, so a strip
    // grows and shrinks as its foremost page title changes while rolled up
    // (ADR-0189). The strip never exceeds the frame it was shaded from.
    [[nodiscard]] bool resizeShadeStrip(const QString &containerId, qreal labelWidth);
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
                                            const QPointF &delta,
                                            const std::optional<double> &pinnedContentRatio);
    static void assignError(QString *error, QString message);

    HybridTopologyLookup m_topology;
    HybridCommittedLayoutLookup m_layout;
    HybridContainerReflow m_reflow;
    HybridWorkAreaLookup m_workArea;
    HybridPlacementChangedSink m_changed;
    QHash<QString, FrameDrag> m_moveDrags;
    QHash<QString, FrameDrag> m_resizeDrags;
    QHash<QString, QRect> m_maximizeRestoreFrames;
    // AGENT-GUARD: containers whose Begin was refused (maximized, shaded).
    // The interaction controller keeps sending Update for the rest of the
    // gesture, and re-reporting "has no active baseline" once per pointer
    // motion event drowned the channel a real interaction failure would use --
    // 128 lines from a handful of refused drags in one session. Entries are
    // added only at a refused Begin and removed at the next Begin, Commit or
    // Cancel, so a genuinely missing baseline is still reported.
    QSet<QString> m_refusedGestures;
    QHash<QString, double> m_aspectPins;
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
