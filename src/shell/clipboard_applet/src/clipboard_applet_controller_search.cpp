// SPDX-License-Identifier: LGPL-3.0-or-later

// Search-lifecycle member functions of ClipboardAppletController, split from
// clipboard_applet_controller.cpp under the source-shape decomposition rule.
// The query-generation and exact-id reply fences here are one contract with
// the dispatch/drain path in the sibling file; keep the AGENT-GUARD
// invariants aligned when editing either side.

#include "qindaqt/shell/clipboard_applet/clipboard_applet_controller.h"
#include "qindaqt/shell/clipboard_applet/clipboard_snapshot_gate.h"

namespace QindaQt::ShellClipboardApplet {

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
    // AGENT-GUARD: a reply that already passed the id and query-generation
    // fences can still carry hostile match content. Matches are admitted
    // through the same descriptor floor as snapshots (against the generation
    // the query ran under); a refusal or a hostile list clears the displayed
    // results instead of copying hostile metadata into rows.
    if (outcome.accepted()
        && assessDescriptorList(outcome.matches, m_snapshot.generation)
            == SnapshotGateDecision::Accept) {
        m_searchResults = outcome.matches;
        m_searchTruncated = outcome.truncated;
    } else {
        m_searchResults.clear();
        m_searchTruncated = false;
    }
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
    // emitted during the call are buffered; the request is registered before
    // the drain so its own synchronous reply attributes by exact id, while a
    // hostile adapter flushing a superseded reply inside this call matches no
    // registered id and is dropped.
    m_insideClientCall = true;
    const quint64 requestId = m_client->requestSearch(
        m_searchQuery, m_snapshot.generation, kMaxPresentedEntries);
    m_insideClientCall = false;
    m_pendingSearchRequests.insert(requestId, m_searchQueryGeneration);
    drainDeferredSignals();
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
    if (!m_clipboardReadGranted) {
        return;
    }
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
