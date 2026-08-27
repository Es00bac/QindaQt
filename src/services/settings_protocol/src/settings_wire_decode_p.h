// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/services/settings_protocol/settings_wire_decode.h"

namespace QindaQt::Services::SettingsProtocol::Private {

// Same single bounded traversal as inbound decode, but reserves every
// QDBusSignature for the encoder itself so callers cannot smuggle the null
// marker into the local JSON domain.
[[nodiscard]] std::optional<QVariant> normalizeBoundedJsonValueForWireEncoding(
    const QVariant &value,
    QString *error,
    BoundedSettingsValueCodec::Usage *usage);
[[nodiscard]] std::optional<QVariant> normalizeBoundedJsonValueForWireEncoding(
    const QVariant &value,
    AggregateValueDecodeBudget &aggregate,
    QString *error,
    BoundedSettingsValueCodec::Usage *usage);

} // namespace QindaQt::Services::SettingsProtocol::Private
