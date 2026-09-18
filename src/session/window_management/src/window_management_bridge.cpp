// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/session/window_management/window_management_bridge.h"

#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/session/window_management/kwin_reconfigure_requester.h"
#include "qindaqt/session/window_management/kwin_window_management_writer.h"

#include <QDebug>

namespace QindaQt::Session::WindowManagement {

WindowManagementBridge::WindowManagementBridge(
    Services::SettingsClient::SettingsClient &settings, const KWinWindowManagementWriter &writer,
    KWinReconfigureRequester &reconfigure, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_writer(writer)
    , m_reconfigure(reconfigure)
{
    m_reconfigureDebounce.setSingleShot(true);
    m_reconfigureDebounce.setInterval(200);
    connect(&m_reconfigureDebounce, &QTimer::timeout, this,
            [this] { m_reconfigure.requestReconfigure(); });
    connect(&m_settings, &Services::SettingsClient::SettingsClient::snapshotChanged, this,
            &WindowManagementBridge::applySnapshot);
    applySnapshot();
}

void WindowManagementBridge::setReconfigureDebounceMilliseconds(int milliseconds)
{
    m_reconfigureDebounce.setInterval(milliseconds < 0 ? 0 : milliseconds);
}

void WindowManagementBridge::applySnapshot()
{
    // AGENT-GUARD: a snapshot observed while our own commit is in flight is
    // not relevant here (the bridge never writes Settings1), but a partial
    // snapshot is: the total decode rejects it and keeps the last good state.
    const auto &snapshot = m_settings.snapshot();
    if (!snapshot.has_value()) {
        return;
    }
    QString error;
    const auto preferences = WindowManagementPreferences::fromVariantMap(snapshot->values, &error);
    if (!preferences.has_value()) {
        ++m_rejected;
        m_lastError = error;
        qWarning().noquote() << "QindaQt session ignored a windowManagement snapshot:" << error;
        Q_EMIT rejected(error);
        return;
    }
    if (m_lastApplied.has_value() && *m_lastApplied == *preferences) {
        return;
    }
    const KWinWriteOutcome outcome = m_writer.write(*preferences);
    if (!outcome.ok) {
        ++m_rejected;
        m_lastError = outcome.error;
        qWarning().noquote() << "QindaQt session could not apply windowManagement preferences:"
                             << outcome.error;
        Q_EMIT rejected(outcome.error);
        return;
    }
    m_lastApplied = *preferences;
    m_lastError.clear();
    ++m_applied;
    if (outcome.changed) {
        m_reconfigureDebounce.start();
    }
    Q_EMIT applied(*preferences, outcome.changed);
}

} // namespace QindaQt::Session::WindowManagement
