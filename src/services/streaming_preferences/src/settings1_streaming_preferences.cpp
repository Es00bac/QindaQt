// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/streaming_preferences/settings1_streaming_preferences.h>

#include <QVariant>

namespace QindaQt::Services::StreamingPreferences {
namespace {
constexpr auto kPortKey = "services.obsWebSocketPort";
constexpr auto kAutoConnectKey = "services.obsAutoConnect";
constexpr auto kStartAtLoginKey = "services.obsStartAtLogin";
} // namespace

const QStringList &Settings1StreamingPreferences::scopedKeys() {
    static const QStringList keys{QLatin1String(kPortKey),
                                  QLatin1String(kAutoConnectKey),
                                  QLatin1String(kStartAtLoginKey)};
    return keys;
}

Settings1StreamingPreferences::Settings1StreamingPreferences(
    Services::SettingsClient::SettingsClient &client, QObject *parent)
    : StreamingPreferences(parent), m_client(client) {
    connect(&m_client,
            &Services::SettingsClient::SettingsClient::snapshotChanged, this,
            &Settings1StreamingPreferences::onSnapshotChanged);
    connect(&m_client, &SettingsClient::SettingsClient::stateChanged, this,
            &Settings1StreamingPreferences::onStateChanged);
    connect(&m_client, &SettingsClient::SettingsClient::commitFinished, this,
            &Settings1StreamingPreferences::onCommitFinished);
    connect(&m_client, &SettingsClient::SettingsClient::commitUncertain, this,
            &Settings1StreamingPreferences::onCommitUncertain);
    onSnapshotChanged();
}

bool Settings1StreamingPreferences::isLoaded() const {
    const auto &snapshot = m_client.snapshot();
    // A commit's readback briefly puts SettingsClient in Authenticating for
    // the SAME owner. The last confirmed values remain safe to consume there;
    // owner loss/replacement or degradation must invalidate them.
    return m_loaded && m_client.state() != SettingsClient::ClientState::Unavailable
           && m_client.state() != SettingsClient::ClientState::Degraded
           && snapshot && !m_client.currentOwner().isEmpty()
           && snapshot->owner == m_client.currentOwner();
}

void Settings1StreamingPreferences::setWriteStatus(const QString &status) {
    if (m_writeStatus == status) return;
    m_writeStatus = status;
    Q_EMIT writeStatusChanged();
}

void Settings1StreamingPreferences::onStateChanged() {
    const bool loaded = isLoaded();
    if (!loaded && m_awaitingReadback) {
        // An applied commit is not a confirmed preference until its snapshot
        // arrives. A failed readback must release the write slot without replay.
        m_awaitingReadback = false;
        m_pendingKey.clear();
        m_pendingValue.clear();
        setWriteStatus(QStringLiteral("The change was accepted but its saved value was not confirmed. Check the current value before retrying."));
    }
    if (!loaded && m_loaded) {
        m_loaded = false;
        Q_EMIT preferencesChanged();
    }
}

void Settings1StreamingPreferences::onSnapshotChanged() {
    const auto &snapshot = m_client.snapshot();
    if (!snapshot || snapshot->owner != m_client.currentOwner()
        || m_client.state() != SettingsClient::ClientState::Ready) return;
    const QVariant port = snapshot->values.value(QLatin1String(kPortKey));
    const QVariant autoConnect = snapshot->values.value(QLatin1String(kAutoConnectKey));
    const QVariant startAtLogin = snapshot->values.value(QLatin1String(kStartAtLoginKey));
    const int nextPort = port.canConvert<int>() && port.toInt() > 0 && port.toInt() <= 65535
                             ? port.toInt() : 4455;
    const bool nextAutoConnect = autoConnect.typeId() == QMetaType::Bool
                                     ? autoConnect.toBool() : true;
    const bool nextStartAtLogin = startAtLogin.typeId() == QMetaType::Bool
                                      ? startAtLogin.toBool() : false;
    const bool changed = !m_loaded || m_port != nextPort || m_autoConnect != nextAutoConnect
                         || m_startAtLogin != nextStartAtLogin;
    m_loaded = true;
    m_port = nextPort;
    m_autoConnect = nextAutoConnect;
    m_startAtLogin = nextStartAtLogin;
    if (changed) Q_EMIT preferencesChanged();
    if (m_awaitingReadback) {
        const bool matched = snapshot->values.value(m_pendingKey) == m_pendingValue;
        m_awaitingReadback = false;
        m_pendingKey.clear();
        m_pendingValue.clear();
        setWriteStatus(matched ? QStringLiteral("Saved.")
                               : QStringLiteral("The saved setting did not match the request. Check its current value."));
    }
}

void Settings1StreamingPreferences::onCommitFinished(const SettingsClient::CommitOutcome &outcome) {
    if (m_pendingKey.isEmpty()) return;
    if (outcome.status == SettingsProtocol::SettingsWireStatus::Applied) {
        m_awaitingReadback = true;
        setWriteStatus(QStringLiteral("Checking the saved setting…"));
        return;
    }
    m_pendingKey.clear();
    m_pendingValue.clear();
    setWriteStatus(outcome.message.isEmpty() ? QStringLiteral("Settings rejected the change.")
                                             : outcome.message);
}

void Settings1StreamingPreferences::onCommitUncertain(const QString &message) {
    if (m_pendingKey.isEmpty()) return;
    m_pendingKey.clear();
    m_pendingValue.clear();
    m_awaitingReadback = false;
    setWriteStatus(message.isEmpty() ? QStringLiteral("The change was not confirmed. Check the current value before retrying.")
                                      : QStringLiteral("The change was not confirmed: %1").arg(message));
}

bool Settings1StreamingPreferences::request(const QString &key, const QVariant &value) {
    if (!isLoaded() || !m_pendingKey.isEmpty()) {
        setWriteStatus(QStringLiteral("Settings are unavailable or another change is pending."));
        return false;
    }
    QString error;
    if (!m_client.setUserValue(key, value, &error)) {
        setWriteStatus(error.isEmpty() ? QStringLiteral("Settings could not accept the change.") : error);
        return false;
    }
    m_pendingKey = key;
    m_pendingValue = value;
    m_awaitingReadback = false;
    setWriteStatus(QStringLiteral("Saving setting…"));
    return true;
}

bool Settings1StreamingPreferences::setWebSocketPort(int port) {
    if (port <= 0 || port > 65535) {
        setWriteStatus(QStringLiteral("The port must be between 1 and 65535."));
        return false;
    }
    return request(QLatin1String(kPortKey), port);
}

bool Settings1StreamingPreferences::setAutoConnect(bool enabled) {
    return request(QLatin1String(kAutoConnectKey), enabled);
}

bool Settings1StreamingPreferences::setStartObsAtLogin(bool enabled) {
    return request(QLatin1String(kStartAtLoginKey), enabled);
}

} // namespace QindaQt::Services::StreamingPreferences
