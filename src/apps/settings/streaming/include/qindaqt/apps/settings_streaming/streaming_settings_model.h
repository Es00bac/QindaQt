// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/obs_client/obs_client.h>
#include <qindaqt/services/obs_client/obs_provisioning.h>
#include <qindaqt/services/obs_client/obs_secret_store.h>
#include <qindaqt/services/streaming_preferences/streaming_preferences.h>
#include <qindaqt/session_autostart/autostart_catalog.h>

#include <QObject>
#include <QVariantList>
#include <optional>

namespace QindaQt::Apps::SettingsStreaming {

using StreamingPreferences = Services::StreamingPreferences::StreamingPreferences;

// The Settings → Streaming route.
//
// AGENT-CONTRACT: This model owns presentation and sequencing only. OBS's
// live state comes from the client's snapshot, the password from the scoped
// keyring store, and the profile/scene/websocket files from the provisioning
// functions. It never writes an OBS file itself and never holds the password
// in a published property.
//
// AGENT-GUARD: Every action reports a truthful result. A button that cannot
// do its work says why instead of appearing to succeed — the wave rejected
// four candidates for exactly that class of untruth.
class StreamingSettingsModel final : public QObject {
    Q_OBJECT
    // Connection
    Q_PROPERTY(QString connectionState READ connectionState NOTIFY changed)
    Q_PROPERTY(QString connectionDescription READ connectionDescription NOTIFY changed)
    Q_PROPERTY(QString statusText READ statusText NOTIFY changed)
    Q_PROPERTY(bool connected READ connected NOTIFY changed)
    Q_PROPERTY(QString obsVersion READ obsVersion NOTIFY changed)
    Q_PROPERTY(int webSocketPort READ webSocketPort NOTIFY preferencesChanged)
    Q_PROPERTY(bool autoConnect READ autoConnect NOTIFY preferencesChanged)
    Q_PROPERTY(bool startObsAtLogin READ startObsAtLogin NOTIFY preferencesChanged)
    Q_PROPERTY(bool preferencesReady READ preferencesReady NOTIFY preferencesChanged)
    Q_PROPERTY(bool preferenceWritePending READ preferenceWritePending NOTIFY preferencesChanged)
    Q_PROPERTY(QString preferenceStatusText READ preferenceStatusText NOTIFY preferencesChanged)
    Q_PROPERTY(QString loginPolicyStatus READ loginPolicyStatus NOTIFY preferencesChanged)
    // The address is fixed; the route states it rather than offering a field
    // that would let a user point the password somewhere else.
    Q_PROPERTY(QString address READ address NOTIFY preferencesChanged)
    Q_PROPERTY(bool passwordStored READ passwordStored NOTIFY changed)
    Q_PROPERTY(QString keyringProblem READ keyringProblem NOTIFY changed)

    // Outputs
    Q_PROPERTY(bool recording READ recording NOTIFY changed)
    Q_PROPERTY(bool outputControlsAvailable READ outputControlsAvailable NOTIFY changed)
    Q_PROPERTY(bool streaming READ streaming NOTIFY changed)
    Q_PROPERTY(bool virtualCamera READ virtualCamera NOTIFY changed)
    Q_PROPERTY(QString recordingElapsed READ recordingElapsed NOTIFY changed)
    Q_PROPERTY(QString streamingElapsed READ streamingElapsed NOTIFY changed)
    Q_PROPERTY(QString droppedFramesWarning READ droppedFramesWarning NOTIFY changed)

    // Scenes
    Q_PROPERTY(QStringList sceneNames READ sceneNames NOTIFY changed)
    Q_PROPERTY(QString currentScene READ currentScene NOTIFY changed)

    // Defaults
    Q_PROPERTY(bool defaultsInstalled READ defaultsInstalled NOTIFY changed)
    Q_PROPERTY(QStringList defaultsProblems READ defaultsProblems NOTIFY changed)

