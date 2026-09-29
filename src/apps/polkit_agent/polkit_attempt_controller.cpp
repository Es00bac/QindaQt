// SPDX-License-Identifier: GPL-3.0-or-later
#include "polkit_attempt_controller.h"

#include <utility>

namespace QindaQt::Apps::PolkitAgent {

PolkitAttemptController::PolkitAttemptController(QList<AgentIdentity> identities,
                                                 int preferredIdentityIndex,
                                                 AttemptFactory factory, QObject *parent)
    : QObject(parent)
    , m_identities(std::move(identities))
    , m_selectedIndex(preferredIdentityIndex)
    , m_factory(std::move(factory))
{
}

void PolkitAttemptController::setSelectedIdentityIndex(int index)
{
    if (m_started || index < 0 || index >= m_identities.size()) {
        return;
    }
    m_selectedIndex = index;
}

void PolkitAttemptController::start()
{
    if (m_started || m_finished || m_selectedIndex < 0
        || m_selectedIndex >= m_identities.size()) {
        return;
    }
    m_started = true;
    beginAttempt();
}

void PolkitAttemptController::beginAttempt()
{
    m_attempt = m_factory(m_identities.at(m_selectedIndex).token);
    connect(m_attempt.get(), &AuthenticationAttempt::request, this,
           &PolkitAttemptController::promptRequested);
    connect(m_attempt.get(), &AuthenticationAttempt::showInfo, this,
           &PolkitAttemptController::infoMessage);
    connect(m_attempt.get(), &AuthenticationAttempt::showError, this,
           &PolkitAttemptController::errorMessage);
    connect(m_attempt.get(), &AuthenticationAttempt::completed, this,
           &PolkitAttemptController::handleCompleted);
}

void PolkitAttemptController::authenticate(const QString &response)
{
    if (!m_started || m_finished || !m_attempt) {
        return;
    }
    m_attempt->respond(response);
}

void PolkitAttemptController::cancel()
{
    if (m_finished) {
        return;
    }
    m_cancelRequested = true;
    if (m_attempt) {
        m_attempt->cancel();
    } else {
        // Cancelled before start(); there is no live attempt to unwind.
        handleCompleted(false);
    }
}

void PolkitAttemptController::cancelExternally()
{
    if (m_finished) {
        return;
    }
    // AGENT-NOTE: polkitd's cancelAuthentication() carries no cookie in the
    // installed polkit-qt6 API (PolkitQt1::Agent::Listener::cancelAuthentication()
    // takes no argument), so this always means "the one request this agent
    // currently has open." PolkitRequestQueue::cancelActive() is responsible
    // for routing it to the right controller.
    m_cancelRequested = true;
    if (m_attempt) {
        m_attempt->cancel();
    } else {
        handleCompleted(false);
    }
}

void PolkitAttemptController::handleCompleted(bool gainedAuthorization)
{
    if (m_finished) {
        return;
    }
    if (gainedAuthorization || m_cancelRequested) {
        m_finished = true;
        Q_EMIT completed(gainedAuthorization);
        return;
    }
    // A genuine failure (wrong password) that nobody cancelled: the dialog
    // stays open and a fresh Session starts for the same identity.
    Q_EMIT attemptFailed(tr("That password didn't work. Try again."));
    beginAttempt();
}

} // namespace QindaQt::Apps::PolkitAgent
