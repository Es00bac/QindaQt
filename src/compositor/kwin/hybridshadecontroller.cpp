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

    QStringList hidden;
    for (const auto &windowId : memberWindowIds) {
        const bool ok = windowId == anchorWindowId
            ? m_platform.hideAnchorContent(windowId, error)
            : m_platform.hideMember(windowId, error);
        if (!ok) {
            // AGENT-GUARD: A rejected shade must never leave some members
            // hidden and others visible. Reverse exactly what this call
            // applied, in reverse order, before reporting failure.
            for (auto it = hidden.crbegin(); it != hidden.crend(); ++it) {
                QString restoreError;
                if (*it == anchorWindowId) {
                    m_platform.showAnchorContent(*it, &restoreError);
                } else {
                    m_platform.showMember(*it, &restoreError);
                }
            }
            return false;
        }
        hidden.append(windowId);
    }
    m_shaded.insert(containerId, ShadedMembers{memberWindowIds, anchorWindowId});
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

void HybridShadeController::forgetContainer(const QString &containerId) noexcept
{
    m_shaded.remove(containerId);
}

} // namespace QindaQt::Compositor::KWinIntegration
