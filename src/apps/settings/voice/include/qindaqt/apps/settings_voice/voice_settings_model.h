// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/voice_protocol/voice_settings_keys.h>
#include <qindaqt/services/voice_protocol/voice_types.h>

#include <QtCore/QObject>
#include <QtCore/QVariantList>

namespace QindaQt::Services::Voice {
class VoiceClient;
}
namespace QindaQt::Services::SettingsClient {
class SettingsClient;
struct CommitOutcome;
}

namespace QindaQt::Apps::SettingsVoice {

// The desktop's own voice preferences; the provider owns everything else.
// Defined once in the protocol module, which the shell applet also reads.
inline constexpr const char *VoiceInputSettingsKey =
    Services::Voice::kVoiceInputSettingsKey;
inline constexpr const char *VoicePanelTranscriptSettingsKey =
    Services::Voice::kVoicePanelTranscriptSettingsKey;

// AGENT-CONTRACT: this same-thread projection borrows one public Settings1
// client and one public Voice1 client; both must outlive it. QML receives
// bounded labels, a provider list and one intent per control. Raw provider bus
// names, credentials and unbounded transcripts never cross this boundary.
//
// AGENT-GUARD: the two Settings1 keys are drafted together and committed one
// key at a time, because Settings1 has no multi-key transaction. A commit that
// fails partway leaves the successful key applied and reports the failure; it
// must never silently roll the other key back, because the user's own next
// action is the only thing that can decide what they meant.
class VoiceSettingsModel final : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool preferenceLoading READ preferenceLoading NOTIFY viewChanged)
    Q_PROPERTY(bool preferenceReady READ preferenceReady NOTIFY viewChanged)
    Q_PROPERTY(bool preferenceSaving READ preferenceSaving NOTIFY viewChanged)
    Q_PROPERTY(bool preferenceUnavailable READ preferenceUnavailable NOTIFY viewChanged)
    Q_PROPERTY(bool canEditPreference READ canEditPreference NOTIFY viewChanged)
    Q_PROPERTY(bool voiceInputEnabled READ voiceInputEnabled NOTIFY viewChanged)
    Q_PROPERTY(bool draftVoiceInputEnabled READ draftVoiceInputEnabled NOTIFY viewChanged)
    Q_PROPERTY(bool panelTranscriptEnabled READ panelTranscriptEnabled NOTIFY viewChanged)
    Q_PROPERTY(bool draftPanelTranscriptEnabled READ draftPanelTranscriptEnabled
                   NOTIFY viewChanged)
    Q_PROPERTY(bool preferenceDirty READ preferenceDirty NOTIFY viewChanged)
    Q_PROPERTY(bool applyAvailable READ applyAvailable NOTIFY viewChanged)
    Q_PROPERTY(QString preferenceStatusText READ preferenceStatusText NOTIFY viewChanged)
    Q_PROPERTY(QString preferenceErrorText READ preferenceErrorText NOTIFY viewChanged)

    Q_PROPERTY(QString serviceState READ serviceState NOTIFY viewChanged)
    Q_PROPERTY(QString serviceStatusText READ serviceStatusText NOTIFY viewChanged)
    Q_PROPERTY(bool serviceAvailable READ serviceAvailable NOTIFY viewChanged)
    Q_PROPERTY(QString sessionStateText READ sessionStateText NOTIFY viewChanged)
    Q_PROPERTY(bool capturing READ capturing NOTIFY viewChanged)
    Q_PROPERTY(bool shortcutsArmed READ shortcutsArmed NOTIFY viewChanged)
    Q_PROPERTY(QString providerId READ providerId NOTIFY viewChanged)
    Q_PROPERTY(QString providerLabel READ providerLabel NOTIFY viewChanged)
    Q_PROPERTY(QString languageLabel READ languageLabel NOTIFY viewChanged)
    Q_PROPERTY(QString microphoneLabel READ microphoneLabel NOTIFY viewChanged)
    Q_PROPERTY(QString dictationShortcut READ dictationShortcut NOTIFY viewChanged)
    Q_PROPERTY(QString commandShortcut READ commandShortcut NOTIFY viewChanged)
    Q_PROPERTY(QString routeLabel READ routeLabel NOTIFY viewChanged)
    Q_PROPERTY(QString lastText READ lastText NOTIFY viewChanged)
    Q_PROPERTY(QVariantList providerRows READ providerRows NOTIFY viewChanged)
    Q_PROPERTY(QVariantList capabilityRows READ capabilityRows NOTIFY viewChanged)
    Q_PROPERTY(bool providerBusy READ providerBusy NOTIFY viewChanged)
    Q_PROPERTY(QString providerErrorText READ providerErrorText NOTIFY viewChanged)
    Q_PROPERTY(bool canStartDictation READ canStartDictation NOTIFY viewChanged)
    Q_PROPERTY(bool canCancelDictation READ canCancelDictation NOTIFY viewChanged)
    Q_PROPERTY(bool canChooseProvider READ canChooseProvider NOTIFY viewChanged)

