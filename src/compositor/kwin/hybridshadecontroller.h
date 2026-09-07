// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QString>
#include <QStringList>

namespace QindaQt::Compositor::KWinIntegration {

// Atomic platform seam for hiding/showing one member's paint and pointer-
// input eligibility while its container is shaded. Implementations must be
// idempotent and tolerate a dead/unmanaged windowId (the member may have
// closed while shaded).
class HybridShadeMemberPlatform
{
public:
    virtual ~HybridShadeMemberPlatform() = default;

    // Removes the member entirely from paint and pointer-input targeting.
    [[nodiscard]] virtual bool hideMember(const QString &windowId,
                                         QString *error = nullptr) = 0;
    [[nodiscard]] virtual bool showMember(const QString &windowId,
                                         QString *error = nullptr) = 0;
    // Same input-eligibility removal as hideMember, but additionally forces
    // the member's own outer scene item to stay paintable (so shared chrome
    // anchored to it remains visible) while explicitly hiding its own
    // content, decoration, and shadow. Called for exactly one member per
    // shaded container: the current chrome anchor.
    [[nodiscard]] virtual bool hideAnchorContent(const QString &windowId,
                                                 QString *error = nullptr) = 0;
    [[nodiscard]] virtual bool showAnchorContent(const QString &windowId,
                                                 QString *error = nullptr) = 0;
};

// Pure orchestration: decides which member gets plain hide/show and which
// one (the chrome anchor) gets the content-preserving-visibility variant,
// and remembers exactly what it did per container so restore is exact even
// if the caller cannot re-derive the same anchor/member list later (for
// example after a member closed). Retains no KWin/scene reference itself.
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
    // call, independent of the container's current topology. Rejects if
    // containerId was not shaded. A partial platform failure still forgets
    // the container (its real KWin state is left however far the restore
    // got; retrying show on already-shown members is safe/idempotent).
    [[nodiscard]] bool unshadeMembers(const QString &containerId,
                                      QString *error = nullptr);
    [[nodiscard]] bool isShaded(const QString &containerId) const noexcept;
    [[nodiscard]] QStringList shadedContainerIds() const;
    // The member kept content-visible while shaded (see hideAnchorContent),
    // or empty if containerId is not shaded. Group stacking uses this to
    // know which single member must still participate in live KWin stack
    // validation while its shaded siblings are genuinely hidden and must
    // not be held to that same requirement.
    [[nodiscard]] QString anchorWindowId(const QString &containerId) const;
    void forgetContainer(const QString &containerId) noexcept;

private:
    struct ShadedMembers final
    {
        QStringList windowIds;
        QString anchorWindowId;
    };

    HybridShadeMemberPlatform &m_platform;
    QHash<QString, ShadedMembers> m_shaded;
};

inline bool HybridShadeController::isShaded(const QString &containerId) const noexcept
{
    return m_shaded.contains(containerId);
}

inline QString HybridShadeController::anchorWindowId(const QString &containerId) const
{
    const auto found = m_shaded.constFind(containerId);
    return found == m_shaded.cend() ? QString{} : found->anchorWindowId;
}

} // namespace QindaQt::Compositor::KWinIntegration
