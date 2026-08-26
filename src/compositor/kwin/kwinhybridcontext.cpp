// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinhybridsession.h"

#include "hybridinteractionruntime.h"
#include "kwinchromemanager.h"
#include "kwinhybridscene.h"
#include "kwininteractionfilter.h"
#include "managedwindowregistry.h"

namespace QindaQt::Compositor::KWinIntegration {

void KWinHybridSession::adoptMemberContext(
    const QString &containerId,
    const QString &sourceWindowId)
{
    if (!ready() || m_registry.owner(sourceWindowId) != containerId) {
        return;
    }
    const auto *container = m_runtime->topology().container(containerId);
    if (!container || !container->findWindow(sourceWindowId)) {
        return;
    }

    const auto recovery = recoverHybridGroupContext(
        containerId,
        [this, container, &sourceWindowId](QString *error) {
            const auto adopted = m_sceneFactory->recontextualizeContainer(
                *container, sourceWindowId);
            if (error) {
                *error = adopted.message;
            }
            return adopted.succeeded;
        },
        [this, &containerId](QString *error) {
            const auto released = m_runtime->releaseContainer(containerId);
            if (error) {
                *error = released.message;
            }
            return released.topologyChanged();
        },
        [this](const QString &id, bool coherent) {
            if (coherent) {
                m_chromeManager->markContainerContextCoherent(id);
                return;
            }
            // AGENT-GUARD: The manager quarantine persists across ordinary
            // synchronization and scene recreation. Publish it before
            // cancellation so re-entrant placement work already observes the
            // fail-closed state, then cancel shared-chrome, exact-modifier,
            // and keyboard interactions that may retain the unsafe group.
            m_chromeManager->quarantineContainer(id);
            if (m_inputFilter) {
                m_inputFilter->cancel();
            }
        });
    if (recovery.status != HybridGroupContextRecoveryStatus::Adopted) {
        qWarning("QindaQt could not atomically adopt group context: %s",
                 qPrintable(recovery.adoptionError));
    }
    if (recovery.status == HybridGroupContextRecoveryStatus::Quarantined) {
        qCritical("QindaQt could not recover split group '%s': %s",
                  qPrintable(containerId), qPrintable(recovery.releaseError));
    }
    synchronizeChrome();
}

} // namespace QindaQt::Compositor::KWinIntegration
