// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "radio_platform_p.h"
#include <QtCore/QHash>
#include <QtCore/QPair>
#include <functional>

namespace QindaQt::BluetoothRadio {
// Same-thread private ports, borrowed for the operation engine's lifetime.
// current() independently validates bus-derived caller/current service owner,
// selected exact BlueZ owner/object/address and deadline. It never writes.
class RadioAuthority {
public:
    virtual ~RadioAuthority() = default;
    virtual bool current(const QString &sender, const Request &request) = 0;
};
class RadioOperation final {
public:
    RadioOperation(RadioAuthority &authority, RadioPlatform &platform,
        std::function<quint64()> clock = boottimeMilliseconds);
    Result execute(const QString &sender, const Request &request);
private:
    bool admitted(const QString &sender, const Request &request);
    RadioAuthority &m_authority;
    RadioPlatform &m_platform;
    std::function<quint64()> m_clock;
    // Unique owners may relinquish and reacquire an alias without exiting.
    // Never forget an unexpired issued nonce merely because another owner acts.
    QHash<QPair<QString, QString>, quint64> m_seen;
};
} // namespace QindaQt::BluetoothRadio
