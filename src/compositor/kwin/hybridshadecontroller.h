// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>

namespace QindaQt::Compositor::KWinIntegration {

// Atomic platform seam for hiding/showing one member's paint and pointer-
// input eligibility while its container is shaded. Implementations must be
// idempotent and tolerate a dead/unmanaged windowId (the member may have
// closed while shaded). A call that returns false must leave no partial
// state for that window.
class HybridShadeMemberPlatform
{
public:
    virtual ~HybridShadeMemberPlatform() = default;

    // Removes the member entirely from paint (content, decoration, shadow,
    // and the transient windows it owns) and from pointer-input targeting.
    [[nodiscard]] virtual bool hideMember(const QString &windowId,
                                         QString *error = nullptr) = 0;
    [[nodiscard]] virtual bool showMember(const QString &windowId,
                                         QString *error = nullptr) = 0;
    // Same removal as hideMember, but additionally forces the member's own
    // outer scene item to stay paintable (so shared chrome anchored to it
    // remains visible) while its content, decoration, and shadow stay hidden.
    // Called for exactly one member per shaded container: the current chrome
    // anchor. It may also be called for a member already hidden by hideMember
    // (anchor promotion after the previous anchor closed); showAnchorContent
    // must then undo both treatments.
    [[nodiscard]] virtual bool hideAnchorContent(const QString &windowId,
                                                 QString *error = nullptr) = 0;
    [[nodiscard]] virtual bool showAnchorContent(const QString &windowId,
                                                 QString *error = nullptr) = 0;
};

enum class ShadeReassertResult {
    // The window is not a member of an enforced shaded container (never
    // shaded, already restored, or its container is being unrolled).
    NotEnforced,
    Reapplied,
    Failed,
};

// Pure orchestration: decides which member gets plain hide/show and which
// one (the chrome anchor) gets the content-preserving-visibility variant,
// and remembers exactly what it did per container so restore is exact even
// if the caller cannot re-derive the same anchor/member list later (for
// example after a member closed). Retains no KWin/scene reference itself.
//
// AGENT-CONTRACT: KWin clears Window::isHidden() by itself whenever it
// activates a window (Workspace::activateWindow calls setHidden(false)), so
// a client activation request, Alt-Tab, an unminimize, or a reflow can reveal
// a shaded member behind this controller's back. The KWin adapter
// (kwinhybridshade.cpp) must report every such reveal through
// reassertMember(); otherwise a plain member paints again (ghost) and the
// anchor becomes an input-eligible window with invisible content (invisible
// input blocker). See ADR-0099's 2026-09-12 follow-up.
class HybridShadeController final
{
public:
    explicit HybridShadeController(HybridShadeMemberPlatform &platform);

    // Rejects if containerId is already shaded, memberWindowIds is empty, or
    // anchorWindowId is not one of memberWindowIds. On partial platform
    // failure, already-hidden members in this call are shown again before
    // returning false, so a rejected shade never leaves a half-hidden group.
    [[nodiscard]] bool shadeMembers(const QString &containerId,
                                    const QStringList &memberWindowIds,
                                    const QString &anchorWindowId,
                                    QString *error = nullptr);
    // Restores exactly the member set captured by the matching shadeMembers
    // call (minus members that closed while shaded), independent of the
    // container's current topology. Rejects if containerId was not shaded. A
    // partial platform failure still forgets the container (its real KWin
    // state is left however far the restore got; retrying show on
    // already-shown members is safe/idempotent).
    [[nodiscard]] bool unshadeMembers(const QString &containerId,
                                      QString *error = nullptr);
    [[nodiscard]] bool isShaded(const QString &containerId) const noexcept;
    [[nodiscard]] QStringList shadedContainerIds() const;
    // The member kept content-visible while shaded (see hideAnchorContent),
    // or empty if containerId is not shaded or no surviving member could be
    // promoted. Group stacking uses this to know which single member must
    // still participate in live KWin stack validation while its shaded
    // siblings are genuinely hidden and must not be held to that requirement.
    [[nodiscard]] QString anchorWindowId(const QString &containerId) const;
    // The shaded container whose record contains windowId, or empty.
    [[nodiscard]] QString containerOfMember(const QString &windowId) const;
    // Records the member that held keyboard focus when containerId was
    // shaded, so an explicit unroll can hand focus back (KWin moves focus off
    // a window while it is hidden). Ignored unless windowId is a recorded
    // member of that shaded container. Cleared when that member closes;
    // dropped with the record by unshadeMembers/forgetContainer.
    void recordFocusedMember(const QString &containerId, const QString &windowId);
    [[nodiscard]] QString focusedMember(const QString &containerId) const;
    void forgetContainer(const QString &containerId) noexcept;

    // Re-applies the exact treatment (anchor or plain member) this controller
    // chose for windowId after the platform reported it was revealed.
    [[nodiscard]] ShadeReassertResult reassertMember(const QString &windowId,
                                                     QString *error = nullptr);
    // Re-applies every member of one enforced shaded container, attempting
    // all of them even if one fails (for example after a rejected unroll
    // whose reflow already re-activated a member).
    [[nodiscard]] bool reassertContainer(const QString &containerId,
                                         QString *error = nullptr);

    // Marks a shaded container as being unrolled. Its members stay recorded
    // so unshadeMembers restores them exactly, but reassertMember no longer
    // re-hides them: the unroll's own reflow legitimately activates one member
    // before unshadeMembers runs. cancelRelease re-enables enforcement when
    // the unroll is rejected; unshadeMembers/forgetContainer clear it.
    void beginRelease(const QString &containerId);
    void cancelRelease(const QString &containerId) noexcept;
    [[nodiscard]] bool isReleasing(const QString &containerId) const noexcept;

    // A shaded member closed. Drops it from its container's record so restore
    // never targets it. If it was the anchor, promotes the last surviving
    // member that the platform accepts (hideAnchorContent, trying earlier
    // members on failure) so the shared strip keeps a paintable anchor. When
    // the last member closes the record is dropped. Returns false only when
    // an anchor was lost and no survivor could be promoted; the caller must
    // then unroll the container rather than leave hidden, unreachable windows.
    [[nodiscard]] bool memberClosed(const QString &windowId,
                                    QString *error = nullptr);

private:
    struct ShadedMembers final
    {
        QStringList windowIds;
        QString anchorWindowId;
        QString focusedWindowId;
    };

    [[nodiscard]] bool applyHide(const QString &windowId,
                                 const QString &anchorWindowId,
                                 QString *error);

    HybridShadeMemberPlatform &m_platform;
    QHash<QString, ShadedMembers> m_shaded;
    QSet<QString> m_releasing;
};

inline bool HybridShadeController::isShaded(const QString &containerId) const noexcept
{
    return m_shaded.contains(containerId);
}

inline bool HybridShadeController::isReleasing(const QString &containerId) const noexcept
{
    return m_releasing.contains(containerId);
}

inline QString HybridShadeController::anchorWindowId(const QString &containerId) const
{
    const auto found = m_shaded.constFind(containerId);
    return found == m_shaded.cend() ? QString{} : found->anchorWindowId;
}

} // namespace QindaQt::Compositor::KWinIntegration
