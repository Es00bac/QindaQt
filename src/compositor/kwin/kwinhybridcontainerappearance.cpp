// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinhybridsession.h"

#include "hybridcontainerplacement.h"

namespace QindaQt::Compositor::KWinIntegration {

bool KWinHybridSession::isContainerShaded(const QString &containerId) const noexcept
{
    return m_placement && m_placement->isShaded(containerId);
}

Compositor::ContainerAppearance KWinHybridSession::containerAppearance(
    const QString &containerId) const
{
    return m_appearance.appearance(containerId);
}

bool KWinHybridSession::renameContainer(
    const QString &containerId, const QString &name, QString *error)
{
    if (!ready() || !m_runtime->topology().container(containerId)) {
        if (error) {
            *error = QStringLiteral("the selected window group is stale");
        }
        return false;
    }
    if (!m_appearance.setName(containerId, name, error)) {
        return false;
    }
    synchronizeChrome();
    // AGENT-CONTRACT: The task-facts publisher also reads this override.
    // Chrome repaint alone need not change any watched native window state.
    Q_EMIT shellVisibilityStateChanged();
    return true;
}

bool KWinHybridSession::setContainerColor(
    const QString &containerId, const QString &colorHex, QString *error)
{
    if (!ready() || !m_runtime->topology().container(containerId)) {
        if (error) {
            *error = QStringLiteral("the selected window group is stale");
        }
        return false;
    }
    if (!m_appearance.setColor(containerId, colorHex, error)) {
        return false;
    }
    synchronizeChrome();
    Q_EMIT shellVisibilityStateChanged();
    return true;
}

} // namespace QindaQt::Compositor::KWinIntegration
