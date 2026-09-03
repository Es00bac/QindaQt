// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/services/font_preferences/font_preferences.h"
#include "qindaqt/services/font_preferences/font_preferences_coordinator.h"

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

namespace QindaQt::Services::SettingsClient {
class SettingsClient;
struct CommitOutcome;
}

namespace QindaQt::Services::FontPreferences {

// AGENT-CONTRACT: FontSettingsBridge composes the pure FontPreferencesCoordinator
// with the public Settings1 client (ADR-0028 recovery contract). Confirmed
// snapshots update the coordinator atomically through
// FontPreferencesCodec::fromSettingsMap; a decode failure or any transport loss
// leaves the last-known-good snapshot untouched. applyPreferences() writes the
// six fonts.* keys as a fixed-order sequence of single-key optimistic commits,
// waits for a fresh authoritative snapshot between keys so no write carries a
// stale base revision, and never replays an uncertain write.
class FontSettingsBridge final : public QObject {
    Q_OBJECT
public:
    enum class KeyOutcome : quint8 {
        NotAttempted,
        Applied,
        Conflict,
        Failed,
        Uncertain,
    };

    struct KeyResult final {
        QString key;
        KeyOutcome outcome = KeyOutcome::NotAttempted;
        QString message;

        [[nodiscard]] bool operator==(const KeyResult &other) const noexcept
        {
            return key == other.key && outcome == other.outcome && message == other.message;
        }
    };

    // Both collaborators must outlive the bridge. The client must be scoped to
    // scopedKeys() so its snapshots decode into complete preferences.
    FontSettingsBridge(QindaQt::Services::SettingsClient::SettingsClient &client,
                       FontPreferencesCoordinator &coordinator,
                       QObject *parent = nullptr);

    // AGENT-CONTRACT: The six Settings1 schema-v2 keys, in the fixed commit
    // order used by applyPreferences().
    [[nodiscard]] static QStringList scopedKeys();

    [[nodiscard]] bool hasBaseline() const noexcept { return m_hasBaseline; }
    [[nodiscard]] bool applyInFlight() const noexcept { return m_sequenceActive; }
    [[nodiscard]] QString lastSyncError() const { return m_syncError; }
    [[nodiscard]] const QList<KeyResult> &lastApplyResults() const noexcept { return m_results; }

    // Starts the per-key commit sequence for the draft. Returns false without
    // changing anything when the draft is invalid, the client is not Ready, or
    // a sequence is already running.
    bool applyPreferences(const FontPreferences &draft, QString *error = nullptr);

Q_SIGNALS:
    void snapshotSynced();
    void applyFinished();

private:
    void handleSnapshot();
    void handleStateChanged();
    void handleCommitFinished(const QindaQt::Services::SettingsClient::CommitOutcome &outcome);
    void handleCommitUncertain(const QString &message);
    void advanceSequence();
    void abortSequence(KeyOutcome current, const QString &message);
    void abortRemainingNotAttempted();
    void finishSequence();

    QindaQt::Services::SettingsClient::SettingsClient &m_client;
    FontPreferencesCoordinator &m_coordinator;
    QList<KeyResult> m_results;
    QVariantMap m_draftValues;
    QString m_syncError;
    qsizetype m_sequenceIndex = 0;
    bool m_sequenceActive = false;
    bool m_waitingSnapshot = false;
    bool m_hasBaseline = false;
};

} // namespace QindaQt::Services::FontPreferences
