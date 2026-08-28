// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/clipboard_model/clipboard_types.h>

namespace QindaQt::Services::ClipboardModel {

// Volatile, privacy-aware clipboard history state machine.
//
// AGENT-CONTRACT (thread confinement): the model is not thread-safe. The
// owning thread (the future host's Wayland/Qt main thread) must confine all
// calls or provide external synchronization. Returned Qt values are
// implicitly shared copies; writing to them detaches, so results are safe to
// hand to other threads after the call returns.
//
// AGENT-CONTRACT (ownership/lifetime): the model owns its own deep copy of
// every admitted payload; callers may mutate or destroy their ClipboardValue
// afterwards without affecting stored state. Values leave the model only as
// copies returned by promote(), which models the future "payload moves only
// after an explicit user action" rule. There is no I/O, no clock, no IPC, and
// no QML here: ticks are caller-supplied monotonic metadata, and the future
// Wayland adapter owns all transport, FD, and lock-authentication authority.
//
// AGENT-CONTRACT (privacy/enable gating): construction starts with privacy
// Denied and history disabled. Admission and every other content operation
// require BOTH enabled history and Allowed privacy; snapshot contents are
// empty in either state. Setting privacy to Denied or disabling history
// purges every entry and raises the generation, so every previously observed
// EntryId and every in-flight admission decision made before the transition
// becomes stale and is refused.
//
// AGENT-CONTRACT (generation fencing): generation starts at 1 and increases
// by exactly one per purge. Every content operation except the two authority
// transitions takes the caller's expectedGeneration and refuses with
// StaleGeneration on mismatch. Counters are fixed-width and fail closed:
// when serial, revision, or generation lineage is exhausted, content
// operations return LineageExhausted instead of wrapping — zero or duplicate
// EntryId values are unreachable by construction.
//
// AGENT-CONTRACT (revision lineage): revision advances by exactly one per
// successful CONTENT change (admission, dedup move, promote, pin change,
// removal, clear that removes at least one entry). It is deliberately a
// within-generation content lineage. Non-purging authority transitions only
// change the snapshot flags. Purging transitions also destroy content, but
// intentionally leave the old revision behind because the generation bump
// invalidates that entire lineage. Consumers observe authority through the
// snapshot flags plus the generation. Refusals and no-ops change nothing.
class ClipboardHistoryModel final {
public:
    // Normalizes limits and counters through sanitizeLimits/sanitizeCounters,
    // so the protocol ceilings hold in Release builds without relying on
    // assertions. The counters overload is a diagnostic/test seam for
    // lineage-boundary coverage; production code uses the defaults.
    explicit ClipboardHistoryModel(HistoryLimits limits = HistoryLimits {});
    ClipboardHistoryModel(HistoryLimits limits, const HistoryCounters &counters);

    [[nodiscard]] const HistoryLimits &limits() const noexcept { return m_limits; }
    [[nodiscard]] quint32 generation() const noexcept { return m_generation; }
    [[nodiscard]] quint64 revision() const noexcept { return m_revision; }
    [[nodiscard]] bool isHistoryEnabled() const noexcept { return m_historyEnabled; }
    [[nodiscard]] PrivacyState privacyState() const noexcept { return m_privacy; }

    // Authority transitions. Disabling an enabled history purges; enabling
    // only raises the flag (content operations still require privacy
    // Allowed). Re-stating the current value is a no-op that neither purges
    // nor raises the generation. A disabling/denying purge destroys content
    // and raises the generation without advancing the now-irrelevant old
    // generation's content revision.
    void setHistoryEnabled(bool enabled);
    void setPrivacyAllowed(bool allowed);

    // Admits a value when every gate passes. Refusal order is fixed and
    // tested: HistoryDisabled, PrivacyDenied, StaleGeneration,
    // LineageExhausted, then value validation (EmptyValue, TooManyFormats,
    // DuplicateFormat, MediaTypeRejected, SensitiveRefused, OneTimeRefused,
    // NonStorableRefused, OversizedValue — class refusals accumulated across
    // all formats, so precedence never depends on producer order), then
    // CapacityRefused. A refusal stores nothing, evicts nothing, and changes
    // no state; the refused payload is dropped. sourceLabel is sanitized and
    // clamped before storage; tick is caller monotonic metadata stored
    // verbatim in the descriptor.
    [[nodiscard]] AdmitOutcome admit(const ClipboardValue &value,
                                     quint32 expectedGeneration,
                                     const QString &sourceLabel,
                                     quint64 tick);