    // Bus mapping (from the QindaQt OBS bridge's vendor request)
    Q_PROPERTY(bool bridgePresent READ bridgePresent NOTIFY changed)
    Q_PROPERTY(QVariantList busMapping READ busMapping NOTIFY changed)
    Q_PROPERTY(QString bridgeProblem READ bridgeProblem NOTIFY changed)

public:
    StreamingSettingsModel(Obs::ObsClient &client, Obs::ObsSecretStore &secrets,
                           StreamingPreferences &preferences,
                           QString obsConfigRoot, QObject *parent = nullptr,
                           std::optional<SessionAutostart::ScanOptions> loginScanOptions = std::nullopt);

    [[nodiscard]] QString connectionState() const;
    [[nodiscard]] QString connectionDescription() const;
    [[nodiscard]] bool outputControlsAvailable() const;
    [[nodiscard]] QString statusText() const { return m_statusText; }
    [[nodiscard]] bool connected() const;
    [[nodiscard]] QString obsVersion() const;
    [[nodiscard]] int webSocketPort() const;
    [[nodiscard]] bool autoConnect() const;
    [[nodiscard]] bool startObsAtLogin() const;
    [[nodiscard]] bool preferencesReady() const;
    [[nodiscard]] bool preferenceWritePending() const;
    [[nodiscard]] QString preferenceStatusText() const;
    [[nodiscard]] QString loginPolicyStatus() const;
    [[nodiscard]] QString address() const;
    [[nodiscard]] bool passwordStored() const { return m_passwordStored; }
    [[nodiscard]] QString keyringProblem() const { return m_keyringProblem; }
    [[nodiscard]] bool recording() const;
    [[nodiscard]] bool streaming() const;
    [[nodiscard]] bool virtualCamera() const;
    [[nodiscard]] QString recordingElapsed() const;
    [[nodiscard]] QString streamingElapsed() const;
    [[nodiscard]] QString droppedFramesWarning() const;
    [[nodiscard]] QStringList sceneNames() const;
    [[nodiscard]] QString currentScene() const;
    [[nodiscard]] bool defaultsInstalled() const { return m_defaults.complete(); }
    [[nodiscard]] QStringList defaultsProblems() const {
        return m_defaults.problems;
    }
    [[nodiscard]] bool bridgePresent() const;
    [[nodiscard]] QVariantList busMapping() const;
    [[nodiscard]] QString bridgeProblem() const;

    // Reads the keyring and the OBS files, then connects when the user asked
    // for auto-connect. Called when the route becomes visible.
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void connectToObs();
    Q_INVOKABLE void disconnectFromObs();
    Q_INVOKABLE void setRecording(bool active);
    Q_INVOKABLE void setStreaming(bool active);
    Q_INVOKABLE void setVirtualCamera(bool active);
    Q_INVOKABLE void selectScene(const QString &sceneName);
    Q_INVOKABLE void setWebSocketPort(int port);
    Q_INVOKABLE void setAutoConnect(bool enabled);
    Q_INVOKABLE void setStartObsAtLogin(bool enabled);
    // Writes the QindaQt profile, scene collection and websocket settings,
    // generating and storing a password when there is not one yet. Reports
    // what it did, or why it could not.
    Q_INVOKABLE void installDefaults();
    // Asks the bridge again for the console mapping.
    Q_INVOKABLE void refreshBusMapping();

    // Elapsed time as the route shows it; exported so a row can assert the
    // exact text rather than a format string.
    [[nodiscard]] static QString formatElapsed(qint64 milliseconds);

Q_SIGNALS:
    void changed();
    void preferencesChanged();

private:
    void setStatusText(const QString &text);
    void readKeyring();
    void readProvisioning();
    [[nodiscard]] bool admitOutputAction();
    void trackOutputRequest(quint64 requestId);
    [[nodiscard]] Obs::WebSocketSettings currentSettings() const;

    Obs::ObsClient &m_client;
    Obs::ObsSecretStore &m_secrets;
    StreamingPreferences &m_preferences;
    QString m_obsConfigRoot;
    SessionAutostart::ScanOptions m_loginScanOptions;
    QString m_statusText;
    QString m_keyringProblem;
    Obs::ProvisioningState m_defaults;
    bool m_passwordStored = false;
    bool m_refreshPending = false;
    int m_lastConfirmedPort = 4455;
    bool m_lastConfirmedAutoConnect = true;
    quint64 m_outputRequestId = 0;
};

} // namespace QindaQt::Apps::SettingsStreaming
