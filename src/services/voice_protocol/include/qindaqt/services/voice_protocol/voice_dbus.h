// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/voice_protocol/voice_types.h>

#include <QtCore/QVariantList>
#include <QtCore/QVariantMap>

namespace QindaQt::Services::Voice {

// The Voice1 payload is a string-keyed variant map, not a D-Bus structure.
//
// AGENT-CONTRACT (ADR-0187): the provider is deliberately implementable in any
// language. A positional structure signature would require every provider to
// register a C++-shaped custom type, which is not reachable from PyQt or from
// most bindings; a{sv} is. The strictness a structure would have given is
// recovered on decode: an unexpected key, a missing key, or a key of the wrong
// type marks the payload invalid, and validateSnapshot() then rejects it.
[[nodiscard]] QVariantMap encodeSnapshot(const Snapshot &snapshot);
[[nodiscard]] Snapshot decodeSnapshot(const QVariantMap &payload);

[[nodiscard]] QVariantMap encodeOperationResult(const OperationResult &result);
[[nodiscard]] OperationResult decodeOperationResult(const QVariantMap &payload);

// The D-Bus member a request kind is submitted through, and the positional
// arguments that member takes. An unrecognised kind yields an empty name,
// which callers must treat as a rejected request.
[[nodiscard]] QString methodNameForKind(OperationKind kind);
[[nodiscard]] QVariantList argumentsForRequest(const OperationRequest &request);

} // namespace QindaQt::Services::Voice
