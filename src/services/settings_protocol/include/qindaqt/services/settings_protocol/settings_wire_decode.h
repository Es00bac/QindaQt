// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QVariant>

#include <optional>

namespace QindaQt::Services::SettingsProtocol {

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

// Converts an ordinary QVariant or an opaque QtDBus `variant` container into
// the same recursively JSON-native QVariant shape validated by the shared
// codec. Aggregate node/byte/depth limits are applied after decoding.
[[nodiscard]] std::optional<QVariant> decodeBoundedJsonValue(const QVariant &value,
                                                              QString *error = nullptr);

} // namespace QindaQt::Services::SettingsProtocol
