// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/power_protocol/power_types.h>

#include <QtCore/QString>
#include <QtCore/QVariantMap>

namespace QindaQt::Power::Upstream {

struct UpowerDeviceTruth {
    bool acPresent = false;
    bool hasSupply = false;
    PowerSupply supply;
};

// Converts one complete org.freedesktop.UPower.Device property map without
// retaining its object path. AGENT-CONTRACT: Online is authoritative only for
// line power, PowerSupply gates battery/UPS admission, and IsPresent is read
// only for batteries, matching org.freedesktop.UPower.Device.xml. Missing
// optional values become canonical unknown truth; a present value with the
// wrong D-Bus type or an unknown enum fails the complete device.
[[nodiscard]] bool decodeUpowerDevice(const QString &objectPath,
                                      const QVariantMap &properties,
                                      UpowerDeviceTruth &truth);

} // namespace QindaQt::Power::Upstream
