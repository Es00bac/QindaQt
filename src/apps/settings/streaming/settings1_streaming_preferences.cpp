// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_streaming/settings1_streaming_preferences.h>

#include <QVariant>

namespace QindaQt::Apps::SettingsStreaming {
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
    onSnapshotChanged();
}

void Settings1StreamingPreferences::onSnapshotChanged() {
    const auto &snapshot = m_client.snapshot();
    if (!snapshot.has_value()) {
        return;
    }
    const QVariant port = snapshot->values.value(QLatin1String(kPortKey));
    const QVariant autoConnect =
        snapshot->values.value(QLatin1String(kAutoConnectKey));
    const QVariant startAtLogin =
        snapshot->values.value(QLatin1String(kStartAtLoginKey));
    const int nextPort = port.canConvert<int>() && port.toInt() > 0 &&
                                 port.toInt() <= 65535
                             ? port.toInt()
                             : 4455;
    const bool nextAutoConnect =
        autoConnect.typeId() == QMetaType::Bool ? autoConnect.toBool() : true;
    const bool nextStartAtLogin =
        startAtLogin.typeId() == QMetaType::Bool ? startAtLogin.toBool() : false;
    const bool wasLoaded = m_loaded;
    m_loaded = true;
    if (wasLoaded && nextPort == m_port && nextAutoConnect == m_autoConnect &&
        nextStartAtLogin == m_startAtLogin) {
        return;
    }
    m_port = nextPort;
    m_autoConnect = nextAutoConnect;
    m_startAtLogin = nextStartAtLogin;
    Q_EMIT preferencesChanged();
}

bool Settings1StreamingPreferences::setWebSocketPort(int port) {
    if (port <= 0 || port > 65535) {
        return false;
    }
    m_port = port;
    Q_EMIT preferencesChanged();
    QString error;
    return m_client.setUserValue(QLatin1String(kPortKey), port, &error);
}

bool Settings1StreamingPreferences::setAutoConnect(bool enabled) {
    m_autoConnect = enabled;
    Q_EMIT preferencesChanged();
    QString error;
    return m_client.setUserValue(QLatin1String(kAutoConnectKey), enabled, &error);
}

bool Settings1StreamingPreferences::setStartObsAtLogin(bool enabled) {
    m_startAtLogin = enabled;
    Q_EMIT preferencesChanged();
    QString error;
    return m_client.setUserValue(QLatin1String(kStartAtLoginKey), enabled,
                                 &error);
}

} // namespace QindaQt::Apps::SettingsStreaming
