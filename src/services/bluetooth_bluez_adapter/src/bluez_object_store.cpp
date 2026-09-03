// SPDX-License-Identifier: LGPL-3.0-or-later

#include "bluez_object_store.h"

#include <qindaqt/services/bluetooth_protocol/bluetooth_limits.h>
#include <qindaqt/services/bluetooth_protocol/bluetooth_types.h>
#include <qindaqt/services/bluetooth_protocol/bluetooth_validation.h>

#include <algorithm>
#include <QtDBus/QDBusObjectPath>

namespace QindaQt::Bluetooth::Bluez
{
namespace
{

constexpr QLatin1StringView kAdapterInterface{"org.bluez.Adapter1"};
constexpr QLatin1StringView kDeviceInterface{"org.bluez.Device1"};
constexpr qsizetype kMaxIconCharacters = 64;

// Bounded, control-safe display text. Embedded nulls are removed, unexpected
// control characters become spaces, and truncation never splits a UTF-8
// sequence. This is sanitization of hostile platform payloads, not
// fabrication: nothing new is added.
QString boundedName(QString value, const qsizetype maxUtf8Bytes)
{
    value.remove(QChar::Null);
    for (QChar &character : value) {
        if (character.category() == QChar::Other_Control
            && character != QLatin1Char('\n') && character != QLatin1Char('\t')) {
            character = QLatin1Char(' ');
        }
    }
    QByteArray bytes = value.toUtf8();
    if (bytes.size() <= maxUtf8Bytes) {
        return value;
    }
    bytes.truncate(maxUtf8Bytes);
    while (!bytes.isEmpty() && (bytes.back() & 0xC0) == 0x80) {
        bytes.chop(1);
    }
    if (!bytes.isEmpty() && (bytes.back() & 0xC0) == 0xC0) {
        bytes.chop(1);
    }
    return QString::fromUtf8(bytes);
}

// Decodes the 24-bit Bluetooth Class of Device. Anything outside the CoD
// space, with a reserved format type, or without a representable
// Bluetooth1 enum value yields Unknown rather than a guess.
DeviceClass deviceClassFromCod(const quint32 cod)
{
    if (cod == 0 || cod > 0xFFFFFF || (cod & 0x3U) != 0) {
        return DeviceClass::Unknown;
    }
    const quint32 major = (cod >> 8U) & 0x1FU;
    const quint32 minor = (cod >> 2U) & 0x3FU;
    switch (major) {
    case 1:
        return DeviceClass::Computer;
    case 2:
        return DeviceClass::Phone;
    case 4:
        if (minor == 0x01U || minor == 0x02U) {
            return DeviceClass::Headset;
        }
        if (minor == 0x10U) {
            return DeviceClass::Headphones;
        }
        return DeviceClass::AudioVideo;
    case 5:
        if ((minor & 0x40U) != 0) {
            return DeviceClass::Keyboard;
        }
        if ((minor & 0x80U) != 0) {
            return DeviceClass::Mouse;
        }
        if (minor == 0x01U || minor == 0x02U) {
            return DeviceClass::GameInput;
        }
        if (minor == 0x05U) {
            return DeviceClass::Tablet;
        }
        return DeviceClass::Unknown;
    case 6:
        return (minor & 0x20U) != 0 ? DeviceClass::Printer : DeviceClass::Unknown;
    case 7:
        return DeviceClass::Wearable;
    default:
        return DeviceClass::Unknown;
    }
}

// BlueZ proposes freedesktop.org icon names; only the curated set below maps
// to a Bluetooth1 device class. Unknown icon strings never invent a class.
DeviceClass deviceClassFromIcon(const QString &icon)
{
    if (icon == QLatin1String("computer")) {
        return DeviceClass::Computer;
    }
    if (icon == QLatin1String("phone")) {
        return DeviceClass::Phone;
    }
    if (icon == QLatin1String("audio-headset") || icon == QLatin1String("headset")) {
        return DeviceClass::Headset;
    }
    if (icon == QLatin1String("audio-headphones")
        || icon == QLatin1String("headphones")) {
        return DeviceClass::Headphones;
    }
    if (icon == QLatin1String("audio-card") || icon == QLatin1String("camera-photo")
        || icon == QLatin1String("camera-video")
        || icon == QLatin1String("video-display")
        || icon == QLatin1String("multimedia-player")) {
        return DeviceClass::AudioVideo;
    }
    if (icon == QLatin1String("input-keyboard")) {
        return DeviceClass::Keyboard;
    }
    if (icon == QLatin1String("input-mouse")) {
        return DeviceClass::Mouse;
    }
    if (icon == QLatin1String("input-tablet")) {
        return DeviceClass::Tablet;
    }
    if (icon == QLatin1String("input-gaming")) {
        return DeviceClass::GameInput;
    }
    if (icon == QLatin1String("printer")) {
        return DeviceClass::Printer;
    }
    if (icon == QLatin1String("watch") || icon == QLatin1String("wearable")) {
        return DeviceClass::Wearable;
    }
    return DeviceClass::Unknown;
}

// Property strings may arrive as plain D-Bus strings or as object paths
// (BlueZ types Device1.Adapter as 'o'); both spell the same path value.
QString propertyString(const QVariant &value)
{
    if (value.metaType() == QMetaType::fromType<QDBusObjectPath>()) {
        return value.value<QDBusObjectPath>().path();
    }
    return value.canConvert<QString>() ? value.toString() : QString{};
}

QString canonicalAddressOrEmpty(const QVariantMap &properties)
{
    const QString address = propertyString(properties.value(QStringLiteral("Address"))).toUpper();
    return isCanonicalAddress(address) ? address : QString{};
}

} // namespace

void BluezObjectStore::replaceFrom(const BluezManagedObjects &objects)
{
    m_adapters.clear();
    m_devices.clear();
    for (auto objectIt = objects.cbegin(); objectIt != objects.cend(); ++objectIt) {
        if (objectIt.value().contains(kAdapterInterface)) {
            upsertAdapter(objectIt.key(), objectIt.value().value(kAdapterInterface));
        }
        if (objectIt.value().contains(kDeviceInterface)) {
            upsertDevice(objectIt.key(), objectIt.value().value(kDeviceInterface));
        }
    }
}

void BluezObjectStore::upsertInterfaces(const QString &path,
                                        const BluezInterfaces &interfaces)
{
    if (interfaces.contains(kAdapterInterface)) {
        upsertAdapter(path, interfaces.value(kAdapterInterface));
    }
    if (interfaces.contains(kDeviceInterface)) {
        upsertDevice(path, interfaces.value(kDeviceInterface));
    }
}

void BluezObjectStore::removeInterfaces(const QString &path,
                                        const QList<QString> &interfaces)
{
    if (interfaces.contains(kAdapterInterface)) {
        dropAdapter(path);
    }
    if (interfaces.contains(kDeviceInterface)) {
        m_devices.remove(path);
    }
}

void BluezObjectStore::dropAdapter(const QString &path)
{
    m_adapters.remove(path);
    for (auto it = m_devices.begin(); it != m_devices.end();) {
        if (it.value().adapterPath == path) {
            it = m_devices.erase(it);
        } else {
            ++it;
        }
    }
}

void BluezObjectStore::clear()
{
    m_adapters.clear();
    m_devices.clear();
}

const BluezAdapterState *BluezObjectStore::adapter(const QString &path) const
{
    const auto it = m_adapters.constFind(path);
    return it == m_adapters.cend() ? nullptr : &it.value();
}

const BluezDeviceState *BluezObjectStore::device(const QString &path) const
{
    const auto it = m_devices.constFind(path);
    return it == m_devices.cend() ? nullptr : &it.value();
}

const BluezAdapterState *BluezObjectStore::adapterByAddress(
    const QString &address) const
{
    for (auto it = m_adapters.cbegin(); it != m_adapters.cend(); ++it) {
        if (it.value().address == address) {
            return &it.value();
        }
    }
    return nullptr;
}

const BluezDeviceState *BluezObjectStore::deviceByAddress(
    const QString &address) const
{
    for (auto it = m_devices.cbegin(); it != m_devices.cend(); ++it) {
        if (it.value().address == address) {
            return &it.value();
        }
    }
    return nullptr;
}

void BluezObjectStore::applyPowered(const QString &adapterPath, const bool powered)
{
    const auto it = m_adapters.find(adapterPath);
    if (it != m_adapters.end()) {
        it.value().powered = powered;
    }
}

void BluezObjectStore::applyConnected(const QString &devicePath, const bool connected)
{
    const auto it = m_devices.find(devicePath);
    if (it != m_devices.end()) {
        it.value().connected = connected;
    }
}

void BluezObjectStore::upsertAdapter(const QString &path,
                                     const QVariantMap &properties)
{
    BluezAdapterState record = m_adapters.value(path);
    record.path = path;
    if (properties.contains(QStringLiteral("Address"))) {
        record.address = canonicalAddressOrEmpty(properties);
    }
    if (properties.contains(QStringLiteral("Alias"))) {
        record.name = boundedName(properties.value(QStringLiteral("Alias")).toString(),
                                  kMaxAdapterNameUtf8Bytes);
    } else if (properties.contains(QStringLiteral("Name")) && record.name.isEmpty()) {
        record.name = boundedName(properties.value(QStringLiteral("Name")).toString(),
                                  kMaxAdapterNameUtf8Bytes);
    }
    if (properties.contains(QStringLiteral("Powered"))) {
        record.powered = properties.value(QStringLiteral("Powered")).toBool();
    }
    if (properties.contains(QStringLiteral("Discovering"))) {
        record.discovering = properties.value(QStringLiteral("Discovering")).toBool();
    }
    m_adapters.insert(path, record);
}

void BluezObjectStore::upsertDevice(const QString &path,
                                    const QVariantMap &properties)
{
    BluezDeviceState record = m_devices.value(path);
    record.path = path;
    if (properties.contains(QStringLiteral("Address"))) {
        record.address = canonicalAddressOrEmpty(properties);
    }
    if (properties.contains(QStringLiteral("Alias"))) {
        record.name = boundedName(properties.value(QStringLiteral("Alias")).toString(),
                                  kMaxDeviceNameUtf8Bytes);
    } else if (properties.contains(QStringLiteral("Name")) && record.name.isEmpty()) {
        record.name = boundedName(properties.value(QStringLiteral("Name")).toString(),
                                  kMaxDeviceNameUtf8Bytes);
    }
    if (properties.contains(QStringLiteral("Class"))) {
        record.classOfDevice = properties.value(QStringLiteral("Class")).toUInt();
    }
    if (properties.contains(QStringLiteral("Icon"))) {
        const QString icon =
            properties.value(QStringLiteral("Icon")).toString();
        record.icon = icon.size() <= kMaxIconCharacters ? icon : QString{};
    }
    if (properties.contains(QStringLiteral("Paired"))) {
        record.paired = properties.value(QStringLiteral("Paired")).toBool();
    }
    if (properties.contains(QStringLiteral("Connected"))) {
        record.connected = properties.value(QStringLiteral("Connected")).toBool();
    }
    if (properties.contains(QStringLiteral("RSSI"))) {
        // AGENT-GUARD: The Bluetooth1 contract accepts RSSI only in
        // [-128, 0] dBm; BlueZ also reports 127 for "invalid". Anything else
        // is an unknown value, never a clamped or fabricated one.
        const qint16 rssi =
            static_cast<qint16>(properties.value(QStringLiteral("RSSI")).toInt());
        record.rssiKnown = rssi >= -128 && rssi <= 0;
        record.rssi = record.rssiKnown ? rssi : 0;
    }
    if (properties.contains(QStringLiteral("Adapter"))) {
        record.adapterPath = propertyString(
            properties.value(QStringLiteral("Adapter")));
    }
    m_devices.insert(path, record);
}

QList<BackendAdapter> BluezObjectStore::projectAdapters() const
{
    QList<BackendAdapter> projected;
    for (auto it = m_adapters.cbegin(); it != m_adapters.cend(); ++it) {
        const BluezAdapterState &record = it.value();
        if (record.address.isEmpty()) {
            continue;
        }
        BackendAdapter adapter;
        adapter.address = record.address;
        adapter.name = record.name;
        adapter.powered = record.powered;
        adapter.discovering = record.powered && record.discovering;
        projected.push_back(adapter);
    }
    std::sort(projected.begin(), projected.end(),
              [](const BackendAdapter &left, const BackendAdapter &right) {
                  return left.address < right.address;
              });
    if (projected.size() > kMaxAdapters) {
        projected.erase(projected.begin() + kMaxAdapters, projected.end());
    }
    return projected;
}

QList<BackendDevice> BluezObjectStore::projectDevices(
    const QSet<QString> &adapterAddresses) const
{
    QList<BackendDevice> projected;
    for (auto it = m_devices.cbegin(); it != m_devices.cend(); ++it) {
        const BluezDeviceState &record = it.value();
        if (record.address.isEmpty()) {
            continue;
        }
        const auto adapterIt = m_adapters.constFind(record.adapterPath);
        const BluezAdapterState *adapter =
            adapterIt == m_adapters.cend() ? nullptr : &adapterIt.value();
        if (adapter == nullptr || adapter->address.isEmpty()
            || !adapterAddresses.contains(adapter->address)) {
            continue;
        }
        // AGENT-GUARD: Bluetooth1 v1 cannot represent a connection on an
        // unpowered adapter or an unpaired device. BlueZ truth that cannot be
        // represented is suppressed at the mapping boundary instead of
        // poisoning the whole snapshot; pairing truth itself is never edited.
        BackendDevice device;
        device.adapterAddress = adapter->address;
        device.address = record.address;
        device.name = record.name;
        device.deviceClass = deviceClassFromCod(record.classOfDevice);
        if (device.deviceClass == DeviceClass::Unknown && !record.icon.isEmpty()) {
            device.deviceClass = deviceClassFromIcon(record.icon);
        }
        device.role = DeviceRole::Unknown;
        device.paired = record.paired;
        device.connected =
            record.connected && record.paired && adapter->powered;
        device.rssiKnown = record.rssiKnown;
        device.rssi = record.rssiKnown ? record.rssi : 0;
        projected.push_back(device);
    }
    std::sort(projected.begin(), projected.end(),
              [](const BackendDevice &left, const BackendDevice &right) {
                  return left.address < right.address;
              });
    // Duplicate addresses (the same remote seen under two objects) keep the
    // first entry; Bluetooth1 identity is the address, never the object path.
    auto uniqueEnd =
        std::unique(projected.begin(), projected.end(),
                    [](const BackendDevice &left, const BackendDevice &right) {
                        return left.address == right.address;
                    });
    projected.erase(uniqueEnd, projected.end());
    if (projected.size() > kMaxDevices) {
        projected.erase(projected.begin() + kMaxDevices, projected.end());
    }
    return projected;
}

} // namespace QindaQt::Bluetooth::Bluez
