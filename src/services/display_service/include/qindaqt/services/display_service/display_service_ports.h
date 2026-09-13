// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/display_service/display_inventory.h>
#include <qindaqt/services/display_transaction/transaction_ports.h>

#include <QtDBus/QDBusConnection>

#include <memory>

namespace QindaQt::DisplayService
{

class InventoryObserver
{
public:
    virtual ~InventoryObserver() = default;
    virtual void inventoryObserved(const InventoryFrame &frame) = 0;
    virtual void inventoryUnavailable() = 0;
};

enum class InventorySourceStartStatus {
    Started,
    InvalidConnection,
    SignalRegistrationFailed,
};

class InventorySource
{
public:
    virtual ~InventorySource() = default;

    // The observer is borrowed, receives callbacks only on the start/stop
    // thread, and must outlive a started source. nullptr detaches it. stop()
    // is idempotent and prevents later callbacks.
    virtual void setObserver(InventoryObserver *observer) = 0;
    [[nodiscard]] virtual InventorySourceStartStatus start() = 0;
    virtual void stop() = 0;
};

// D7 brightness facts for one compositor output device, as observed on the
// transaction port's own compositor connection.
struct DeviceBrightness {
    QString connectorName;
    QString runtimeUuid;
    bool enabled = false;
    bool capable = false;
    bool observed = false;
    quint32 value = 0;

    friend bool operator==(const DeviceBrightness &, const DeviceBrightness &) = default;
};

struct DeviceBrightnessFrame {
    // Positive only while the port holds a live connection whose complete,
    // unambiguous device set is known. Zero carries no device authority and
    // an empty device list.
    quint64 ownerGeneration = 0;
    QList<DeviceBrightness> devices;

    friend bool operator==(const DeviceBrightnessFrame &, const DeviceBrightnessFrame &) = default;
};

struct BrightnessApplyRequest {
    quint64 requestId = 0;
    quint64 ownerGeneration = 0;
    QString connectorName;
    QString runtimeUuid;
    quint32 value = 0;

    friend bool operator==(const BrightnessApplyRequest &, const BrightnessApplyRequest &) = default;
};

enum class BrightnessSubmitStatus {
    Accepted,
    Unavailable,
    Busy,
    Unsupported,
    Malformed,
};

enum class BrightnessApplyOutcome {
    Applied,
    Rejected,
    TransportUncertain,
};

class TransactionPortObserver
{
public:
    virtual ~TransactionPortObserver() = default;
    virtual void applyCompleted(quint64 machineLineage, quint64 token,
                                DisplayTransaction::ApplyOutcome outcome) = 0;
    // D7 notifications default to no-ops so topology-only ports and observers
    // stay source-compatible. A frame replaces all earlier device facts.
    virtual void brightnessDevicesObserved(const DeviceBrightnessFrame &) {}
    virtual void brightnessCompleted(quint64, BrightnessApplyOutcome) {}
};

class TransactionPort : public DisplayTransaction::SideEffectPort
{
public:
    // Observer lifetime/thread rules match InventorySource. A transport loss
    // reports TransportUncertain for an accepted token or produces no callback;
    // it never swaps the borrowed D1 port during a Machine lifetime.
    virtual void setObserver(TransactionPortObserver *observer) = 0;
    // Called before a replacement D1 Machine can issue a request. The port
    // copies the current lineage beside each accepted ApplyRequest and reports
    // that request's copied lineage on completion, even after a later call has
    // advanced the current lineage. It never reenters the observer
    // synchronously.
    virtual void beginMachineLineage(quint64 machineLineage) = 0;
    // AGENT-CONTRACT: Accepted promises exactly one later, asynchronous
    // brightnessCompleted for request.requestId; every other status promises
    // none. The port submits only to the exact connector and runtime UUID of
    // request.ownerGeneration and never retries.
    [[nodiscard]] virtual BrightnessSubmitStatus requestBrightness(
        const BrightnessApplyRequest &)
    {
        return BrightnessSubmitStatus::Unsupported;
    }
};

// Owns only QtDBus transport state. It calls Compositor1.Outputs on the exact
// unique owner, treats OutputsChanged as an invalidation hint, serializes one
// read at a time, and rejects stale replies after owner replacement.
[[nodiscard]] std::unique_ptr<InventorySource> makeCompositorInventorySource(
    const QDBusConnection &connection);

} // namespace QindaQt::DisplayService
