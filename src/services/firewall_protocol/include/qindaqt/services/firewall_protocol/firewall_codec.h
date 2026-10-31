// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <qindaqt/services/firewall_protocol/firewall_types.h>

#include <QtCore/QByteArray>
#include <QtCore/QList>

namespace QindaQt::Firewall {

// Decodes the `Headline` property's JSON object `{level, title, text}`.
// A malformed or non-object payload leaves `*out` default-constructed
// (level Unknown) and returns false; callers must never guess a level from
// local state when decoding fails.
[[nodiscard]] bool decodeHeadline(const QByteArray &json, Headline *out);

// Decodes one SecurityMonitor alert map, as `TopAlert` or `Alert(id)`
// return it. An empty payload is the documented "no alert" value and is not
// a decode error in the caller-visible sense, but it still returns false
// with `*out` left default-constructed, exactly like a malformed payload:
// callers branch on the return value, never on the input text.
[[nodiscard]] bool decodeAlert(const QByteArray &json, Alert *out);

// Decodes the JSON array `Alerts()` returns (most serious first), preserving
// order. One malformed element is skipped rather than failing the whole
// list, so a single bad entry never hides every other open alert.
[[nodiscard]] QList<Alert> decodeAlertList(const QByteArray &json);

} // namespace QindaQt::Firewall
