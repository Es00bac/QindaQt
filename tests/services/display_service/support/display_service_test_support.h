// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/display_service/display_service_ports.h>

#include <utility>

namespace QindaQt::DisplayService::TestSupport
{

class FakeClock final : public DisplayTransaction::MonotonicClock
{
public:
    [[nodiscard]] quint64 nowMilliseconds() const noexcept override { return now; }
    quint64 now = 100;
};

class FakeTransactionPort final : public TransactionPort
{
public:
    void setObserver(TransactionPortObserver *value) override { observer = value; }
    void beginMachineLineage(const quint64 value) override
    {
        currentMachineLineage = value;
    }
    bool storeJournal(const DisplayTransaction::Journal &journal) override
    {
        storedJournals.push_back(journal);
        return storeSucceeds;
    }
    bool clearJournal() override
    {
        ++clearCalls;
        return clearSucceeds;
    }
    void requestApply(const DisplayTransaction::ApplyRequest &request) override
    {
        requestMachineLineages.push_back(currentMachineLineage);
        applyRequests.push_back(request);
    }
    void completeLast(DisplayTransaction::ApplyOutcome outcome)
    {
        Q_ASSERT(observer != nullptr);
        Q_ASSERT(!applyRequests.isEmpty());
        observer->applyCompleted(requestMachineLineages.constLast(),
                                 applyRequests.constLast().token, outcome);
    }

    TransactionPortObserver *observer = nullptr;
    QList<DisplayTransaction::Journal> storedJournals;
    QList<quint64> requestMachineLineages;
    QList<DisplayTransaction::ApplyRequest> applyRequests;
    quint64 currentMachineLineage = 0;
    int clearCalls = 0;
    bool storeSucceeds = true;
    bool clearSucceeds = true;
};

class FakeInventorySource final : public InventorySource
{
public:
    void setObserver(InventoryObserver *value) override { observer = value; }
    InventorySourceStartStatus start() override
    {
        started = true;
        return startStatus;
    }
    void stop() override { started = false; }
    void publish(const InventoryFrame &frame)
    {
        Q_ASSERT(observer != nullptr);
        observer->inventoryObserved(frame);
    }
    void loseTransport()
    {
        Q_ASSERT(observer != nullptr);
        observer->inventoryUnavailable();
    }

    InventoryObserver *observer = nullptr;
    InventorySourceStartStatus startStatus = InventorySourceStartStatus::Started;
    bool started = false;
};

inline InventoryOutput output(QString name = QStringLiteral("DP-1"),
                              QRect geometry = QRect(0, 0, 1920, 1080),
                              double scale = 1.0,
                              Display::Transform transform = Display::Transform::Normal)
{
    return {.name = std::move(name),
            .geometry = geometry,
            .scale = scale,
            .refreshRateMilliHertz = 60'000,
            .transform = transform,
            .internal = false,
            .runtimeCompositorUuid = QStringLiteral("runtime-uuid"),
            .compositorPriority = 0,
            .physicalSizeMillimeters = QSize(600, 340),
            .manufacturer = QStringLiteral("Qinda"),
            .model = QStringLiteral("Reference Display")};
}

inline InventoryFrame frame(quint64 generation, QList<InventoryOutput> outputs,
                            QString owner = QStringLiteral(":1.42"))
{
    return {.uniqueOwner = std::move(owner),
            .outputGeneration = generation,
            .outputs = std::move(outputs)};
}

} // namespace QindaQt::DisplayService::TestSupport
