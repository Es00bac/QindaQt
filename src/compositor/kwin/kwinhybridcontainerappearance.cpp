// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinhybridsession.h"

#include "hybridcontainerplacement.h"
#include "hybridinteractionruntime.h"
#include <QInputDialog>
#include <QLineEdit>

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

QString KWinHybridSession::containerDisplayName(const QString &containerId) const
{
    return m_appearance.displayName(containerId);
}

QString KWinHybridSession::containerIdentityColor(const QString &containerId) const
{
    const auto override = m_appearance.appearance(containerId).colorHex;
    return override.isEmpty() ? m_chromeStyle.palette.accent.name().toUpper() : override;
}

void KWinHybridSession::synchronizeContainerIdentities()
{
    QVariantMap identities;
    for (const auto &containerId : m_runtime->topology().containerIds()) {
        const QVariantMap identity{
            {QStringLiteral("name"), containerDisplayName(containerId)},
            {QStringLiteral("color"), containerIdentityColor(containerId)}};
        for (const auto &windowId : m_runtime->topology().windowIds(containerId)) {
            identities.insert(windowId, identity);
        }
    }
    m_containerIdentities.publish(identities);
}

bool KWinHybridSession::promptContainerRename(const QString &containerId, QString *error)
{
    // The menu queues this after hiding, so the modal prompt never nests
    // inside a still-closing native popup. renameContainer revalidates lifetime.
    bool accepted = false;
    const auto entered = QInputDialog::getText(
        nullptr, tr("Rename Container"), tr("Container name:"), QLineEdit::Normal,
        containerAppearance(containerId).name, &accepted);
    return !accepted || renameContainer(containerId, entered, error);
}

bool KWinHybridSession::promptTabRename(
    const QString &containerId, const QString &pageId, QString *error)
{
    const auto *container = ready() ? m_runtime->topology().container(containerId) : nullptr;
    const auto *page = container ? container->page(pageId) : nullptr;
    if (!page) {
        if (error) *error = tr("The selected tab no longer exists");
        return false;
    }
    const QString current = page->name();
    bool accepted = false;
    const auto entered = QInputDialog::getText(
        nullptr, tr("Rename Tab"), tr("Tab name:"), QLineEdit::Normal, current, &accepted);
    if (!accepted) {
        return true;
    }
    // No borrowed page pointer survives the modal loop. Revalidate through
    // the atomic transaction, including shutdown while the prompt was open.
    if (!ready()) {
        if (error) *error = tr("The selected tab no longer exists");
        return false;
    }
    const auto result = m_runtime->renamePage(containerId, pageId, entered);
    if (!result.topologyChanged()) {
        if (error) *error = result.message;
        return false;
    }
    synchronizeChrome();
    return true;
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
