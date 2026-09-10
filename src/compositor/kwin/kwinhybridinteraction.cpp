// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinhybridsession.h"

#include "hybridchromedragtranslator.h"
#include "hybridchromepointerrouter.h"
#include "hybridcontainerplacement.h"
#include "hybridgroupedgeometryreconciler.h"
#include "hybridinteractionruntime.h"
#include "kwinchromemanager.h"
#include "kwindockpreview.h"
#include "kwinhybridgroupstacking.h"
#include "kwinmemberpolicy.h"
#include "managedwindowregistry.h"

#include <core/output.h>
#include <window.h>
#include <workspace.h>

#include <QSet>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

void warnRuntimeFailure(QLatin1StringView operation,
                        const HybridRuntimeResult &result)
{
    if (result.status == HybridRuntimeStatus::Rejected
        || result.status == HybridRuntimeStatus::Unsupported
        || result.status == HybridRuntimeStatus::NeedsGeometry) {
        qWarning("QindaQt Hybrid %s failed: %s",
                 qPrintable(QString(operation)), qPrintable(result.message));
    }
}

QString firstWindowId(const Core::LayoutNode &root)
{
    const auto *node = &root;
    while (node->isSplit()) {
        node = node->firstChild();
    }
    return node->windowId();
}

QString activeRepresentative(const Core::WindowContainer &container)
{
    const auto *page = container.page(container.activePageId());
    return page ? firstWindowId(page->root()) : QString{};
}

} // namespace

qreal KWinHybridSession::containerScale(const QString &containerId) const
{
    const auto *container = m_runtime->topology().container(containerId);
    auto *window = container
        ? m_registry.window(activeRepresentative(*container)) : nullptr;
    return window && window->output() ? window->output()->scale() : 1.0;
}

QRect KWinHybridSession::workArea(const QString &containerId) const
{
    const auto *container = m_runtime->topology().container(containerId);
    auto *window = container
        ? m_registry.window(activeRepresentative(*container)) : nullptr;
    if (!window) {
        return {};
    }
    return KWin::workspace()->clientArea(KWin::MaximizeArea, window)
        .toAlignedRect();
}

void KWinHybridSession::initializeGroupedGeometryReconciliation()
{
    m_groupedGeometryReconciler =
        std::make_unique<HybridGroupedGeometryReconciler>(
            [this] {
                QVector<GroupedWindowGeometry> result;
                QSet<QString> focusOwners;
                if (m_memberPolicy) {
                    for (const auto &state : m_memberPolicy->focusStates()) {
                        focusOwners.insert(state.windowId);
                    }
                }
                for (const auto &windowId : m_registry.windowIds()) {
                    auto *const window = m_registry.window(windowId);
                    if (!window) {
                        continue;
                    }
                    const auto owner = m_registry.owner(windowId);
                    result.append({.windowId = windowId,
                                   .containerId = owner,
                                   .requestedFrame = window->moveResizeGeometry(),
                                   .targetFrame = m_registry.targetFrame(windowId),
                                   .nativeFrameOverride =
                                       m_minimizedContainers.contains(owner)
                                       || (m_placement
                                           && m_placement->isMaximized(owner))
                                       || focusOwners.contains(windowId)});
                }
                return result;
            },
            [this](const QString &windowId, const QRectF &target, QString *error) {
                auto *const window = m_registry.window(windowId);
                if (!window || m_registry.owner(windowId).isEmpty()) {
                    if (error) {
                        *error = QStringLiteral("owned window disappeared");
                    }
                    return false;
                }
                window->moveResize(target);
                if (window->moveResizeGeometry() != target) {
                    if (error) {
                        *error = QStringLiteral("KWin did not accept the target frame");
                    }
                    return false;
                }
                return true;
            });
}

void KWinHybridSession::reconcileWorkAreaGeometry()
{
    if (m_shutdown) {
        return;
    }
    if (m_placement) {
        for (const auto &failure : m_placement->refreshMaximizedAreas()) {
            qWarning("QindaQt Hybrid maximize-area refresh failed: %s",
                     qPrintable(failure));
        }
    }
    // KWin constrains every ordinary client separately after a strut change.
    // Reassert the container solver's target frames only after maximized groups
    // have published their new area-dependent layout.
    if (m_groupedGeometryReconciler) {
        for (const auto &failure : m_groupedGeometryReconciler->reconcile()) {
            qWarning("QindaQt Hybrid grouped geometry reconciliation failed: %s",
                     qPrintable(failure));
        }
    }
}

