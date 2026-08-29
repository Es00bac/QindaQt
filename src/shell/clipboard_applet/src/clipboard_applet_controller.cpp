// SPDX-License-Identifier: LGPL-3.0-or-later

#include "qindaqt/shell/clipboard_applet/clipboard_applet_controller.h"
#include "qindaqt/shell/clipboard_applet/clipboard_applet_model.h"

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
        noteObservedTicks(m_snapshot);
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

int ClipboardAppletController::pendingOperationCount() const noexcept
{
    return static_cast<int>(m_pendingRequests.size());
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
    const quint32 oldGeneration = m_snapshot.generation;
    const bool generationChanged = (snapshot.generation != oldGeneration);
    if (generationChanged) {
        cancelPendingForGeneration(oldGeneration);
    }
    m_snapshot = snapshot;
    noteObservedTicks(m_snapshot);
    if (generationChanged && m_isSearchActive) {
        // Re-run the live query against the new generation; any reply to the
        // pre-transition request is fenced out by the query-generation bump.
        if (!m_searchQuery.isEmpty() && m_client) {
            dispatchSearch();
        } else {
            clearSearch();
        }
    }
    reproject();
}

void ClipboardAppletController::onLockStateChanged(bool locked)
{
    if (locked) {
        // AGENT-GUARD: a lock is an authority denial. Presentation must not
        // merely hide rows behind the locked phase — it must destroy its own
        // copy of pre-lock content, pending intents, and search state so an
        // unlock (or a client that never delivers the purged snapshot) can
        // never redisclose them. The adapter has already purged the model and
        // raised its generation before this signal fires.
        m_isSearchActive = false;
        abandonSearch();
        m_searchQuery.clear();
        m_snapshot.entries.clear();
        m_snapshot.totalPayloadBytes = 0;
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
    // AGENT-GUARD: a completion is attributed only to an id this controller
    // registered as pending. Unknown, duplicated, or replayed ids are hostile
    // or duplicate noise: they must not clear pending markers, alter
    // feedback, or otherwise touch state.
    if (m_insideClientCall) {
        // The issuing dispatch has not returned its request id yet; buffer
        // and attribute afterwards by exact id.
        m_deferredCompletions.append({requestId, outcome});
        return;
    }
    const auto it = m_pendingRequests.constFind(requestId);
    if (it == m_pendingRequests.constEnd()) {
        return;
    }
    m_pendingRequests.erase(it);
    applyOperationOutcome(outcome);
    reproject();
}

void ClipboardAppletController::onSearchCompleted(
    quint64 requestId,
    const QindaQt::Services::ClipboardModel::SearchOutcome &outcome)
{
    // AGENT-GUARD: request ids are unique but unordered by contract. A reply
    // is accepted only when its id maps to the current internal query
    // generation; anything else (superseded query, abandoned search, unknown
    // or duplicated id) is dropped without touching displayed results.
    if (m_insideClientCall) {
        // The issuing dispatch has not returned its request id yet; buffer
        // and attribute afterwards by exact id so a hostile adapter flushing
        // a superseded reply inside requestSearch() cannot impersonate the
        // live one.
        m_deferredSearchReplies.append({requestId, outcome});
        return;
    }

    const auto it = m_pendingSearchRequests.constFind(requestId);
    if (it == m_pendingSearchRequests.constEnd()) {
        return;
    }
    const quint64 replyQueryGeneration = it.value();
    m_pendingSearchRequests.erase(it);
    if (replyQueryGeneration != m_searchQueryGeneration) {
        return;
    }

    applySearchOutcome(outcome);
    reproject();
}

void ClipboardAppletController::applySearchOutcome(
    const QindaQt::Services::ClipboardModel::SearchOutcome &outcome)
{
    if (outcome.accepted()) {
        m_searchResults = outcome.matches;
        m_searchTruncated = outcome.truncated;
    } else {
        m_searchResults.clear();
        m_searchTruncated = false;
    }
}

void ClipboardAppletController::applyOperationOutcome(const OperationOutcome &outcome)
{
    if (outcome.id.isValid()) {
        m_pendingEntries.remove({outcome.id.generation, outcome.id.serial});
    }
    if (!outcome.ok()) {
        setFeedback(outcome.message, QStringLiteral("error"));
    }
}

void ClipboardAppletController::noteObservedTicks(
    const QindaQt::Services::ClipboardModel::HistorySnapshot &snapshot)
{
    for (const auto &entry : snapshot.entries) {
        m_nextPromoteTick = qMax(m_nextPromoteTick, entry.lastUsedTick);
        m_nextPromoteTick = qMax(m_nextPromoteTick, entry.admittedTick);
    }
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
    // AGENT-GUARD: promote ticks are monotonic metadata the model trusts for
    // recency ordering; wall-clock time can step backwards (NTP, suspend), so
    // ticks come from the controller's own strictly increasing counter.
    const quint64 tick = ++m_nextPromoteTick;
    m_insideClientCall = true;
    const quint64 reqId = m_client->requestPromote(id, generation, tick);
    m_insideClientCall = false;

    // Attribute any completion the seam emitted inside the call by exact id.
    bool completed = false;
    for (const auto &deferred : std::as_const(m_deferredCompletions)) {
        if (!completed && deferred.first == reqId) {
            completed = true;
            applyOperationOutcome(deferred.second);
        }
    }
    m_deferredCompletions.clear();

    if (!completed) {
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
    m_insideClientCall = true;
    const quint64 reqId = m_client->requestRemove(id, generation);
    m_insideClientCall = false;

    bool completed = false;
    for (const auto &deferred : std::as_const(m_deferredCompletions)) {
        if (!completed && deferred.first == reqId) {
            completed = true;
            applyOperationOutcome(deferred.second);
        }
    }
    m_deferredCompletions.clear();

    if (!completed) {
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
    m_insideClientCall = true;
    const quint64 reqId = m_client->requestSetPinned(id, !currentPinned, generation);
    m_insideClientCall = false;

    bool completed = false;
    for (const auto &deferred : std::as_const(m_deferredCompletions)) {
        if (!completed && deferred.first == reqId) {
            completed = true;
            applyOperationOutcome(deferred.second);
        }
    }
    m_deferredCompletions.clear();

    if (!completed) {
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

    // AGENT-GUARD: a seam that completes the clear synchronously inside
    // requestClear() must not leave a permanently pending record behind —
    // the completion is attributed by exact id and the record is registered
    // only when the answer has not already arrived.
    m_insideClientCall = true;
    const quint64 reqId = m_client->requestClear(scope, m_snapshot.generation);
    m_insideClientCall = false;

    bool completed = false;
    for (const auto &deferred : std::as_const(m_deferredCompletions)) {
        if (!completed && deferred.first == reqId) {
            completed = true;
            applyOperationOutcome(deferred.second);
        }
    }
    m_deferredCompletions.clear();

    if (!completed) {
        PendingRequest req;
        req.kind = OperationKind::Clear;
        req.generation = m_snapshot.generation;
        m_pendingRequests.insert(reqId, req);
    }

    reproject();
    return true;
}

void ClipboardAppletController::dispatchSearch()
{
    // Issuing a new query supersedes every earlier reply, whatever numeric
    // ids the client assigned them.
    ++m_searchQueryGeneration;
    m_pendingSearchRequests.clear();
    if (!m_client) {
        return;
    }
    // The seam may answer synchronously inside requestSearch(). Replies
    // emitted during the call are buffered and attributed afterwards ONLY by
    // exact request id, so a hostile adapter flushing a superseded reply
    // inside this call can never impersonate the live one.
    m_insideClientCall = true;
    const quint64 requestId = m_client->requestSearch(
        m_searchQuery, m_snapshot.generation, kMaxPresentedEntries);
    m_insideClientCall = false;

    bool consumed = false;
    for (const auto &deferred : std::as_const(m_deferredSearchReplies)) {
        if (!consumed && deferred.first == requestId) {
            consumed = true;
            applySearchOutcome(deferred.second);
        }
    }
    m_deferredSearchReplies.clear();
    if (!consumed) {
        m_pendingSearchRequests.insert(requestId, m_searchQueryGeneration);
    }
}

void ClipboardAppletController::abandonSearch()
{
    // Fence off all in-flight replies: late answers now map to either an
    // unknown id or an expired query generation.
    ++m_searchQueryGeneration;
    m_pendingSearchRequests.clear();
    m_searchResults.clear();
    m_searchTruncated = false;
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
        dispatchSearch();
    } else {
        // Without a dispatch the displayed rows must not keep results of an
        // earlier query lineage.
        abandonSearch();
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
    abandonSearch();
    reproject();
}

} // namespace QindaQt::ShellClipboardApplet
