// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <qindaqt/services/power_protocol/peripheral_types.h>
#include <QtCore/QVariantMap>
namespace QindaQt::Power::Upstream {
// Separate failure domain: malformed peripherals never invalidate system supply truth.
enum class PeripheralDecode { Ignored, Accepted, Malformed };
PeripheralDecode decodePeripheral(const QString &path, const QVariantMap &properties,
                                  PeripheralBattery &result);
}
