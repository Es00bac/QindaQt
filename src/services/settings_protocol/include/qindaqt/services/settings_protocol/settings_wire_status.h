// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QString>
#include <QtTypes>

#include <optional>

namespace QindaQt::Services::SettingsProtocol {

// AGENT-CONTRACT: these ordinals are the org.qindaqt.Settings1 wire contract.
// Never renumber an existing value; append new statuses instead. This enum
// and the rest of the protocol module depend only on Qt Core/DBus -- mapping
// QindaQt::Settings::CommitStatus to a wire status is the service adapter's
// job (src/services/settings_service), not the shared protocol's.
//
// PersistenceFailed: copy-on-write ordering means the candidate model is
// validated and the candidate document is saved to disk *before* the
// authoritative in-memory model is swapped and published. A PersistenceFailed
// reply therefore means the authoritative model, revision, on-disk document,
// and SettingsChanged publication are all exactly as they were before the
// call -- nothing was ever swapped in. It is not "committed then failed to
// save."
enum class SettingsWireStatus : quint32 {
    Applied = 0,
    ValidationFailed = 1,
    Conflict = 2,
    ReadOnlyLayer = 3,
    PersistenceFailed = 4,
    // The authoritative revision is already the maximum representable value;
    // committing would wrap it back to a low number and let a stale client
    // believe an old snapshot is current. The service refuses the write
    // before touching the candidate model.
    RevisionExhausted = 5,
    // The caller's epoch does not match the service's current epoch: the
    // service process restarted (or the caller never established a
    // baseline). The revision comparison is meaningless across epochs, so
    // the caller must fetch a fresh GetSnapshot rather than retry the write.
    EpochMismatch = 6,
    // A requested or operation key is not defined by the active schema.
    UnknownKey = 7,
    // The request itself violates a protocol bound (too many keys/
    // operations, duplicate operation keys, oversized value, malformed
    // value shape) and was rejected before reaching the settings model.
    MalformedRequest = 8,
};

// Rejects any ordinal outside the known range so a malformed or
// future-protocol reply cannot be silently misread as a status it does not
// name.
[[nodiscard]] std::optional<SettingsWireStatus> fromWireStatus(quint32 wireStatus) noexcept;

[[nodiscard]] QString settingsWireStatusName(SettingsWireStatus status);

} // namespace QindaQt::Services::SettingsProtocol
