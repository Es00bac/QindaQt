// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_clipboard/clipboard_settings_model.h>

#include <qindaqt/services/clipboard_client/clipboard_client.h>
#include <qindaqt/services/clipboard_model/clipboard_descriptor.h>

namespace QindaQt::Apps::SettingsClipboard {
using namespace Services;

int ClipboardSettingsModel::capacity() const noexcept
{
    return ClipboardModel::kMaxEntries;
}

QString ClipboardSettingsModel::serviceState() const
{
    if (!m_serviceReady) {
        return QStringLiteral("degraded");
    }
    if (m_privacyDenied) {
        return QStringLiteral("privacy-denied");
    }
    return QStringLiteral("available");
}

QString ClipboardSettingsModel::serviceStatusText() const
{
    if (!m_serviceReady) {
        return QStringLiteral("Clipboard1 is unavailable or its current snapshot cannot be trusted.");
    }
    if (!m_serviceHistoryEnabled) {
        return QStringLiteral("Clipboard1 is available. History capture is off.");
    }
    if (m_privacyDenied) {
        return QStringLiteral("Clipboard1 denied history access for privacy; no history metadata is retained here.");
    }
    return QStringLiteral("Clipboard1 history metadata is current.");
}

bool ClipboardSettingsModel::clearAvailable() const noexcept
{
    return m_serviceReady && m_serviceHistoryEnabled && !m_privacyDenied
        && m_entryCount > 0 && !m_confirmingClear
        && m_clearState != ClearState::Pending
        && !m_clipboardClient.operationPending();
}

QString ClipboardSettingsModel::clearStatusText() const
{
    switch (m_clearState) {
    case ClearState::Idle:
        return {};
    case ClearState::Pending:
        return m_waitingClearSnapshot
            ? QStringLiteral("Clear accepted; waiting for the authoritative history snapshot…")
            : QStringLiteral("Clearing clipboard history…");
    case ClearState::Succeeded:
        return m_entryCount == 0
            ? QStringLiteral("Clipboard history cleared.")
            : QStringLiteral("Clipboard history cleared; new items have since been captured.");
    case ClearState::Failed:
        return QStringLiteral("Clipboard history was not cleared.");
    case ClearState::Uncertain:
        return QStringLiteral("The clear outcome is uncertain. It was not retried.");
    }
    return {};
}

void ClipboardSettingsModel::retryClipboard()
{
    // ClipboardClient start is intentionally idempotent. A deliberate
    // stop/start is the route's explicit retry and cannot replay a mutation.
    m_clipboardClient.stop();
    m_clipboardClient.start();
}

bool ClipboardSettingsModel::requestClearHistory()
{
    if (!clearAvailable()) {
        return false;
    }
    m_confirmOwner = m_serviceOwner;
    m_confirmEpoch = m_serviceEpoch;
    m_confirmGeneration = m_serviceGeneration;
    m_confirmRevision = m_serviceRevision;
    m_confirmingClear = true;
    Q_EMIT viewChanged();
    return true;
}

void ClipboardSettingsModel::cancelClearHistory()
{
    if (!m_confirmingClear) {
        return;
    }
    m_confirmingClear = false;
    Q_EMIT viewChanged();
}

bool ClipboardSettingsModel::sameClipboardAuthority() const
{
    if (m_clipboardClient.state() != Clipboard::ClientState::Ready
        || !m_clipboardClient.hasSnapshot()) {
        return false;
    }
    const Clipboard::Snapshot snapshot = m_clipboardClient.snapshot();
    return m_clipboardClient.owner() == m_confirmOwner
        && snapshot.epoch == m_confirmEpoch
        && snapshot.generation == m_confirmGeneration
        && snapshot.revision == m_confirmRevision;
}

bool ClipboardSettingsModel::confirmClearHistory()
{
    if (!m_confirmingClear || !sameClipboardAuthority() || m_entryCount <= 0
        || m_clipboardClient.operationPending()) {
        m_confirmingClear = false;
        m_clearState = ClearState::Failed;
        m_clearError = QStringLiteral("History changed before confirmation; review the current state and try again.");
        Q_EMIT viewChanged();
        return false;
    }
    m_clearOwner = m_confirmOwner;
    m_clearEpoch = m_confirmEpoch;
    m_clearGeneration = m_confirmGeneration;
    m_clearRevision = m_confirmRevision;
    m_clearError.clear();
    m_confirmingClear = false;
    m_clearRequestId = m_clipboardClient.clear(true);
    if (m_clearRequestId == 0) {
        m_clearState = ClearState::Failed;
        m_clearError = QStringLiteral("Clipboard1 could not admit a clear request.");
        Q_EMIT viewChanged();
        return false;
    }
    m_clearState = ClearState::Pending;
    m_waitingClearSnapshot = false;
    Q_EMIT viewChanged();
    return true;
}

void ClipboardSettingsModel::handleClipboardState()
{
    if (m_clipboardClient.state() == Clipboard::ClientState::Ready
        && m_clipboardClient.hasSnapshot()) {
        handleClipboardSnapshot(m_clipboardClient.snapshot());
        return;
    }
    if (m_clearState == ClearState::Pending) {
        retireClearAsUncertain(QStringLiteral("Clipboard1 authority was lost during clear."));
    }
    m_serviceReady = false;
    m_serviceHistoryEnabled = false;
    m_privacyDenied = false;
    m_entryCount = 0;
    m_serviceOwner.clear();
    m_serviceEpoch = 0;
    m_serviceGeneration = 0;
    m_serviceRevision = 0;
    m_confirmingClear = false;
    Q_EMIT viewChanged();
}

void ClipboardSettingsModel::handleClipboardSnapshot(const Clipboard::Snapshot &snapshot)
{
    const ClipboardModel::DecodedDescriptorList decoded =
        ClipboardModel::decodeDescriptorList(snapshot.descriptorList);
    if (!decoded.accepted()) {
        if (m_clearState == ClearState::Pending) {
            retireClearAsUncertain(QStringLiteral("Clipboard1 published invalid metadata after clear."));
        }
        m_serviceReady = false;
        m_entryCount = 0;
        m_confirmingClear = false;
        Q_EMIT viewChanged();
        return;
    }

    const QString nextOwner = m_clipboardClient.owner();
    const bool authorityChanged = !m_serviceOwner.isEmpty()
        && (nextOwner != m_serviceOwner || snapshot.epoch != m_serviceEpoch
            || snapshot.generation != m_serviceGeneration);
    m_serviceOwner = nextOwner;
    m_serviceEpoch = snapshot.epoch;
    m_serviceGeneration = snapshot.generation;
    m_serviceRevision = snapshot.revision;
    m_serviceHistoryEnabled = snapshot.historyEnabled;
    m_privacyDenied = snapshot.historyEnabled && !snapshot.privacyAllowed;
    m_entryCount = (snapshot.historyEnabled && snapshot.privacyAllowed)
        ? int(decoded.descriptors.size()) : 0;
    m_serviceReady = true;

    if (m_confirmingClear
        && (nextOwner != m_confirmOwner || snapshot.epoch != m_confirmEpoch
            || snapshot.generation != m_confirmGeneration
            || snapshot.revision != m_confirmRevision)) {
        m_confirmingClear = false;
    }
    if (m_clearState == ClearState::Pending && authorityChanged) {
        retireClearAsUncertain(QStringLiteral("Clipboard1 authority changed during clear."));
    } else if (m_waitingClearSnapshot && nextOwner == m_clearOwner
               && snapshot.epoch == m_clearEpoch
               && snapshot.generation == m_clearGeneration
               && snapshot.revision >= m_clearExpectedRevision) {
        m_waitingClearSnapshot = false;
        m_clearState = ClearState::Succeeded;
        m_clearError.clear();
    }
    Q_EMIT viewChanged();
}

void ClipboardSettingsModel::handleClearResult(
    quint64 requestId, const Clipboard::OperationResult &result)
{
    if (m_clearState != ClearState::Pending || requestId != m_clearRequestId) {
        return;
    }
    const bool exact = result.kind == Clipboard::OperationKind::Clear
        && result.requestId == m_clearRequestId
        && result.initiatingEpoch == m_clearEpoch
        && result.initiatingGeneration == m_clearGeneration
        && result.initiatingRevision == m_clearRevision;
    if (!exact || result.status == Clipboard::OperationStatus::Uncertain) {
        retireClearAsUncertain(exact ? result.reasonCode
                                    : QStringLiteral("Clipboard1 returned an inexact clear result."));
        return;
    }
    if (result.status != Clipboard::OperationStatus::Succeeded) {
        m_clearState = ClearState::Failed;
        m_clearError = result.reasonCode.isEmpty()
            ? QStringLiteral("Clipboard1 rejected the clear request.")
            : result.reasonCode.left(512);
        Q_EMIT viewChanged();
        return;
    }
    m_clearExpectedRevision = result.observedRevision;
    const Clipboard::Snapshot current = m_clipboardClient.snapshot();
    if (m_clipboardClient.owner() == m_clearOwner
        && current.epoch == m_clearEpoch
        && current.generation == m_clearGeneration
        && current.revision >= m_clearExpectedRevision) {
        m_clearState = ClearState::Succeeded;
        m_waitingClearSnapshot = false;
        m_clearError.clear();
    } else {
        m_waitingClearSnapshot = true;
    }
    Q_EMIT viewChanged();
}

void ClipboardSettingsModel::retireClearAsUncertain(QString reason)
{
    m_waitingClearSnapshot = false;
    m_clearState = ClearState::Uncertain;
    m_clearError = reason.isEmpty()
        ? QStringLiteral("The clear outcome is uncertain.") : reason.left(512);
    Q_EMIT viewChanged();
}

} // namespace QindaQt::Apps::SettingsClipboard
