// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/network_protocol/network_types.h>

#include <QtCore/QByteArrayView>
#include <QtCore/QString>

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

[[nodiscard]] SsidIdentity normalizeSsid(QByteArrayView rawSsid);

// BSSID normalization: exactly seventeen `xx:xx:xx:xx:xx:xx` lowercase hex
// characters after accepting case-insensitive separators.
[[nodiscard]] bool normalizeBssid(const QString &rawBssid, QString *normalized);

// Interface-name normalization: 1..15 octets of [A-Za-z0-9._-] starting with
// an alphanumeric character, matching Linux IFNAMSIZ constraints.
[[nodiscard]] bool normalizeInterfaceName(const QString &rawName,
                                          QString *normalized);

// Privacy-preserving stable known-network identity: the first 32 hex
// characters of SHA-256 over the raw SSID octets and the security suite.
// The digest never reveals the SSID text and is stable across scans, so UI
// and intent only ever exchange opaque ids, never credentials.
[[nodiscard]] QString knownNetworkId(QByteArrayView rawSsid,
                                     SecuritySuite security);

} // namespace QindaQt::Network
