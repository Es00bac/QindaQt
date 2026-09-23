// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/settings_client/do_not_disturb_controller.h"

#include "qindaqt/services/settings_client/settings_client.h"

#include <utility>

namespace QindaQt::Services::SettingsClient {
namespace {
const QString DoNotDisturbKey = QStringLiteral("services.doNotDisturb");
}

DoNotDisturbController::DoNotDisturbController(SettingsClient &client, QObject *parent)
    : QObject(parent), m_client(client)
{
    connect(&m_client, &SettingsClient::stateChanged,
            this, &DoNotDisturbController::handleClientState);
    connect(&m_client, &SettingsClient::ownerChanged,
            this, &DoNotDisturbController::handleClientState);
    connect(&m_client, &SettingsClient::snapshotChanged,
            this, &DoNotDisturbController::handleSnapshot);
    connect(&m_client, &SettingsClient::commitFinished,
            this, &DoNotDisturbController::handleCommit);
    connect(&m_client, &SettingsClient::writeInFlightChanged,
            this, &DoNotDisturbController::stateChanged);
    connect(&m_client, &SettingsClient::commitUncertain, this, [this](const QString &message) {
        if (!m_commitInFlight) return;
        m_commitInFlight = false;
        m_waitingForCommitSnapshot = false;
        m_conflictIntent = false;
        m_hasRequestedValue = false;
        setState(State::Unavailable, message.left(512));
    });
}

bool DoNotDisturbController::canToggle() const noexcept
{
    return ready() && m_client.state() == ClientState::Ready && !m_client.writeInFlight();
}

QString DoNotDisturbController::errorText() const
{
    return m_transientError.isEmpty() ? m_confirmedError : m_transientError;
}

QString DoNotDisturbController::statusText() const
{
    switch (m_state) {
    case State::Loading: return QStringLiteral("Loading Do Not Disturb setting…");
    case State::Ready: return {};
    case State::Saving: return QStringLiteral("Saving…");
    case State::Conflict: return QStringLiteral("Changed elsewhere; current value reloaded");
    case State::Unavailable:
        return m_hasBaseline
                   ? QStringLiteral("Last confirmed: %1").arg(m_enabled ? QStringLiteral("On")
                                                                       : QStringLiteral("Off"))
                   : QStringLiteral("Do Not Disturb setting unavailable");
    }
    return {};
}

bool DoNotDisturbController::requestSet(bool enabled)
{
    if (!canToggle()) {
        return false;
    }
    // A new explicit write is the dismissal contract for an earlier confirmed
    // rejection diagnostic. Automatic authority refresh never clears it.
    m_requestedValue = enabled;
    m_hasRequestedValue = true;
    m_commitInFlight = true;
    m_writeOwner = m_client.currentOwner();
    m_readbackRevision = 0;
    m_waitingForCommitSnapshot = false;
    m_conflictIntent = false;
    QString error;
    if (!m_client.setUserValue(DoNotDisturbKey, enabled, &error)) {
        m_commitInFlight = false;
        m_hasRequestedValue = false;
        setState(State::Ready, error.left(512));
        return false;
    }
    m_confirmedError.clear();
    setState(State::Saving);
    return true;
}

bool DoNotDisturbController::applyMyChoice()
{
    if (!conflict() || !m_hasRequestedValue || m_client.state() != ClientState::Ready
        || m_client.writeInFlight()) {
        return false;
    }
    const bool requested = m_requestedValue;
    const QString previousError = m_transientError;
    setState(State::Ready);
    if (requestSet(requested)) return true;
    // Admission can still fail while a same-owner snapshot request occupies
    // the serial lane. Preserve the explicit conflict action and its intent.
    m_requestedValue = requested;
    m_hasRequestedValue = true;
    m_conflictIntent = true;
    setState(State::Conflict, previousError);
    return false;
}

void DoNotDisturbController::retry()
{
    // Do not claim Loading until the client actually enters Authenticating.
    // A repeated synchronous start failure can preserve the same client
    // state/error and emit no signal; retaining Unavailable keeps Retry honest.
    m_client.refresh();
}

void DoNotDisturbController::handleClientState()
{
    if (m_waitingForCommitSnapshot && m_writeOwner != m_client.currentOwner()) {
        m_waitingForCommitSnapshot = false;
        m_hasRequestedValue = false;
        m_readbackRevision = 0;
    }
    switch (m_client.state()) {
    case ClientState::Ready:
        // SettingsClient emits snapshotChanged immediately after Ready. Keep
        // accepted/conflict intent private until that fresh baseline resolves it.
        if (m_hasBaseline && !m_waitingForCommitSnapshot && !m_conflictIntent) {
            setState(State::Ready);
        }
        break;
    case ClientState::Authenticating:
        // A same-owner refresh also follows a schedule commit on this client.
        // Its refusal text belongs to that control, not to Do Not Disturb.
        if (m_client.snapshot() &&
            m_client.snapshot()->owner == m_client.currentOwner()) {
            setState(m_waitingForCommitSnapshot ? State::Saving : State::Loading);
        } else {
            setState(m_hasBaseline ? State::Unavailable : State::Loading,
                     m_client.lastError());
        }
        break;
    case ClientState::Unavailable:
    case ClientState::Degraded:
        // A successful commit without confirmable readback is not an active
        // save. Later recovery may display authority but cannot replay intent.
        m_waitingForCommitSnapshot = false;
        m_hasRequestedValue = false;
        m_readbackRevision = 0;
        setState(State::Unavailable, m_client.lastError());
        break;
    }
}

void DoNotDisturbController::handleSnapshot()
{
    if (!m_client.snapshot()) return;
    const QVariant value = m_client.snapshot()->values.value(DoNotDisturbKey);
    if (value.metaType().id() != QMetaType::Bool) {
        setState(State::Unavailable, QStringLiteral("Do Not Disturb setting has an invalid type"));
        return;
    }
    const bool next = value.toBool();
    if (next != m_enabled) {
        m_enabled = next;
        Q_EMIT enabledChanged();
    }
    if (!m_hasBaseline) {
        m_hasBaseline = true;
        Q_EMIT hasBaselineChanged();
    }
    Q_EMIT confirmedValue(m_enabled);
    if (m_conflictIntent) {
        m_waitingForCommitSnapshot = false;
        if (m_hasRequestedValue && m_enabled != m_requestedValue) {
            setState(State::Conflict,
                     QStringLiteral("Changed elsewhere; current value reloaded"));
            return;
        }
        m_conflictIntent = false;
        m_hasRequestedValue = false;
    } else if (m_waitingForCommitSnapshot) {
        m_waitingForCommitSnapshot = false;
        if (m_client.snapshot()->revision < m_readbackRevision) {
            m_hasRequestedValue = false;
            m_readbackRevision = 0;
            setState(State::Unavailable,
                     QStringLiteral("Do Not Disturb save could not be confirmed"));
            return;
        }
        m_readbackRevision = 0;
        if (m_enabled != m_requestedValue) {
            m_conflictIntent = true;
            setState(State::Conflict,
                     QStringLiteral("Changed elsewhere; current value reloaded"));
            return;
        }
        m_hasRequestedValue = false;
    }
    setState(State::Ready);
}

void DoNotDisturbController::handleCommit(const CommitOutcome &outcome)
{
    // AGENT-GUARD: SettingsClient's completion signal is intentionally
    // untagged. The other Notifications control shares its serial write lane.
    if (!m_commitInFlight) return;
    m_commitInFlight = false;
    if (outcome.status == SettingsProtocol::SettingsWireStatus::Applied) {
        m_waitingForCommitSnapshot = true;
        m_readbackRevision = outcome.revisionAfter;
        m_conflictIntent = false;
        setState(State::Saving);
        return;
    }
    if (outcome.status == SettingsProtocol::SettingsWireStatus::Conflict) {
        const QVariant current = outcome.currentValues.value(DoNotDisturbKey);
        if (m_hasRequestedValue && current.metaType().id() == QMetaType::Bool
            && current.toBool() == m_requestedValue) {
            m_waitingForCommitSnapshot = true;
            m_conflictIntent = false;
            setState(State::Saving);
        } else {
            m_waitingForCommitSnapshot = false;
            m_conflictIntent = true;
            setState(State::Conflict,
                     QStringLiteral("Changed elsewhere; current value reloaded"));
        }
        return;
    }
    m_hasRequestedValue = false;
    m_waitingForCommitSnapshot = false;
    m_conflictIntent = false;
    m_confirmedError = outcome.message.left(512);
    if (m_confirmedError.isEmpty()) {
        m_confirmedError = QStringLiteral("Settings save failed: %1")
                               .arg(SettingsProtocol::settingsWireStatusName(outcome.status));
    }
    setState(State::Ready);
}

void DoNotDisturbController::setState(State state, QString error)
{
    if (m_state == state && m_transientError == error) return;
    m_state = state;
    m_transientError = std::move(error);
    Q_EMIT stateChanged();
}

} // namespace QindaQt::Services::SettingsClient
