// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/clipboard_protocol/clipboard_protocol.h>

#include <QtCore/QObject>

namespace QindaQt::Services::Clipboard {
class ClipboardClient;
}
namespace QindaQt::Services::SettingsClient {
class SettingsClient;
struct CommitOutcome;
}

namespace QindaQt::Apps::SettingsClipboard {

inline constexpr char ClipboardHistorySettingsKey[] = "services.clipboardHistory";

// AGENT-CONTRACT: This same-thread projection borrows one public Settings1
// client and one public Clipboard1 client; both must outlive it. QML receives
// counts, authority state, and one all-history clear intent only. Descriptor
// identities and clipboard content never cross this boundary.
class ClipboardSettingsModel final : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool preferenceLoading READ preferenceLoading NOTIFY viewChanged)
    Q_PROPERTY(bool preferenceReady READ preferenceReady NOTIFY viewChanged)
    Q_PROPERTY(bool preferenceSaving READ preferenceSaving NOTIFY viewChanged)
    Q_PROPERTY(bool preferenceConflict READ preferenceConflict NOTIFY viewChanged)
    Q_PROPERTY(bool preferenceUnavailable READ preferenceUnavailable NOTIFY viewChanged)
    Q_PROPERTY(bool canEditPreference READ canEditPreference NOTIFY viewChanged)
    Q_PROPERTY(bool historyEnabled READ historyEnabled NOTIFY viewChanged)
    Q_PROPERTY(bool draftHistoryEnabled READ draftHistoryEnabled NOTIFY viewChanged)
    Q_PROPERTY(bool preferenceDirty READ preferenceDirty NOTIFY viewChanged)
    Q_PROPERTY(bool applyAvailable READ applyAvailable NOTIFY viewChanged)
    Q_PROPERTY(QString preferenceStatusText READ preferenceStatusText NOTIFY viewChanged)
    Q_PROPERTY(QString preferenceErrorText READ preferenceErrorText NOTIFY viewChanged)

    Q_PROPERTY(QString serviceState READ serviceState NOTIFY viewChanged)
    Q_PROPERTY(QString serviceStatusText READ serviceStatusText NOTIFY viewChanged)
    Q_PROPERTY(bool serviceAvailable READ serviceAvailable NOTIFY viewChanged)
    Q_PROPERTY(bool serviceDegraded READ serviceDegraded NOTIFY viewChanged)
    Q_PROPERTY(bool privacyDenied READ privacyDenied NOTIFY viewChanged)
    Q_PROPERTY(int entryCount READ entryCount NOTIFY viewChanged)
    Q_PROPERTY(int capacity READ capacity CONSTANT)
    Q_PROPERTY(qulonglong serviceEpoch READ serviceEpoch NOTIFY viewChanged)
    Q_PROPERTY(qulonglong serviceGeneration READ serviceGeneration NOTIFY viewChanged)
    Q_PROPERTY(qulonglong serviceRevision READ serviceRevision NOTIFY viewChanged)

    Q_PROPERTY(bool clearAvailable READ clearAvailable NOTIFY viewChanged)
    Q_PROPERTY(bool clearConfirmationPending READ clearConfirmationPending NOTIFY viewChanged)
    Q_PROPERTY(bool clearBusy READ clearBusy NOTIFY viewChanged)
    Q_PROPERTY(bool clearUncertain READ clearUncertain NOTIFY viewChanged)
    Q_PROPERTY(QString clearStatusText READ clearStatusText NOTIFY viewChanged)
    Q_PROPERTY(QString clearErrorText READ clearErrorText NOTIFY viewChanged)

