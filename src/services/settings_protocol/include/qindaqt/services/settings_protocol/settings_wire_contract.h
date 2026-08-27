// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QtTypes>

namespace QindaQt::Services::SettingsProtocol {

// AGENT-NOTE: org.qindaqt.Settings1 is a bounded *generic* scoped-snapshot
// and single-optimistic-transaction protocol over validated settings keys.
// It has no Do Not Disturb-specific method, field, or key baked into the
// wire surface: DND is just the first consumer, expressed entirely by a
// client-side view model/controller (shell composition and the settings
// app) that calls GetSnapshot/CommitUserTransaction with the
// "services.doNotDisturb" key. See
// docs/wiki/adr/0012-persist-notification-quieting-through-settings1.md.
//
// AGENT-CONTRACT: "owner-authenticated" means exact-unique-owner lineage
// fencing over the session bus (same-user session authority), not PID or
// executable attestation -- do not add compositor-PID checks here. Lineage
// is the pair (unique owner, epoch); epoch is a fresh value generated once
// per service process lifetime, so a revision is only meaningful when
// compared within the same (owner, epoch). WireSchemaVersion versions this
// wire contract independently of the active persisted settings schema
// version (see QINDAQT_SETTINGS_SCHEMA_VERSION in src/settings) -- the two
// numbers are unrelated and must not be conflated.
struct WireContract final {
    static constexpr quint32 WireSchemaVersion = 1;
    static constexpr auto ServiceName = "org.qindaqt.Settings1";
    static constexpr auto ObjectPath = "/org/qindaqt/Settings1";
    static constexpr auto InterfaceName = "org.qindaqt.Settings1";

    static constexpr auto GetSnapshotMethod = "GetSnapshot";
    static constexpr auto CommitUserTransactionMethod = "CommitUserTransaction";
    static constexpr auto SettingsChangedSignal = "SettingsChanged";

    // Bounds enforced before a request ever reaches the settings model, and
    // before a reply/signal is ever decoded by a client. A violation is
    // MalformedRequest, not a partial best-effort application.
    static constexpr qsizetype MaximumRequestedKeys = 64;
    static constexpr qsizetype MaximumOperationsPerTransaction = 64;
    static constexpr qsizetype MaximumKeyBytes = 256;
    static constexpr qsizetype MaximumStringValueBytes = 16'384;
    static constexpr qsizetype MaximumListEntries = 512;
    static constexpr qsizetype MaximumMapEntries = 256;
    static constexpr qsizetype MaximumValueDepth = 16;
    static constexpr qsizetype MaximumValueNodes = 4'096;
    static constexpr qsizetype MaximumAggregateValueBytes = 262'144;
    static constexpr qsizetype MaximumSnapshotValueNodes = 16'384;
    static constexpr qsizetype MaximumSnapshotValueBytes = 1'048'576;
    static constexpr qsizetype MaximumTransactionValueNodes = 16'384;
    static constexpr qsizetype MaximumTransactionValueBytes = 1'048'576;
    static constexpr qsizetype MaximumChangedKeysPerSignal = 64;
    static constexpr qsizetype MaximumEpochBytes = 128;
    static constexpr qsizetype MaximumMessageBytes = 2'048;

    // Fixed envelopes are deliberately smaller than the generic map limit.
    // The transport streams only this many fields before handing a reply to
    // the semantic validator, which then requires the exact field set.
    static constexpr qsizetype SnapshotReplyFieldCount = 8;
    static constexpr qsizetype CommitReplyFieldCount = 10;
    static constexpr qsizetype OperationFieldCount = 3;

    // QVariantMap field names shared by every request/reply/signal shape, so
    // the service and client never hand-duplicate string literals.
    static constexpr auto FieldEpoch = "epoch";
    static constexpr auto FieldWireSchemaVersion = "wireSchemaVersion";
    static constexpr auto FieldSettingsSchemaVersion = "settingsSchemaVersion";
    static constexpr auto FieldRevision = "revision";
    static constexpr auto FieldRevisionBefore = "revisionBefore";
    static constexpr auto FieldRevisionAfter = "revisionAfter";
    static constexpr auto FieldValues = "values";
    static constexpr auto FieldSourceLayers = "sourceLayers";
    static constexpr auto FieldChangedKeys = "changedKeys";
    static constexpr auto FieldStatus = "status";
    static constexpr auto FieldMessage = "message";
    static constexpr auto FieldKey = "key";
    static constexpr auto FieldKind = "kind";
    static constexpr auto FieldValue = "value";
    static constexpr auto OperationKindSet = "set";
    static constexpr auto OperationKindRemove = "remove";
};

} // namespace QindaQt::Services::SettingsProtocol
