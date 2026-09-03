// SPDX-License-Identifier: LGPL-3.0-or-later

#include "qindaqt/shell/clipboard_applet/clipboard_applet_controller.h"
#include "qindaqt/shell/clipboard_applet/clipboard_applet_model.h"

#include <utility>

namespace QindaQt::ShellClipboardApplet {

ClipboardAppletController::ClipboardAppletController(
    ClipboardClientInterface *client,
    bool clipboardReadGranted,
    bool clipboardWriteGranted,
    QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_clipboardReadGranted(clipboardReadGranted)
    , m_clipboardWriteGranted(clipboardWriteGranted)
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

        if (m_clipboardReadGranted) {
            m_snapshot = m_client->snapshot();
            noteObservedTicks(m_snapshot);
        }
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

bool ClipboardAppletController::refuseMutation()
{
    // AGENT-GUARD: a missing clipboard.write grant refuses every mutating
    // intent before any dispatch or pending bookkeeping, so a denied applet
    // instance can browse but never changes model or live-selection state.
    if (!m_clipboardWriteGranted) {
        setFeedback(QStringLiteral("Clipboard control is not granted by applet policy."));
        return true;
    }
    return false;
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
    if (!m_clipboardReadGranted) {
        // AGENT-GUARD: read denial withholds observation entirely. The
        // projection must be built from an empty snapshot so no retained
        // entry metadata can reach QML while the grant is absent.
        m_projection = ClipboardAppletModel::project(
            {},
            ClientState::Unavailable,
            QStringLiteral("clipboard-read-not-granted"),
            false,
            false,
            false,
            QString(),
            {},
            false,
            {});
        Q_EMIT stateReprojected();
        return;
    }

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
    if (m_clipboardReadGranted) {
        m_snapshot = snapshot;
        noteObservedTicks(m_snapshot);
    } else {
        // Read denial retains only the authority flags, never content.
        m_snapshot = {};
        m_snapshot.generation = snapshot.generation;
        m_snapshot.historyEnabled = snapshot.historyEnabled;
        m_snapshot.privacyAllowed = snapshot.privacyAllowed;
    }
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
    // registered as pending (or the id of the in-flight dispatch). Unknown,
    // duplicated, or replayed ids are hostile or duplicate noise: they must
    // not clear pending markers, alter feedback, or otherwise touch state.
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

void ClipboardAppletController::drainDeferredSignals()
{
    // AGENT-GUARD: attribute every buffered synchronous signal by exact id —
    // the id of the dispatch in progress OR an id already registered as
    // pending. Draining only the current id would strand a legitimate
    // completion for an earlier request that a seam flushed inside this call
    // (permanently pending record); accepting unregistered ids would let a
    // replayed or hostile reply impersonate live state. Both failure modes
    // were exact-review findings; keep this the single attribution point.
    const auto completions = std::exchange(m_deferredCompletions, {});
    for (const auto &[requestId, outcome] : completions) {
        const auto it = m_pendingRequests.constFind(requestId);
        if (it == m_pendingRequests.constEnd()) {
            continue;
        }
        m_pendingRequests.erase(it);
        applyOperationOutcome(outcome);
    }
    const auto searchReplies = std::exchange(m_deferredSearchReplies, {});
    for (const auto &[requestId, outcome] : searchReplies) {
        const auto it = m_pendingSearchRequests.constFind(requestId);
        if (it == m_pendingSearchRequests.constEnd()) {
            continue;
        }
        const quint64 replyQueryGeneration = it.value();
        m_pendingSearchRequests.erase(it);
        if (replyQueryGeneration != m_searchQueryGeneration) {
            continue;
        }
        applySearchOutcome(outcome);
    }
}

quint64 ClipboardAppletController::dispatchOperation(
    OperationKind kind,
    QindaQt::Services::ClipboardModel::EntryId id,
    quint32 generation,
    const std::function<quint64(ClipboardClientInterface *)> &invoke)
{
    m_insideClientCall = true;
    const quint64 requestId = invoke(m_client);
    m_insideClientCall = false;

    // Register BEFORE draining so the seam's synchronous completion for this
    // very request attributes correctly through the single drain path.
    PendingRequest request;
    request.kind = kind;
    request.id = id;
    request.generation = generation;
    m_pendingRequests.insert(requestId, request);
    drainDeferredSignals();
    return requestId;
}

bool ClipboardAppletController::selectEntry(quint32 generation, quint32 serial)
{
    if (refuseMutation()) {
        return false;
    }
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
    dispatchOperation(OperationKind::Promote, id, generation,
                      [id, generation, tick](ClipboardClientInterface *client) {
                          return client->requestPromote(id, generation, tick);
                      });
    reproject();
    return true;
}

bool ClipboardAppletController::deleteEntry(quint32 generation, quint32 serial)
{
    if (refuseMutation()) {
        return false;
    }
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
    dispatchOperation(OperationKind::Remove, id, generation,
                      [id, generation](ClipboardClientInterface *client) {
                          return client->requestRemove(id, generation);
                      });
    reproject();
    return true;
}

bool ClipboardAppletController::togglePin(quint32 generation, quint32 serial)
{
    if (refuseMutation()) {
        return false;
    }
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
    dispatchOperation(OperationKind::SetPinned, id, generation,
                      [id, currentPinned, generation](ClipboardClientInterface *client) {
                          return client->requestSetPinned(id, !currentPinned, generation);
                      });
    reproject();
    return true;
}

bool ClipboardAppletController::clearHistory(bool unpinnedOnly)
{
    if (refuseMutation()) {
        return false;
    }
    if (!m_client || m_projection.phase != Phase::Ready) {
        setFeedback(QStringLiteral("Clipboard is not currently accessible."));
        return false;
    }

    const auto scope = unpinnedOnly
        ? QindaQt::Services::ClipboardModel::ClearScope::UnpinnedOnly
        : QindaQt::Services::ClipboardModel::ClearScope::All;
    const quint32 generation = m_snapshot.generation;

    // AGENT-GUARD: a seam that completes the clear synchronously inside
    // requestClear() must not leave a permanently pending record behind —
    // the completion is attributed by exact id through the drain path.
    dispatchOperation(OperationKind::Clear, {}, generation,
                      [scope, generation](ClipboardClientInterface *client) {
                          return client->requestClear(scope, generation);
                      });
    reproject();
    return true;
}





} // namespace QindaQt::ShellClipboardApplet
