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

enum class WindowManagementApplyPhase { Unavailable, Applying, Applied, Failed };

struct WindowManagementApplyState final {
    WindowManagementApplyPhase phase = WindowManagementApplyPhase::Unavailable;
    std::optional<WindowManagementPreferences> preferences;
    QString settingsOwner;
    QString settingsEpoch;
    quint64 settingsRevision = 0;
    QString kwinOwner;
    QString error;
};

// The live bridge (ADR-0209): every confirmed Settings1 snapshot of the
// windowManagement scope is decoded totally, written into kwinrc when it
// differs from what the file holds, and followed by one KWin reconfigure
// (debounced, so a burst of edits costs one reload). Invalid confirmed
// snapshots stay visible as failed applies; they are never reported as applied.
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
    [[nodiscard]] const WindowManagementApplyState &applyState() const noexcept
    {
        return m_applyState;
    }
    [[nodiscard]] int appliedCount() const noexcept { return m_applied; }
    [[nodiscard]] int rejectedCount() const noexcept { return m_rejected; }
    [[nodiscard]] const QString &lastError() const noexcept { return m_lastError; }
    void setReconfigureDebounceMilliseconds(int milliseconds);
    void retry();

Q_SIGNALS:
    void applied(const QindaQt::Session::WindowManagement::WindowManagementPreferences &preferences,
                 bool kwinrcChanged);
    void rejected(const QString &error);
    void applyStateChanged(
        const QindaQt::Session::WindowManagement::WindowManagementApplyState &state);

private:
    void applySnapshot();
    void handleSettingsState();
    void handleKWinOwnerChanged(const QString &newOwner);
    void handleReconfigureFinished(quint64 requestId, const QString &owner,
                                   const QString &errorMessage);
    void setApplyState(WindowManagementApplyPhase phase, QString error = {});
    void failApply(const QString &error);

    Services::SettingsClient::SettingsClient &m_settings;
    const KWinWindowManagementWriter &m_writer;
    KWinReconfigureRequester &m_reconfigure;
    QTimer m_reconfigureDebounce;
    std::optional<WindowManagementPreferences> m_lastApplied;
    std::optional<WindowManagementPreferences> m_lastRequested;
    std::optional<WindowManagementPreferences> m_pendingPreferences;
    WindowManagementApplyState m_applyState;
    QString m_lastError;
    QString m_appliedKWinOwner;
    QString m_settingsOwner;
    QString m_settingsEpoch;
    QString m_pendingSettingsOwner;
    QString m_pendingSettingsEpoch;
    quint64 m_settingsRevision = 0;
    quint64 m_pendingSettingsRevision = 0;
    quint64 m_pendingRequestId = 0;
    quint64 m_nextRequestId = 1;
    bool m_pendingKWinrcChanged = false;
    int m_applied = 0;
    int m_rejected = 0;
};

} // namespace QindaQt::Session::WindowManagement
