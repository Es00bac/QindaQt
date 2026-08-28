// SPDX-License-Identifier: LGPL-3.0-or-later

#include "qindaqt/shell/clipboard_applet/clipboard_applet_controller.h"
#include "qindaqt/shell/clipboard_applet/clipboard_applet_model.h"

#include <QtCore/QDateTime>

namespace QindaQt::ShellClipboardApplet {

ClipboardAppletController::ClipboardAppletController(
    ClipboardClientInterface *client,
    QObject *parent)
    : QObject(parent)
    , m_client(client)
{
    if (m_client) {
        connect(m_client, &ClipboardClientInterface::stateChanged,
                this, &ClipboardAppletController::onStateChanged);
        connect(m_client, &ClipboardClientInterface::snapshotChanged,
                this, &ClipboardAppletController::onSnapshotChanged);
        connect(m_client, &ClipboardClientInterface::lockStateChanged,
                this, &ClipboardAppletController::onLockStateChanged);
        connect(m_client, &ClipboardClientInterface::operationCompleted,
                this, &ClipboardAppletController::onOperationCompleted);
        connect(m_client, &ClipboardClientInterface::searchCompleted,
                this, &ClipboardAppletController::onSearchCompleted);

        m_snapshot = m_client->snapshot();
    }
    reproject();
}

QString ClipboardAppletController::phaseText() const noexcept
{
    return phaseToString(m_projection.phase);
}

QString ClipboardAppletController::phaseReasonText() const noexcept
{
    return m_projection.phaseReasonText;
}

bool ClipboardAppletController::isLocked() const noexcept
{
    return m_client ? m_client->isLocked() : true;
}

bool ClipboardAppletController::isHistoryEnabled() const noexcept
{
    return m_snapshot.historyEnabled;
}

QVariantList ClipboardAppletController::entryRows() const
{
    QVariantList list;
    list.reserve(m_projection.entryRows.size());
    for (const auto &row : m_projection.entryRows) {
        list.append(QVariant::fromValue(row));
    }
    return list;
}

int ClipboardAppletController::entryCount() const noexcept
{
    return static_cast<int>(m_projection.entryRows.size());
}

int ClipboardAppletController::pinnedCount() const noexcept
{
    return m_projection.pinnedCount;
}

int ClipboardAppletController::unpinnedCount() const noexcept
{
    return m_projection.unpinnedCount;
}

qint64 ClipboardAppletController::totalPayloadBytes() const noexcept
{
    return m_projection.totalPayloadBytes;
}

QString ClipboardAppletController::totalPayloadBytesFormatted() const
{
    return m_projection.totalPayloadBytesFormatted;
}

bool ClipboardAppletController::isSearchActive() const noexcept
{
    return m_projection.isSearchActive;
}

QString ClipboardAppletController::searchQuery() const
{
    return m_projection.searchQuery;
}

int ClipboardAppletController::searchResultCount() const noexcept
{
    return m_projection.searchResultCount;
}

bool ClipboardAppletController::searchTruncated() const noexcept
{
    return m_projection.searchTruncated;
}

QString ClipboardAppletController::emptyReasonText() const
{
    return m_projection.emptyReasonText;
}

bool ClipboardAppletController::feedbackPresent() const noexcept
{
    return m_feedbackPresent;
}

QString ClipboardAppletController::feedback() const
{
    return m_feedback;
}

QString ClipboardAppletController::feedbackStatus() const
{
    return m_feedbackStatus;
}

void ClipboardAppletController::setFeedback(const QString &message, const QString &status)
{
    m_feedbackPresent = !message.isEmpty();
    m_feedback = message;
    m_feedbackStatus = status;
    Q_EMIT feedbackChanged();
}

void ClipboardAppletController::clearFeedback()
{
    if (m_feedbackPresent) {
        m_feedbackPresent = false;
        m_feedback.clear();
        Q_EMIT feedbackChanged();
    }
}

void ClipboardAppletController::cancelPendingForGeneration(quint32 oldGeneration)
{
    for (auto it = m_pendingRequests.begin(); it != m_pendingRequests.end();) {
        if (it->generation == oldGeneration) {
            m_pendingEntries.remove({it->id.generation, it->id.serial});
            it = m_pendingRequests.erase(it);
        } else {
            ++it;
        }
    }
}

void ClipboardAppletController::reproject()
{
    const auto state = m_client ? m_client->clientState() : ClientState::Unavailable;
    const auto reason = m_client ? m_client->reasonCode() : QStringLiteral("no-client");
    const auto ownerAvailable = m_client ? m_client->isOwnerAvailable() : false;
    const auto locked = m_client ? m_client->isLocked() : true;

    m_projection = ClipboardAppletModel::project(
        m_snapshot,
        state,
        reason,
        ownerAvailable,
        locked,
        m_isSearchActive,
        m_searchQuery,
        m_searchResults,
        m_searchTruncated,
        m_pendingEntries);

    Q_EMIT stateReprojected();
}

void ClipboardAppletController::onStateChanged(ClientState /*state*/, const QString &/*reasonCode*/)
{
    reproject();
}

void ClipboardAppletController::onSnapshotChanged(const QindaQt::Services::ClipboardModel::HistorySnapshot &snapshot)
{
    const bool generationChanged = (snapshot.generation != m_snapshot.generation);
    if (generationChanged) {
        cancelPendingForGeneration(m_snapshot.generation);
        if (m_isSearchActive) {
            // Re-run search against new generation if non-empty
            if (!m_searchQuery.isEmpty() && m_client) {
                m_activeSearchRequestId = m_client->requestSearch(
                    m_searchQuery, snapshot.generation, kMaxPresentedEntries);
            } else {
                clearSearch();
            }
        }
    }
    m_snapshot = snapshot;
    reproject();
}

void ClipboardAppletController::onLockStateChanged(bool locked)
{
    if (locked) {
        m_isSearchActive = false;
        m_searchQuery.clear();
        m_searchResults.clear();
        m_pendingRequests.clear();
        m_pendingEntries.clear();
        clearFeedback();
    }
    reproject();
}

void ClipboardAppletController::onOperationCompleted(
    quint64 requestId,
    const OperationOutcome &outcome)
{
    m_pendingRequests.remove(requestId);
    if (outcome.id.isValid()) {
        m_pendingEntries.remove({outcome.id.generation, outcome.id.serial});
    }

    if (!outcome.ok()) {
        setFeedback(outcome.message, QStringLiteral("error"));
    }
    reproject();
}

void ClipboardAppletController::onSearchCompleted(
    quint64 requestId,
    const QindaQt::Services::ClipboardModel::SearchOutcome &outcome)
{
    if (m_activeSearchRequestId != 0 && requestId < m_activeSearchRequestId) {
        return; // Stale search result from a previous query
    }
    m_activeSearchRequestId = requestId;

    if (outcome.accepted()) {
        m_searchResults = outcome.matches;
        m_searchTruncated = outcome.truncated;
    } else {
        m_searchResults.clear();
        m_searchTruncated = false;
    }
    reproject();
}

bool ClipboardAppletController::selectEntry(quint32 generation, quint32 serial)
{
    if (!m_client || m_projection.phase != Phase::Ready) {
        setFeedback(QStringLiteral("Clipboard is not currently accessible."));
        return false;
    }
    if (generation != m_snapshot.generation) {
        setFeedback(QStringLiteral("The clipboard history has changed. Please select from current entries."));
        return false;
    }

    const QindaQt::Services::ClipboardModel::EntryId id { generation, serial };
    if (m_pendingEntries.contains({generation, serial})) {
        return false; // Already in flight
    }

    m_pendingEntries.insert({generation, serial});
    const quint64 tick = static_cast<quint64>(QDateTime::currentMSecsSinceEpoch());
    const quint64 reqId = m_client->requestPromote(id, generation, tick);

    if (m_pendingEntries.contains({generation, serial})) {
        PendingRequest req;
        req.kind = OperationKind::Promote;
        req.id = id;
        req.generation = generation;
        m_pendingRequests.insert(reqId, req);
    }

    reproject();
    return true;
}

bool ClipboardAppletController::deleteEntry(quint32 generation, quint32 serial)
{
    if (!m_client || m_projection.phase != Phase::Ready) {
        setFeedback(QStringLiteral("Clipboard is not currently accessible."));
        return false;
    }
    if (generation != m_snapshot.generation) {
        setFeedback(QStringLiteral("The clipboard history has changed."));
        return false;
    }

    const QindaQt::Services::ClipboardModel::EntryId id { generation, serial };
    if (m_pendingEntries.contains({generation, serial})) {
        return false;
    }

    m_pendingEntries.insert({generation, serial});
    const quint64 reqId = m_client->requestRemove(id, generation);

    if (m_pendingEntries.contains({generation, serial})) {
        PendingRequest req;
        req.kind = OperationKind::Remove;
        req.id = id;
        req.generation = generation;
        m_pendingRequests.insert(reqId, req);
    }

    reproject();
    return true;
}

bool ClipboardAppletController::togglePin(quint32 generation, quint32 serial)
{
    if (!m_client || m_projection.phase != Phase::Ready) {
        setFeedback(QStringLiteral("Clipboard is not currently accessible."));
        return false;
    }
    if (generation != m_snapshot.generation) {
        setFeedback(QStringLiteral("The clipboard history has changed."));
        return false;
    }

    const QindaQt::Services::ClipboardModel::EntryId id { generation, serial };
    if (m_pendingEntries.contains({generation, serial})) {
        return false;
    }

    // Find current pin state
    bool currentPinned = false;
    for (const auto &entry : m_snapshot.entries) {
        if (entry.id == id) {
            currentPinned = entry.pinned;
            break;
        }
    }

    m_pendingEntries.insert({generation, serial});
    const quint64 reqId = m_client->requestSetPinned(id, !currentPinned, generation);

    if (m_pendingEntries.contains({generation, serial})) {
        PendingRequest req;
        req.kind = OperationKind::SetPinned;
        req.id = id;
        req.generation = generation;
        m_pendingRequests.insert(reqId, req);
    }

    reproject();
    return true;
}

bool ClipboardAppletController::clearHistory(bool unpinnedOnly)
{
    if (!m_client || m_projection.phase != Phase::Ready) {
        setFeedback(QStringLiteral("Clipboard is not currently accessible."));
        return false;
    }

    const auto scope = unpinnedOnly
        ? QindaQt::Services::ClipboardModel::ClearScope::UnpinnedOnly
        : QindaQt::Services::ClipboardModel::ClearScope::All;

    const quint64 reqId = m_client->requestClear(scope, m_snapshot.generation);
    PendingRequest req;
    req.kind = OperationKind::Clear;
    req.generation = m_snapshot.generation;
    m_pendingRequests.insert(reqId, req);

    return true;
}

void ClipboardAppletController::setSearchQuery(const QString &query)
{
    const QString trimmed = query.trimmed().left(kMaxSearchQueryLength);
    if (trimmed == m_searchQuery && m_isSearchActive) {
        return;
    }

    m_searchQuery = trimmed;
    if (m_searchQuery.isEmpty()) {
        clearSearch();
        return;
    }

    m_isSearchActive = true;
    if (m_client && m_projection.phase == Phase::Ready) {
        m_activeSearchRequestId = m_client->requestSearch(
            m_searchQuery, m_snapshot.generation, kMaxPresentedEntries);
    }
    reproject();
}

void ClipboardAppletController::clearSearch()
{
    if (!m_isSearchActive && m_searchQuery.isEmpty()) {
        return;
    }
    m_isSearchActive = false;
    m_searchQuery.clear();
    m_searchResults.clear();
    m_searchTruncated = false;
    m_activeSearchRequestId = 0;
    reproject();
}

} // namespace QindaQt::ShellClipboardApplet
