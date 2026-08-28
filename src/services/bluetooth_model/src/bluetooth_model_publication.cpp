// SPDX-License-Identifier: LGPL-3.0-or-later

// AGENT-NOTE: Publication half of BluetoothModel, split from
// bluetooth_model.cpp to keep each translation unit under the repository's
// 500-nonblank decomposition-review threshold (see module-boundaries and the
// coding practices). Request validation/lifecycle lives in bluetooth_model.cpp;
// backend inventory acceptance and snapshot projection live here. The two
// halves share the class invariants documented on BluetoothModel.

#include <qindaqt/services/bluetooth_model/bluetooth_model.h>

#include <qindaqt/services/bluetooth_protocol/bluetooth_limits.h>
#include <qindaqt/services/bluetooth_protocol/bluetooth_validation.h>

#include <QtCore/QHash>
#include <QtCore/QSet>

#include <algorithm>
#include <limits>

namespace QindaQt::Bluetooth
{
namespace
{

constexpr quint64 kFnvOffsetBasis = 14695981039346656037ULL;
constexpr quint64 kFnvPrime = 1099511628211ULL;

// Stable privacy-safe serials derive from the canonical address, never from
// list position or a transient platform object ID. Adapters and devices use
// distinct prefixes so one epoch's serial space cannot collide across kinds.
quint64 stableSerial(const char prefix, const QString &address)
{
    quint64 hash = kFnvOffsetBasis;
    const QByteArray bytes = address.toUtf8();
    hash ^= static_cast<quint64>(prefix);
    hash *= kFnvPrime;
    for (const char byte : bytes) {
        hash ^= static_cast<quint64>(static_cast<unsigned char>(byte));
        hash *= kFnvPrime;
    }
    return hash == 0 ? 1 : hash;
}

} // namespace

void BluetoothModel::publishBackendMalformed()
{
    if (m_snapshot.revision == std::numeric_limits<quint64>::max()) {
        return;
    }
    Snapshot degraded;
    degraded.schemaVersion = kSchemaVersion;
    degraded.epoch = m_snapshot.epoch;
    degraded.revision = m_snapshot.revision + 1;
    degraded.availability = Availability::Degraded;
    degraded.reasonCode = QStringLiteral("backend-malformed");
    m_inventory = BackendInventory{};
    makePendingUncertain(QStringLiteral("backend-malformed"));
    m_snapshot = degraded;
    m_hasInventory = true;
    Q_EMIT snapshotChanged(m_snapshot);
    Q_EMIT invalidated(m_snapshot.epoch, m_snapshot.revision);
}

Snapshot BluetoothModel::projectInventory(const BackendInventory &inventory) const
{
    Snapshot snapshot;
    snapshot.schemaVersion = kSchemaVersion;
    snapshot.epoch = m_snapshot.epoch;
    snapshot.revision = m_snapshot.revision;
    if (inventory.adapters.isEmpty()) {
        snapshot.availability = Availability::Unavailable;
        snapshot.reasonCode = QStringLiteral("no-adapter");
        return snapshot;
    }
    snapshot.availability = Availability::Ready;
    snapshot.capabilities = Capabilities::fromInt(
        static_cast<quint32>(Capability::SetAdapterPower)
        | static_cast<quint32>(Capability::DiscoveryLease)
        | static_cast<quint32>(Capability::ConnectPaired)
        | static_cast<quint32>(Capability::DisconnectPaired));
    snapshot.reasonCode = QStringLiteral("ready");
    for (const BackendAdapter &backendAdapter : inventory.adapters) {
        Adapter adapter;
        adapter.handle = {.epoch = snapshot.epoch,
                          .serial = stableSerial('a', backendAdapter.address)};
        adapter.address = backendAdapter.address;
        adapter.name = backendAdapter.name;
        adapter.powered = backendAdapter.powered;
        adapter.discovering = backendAdapter.discovering;
        snapshot.adapters.push_back(adapter);
    }
    for (const BackendDevice &backendDevice : inventory.devices) {
        Device device;
        device.handle = {.epoch = snapshot.epoch,
                         .serial = stableSerial('d', backendDevice.address)};
        device.adapterHandle = {.epoch = snapshot.epoch,
                                .serial = stableSerial('a', backendDevice.adapterAddress)};
        device.address = backendDevice.address;
        device.name = backendDevice.name;
        device.deviceClass = backendDevice.deviceClass;
        device.role = backendDevice.role;
        device.paired = backendDevice.paired;
        device.connected = backendDevice.connected;
        device.rssiKnown = backendDevice.rssiKnown;
        device.rssi = backendDevice.rssi;
        device.batteryKnown = backendDevice.batteryKnown;
        device.batteryPercent = backendDevice.batteryPercent;
        snapshot.devices.push_back(device);
    }
    std::sort(snapshot.adapters.begin(), snapshot.adapters.end(),
              [](const Adapter &left, const Adapter &right) {
                  return left.handle.serial < right.handle.serial;
              });
    std::sort(snapshot.devices.begin(), snapshot.devices.end(),
              [](const Device &left, const Device &right) {
                  return left.handle.serial < right.handle.serial;
              });
    return snapshot;
}

bool BluetoothModel::leaseBoundsRespected(const BackendInventory &inventory) const
{
    QSet<QPair<QString, QString>> seen;
    QHash<QString, quint32> perAdapter;
    quint32 total = 0;
    for (const BackendLease &lease : inventory.leases) {
        // AGENT-GUARD: Every lease must reference a known adapter, carry a
        // safe caller identity and a usable refcount, and never duplicate a
        // caller/adapter entry. The table must also agree with each adapter's
        // discovering flag; a contradiction means the backend has lost its
        // own lease state and the snapshot fails closed.
        const bool adapterKnown = std::any_of(
            inventory.adapters.cbegin(), inventory.adapters.cend(),
            [&](const BackendAdapter &adapter) {
                return adapter.address == lease.adapterAddress;
            });
        const auto key = qMakePair(lease.callerId, lease.adapterAddress);
        if (!adapterKnown || !safeCallerId(lease.callerId) || lease.refcount == 0
            || !isCanonicalAddress(lease.adapterAddress) || seen.contains(key)) {
            return false;
        }
        seen.insert(key);
        perAdapter[lease.adapterAddress] += lease.refcount;
        total += lease.refcount;
        if (total > kMaxDiscoveryLeasesTotal) {
            return false;
        }
    }
    for (auto it = perAdapter.cbegin(); it != perAdapter.cend(); ++it) {
        if (it.value() > kMaxDiscoveryLeasesPerAdapter) {
            return false;
        }
    }
    for (const BackendAdapter &adapter : inventory.adapters) {
        const bool expected = adapter.powered && adapterLeaseTotal(inventory,
                                                                   adapter.address) != 0;
        if (adapter.discovering != expected) {
            return false;
        }
    }
    return true;
}

void BluetoothModel::acceptInventory(const quint64 generation,
                                     const BackendInventory &inventory)
{
    // AGENT-GUARD: stop() and every later start supersede already queued
    // backend values. Check the run before validation so stale malformed data
    // cannot mutate even the fail-closed projection.
    if (!m_running || generation == 0 || generation != m_backendGeneration) {
        return;
    }

    const bool inventorySane = inventory.adapters.size() <= kMaxAdapters
        && inventory.devices.size() <= kMaxDevices && leaseBoundsRespected(inventory)
        && std::all_of(inventory.adapters.cbegin(), inventory.adapters.cend(),
                       [](const BackendAdapter &adapter) {
                           return isCanonicalAddress(adapter.address)
                               && isBoundedText(adapter.name, kMaxAdapterNameUtf8Bytes);
                       })
        && std::all_of(inventory.devices.cbegin(), inventory.devices.cend(),
                       [&](const BackendDevice &device) {
                           const auto adapterIt = std::find_if(
                               inventory.adapters.cbegin(), inventory.adapters.cend(),
                               [&](const BackendAdapter &adapter) {
                                   return adapter.address == device.adapterAddress;
                               });
                           return adapterIt != inventory.adapters.cend()
                               && isCanonicalAddress(device.address)
                               && isBoundedText(device.name, kMaxDeviceNameUtf8Bytes);
                       });
    if (!inventorySane) {
        publishBackendMalformed();
        return;
    }

    Snapshot candidate = projectInventory(inventory);
    if (validateSnapshot(candidate).accepted) {
        m_inventory = inventory;
    } else {
        publishBackendMalformed();
        return;
    }

    if (m_hasInventory && candidate == m_snapshot) {
        // Equal lineage has one canonical value; a redundant republication of
        // identical content is dropped without advancing the revision.
        return;
    }
    if (m_snapshot.revision == std::numeric_limits<quint64>::max()) {
        return;
    }
    candidate.revision = m_snapshot.revision + 1;
    m_snapshot = candidate;
    m_hasInventory = true;
    Q_EMIT snapshotChanged(m_snapshot);
    Q_EMIT invalidated(m_snapshot.epoch, m_snapshot.revision);
}

} // namespace QindaQt::Bluetooth
