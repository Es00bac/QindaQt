// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/clipboard_model/clipboard_types.h>

namespace QindaQt::Services::ClipboardModel {

// Volatile, privacy-aware clipboard history state machine.
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
// Denied and history disabled. Admission requires BOTH enabled history and
// Allowed privacy; snapshot contents are empty in either state. Setting
// privacy to Denied or disabling history purges every entry and raises the
// generation, so every previously observed EntryId and every in-flight
// admission decision made before the transition becomes stale and is
// refused.
//
// AGENT-CONTRACT (generation fencing): generation starts at 1 and increases
// by exactly one per purge. Every content mutation except the two authority
// transitions takes the caller's expectedGeneration and refuses with
// StaleGeneration on mismatch. Revision increases by exactly one per
// successful state change (admission, dedup move, promote, pin change,
// removal, clear that removes at least one entry); refusals change nothing.
class ClipboardHistoryModel final {
public:
    // Requires isValidLimits(limits); a caller that wants the protocol
    // defaults passes a default-constructed HistoryLimits.
    explicit ClipboardHistoryModel(HistoryLimits limits = HistoryLimits {});

    [[nodiscard]] const HistoryLimits &limits() const noexcept { return m_limits; }
    [[nodiscard]] quint32 generation() const noexcept { return m_generation; }
    [[nodiscard]] quint64 revision() const noexcept { return m_revision; }
    [[nodiscard]] bool isHistoryEnabled() const noexcept { return m_historyEnabled; }
    [[nodiscard]] PrivacyState privacyState() const noexcept { return m_privacy; }

    // Authority transitions. Disabling an enabled history purges; enabling
    // only raises the flag (admission still requires privacy Allowed).
    // Re-stating the current value is a no-op that neither purges nor
    // raises the generation.
    void setHistoryEnabled(bool enabled);
    void setPrivacyAllowed(bool allowed);

    // Admits a value when every gate passes. Refusal order is fixed and
    // tested: HistoryDisabled, PrivacyDenied, StaleGeneration, then value
    // validation (EmptyValue, TooManyFormats, DuplicateFormat,
    // MediaTypeRejected, SensitiveRefused, OneTimeRefused,
    // NonStorableRefused, OversizedValue), then CapacityRefused. A refusal
    // stores nothing and changes no state; the refused payload is dropped.
    // sourceLabel is sanitized and clamped before storage; tick is caller
    // monotonic metadata stored verbatim in the descriptor.
    [[nodiscard]] AdmitOutcome admit(const ClipboardValue &value,
                                     quint32 expectedGeneration,
                                     const QString &sourceLabel,
                                     quint64 tick);

    // Re-selects an entry: moves it to the most-recent position, refreshes
    // lastUsedTick, and returns the stored payload as a copy. Refusals:
    // HistoryDisabled, PrivacyDenied, StaleGeneration, UnknownEntry (also
    // covers ids from an earlier generation).
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
    // in the documented order or an empty optional when the caller may act.
    struct Gate {
        ClipboardError error = ClipboardError::None;
        bool refused = false;
    };
    [[nodiscard]] Gate gateOperation(quint32 expectedGeneration) const noexcept;
    [[nodiscard]] qsizetype indexOf(EntryId id) const noexcept;
    [[nodiscard]] qsizetype lastUnpinnedIndex() const noexcept;
    void purgeAndRaiseGeneration();
    [[nodiscard]] ClipboardEntryDescriptor makeDescriptor(const ClipboardValue &value,
                                                          const QString &sourceLabel,
                                                          quint64 tick) const;

    HistoryLimits m_limits;
    QList<Entry> m_entries; // index 0 is the most recently used
    quint32 m_generation = 1;
    quint32 m_nextSerial = 1;
    quint64 m_revision = 0;
    qint64 m_totalPayloadBytes = 0;
    bool m_historyEnabled = false;
    PrivacyState m_privacy = PrivacyState::Denied;
};

} // namespace QindaQt::Services::ClipboardModel
