// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <qindaqt/services/power_protocol/power_types.h>
#include <QtCore/QByteArray>

namespace QindaQt::Power {
inline constexpr quint32 kPeripheralSchemaVersion = 1;
inline constexpr qsizetype kMaxPeripheralBatteries = 64;
inline constexpr qsizetype kMaxPeripheralPayloadBytes = 131072;
// These are QindaQt values, never upstream enum ordinals. Unknown upstream
// kinds retain a row as Other; they never become a system power supply.
enum class PeripheralKind : quint32 {
    Other, Mouse, Keyboard, Controller, Headset, Speaker, Headphones,
    Phone, Tablet, Computer, Pen, Touchpad, MediaPlayer, Remote, Camera, Wearable
};
struct PeripheralBattery {
    Handle handle;
    PeripheralKind kind = PeripheralKind::Other;
    QString vendor;
    QString model;
    bool present = true;
    bool percentageKnown = false;
    double percentage = 0.0;
    BatteryLevel level = BatteryLevel::Unknown;
    ChargeState state = ChargeState::Unknown;
    bool timeToEmptyKnown = false;
    qint64 timeToEmptySeconds = 0;
    bool timeToFullKnown = false;
    qint64 timeToFullSeconds = 0;
    friend bool operator==(const PeripheralBattery &, const PeripheralBattery &) = default;
};
struct PeripheralSnapshot {
    quint32 schemaVersion = kPeripheralSchemaVersion;
    quint64 epoch = 0;
    quint64 revision = 0;
    Availability availability = Availability::Starting;
    QString reasonCode;
    // Known omitted rows include malformed/unreadable rows. truncated says the
    // independent 64-row presentation bound was reached, never silent success.
    quint32 omittedCount = 0;
    bool truncated = false;
    QList<PeripheralBattery> devices;
    friend bool operator==(const PeripheralSnapshot &, const PeripheralSnapshot &) = default;
};
// Copied, reentrant values; validation never repairs. Codecs reject oversize
// lengths before allocation and leave the destination untouched on failure.
// This is a separate read-only wire schema; Power1 Snapshot/aggregate is frozen.
[[nodiscard]] bool validatePeripheralSnapshot(const PeripheralSnapshot &value);
[[nodiscard]] bool encodePeripheralSnapshot(const PeripheralSnapshot &value, QByteArray &output);
[[nodiscard]] bool decodePeripheralSnapshot(const QByteArray &input, PeripheralSnapshot &output);
} // namespace QindaQt::Power
Q_DECLARE_METATYPE(QindaQt::Power::PeripheralSnapshot)