    // Re-selects an entry: moves it to the most-recent position, refreshes
    // lastUsedTick, and returns the stored payload as a copy. Refusals:
    // HistoryDisabled, PrivacyDenied, StaleGeneration, LineageExhausted,
    // UnknownEntry (also covers ids from an earlier generation).
    [[nodiscard]] PromoteOutcome promote(EntryId id,
                                         quint32 expectedGeneration,
                                         quint64 tick);

    // Removes one entry regardless of pin state (an explicit per-item delete
    // is a user decision). Refusals as for promote().
    [[nodiscard]] MutationOutcome removeEntry(EntryId id, quint32 expectedGeneration);

    // Pins or unpins an entry. Pinned entries survive capacity eviction and
    // UnpinnedOnly clears; pinning beyond limits.maxPinnedEntries is
    // PinnedLimitReached.
    [[nodiscard]] MutationOutcome setPinned(EntryId id, bool pinned, quint32 expectedGeneration);

    // ClearScope::UnpinnedOnly removes every unpinned entry and keeps pins;
    // ClearScope::All removes everything, pins included. A clear that
    // removes nothing succeeds without a revision change. Neither scope
    // raises the generation: user clearing is not an authority transition.
    [[nodiscard]] MutationOutcome clear(ClearScope scope, quint32 expectedGeneration);

    // Deterministic bounded metadata search over the current, unlocked
    // history. Case-insensitive substring match against the sanitized
    // sourceLabel and the bounded preview of each entry — payloads are never
    // searched. Matches are most-recent-first, capped at maxResults
    // (sanitized into [1, kMaxEntries]); truncated reports additional
    // matches beyond the cap. Refusal order as for promote(), plus
    // EmptyValue for an empty query and OversizedValue for a query longer
    // than kMaxPreviewCodeUnits. Search is a read: it never advances the
    // revision.
    [[nodiscard]] SearchOutcome search(const QString &query,
                                       quint32 expectedGeneration,
                                       int maxResults) const;

    // Always succeeds; when history is disabled or privacy is denied the
    // entry list is empty and the flags explain why.
    [[nodiscard]] HistorySnapshot snapshot() const;

    [[nodiscard]] qint64 totalPayloadBytes() const noexcept { return m_totalPayloadBytes; }

private:
    struct Entry {
        ClipboardEntryDescriptor descriptor;
        ClipboardValue value;
    };

    // Shared refusal gate for content operations; returns the first refusal
    // in the documented order or a cleared Gate when the caller may act.
    struct Gate {
        ClipboardError error = ClipboardError::None;
        bool refused = false;
    };
    [[nodiscard]] Gate gateOperation(quint32 expectedGeneration) const noexcept;
    [[nodiscard]] bool revisionAtCeiling() const noexcept;
    [[nodiscard]] qsizetype indexOf(EntryId id) const noexcept;
    void purgeAndRaiseGeneration();
    void bumpRevision() noexcept;
    [[nodiscard]] ClipboardEntryDescriptor makeDescriptor(const ClipboardValue &value,
                                                          const QString &sourceLabel,
                                                          quint64 tick) const;

    HistoryLimits m_limits;
    QList<Entry> m_entries; // index 0 is the most recently used
    quint32 m_generation = 1;
    quint32 m_nextSerial = 1;
    quint64 m_revision = 0;
    // Fail-closed exhaustion latches. Serial exhaustion only blocks
    // admission; generation exhaustion (a purge that cannot raise the
    // counter any further) blocks every content operation. Purging content
    // itself is never refused: a privacy purge must always destroy content.
    bool m_serialExhausted = false;
    bool m_generationExhausted = false;
    qint64 m_totalPayloadBytes = 0;
    bool m_historyEnabled = false;
    PrivacyState m_privacy = PrivacyState::Denied;
};

} // namespace QindaQt::Services::ClipboardModel
