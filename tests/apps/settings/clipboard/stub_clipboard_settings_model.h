// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtCore/QObject>

namespace QindaQt::Apps::SettingsClipboard::TestSupport {

class StubClipboardSettingsModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool preferenceLoading MEMBER preferenceLoading NOTIFY viewChanged)
    Q_PROPERTY(bool preferenceReady MEMBER preferenceReady NOTIFY viewChanged)
    Q_PROPERTY(bool preferenceSaving MEMBER preferenceSaving NOTIFY viewChanged)
    Q_PROPERTY(bool preferenceConflict MEMBER preferenceConflict NOTIFY viewChanged)
    Q_PROPERTY(bool preferenceUnavailable MEMBER preferenceUnavailable NOTIFY viewChanged)
    Q_PROPERTY(bool canEditPreference MEMBER canEditPreference NOTIFY viewChanged)
    Q_PROPERTY(bool historyEnabled MEMBER historyEnabled NOTIFY viewChanged)
    Q_PROPERTY(bool draftHistoryEnabled MEMBER draftHistoryEnabled NOTIFY viewChanged)
    Q_PROPERTY(bool preferenceDirty MEMBER preferenceDirty NOTIFY viewChanged)
    Q_PROPERTY(bool applyAvailable MEMBER applyAvailable NOTIFY viewChanged)
    Q_PROPERTY(QString preferenceStatusText MEMBER preferenceStatusText NOTIFY viewChanged)
    Q_PROPERTY(QString preferenceErrorText MEMBER preferenceErrorText NOTIFY viewChanged)
    Q_PROPERTY(QString serviceState MEMBER serviceState NOTIFY viewChanged)
    Q_PROPERTY(QString serviceStatusText MEMBER serviceStatusText NOTIFY viewChanged)
    Q_PROPERTY(bool serviceAvailable MEMBER serviceAvailable NOTIFY viewChanged)
    Q_PROPERTY(bool serviceDegraded MEMBER serviceDegraded NOTIFY viewChanged)
    Q_PROPERTY(bool privacyDenied MEMBER privacyDenied NOTIFY viewChanged)
    Q_PROPERTY(int entryCount MEMBER entryCount NOTIFY viewChanged)
    Q_PROPERTY(int capacity MEMBER capacity CONSTANT)
    Q_PROPERTY(qulonglong serviceEpoch MEMBER serviceEpoch NOTIFY viewChanged)
    Q_PROPERTY(qulonglong serviceGeneration MEMBER serviceGeneration NOTIFY viewChanged)
    Q_PROPERTY(qulonglong serviceRevision MEMBER serviceRevision NOTIFY viewChanged)
    Q_PROPERTY(bool clearAvailable MEMBER clearAvailable NOTIFY viewChanged)
    Q_PROPERTY(bool clearConfirmationPending MEMBER clearConfirmationPending NOTIFY viewChanged)
    Q_PROPERTY(bool clearBusy MEMBER clearBusy NOTIFY viewChanged)
    Q_PROPERTY(bool clearUncertain MEMBER clearUncertain NOTIFY viewChanged)
    Q_PROPERTY(QString clearStatusText MEMBER clearStatusText NOTIFY viewChanged)
    Q_PROPERTY(QString clearErrorText MEMBER clearErrorText NOTIFY viewChanged)

public:
    bool preferenceLoading = false;
    bool preferenceReady = true;
    bool preferenceSaving = false;
    bool preferenceConflict = false;
    bool preferenceUnavailable = false;
    bool canEditPreference = true;
    bool historyEnabled = false;
    bool draftHistoryEnabled = false;
    bool preferenceDirty = false;
    bool applyAvailable = false;
    QString preferenceStatusText;
    QString preferenceErrorText;
    QString serviceState = QStringLiteral("available");
    QString serviceStatusText = QStringLiteral("Clipboard1 history metadata is current.");
    bool serviceAvailable = true;
    bool serviceDegraded = false;
    bool privacyDenied = false;
    int entryCount = 3;
    int capacity = 64;
    qulonglong serviceEpoch = 12;
    qulonglong serviceGeneration = 4;
    qulonglong serviceRevision = 9;
    bool clearAvailable = true;
    bool clearConfirmationPending = false;
    bool clearBusy = false;
    bool clearUncertain = false;
    QString clearStatusText;
    QString clearErrorText;
    int draftRequests = 0;
    int applyRequests = 0;
    int retryPreferenceRequests = 0;
    int retryClipboardRequests = 0;
    int clearRequests = 0;
    int confirmRequests = 0;
    int cancelClearRequests = 0;

    Q_INVOKABLE bool setDraftHistoryEnabled(bool enabled)
    {
        draftHistoryEnabled = enabled;
        preferenceDirty = draftHistoryEnabled != historyEnabled;
        applyAvailable = preferenceDirty;
        ++draftRequests;
        Q_EMIT viewChanged();
        return true;
    }
    Q_INVOKABLE bool cancelPreferenceDraft() { return true; }
    Q_INVOKABLE bool applyPreference() { ++applyRequests; return true; }
    Q_INVOKABLE bool applyMyChoice() { ++applyRequests; return true; }
    Q_INVOKABLE void retryPreference() { ++retryPreferenceRequests; }
    Q_INVOKABLE void retryClipboard() { ++retryClipboardRequests; }
    Q_INVOKABLE bool requestClearHistory()
    {
        if (!clearAvailable) return false;
        ++clearRequests;
        clearConfirmationPending = true;
        Q_EMIT viewChanged();
        return true;
    }
    Q_INVOKABLE void cancelClearHistory()
    {
        ++cancelClearRequests;
        clearConfirmationPending = false;
        Q_EMIT viewChanged();
    }
    Q_INVOKABLE bool confirmClearHistory()
    {
        ++confirmRequests;
        clearConfirmationPending = false;
        Q_EMIT viewChanged();
        return true;
    }

Q_SIGNALS:
    void viewChanged();
};

} // namespace QindaQt::Apps::SettingsClipboard::TestSupport