public:
    enum class PreferenceState { Loading, Ready, Saving, Conflict, Unavailable };
    enum class ClearState { Idle, Pending, Succeeded, Failed, Uncertain };

    ClipboardSettingsModel(
        QindaQt::Services::SettingsClient::SettingsClient &settingsClient,
        QindaQt::Services::Clipboard::ClipboardClient &clipboardClient,
        QObject *parent = nullptr);

    [[nodiscard]] bool preferenceLoading() const noexcept;
    [[nodiscard]] bool preferenceReady() const noexcept;
    [[nodiscard]] bool preferenceSaving() const noexcept;
    [[nodiscard]] bool preferenceConflict() const noexcept;
    [[nodiscard]] bool preferenceUnavailable() const noexcept;
    [[nodiscard]] bool canEditPreference() const noexcept;
    [[nodiscard]] bool historyEnabled() const noexcept { return m_historyEnabled; }
    [[nodiscard]] bool draftHistoryEnabled() const noexcept { return m_draftEnabled; }
    [[nodiscard]] bool preferenceDirty() const noexcept;
    [[nodiscard]] bool applyAvailable() const noexcept;
    [[nodiscard]] QString preferenceStatusText() const;
    [[nodiscard]] QString preferenceErrorText() const;

    [[nodiscard]] QString serviceState() const;
    [[nodiscard]] QString serviceStatusText() const;
    [[nodiscard]] bool serviceAvailable() const noexcept { return m_serviceReady; }
    [[nodiscard]] bool serviceDegraded() const noexcept { return !m_serviceReady; }
    [[nodiscard]] bool privacyDenied() const noexcept { return m_privacyDenied; }
    [[nodiscard]] int entryCount() const noexcept { return m_entryCount; }
    [[nodiscard]] int capacity() const noexcept;
    [[nodiscard]] qulonglong serviceEpoch() const noexcept { return m_serviceEpoch; }
    [[nodiscard]] qulonglong serviceGeneration() const noexcept { return m_serviceGeneration; }
    [[nodiscard]] qulonglong serviceRevision() const noexcept { return m_serviceRevision; }

    [[nodiscard]] bool clearAvailable() const noexcept;
    [[nodiscard]] bool clearConfirmationPending() const noexcept { return m_confirmingClear; }
    [[nodiscard]] bool clearBusy() const noexcept { return m_clearState == ClearState::Pending; }
    [[nodiscard]] bool clearUncertain() const noexcept { return m_clearState == ClearState::Uncertain; }
    [[nodiscard]] QString clearStatusText() const;
    [[nodiscard]] QString clearErrorText() const { return m_clearError; }

    Q_INVOKABLE bool setDraftHistoryEnabled(bool enabled);
    Q_INVOKABLE bool cancelPreferenceDraft();
    Q_INVOKABLE bool applyPreference();
    Q_INVOKABLE bool applyMyChoice();
    Q_INVOKABLE void retryPreference();
    Q_INVOKABLE void retryClipboard();
    Q_INVOKABLE bool requestClearHistory();
    Q_INVOKABLE void cancelClearHistory();
    Q_INVOKABLE bool confirmClearHistory();

Q_SIGNALS:
    void viewChanged();

private:
    void handleSettingsState();
    void handleSettingsSnapshot();
    void handleSettingsCommit(
        const QindaQt::Services::SettingsClient::CommitOutcome &outcome);
    void handleSettingsUncertain(const QString &message);
    void setPreferenceState(PreferenceState state, QString transientError = {});
    void handleClipboardState();
    void handleClipboardSnapshot(
        const QindaQt::Services::Clipboard::Snapshot &snapshot);
    void handleClearResult(quint64 requestId,
                           const QindaQt::Services::Clipboard::OperationResult &result);
    void retireClearAsUncertain(QString reason);
    [[nodiscard]] bool sameClipboardAuthority() const;

    QindaQt::Services::SettingsClient::SettingsClient &m_settingsClient;
    QindaQt::Services::Clipboard::ClipboardClient &m_clipboardClient;
    PreferenceState m_preferenceState = PreferenceState::Loading;
    ClearState m_clearState = ClearState::Idle;
    QString m_preferenceTransientError;
    QString m_preferenceConfirmedError;
    QString m_clearError;
    QString m_serviceOwner;
    QString m_confirmOwner;
    QString m_clearOwner;
    QString m_settingsOwner;
    QString m_settingsEpoch;
    qulonglong m_serviceEpoch = 0;
    qulonglong m_serviceRevision = 0;
    qulonglong m_confirmEpoch = 0;
    qulonglong m_confirmRevision = 0;
    qulonglong m_clearEpoch = 0;
    qulonglong m_clearRevision = 0;
    qulonglong m_clearExpectedRevision = 0;
    quint64 m_clearRequestId = 0;
    qulonglong m_serviceGeneration = 0;
    qulonglong m_confirmGeneration = 0;
    qulonglong m_clearGeneration = 0;
    int m_entryCount = 0;
    bool m_historyEnabled = false;
    bool m_draftEnabled = false;
    bool m_hasPreferenceBaseline = false;
    bool m_preferenceDirty = false;
    bool m_waitingPreferenceSnapshot = false;
    bool m_conflictIntent = false;
    bool m_serviceReady = false;
    bool m_serviceHistoryEnabled = false;
    bool m_privacyDenied = false;
    bool m_confirmingClear = false;
    bool m_waitingClearSnapshot = false;
};

} // namespace QindaQt::Apps::SettingsClipboard
