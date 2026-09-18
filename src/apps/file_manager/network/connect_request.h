// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "network_locations_store.h"

#include <QString>
#include <QStringList>

namespace QindaQt::Apps::FileManager {

// What the Connect-to-server dialog collected, verbatim and untrusted.
// There is deliberately no user-name and no password field: a saved location
// never carries userinfo (NetworkLocation's ADR-0137 guard), and QindaQt
// writes no secret of its own -- the platform's KIO credential prompt owns
// both (ADR-0196).
struct ConnectRequest final {
  QString scheme;
  QString host;
  // Empty means "the scheme's default port".
  QString port;
  QString remotePath;
  // Empty means "derive a readable name from the address".
  QString displayName;
  bool showInPlaces = true;
  // ADR-0199. Refused for anything but sftp, since sshfs is the only mount
  // this knob knows.
  bool mountAtLogin = false;
};

enum class ConnectRequestError {
  None,
  UnsupportedScheme,
  MissingHost,
  InvalidHost,
  InvalidPort,
  InvalidPath,
  InvalidName,
  MountUnsupported,
};

struct ConnectRequestResult final {
  NetworkLocationRecord record;
  ConnectRequestError error = ConnectRequestError::None;
  // Bounded, human-readable, and safe to show verbatim: it repeats nothing
  // the user typed beyond what they can already see in the dialog.
  QString message;

  [[nodiscard]] bool ok() const { return error == ConnectRequestError::None; }
};

// AGENT-CONTRACT: the one place dialog text becomes a saved location. Pure
// policy -- it performs no I/O, resolves no host, contacts no server, and
// reads no environment, so every refusal is deterministic and testable.
// A successful result's URL is exactly what NetworkLocation::canonicalize()
// produced, which is what NetworkLocationsStore then re-proves.
[[nodiscard]] ConnectRequestResult buildNetworkLocation(const ConnectRequest &request);

// The schemes the dialog offers, in presentation order.
[[nodiscard]] QStringList connectableSchemes();

} // namespace QindaQt::Apps::FileManager