public:
    enum class PreferenceState { Loading, Ready, Saving, Unavailable };

    VoiceSettingsModel(Services::SettingsClient::SettingsClient &settingsClient,
                       Services::Voice::VoiceClient &voiceClient,
                       QObject *parent = nullptr);

    [[nodiscard]] bool preferenceLoading() const noexcept;
    [[nodiscard]] bool preferenceReady() const noexcept;
    [[nodiscard]] bool preferenceSaving() const noexcept;
    [[nodiscard]] bool preferenceUnavailable() const noexcept;
    [[nodiscard]] bool canEditPreference() const noexcept;
    [[nodiscard]] bool voiceInputEnabled() const noexcept { return m_voiceInput; }
    [[nodiscard]] bool draftVoiceInputEnabled() const noexcept { return m_draftVoiceInput; }
    [[nodiscard]] bool panelTranscriptEnabled() const noexcept { return m_panelTranscript; }
    [[nodiscard]] bool draftPanelTranscriptEnabled() const noexcept
    {
        return m_draftPanelTranscript;
    }
    [[nodiscard]] bool preferenceDirty() const noexcept;
    [[nodiscard]] bool applyAvailable() const noexcept;
    [[nodiscard]] QString preferenceStatusText() const;
    [[nodiscard]] QString preferenceErrorText() const { return m_preferenceError; }

    [[nodiscard]] QString serviceState() const;
    [[nodiscard]] QString serviceStatusText() const;
    [[nodiscard]] bool serviceAvailable() const noexcept { return m_serviceReady; }
    [[nodiscard]] QString sessionStateText() const;
    [[nodiscard]] bool capturing() const noexcept;
    [[nodiscard]] bool shortcutsArmed() const noexcept { return m_snapshot.enabled; }
    [[nodiscard]] QString providerId() const { return m_snapshot.providerId; }
    [[nodiscard]] QString providerLabel() const;
    [[nodiscard]] QString languageLabel() const { return m_snapshot.languageCode; }
    [[nodiscard]] QString microphoneLabel() const { return m_snapshot.microphoneLabel; }
    [[nodiscard]] QString dictationShortcut() const { return m_snapshot.dictationShortcut; }
    [[nodiscard]] QString commandShortcut() const { return m_snapshot.commandShortcut; }
    [[nodiscard]] QString routeLabel() const;
    [[nodiscard]] QString lastText() const { return m_snapshot.lastText; }
    [[nodiscard]] QVariantList providerRows() const;
    [[nodiscard]] QVariantList capabilityRows() const;
    [[nodiscard]] bool providerBusy() const noexcept { return m_voiceRequestId != 0; }
    [[nodiscard]] QString providerErrorText() const { return m_voiceError; }
    [[nodiscard]] bool canStartDictation() const noexcept;
    [[nodiscard]] bool canCancelDictation() const noexcept;
    [[nodiscard]] bool canChooseProvider() const noexcept;

    Q_INVOKABLE bool setDraftVoiceInputEnabled(bool enabled);
    Q_INVOKABLE bool setDraftPanelTranscriptEnabled(bool enabled);
    Q_INVOKABLE bool cancelPreferenceDraft();
    Q_INVOKABLE bool applyPreferences();
    Q_INVOKABLE void retryPreference();
    Q_INVOKABLE void retryProvider();

    Q_INVOKABLE bool setShortcutsArmed(bool armed);
    Q_INVOKABLE bool selectProvider(const QString &providerId);
    // A "does my microphone work" control. It opens the provider's normal
    // dictation session, so whatever window has focus receives the text — the
    // Settings window itself, if the user left it focused.
    Q_INVOKABLE bool startDictation();
    Q_INVOKABLE bool cancelDictation();

Q_SIGNALS:
    void viewChanged();

private:
    void handleSettingsState();
    void handleSettingsSnapshot();
    void handleSettingsCommit(const Services::SettingsClient::CommitOutcome &outcome);
    void handleSettingsUncertain(const QString &message);
    void setPreferenceState(PreferenceState state, QString error = {});
    [[nodiscard]] bool commitNextDraftKey();

    void handleVoiceState();
    void handleVoiceSnapshot(const Services::Voice::Snapshot &snapshot);
    void handleVoiceResult(quint64 requestId,
                           const Services::Voice::OperationResult &result);
    [[nodiscard]] bool submit(Services::Voice::OperationKind kind,
                              const QString &providerId, bool enable);

    Services::SettingsClient::SettingsClient &m_settingsClient;
    Services::Voice::VoiceClient &m_voiceClient;

    PreferenceState m_preferenceState = PreferenceState::Loading;
    QString m_preferenceError;
    QString m_settingsOwner;
    QString m_settingsEpoch;
    bool m_voiceInput = false;
    bool m_panelTranscript = true;
    bool m_draftVoiceInput = false;
    bool m_draftPanelTranscript = true;
    bool m_hasPreferenceBaseline = false;
    // Which drafted key the in-flight commit is carrying, so the reply can be
    // attributed and the next key started.
    bool m_committingVoiceInput = false;
    bool m_committingPanelTranscript = false;

    Services::Voice::Snapshot m_snapshot;
    QString m_voiceOwner;
    QString m_voiceError;
    quint64 m_voiceRequestId = 0;
    Services::Voice::OperationKind m_voiceRequestKind =
        Services::Voice::OperationKind::StartDictation;
    bool m_serviceReady = false;
};

} // namespace QindaQt::Apps::SettingsVoice
