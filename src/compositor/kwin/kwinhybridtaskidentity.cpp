// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinhybridsession.h"

#include "hybridchromeaccessibilityregistry.h"
#include "hybridinteractionruntime.h"
#include "hybridsemanticcommand.h"
#include "hybridshortcutmanager.h"
#include "kwinchromemanager.h"
#include "kwinhybridscene.h"
#include "kwininteractionfilter.h"
#include "kwintaskidentitymanager.h"
#include "managedwindowregistry.h"

#include <workspace.h>

#include <utility>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

bool runtimeCommitted(const HybridRuntimeResult &result, QString *error)
{
    if (result.status == HybridRuntimeStatus::TopologyCommitted) {
        return true;
    }
    if (error) {
        *error = result.message.isEmpty()
            ? QStringLiteral("Hybrid topology command was rejected")
            : result.message;
    }
    return false;
}

} // namespace

void KWinHybridSession::initializeTaskIdentityAndShortcuts()
{
    m_taskIdentity = std::make_unique<KWinTaskIdentityManager>(
        m_registry,
        [this]() -> const Hybrid::WindowTopology & {
            return m_runtime->topology();
        },
        [this](const QString &containerId, const QString &pageId, QString *error) {
            if (!ready()) {
                if (error) {
                    *error = QStringLiteral("Hybrid session is not ready");
                }
                return false;
            }
            if (!restoreMemberFocusForInteraction(error)) {
                return false;
            }
            const auto result = m_runtime->activatePage(containerId, pageId);
            if (!runtimeCommitted(result, error)) {
                return false;
            }
            if (result.topologyChanged()) {
                synchronizeChrome();
            }
            return true;
        },
        [this](const QString &containerId, QString *error) {
            if (!ready() || !m_runtime->topology().container(containerId)) {
                if (error) {
                    *error = QStringLiteral("task minimize references a stale group");
                }
                return false;
            }
            if (!restoreMemberFocusForInteraction(error)) {
                return false;
            }
            minimizeContainer(containerId);
            return true;
        },
        [this] {
            return m_shutdown || m_applyingWindowAction
                || (m_sceneFactory && m_sceneFactory->applyingWindowStates());
        });
    m_accessibility = std::make_unique<HybridChromeAccessibilityRegistry>(
        HybridChromeAccessibleActions{
            .dispatch = [this](const HybridSemanticRequest &request, QString *error) {
                return dispatchSemanticRequest(request, error);
            },
        });
    connect(m_chromeManager.get(), &KWinChromeManager::overlayVisibilityChanged,
            this, [this](const QString &, bool) {
                if (!m_shutdown && !m_synchronizingChrome) {
                    synchronizeAccessibility();
                }
            });

    // AGENT-CONTRACT: Every registered action has a production callback. A
    // discoverable no-op global shortcut is a release defect, not a harmless
    // placeholder. Typed semantic requests keep keyboard and accessibility
    // page/group policy on the same topology transaction boundary.
    m_shortcuts = std::make_unique<HybridShortcutManager>(HybridShortcutTriggers{
        .dock = [this] { startKeyboardDock(); },
        .dockPage = [this] {
            dispatchSemanticShortcut(HybridSemanticCommand::BeginPageDock);
        },
        .moveGroup = [this] { startKeyboardMove(); },
        .resizeActiveSplit = [this] { startKeyboardDividerResize(); },
        .resizeGroup = [this] { startKeyboardContainerResize(); },
        .nextPage = [this] {
            dispatchSemanticShortcut(HybridSemanticCommand::ActivateNextPage);
        },
        .previousPage = [this] {
            dispatchSemanticShortcut(HybridSemanticCommand::ActivatePreviousPage);
        },
        .reorderPageNext = [this] {
            dispatchSemanticShortcut(HybridSemanticCommand::ReorderPageNext);
        },
        .reorderPagePrevious = [this] {
            dispatchSemanticShortcut(HybridSemanticCommand::ReorderPagePrevious);
        },
        .closeGroup = [this] {
            dispatchSemanticShortcut(HybridSemanticCommand::CloseGroup);
        },
        .minimizeGroup = [this] {
            dispatchSemanticShortcut(HybridSemanticCommand::MinimizeGroup);
        },
        .maximizeGroup = [this] {
            dispatchSemanticShortcut(HybridSemanticCommand::MaximizeGroup);
        },
        .restoreGroup = [this] {
            dispatchSemanticShortcut(HybridSemanticCommand::RestoreGroup);
        },
    });
}

