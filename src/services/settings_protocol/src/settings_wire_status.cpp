// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/settings_protocol/settings_wire_status.h"

namespace QindaQt::Services::SettingsProtocol {

std::optional<SettingsWireStatus> fromWireStatus(quint32 wireStatus) noexcept
{
    switch (wireStatus) {
    case 0:
        return SettingsWireStatus::Applied;
    case 1:
        return SettingsWireStatus::ValidationFailed;
    case 2:
        return SettingsWireStatus::Conflict;
    case 3:
        return SettingsWireStatus::ReadOnlyLayer;
    case 4:
        return SettingsWireStatus::PersistenceFailed;
    case 5:
        return SettingsWireStatus::RevisionExhausted;
    case 6:
        return SettingsWireStatus::EpochMismatch;
    case 7:
        return SettingsWireStatus::UnknownKey;
    case 8:
        return SettingsWireStatus::MalformedRequest;
    default:
        return std::nullopt;
    }
}

QString settingsWireStatusName(SettingsWireStatus status)
{
    switch (status) {
    case SettingsWireStatus::Applied:
        return QStringLiteral("applied");
    case SettingsWireStatus::ValidationFailed:
        return QStringLiteral("validation-failed");
    case SettingsWireStatus::Conflict:
        return QStringLiteral("conflict");
    case SettingsWireStatus::ReadOnlyLayer:
        return QStringLiteral("read-only-layer");
    case SettingsWireStatus::PersistenceFailed:
        return QStringLiteral("persistence-failed");
    case SettingsWireStatus::RevisionExhausted:
        return QStringLiteral("revision-exhausted");
    case SettingsWireStatus::EpochMismatch:
        return QStringLiteral("epoch-mismatch");
    case SettingsWireStatus::UnknownKey:
        return QStringLiteral("unknown-key");
    case SettingsWireStatus::MalformedRequest:
        return QStringLiteral("malformed-request");
    }
    return QStringLiteral("unknown");
}

} // namespace QindaQt::Services::SettingsProtocol
