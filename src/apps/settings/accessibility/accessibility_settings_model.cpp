// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_accessibility/accessibility_settings_model.h>

#include <qindaqt/services/settings_client/settings_client.h>
#include <qindaqt/services/settings_protocol/settings_wire_status.h>

#include <utility>

namespace QindaQt::Apps::SettingsAccessibility {

using Services::SettingsClient::ClientState;
using Services::SettingsClient::CommitOutcome;
using Services::SettingsClient::SettingsClient;
using Services::SettingsProtocol::SettingsWireStatus;

namespace {
constexpr qsizetype MaximumDiagnosticLength = 512;

[[nodiscard]] QString statusFailureMessage(SettingsWireStatus status)
{
    return QStringLiteral("Settings1 rejected the accessibility change: %1")
        .arg(Services::SettingsProtocol::settingsWireStatusName(status));
}
} // namespace

AccessibilitySettingsModel::AccessibilitySettingsModel(SettingsClient &client,
                                                       QObject *parent)
    : QObject(parent)
    , m_client(client)
{
    Q_ASSERT(m_client.thread() == thread());
    connect(&m_client, &SettingsClient::stateChanged,
            this, &AccessibilitySettingsModel::handleClientState);
    connect(&m_client, &SettingsClient::snapshotChanged,
            this, &AccessibilitySettingsModel::handleSnapshot);
    connect(&m_client, &SettingsClient::commitFinished,
            this, &AccessibilitySettingsModel::handleCommit);
    connect(&m_client, &SettingsClient::commitUncertain,
            this, &AccessibilitySettingsModel::handleUncertain);
}

bool AccessibilitySettingsModel::canEdit() const noexcept
{
    // A resolved conflict has a fresh authoritative baseline, so the draft
    // stays editable for revise/re-Apply or Revert. Saving, Unavailable, and
    // the commit-reply-to-snapshot gap remain fail-closed.
    return m_hasBaseline && (ready() || conflict()) && m_authorityReady;
}

bool AccessibilitySettingsModel::draftDirty() const noexcept
{
    return m_hasBaseline && m_draft != m_confirmed;
}

bool AccessibilitySettingsModel::applyAvailable() const noexcept
{
    return canEdit() && draftDirty();
}

QString AccessibilitySettingsModel::statusText() const
{
    switch (m_state) {
    case State::Loading:
        return QStringLiteral("Loading accessibility settings…");
    case State::Ready:
        return draftDirty() ? QStringLiteral("Changes not yet applied.") : QString{};
    case State::Saving:
        return QStringLiteral("Applying accessibility settings…");
    case State::Conflict:
        return QStringLiteral("Changed elsewhere. Current values were reloaded; apply again to keep your changes.");
    case State::Unavailable:
        return m_hasBaseline
            ? QStringLiteral("Settings1 is unavailable; the last confirmed values are shown.")
            : QStringLiteral("Accessibility settings are unavailable.");
    }
    return {};
}

QString AccessibilitySettingsModel::errorText() const
{
    // A confirmed rejection or uncertain outcome outranks the client's
    // transient authority diagnostic so the "not replayed" truth stays
    // visible while the authority state churns; Apply/Revert clear it.
    return m_confirmedError.isEmpty() ? m_transientError : m_confirmedError;
}

bool AccessibilitySettingsModel::setDraftFlag(bool AccessibilityValues::*field,
                                              bool enabled)
{
    if (!canEdit()) {
        return false;
    }
    if (m_draft.*field == enabled) {
        return true;
    }
    m_draft.*field = enabled;
    Q_EMIT viewChanged();
    return true;
}

bool AccessibilitySettingsModel::setDraftHighContrast(bool enabled)
{
    return setDraftFlag(&AccessibilityValues::highContrast, enabled);
}

bool AccessibilitySettingsModel::setDraftReducedMotion(bool enabled)
{
    return setDraftFlag(&AccessibilityValues::reducedMotion, enabled);
}

bool AccessibilitySettingsModel::setDraftReducedTransparency(bool enabled)
{
    return setDraftFlag(&AccessibilityValues::reducedTransparency, enabled);
}

bool AccessibilitySettingsModel::setDraftTextScale(double scale)
{
    if (!canEdit()) {
        return false;
    }
    if (!AccessibilityValues::isValidTextScale(scale)) {
        m_textScaleError = QStringLiteral("Choose a text scale between %1× and %2×.")
                               .arg(AccessibilityValues::MinimumTextScale)
                               .arg(AccessibilityValues::MaximumTextScale);
        Q_EMIT viewChanged();
        return false;
    }
    m_textScaleError.clear();
    if (qFuzzyCompare(m_draft.textScale, scale)) {
        Q_EMIT viewChanged();
        return true;
    }
    m_draft.textScale = scale;
    Q_EMIT viewChanged();
    return true;
}

bool AccessibilitySettingsModel::revertDraft()
{
    if (!canEdit() || !draftDirty()) {
        return false;
    }
    m_draft = m_confirmed;
    m_textScaleError.clear();
    m_confirmedError.clear();
    m_conflictIntent = false;
    m_state = State::Ready;
    m_transientError.clear();
    Q_EMIT viewChanged();
    return true;
}

bool AccessibilitySettingsModel::applyDraft()
{
    if (!applyAvailable()) {
        return false;
    }
    m_confirmedError.clear();
    m_textScaleError.clear();
    m_conflictIntent = false;
    m_waitingFinalSnapshot = false;
    m_queue.clear();
    const QStringList keys = AccessibilityKeys::scopedKeys();
    for (const QString &key : keys) {
        const QVariant intended = m_draft.value(key);
        if (!AccessibilityValues::sameValue(key, m_confirmed.value(key), intended)) {
            m_queue.append(CommitIntent{key, intended});
        }
    }
    m_sequenceActive = true;
    setState(State::Saving);
    writeNextQueuedKey();
    return m_sequenceActive;
}

void AccessibilitySettingsModel::retry()
{
    // Never claim progress before the client actually retries; a repeated
    // synchronous failure keeps the Unavailable truth visible.
    m_client.refresh();
}

void AccessibilitySettingsModel::handleClientState()
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

void AccessibilitySettingsModel::setConfirmed(const AccessibilityValues &values)
{
    const bool keepDraft = m_hasBaseline && m_draft != m_confirmed;
    m_confirmed = values;
    if (!keepDraft) {
        m_draft = values;
    }
    m_hasBaseline = true;
}

void AccessibilitySettingsModel::handleSnapshot()
{
    const auto &snapshot = m_client.snapshot();
    if (!snapshot) {
        return;
    }
    QString error;
    const auto decoded = AccessibilityValues::fromVariantMap(snapshot->values, &error);
    if (!decoded) {
        setAuthorityReady(false);
        abortSequence();
        setState(State::Unavailable,
                 error.isEmpty() ? QStringLiteral("Accessibility settings have an invalid value.")
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

void AccessibilitySettingsModel::handleCommit(const CommitOutcome &outcome)
{
    if (!m_sequenceActive || m_queue.isEmpty()) {
        return;
    }
    const CommitIntent intended = m_queue.first();
    const bool alreadyAuthority = outcome.status == SettingsWireStatus::Conflict
        && AccessibilityValues::sameValue(intended.key,
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

void AccessibilitySettingsModel::handleUncertain(const QString &message)
{
    // Timeout, owner replacement, or transport loss during a write is never
    // retried automatically; the user must refresh and re-apply explicitly.
    abortSequence();
    m_conflictIntent = false;
    m_confirmedError = QStringLiteral(
        "The accessibility change outcome is uncertain; the draft was not replayed.");
    if (!message.isEmpty()) {
        m_confirmedError += QStringLiteral(" ") + message.left(MaximumDiagnosticLength);
    }
    setState(State::Unavailable);
    Q_EMIT viewChanged();
}

void AccessibilitySettingsModel::setState(State state, QString transientError)
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

void AccessibilitySettingsModel::setAuthorityReady(bool ready)
{
    if (m_authorityReady == ready) {
        return;
    }
    m_authorityReady = ready;
    // canEdit/applyAvailable depend on freshness as well as the visible
    // state, so controls cannot stay enabled across the refresh interval.
    Q_EMIT viewChanged();
}

void AccessibilitySettingsModel::writeNextQueuedKey()
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

void AccessibilitySettingsModel::abortSequence()
{
    m_queue.clear();
    m_sequenceActive = false;
    m_waitingFinalSnapshot = false;
}

} // namespace QindaQt::Apps::SettingsAccessibility