void KWinHybridSession::dispatchIntent(const HybridInput::InteractionIntent &intent)
{
    if (!ready()) {
        return;
    }
    if (intent.phase == HybridInput::IntentPhase::Commit) {
        QString error;
        if (!restoreMemberFocusForInteraction(&error)) {
            qWarning("QindaQt Hybrid interaction could not leave member focus: %s",
                     qPrintable(error));
            if (m_dockPreview) {
                m_dockPreview->clear();
            }
            return;
        }
    }
    const auto result = m_runtime->handleIntent(intent);
    warnRuntimeFailure(QLatin1StringView("interaction"), result);
    if (intent.phase == HybridInput::IntentPhase::Commit
        || intent.phase == HybridInput::IntentPhase::Cancel) {
        m_dockPreview->clear();
    }
    if (result.topologyChanged()) {
        synchronizeChrome();
    }
}

void KWinHybridSession::dispatchChromePointerDecision(
    const ChromePointerDecision &decision)
{
    if (!ready()) {
        return;
    }
    if (decision.hoverChanged) {
        m_chromeManager->setPointerHover(decision.hovered);
    }
    for (const auto &containerId : decision.containerRaiseRequests) {
        // AGENT-GUARD: raiseContainer's default activation targets the
        // group's activation representative, which for a shaded container is
        // the content-preserving anchor -- genuinely Window::isHidden() by
        // design, not "a real client" the way its own AGENT-CONTRACT assumes.
        // Activating it anyway (observed via a live nested run) unhid and
        // natively detached the other, plain-hidden member as a side effect.
        // A shaded strip's press still needs the real z-order raise (an
        // occluded strip must come to front), just never the activation.
        const auto raiseActivation = m_placement && m_placement->isShaded(containerId)
            ? RaiseActivation::RaiseOnly
            : RaiseActivation::ActivateRepresentative;
        QString error;
        if (!m_groupStacking->raiseContainer(containerId, raiseActivation, &error)) {
            m_lastGroupStackingFailure = error;
            qWarning("QindaQt Hybrid group raise failed: %s", qPrintable(error));
            // The pointer router captured this press before requesting the
            // raise. Revoke the entire publication and grab before returning;
            // otherwise held-button motion can begin a stale same-ID drag.
            invalidateChromePublication();
            return;
        }
    }
    for (const auto &activation : decision.activations) {
        if (!m_chromeManager->dispatchPointerActivation(activation)) {
            qWarning("QindaQt ignored a stale shared-chrome activation");
        }
    }
    for (const auto &drag : decision.drags) {
        handleChromeDrag(drag.containerId, drag.event);
    }
    for (const auto &request : decision.contextMenus) {
        showGroupContextMenu(request.containerId, request.globalPosition);
    }
}

void KWinHybridSession::handleChromeDrag(
    const QString &containerId,
    const HybridChrome::ChromeDragEvent &event)
{
    if (!ready()) {
        return;
    }
    if (event.target.kind == HybridChrome::HitKind::OuterResize) {
        QString error;
        if (!m_placement->handleOuterResize(containerId, event, &error)) {
            qWarning("QindaQt Hybrid outer resize failed: %s", qPrintable(error));
        }
        return;
    }
    QString error;
    const auto intent = m_dragTranslator->translate(
        m_runtime->topology(), containerId, event, &error);
    if (!intent) {
        qWarning("QindaQt Hybrid chrome drag could not be translated: %s",
                 qPrintable(error));
        return;
    }
    dispatchIntent(*intent);
}

void KWinHybridSession::handleTabActivation(
    const QString &containerId, const QString &pageId)
{
    if (!ready()) {
        return;
    }
    QString error;
    if (!restoreMemberFocusForInteraction(&error)) {
        qWarning("QindaQt Hybrid tab activation could not leave member focus: %s",
                 qPrintable(error));
        return;
    }
    const auto result = m_runtime->activatePage(containerId, pageId);
    warnRuntimeFailure(QLatin1StringView("tab activation"), result);
    if (result.topologyChanged()) {
        synchronizeChrome();
    }
}

} // namespace QindaQt::Compositor::KWinIntegration
