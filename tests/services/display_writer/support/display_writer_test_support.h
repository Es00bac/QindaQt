// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/display_topology/topology.h>
#include <qindaqt/services/display_writer/writer_transaction_port.h>

#include <optional>

namespace QindaQt::DisplayWriter::TestSupport
{

class FakeOutputManagementPort final : public OutputManagementPort
{
public:
    void setObserver(OutputManagementObserver *value) override { observer = value; }
    PortStartStatus start() override
    {
        ++startCalls;
        started = startStatus == PortStartStatus::Started
            || startStatus == PortStartStatus::AlreadyStarted;
        return startStatus;
    }
    void stop() override
    {
        ++stopCalls;
        started = false;
        if (detachObserverOnStop) {
            observer = nullptr;
        }
    }
    SubmitStatus submit(const Configuration &configuration) override
    {
        submissions.push_back(configuration);
        if (synchronousOutcome) {
            observer->outputManagementCompleted(ownerGeneration,
                                                configuration.requestId,
                                                *synchronousOutcome);
        }
        return submitStatus;
    }
    void publishOwner(quint64 generation, bool available)
    {
        Q_ASSERT(observer != nullptr);
        ownerGeneration = generation;
        observer->outputManagementOwnerChanged(generation, available);
    }
    void complete(quint64 generation, quint64 requestId,
                  CompletionOutcome outcome)
    {
        Q_ASSERT(observer != nullptr);
        observer->outputManagementCompleted(generation, requestId, outcome);
    }

    OutputManagementObserver *observer = nullptr;
    PortStartStatus startStatus = PortStartStatus::Started;
    SubmitStatus submitStatus = SubmitStatus::Accepted;
    std::optional<CompletionOutcome> synchronousOutcome;
    QList<Configuration> submissions;
    quint64 ownerGeneration = 0;
    int startCalls = 0;
    int stopCalls = 0;
    bool started = false;
    bool detachObserverOnStop = false;
};

class FakeJournalStore final : public JournalStore
{
public:
    bool store(const DisplayTransaction::Journal &journal) override
    {
        journals.push_back(journal);
        return storeSucceeds;
    }
    bool clear() override
    {
        ++clearCalls;
        return clearSucceeds;
    }

    QList<DisplayTransaction::Journal> journals;
    int clearCalls = 0;
    bool storeSucceeds = true;
    bool clearSucceeds = true;
};

struct Completion {
    quint64 machineLineage = 0;
    quint64 token = 0;
    DisplayTransaction::ApplyOutcome outcome =
        DisplayTransaction::ApplyOutcome::TransportUncertain;

    friend bool operator==(const Completion &, const Completion &) = default;
};

class RecordingObserver final : public DisplayService::TransactionPortObserver
{
public:
    void applyCompleted(quint64 machineLineage, quint64 token,
                        DisplayTransaction::ApplyOutcome outcome) override
    {
        completions.push_back({machineLineage, token, outcome});
    }

    QList<Completion> completions;
};

inline Display::Output output(QString stableId = QStringLiteral("conn:DP-1"),
                              QString connector = QStringLiteral("DP-1"),
                              bool primary = true, quint32 priority = 1)
{
    const Display::Mode mode{.id = QStringLiteral("current:1920x1080@60000"),
                             .pixelSize = QSize(1920, 1080),
                             .refreshMilliHertz = 60'000,
                             .preferred = true};
    return {.stableId = std::move(stableId),
            .connectorName = std::move(connector),
            .runtimeCompositorUuid = QStringLiteral("runtime-uuid"),
            .label = QStringLiteral("Reference"),
            .manufacturer = QStringLiteral("QIN"),
            .model = QStringLiteral("Panel"),
            .physicalSizeMillimeters = QSize(600, 340),
            .hasSerial = false,
            .internal = false,
            .ambiguousIdentity = false,
            .enabled = true,
            .primary = primary,
            .modeId = mode.id,
            .position = priority == 1 ? QPoint{} : QPoint(1920, 0),
            .logicalSize = QSize(1920, 1080),
            .scale = 1.0,
            .transform = Display::Transform::Normal,
            .priority = priority,
            .replicationSourceStableId = {},
            .modes = {mode},
            .wireValid = true};
}

inline Display::Snapshot snapshot(bool dual = false)
{
    QList<Display::Output> outputs{output()};
    if (dual) {
        outputs.push_back(output(QStringLiteral("conn:DP-2"),
                                 QStringLiteral("DP-2"), false, 2));
    }
    Display::Snapshot value{.protocolVersion = Display::kProtocolVersion,
                            .serviceEpoch = QStringLiteral("epoch"),
                            .revision = 7,
                            .liveFingerprint = {},
                            .outputs = std::move(outputs),
                            .transactions = {},
                            .wireValid = true};
    value.liveFingerprint = DisplayTopology::canonicalFingerprint(
        DisplayTopology::candidateFromSnapshot(value));
    return value;
}

inline DisplayTransaction::ApplyRequest completeRequest(quint64 token = 11,
                                                        bool dual = false)
{
    return {.token = token,
            .scope = DisplayTransaction::ApplyScope::ForwardCandidate,
            .candidate = DisplayTopology::candidateFromSnapshot(snapshot(dual)),
            .survivingProperties = {}};
}

} // namespace QindaQt::DisplayWriter::TestSupport
