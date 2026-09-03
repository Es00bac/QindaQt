// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QList>
#include <qindaqt/services/clipboard_model/clipboard_types.h>

namespace QindaQt::ShellClipboardApplet {

// AGENT-CONTRACT: Hostile-input admission floor for everything the applet
// receives from the client seam. Every incoming history snapshot and every
// search-reply match list passes through this gate (or an equivalent call at
// the controller) BEFORE any descriptor reaches the projection or retained
// controller state. The per-entry rules are the public C0 descriptor floor
// (reused through the canonical descriptor codec — never restated here) plus
// the storable-media allowlist and the per-snapshot entry-lineage rule; the
// collection bound is the C0 kMaxEntries ceiling. Anything that fails is
// rejected whole: the applet presents nothing for it rather than sanitizing
// hostile metadata into presentable rows. See
// docs/wiki/shell/clipboard-applet.md ("Snapshot admission gate").
enum class SnapshotGateDecision {
    Accept,
    // Ready and withheld C0 snapshots always carry a real model generation.
    RejectZeroGeneration,
    // Disabled/privacy-denied snapshots must expose neither descriptors nor
    // aggregate content bytes.
    RejectAuthorityContent,
    // More descriptors than the C0 kMaxEntries protocol ceiling.
    RejectCollectionBound,
    // C0 identities are unique within one snapshot.
    RejectDuplicateEntry,
    // More pinned descriptors than the C0 kMaxPinnedEntries ceiling.
    RejectPinnedBound,
    // An entry id whose generation disagrees with the generation the
    // collection claims; the C0 model purges on generation change, so a
    // snapshot can never legitimately mix entry generations.
    RejectEntryLineage,
    // Snapshot aggregate byte total negative or above kMaxTotalPayloadBytes.
    RejectAggregateBytes,
    // Snapshot aggregate does not equal the sum of descriptor byte claims.
    RejectAggregateMismatch,
    // Media classes the C0 model refuses for history storage; a snapshot or
    // search reply carrying them is forged, not merely unusual.
    RejectSensitiveMedia,
    RejectOneTimeMedia,
    RejectNonStorableMedia,
    // Any other C0 descriptor-floor violation (invalid identity, unpaired
    // surrogates, control/format characters, overlong label/preview,
    // non-canonical or duplicate media names, negative or oversized claimed
    // bytes, wrong fingerprint width, forged truncation flag).
    RejectDescriptorFloor,
};

// Assesses one descriptor collection against the admission floor. Every
// descriptor id must carry `expectedGeneration`: snapshots pass their own
// generation, search replies pass the generation the query was issued
// against. Deterministic order: collection bound, then per entry (in list
// order) lineage, C0 descriptor floor, and finally the media allowlist with
// the C0 precedence sensitive → one-time → non-storable.
[[nodiscard]] SnapshotGateDecision assessDescriptorList(
    const QList<QindaQt::Services::ClipboardModel::ClipboardEntryDescriptor> &descriptors,
    quint32 expectedGeneration);

// Assesses a whole incoming history snapshot: nonzero lineage, authority/content
// consistency, the descriptor collection floor, unique identities, pin bound,
// and an exact bounded aggregate byte claim.
[[nodiscard]] SnapshotGateDecision assessSnapshot(
    const QindaQt::Services::ClipboardModel::HistorySnapshot &snapshot);

} // namespace QindaQt::ShellClipboardApplet
