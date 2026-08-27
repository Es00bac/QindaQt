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
    connect(&m_client, &SettingsClient::snapshotChanged,
            this, &DoNotDisturbController::handleSnapshot);
    connect(&m_client, &SettingsClient::commitFinished,
            this, &DoNotDisturbController::handleCommit);
    connect(&m_client, &SettingsClient::commitUncertain, this, [this](const QString &message) {
        m_waitingForCommitSnapshot = false;
        m_conflictIntent = false;
        m_hasRequestedValue = false;
        setState(State::Unavailable, message.left(512));
    });
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
    if (!ready()) {
        return false;
    }
    // A new explicit write is the dismissal contract for an earlier confirmed
    // rejection diagnostic. Automatic authority refresh never clears it.
    m_confirmedError.clear();
    m_requestedValue = enabled;
    m_hasRequestedValue = true;
    m_waitingForCommitSnapshot = false;
    m_conflictIntent = false;
    QString error;
    if (!m_client.setUserValue(DoNotDisturbKey, enabled, &error)) {
        m_hasRequestedValue = false;
        setState(State::Unavailable, error.left(512));
        return false;
    }
    setState(State::Saving);
    return true;
}

bool DoNotDisturbController::applyMyChoice()
{
    if (!conflict() || !m_hasRequestedValue || m_client.state() != ClientState::Ready) {
        return false;
    }
    const bool requested = m_requestedValue;
    setState(State::Ready);
    return requestSet(requested);
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
    switch (m_client.state()) {
    case ClientState::Ready:
        // SettingsClient emits snapshotChanged immediately after Ready. Keep
        // accepted/conflict intent private until that fresh baseline resolves it.
        if (m_hasBaseline && !m_waitingForCommitSnapshot && !m_conflictIntent) {
            setState(State::Ready);
        }
        break;
    case ClientState::Authenticating:
        // A retained baseline is not current authority. Replacement/loss must
        // hide Conflict actions and never leave a stale Saving claim visible.
        setState(m_hasBaseline ? State::Unavailable : State::Loading,
                 m_client.lastError());
        break;
    case ClientState::Unavailable:
    case ClientState::Degraded:
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
        m_hasRequestedValue = false;
    }
    setState(State::Ready);
}

void DoNotDisturbController::handleCommit(const CommitOutcome &outcome)
{
    if (outcome.status == SettingsProtocol::SettingsWireStatus::Applied) {
        m_waitingForCommitSnapshot = true;
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