void KWinHybridSession::dispatchSemanticShortcut(HybridSemanticCommand command)
{
    if (!ready()) {
        return;
    }
    QString error;
    const QString activeWindowId = m_registry.windowId(
        KWin::workspace()->activeWindow());
    const auto request = HybridSemanticCommandResolver::resolveActive(
        m_runtime->topology(), activeWindowId, command, &error);
    if (!request) {
        qWarning("QindaQt semantic shortcut was rejected: %s", qPrintable(error));
        return;
    }

    if (!dispatchSemanticRequest(*request, &error)) {
        qWarning("QindaQt semantic shortcut dispatch failed: %s", qPrintable(error));
    }
}

bool KWinHybridSession::dispatchSemanticRequest(
    const HybridSemanticRequest &request, QString *error)
{
    if (!ready()) {
        if (error) {
            *error = QStringLiteral("Hybrid session is not ready");
        }
        return false;
    }
    const HybridSemanticCommandDispatcher dispatcher({
        .beginPageDock = [this](const HybridInput::HitTarget &source,
                               QString *handlerError) {
            if (!restoreMemberFocusForInteraction(handlerError)) {
                return false;
            }
            if (m_inputFilter && m_inputFilter->beginKeyboardDock(source)) {
                return true;
            }
            if (handlerError) {
                *handlerError = QStringLiteral(
                    "keyboard page docking could not acquire input");
            }
            return false;
        },
        .activatePage = [this](const QString &containerId,
                              const QString &pageId,
                              QString *handlerError) {
            if (!restoreMemberFocusForInteraction(handlerError)) {
                return false;
            }
            const auto result = m_runtime->activatePage(containerId, pageId);
            if (!runtimeCommitted(result, handlerError)) {
                return false;
            }
            if (result.topologyChanged()) {
                synchronizeChrome();
            }
            return true;
        },
        .reorderPage = [this](const QString &containerId,
                             const QString &pageId,
                             qsizetype destinationPageIndex,
                             QString *handlerError) {
            if (!restoreMemberFocusForInteraction(handlerError)) {
                return false;
            }
            const auto result = m_runtime->reorderPage(
                containerId, pageId, destinationPageIndex);
            if (!runtimeCommitted(result, handlerError)) {
                return false;
            }
            if (result.topologyChanged()) {
                synchronizeChrome();
            }
            return true;
        },
        .groupWindowAction = [this](const QString &containerId,
                                   HybridChrome::WindowAction action,
                                   QString *handlerError) {
            return dispatchGroupWindowAction(containerId, action, handlerError);
        },
    });
    return dispatcher.dispatch(request, error);
}

void KWinHybridSession::synchronizeAccessibility()
{
    if (!m_accessibility || !m_runtime || !m_chromeManager) {
        return;
    }
    QString error;
    if (!m_accessibility->synchronize(
            m_runtime->topology().containerIds(),
            [this](const QString &containerId)
                -> std::optional<HybridChrome::ChromeRenderPlan> {
                return m_chromeManager->plan(containerId);
            },
            [this](const QString &containerId) {
                return m_chromeManager->tabRepresentatives(containerId);
            },
            [this](const QString &containerId) {
                return m_chromeManager->overlayVisible(containerId);
            },
            &error)) {
        qWarning("QindaQt accessible chrome synchronization failed: %s",
                 qPrintable(error));
    }
}

void KWinHybridSession::shutdownAccessibility() noexcept
{
    if (m_accessibility) {
        // This destroys virtual QAccessible roots and unregisters the
        // process-global factory before the plugin DSO or chrome plans vanish.
        m_accessibility->clear();
        m_accessibility.reset();
    }
}

void KWinHybridSession::synchronizeTaskIdentity()
{
    if (!m_taskIdentity || !m_runtime) {
        return;
    }
    QString error;
    if (!m_taskIdentity->synchronize(m_runtime->topology(), &error)) {
        qWarning("QindaQt task identity synchronization failed: %s",
                 qPrintable(error));
    }
}

void KWinHybridSession::shutdownTaskIdentity() noexcept
{
    if (m_taskIdentity) {
        m_taskIdentity->shutdown();
        m_taskIdentity.reset();
    }
}

} // namespace QindaQt::Compositor::KWinIntegration
