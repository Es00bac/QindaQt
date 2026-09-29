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
    persistContainerAppearance();
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
    persistContainerAppearance();
    synchronizeChrome();
    Q_EMIT shellVisibilityStateChanged();
    return true;
}

void KWinHybridSession::loadContainerAppearance()
{
    const auto entries = m_appearanceLedger.load();
    for (auto it = entries.constBegin(); it != entries.constEnd(); ++it) {
        // AGENT-NOTE: setName/setColor are pure map writes with no topology
        // check (unlike the public renameContainer/setContainerColor above),
        // so staging an id that names no live container yet is harmless: it
        // is only ever read back through appearance()/displayName() for an
        // id a caller already resolved against the live topology.
        if (!it.value().name.isEmpty()) {
            (void)m_appearance.setName(it.key(), it.value().name);
        }
        if (!it.value().colorHex.isEmpty()) {
            (void)m_appearance.setColor(it.key(), it.value().colorHex);
        }
    }
}

void KWinHybridSession::persistContainerAppearance()
{
    m_appearanceLedger.save(m_appearance.snapshot());
}

} // namespace QindaQt::Compositor::KWinIntegration
