// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/bluetooth_protocol/bluetooth_types.h>

#include <QtCore/QList>
#include <QtCore/QString>

namespace QindaQt::Shell::BluetoothApplet
{

enum class ServicePhase {
    Loading,
    Ready,
    Unavailable,
};

struct AdapterRow {
    QString id;
    QString label;
    bool powered = false;
    bool discovering = false;
    bool canSetPowered = false;
    bool canAcquireDiscovery = false;
    bool canReleaseDiscovery = false;
    QString accessibleName;
    QString accessibleDescription;

    friend bool operator==(const AdapterRow &, const AdapterRow &) = default;
};

struct DeviceRow {
    QString id;
    QString adapterId;
    QString label;
    QString classLabel;
    bool paired = false;
    bool connected = false;
    bool batteryKnown = false;
    int batteryPercent = 0;
    bool signalKnown = false;
    int signalDbm = 0;
    bool canConnect = false;
    bool canDisconnect = false;
    bool canPair = false;
    bool canRemove = false;
    bool canSetTrusted = false;
    bool trusted = false;
    QString accessibleName;
    QString accessibleDescription;

    friend bool operator==(const DeviceRow &, const DeviceRow &) = default;
};

struct BluetoothAppletModel {
    ServicePhase phase = ServicePhase::Unavailable;
    QString diagnostic;
    quint64 epoch = 0;
    quint64 revision = 0;
    QString summaryLabel;
    QString accessibleName;
    QString accessibleDescription;
    QList<AdapterRow> adapters;
    QList<DeviceRow> devices;

    friend bool operator==(const BluetoothAppletModel &,
                           const BluetoothAppletModel &) = default;
};

[[nodiscard]] QString adapterRowId(const Bluetooth::Handle &handle);
[[nodiscard]] QString deviceRowId(const Bluetooth::Handle &handle);

} // namespace QindaQt::Shell::BluetoothApplet
