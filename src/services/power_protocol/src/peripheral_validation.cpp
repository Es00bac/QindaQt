// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/power_protocol/peripheral_types.h>
#include <qindaqt/services/power_protocol/power_validation.h>
#include <QtCore/QSet>
#include <cmath>
namespace QindaQt::Power {
bool validatePeripheralSnapshot(const PeripheralSnapshot &value)
{
    if (value.schemaVersion != kPeripheralSchemaVersion || value.epoch == 0
        || value.revision == 0 || static_cast<quint32>(value.availability) > 3
        || !isBoundedText(value.reasonCode, 64) || value.omittedCount > 4096
        || value.devices.size() > kMaxPeripheralBatteries
        || (value.truncated && value.omittedCount == 0)) return false;
    if ((value.availability == Availability::Starting
         || value.availability == Availability::Unavailable) && !value.devices.isEmpty()) return false;
    if ((value.omittedCount > 0 || value.truncated) && value.availability != Availability::Degraded) return false;
    QSet<QString> ids;
    for (const auto &device : value.devices) {
        if (device.handle.epoch != value.epoch || device.handle.opaqueId.isEmpty()
            || !isBoundedText(device.handle.opaqueId, 128) || ids.contains(device.handle.opaqueId)
            || static_cast<quint32>(device.kind) > static_cast<quint32>(PeripheralKind::Wearable)
            || !isBoundedText(device.vendor, 256) || !isBoundedText(device.model, 256)
            || static_cast<quint32>(device.level) > 6 || static_cast<quint32>(device.state) > 6
            || !std::isfinite(device.percentage) || device.percentage < 0 || device.percentage > 100
            || (!device.percentageKnown && device.percentage != 0)
            || (device.percentageKnown && device.level != BatteryLevel::None)) return false;
        const auto validTime = [](bool known, qint64 seconds) {
            return known ? seconds > 0 && seconds <= 315360000 : seconds == 0;
        };
        if (!validTime(device.timeToEmptyKnown, device.timeToEmptySeconds)
            || !validTime(device.timeToFullKnown, device.timeToFullSeconds)) return false;
        ids.insert(device.handle.opaqueId);
    }
    return true;
}
} // namespace QindaQt::Power
