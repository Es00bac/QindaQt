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
    connect(&m_reconfigureDebounce, &QTimer::timeout, this, [this] {
        if (!m_pendingPreferences.has_value()) {
            return;
        }
        setApplyState(WindowManagementApplyPhase::Applying);
        if (m_nextRequestId == 0) {
            failApply(QStringLiteral("The KWin reconfigure request counter was exhausted."));
            return;
        }
        m_pendingRequestId = m_nextRequestId++;
        m_reconfigure.requestReconfigure(m_pendingRequestId);
    });
    connect(&m_settings, &Services::SettingsClient::SettingsClient::snapshotChanged, this,
            &WindowManagementBridge::applySnapshot);
    connect(&m_settings, &Services::SettingsClient::SettingsClient::stateChanged, this,
            &WindowManagementBridge::handleSettingsState);
    connect(&m_settings, &Services::SettingsClient::SettingsClient::ownerChanged, this,
            &WindowManagementBridge::handleSettingsState);
    connect(&m_reconfigure, &KWinReconfigureRequester::ownerChanged, this,
            &WindowManagementBridge::handleKWinOwnerChanged);
    connect(&m_reconfigure, &KWinReconfigureRequester::reconfigureFinished, this,
            &WindowManagementBridge::handleReconfigureFinished);
    applySnapshot();
}

void WindowManagementBridge::setReconfigureDebounceMilliseconds(int milliseconds)
{
    m_reconfigureDebounce.setInterval(milliseconds < 0 ? 0 : milliseconds);
}

void WindowManagementBridge::retry()
{
    if (m_settings.state() != Services::SettingsClient::ClientState::Ready
        || !m_settings.snapshot().has_value()) {
        handleSettingsState();
        return;
    }
    applySnapshot();
}

void WindowManagementBridge::applySnapshot()
{
    const auto &snapshot = m_settings.snapshot();
    if (!snapshot.has_value()) {
        return;
    }

    QString error;
    const auto preferences = WindowManagementPreferences::fromVariantMap(snapshot->values, &error);
    if (!preferences.has_value()) {
        failApply(error);
        return;
    }
    m_settingsOwner = snapshot->owner;
    m_settingsEpoch = snapshot->epoch;
    m_settingsRevision = snapshot->revision;
    m_lastRequested = *preferences;
    m_applyState.preferences = *preferences;
    m_applyState.settingsOwner = m_settingsOwner;
    m_applyState.settingsEpoch = m_settingsEpoch;
    m_applyState.settingsRevision = m_settingsRevision;

    if (m_pendingPreferences == preferences) {
        if (m_pendingSettingsOwner != m_settingsOwner
            || m_pendingSettingsEpoch != m_settingsEpoch) {
            m_reconfigureDebounce.stop();
            m_pendingPreferences.reset();
            m_pendingRequestId = 0;
        } else {
            // An unrelated Settings1 key can advance the global revision while
            // this same window preference is in flight. Fence completion to
            // the newest revision without issuing duplicate compositor work.
            m_pendingSettingsRevision = m_settingsRevision;
            m_applyState.settingsOwner = m_settingsOwner;
            m_applyState.settingsEpoch = m_settingsEpoch;
            m_applyState.settingsRevision = m_settingsRevision;
            setApplyState(WindowManagementApplyPhase::Applying);
            return;
        }
    } else if (m_pendingPreferences.has_value()) {
        m_reconfigureDebounce.stop();
        m_pendingPreferences.reset();
        m_pendingRequestId = 0;
    }
    if (m_lastApplied == preferences) {
        m_applyState.preferences = *preferences;
        m_applyState.settingsOwner = m_settingsOwner;
        m_applyState.settingsEpoch = m_settingsEpoch;
        m_applyState.settingsRevision = m_settingsRevision;
        m_applyState.kwinOwner = m_appliedKWinOwner;
        setApplyState(WindowManagementApplyPhase::Applied);
        return;
    }

    // The prior owner acknowledged an older preference. Do not carry that
    // acknowledgement into a write/readback failure for the new snapshot.
    m_applyState.kwinOwner.clear();
    const KWinWriteOutcome outcome = m_writer.write(*preferences);
    if (!outcome.ok) {
        failApply(outcome.error);
        return;
    }
    const KWinReadbackOutcome readback = m_writer.readback(*preferences);
    if (!readback.matches) {
        failApply(readback.error);
        return;
    }

    m_pendingPreferences = *preferences;
    m_pendingSettingsOwner = m_settingsOwner;
    m_pendingSettingsEpoch = m_settingsEpoch;
    m_pendingSettingsRevision = m_settingsRevision;
    m_pendingRequestId = 0;
    m_pendingKWinrcChanged = outcome.changed;
    m_applyState.preferences = *preferences;
    m_applyState.settingsOwner = m_settingsOwner;
    m_applyState.settingsEpoch = m_settingsEpoch;
    m_applyState.settingsRevision = m_settingsRevision;
    m_applyState.kwinOwner.clear();
    setApplyState(WindowManagementApplyPhase::Applying);
    // AGENT-GUARD: Initial startup must get an acknowledged reload even when
    // kwinrc already matched; a file readback alone cannot prove this KWin
    // owner consumed the saved values.
    m_reconfigureDebounce.start();
}

