// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/notification_presentation_model/notification_presentation_controller.h"

#include "qindaqt/services/notification_presentation_client/notification_presentation_client.h"

namespace QindaQt::Services::NotificationPresentationModel {

void NotificationPresentationController::handlePrivacyPolicyChanged()
{
    if (!privatePresentationAllowed()) {
        clearPrivatePresentation();
    } else {
        // AGENT-GUARD: granting presentation always consumes the current
        // authoritative snapshot as a fresh baseline. Never retain m_previous
        // across a private interval or notifications observed while locked can
        // replay as banners/history on unlock.
        synchronize();
    }
    Q_EMIT privatePresentationAllowedChanged();
}

void NotificationPresentationController::clearPrivatePresentation()
{
    const bool hadPopups = m_popups.rowCount() > 0;
    const bool wasCenterOpen = m_centerOpen;
    const bool exposedBusy =
        !m_suppressCurrentOperationOutcome && m_client.operationInFlight();

    m_popupTimer.stop();
    m_operationErrorTimer.stop();
    if (m_client.operationInFlight() || m_pendingOperationId) {
        // The transport cannot be synchronously cancelled. Its exact outcome
        // is consumed but never projected, even if unlock happens before it
        // settles.
        m_suppressCurrentOperationOutcome = true;
    }
    m_pendingOperationId.reset();
    // NotificationListModel::replace is equality guarded, so repeated client
    // state changes while private do not emit noisy empty-model resets.
    m_active.replace({});
    m_popupEntries.clear();
    m_popups.replace({});
    m_historyEntries.clear();
    m_history.replace({});
    m_previous.clear();
    m_epoch.clear();
    m_baselined = false;
    m_sequence = 0;
    m_centerOpen = false;
    setOperationError({});

    if (hadPopups) {
        Q_EMIT popupCountChanged();
    }
    if (wasCenterOpen) {
        Q_EMIT centerOpenChanged();
    }
    if (exposedBusy) {
        Q_EMIT operationBusyChanged();
    }
}

} // namespace QindaQt::Services::NotificationPresentationModel
