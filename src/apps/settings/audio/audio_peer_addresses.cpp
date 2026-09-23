// SPDX-License-Identifier: LGPL-3.0-or-later
#include "audio_peer_addresses.h"

#include <QtNetwork/QHostAddress>
#include <QtNetwork/QNetworkAddressEntry>
#include <QtNetwork/QNetworkInterface>

#include <algorithm>

namespace QindaQt::Apps::SettingsAudio {

QStringList localPeerIpv4Addresses() {
  QStringList result;
  for (const QNetworkInterface &network : QNetworkInterface::allInterfaces()) {
    const auto flags = network.flags();
    if (!flags.testFlag(QNetworkInterface::IsUp)
        || !flags.testFlag(QNetworkInterface::IsRunning)
        || flags.testFlag(QNetworkInterface::IsLoopBack))
      continue;
    for (const QNetworkAddressEntry &entry : network.addressEntries()) {
      const QHostAddress address = entry.ip();
      if (address.protocol() != QAbstractSocket::IPv4Protocol
          || address.isLoopback() || address.isMulticast()) continue;
      const QString text = address.toString();
      if (text == QStringLiteral("0.0.0.0")
          || text == QStringLiteral("255.255.255.255")) continue;
      if (!result.contains(text)) result.append(text);
    }
  }
  std::sort(result.begin(), result.end());
  return result;
}

} // namespace QindaQt::Apps::SettingsAudio
