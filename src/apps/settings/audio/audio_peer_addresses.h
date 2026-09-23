// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QtCore/QStringList>

namespace QindaQt::Apps::SettingsAudio {

// Current, active non-loopback IPv4 candidates. The person chooses the
// address the other computer can reach; this does not discover a peer.
[[nodiscard]] QStringList localPeerIpv4Addresses();

} // namespace QindaQt::Apps::SettingsAudio
