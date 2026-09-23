// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <qindaqt/services/streaming_preferences/streaming_preferences.h>
#include <qindaqt/services/settings_client/settings_client.h>

#include <QElapsedTimer>
#include <QTimer>
#include <QVariant>

namespace QindaQt::Services::StreamingPreferences {

// Borrows a purpose-scoped SettingsClient. No optimistic publication or
// automatic replay after timeout/owner loss; accepted writes await readback.
class Settings1StreamingPreferences final : public StreamingPreferences {
    Q_OBJECT
public:
    static const QStringList &scopedKeys();
    explicit Settings1StreamingPreferences(
        SettingsClient::SettingsClient &client, QObject *parent = nullptr);

    [[nodiscard]] bool isLoaded() const override;
    [[nodiscard]] int webSocketPort() const override { return m_port; }
    [[nodiscard]] bool autoConnect() const override { return m_autoConnect; }
    [[nodiscard]] bool startObsAtLogin() const override { return m_startAtLogin; }
    [[nodiscard]] bool writePending() const override { return !m_pendingKey.isEmpty(); }
    [[nodiscard]] QString writeStatusText() const override { return m_writeStatus; }
    bool setWebSocketPort(int port) override;
    bool setAutoConnect(bool enabled) override;
    bool setStartObsAtLogin(bool enabled) override;

private:
    bool request(const QString &key, const QVariant &value);
    void onSnapshotChanged();
    void onStateChanged();
    void onCommitFinished(const SettingsClient::CommitOutcome &outcome);
    void onCommitUncertain(const QString &message);
    void setWriteStatus(const QString &status);
    void clearPending();

    SettingsClient::SettingsClient &m_client;
    int m_port = 4455;
    bool m_autoConnect = true;
    bool m_startAtLogin = false;
    bool m_loaded = false;
    bool m_awaitingReadback = false;
    QString m_pendingKey;
    QVariant m_pendingValue;
    QString m_pendingOwner;
    QString m_pendingEpoch;
    quint64 m_pendingRevisionFloor = 0;
    QElapsedTimer m_readbackAge;
    QTimer m_readbackTimer;
    QString m_writeStatus;
};

} // namespace QindaQt::Services::StreamingPreferences
