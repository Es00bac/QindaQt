// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/services/settings_protocol/settings_wire_decode.h"

namespace QindaQt::Services::SettingsProtocol {

// Produces a QtDBus-safe representation of one bounded JSON-native value.
// Canonical Nullptr values become the reserved QDBusSignature scalar documented
// by Settings1; the decoder performs the inverse operation. Invalid QVariant
// and caller-supplied QDBusSignature are rejected rather than confused with null.
[[nodiscard]] std::optional<QVariant> encodeBoundedJsonValueForWire(
    const QVariant &value,
    QString *error = nullptr,
    BoundedSettingsValueCodec::Usage *usage = nullptr);
[[nodiscard]] std::optional<QVariant> encodeBoundedJsonValueForWire(
    const QVariant &value,
    AggregateValueDecodeBudget &aggregate,
    QString *error = nullptr,
    BoundedSettingsValueCodec::Usage *usage = nullptr);

// Validates the complete public transport operation envelope and replaces
// every set value with its bounded wire representation before QtDBus sees it.
// A failure returns no partially encoded transaction.
[[nodiscard]] std::optional<QVariantList> encodeBoundedOperationsForWire(
    const QVariantList &operations,
    QString *error = nullptr);

} // namespace QindaQt::Services::SettingsProtocol
