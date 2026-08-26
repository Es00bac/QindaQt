// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinhybridsession.h"

#include "hybridchromedragtranslator.h"
#include "hybridchromepointerrouter.h"
#include "hybridcontainerplacement.h"
#include "hybridinteractionruntime.h"
#include "kwinchromemanager.h"
#include "kwindockpreview.h"
#include "kwinhybridgroupstacking.h"

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

} // namespace

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
        QString error;
        if (!m_groupStacking->raiseContainer(containerId, &error)) {
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
