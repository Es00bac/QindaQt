// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/network_protocol/network_types.h>

#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <QtCore/QString>

namespace QindaQt::Network::Model {

// AGENT-CONTRACT: ModelState is the atomic, consumer-facing projection of one
// accepted Network1 snapshot plus lease truth. It is either derived completely
// from an accepted snapshot or reports no-snapshot truth; it is never a mix of
// two snapshots. Every free-text field is redacted through the protocol
// redaction helper before publication.
struct ModelState {
  bool hasSnapshot = false;
  QString owner;
  quint64 epoch = 0;
  quint64 revision = 0;
  Availability availability = Availability::Unavailable;
  QString reasonCode;
  QString diagnostic;
  ConnectivityKind connectivity = ConnectivityKind::Unknown;
  QList<Radio> radios;
  QList<Device> devices;
  QList<AccessPoint> accessPoints;
  QList<KnownNetwork> knownNetworks;
  QList<ActiveConnection> activeConnections;
  ScanPhase scanPhase = ScanPhase::Idle;
  bool scanBusy = false;
  bool scanLeaseExpired = false;
  qint64 scanLeaseRemainingMs = 0;
  bool scanCapable = false;
  bool radioControlCapable = false;

  friend bool operator==(const ModelState &, const ModelState &) = default;
};

// Presentation-safe name for an access point: hidden networks never expose
// their SSID text and are announced as such instead.
[[nodiscard]] QString accessPointDisplayName(const AccessPoint &point);

} // namespace QindaQt::Network::Model

Q_DECLARE_METATYPE(QindaQt::Network::Model::ModelState)
