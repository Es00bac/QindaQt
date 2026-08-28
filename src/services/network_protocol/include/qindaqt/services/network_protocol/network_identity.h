// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/network_protocol/network_types.h>

#include <QtCore/QByteArrayView>
#include <QtCore/QString>
#include <QtCore/QStringView>

namespace QindaQt::Network {

// Raw 802.11 SSID octets decoded to a presentation-safe truth. An SSID that
// is not strictly printable UTF-8 without control characters is kept in the
// snapshot only as `hidden` with empty text; the octets themselves are never
// published. An empty SSID is a hidden network by definition.
struct SsidIdentity {
  QString text;
  bool hidden = false;
  bool valid = false;

  friend bool operator==(const SsidIdentity &, const SsidIdentity &) = default;
};

// Shared presentation-text predicate for Network1. It accepts well-formed
// printable Unicode, including supplementary characters, and rejects control,
// formatting, surrogate, private-use, unassigned, line-separator, and
// paragraph-separator scalars. SSID normalization and snapshot validation must
// use this same predicate so an adapter cannot bypass anti-spoof policy by
// constructing a QString directly.
[[nodiscard]] bool isPresentationSafeText(QStringView text);

[[nodiscard]] SsidIdentity normalizeSsid(QByteArrayView rawSsid);

// BSSID normalization: exactly seventeen `xx:xx:xx:xx:xx:xx` lowercase hex
// characters after accepting case-insensitive separators.
[[nodiscard]] bool normalizeBssid(const QString &rawBssid, QString *normalized);

// Interface-name normalization: 1..15 octets of [A-Za-z0-9._-] starting with
// an alphanumeric character, matching Linux IFNAMSIZ constraints.
[[nodiscard]] bool normalizeInterfaceName(const QString &rawName,
                                          QString *normalized);

// Stable pseudonymous known-network identity: the complete lowercase SHA-256
// digest over the raw SSID octets and security suite. This avoids transporting
// SSID text in intents, but the unsalted digest is not confidentiality against
// offline guessing of low-entropy SSIDs. It never contains credentials.
[[nodiscard]] QString knownNetworkId(QByteArrayView rawSsid,
                                     SecuritySuite security);

} // namespace QindaQt::Network
