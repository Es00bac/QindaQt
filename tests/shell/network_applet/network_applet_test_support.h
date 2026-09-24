// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// AGENT-NOTE: the Network1 fake transport and the canonical ready snapshot
// are the ones the Settings Network route tests already use, so the applet
// is exercised against exactly the same public client seam.
#include "network_settings_test_support.h"

#include <qindaqt/services/network_protocol/network_identity.h>

namespace QindaQt::Shell::NetworkApplet::TestSupport
{

using namespace QindaQt::Apps::SettingsNetwork::TestSupport;

inline const QString kOwner = QStringLiteral(":1.20");

inline QString homeId() { return networkId(u'a'); }
inline QString cafeId() { return networkId(u'b'); }
inline QString guestKnownId() { return networkId(u'c'); }

inline QString pointId(const QString &bssid)
{
    return Network::visibleAccessPointId(QStringLiteral("wlan0"), bssid);
}

// Home (WPA3, 88, saved, active), Guest (open, 61), Cafe (open, 52, saved),
// Secure Guest (WPA2, 48), on one Wi-Fi device plus an idle Ethernet port.
inline Network::Snapshot appletSnapshot(const quint64 revision = 1,
                                        const bool radioControl = true)
{
    Network::Snapshot snapshot = readySnapshot(kOwner, 20, revision);
    if (radioControl) {
        snapshot.capabilities |= Network::Capability::RadioControl;
    }
    return snapshot;
}

inline QString homePoint() { return pointId(QStringLiteral("00:11:22:33:44:55")); }
inline QString cafePoint() { return pointId(QStringLiteral("66:77:88:99:aa:bb")); }
inline QString guestPoint() { return pointId(QStringLiteral("66:77:88:99:aa:cc")); }
inline QString secureGuestPoint() { return pointId(QStringLiteral("66:77:88:99:aa:dd")); }

} // namespace QindaQt::Shell::NetworkApplet::TestSupport
