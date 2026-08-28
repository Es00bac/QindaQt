// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/apps/settings_appearance/appearance_settings_model.h"

#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/services/settings_protocol/settings_wire_status.h"

#include <utility>

namespace QindaQt::Apps::SettingsAppearance {

using Services::SettingsClient::ClientState;
using Services::SettingsClient::CommitOutcome;
using Services::SettingsClient::SettingsClient;
using Services::SettingsProtocol::SettingsWireStatus;

namespace {
constexpr int MaximumDiagnosticLength = 512;

[[nodiscard]] QString statusFailureMessage(SettingsWireStatus status)
{
    return QStringLiteral("Settings save failed: %1")
        .arg(Services::SettingsProtocol::settingsWireStatusName(status));
}
} // namespace

AppearanceSettingsModel::AppearanceSettingsModel(
    SettingsClient &client, QVector<Themes::ThemeSpec> installedThemes,
    Qt::ColorScheme platformScheme, DesignTokens::TokenFacade *previewFacade,
    QObject *parent)
    : QObject(parent),
      m_client(client),
      m_preview(std::move(installedThemes)),
      m_platformScheme(platformScheme),
      m_previewFacade(previewFacade)
{
    Q_ASSERT(m_client.thread() == thread());
    Q_ASSERT(m_previewFacade.isNull() || m_previewFacade->thread() == thread());
    connect(&m_client, &SettingsClient::stateChanged,
            this, &AppearanceSettingsModel::handleClientState);
    connect(&m_client, &SettingsClient::snapshotChanged,
            this, &AppearanceSettingsModel::handleSnapshot);
    connect(&m_client, &SettingsClient::commitFinished,
            this, &AppearanceSettingsModel::handleCommit);
    connect(&m_client, &SettingsClient::commitUncertain,
            this, &AppearanceSettingsModel::handleUncertain);

    m_validation = validateAppearanceDraft(m_draft, installedThemeIds());
    refreshValidationAndPreview();
}

bool AppearanceSettingsModel::loading() const noexcept
{
    return m_state == State::Loading;
}

bool AppearanceSettingsModel::ready() const noexcept
{
    return m_state == State::Ready;
}

bool AppearanceSettingsModel::saving() const noexcept
{
    return m_state == State::Saving;
}

bool AppearanceSettingsModel::conflict() const noexcept
{
    return m_state == State::Conflict;
}

bool AppearanceSettingsModel::unavailable() const noexcept
{
    return m_state == State::Unavailable;
}

bool AppearanceSettingsModel::canEdit() const noexcept
{
    // A resolved conflict has a fresh authoritative baseline. Keeping the
    // draft editable lets the user either revise/re-Apply it or Revert, while
    // Saving/Unavailable and the conflict reply-to-snapshot gap remain
    // fail-closed.
    return (ready() || conflict()) && m_authorityReady;
}

bool AppearanceSettingsModel::draftDirty() const noexcept
{
    return !(m_draft == m_confirmed);
}

bool AppearanceSettingsModel::draftValid() const noexcept
{
    return m_validation.valid;
}

bool AppearanceSettingsModel::applyAvailable() const
{
    // A Conflict with a fresh baseline is an answerable state: re-Apply is
    // the explicit user intent the Settings1 contract requires, so it is
    // offered beside the normal Ready path.
    return canEdit() && draftDirty() && draftValid();
}

QString AppearanceSettingsModel::statusText() const
{
    switch (m_state) {
    case State::Loading:
        return QStringLiteral("Loading appearance settings…");
    case State::Ready:
    case State::Saving:
        break;
    case State::Conflict:
        return QStringLiteral("Appearance changed elsewhere; current values reloaded");
    case State::Unavailable:
        return m_hasBaseline
            ? QStringLiteral("Last confirmed appearance settings retained; refresh to continue")
            : QStringLiteral("Appearance settings unavailable");
    }
    if (m_state == State::Saving) {
        return QStringLiteral("Saving appearance settings…");
    }
    return {};
}

QString AppearanceSettingsModel::errorText() const
{
    return m_transientError.isEmpty() ? m_confirmedError : m_transientError;
}

QVariantMap AppearanceSettingsModel::draft() const
{
    return m_draft.toVariantMap();
}

QVariantMap AppearanceSettingsModel::fieldErrors() const
{
    return m_validation.fieldErrors;
}

void AppearanceSettingsModel::retry()
{
    // Do not claim progress before the client actually retries; a repeated
    // synchronous start failure must keep the Unavailable truth visible.
    m_client.refresh();
}

void AppearanceSettingsModel::handleClientState()
{
    switch (m_client.state()) {
    case ClientState::Ready:
        // Snapshot handling promotes to Ready once fresh authority lands.
        // Keep Saving/Conflict intent private until that resolves them.
        break;
    case ClientState::Authenticating:
        // AGENT-GUARD: The client publishes Authenticating for startup,
        // replacement, and the routine refresh after every commit reply.
        // Killing held intent on the routine reply→snapshot transition would
        // abort every multi-key Apply at its first key, so Saving, the
        // final-snapshot wait, and unresolved conflict intent survive here.
        // handleSnapshot() verifies lineage when the fresh authority lands
        // and aborts explicitly if owner/epoch changed in the gap. Genuine
        // owner loss always publishes Unavailable, which still aborts.
        setAuthorityReady(false);
        if (m_sequenceActive || m_waitingFinalSnapshot || m_conflictIntent) {
            break;
        }
        setState(m_hasBaseline ? State::Unavailable : State::Loading,
                 m_client.lastError());
        break;
    case ClientState::Unavailable:
    case ClientState::Degraded:
        setAuthorityReady(false);
        if (m_sequenceActive && !m_queue.isEmpty()) {
            updateSaveResult(m_queue.first().key, SaveResultState::Uncertain,
                             m_client.lastError());
        }
        abortSequence();
        setState(State::Unavailable, m_client.lastError());
        break;
    }
}

void AppearanceSettingsModel::handleSnapshot()
{
    const auto &snapshot = m_client.snapshot();
    if (!snapshot) {
        return;
    }
    QString error;
    const auto decoded =
        AppearanceValues::fromVariantMap(snapshot->values, &error);
    if (!decoded.has_value()) {
        setAuthorityReady(false);
        abortSequence();
        setState(State::Unavailable,
                 error.isEmpty()
                     ? QStringLiteral("Appearance settings have an invalid value")
                     : error);
        return;
    }
    // AGENT-GUARD: Authority that changed owner/epoch between an applied key
    // and this snapshot is replacement authority. Feeding the remaining
    // queued intent to it would write stale user intent behind the
    // replacement's back; abort instead, keep the draft, and let an explicit
    // re-Apply restate it.
    const bool lineageChanged =
        m_hasBaseline && (m_confirmedOwner != snapshot->owner
                          || m_confirmedEpoch != snapshot->epoch);
    m_confirmedOwner = snapshot->owner;
    m_confirmedEpoch = snapshot->epoch;
    setConfirmed(*decoded);
    setAuthorityReady(true);

    if (lineageChanged
        && (m_sequenceActive || m_waitingFinalSnapshot || m_conflictIntent)) {
        abortSequence();
        m_conflictIntent = false;
        setState(State::Ready);
        return;
    }
    if (m_waitingFinalSnapshot) {
        m_waitingFinalSnapshot = false;
        if (*decoded == m_draft) {
            m_conflictIntent = false;
            setState(State::Ready);
        } else {
            // Another writer changed an appearance key after our commit.
            m_conflictIntent = true;
            setState(State::Conflict);
        }
        return;
    }
    if (m_sequenceActive) {
        // AGENT-GUARD: The client refreshes authority after every commit and
        // refuses writes until it is Ready again. Issue the next queued key
        // only here, from the fresh baseline, or every second write would
        // carry a stale base revision and fail as Conflict.
        if (m_client.state() != ClientState::Ready) {
            return;
        }
        writeNextQueuedKey();
        return;
    }
    if (m_conflictIntent) {
        if (*decoded == m_draft) {
            m_conflictIntent = false;
            setState(State::Ready);
        } else if (m_state != State::Conflict) {
            // Fresh authority confirms the unresolved conflict after a loss.
            setState(State::Conflict);
        }
        return;
    }
    setState(State::Ready);
}

void AppearanceSettingsModel::handleCommit(const CommitOutcome &outcome)
{
    if (!m_sequenceActive || m_queue.isEmpty()) {
        return;
    }

    const CommitIntent intended = m_queue.first();
    if (outcome.status == SettingsWireStatus::Applied) {
        updateSaveResult(intended.key, SaveResultState::Applied);
        m_queue.removeFirst();
        // AGENT-GUARD: Never write the next key from the commit reply. The
        // client still holds the pre-commit base revision until its automatic
        // refresh lands, and the service would reject the write as Conflict.
        // The fresh snapshot handler continues the sequence.
        if (m_queue.isEmpty()) {
            m_sequenceActive = false;
            m_waitingFinalSnapshot = true;
        }
        return;
    }

    if (outcome.status == SettingsWireStatus::Conflict) {
        const QVariant current = outcome.currentValues.value(intended.key);
        if (current == intended.value) {
            // The queued value already is authority; treat the key as done
            // once the matching fresh snapshot confirms it.
            updateSaveResult(intended.key, SaveResultState::Applied);
            m_queue.removeFirst();
            if (m_queue.isEmpty()) {
                m_sequenceActive = false;
                m_waitingFinalSnapshot = true;
            }
            return;
        }
        updateSaveResult(intended.key, SaveResultState::Conflict,
                         outcome.message);
        abortSequence();
        m_conflictIntent = true;
        setState(State::Conflict);
        return;
    }

    // Validation/read-only/persistence/exhaustion/unknown-key failures are
    // confirmed rejections: keep the diagnostic visible across rebaseline and
    // never resubmit. A new explicit Apply after fresh authority dismisses it.
    abortSequence();
    m_confirmedError =
        outcome.message.isEmpty() ? statusFailureMessage(outcome.status)
                                  : outcome.message.left(MaximumDiagnosticLength);
    updateSaveResult(intended.key, SaveResultState::Failed, m_confirmedError);
    setState(State::Ready);
}

void AppearanceSettingsModel::handleUncertain(const QString &message)
{
    // Timeout, owner replacement, or transport loss during a write is never
    // retried automatically; the user must refresh and re-apply explicitly.
    if (!m_queue.isEmpty()) {
        updateSaveResult(m_queue.first().key, SaveResultState::Uncertain,
                         message.left(MaximumDiagnosticLength));
    }
    abortSequence();
    setState(State::Unavailable,
             message.isEmpty()
                 ? QStringLiteral("Appearance commit outcome is uncertain")
                 : message.left(MaximumDiagnosticLength));
}

void AppearanceSettingsModel::setState(State state, QString transientError)
{
    // AGENT-GUARD: Transient errors are replaced on every state change, while
    // m_confirmedError is only cleared by an explicit new apply sequence.
    // Clearing it on automatic authority refresh would hide a confirmed
    // rejection the user has not yet been able to answer.
    const QString nextError = std::move(transientError);
    if (m_state == state && m_transientError == nextError) {
        return;
    }
    m_state = state;
    m_transientError = nextError;
    Q_EMIT stateChanged();
}

void AppearanceSettingsModel::setAuthorityReady(bool ready)
{
    if (m_authorityReady == ready) {
        return;
    }
    m_authorityReady = ready;
    // canEdit/applyAvailable depend on freshness as well as the visible state.
    // This explicit notification prevents controls from remaining enabled
    // across the commit-reply-to-snapshot Authenticating interval.
    Q_EMIT stateChanged();
}

void AppearanceSettingsModel::writeNextQueuedKey()
{
    if (m_queue.isEmpty()) {
        return;
    }
    QString error;
    const CommitIntent intent = m_queue.first();
    if (m_client.setUserValue(intent.key, intent.value, &error)) {
        return;
    }
    if (m_client.state() == ClientState::Ready) {
        // Ready but refusing this key is a caller error; publish it as truth
        // instead of spinning or silently dropping the user's Apply intent.
        abortSequence();
        setState(State::Unavailable, error.left(MaximumDiagnosticLength));
        return;
    }
    // Authority is refreshing (Authenticating/Degraded); the matching fresh
    // snapshot or the state-change path continues or aborts the sequence.
}

void AppearanceSettingsModel::abortSequence()
{
    m_queue.clear();
    m_sequenceActive = false;
    m_waitingFinalSnapshot = false;
}

} // namespace QindaQt::Apps::SettingsAppearance
