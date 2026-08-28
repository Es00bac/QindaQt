// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/network_protocol/network_types.h>

#include <QtCore/QMetaType>
#include <QtCore/QString>

namespace QindaQt::Network {

// User intents are the only mutation inputs N0 understands. Each intent is a
// value; the model validates it against the current snapshot and lease state
// before a client may transport it. No intent can carry credentials.
struct RequestScanIntent {
  qint64 deadlineMilliseconds = 30'000;

  friend bool operator==(const RequestScanIntent &,
                         const RequestScanIntent &) = default;
};

struct ConnectIntent {
  QString knownNetworkId;

  friend bool operator==(const ConnectIntent &, const ConnectIntent &) = default;
};

struct DisconnectIntent {
  QString deviceInterface;

  friend bool operator==(const DisconnectIntent &,
                         const DisconnectIntent &) = default;
};

struct SetRadioIntent {
  RadioKind kind = RadioKind::Wifi;
  bool enable = false;

  friend bool operator==(const SetRadioIntent &, const SetRadioIntent &) = default;
};

} // namespace QindaQt::Network

Q_DECLARE_METATYPE(QindaQt::Network::RequestScanIntent)
Q_DECLARE_METATYPE(QindaQt::Network::ConnectIntent)
Q_DECLARE_METATYPE(QindaQt::Network::DisconnectIntent)
Q_DECLARE_METATYPE(QindaQt::Network::SetRadioIntent)
