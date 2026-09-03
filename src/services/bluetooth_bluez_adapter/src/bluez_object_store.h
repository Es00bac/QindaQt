// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/bluetooth_model/adapter_backend.h>

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QSet>
#include <QtCore/QString>
#include <QtCore/QVariantMap>

namespace QindaQt::Bluetooth::Bluez
{

// Parsed ObjectManager payload: object path -> interface name -> properties.
using BluezInterfaces = QHash<QString, QVariantMap>;
using BluezManagedObjects = QHash<QString, BluezInterfaces>;

// One BlueZ adapter or device object as raw platform truth. Fields the
// platform never reported stay at their defaults; sanitization and dropping
// happen only at projection time.
struct BluezAdapterState {
    QString path;
    QString address;
    QString name;
    bool powered = false;
    bool discovering = false;
};

struct BluezDeviceState {
    QString path;
    QString adapterPath;
    QString address;
    QString name;
    quint32 classOfDevice = 0;
    QString icon;
    bool paired = false;
    bool trusted = false;
    bool connected = false;
    bool rssiKnown = false;
    qint16 rssi = 0;
};

// AGENT-CONTRACT: The store holds only parsed BlueZ truth and never
// fabricates a value the platform did not report. Projection maps that truth
// into Bluetooth1 backend values fail-closed per entity: an adapter with a
// non-canonical address is dropped together with its devices, text is bounded
// without splitting UTF-8 sequences, RSSI outside [-128, 0] becomes unknown,
// class decoding accepts only the 24-bit CoD space, devices referencing a
// dropped or missing adapter are dropped, duplicate device addresses keep the
// lexicographically smallest object path, and both lists are capped at the
// Bluetooth1 inventory bounds. The lease table and the published discovering
// flags are owned by the backend, not this store.
class BluezObjectStore final
{
public:
    void replaceFrom(const BluezManagedObjects &objects);
    void upsertInterfaces(const QString &path, const BluezInterfaces &interfaces);
    void removeInterfaces(const QString &path, const QList<QString> &interfaces);
    void clear();

    [[nodiscard]] const BluezAdapterState *adapter(const QString &path) const;
    [[nodiscard]] const BluezDeviceState *device(const QString &path) const;
    [[nodiscard]] const QHash<QString, BluezAdapterState> &adapters() const
    {
        return m_adapters;
    }
    [[nodiscard]] const BluezAdapterState *adapterByAddress(
        const QString &address) const;
    [[nodiscard]] const BluezDeviceState *deviceByAddress(
        const QString &address) const;

    // Optimistic platform-confirmed state applied from successful method
    // replies so a BlueZ build that omits the PropertiesChanged emission
    // cannot leave the cache behind the accepted truth.
    void applyPowered(const QString &adapterPath, bool powered);
    void applyConnected(const QString &devicePath, bool connected);
    void applyPaired(const QString &devicePath, bool paired);
    void applyTrusted(const QString &devicePath, bool trusted);

    // Sanitized, bounded, deduplicated projection of adapters and of the
    // devices belonging to the projected adapter addresses.
    [[nodiscard]] QList<BackendAdapter> projectAdapters() const;
    [[nodiscard]] QList<BackendDevice> projectDevices(
        const QSet<QString> &adapterAddresses) const;

private:
    void upsertAdapter(const QString &path, const QVariantMap &properties);
    void upsertDevice(const QString &path, const QVariantMap &properties);
    void dropAdapter(const QString &path);

    QHash<QString, BluezAdapterState> m_adapters;
    QHash<QString, BluezDeviceState> m_devices;
};

} // namespace QindaQt::Bluetooth::Bluez
