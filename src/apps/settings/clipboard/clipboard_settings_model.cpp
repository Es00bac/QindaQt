// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_clipboard/clipboard_settings_model.h>

#include <qindaqt/services/clipboard_client/clipboard_client.h>
#include <qindaqt/services/settings_client/settings_client.h>
#include <qindaqt/services/settings_protocol/settings_wire_status.h>

#include <QtCore/QMetaType>

#include <utility>

namespace QindaQt::Apps::SettingsClipboard {
using Services::SettingsClient::ClientState;
using Services::SettingsClient::CommitOutcome;
using Services::SettingsProtocol::SettingsWireStatus;

namespace {
constexpr qsizetype MaximumDiagnosticLength = 512;
}

ClipboardSettingsModel::ClipboardSettingsModel(
    Services::SettingsClient::SettingsClient &settingsClient,
    Services::Clipboard::ClipboardClient &clipboardClient, QObject *parent)
    : QObject(parent)
    , m_settingsClient(settingsClient)
    , m_clipboardClient(clipboardClient)
{
    connect(&m_settingsClient, &Services::SettingsClient::SettingsClient::stateChanged,
            this, &ClipboardSettingsModel::handleSettingsState);
    connect(&m_settingsClient, &Services::SettingsClient::SettingsClient::snapshotChanged,
            this, &ClipboardSettingsModel::handleSettingsSnapshot);
    connect(&m_settingsClient, &Services::SettingsClient::SettingsClient::commitFinished,
            this, &ClipboardSettingsModel::handleSettingsCommit);
    connect(&m_settingsClient, &Services::SettingsClient::SettingsClient::commitUncertain,
            this, &ClipboardSettingsModel::handleSettingsUncertain);
    connect(&m_clipboardClient, &Services::Clipboard::ClipboardClient::stateChanged,
            this, [this] { handleClipboardState(); });
    connect(&m_clipboardClient, &Services::Clipboard::ClipboardClient::snapshotChanged,
            this, &ClipboardSettingsModel::handleClipboardSnapshot);
    connect(&m_clipboardClient, &Services::Clipboard::ClipboardClient::operationCompleted,
            this, &ClipboardSettingsModel::handleClearResult);
}

bool ClipboardSettingsModel::preferenceLoading() const noexcept
{
    return m_preferenceState == PreferenceState::Loading;
}

bool ClipboardSettingsModel::preferenceReady() const noexcept
{
    return m_preferenceState == PreferenceState::Ready;
}

bool ClipboardSettingsModel::preferenceSaving() const noexcept
{
    return m_preferenceState == PreferenceState::Saving;
}

bool ClipboardSettingsModel::preferenceConflict() const noexcept
{
    return m_preferenceState == PreferenceState::Conflict;
}

bool ClipboardSettingsModel::preferenceUnavailable() const noexcept
{
    return m_preferenceState == PreferenceState::Unavailable;
}

bool ClipboardSettingsModel::canEditPreference() const noexcept
{
    return m_hasPreferenceBaseline && m_settingsClient.state() == ClientState::Ready
        && !preferenceSaving();
}

bool ClipboardSettingsModel::preferenceDirty() const noexcept
{
    return m_preferenceDirty;
}

bool ClipboardSettingsModel::applyAvailable() const noexcept
{
    return canEditPreference() && m_preferenceDirty;
}

QString ClipboardSettingsModel::preferenceErrorText() const
{
    return m_preferenceConfirmedError.isEmpty() ? m_preferenceTransientError
                                                 : m_preferenceConfirmedError;
}

QString ClipboardSettingsModel::preferenceStatusText() const
{
    switch (m_preferenceState) {
    case PreferenceState::Loading:
        return QStringLiteral("Loading the clipboard-history preference…");
    case PreferenceState::Ready:
        return m_preferenceDirty ? QStringLiteral("Draft not yet applied.") : QString{};
    case PreferenceState::Saving:
        return QStringLiteral("Applying the clipboard-history preference…");
    case PreferenceState::Conflict:
        return QStringLiteral("The preference changed elsewhere. Review the current value or apply your draft again.");
    case PreferenceState::Unavailable:
        return m_hasPreferenceBaseline
            ? QStringLiteral("Settings1 is unavailable; the last confirmed preference is shown.")
            : QStringLiteral("The clipboard-history preference is unavailable.");
    }
    return {};
}

bool ClipboardSettingsModel::setDraftHistoryEnabled(bool enabled)
{
    if (!canEditPreference()) {
        return false;
    }
    if (m_draftEnabled == enabled) {
        return true;
    }
    m_draftEnabled = enabled;
    m_preferenceDirty = m_draftEnabled != m_historyEnabled;
    Q_EMIT viewChanged();
    return true;
}

bool ClipboardSettingsModel::cancelPreferenceDraft()
{
    if (!canEditPreference() || !m_preferenceDirty) {
        return false;
    }
    m_draftEnabled = m_historyEnabled;
    m_preferenceDirty = false;
    m_conflictIntent = false;
    m_preferenceConfirmedError.clear();
    setPreferenceState(PreferenceState::Ready);
    return true;
}

bool ClipboardSettingsModel::applyPreference()
{
    if (!applyAvailable()) {
        return false;
    }
    m_preferenceConfirmedError.clear();
    m_conflictIntent = false;
    m_waitingPreferenceSnapshot = false;
    QString error;
    if (!m_settingsClient.setUserValue(
            QString::fromLatin1(ClipboardHistorySettingsKey), m_draftEnabled, &error)) {
        setPreferenceState(PreferenceState::Unavailable,
                           error.left(MaximumDiagnosticLength));
        return false;
    }
    setPreferenceState(PreferenceState::Saving);
    return true;
}

bool ClipboardSettingsModel::applyMyChoice()
{
    if (!preferenceConflict() || !m_preferenceDirty
        || m_settingsClient.state() != ClientState::Ready) {
        return false;
    }
    setPreferenceState(PreferenceState::Ready);
    return applyPreference();
}

void ClipboardSettingsModel::retryPreference()
{
    m_settingsClient.refresh();
}

void ClipboardSettingsModel::handleSettingsState()
{
    switch (m_settingsClient.state()) {
    case ClientState::Ready:
        if (!m_waitingPreferenceSnapshot && !m_conflictIntent) {
            setPreferenceState(PreferenceState::Ready);
        }
        break;
    case ClientState::Authenticating:
        if (!m_waitingPreferenceSnapshot && !m_conflictIntent) {
            setPreferenceState(m_hasPreferenceBaseline ? PreferenceState::Unavailable
                                                       : PreferenceState::Loading,
                               m_settingsClient.lastError());
        }
        break;
    case ClientState::Unavailable:
    case ClientState::Degraded:
        m_waitingPreferenceSnapshot = false;
        setPreferenceState(PreferenceState::Unavailable,
                           m_settingsClient.lastError());
        break;
    }
}

void ClipboardSettingsModel::handleSettingsSnapshot()
{
    if (!m_settingsClient.snapshot()) {
        return;
    }
    const auto &snapshot = *m_settingsClient.snapshot();
    const QVariant value = snapshot.values.value(
        QString::fromLatin1(ClipboardHistorySettingsKey));
    if (value.metaType().id() != QMetaType::Bool) {
        setPreferenceState(PreferenceState::Unavailable,
                           QStringLiteral("Clipboard history has an invalid Settings1 value."));
        return;
    }
    const bool lineageChanged = m_hasPreferenceBaseline
        && (snapshot.owner != m_settingsOwner || snapshot.epoch != m_settingsEpoch);
    m_settingsOwner = snapshot.owner;
    m_settingsEpoch = snapshot.epoch;
    m_historyEnabled = value.toBool();
    if (!m_hasPreferenceBaseline || !m_preferenceDirty) {
        m_draftEnabled = m_historyEnabled;
        m_preferenceDirty = false;
    } else {
        m_preferenceDirty = m_draftEnabled != m_historyEnabled;
    }
    m_hasPreferenceBaseline = true;

    if (lineageChanged && (m_waitingPreferenceSnapshot || preferenceSaving())) {
        m_waitingPreferenceSnapshot = false;
        m_conflictIntent = false;
        m_preferenceTransientError = QStringLiteral(
            "Settings authority changed; the draft was not replayed.");
        m_preferenceState = PreferenceState::Ready;
        Q_EMIT viewChanged();
        return;
    }
    if (m_waitingPreferenceSnapshot) {
        m_waitingPreferenceSnapshot = false;
        if (!m_preferenceDirty) {
            m_conflictIntent = false;
            setPreferenceState(PreferenceState::Ready);
        } else {
            m_conflictIntent = true;
            setPreferenceState(PreferenceState::Conflict);
        }
        return;
    }
    if (m_conflictIntent && !m_preferenceDirty) {
        m_conflictIntent = false;
    }
    setPreferenceState(m_conflictIntent ? PreferenceState::Conflict
                                        : PreferenceState::Ready);
}

void ClipboardSettingsModel::handleSettingsCommit(const CommitOutcome &outcome)
{
    if (!preferenceSaving()) {
        return;
    }
    if (outcome.status == SettingsWireStatus::Applied) {
        m_waitingPreferenceSnapshot = true;
        setPreferenceState(PreferenceState::Saving);
        return;
    }
    if (outcome.status == SettingsWireStatus::Conflict) {
        const QVariant current = outcome.currentValues.value(
            QString::fromLatin1(ClipboardHistorySettingsKey));
        if (current.metaType().id() == QMetaType::Bool
            && current.toBool() == m_draftEnabled) {
            m_waitingPreferenceSnapshot = true;
            setPreferenceState(PreferenceState::Saving);
        } else {
            m_conflictIntent = true;
            setPreferenceState(PreferenceState::Conflict,
                               QStringLiteral("The preference changed elsewhere."));
        }
        return;
    }
    m_preferenceConfirmedError = outcome.message.left(MaximumDiagnosticLength);
    if (m_preferenceConfirmedError.isEmpty()) {
        m_preferenceConfirmedError = QStringLiteral("Settings1 rejected the preference change.");
    }
    setPreferenceState(PreferenceState::Ready);
}

void ClipboardSettingsModel::handleSettingsUncertain(const QString &message)
{
    m_waitingPreferenceSnapshot = false;
    m_conflictIntent = false;
    m_preferenceConfirmedError = QStringLiteral(
        "The preference outcome is uncertain; the draft was not replayed.");
    if (!message.isEmpty()) {
        m_preferenceConfirmedError += QStringLiteral(" ")
            + message.left(MaximumDiagnosticLength);
    }
    setPreferenceState(PreferenceState::Unavailable);
}

void ClipboardSettingsModel::setPreferenceState(PreferenceState state,
                                                QString transientError)
{
    const QString nextError = std::move(transientError);
    if (m_preferenceState == state && m_preferenceTransientError == nextError) {
        return;
    }
    m_preferenceState = state;
    m_preferenceTransientError = nextError;
    Q_EMIT viewChanged();
}

} // namespace QindaQt::Apps::SettingsClipboard
