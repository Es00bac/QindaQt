// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinhybridsession.h"

#include "hybridcontainerplacement.h"
#include "hybridshadecontroller.h"
#include "kwinhybridgroupstacking.h"
#include "managedwindowregistry.h"

#include <scene/shadowitem.h>
#include <scene/windowitem.h>
#include <window.h>

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

// AGENT-CONTRACT: real KWin adapter for HybridShadeMemberPlatform. See
// ADR-0099's follow-up correction and docs/wiki/architecture/hybrid-chrome.md
// for the exact mechanism: Window::isHidden() removes a window from
// InputRedirection::findToplevel()'s pointer-input candidates (upstream
// src/input.cpp), and WindowItem::refVisible(PAINT_DISABLED_BY_HIDDEN) is the
// same ref-counted API KWin's own minimize/genie effects use to keep an
// item's outer WindowItem paintable despite Window::isHidden(); this class
// is the only place that touches these KWin scene internals.
class KWinShadeMemberPlatform final : public HybridShadeMemberPlatform
{
public:
    explicit KWinShadeMemberPlatform(ManagedWindowRegistry &registry)
        : m_registry(registry)
    {
    }

    bool hideMember(const QString &windowId, QString *error) override
    {
        auto *const window = m_registry.window(windowId);
        if (!window || window->isDeleted()) {
            return fail(error, QStringLiteral("shade member '%1' is unavailable").arg(windowId));
        }
        window->setHidden(true);
        return true;
    }

    bool showMember(const QString &windowId, QString *error) override
    {
        auto *const window = m_registry.window(windowId);
        if (!window || window->isDeleted()) {
            // A member that closed while shaded has nothing left to restore.
            return true;
        }
        Q_UNUSED(error)
        window->setHidden(false);
        return true;
    }

    bool hideAnchorContent(const QString &windowId, QString *error) override
    {
        auto *const window = m_registry.window(windowId);
        auto *const item = window && !window->isDeleted() ? window->windowItem() : nullptr;
        if (!window || !item) {
            return fail(error,
                        QStringLiteral("shade anchor '%1' has no scene item").arg(windowId));
        }
        // Ref before hiding so the WindowItem never has a frame where
        // computeVisibility() would transiently drop it (and the chrome
        // image parented to it) before the ref takes effect.
        item->refVisible(KWin::WindowItem::PAINT_DISABLED_BY_HIDDEN);
        window->setHidden(true);
        item->windowContainer()->setVisible(false);
        if (auto *const shadow = item->shadowItem()) {
            shadow->setVisible(false);
        }
        return true;
    }

    bool showAnchorContent(const QString &windowId, QString *error) override
    {
        auto *const window = m_registry.window(windowId);
        auto *const item = window && !window->isDeleted() ? window->windowItem() : nullptr;
        if (!window || !item) {
            // The anchor closed while shaded; nothing left to restore.
            Q_UNUSED(error)
            return true;
        }
        item->windowContainer()->setVisible(true);
        if (auto *const shadow = item->shadowItem()) {
            shadow->setVisible(true);
        }
        window->setHidden(false);
        item->unrefVisible(KWin::WindowItem::PAINT_DISABLED_BY_HIDDEN);
        return true;
    }

private:
    ManagedWindowRegistry &m_registry;
};

} // namespace

void KWinHybridSession::ensureShadeController()
{
    if (m_shadeController) {
        return;
    }
    m_shadeMemberPlatform = std::make_unique<KWinShadeMemberPlatform>(m_registry);
    m_shadeController = std::make_unique<HybridShadeController>(*m_shadeMemberPlatform);
}

bool KWinHybridSession::shadeContainer(const QString &containerId, QString *error)
{
    if (!ready() || !m_runtime->topology().container(containerId)) {
        return fail(error, QStringLiteral("the selected window group is stale"));
    }
    if (m_placement->isShaded(containerId)) {
        return true;
    }
    const auto memberIds = m_runtime->topology().windowIds(containerId);
    const QString anchorId = m_groupStacking
        ? m_groupStacking->anchorMemberId(containerId) : QString{};
    if (memberIds.isEmpty() || anchorId.isEmpty()) {
        return fail(error, QStringLiteral("group has no resolvable chrome anchor to shade"));
    }
    ensureShadeController();
    if (!m_shadeController->shadeMembers(containerId, memberIds, anchorId, error)) {
        return false;
    }
    if (!m_placement->shade(containerId, error)) {
        QString restoreError;
        if (!m_shadeController->unshadeMembers(containerId, &restoreError)) {
            qWarning("QindaQt shade rollback could not restore members: %s",
                     qPrintable(restoreError));
        }
        return false;
    }
    synchronizeChrome();
    return true;
}

bool KWinHybridSession::unshadeContainer(const QString &containerId, QString *error)
{
    if (!m_placement->isShaded(containerId)) {
        return fail(error, QStringLiteral("group is not shaded"));
    }
    // AGENT-GUARD: restore real geometry (placement) before member content/
    // input becomes visible again, so nothing paints mid-reflow at a stale
    // frame. A failed reflow leaves the group shaded and members hidden,
    // matching HybridContainerPlacementController::unshade's own contract.
    if (!m_placement->unshade(containerId, error)) {
        return false;
    }
    if (m_shadeController && !m_shadeController->unshadeMembers(containerId, error)) {
        return false;
    }
    synchronizeChrome();
    return true;
}

void KWinHybridSession::applyWheelShade(const QString &containerId, bool shade)
{
    if (!ready() || !m_runtime->topology().container(containerId)
        || isContainerShaded(containerId) == shade) {
        return;
    }
    QString error;
    const bool applied = restoreMemberFocusForContainerAction(containerId, &error)
        && (shade ? shadeContainer(containerId, &error)
                  : unshadeContainer(containerId, &error));
    if (!applied) {
        qWarning("QindaQt wheel roll-up failed for '%s': %s",
                 qPrintable(containerId), qPrintable(error));
    }
}

void KWinHybridSession::forgetShadedContainer(const QString &containerId)
{
    if (!m_shadeController || !m_shadeController->isShaded(containerId)) {
        return;
    }
    // AGENT-GUARD: A container can disappear (ungroup, detach-to-singleton,
    // forget/close) while shaded. Its real committed layout was never
    // touched by shade, so no geometry restore is needed here, but member
    // paint/input eligibility must be restored before the normal teardown
    // path (independent WindowRestoreState reapplication) runs, or a
    // surviving/detached member would stay permanently hidden.
    QString error;
    if (!m_shadeController->unshadeMembers(containerId, &error)) {
        qWarning("QindaQt shade teardown could not restore members for '%s': %s",
                 qPrintable(containerId), qPrintable(error));
    }
}

void KWinHybridSession::restoreShadeForShutdown()
{
    if (!m_shadeController) {
        return;
    }
    for (const auto &containerId : m_shadeController->shadedContainerIds()) {
        forgetShadedContainer(containerId);
    }
}

} // namespace QindaQt::Compositor::KWinIntegration
