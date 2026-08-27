// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/services/settings_protocol/settings_value_codec.h"

#include <QVariant>

#include <optional>

namespace QindaQt::Services::SettingsProtocol {

// One aggregate budget is shared by every value in a snapshot or transaction.
// decodeBoundedJsonValue updates it only after the complete value succeeds, so
// callers can reject an envelope without retaining partial accounting.
struct AggregateValueDecodeBudget final {
    qsizetype maximumBytes = 0;
    qsizetype maximumNodes = 0;
    qsizetype bytes = 0;
    qsizetype nodes = 0;
};

// AGENT-CONTRACT: a QVariant carrying a D-Bus array/map argument decodes
// differently depending on whether it arrived over a real bus connection
// (an opaque QDBusArgument that must be streamed out explicitly) or was
// constructed in-process for a test/offscreen caller (an ordinary
// QVariantList/QVariantMap already). Both call sites -- the service decoding
// an incoming CommitUserTransaction request and the client decoding a reply
// -- need the same bounded handling of both shapes, so it lives here rather
// than being duplicated in settings_service and settings_client.
[[nodiscard]] std::optional<QVariantList> decodeBoundedVariantList(const QVariant &value,
                                                                    qsizetype maximumElements);
[[nodiscard]] std::optional<QVariantMap> decodeBoundedVariantMap(const QVariant &value,
                                                                  qsizetype maximumEntries);

// Decodes a protocol key list and rejects over-bound, malformed, or duplicate
// keys. This is shared by commit replies and SettingsChanged invalidations.
[[nodiscard]] std::optional<QStringList> decodeBoundedKeyList(const QVariant &value,
                                                               qsizetype maximumElements);

// Converts an ordinary QVariant or an opaque QtDBus `variant` container into
// one recursively JSON-native QVariant shape. Nodes, UTF-8 bytes, depth, list
// entries, map entries, keys, and strings are charged before the decoded child
// is appended/inserted. The aggregate overload additionally shares one
// snapshot/transaction budget across values.
[[nodiscard]] std::optional<QVariant> decodeBoundedJsonValue(const QVariant &value,
                                                              QString *error = nullptr,
                                                              BoundedSettingsValueCodec::Usage *usage = nullptr);
[[nodiscard]] std::optional<QVariant> decodeBoundedJsonValue(
    const QVariant &value,
    AggregateValueDecodeBudget &aggregate,
    QString *error = nullptr,
    BoundedSettingsValueCodec::Usage *usage = nullptr);

} // namespace QindaQt::Services::SettingsProtocol
