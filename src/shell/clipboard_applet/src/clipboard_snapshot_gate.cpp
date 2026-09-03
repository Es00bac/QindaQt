// SPDX-License-Identifier: LGPL-3.0-or-later

#include "qindaqt/shell/clipboard_applet/clipboard_snapshot_gate.h"

#include <qindaqt/services/clipboard_model/clipboard_descriptor.h>
#include <qindaqt/services/clipboard_model/clipboard_media.h>

#include <QtCore/QSet>

namespace QindaQt::ShellClipboardApplet {

namespace {

// One descriptor: lineage, the shared C0 floor, then the media allowlist.
// AGENT-GUARD: the floor check deliberately reuses the canonical descriptor
// codec instead of restating the rules. The codec is the single documented
// validation floor for C0 descriptors (see the Clipboard service page);
// duplicating it here would drift the first time C0 tightens a rule.
SnapshotGateDecision assessDescriptor(
    const QindaQt::Services::ClipboardModel::ClipboardEntryDescriptor &descriptor,
    quint32 expectedGeneration)
{
    if (descriptor.id.generation != expectedGeneration) {
        return SnapshotGateDecision::RejectEntryLineage;
    }
    const auto encoded = QindaQt::Services::ClipboardModel::encodeDescriptor(descriptor);
    if (!encoded.accepted()) {
        return SnapshotGateDecision::RejectDescriptorFloor;
    }
    bool sawOneTime = false;
    bool sawNonStorable = false;
    for (const auto &format : descriptor.formats) {
        using QindaQt::Services::ClipboardModel::MediaClass;
        switch (QindaQt::Services::ClipboardModel::classifyMediaType(format.mediaType)) {
        case MediaClass::Sensitive:
            return SnapshotGateDecision::RejectSensitiveMedia;
        case MediaClass::OneTime:
            sawOneTime = true;
            break;
        case MediaClass::NonStorable:
            sawNonStorable = true;
            break;
        case MediaClass::Storable:
            break;
        }
    }
    // C0 refusal precedence (sensitive → one-time → non-storable) so identical
    // input always produces the identical decision regardless of format order.
    if (sawOneTime) {
        return SnapshotGateDecision::RejectOneTimeMedia;
    }
    if (sawNonStorable) {
        return SnapshotGateDecision::RejectNonStorableMedia;
    }
    return SnapshotGateDecision::Accept;
}

} // namespace

SnapshotGateDecision assessDescriptorList(
    const QList<QindaQt::Services::ClipboardModel::ClipboardEntryDescriptor> &descriptors,
    quint32 expectedGeneration)
{
    using QindaQt::Services::ClipboardModel::kMaxEntries;
    if (descriptors.size() > kMaxEntries) {
        return SnapshotGateDecision::RejectCollectionBound;
    }
    QSet<quint32> serials;
    int pinnedCount = 0;
    for (const auto &descriptor : descriptors) {
        const auto decision = assessDescriptor(descriptor, expectedGeneration);
        if (decision != SnapshotGateDecision::Accept) {
            return decision;
        }
        if (serials.contains(descriptor.id.serial)) {
            return SnapshotGateDecision::RejectDuplicateEntry;
        }
        serials.insert(descriptor.id.serial);
        pinnedCount += descriptor.pinned ? 1 : 0;
        if (pinnedCount > QindaQt::Services::ClipboardModel::kMaxPinnedEntries) {
            return SnapshotGateDecision::RejectPinnedBound;
        }
    }
    return SnapshotGateDecision::Accept;
}

SnapshotGateDecision assessSnapshot(
    const QindaQt::Services::ClipboardModel::HistorySnapshot &snapshot)
{
    using QindaQt::Services::ClipboardModel::kMaxTotalPayloadBytes;
    if (snapshot.generation == 0) {
        return SnapshotGateDecision::RejectZeroGeneration;
    }
    if ((!snapshot.historyEnabled || !snapshot.privacyAllowed)
        && (!snapshot.entries.isEmpty() || snapshot.totalPayloadBytes != 0)) {
        return SnapshotGateDecision::RejectAuthorityContent;
    }
    if (snapshot.totalPayloadBytes < 0 || snapshot.totalPayloadBytes > kMaxTotalPayloadBytes) {
        return SnapshotGateDecision::RejectAggregateBytes;
    }
    const auto descriptorDecision = assessDescriptorList(snapshot.entries, snapshot.generation);
    if (descriptorDecision != SnapshotGateDecision::Accept) {
        return descriptorDecision;
    }

    qint64 describedPayloadBytes = 0;
    for (const auto &descriptor : snapshot.entries) {
        for (const auto &format : descriptor.formats) {
            describedPayloadBytes += format.payloadBytes;
        }
    }
    if (describedPayloadBytes != snapshot.totalPayloadBytes) {
        return SnapshotGateDecision::RejectAggregateMismatch;
    }
    return SnapshotGateDecision::Accept;
}

} // namespace QindaQt::ShellClipboardApplet
