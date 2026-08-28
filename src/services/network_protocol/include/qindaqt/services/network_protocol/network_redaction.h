// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/network_protocol/network_types.h>

#include <QtCore/QString>
#include <QtCore/QStringView>
#include <QtCore/QVariantMap>

namespace QindaQt::Network {

// AGENT-CONTRACT: No Network1 value, diagnostic, or intent may carry a
// credential. These helpers are the single shared authority for recognizing
// secret-shaped material; the model, client, and every later adapter must
// route free text through redactDiagnostic before publication and reject
// intent payloads containing secret-shaped keys.

// True when a wire/intent key name denotes credential material by exact or
// suffix match (case-insensitive). The set is deliberately small and flat:
// passphrase, password, psk, secret, private-key, client-key, 8021x-password,
// agent-owned secret-field spellings.
[[nodiscard]] bool isSecretKeyName(QStringView key);

// True when a nested wire map contains any secret-shaped key. Recursion is
// depth-bounded so hostile nesting cannot exhaust the stack.
[[nodiscard]] bool wireContainsSecrets(const QVariantMap &wire);

// Bounded, presentation-safe diagnostic text. Secret-shaped `key=value`
// fragments and `key: value` fragments are replaced with `key=<redacted>`.
// Unsafe Unicode fails closed to a fixed public diagnostic, and every result
// is clamped to the exact v1 UTF-8 byte cap.
[[nodiscard]] QString redactDiagnostic(QString message);

} // namespace QindaQt::Network
