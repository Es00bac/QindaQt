// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/bluetooth_model/bluetooth_model.h>
#include <qindaqt/services/bluetooth_model/fake_adapter_interface.h>

namespace QindaQt::Bluetooth
{

BluetoothModel::BluetoothModel(QObject *parent)
    : QObject(parent)
    , m_epoch(1)
    , m_revision(0)
    , m_nextSerial(1)
    , m_fakeAdapter(std::make_unique<FakeAdapterInterface>())
{
    initializeFakeState();
}

BluetoothModel::~BluetoothModel() = default;

quint64 BluetoothModel::nextEpoch()
{
    return ++m_epoch;
}

quint64 BluetoothModel::nextRevision()
{
    return ++m_revision;
}

quint64 BluetoothModel::nextSerial()
{
    return m_nextSerial++;
}

Snapshot BluetoothModel::currentSnapshot() const
{
    Snapshot snapshot;
    snapshot.schemaVersion = 1;
    snapshot.epoch = m_epoch;
    snapshot.revision = m_revision;
    snapshot.reasonCode = QStringLiteral("ready");
    snapshot.diagnostic = QString();
    snapshot.adapters = m_adapters;
    snapshot.devices = m_devices;
    snapshot.wireValid = true;
    return snapshot;
}

OperationResult BluetoothModel::executeOperation(const OperationRequest &request)
{
    const quint64 initiatingRevision = m_revision;
    OperationResult result;
    result.kind = request.kind;
    result.initiatingEpoch = m_epoch;
    result.initiatingRevision = initiatingRevision;
    result.wireValid = true;

    switch (request.kind) {
    case OperationKind::Pair:
        result = executePairOperation(request);
        break;
    case OperationKind::Connect:
        result = executeConnectOperation(request);
        break;
    case OperationKind::Disconnect:
        result = executeDisconnectOperation(request);
        break;
    case OperationKind::Trust:
    case OperationKind::Untrust:
        result = executeTrustOperation(request);
        break;
    }

    result.observedEpoch = m_epoch;
    result.observedRevision = m_revision;
    return result;
}

void BluetoothModel::initializeFakeState()
{
    // Create default adapter
    Handle adapterHandle{m_epoch, static_cast<quint64>(nextSerial())};
    Adapter adapter;
    adapter.handle = adapterHandle;
    adapter.address = QStringLiteral("00:1A:7D:DA:71:13");
    adapter.name = QStringLiteral("QindaQt Device");
    adapter.state = AdapterState::On;
    adapter.discoveringActive = false;
    adapter.capabilities = AdapterCapability::Discover | AdapterCapability::Pair
        | AdapterCapability::Connect;
    m_adapters.append(adapter);

    // Create some default discovered devices
    Handle device1Handle{m_epoch, static_cast<quint64>(nextSerial())};
    Device device1;
    device1.handle = device1Handle;
    device1.adapterHandle = adapterHandle;
    device1.address = QStringLiteral("00:1B:63:84:45:E6");
    device1.name = QStringLiteral("Test Headphones");
    device1.state = DeviceState::Disconnected;
    device1.rssi = -65;
    device1.rssiKnown = true;
    device1.paired = true;
    device1.trusted = true;
    device1.capabilities = DeviceCapability::Connect | DeviceCapability::Disconnect
        | DeviceCapability::Trust;
    m_devices.append(device1);

    m_revision = 1;
}

OperationResult BluetoothModel::executePairOperation(const OperationRequest &request)
{
    OperationResult result;
    result.kind = OperationKind::Pair;

    auto it = std::find_if(m_devices.begin(), m_devices.end(),
                           [&request](const Device &d) { return d.handle == request.primary; });
    if (it == m_devices.end()) {
        result.status = OperationStatus::Rejected;
        result.reasonCode = QStringLiteral("device-not-found");
        return result;
    }

    if (it->paired) {
        result.status = OperationStatus::Rejected;
        result.reasonCode = QStringLiteral("already-paired");
        return result;
    }

    it->paired = true;
    ++m_revision;
    result.status = OperationStatus::Succeeded;
    result.reasonCode = QString();
    return result;
}

OperationResult BluetoothModel::executeConnectOperation(const OperationRequest &request)
{
    OperationResult result;
    result.kind = OperationKind::Connect;

    auto it = std::find_if(m_devices.begin(), m_devices.end(),
                           [&request](const Device &d) { return d.handle == request.primary; });
    if (it == m_devices.end()) {
        result.status = OperationStatus::Rejected;
        result.reasonCode = QStringLiteral("device-not-found");
        return result;
    }

    if (!it->paired) {
        result.status = OperationStatus::Rejected;
        result.reasonCode = QStringLiteral("not-paired");
        return result;
    }

    it->state = DeviceState::Connected;
    ++m_revision;
    result.status = OperationStatus::Succeeded;
    result.reasonCode = QString();
    return result;
}

OperationResult BluetoothModel::executeDisconnectOperation(const OperationRequest &request)
{
    OperationResult result;
    result.kind = OperationKind::Disconnect;

    auto it = std::find_if(m_devices.begin(), m_devices.end(),
                           [&request](const Device &d) { return d.handle == request.primary; });
    if (it == m_devices.end()) {
        result.status = OperationStatus::Rejected;
        result.reasonCode = QStringLiteral("device-not-found");
        return result;
    }

    if (it->state == DeviceState::Disconnected) {
        result.status = OperationStatus::Rejected;
        result.reasonCode = QStringLiteral("already-disconnected");
        return result;
    }

    it->state = DeviceState::Disconnected;
    ++m_revision;
    result.status = OperationStatus::Succeeded;
    result.reasonCode = QString();
    return result;
}

OperationResult BluetoothModel::executeTrustOperation(const OperationRequest &request)
{
    OperationResult result;
    result.kind = request.kind;

    auto it = std::find_if(m_devices.begin(), m_devices.end(),
                           [&request](const Device &d) { return d.handle == request.primary; });
    if (it == m_devices.end()) {
        result.status = OperationStatus::Rejected;
        result.reasonCode = QStringLiteral("device-not-found");
        return result;
    }

    const bool shouldTrust = request.kind == OperationKind::Trust;
    if (it->trusted == shouldTrust) {
        result.status = OperationStatus::Rejected;
        result.reasonCode = shouldTrust ? QStringLiteral("already-trusted")
                                         : QStringLiteral("already-untrusted");
        return result;
    }

    it->trusted = shouldTrust;
    ++m_revision;
    result.status = OperationStatus::Succeeded;
    result.reasonCode = QString();
    return result;
}

} // namespace QindaQt::Bluetooth
