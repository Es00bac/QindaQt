// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_streaming/streaming_settings_model.h>

#include <QVariantMap>
#include <QFileInfo>
#include <QDir>

namespace QindaQt::Apps::SettingsStreaming {

using namespace QindaQt::Obs;

namespace {

// obs-websocket has no transport security, so the address is not a
// preference (see StreamingPreferences).
constexpr auto LoopbackHost = "127.0.0.1";
// A stream that has dropped this much is worth warning about; below it the
// number is noise the user cannot act on.
constexpr double DroppedFrameWarningFraction = 0.01;

QString sourceKindLabel(const QString &sourceKind) {
    if (sourceKind == QLatin1String("qindaqt_console_bus")) {
        return QObject::tr("Bus");
    }
    if (sourceKind == QLatin1String("qindaqt_console_strip")) {
        return QObject::tr("Strip");
    }
    return QObject::tr("Unknown");
}

QVariantMap mappingRow(const ConsoleSourceMapping &mapping) {
    return QVariantMap{
        {QStringLiteral("consoleId"), mapping.consoleId},
        {QStringLiteral("code"), mapping.code},
        {QStringLiteral("label"), mapping.label},
        {QStringLiteral("sourceName"), mapping.sourceName},
        {QStringLiteral("kind"), sourceKindLabel(mapping.sourceKind)},
        {QStringLiteral("captureDevice"), mapping.captureDevice},
        {QStringLiteral("muted"), mapping.muted},
        {QStringLiteral("gainDb"), mapping.gainDb},
    };
}

} // namespace

StreamingSettingsModel::StreamingSettingsModel(ObsClient &client,
                                               ObsSecretStore &secrets,
                                               StreamingPreferences &preferences,
                                               QString obsConfigRoot,
                                               QObject *parent,
                                               std::optional<SessionAutostart::ScanOptions> loginScanOptions)
    : QObject(parent), m_client(client), m_secrets(secrets),
      m_preferences(preferences), m_obsConfigRoot(std::move(obsConfigRoot)),
      m_loginScanOptions(loginScanOptions.value_or(SessionAutostart::ScanOptions::fromEnvironment())) {
    connect(&m_client, &ObsClient::snapshotChanged, this,
            &StreamingSettingsModel::changed);
    connect(&m_client, &ObsClient::stateChanged, this,
            [this](ConnectionState, const QString &) { Q_EMIT changed(); });
    connect(&m_client, &ObsClient::operationFinished, this,
            [this](const ObsClient::OperationResult &result) {
                if (m_outputRequestId == result.requestId) {
                    m_outputRequestId = 0;
                    Q_EMIT changed();
                }
                if (result.ok) {
                    setStatusText(QString());
                    return;
                }
                if (result.reasonCode == QLatin1String("obs-timeout")
                    || result.reasonCode == QLatin1String("obs-connection-lost")
                    || result.reasonCode == QLatin1String("obs-connection-replaced")) {
                    setStatusText(tr("OBS did not confirm the change. Check OBS before trying again."));
                    return;
                }
                setStatusText(result.comment.isEmpty()
                                  ? tr("OBS refused %1 (%2).")
                                        .arg(result.requestType, result.reasonCode)
                                  : tr("OBS refused %1: %2")
                                        .arg(result.requestType, result.comment));
            });
    m_lastConfirmedPort = m_preferences.webSocketPort();
    m_lastConfirmedAutoConnect = m_preferences.autoConnect();
    connect(&m_preferences, &StreamingPreferences::preferencesChanged, this,
            [this] {
                const bool portChanged = m_lastConfirmedPort != m_preferences.webSocketPort();
                const bool autoEnabled = !m_lastConfirmedAutoConnect && m_preferences.autoConnect();
                const bool autoDisabled = m_lastConfirmedAutoConnect && !m_preferences.autoConnect();
                m_lastConfirmedPort = m_preferences.webSocketPort();
                m_lastConfirmedAutoConnect = m_preferences.autoConnect();
                Q_EMIT preferencesChanged();
                if (!m_preferences.isLoaded()) {
                    m_client.stop();
                    return;
                }
                if (m_refreshPending) {
                    m_refreshPending = false;
                    refresh();
                } else if (autoDisabled) {
                    m_client.stop();
                } else if ((portChanged && m_client.state() != ConnectionState::Disconnected) || autoEnabled) {
                    // A confirmed selected-port change replaces the connection;
                    // OBS may still require the documented setup/restart step.
                    connectToObs();
                }
                readProvisioning();
                Q_EMIT changed();
            });
    connect(&m_preferences, &StreamingPreferences::writeStatusChanged, this,
            &StreamingSettingsModel::preferencesChanged);
}

QString StreamingSettingsModel::connectionState() const {
    return connectionStateName(m_client.state());
}

QString StreamingSettingsModel::connectionDescription() const {
    const QString reason = m_client.reasonCode();
    if (reason == QLatin1String(ReasonCodes::AuthRejected)
        || reason == QLatin1String(ReasonCodes::AuthRequired)) {
        return tr("OBS did not accept QindaQt's password. Repair OBS setup and restart OBS.");
    }
    if (reason == QLatin1String(ReasonCodes::RpcVersion)) {
        return tr("OBS uses a control protocol this version of QindaQt does not support.");
    }
    if (reason == QLatin1String(ReasonCodes::Malformed)) {
        return tr("OBS sent an unreadable control message. The connection was closed.");
    }
    if (connected()) {
        return tr("Connected to OBS %1 at %2.").arg(obsVersion(), address());
    }
    if (m_client.state() == ConnectionState::Authenticating
        || (m_client.state() == ConnectionState::Connecting && reason.isEmpty())) {
        return tr("Connecting to OBS at %1…").arg(address());
    }
    return tr("OBS is not running or its control server is unavailable at %1.").arg(address());
}

bool StreamingSettingsModel::outputControlsAvailable() const {
    return connected() && m_outputRequestId == 0;
}

bool StreamingSettingsModel::admitOutputAction() {
    if (!outputControlsAvailable()) {
        setStatusText(connected() ? tr("Waiting for OBS to finish that change.")
                                  : tr("Not connected to OBS."));
        return false;
    }
    return true;
}

void StreamingSettingsModel::trackOutputRequest(const quint64 requestId) {
    if (requestId == 0) {
        setStatusText(tr("OBS is busy. Try again in a moment."));
        return;
    }
    m_outputRequestId = requestId;
    setStatusText(tr("Waiting for OBS…"));
    Q_EMIT changed();
}

bool StreamingSettingsModel::connected() const {
    return m_client.state() == ConnectionState::Ready;
}

QString StreamingSettingsModel::obsVersion() const {
    return m_client.snapshot().obsVersion;
}

int StreamingSettingsModel::webSocketPort() const {
    return m_preferences.webSocketPort();
}

bool StreamingSettingsModel::autoConnect() const {
    return m_preferences.autoConnect();
}

bool StreamingSettingsModel::startObsAtLogin() const {
    return m_preferences.startObsAtLogin();
}

bool StreamingSettingsModel::preferencesReady() const {
    return m_preferences.isLoaded();
}

bool StreamingSettingsModel::preferenceWritePending() const {
    return m_preferences.writePending();
}

QString StreamingSettingsModel::preferenceStatusText() const {
    return m_preferences.writeStatusText();
}

QString StreamingSettingsModel::loginPolicyStatus() const {
    if (!m_preferences.isLoaded())
        return tr("Waiting for confirmed login settings.");
    if (!m_preferences.startObsAtLogin())
        return tr("OBS will not start at login.");
    const auto entries = SessionAutostart::scan(m_loginScanOptions);
    for (const auto &entry : entries) {
        if (entry.id != QLatin1String("qindaqt-obs-login")) continue;
        if (!entry.eligible)
            return tr("Startup blocks OBS login: %1").arg(entry.ineligibilityReason);
        for (const QString &directory : m_loginScanOptions.executableDirectories) {
            const QFileInfo obs(QDir(directory).filePath(QStringLiteral("obs")));
            if (obs.isFile() && obs.isExecutable())
                return tr("OBS is set to start at the next login.");
        }
        return tr("OBS is not installed in the session executable path.");
    }
    return tr("The OBS login entry is not installed or is masked by Startup.");
}

QString StreamingSettingsModel::address() const {
    return QStringLiteral("ws://%1:%2")
        .arg(QLatin1String(LoopbackHost))
        .arg(m_preferences.webSocketPort());
}

bool StreamingSettingsModel::recording() const {
    return m_client.snapshot().record.active;
}

bool StreamingSettingsModel::streaming() const {
    return m_client.snapshot().stream.active;
}

bool StreamingSettingsModel::virtualCamera() const {
    return m_client.snapshot().virtualCam.active;
}

QString StreamingSettingsModel::formatElapsed(qint64 milliseconds) {
    // AGENT-CONTRACT: a dash, not 00:00:00, when OBS reported no duration.
    // A zero clock reads as "it just started", which is a different claim.
    if (milliseconds < 0) {
        return QStringLiteral("—");
    }
    const qint64 totalSeconds = milliseconds / 1000;
    return QStringLiteral("%1:%2:%3")
        .arg(totalSeconds / 3600, 2, 10, QLatin1Char('0'))
        .arg((totalSeconds / 60) % 60, 2, 10, QLatin1Char('0'))
        .arg(totalSeconds % 60, 2, 10, QLatin1Char('0'));
}

QString StreamingSettingsModel::recordingElapsed() const {
    return recording() ? formatElapsed(m_client.snapshot().record.durationMs)
                       : QStringLiteral("—");
}

QString StreamingSettingsModel::streamingElapsed() const {
    return streaming() ? formatElapsed(m_client.snapshot().stream.durationMs)
                       : QStringLiteral("—");
}

QString StreamingSettingsModel::droppedFramesWarning() const {
    const OutputStatus &stream = m_client.snapshot().stream;
    // AGENT-GUARD: never present a warning the numbers do not support. OBS
    // reports no frame counts for a stream it is not running.
    if (!stream.active || !stream.hasFrameCounts()) {
        return {};
    }
    const double fraction = stream.droppedFraction();
    if (fraction < DroppedFrameWarningFraction) {
        return {};
    }
    return tr("%1% of frames dropped (%2 of %3). The connection is not "
              "keeping up.")
        .arg(fraction * 100.0, 0, 'f', 1)
        .arg(stream.skippedFrames)
        .arg(stream.totalFrames);
}

QStringList StreamingSettingsModel::sceneNames() const {
    return m_client.snapshot().scenes.names;
}

QString StreamingSettingsModel::currentScene() const {
    return m_client.snapshot().scenes.currentProgramScene;
}

bool StreamingSettingsModel::bridgePresent() const {
    return m_client.snapshot().consoleMapping.present();
}

QVariantList StreamingSettingsModel::busMapping() const {
    QVariantList rows;
    const ConsoleMapping &mapping = m_client.snapshot().consoleMapping;
    for (const ConsoleSourceMapping &bus : mapping.buses) {
        rows.append(mappingRow(bus));
    }
    for (const ConsoleSourceMapping &strip : mapping.strips) {
        rows.append(mappingRow(strip));
    }
    return rows;
}

QString StreamingSettingsModel::bridgeProblem() const {
    if (!connected()) {
        return tr("Connect to OBS to see which console buses it can record.");
    }
    const ConsoleMapping &mapping = m_client.snapshot().consoleMapping;
    if (!mapping.present()) {
        // AGENT-CONTRACT: "no plugin" and "no buses" are different sentences,
        // because the user's next action is different.
        return tr("The QindaQt bridge plugin is not loaded in OBS, so the "
                  "console buses are not available as sources.");
    }
    if (mapping.size() == 0 && (mapping.audioState.isEmpty()
                               || mapping.audioState == QLatin1String("ready"))) {
        return tr("The bridge is loaded and the console has no buses or "
                  "strips yet.");
    }
    if (mapping.audioState != QLatin1String("ready")) {
        return tr("The bridge cannot reach the audio console (%1).")
            .arg(mapping.audioState.isEmpty() ? mapping.reasonCode
                                              : mapping.audioState);
    }
    return {};
}

void StreamingSettingsModel::setStatusText(const QString &text) {
    if (m_statusText == text) {
        return;
    }
    m_statusText = text;
    Q_EMIT changed();
}

WebSocketSettings StreamingSettingsModel::currentSettings() const {
    WebSocketSettings settings;
    settings.serverPort = m_preferences.webSocketPort();
    return settings;
}

void StreamingSettingsModel::readKeyring() {
    QString error;
    const auto password = m_secrets.password(&error);
    m_passwordStored = password.has_value();
    m_keyringProblem = error;
}

void StreamingSettingsModel::readProvisioning() {
    m_defaults = inspectProvisioning(m_obsConfigRoot, currentSettings());
}

void StreamingSettingsModel::refresh() {
    if (!m_preferences.isLoaded()) {
        m_refreshPending = true;
        Q_EMIT preferencesChanged();
        return;
    }
    readKeyring();
    readProvisioning();
    Q_EMIT preferencesChanged();
    Q_EMIT changed();
    if (m_preferences.autoConnect()) {
        connectToObs();
    }
}

void StreamingSettingsModel::connectToObs() {
    if (!m_preferences.isLoaded()) {
        setStatusText(tr("Waiting for confirmed Streaming settings before connecting."));
        return;
    }
    QString error;
    const auto password = m_secrets.password(&error);
    if (!error.isEmpty()) {
        // AGENT-GUARD: a keyring that could not be read is not the same as
        // "there is no password". Connecting with an empty one would make
        // OBS's refusal look like a wrong password the user chose.
        setStatusText(tr("The password could not be read from the keyring: %1")
                          .arg(error));
        return;
    }
    if (!password.has_value()) {
        setStatusText(tr("No obs-websocket password is stored yet. Use "
                         "\"Set up OBS\" to create one."));
        return;
    }
    setStatusText(QString());
    m_client.start(address(), *password);
}

void StreamingSettingsModel::disconnectFromObs() {
    m_client.stop();
    setStatusText(QString());
}

void StreamingSettingsModel::setRecording(bool active) {
    if (admitOutputAction())
        trackOutputRequest(m_client.setOutputActive(OutputKind::Record, active));
}

void StreamingSettingsModel::setStreaming(bool active) {
    if (admitOutputAction())
        trackOutputRequest(m_client.setOutputActive(OutputKind::Stream, active));
}

void StreamingSettingsModel::setVirtualCamera(bool active) {
    if (admitOutputAction())
        trackOutputRequest(m_client.setOutputActive(OutputKind::VirtualCam, active));
}

void StreamingSettingsModel::selectScene(const QString &sceneName) {
    if (sceneName == currentScene() || sceneName.isEmpty())
        return;
    if (admitOutputAction())
        trackOutputRequest(m_client.setCurrentProgramScene(sceneName));
}

void StreamingSettingsModel::setWebSocketPort(int port) {
    if (port <= 0 || port > 65535 || port == m_preferences.webSocketPort()) {
        return;
    }
    if (!m_preferences.setWebSocketPort(port)) {
        setStatusText(tr("The port could not be saved."));
        return;
    }
    readProvisioning();
    Q_EMIT changed();
}

void StreamingSettingsModel::setAutoConnect(bool enabled) {
    if (enabled == m_preferences.autoConnect()) {
        return;
    }
    if (!m_preferences.setAutoConnect(enabled)) {
        setStatusText(tr("That preference could not be saved."));
    }
}

void StreamingSettingsModel::setStartObsAtLogin(bool enabled) {
    if (enabled == m_preferences.startObsAtLogin()) {
        return;
    }
    if (!m_preferences.setStartObsAtLogin(enabled)) {
        setStatusText(tr("That preference could not be saved."));
    }
}

void StreamingSettingsModel::installDefaults() {
    if (!m_preferences.isLoaded()) {
        setStatusText(tr("Waiting for confirmed Streaming settings before setting up OBS."));
        return;
    }
    QString keyringError;
    auto password = m_secrets.password(&keyringError);
    if (!keyringError.isEmpty()) {
        setStatusText(tr("The keyring is not available, so no password could "
                         "be stored: %1")
                          .arg(keyringError));
        return;
    }
    if (!password.has_value()) {
        const QString generated = generateWebSocketPassword();
        QString storeError;
        if (!m_secrets.setPassword(generated, &storeError)) {
            setStatusText(tr("The password could not be stored: %1")
                              .arg(storeError));
            return;
        }
        password = generated;
    }
    WebSocketSettings settings = currentSettings();
    settings.password = *password;
    QString error;
    if (!installProvisioning(m_obsConfigRoot, settings, &error)) {
        setStatusText(tr("OBS's configuration could not be written: %1")
                          .arg(error));
        return;
    }
    readKeyring();
    readProvisioning();
    // AGENT-CONTRACT: OBS reads its configuration once, at start. Saying so
    // is the difference between a working button and a user wondering why
    // nothing happened.
    setStatusText(tr("OBS is set up. Restart OBS if it is already running."));
    Q_EMIT changed();
}

void StreamingSettingsModel::refreshBusMapping() {
    if (m_client.refreshConsoleMapping() == 0) {
        setStatusText(tr("Not connected to OBS."));
    }
}

} // namespace QindaQt::Apps::SettingsStreaming
