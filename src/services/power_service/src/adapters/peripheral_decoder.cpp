// SPDX-License-Identifier: GPL-3.0-or-later
#include "peripheral_decoder_p.h"
#include "upstream_identity.h"
#include <qindaqt/services/power_protocol/power_validation.h>
namespace QindaQt::Power::Upstream {
namespace {
template<class T> bool read(const QVariantMap &p, const char *key, T &out, bool required = false)
{
    const auto it = p.constFind(QString::fromLatin1(key));
    if (it == p.cend()) return !required;
    if (it->metaType() != QMetaType::fromType<T>()) return false;
    out = it->value<T>(); return true;
}
PeripheralKind kind(uint type)
{
    switch(type) {
    case 5: return PeripheralKind::Mouse;
    case 6: return PeripheralKind::Keyboard;
    case 8: return PeripheralKind::Phone;
    case 9: return PeripheralKind::MediaPlayer;
    case 10: return PeripheralKind::Tablet;
    case 11: return PeripheralKind::Computer;
    case 12: return PeripheralKind::Controller;
    case 13: return PeripheralKind::Pen;
    case 14: return PeripheralKind::Touchpad;
    case 17: return PeripheralKind::Headset;
    case 18: return PeripheralKind::Speaker;
    case 19: return PeripheralKind::Headphones;
    case 22: return PeripheralKind::Remote;
    case 25: return PeripheralKind::Camera;
    case 26: return PeripheralKind::Wearable;
    default: return PeripheralKind::Other;
    }
}
}
PeripheralDecode decodePeripheral(const QString &path, const QVariantMap &p,
                                  PeripheralBattery &result)
{
    uint type = 0; bool supply = false;
    if (!read(p,"Type",type,true)) return PeripheralDecode::Malformed;
    if (type == 1) return PeripheralDecode::Ignored;
    if (!read(p,"PowerSupply",supply,true)) return PeripheralDecode::Malformed;
    if (supply) return PeripheralDecode::Ignored;
    PeripheralBattery row;
    row.handle = {1,deriveOpaqueId(QStringLiteral("upower-peripheral"),path)};
    row.kind = kind(type);
    uint state = 0, level = 0;
    double percentage = 0;
    qint64 empty = 0, full = 0;
    if (!read(p,"IsPresent",row.present) || !read(p,"Vendor",row.vendor)
        || !read(p,"Model",row.model) || !read(p,"State",state)
        || !read(p,"BatteryLevel",level) || !read(p,"Percentage",percentage)
        || !read(p,"TimeToEmpty",empty) || !read(p,"TimeToFull",full)
        || state > 6 || level > 6) return PeripheralDecode::Malformed;
    row.vendor = sanitizeText(row.vendor,256);
    row.model = sanitizeText(row.model,256);
    row.state = static_cast<ChargeState>(state);
    row.level = static_cast<BatteryLevel>(level);
    if (p.contains(QStringLiteral("Percentage")) && level <= 1) {
        row.percentageKnown = true; row.percentage = percentage; row.level = BatteryLevel::None;
    }
    row.timeToEmptyKnown = empty != 0; row.timeToEmptySeconds = empty;
    row.timeToFullKnown = full != 0; row.timeToFullSeconds = full;
    PeripheralSnapshot check;
    check.epoch=1; check.revision=1; check.availability=Availability::Ready;
    check.devices.push_back(row);
    if (!validatePeripheralSnapshot(check)) return PeripheralDecode::Malformed;
    result = std::move(row); return PeripheralDecode::Accepted;
}
}
