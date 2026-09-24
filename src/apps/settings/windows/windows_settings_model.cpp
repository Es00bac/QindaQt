// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_windows/windows_settings_model.h>

#include <qindaqt/services/settings_client/settings_client.h>
#include <qindaqt/services/settings_protocol/settings_wire_status.h>

#include <utility>

namespace QindaQt::Apps::SettingsWindows {

using Services::SettingsClient::ClientState;
using Services::SettingsClient::CommitOutcome;
using Services::SettingsClient::SettingsClient;
using Services::SettingsProtocol::SettingsWireStatus;

namespace {
constexpr qsizetype MaximumDiagnosticLength = 512;

[[nodiscard]] QString statusFailureMessage(SettingsWireStatus status)
{
    return QStringLiteral("Settings1 rejected the window management change: %1")
        .arg(Services::SettingsProtocol::settingsWireStatusName(status));
}
} // namespace

WindowsSettingsModel::WindowsSettingsModel(SettingsClient &client,
                                                       QObject *parent)
    : QObject(parent)
    , m_client(client)
{
    Q_ASSERT(m_client.thread() == thread());
    connect(&m_client, &SettingsClient::stateChanged,
            this, &WindowsSettingsModel::handleClientState);
    connect(&m_client, &SettingsClient::snapshotChanged,
            this, &WindowsSettingsModel::handleSnapshot);
    connect(&m_client, &SettingsClient::commitFinished,
            this, &WindowsSettingsModel::handleCommit);
    connect(&m_client, &SettingsClient::commitUncertain,
            this, &WindowsSettingsModel::handleUncertain);
}

bool WindowsSettingsModel::canEdit() const noexcept
{
    // A resolved conflict has a fresh authoritative baseline, so the draft
    // stays editable for revise/re-Apply or Revert. Saving, Unavailable, and
    // the commit-reply-to-snapshot gap remain fail-closed.
    return m_hasBaseline && (ready() || conflict()) && m_authorityReady;
}

bool WindowsSettingsModel::draftDirty() const noexcept
{
    return m_hasBaseline && m_draft != m_confirmed;
}

bool WindowsSettingsModel::applyAvailable() const noexcept
{
    return canEdit() && draftDirty();
}

QString WindowsSettingsModel::statusText() const
{
    switch (m_state) {
    case State::Loading:
        return QStringLiteral("Loading window settings…");
    case State::Ready:
        return {};
    case State::Saving:
        return QStringLiteral("Saving window settings…");
    case State::Conflict:
        return QStringLiteral("Changed elsewhere. Current values were reloaded; apply again to keep your changes.");
    case State::Unavailable:
        return m_hasBaseline
            ? QStringLiteral("Settings1 is unavailable; the last confirmed values are shown.")
            : QStringLiteral("Window settings are unavailable.");
    }
    return {};
}

QString WindowsSettingsModel::errorText() const
{
    // A confirmed rejection or uncertain outcome outranks the client's
    // transient authority diagnostic so the "not replayed" truth stays
    // visible while the authority state churns; Apply/Revert clear it.
    return m_confirmedError.isEmpty() ? m_transientError : m_confirmedError;
}

QString WindowsSettingsModel::savedStatusText() const
{
    if (!m_hasBaseline) {
        return loading() ? QStringLiteral("Loading saved preferences…")
                         : QStringLiteral("Saved preferences are unavailable.");
    }
    if (saving()) {
        return QStringLiteral("Saving changes…");
    }
    if (!m_authorityReady || unavailable()) {
        return QStringLiteral("Showing the last confirmed saved preference; current settings "
                              "authority is unavailable.");
    }
    if (draftDirty()) {
        return QStringLiteral("Unsaved changes");
    }
    return QStringLiteral("Saved preference");
}

bool WindowsSettingsModel::sessionApplyMatchesConfirmed() const noexcept
{
    return m_hasBaseline && m_sessionApply.preferences.has_value()
        && *m_sessionApply.preferences == m_confirmed;
}

bool WindowsSettingsModel::sessionApplyFailed() const noexcept
{
    return m_sessionApply.serviceAvailable
        && m_sessionApply.phase == SessionApplyPhase::Failed
        && sessionApplyMatchesConfirmed();
}

bool WindowsSettingsModel::applyRetryAvailable() const noexcept
{
    return sessionApplyFailed() && canEdit() && !draftDirty();
}

QString WindowsSettingsModel::sessionApplyStatusText() const
{
    if (!m_hasBaseline) {
        return QStringLiteral("Waiting for saved preferences before checking session effect.");
    }
    if (saving()) {
        return QStringLiteral("Waiting for Settings1 to confirm the saved preference.");
    }
    if (draftDirty()) {
        return QStringLiteral("The session applies saved preferences after these changes are saved.");
    }
    if (!m_authorityReady || unavailable()) {
        return QStringLiteral("The last confirmed preference is shown; its current session "
                              "effect cannot be compared while Settings1 is unavailable.");
    }
    if (!m_sessionApply.serviceAvailable) {
        return QStringLiteral("Saved preference; session apply status is unavailable. Check "
                              "that the QindaQt session is running.");
    }
    if (m_sessionApply.phase == SessionApplyPhase::Unavailable) {
        return m_sessionApply.message.isEmpty()
            ? QStringLiteral("Saved preference; the session cannot currently apply it.")
            : QStringLiteral("Saved preference; %1").arg(m_sessionApply.message);
    }
    if (!sessionApplyMatchesConfirmed()) {
        return QStringLiteral("Waiting for the session to apply the current saved preference.");
    }
    switch (m_sessionApply.phase) {
    case SessionApplyPhase::Unavailable:
        return m_sessionApply.message.isEmpty()
            ? QStringLiteral("Saved preference; KWin is unavailable in this session.")
            : QStringLiteral("Saved preference; %1").arg(m_sessionApply.message);
    case SessionApplyPhase::Applying:
        return QStringLiteral("Saved preference; applying to this session…");
    case SessionApplyPhase::Applied:
        return QStringLiteral("Applied in this session.");
    case SessionApplyPhase::Failed:
        return QStringLiteral("Saved, but not applied: %1")
            .arg(m_sessionApply.message.isEmpty()
                     ? QStringLiteral("the session could not reload the window settings")
                     : m_sessionApply.message);
    }
    return {};
}

bool WindowsSettingsModel::setDraftToken(QString WindowsValues::*field,
                                         const QStringList &allowed, const QString &token)
{
    if (!canEdit() || !allowed.contains(token)) {
        return false;
    }
    if (m_draft.*field == token) {
        return true;
    }
    m_draft.*field = token;
    Q_EMIT viewChanged();
    return true;
}

bool WindowsSettingsModel::setDraftFocusPolicy(const QString &token)
{
    return setDraftToken(&WindowsValues::focusPolicy, WindowsValues::focusPolicyTokens(), token);
}

bool WindowsSettingsModel::setDraftDockingModifier(const QString &token)
{
    return setDraftToken(&WindowsValues::dockingModifier,
                         WindowsValues::dockingModifierTokens(), token);
}

bool WindowsSettingsModel::setDraftCloseContainerPolicy(const QString &token)
{
    return setDraftToken(&WindowsValues::closeContainerPolicy,
                         WindowsValues::closeContainerPolicyTokens(), token);
}

bool WindowsSettingsModel::setDraftSnapDistance(int distance)
{
    if (!canEdit()) {
        return false;
    }
    if (!WindowsValues::isValidSnapDistance(distance)) {
        m_snapDistanceError = QStringLiteral("Choose a snap distance between %1 and %2 pixels.")
                                  .arg(WindowsValues::MinimumSnapDistance)
                                  .arg(WindowsValues::MaximumSnapDistance);
        Q_EMIT viewChanged();
        return false;
    }
    m_snapDistanceError.clear();
    if (m_draft.snapDistance == distance) {
        Q_EMIT viewChanged();
        return true;
    }
    m_draft.snapDistance = distance;
    Q_EMIT viewChanged();
    return true;
}

QVariantList WindowsSettingsModel::focusPolicyChoices()
{
    return {QVariantMap{{QStringLiteral("token"), QStringLiteral("click")},
                        {QStringLiteral("label"), QStringLiteral("Click to focus")}},
            QVariantMap{{QStringLiteral("token"), QStringLiteral("focus-follows-mouse")},
                        {QStringLiteral("label"), QStringLiteral("Focus follows mouse")}},
            QVariantMap{{QStringLiteral("token"), QStringLiteral("focus-under-mouse")},
                        {QStringLiteral("label"), QStringLiteral("Focus under mouse")}}};
}

QVariantList WindowsSettingsModel::dockingModifierChoices()
{
    return {QVariantMap{{QStringLiteral("token"), QStringLiteral("super")},
                        {QStringLiteral("label"), QStringLiteral("Meta + Shift")}},
            QVariantMap{{QStringLiteral("token"), QStringLiteral("alt")},
                        {QStringLiteral("label"), QStringLiteral("Alt + Shift")}},
            QVariantMap{{QStringLiteral("token"), QStringLiteral("control")},
                        {QStringLiteral("label"), QStringLiteral("Ctrl + Shift")}},
            QVariantMap{{QStringLiteral("token"), QStringLiteral("disabled")},
                        {QStringLiteral("label"), QStringLiteral("Off (keyboard docking only)")}}};
}

QVariantList WindowsSettingsModel::closeContainerPolicyChoices()
{
    return {QVariantMap{{QStringLiteral("token"), QStringLiteral("ask")},
                        {QStringLiteral("label"), QStringLiteral("Ask every time")}},
            QVariantMap{{QStringLiteral("token"), QStringLiteral("close-all")},
                        {QStringLiteral("label"), QStringLiteral("Close every window")}},
            QVariantMap{{QStringLiteral("token"), QStringLiteral("ungroup")},
                        {QStringLiteral("label"), QStringLiteral("Ungroup and keep windows")}}};
}

bool WindowsSettingsModel::revertDraft()
{
    if (!canEdit() || !draftDirty()) {
        return false;
    }
    m_draft = m_confirmed;
    m_snapDistanceError.clear();
    m_confirmedError.clear();
    m_conflictIntent = false;
    m_state = State::Ready;
    m_transientError.clear();
    Q_EMIT viewChanged();
    return true;
}

bool WindowsSettingsModel::applyDraft()
{
    if (!applyAvailable()) {
        return false;
    }
    m_confirmedError.clear();
    m_snapDistanceError.clear();
    m_conflictIntent = false;
    m_waitingFinalSnapshot = false;
    m_queue.clear();
    const QStringList keys = WindowsKeys::scopedKeys();
    for (const QString &key : keys) {
        const QVariant intended = m_draft.value(key);
        if (!WindowsValues::sameValue(key, m_confirmed.value(key), intended)) {
            m_queue.append(CommitIntent{key, intended});
        }
    }
    m_sequenceActive = true;
    setState(State::Saving);
    writeNextQueuedKey();
    return m_sequenceActive;
}

void WindowsSettingsModel::retry()
{
    // Never claim progress before the client actually retries; a repeated
    // synchronous failure keeps the Unavailable truth visible.
    m_client.refresh();
}

bool WindowsSettingsModel::retrySessionApply()
{
    if (!applyRetryAvailable()) {
        return false;
    }
    Q_EMIT retrySessionApplyRequested();
    return true;
}

void WindowsSettingsModel::setSessionApplyStatus(SessionApplyStatus status)
{
    m_sessionApply = std::move(status);
    Q_EMIT viewChanged();
}

void WindowsSettingsModel::handleClientState()
{
    switch (m_client.state()) {
    case ClientState::Ready:
        // The snapshot handler promotes to Ready once fresh authority lands.
        break;
    case ClientState::Authenticating:
        // AGENT-GUARD: The client publishes Authenticating for startup,
        // replacement, and the routine refresh after every commit reply.
        // Dropping held intent here would abort every multi-key Apply at its
        // first key; handleSnapshot() verifies lineage when the fresh
        // authority lands and aborts explicitly if owner/epoch changed.
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
        abortSequence();
        setState(State::Unavailable, m_client.lastError());
        break;
    }
}

void WindowsSettingsModel::setConfirmed(const WindowsValues &values)
{
    const bool keepDraft = m_hasBaseline && m_draft != m_confirmed;
    m_confirmed = values;
    if (!keepDraft) {
        m_draft = values;
    }
    m_hasBaseline = true;
}

void WindowsSettingsModel::handleSnapshot()
{
    const auto &snapshot = m_client.snapshot();
    if (!snapshot) {
        return;
    }
    QString error;
    const auto decoded = WindowsValues::fromVariantMap(snapshot->values, &error);
    if (!decoded) {
        setAuthorityReady(false);
        abortSequence();
        setState(State::Unavailable,
                 error.isEmpty() ? QStringLiteral("Window settings have an invalid value.")
                                 : error.left(MaximumDiagnosticLength));
        return;
    }
    // AGENT-GUARD: Authority that changed owner/epoch between an applied key
    // and this snapshot is replacement authority. Writing the remaining
    // queued intent to it would push stale user intent behind the
    // replacement's back; abort, keep the draft, and require an explicit
    // re-Apply instead.
    const bool lineageChanged = m_hasBaseline
        && (m_confirmedOwner != snapshot->owner || m_confirmedEpoch != snapshot->epoch);
    m_confirmedOwner = snapshot->owner;
    m_confirmedEpoch = snapshot->epoch;
    setConfirmed(*decoded);
    setAuthorityReady(true);

    if (lineageChanged && (m_sequenceActive || m_waitingFinalSnapshot || m_conflictIntent)) {
        abortSequence();
        m_conflictIntent = false;
        setState(State::Ready,
                 QStringLiteral("Settings authority changed; the draft was not replayed."));
        return;
    }
    if (m_waitingFinalSnapshot) {
        m_waitingFinalSnapshot = false;
        if (*decoded == m_draft) {
            m_conflictIntent = false;
            setState(State::Ready);
        } else {
            // Another writer changed a scoped key after our last commit.
            m_conflictIntent = true;
            setState(State::Conflict);
        }
        return;
    }
    if (m_sequenceActive) {
        // AGENT-GUARD: The client refreshes authority after every commit and
        // refuses writes until it is Ready again. Issue the next queued key
        // only from this fresh baseline, or every second write would carry a
        // stale base revision and fail as Conflict.
        if (m_client.state() != ClientState::Ready) {
            Q_EMIT viewChanged();
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
            setState(State::Conflict);
        } else {
            Q_EMIT viewChanged();
        }
        return;
    }
    setState(State::Ready);
    Q_EMIT viewChanged();
}

void WindowsSettingsModel::handleCommit(const CommitOutcome &outcome)
{
    if (!m_sequenceActive || m_queue.isEmpty()) {
        return;
    }
    const CommitIntent intended = m_queue.first();
    const bool alreadyAuthority = outcome.status == SettingsWireStatus::Conflict
        && WindowsValues::sameValue(intended.key,
                                          outcome.currentValues.value(intended.key),
                                          intended.value);
    if (outcome.status == SettingsWireStatus::Applied || alreadyAuthority) {
        m_queue.removeFirst();
        // AGENT-GUARD: Never write the next key from the commit reply. The
        // client still holds the pre-commit base revision until its automatic
        // refresh lands; the fresh snapshot handler continues the sequence.
        if (m_queue.isEmpty()) {
            m_sequenceActive = false;
            m_waitingFinalSnapshot = true;
        }
        return;
    }
    if (outcome.status == SettingsWireStatus::Conflict) {
        abortSequence();
        m_conflictIntent = true;
        setState(State::Conflict, outcome.message.left(MaximumDiagnosticLength));
        return;
    }
    // Validation/read-only/persistence/unknown-key failures are confirmed
    // rejections: keep the diagnostic visible across rebaseline and never
    // resubmit. A new explicit Apply dismisses it.
    abortSequence();
    m_confirmedError = outcome.message.isEmpty()
        ? statusFailureMessage(outcome.status)
        : outcome.message.left(MaximumDiagnosticLength);
    setState(State::Ready);
    Q_EMIT viewChanged();
}

void WindowsSettingsModel::handleUncertain(const QString &message)
{
    // Timeout, owner replacement, or transport loss during a write is never
    // retried automatically; the user must refresh and re-apply explicitly.
    abortSequence();
    m_conflictIntent = false;
    m_confirmedError = QStringLiteral(
        "The window management change outcome is uncertain; the draft was not replayed.");
    if (!message.isEmpty()) {
        m_confirmedError += QStringLiteral(" ") + message.left(MaximumDiagnosticLength);
    }
    setState(State::Unavailable);
    Q_EMIT viewChanged();
}

void WindowsSettingsModel::setState(State state, QString transientError)
{
    // AGENT-GUARD: Transient errors are replaced on every state change while
    // m_confirmedError is only cleared by an explicit Apply or Revert, so a
    // confirmed rejection cannot be hidden by an automatic refresh.
    const QString nextError = std::move(transientError);
    if (m_state == state && m_transientError == nextError) {
        return;
    }
    m_state = state;
    m_transientError = nextError;
    Q_EMIT viewChanged();
}

void WindowsSettingsModel::setAuthorityReady(bool ready)
{
    if (m_authorityReady == ready) {
        return;
    }
    m_authorityReady = ready;
    // canEdit/applyAvailable depend on freshness as well as the visible
    // state, so controls cannot stay enabled across the refresh interval.
    Q_EMIT viewChanged();
}

void WindowsSettingsModel::writeNextQueuedKey()
{
    if (m_queue.isEmpty()) {
        m_sequenceActive = false;
        m_waitingFinalSnapshot = true;
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
    // Authority is refreshing; the matching fresh snapshot or the
    // state-change path continues or aborts the sequence.
}

void WindowsSettingsModel::abortSequence()
{
    m_queue.clear();
    m_sequenceActive = false;
    m_waitingFinalSnapshot = false;
}

} // namespace QindaQt::Apps::SettingsWindows
