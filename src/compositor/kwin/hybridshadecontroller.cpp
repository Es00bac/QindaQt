// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridshadecontroller.h"

#include <utility>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

bool fail(QString *error, QString message)
{
    if (error) {
        *error = std::move(message);
    }
    return false;
}

} // namespace

HybridShadeController::HybridShadeController(HybridShadeMemberPlatform &platform)
    : m_platform(platform)
{
}

bool HybridShadeController::applyHide(const QString &windowId,
                                      const QString &anchorWindowId,
                                      QString *error)
{
    return windowId == anchorWindowId
        ? m_platform.hideAnchorContent(windowId, error)
        : m_platform.hideMember(windowId, error);
}

bool HybridShadeController::shadeMembers(
    const QString &containerId,
    const QStringList &memberWindowIds,
    const QString &anchorWindowId,
    QString *error)
{
    if (isShaded(containerId)) {
        return fail(error, QStringLiteral("container is already shaded"));
    }
    if (memberWindowIds.isEmpty()) {
        return fail(error, QStringLiteral("shaded container has no members"));
    }
    if (!memberWindowIds.contains(anchorWindowId)) {
        return fail(error, QStringLiteral("shade anchor is not a member of this container"));
    }

    const QStringList members = memberWindowIds;
    QStringList hidden;
    for (const auto &windowId : members) {
        if (!applyHide(windowId, anchorWindowId, error)) {
            // AGENT-GUARD: A rejected shade must never leave some members
            // hidden and others visible. Reverse exactly what this call
            // applied, in reverse order, before reporting failure.
            for (auto it = hidden.crbegin(); it != hidden.crend(); ++it) {
                QString restoreError;
                if (*it == anchorWindowId) {
                    (void)m_platform.showAnchorContent(*it, &restoreError);
                } else {
                    (void)m_platform.showMember(*it, &restoreError);
                }
            }
            return false;
        }
        hidden.append(windowId);
    }
    m_shaded.insert(containerId, ShadedMembers{members, anchorWindowId, {}});
    m_releasing.remove(containerId);
    return true;
}

bool HybridShadeController::unshadeMembers(const QString &containerId, QString *error)
{
    const auto found = m_shaded.constFind(containerId);
    if (found == m_shaded.cend()) {
        return fail(error, QStringLiteral("container is not shaded"));
    }
    const auto members = *found;
    m_shaded.erase(found);
    m_releasing.remove(containerId);

    bool allOk = true;
    QString firstError;
    for (const auto &windowId : members.windowIds) {
        QString memberError;
        const bool ok = windowId == members.anchorWindowId
            ? m_platform.showAnchorContent(windowId, &memberError)
            : m_platform.showMember(windowId, &memberError);
        if (!ok && allOk) {
            allOk = false;
            firstError = memberError;
        }
    }
    if (!allOk) {
        return fail(error,
                    QStringLiteral("one or more members could not be restored: %1")
                        .arg(firstError));
    }
    return true;
}

QStringList HybridShadeController::shadedContainerIds() const
{
    return m_shaded.keys();
}

QString HybridShadeController::containerOfMember(const QString &windowId) const
{
    for (auto it = m_shaded.cbegin(); it != m_shaded.cend(); ++it) {
        if (it->windowIds.contains(windowId)) {
            return it.key();
        }
    }
    return {};
}

void HybridShadeController::recordFocusedMember(const QString &containerId,
                                                const QString &windowId)
{
    const auto found = m_shaded.find(containerId);
    if (found != m_shaded.end() && found->windowIds.contains(windowId)) {
        found->focusedWindowId = windowId;
    }
}

QString HybridShadeController::focusedMember(const QString &containerId) const
{
    const auto found = m_shaded.constFind(containerId);
    return found == m_shaded.cend() ? QString{} : found->focusedWindowId;
}

void HybridShadeController::forgetContainer(const QString &containerId) noexcept
{
    m_shaded.remove(containerId);
    m_releasing.remove(containerId);
}

ShadeReassertResult HybridShadeController::reassertMember(const QString &windowId,
                                                          QString *error)
{
    const auto containerId = containerOfMember(windowId);
    if (containerId.isEmpty() || isReleasing(containerId)) {
        return ShadeReassertResult::NotEnforced;
    }
    const auto anchorId = anchorWindowId(containerId);
    return applyHide(windowId, anchorId, error) ? ShadeReassertResult::Reapplied
                                                : ShadeReassertResult::Failed;
}

bool HybridShadeController::reassertContainer(const QString &containerId, QString *error)
{
    const auto found = m_shaded.constFind(containerId);
    if (found == m_shaded.cend() || isReleasing(containerId)) {
        return fail(error, QStringLiteral("container is not enforced as shaded"));
    }
    // Copy before calling out: a platform call must not be able to
    // invalidate the record this loop walks.
    const auto members = *found;
    bool allOk = true;
    for (const auto &windowId : members.windowIds) {
        QString memberError;
        if (!applyHide(windowId, members.anchorWindowId, &memberError) && allOk) {
            allOk = false;
            fail(error, memberError);
        }
    }
    return allOk;
}

void HybridShadeController::beginRelease(const QString &containerId)
{
    if (isShaded(containerId)) {
        m_releasing.insert(containerId);
    }
}

void HybridShadeController::cancelRelease(const QString &containerId) noexcept
{
    m_releasing.remove(containerId);
}

bool HybridShadeController::memberClosed(const QString &windowId, QString *error)
{
    const auto containerId = containerOfMember(windowId);
    if (containerId.isEmpty()) {
        return true;
    }
    auto found = m_shaded.find(containerId);
    found->windowIds.removeAll(windowId);
    if (found->focusedWindowId == windowId) {
        found->focusedWindowId.clear();
    }
    if (found->windowIds.isEmpty()) {
        m_shaded.erase(found);
        m_releasing.remove(containerId);
        return true;
    }
    if (found->anchorWindowId != windowId) {
        return true;
    }
    found->anchorWindowId.clear();
    const auto survivors = found->windowIds;

    // AGENT-GUARD: group stacking anchors the shared strip to exactly
    // anchorWindowId(). Publishing a dead anchor drops the container's chrome
    // and leaves the surviving members hidden with nothing on screen to
    // unroll, so promote a survivor before the lifecycle resynchronizes.
    QString lastError;
    for (auto it = survivors.crbegin(); it != survivors.crend(); ++it) {
        QString promoteError;
        if (m_platform.hideAnchorContent(*it, &promoteError)) {
            const auto current = m_shaded.find(containerId);
            if (current != m_shaded.end()) {
                current->anchorWindowId = *it;
            }
            return true;
        }
        lastError = promoteError;
    }
    return fail(error,
                QStringLiteral("no surviving member could anchor the shaded strip: %1")
                    .arg(lastError));
}

} // namespace QindaQt::Compositor::KWinIntegration
