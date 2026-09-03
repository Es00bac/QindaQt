// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/font_preferences/font_settings_bridge.h"

#include "qindaqt/services/font_preferences/font_preferences_codec.h"
#include "qindaqt/services/settings_client/settings_client.h"

namespace QindaQt::Services::FontPreferences {

namespace {

using QindaQt::Services::SettingsProtocol::SettingsWireStatus;

void setError(QString *output, const QString &message)
{
    if (output != nullptr) {
        *output = message;
    }
}

} // namespace

FontSettingsBridge::FontSettingsBridge(QindaQt::Services::SettingsClient::SettingsClient &client,
                                       FontPreferencesCoordinator &coordinator,
                                       QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_coordinator(coordinator)
{
    connect(&m_client, &SettingsClient::SettingsClient::snapshotChanged,
            this, &FontSettingsBridge::handleSnapshot);
    connect(&m_client, &SettingsClient::SettingsClient::stateChanged,
            this, &FontSettingsBridge::handleStateChanged);
    connect(&m_client, &SettingsClient::SettingsClient::commitFinished,
            this, &FontSettingsBridge::handleCommitFinished);
    connect(&m_client, &SettingsClient::SettingsClient::commitUncertain,
            this, &FontSettingsBridge::handleCommitUncertain);
}

QStringList FontSettingsBridge::scopedKeys()
{
    return {QStringLiteral("fonts.family"),      QStringLiteral("fonts.monospaceFamily"),
            QStringLiteral("fonts.pointSize"),   QStringLiteral("fonts.antialiasing"),
            QStringLiteral("fonts.hinting"),     QStringLiteral("fonts.subpixelOrder")};
}

bool FontSettingsBridge::applyPreferences(const FontPreferences &draft, QString *error)
{
    if (!draft.isValid()) {
        setError(error, QStringLiteral("font preferences draft failed validation"));
        return false;
    }
    if (m_sequenceActive) {
        setError(error, QStringLiteral("a font preferences commit sequence is already running"));
        return false;
    }
    if (m_client.state() != SettingsClient::ClientState::Ready || !m_client.snapshot()
        || !m_hasBaseline) {
        // AGENT-GUARD: Writes are forbidden without a confirmed authoritative
        // baseline; an optimistic commit without one would carry no revision.
        setError(error, QStringLiteral("settings authority is not ready for font preferences"));
        return false;
    }

    m_draftValues = FontPreferencesCodec::toSettingsMap(draft);
    m_results.clear();
    for (const QString &key : scopedKeys()) {
        m_results.append(KeyResult{key, KeyOutcome::NotAttempted, QString()});
    }
    m_sequenceActive = true;
    m_waitingSnapshot = false;
    m_sequenceIndex = 0;

    const QString firstKey = m_results.first().key;
    QString writeError;
    if (!m_client.setUserValue(firstKey, m_draftValues.value(firstKey), &writeError)) {
        abortSequence(KeyOutcome::Failed, writeError.isEmpty()
                                              ? QStringLiteral("settings client refused the first write")
                                              : writeError);
    }
    return true;
}

void FontSettingsBridge::handleSnapshot()
{
    if (m_client.state() != SettingsClient::ClientState::Ready || !m_client.snapshot()) {
        return;
    }
    QString syncError;
    const bool synced = m_coordinator.updateFromSettings(m_client.snapshot()->values, &syncError);
    if (synced) {
        m_hasBaseline = true;
        m_syncError.clear();
        Q_EMIT snapshotSynced();
    } else {
        // AGENT-GUARD: A snapshot that fails validation never touches the
        // coordinator and cannot serve as a write baseline. The last-known-
        // good preferences stay authoritative, but writes remain closed until
        // a later complete valid snapshot arrives.
        m_hasBaseline = false;
        m_syncError = syncError;
    }

    if (m_sequenceActive && m_waitingSnapshot) {
        m_waitingSnapshot = false;
        if (!synced) {
            // AGENT-GUARD (review finding P1-5): A malformed post-commit
            // snapshot must not advance the write sequence -- the next write
            // would carry an unverifiable authority state. The just-written
            // key becomes Uncertain, every later key stays NotAttempted, and
            // the sequence ends unreplayed per the
            // Appearance no-replay rules (ADR-0028). Although the Settings1
            // reply said Applied, malformed fresh authority means this bridge
            // cannot verify the resulting domain snapshot; expose that key as
            // Uncertain rather than claiming a usable confirmed result.
            KeyResult &current = m_results[m_sequenceIndex];
            current.outcome = KeyOutcome::Uncertain;
            current.message = syncError.left(512);
            abortRemainingNotAttempted();
            finishSequence();
            return;
        }
        advanceSequence();
    }
}

void FontSettingsBridge::handleStateChanged()
{
    if (m_client.state() != SettingsClient::ClientState::Ready) {
        // AGENT-GUARD: hasBaseline() reports current write authority, not LKG
        // availability. Any non-Ready client state revokes that authority;
        // only a later complete domain-valid snapshot may restore it.
        m_hasBaseline = false;
    }
    if (!m_sequenceActive || !m_waitingSnapshot) {
        return;
    }
    if (m_client.state() == SettingsClient::ClientState::Unavailable) {
        // AGENT-GUARD: Transport loss between keys fails the sequence closed;
        // keys never written stay NotAttempted so a partial sequence can never
        // masquerade as fully applied. An in-flight key is owned by
        // commitUncertain instead.
        m_hasBaseline = false;
        abortRemainingNotAttempted();
        finishSequence();
    }
}

void FontSettingsBridge::handleCommitFinished(
    const QindaQt::Services::SettingsClient::CommitOutcome &outcome)
{
    if (!m_sequenceActive) {
        return;
    }
    KeyResult &current = m_results[m_sequenceIndex];
    if (outcome.status == SettingsWireStatus::Applied) {
        current.outcome = KeyOutcome::Applied;
        current.message = outcome.message;
        // Wait for the fresh authoritative snapshot so the next write carries
        // the advanced base revision instead of conflicting spuriously.
        m_waitingSnapshot = true;
        return;
    }
    abortSequence(outcome.status == SettingsWireStatus::Conflict ? KeyOutcome::Conflict
                                                                 : KeyOutcome::Failed,
                  outcome.message.isEmpty() ? QStringLiteral("settings commit was rejected")
                                            : outcome.message);
}

void FontSettingsBridge::handleCommitUncertain(const QString &message)
{
    if (!m_sequenceActive) {
        return;
    }
    // AGENT-GUARD: An uncertain write is never replayed automatically; the
    // sequence stops and the remaining keys stay NotAttempted.
    abortSequence(KeyOutcome::Uncertain, message);
}

void FontSettingsBridge::advanceSequence()
{
    ++m_sequenceIndex;
    if (m_sequenceIndex >= m_results.size()) {
        finishSequence();
        return;
    }
    if (m_client.state() != SettingsClient::ClientState::Ready) {
        abortRemainingNotAttempted();
        finishSequence();
        return;
    }
    const QString key = m_results[m_sequenceIndex].key;
    QString writeError;
    if (!m_client.setUserValue(key, m_draftValues.value(key), &writeError)) {
        abortSequence(KeyOutcome::Failed, writeError.isEmpty()
                                              ? QStringLiteral("settings client refused a write")
                                              : writeError);
    }
}

void FontSettingsBridge::abortSequence(KeyOutcome current, const QString &message)
{
    KeyResult &result = m_results[m_sequenceIndex];
    result.outcome = current;
    result.message = message.left(512);
    abortRemainingNotAttempted();
    finishSequence();
}

void FontSettingsBridge::abortRemainingNotAttempted()
{
    for (qsizetype i = m_sequenceIndex + 1; i < m_results.size(); ++i) {
        m_results[i].outcome = KeyOutcome::NotAttempted;
    }
}

void FontSettingsBridge::finishSequence()
{
    m_sequenceActive = false;
    m_waitingSnapshot = false;
    Q_EMIT applyFinished();
}

} // namespace QindaQt::Services::FontPreferences
