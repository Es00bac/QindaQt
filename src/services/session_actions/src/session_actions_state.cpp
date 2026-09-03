// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/session_actions/session_actions_client.h>

#include <utility>

namespace QindaQt::Services::SessionActions {

const SessionActionAvailability &SessionActionsClient::availability() const noexcept
{
    return m_availability;
}

bool SessionActionsClient::canLock() const noexcept { return m_availability.lock; }
bool SessionActionsClient::canLogout() const noexcept { return m_availability.logout; }
bool SessionActionsClient::canSuspend() const noexcept { return m_availability.suspend; }
bool SessionActionsClient::canReboot() const noexcept { return m_availability.reboot; }
bool SessionActionsClient::canPowerOff() const noexcept { return m_availability.powerOff; }
bool SessionActionsClient::pending() const noexcept { return m_pending.has_value(); }
QString SessionActionsClient::feedback() const { return m_feedback; }

bool SessionActionsClient::requestLock() { return requestAction(SessionAction::Lock); }
bool SessionActionsClient::requestLogout() { return requestAction(SessionAction::Logout); }
bool SessionActionsClient::requestSuspend() { return requestAction(SessionAction::Suspend); }
bool SessionActionsClient::requestReboot() { return requestAction(SessionAction::Reboot); }
bool SessionActionsClient::requestPowerOff() { return requestAction(SessionAction::PowerOff); }

void SessionActionsClient::clearFeedback()
{
    publishFeedback({});
}

void SessionActionsClient::publishAvailability(
    const SessionActionAvailability &availability)
{
    if (m_availability == availability) {
        return;
    }
    m_availability = availability;
    Q_EMIT availabilityChanged();
}

bool SessionActionsClient::cachedAvailable(SessionAction action) const noexcept
{
    switch (action) {
    case SessionAction::Lock: return m_availability.lock;
    case SessionAction::Logout: return m_availability.logout;
    case SessionAction::Suspend: return m_availability.suspend;
    case SessionAction::Reboot: return m_availability.reboot;
    case SessionAction::PowerOff: return m_availability.powerOff;
    }
    return false;
}

void SessionActionsClient::publishFeedback(QString feedback)
{
    if (m_feedback == feedback) {
        return;
    }
    m_feedback = std::move(feedback);
    Q_EMIT feedbackChanged();
}

} // namespace QindaQt::Services::SessionActions
