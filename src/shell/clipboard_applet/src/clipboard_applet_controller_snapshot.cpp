// SPDX-License-Identifier: LGPL-3.0-or-later

// Snapshot-admission member functions of ClipboardAppletController, split from
// clipboard_applet_controller.cpp under the source-shape decomposition rule.
// The fences here — hostile-input floor, collection bound, (generation,
// revision) monotonicity, and owner lineage — are one contract with the
// reproject/pending bookkeeping in the sibling files; keep the AGENT-GUARD
// invariants aligned when editing either side.

#include "qindaqt/shell/clipboard_applet/clipboard_applet_controller.h"
#include "qindaqt/shell/clipboard_applet/clipboard_snapshot_gate.h"

namespace QindaQt::ShellClipboardApplet {

void ClipboardAppletController::acceptSnapshot(
    const QindaQt::Services::ClipboardModel::HistorySnapshot &snapshot)
{
    if (!m_clipboardReadGranted) {
        // Read denial retains only the authority flags, never content.
        m_snapshot = {};
        m_snapshot.generation = snapshot.generation;
        m_snapshot.historyEnabled = snapshot.historyEnabled;
        m_snapshot.privacyAllowed = snapshot.privacyAllowed;
        return;
    }

    const QString currentOwner = m_client ? m_client->owner() : QString();
    const bool ownerAvailable = m_client && m_client->isOwnerAvailable();
    if (m_hasBaseline && (!ownerAvailable || currentOwner != m_baselineOwner)) {
        dropAcceptedBaseline();
    }
    if (!ownerAvailable) {
        // Without an owner there is no authority behind any content: keep the
        // authority flags only. The projection reports unavailable regardless.
        m_snapshot = {};
        m_snapshot.generation = snapshot.generation;
        m_snapshot.revision = snapshot.revision;
        m_snapshot.historyEnabled = snapshot.historyEnabled;
        m_snapshot.privacyAllowed = snapshot.privacyAllowed;
        return;
    }

    if (m_client->clientState() == ClientState::Unavailable) {
        // AGENT-GUARD: while the client reports the service unreachable,
        // retain authority flags only and never establish a fresh-owner
        // baseline from such a snapshot. The in-process adapter serves a
        // content-empty fallback in this state; accepting it as the new
        // owner's baseline would arm the NEXT real snapshot (which can still
        // carry the previous owner's history) as legitimate fresh content.
        m_snapshot = {};
        m_snapshot.generation = snapshot.generation;
        m_snapshot.revision = snapshot.revision;
        m_snapshot.historyEnabled = snapshot.historyEnabled;
        m_snapshot.privacyAllowed = snapshot.privacyAllowed;
        return;
    }

    if (m_hasBaseline) {
        // AGENT-GUARD: the C0 lineage is lexicographically monotonic —
        // generation rises by exactly one per purge and revision never moves
        // backwards within or across generations — so anything below the
        // accepted high-water is a stale or replayed snapshot and is refused
        // whole. Re-stating the accepted (generation, revision) is an
        // idempotent re-delivery and stays acceptable.
        if (snapshot.generation < m_baselineGeneration
            || (snapshot.generation == m_baselineGeneration
                && snapshot.revision < m_baselineRevision)) {
            rejectSnapshot();
            return;
        }
    } else if (m_baselineDroppedForOwner && !snapshot.entries.isEmpty()) {
        // AGENT-GUARD: volatile history starts empty for every new owner
        // (no persistence, no synchronization), so the first snapshot under a
        // replacement owner must be content-empty. Non-empty content here is
        // the previous owner's history replayed through the new owner — the
        // exact owner-substitution disclosure this fence exists to prevent.
        rejectSnapshot();
        return;
    }

    // AGENT-GUARD: hostile-input floor and collection bound. A refused
    // snapshot is never partially retained or sanitized into rows.
    if (assessSnapshot(snapshot) != SnapshotGateDecision::Accept) {
        rejectSnapshot();
        return;
    }

    m_snapshot = snapshot;
    noteObservedTicks(m_snapshot);
    m_hasBaseline = true;
    m_baselineDroppedForOwner = false;
    m_baselineOwner = currentOwner;
    m_baselineGeneration = snapshot.generation;
    m_baselineRevision = snapshot.revision;
    m_snapshotRejected = false;
}

void ClipboardAppletController::dropAcceptedBaseline()
{
    // AGENT-GUARD: owner loss/replacement voids the whole accepted baseline —
    // content, high-water fences, pending intents, and search state — so no
    // byte accepted under the old owner can be re-presented under the new
    // one, and the next snapshot must establish a fresh (content-empty)
    // baseline for its owner.
    m_hasBaseline = false;
    m_baselineDroppedForOwner = true;
    m_baselineOwner.clear();
    m_baselineGeneration = 0;
    m_baselineRevision = 0;
    m_snapshotRejected = false;
    m_snapshot = {};
    m_pendingRequests.clear();
    m_pendingEntries.clear();
    m_isSearchActive = false;
    abandonSearch();
    m_searchQuery.clear();
    clearFeedback();
}

void ClipboardAppletController::rejectSnapshot()
{
    // AGENT-GUARD: fail closed on any admission violation — the presented
    // copy, pending intents, and search state are destroyed, and the surface
    // reports unavailable ("invalid-snapshot") until a fresh valid snapshot
    // is accepted. The previously accepted high-water fence deliberately
    // SURVIVES so the violating lineage cannot return below it.
    m_snapshot = {};
    m_pendingRequests.clear();
    m_pendingEntries.clear();
    m_isSearchActive = false;
    abandonSearch();
    m_searchQuery.clear();
    m_snapshotRejected = true;
}

void ClipboardAppletController::noteObservedTicks(
    const QindaQt::Services::ClipboardModel::HistorySnapshot &snapshot)
{
    // qMax saturates at the fixed-width ceiling without wrapping; selectEntry
    // refuses promotes at that ceiling instead of issuing a wrapped tick.
    for (const auto &entry : snapshot.entries) {
        m_nextPromoteTick = qMax(m_nextPromoteTick, entry.lastUsedTick);
        m_nextPromoteTick = qMax(m_nextPromoteTick, entry.admittedTick);
    }
}

} // namespace QindaQt::ShellClipboardApplet
