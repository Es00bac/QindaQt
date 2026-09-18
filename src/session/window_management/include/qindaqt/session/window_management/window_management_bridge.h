// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/session/window_management/window_management_preferences.h"

#include <QObject>
#include <QString>
#include <QTimer>

#include <optional>

namespace QindaQt::Services::SettingsClient {
class SettingsClient;
}

namespace QindaQt::Session::WindowManagement {

class KWinReconfigureRequester;
class KWinWindowManagementWriter;

// The live bridge (ADR-0209): every confirmed Settings1 snapshot of the
// windowManagement scope is decoded totally, written into kwinrc when it
// differs from what the file holds, and followed by one KWin reconfigure
// (debounced, so a burst of edits costs one reload). Invalid snapshots are
// logged and ignored; the last good preferences stay in force.
//
// AGENT-CONTRACT: the client, writer and requester are borrowed and must
// outlive the bridge; all calls are on the owning thread.
class WindowManagementBridge final : public QObject {
    Q_OBJECT
public:
    WindowManagementBridge(Services::SettingsClient::SettingsClient &settings,
                           const KWinWindowManagementWriter &writer,
                           KWinReconfigureRequester &reconfigure, QObject *parent = nullptr);

    [[nodiscard]] const std::optional<WindowManagementPreferences> &lastApplied() const noexcept
    {
        return m_lastApplied;
    }
    [[nodiscard]] int appliedCount() const noexcept { return m_applied; }
    [[nodiscard]] int rejectedCount() const noexcept { return m_rejected; }
    [[nodiscard]] const QString &lastError() const noexcept { return m_lastError; }
    void setReconfigureDebounceMilliseconds(int milliseconds);

Q_SIGNALS:
    void applied(const QindaQt::Session::WindowManagement::WindowManagementPreferences &preferences,
                 bool kwinrcChanged);
    void rejected(const QString &error);

private:
    void applySnapshot();

    Services::SettingsClient::SettingsClient &m_settings;
    const KWinWindowManagementWriter &m_writer;
    KWinReconfigureRequester &m_reconfigure;
    QTimer m_reconfigureDebounce;
    std::optional<WindowManagementPreferences> m_lastApplied;
    QString m_lastError;
    int m_applied = 0;
    int m_rejected = 0;
};

} // namespace QindaQt::Session::WindowManagement
