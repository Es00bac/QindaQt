// SPDX-License-Identifier: LGPL-3.0-or-later

#include "qindaqt/shell/clipboard_applet/clipboard_model_client_adapter.h"

namespace QindaQt::ShellClipboardApplet {

ClipboardModelClientAdapter::ClipboardModelClientAdapter(
    QindaQt::Services::ClipboardModel::ClipboardHistoryModel *model,
    QObject *parent)
    : ClipboardClientInterface(parent)
    , m_model(model)
{
}

QindaQt::Services::ClipboardModel::HistorySnapshot ClipboardModelClientAdapter::snapshot() const
{
    if (!m_model || !m_ownerAvailable || m_state == ClientState::Unavailable || m_locked) {
        QindaQt::Services::ClipboardModel::HistorySnapshot fallback;
        if (m_model) {
            fallback.generation = m_model->generation();
            fallback.revision = m_model->revision();
            fallback.historyEnabled = m_model->isHistoryEnabled();
            fallback.privacyAllowed = !m_locked && (m_model->privacyState() == QindaQt::Services::ClipboardModel::PrivacyState::Allowed);
        }
        return fallback;
    }
    return m_model->snapshot();
}

void ClipboardModelClientAdapter::setClientState(ClientState state, const QString &reasonCode)
{
    if (m_state == state && m_reasonCode == reasonCode) {
        return;
    }
    m_state = state;
    m_reasonCode = reasonCode;
    Q_EMIT stateChanged(m_state, m_reasonCode);
    notifyModelChanged();
}

void ClipboardModelClientAdapter::setOwner(const QString &owner, bool available)
{
    if (m_owner == owner && m_ownerAvailable == available) {
        return;
    }
    m_owner = owner;
    m_ownerAvailable = available;
    if (!m_ownerAvailable) {
        setClientState(ClientState::Unavailable, QStringLiteral("owner-lost"));
    } else {
        notifyModelChanged();
    }
}

void ClipboardModelClientAdapter::setLocked(bool locked)
{
    if (m_locked == locked) {
        return;
    }
    m_locked = locked;
    // AGENT-GUARD: an authenticated lock must deny model privacy BEFORE the
    // lock signal is observable, so the model purges every entry and raises
    // its generation immediately. Unlock must then be unable to redisclose
    // pre-lock content: the purged lineage is unreachable. Only authority the
    // lock itself removed is restored; an independent host denial survives
    // unlock unchanged. Violating the ordering leaks pre-lock content through
    // a snapshot read between the signal and the purge.
    if (m_model) {
        if (locked && !m_privacyDeniedByLock
            && m_model->privacyState()
                == QindaQt::Services::ClipboardModel::PrivacyState::Allowed) {
            m_model->setPrivacyAllowed(false);
            m_privacyDeniedByLock = true;
        } else if (!locked && m_privacyDeniedByLock) {
            m_model->setPrivacyAllowed(true);
            m_privacyDeniedByLock = false;
        }
    }
    Q_EMIT lockStateChanged(m_locked);
    notifyModelChanged();
}

void ClipboardModelClientAdapter::notifyModelChanged()
{
    Q_EMIT snapshotChanged(snapshot());
}

OperationOutcome ClipboardModelClientAdapter::mapClipboardError(
    QindaQt::Services::ClipboardModel::ClipboardError err,
    QindaQt::Services::ClipboardModel::EntryId id) const
{
    OperationOutcome outcome;
    outcome.id = id;
    switch (err) {
    case QindaQt::Services::ClipboardModel::ClipboardError::None:
        outcome.code = OperationErrorCode::None;
        break;
    case QindaQt::Services::ClipboardModel::ClipboardError::HistoryDisabled:
        outcome.code = OperationErrorCode::HistoryDisabled;
        outcome.message = QStringLiteral("Clipboard history is disabled.");
        break;
    case QindaQt::Services::ClipboardModel::ClipboardError::PrivacyDenied:
        outcome.code = OperationErrorCode::PrivacyDenied;
        outcome.message = QStringLiteral("Clipboard access is restricted by privacy policy.");
        break;
    case QindaQt::Services::ClipboardModel::ClipboardError::StaleGeneration:
        outcome.code = OperationErrorCode::StaleGeneration;
        outcome.message = QStringLiteral("The clipboard history has changed since this action was requested.");
        break;
    case QindaQt::Services::ClipboardModel::ClipboardError::UnknownEntry:
        outcome.code = OperationErrorCode::UnknownEntry;
        outcome.message = QStringLiteral("The requested clipboard entry no longer exists.");
        break;
    case QindaQt::Services::ClipboardModel::ClipboardError::PinnedLimitReached:
        outcome.code = OperationErrorCode::PinnedLimitReached;
        outcome.message = QStringLiteral("Maximum number of pinned items reached.");
        break;
    case QindaQt::Services::ClipboardModel::ClipboardError::CapacityRefused:
        outcome.code = OperationErrorCode::CapacityRefused;
        outcome.message = QStringLiteral("Clipboard capacity limit reached.");
        break;
    case QindaQt::Services::ClipboardModel::ClipboardError::LineageExhausted:
        outcome.code = OperationErrorCode::Failed;
        outcome.message = QStringLiteral("History lineage exhausted.");
        break;
    default:
        outcome.code = OperationErrorCode::Failed;
        outcome.message = QStringLiteral("Operation failed.");
        break;
    }
    return outcome;
}

quint64 ClipboardModelClientAdapter::requestPromote(
    QindaQt::Services::ClipboardModel::EntryId id,
    quint32 expectedGeneration,
    quint64 tick)
{
    const quint64 reqId = m_nextRequestId++;
    if (m_locked) {
        OperationOutcome outcome;
        outcome.code = OperationErrorCode::Locked;
        outcome.message = QStringLiteral("Cannot promote items while session is locked.");
        outcome.id = id;
        Q_EMIT operationCompleted(reqId, outcome);
        return reqId;
    }
    if (!m_model || !m_ownerAvailable || m_state != ClientState::Ready) {
        OperationOutcome outcome;
        outcome.code = OperationErrorCode::OwnerLost;
        outcome.message = QStringLiteral("Clipboard service is unavailable.");
        outcome.id = id;
        Q_EMIT operationCompleted(reqId, outcome);
        return reqId;
    }

    const auto res = m_model->promote(id, expectedGeneration, tick);
    const OperationOutcome outcome = mapClipboardError(res.error, id);
    if (outcome.ok()) {
        notifyModelChanged();
    }
    Q_EMIT operationCompleted(reqId, outcome);
    return reqId;
}

quint64 ClipboardModelClientAdapter::requestRemove(
    QindaQt::Services::ClipboardModel::EntryId id,
    quint32 expectedGeneration)
{
    const quint64 reqId = m_nextRequestId++;
    if (m_locked) {
        OperationOutcome outcome;
        outcome.code = OperationErrorCode::Locked;
        outcome.message = QStringLiteral("Cannot delete items while session is locked.");
        outcome.id = id;
        Q_EMIT operationCompleted(reqId, outcome);
        return reqId;
    }
    if (!m_model || !m_ownerAvailable || m_state != ClientState::Ready) {
        OperationOutcome outcome;
        outcome.code = OperationErrorCode::OwnerLost;
        outcome.message = QStringLiteral("Clipboard service is unavailable.");
        outcome.id = id;
        Q_EMIT operationCompleted(reqId, outcome);
        return reqId;
    }

    const auto res = m_model->removeEntry(id, expectedGeneration);
    const OperationOutcome outcome = mapClipboardError(res.error, id);
    if (outcome.ok()) {
        notifyModelChanged();
    }
    Q_EMIT operationCompleted(reqId, outcome);
    return reqId;
}

quint64 ClipboardModelClientAdapter::requestSetPinned(
    QindaQt::Services::ClipboardModel::EntryId id,
    bool pinned,
    quint32 expectedGeneration)
{
    const quint64 reqId = m_nextRequestId++;
    if (m_locked) {
        OperationOutcome outcome;
        outcome.code = OperationErrorCode::Locked;
        outcome.message = QStringLiteral("Cannot pin items while session is locked.");
        outcome.id = id;
        Q_EMIT operationCompleted(reqId, outcome);
        return reqId;
    }
    if (!m_model || !m_ownerAvailable || m_state != ClientState::Ready) {
        OperationOutcome outcome;
        outcome.code = OperationErrorCode::OwnerLost;
        outcome.message = QStringLiteral("Clipboard service is unavailable.");
        outcome.id = id;
        Q_EMIT operationCompleted(reqId, outcome);
        return reqId;
    }

    const auto res = m_model->setPinned(id, pinned, expectedGeneration);
    const OperationOutcome outcome = mapClipboardError(res.error, id);
    if (outcome.ok()) {
        notifyModelChanged();
    }
    Q_EMIT operationCompleted(reqId, outcome);
    return reqId;
}

quint64 ClipboardModelClientAdapter::requestClear(
    QindaQt::Services::ClipboardModel::ClearScope scope,
    quint32 expectedGeneration)
{
    const quint64 reqId = m_nextRequestId++;
    if (m_locked) {
        OperationOutcome outcome;
        outcome.code = OperationErrorCode::Locked;
        outcome.message = QStringLiteral("Cannot clear history while session is locked.");
        Q_EMIT operationCompleted(reqId, outcome);
        return reqId;
    }
    if (!m_model || !m_ownerAvailable || m_state != ClientState::Ready) {
        OperationOutcome outcome;
        outcome.code = OperationErrorCode::OwnerLost;
        outcome.message = QStringLiteral("Clipboard service is unavailable.");
        Q_EMIT operationCompleted(reqId, outcome);
        return reqId;
    }

    const auto res = m_model->clear(scope, expectedGeneration);
    const OperationOutcome outcome = mapClipboardError(res.error, {});
    if (outcome.ok()) {
        notifyModelChanged();
    }
    Q_EMIT operationCompleted(reqId, outcome);
    return reqId;
}

quint64 ClipboardModelClientAdapter::requestSearch(
    const QString &query,
    quint32 expectedGeneration,
    int maxResults)
{
    const quint64 reqId = m_nextRequestId++;
    if (m_locked || !m_model || !m_ownerAvailable || m_state != ClientState::Ready) {
        QindaQt::Services::ClipboardModel::SearchOutcome outcome;
        outcome.error = m_locked ? QindaQt::Services::ClipboardModel::ClipboardError::PrivacyDenied
                                 : QindaQt::Services::ClipboardModel::ClipboardError::HistoryDisabled;
        Q_EMIT searchCompleted(reqId, outcome);
        return reqId;
    }

    const auto outcome = m_model->search(query, expectedGeneration, maxResults);
    Q_EMIT searchCompleted(reqId, outcome);
    return reqId;
}

} // namespace QindaQt::ShellClipboardApplet
