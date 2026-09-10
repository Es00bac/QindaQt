// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "hybridgroupstackingpolicy.h"

#include "qindaqt/hybrid/windowtopology.h"

#include <QMap>
#include <QPointF>
#include <QString>

namespace QindaQt::Compositor::KWinIntegration {

class KWinChromeManager;
class ManagedWindowRegistry;

// Whether raiseContainer also grants native activation to the group's
// activation representative. A shaded container's representative is the
// content-preserving chrome anchor -- genuinely Window::isHidden() by
// design, not a real focusable client -- so activating it is never correct:
// live-verified, doing so unhid and natively detached the container's other,
// plain-hidden member as a side effect. RaiseOnly still performs every dead/
// layer/contiguous-block validation and the real KWin z-order raise; it only
// skips the activateWindow() call and the activation-specific postcondition
// that assumes a real, activatable representative.
enum class RaiseActivation {
    ActivateRepresentative,
    RaiseOnly,
};

// Applies the pure active-page block plan through KWin's public stacking API,
// then reparents each scene chrome item to that block's topmost member.
// Everything runs synchronously on KWin's compositor thread.
class KWinHybridGroupStacking final
{
public:
    KWinHybridGroupStacking(ManagedWindowRegistry &registry,
                            KWinChromeManager &chrome);

    // shadedAnchors names, for each currently-shaded container, the one real
    // member kept content-visible (HybridShadeController::anchorWindowId).
    // That member is still required to resolve to a live, anchorable KWin
    // window; its shaded siblings are genuinely Window::isHidden() by design
    // and are exempted from the same-layer/contiguous-stack-block checks
    // that exist to protect visible, input-eligible members. Omitting a
    // shaded container from this map (or passing none) is only correct for
    // members hidden for unrelated reasons, where a missing/incompatible
    // member remains a real synchronization failure.
    [[nodiscard]] bool synchronize(const Hybrid::WindowTopology &topology,
                                   const QMap<QString, QString> &shadedAnchors = {},
                                   QString *error = nullptr);
    // activation controls whether the group's representative is granted
    // native focus (see RaiseActivation). Every other check -- live/dead
    // members, shared layer, contiguous stack block, transient ordering,
    // and the real z-order raise itself -- runs identically regardless of
    // activation.
    [[nodiscard]] bool raiseContainer(
        const QString &containerId,
        RaiseActivation activation = RaiseActivation::ActivateRepresentative,
        QString *error = nullptr);
    // Answers against KWin's live stack rather than chrome-plan geometry.
    // excludedWindowIds is used only while dragging those exact source
    // windows: a tab drag excludes its whole page, since every page member
    // floats above the destination with the raised source container.
    [[nodiscard]] bool chromeExposedAt(
        const QString &containerId,
        const QPointF &position,
        const QSet<QString> &excludedWindowIds = {}) const;
    [[nodiscard]] qsizetype publishedGroupCount() const noexcept;
    // The current chrome anchor (topmost member in the live stack) for a
    // published container, or empty if the container is not published.
    // Shade uses this to know exactly which member's WindowItem to force
    // visible while hiding every member's content/input.
    [[nodiscard]] QString anchorMemberId(const QString &containerId) const;
    void clear() noexcept;

private:
    ManagedWindowRegistry &m_registry;
    KWinChromeManager &m_chrome;
    QMap<QString, QStringList> m_membersBottomToTop;
    QMap<QString, QStringList> m_transientsBottomToTop;
    QMap<QString, QString> m_activationRepresentatives;
};

} // namespace QindaQt::Compositor::KWinIntegration

inline qsizetype
QindaQt::Compositor::KWinIntegration::KWinHybridGroupStacking::publishedGroupCount()
    const noexcept
{
    return m_membersBottomToTop.size();
}
