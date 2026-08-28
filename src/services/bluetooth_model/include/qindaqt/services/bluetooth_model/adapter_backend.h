// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/bluetooth_protocol/bluetooth_types.h>

#include <QtCore/QObject>

namespace QindaQt::Bluetooth
{

enum class BackendOperationStatus {
    Succeeded,
    Rejected,
    Failed,
    Uncertain,
};

struct BackendOperationOutcome {
    BackendOperationStatus status = BackendOperationStatus::Failed;
    QString reasonCode;
    QString diagnostic;
};

struct BackendAdapter {
    QString address;
    QString name;
    bool powered = false;
    bool discovering = false;
};

struct BackendDevice {
    QString adapterAddress;
    QString address;
    QString name;
    DeviceClass deviceClass = DeviceClass::Unknown;
    DeviceRole role = DeviceRole::Unknown;
    bool paired = false;
    bool connected = false;
    bool rssiKnown = false;
    qint16 rssi = 0;
    bool batteryKnown = false;
    quint8 batteryPercent = 0;
};

// AGENT-CONTRACT: A discovery lease is one caller-scoped reference-counted
// hold on one adapter's discovery session. The backend owns the lease table
// because it owns the discovery session; callers are identified by their
// unique D-Bus name. The backend must honor kMaxDiscoveryLeasesPerAdapter and
// kMaxDiscoveryLeasesTotal, stop discovery when an adapter's last lease
// drops, release an adapter's leases when that adapter powers off (power-on
// must not resurrect discovery), and drop every lease it holds in stop()
// plus all leases of a vanished caller through releaseOwner(). The model
// re-checks the reported lease table against those bounds and fails closed.
// start() must return its generation before that run may publish; the first
// publication of a run is the backend's responsibility and must be fenced by
// the returned generation. The model resolves public handles to canonical
// addresses before dispatch. The backend never sees (epoch, serial) lineage;
// it mutates platform state by address and identifies the mutating caller by
// unique D-Bus name for discovery-lease accounting.
struct BackendLease {
    QString callerId;
    QString adapterAddress;
    quint32 refcount = 0;
};

struct BackendInventory {
    QList<BackendAdapter> adapters;
    QList<BackendDevice> devices;
    QList<BackendLease> leases;
};

// AGENT-CONTRACT: The model resolves public handles to canonical addresses
// before dispatch. The backend never sees (epoch, serial) lineage; it mutates
// platform state by address and identifies the mutating caller by unique
// D-Bus name for discovery-lease accounting.
struct BackendRequest {
    OperationKind kind = OperationKind::Connect;
    QString adapterAddress;
    QString deviceAddress;
    bool powered = false;
    QString callerId;
};

// AGENT-CONTRACT: Implementations receive requests on the Qt main thread and
// publish only immutable value copies through these signals. start() returns a
// fresh nonzero generation before that run can publish; every value carries
// the generation that produced it, and generations are equality tokens rather
// than ordered values. The backend is an untrusted platform boundary: it must
// never fabricate inventory that the platform does not report, and BlueZ (not
// Bluetooth1) owns pairing, trust, keys, device records, profiles, and
// authorization. A future BluezQt adapter implements this port; the
// deterministic adapter stands in until that runtime lane opens. See
// ADR-0037.
class AdapterBackend : public QObject
{
    Q_OBJECT

public:
    explicit AdapterBackend(QObject *parent = nullptr)
        : QObject(parent)
    {
    }
    ~AdapterBackend() override = default;

    [[nodiscard]] virtual quint64 start() = 0;
    virtual void stop() = 0;
    virtual void submit(quint64 operationId, const BackendRequest &request) = 0;
    virtual void releaseOwner(const QString &callerId) = 0;

Q_SIGNALS:
    void inventoryChanged(quint64 generation,
                          const QindaQt::Bluetooth::BackendInventory &inventory);
    void operationFinished(quint64 generation, quint64 operationId,
                           const QindaQt::Bluetooth::BackendOperationOutcome &outcome);
};

} // namespace QindaQt::Bluetooth

Q_DECLARE_METATYPE(QindaQt::Bluetooth::BackendInventory)
Q_DECLARE_METATYPE(QindaQt::Bluetooth::BackendOperationOutcome)