void WindowManagementBridge::handleSettingsState()
{
    if (m_settings.state() == Services::SettingsClient::ClientState::Ready) {
        return;
    }
    m_reconfigureDebounce.stop();
    m_pendingPreferences.reset();
    m_pendingRequestId = 0;
    setApplyState(WindowManagementApplyPhase::Unavailable,
                  m_settings.lastError().isEmpty()
                      ? QStringLiteral("Settings1 is unavailable in this session.")
                      : m_settings.lastError());
}

void WindowManagementBridge::handleKWinOwnerChanged(const QString &newOwner)
{
    if (newOwner.isEmpty()) {
        m_lastApplied.reset();
        m_appliedKWinOwner.clear();
        m_applyState.kwinOwner.clear();
        m_reconfigureDebounce.stop();
        m_pendingPreferences.reset();
        m_pendingRequestId = 0;
        setApplyState(WindowManagementApplyPhase::Unavailable,
                      QStringLiteral("KWin is not running in this session; saved settings will "
                                     "apply when it returns."));
        return;
    }

    // AGENT-GUARD: A replacement compositor has not consumed the old owner's
    // configuration, even if kwinrc still contains the same values.
    m_lastApplied.reset();
    m_appliedKWinOwner.clear();
    m_reconfigureDebounce.stop();
    m_pendingPreferences.reset();
    m_pendingRequestId = 0;
    if (m_settings.snapshot().has_value()
        && m_settings.state() == Services::SettingsClient::ClientState::Ready) {
        applySnapshot();
    }
}

void WindowManagementBridge::handleReconfigureFinished(quint64 requestId,
                                                       const QString &owner,
                                                       const QString &errorMessage)
{
    if (requestId == 0 || requestId != m_pendingRequestId
        || !m_pendingPreferences.has_value()) {
        return;
    }
    m_pendingRequestId = 0;
    if (!errorMessage.isEmpty()) {
        failApply(errorMessage);
        return;
    }
    if (owner.isEmpty()) {
        failApply(QStringLiteral("KWin did not identify the compositor that reloaded the "
                                 "saved settings."));
        return;
    }
    if (m_settings.state() != Services::SettingsClient::ClientState::Ready
        || m_settings.currentOwner() != m_pendingSettingsOwner
        || !m_settings.snapshot().has_value()
        || m_settings.snapshot()->epoch != m_pendingSettingsEpoch
        || m_settings.snapshot()->revision != m_pendingSettingsRevision) {
        failApply(QStringLiteral("Settings authority changed before KWin finished reloading. "
                                 "Retry after the current settings snapshot is available."));
        return;
    }
    const KWinReadbackOutcome readback = m_writer.readback(*m_pendingPreferences);
    if (!readback.matches) {
        failApply(readback.error);
        return;
    }

    m_lastApplied = *m_pendingPreferences;
    m_appliedKWinOwner = owner;
    m_pendingPreferences.reset();
    m_lastError.clear();
    ++m_applied;
    m_applyState.kwinOwner = owner;
    m_applyState.preferences = m_lastApplied;
    m_applyState.settingsOwner = m_pendingSettingsOwner;
    m_applyState.settingsEpoch = m_pendingSettingsEpoch;
    m_applyState.settingsRevision = m_pendingSettingsRevision;
    setApplyState(WindowManagementApplyPhase::Applied);
    Q_EMIT applied(*m_lastApplied, m_pendingKWinrcChanged);
}

void WindowManagementBridge::setApplyState(WindowManagementApplyPhase phase, QString error)
{
    m_applyState.phase = phase;
    m_applyState.error = error.left(512);
    if (phase == WindowManagementApplyPhase::Failed
        || phase == WindowManagementApplyPhase::Unavailable) {
        m_lastError = m_applyState.error;
    } else {
        m_lastError.clear();
    }
    Q_EMIT applyStateChanged(m_applyState);
}

void WindowManagementBridge::failApply(const QString &error)
{
    ++m_rejected;
    m_pendingRequestId = 0;
    m_pendingPreferences.reset();
    m_reconfigureDebounce.stop();
    m_lastError = error.left(512);
    qWarning().noquote() << "QindaQt session could not apply windowManagement preferences:"
                         << m_lastError;
    setApplyState(WindowManagementApplyPhase::Failed, m_lastError);
    Q_EMIT rejected(m_lastError);
}

} // namespace QindaQt::Session::WindowManagement
