// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/display_protocol/display_limits.h>
#include <qindaqt/services/display_topology/topology.h>
#include <qindaqt/services/display_transaction/transaction_machine.h>

namespace QindaQt::DisplayTransaction::Test
{

inline void require(const bool condition, const char *context)
{
    // AGENT-GUARD: Setup transitions must execute in Release tests. Q_ASSERT
    // can compile away its expression and silently leave a fixture in the
    // wrong state, so shared drivers use this always-evaluated fatal check.
    if (!condition) {
        qFatal("transaction test setup failed: %s", context);
    }
}

class FakeClock final : public MonotonicClock
{
public:
    [[nodiscard]] quint64 nowMilliseconds() const noexcept override { return now; }
    void advance(const quint64 delta) { now += delta; }

    quint64 now = 100;
};

class FakePort final : public SideEffectPort
{
public:
    [[nodiscard]] bool storeJournal(const Journal &value) override
    {
        ++storeCalls;
        if (!storeSucceeds) {
            return false;
        }
        journal = value;
        journalPresent = true;
        return true;
    }

    [[nodiscard]] bool clearJournal() override
    {
        ++clearCalls;
        if (!clearSucceeds) {
            return false;
        }
        journal = {};
        journalPresent = false;
        return true;
    }

    void requestApply(const ApplyRequest &request) override { requests.push_back(request); }

    bool storeSucceeds = true;
    bool clearSucceeds = true;
    bool journalPresent = false;
    int storeCalls = 0;
    int clearCalls = 0;
    Journal journal;
    QList<ApplyRequest> requests;
};

inline Timing timing()
{
    return {.applyTimeoutMilliseconds = 10,
            .observationTimeoutMilliseconds = 5,
            .confirmationTimeoutMilliseconds = 20,
            .firstRevertBackoffMilliseconds = 2,
            .secondRevertBackoffMilliseconds = 3};
}

inline Display::Mode mode(const QString &id, const QSize &size)
{
    return {.id = id,
            .pixelSize = size,
            .refreshMilliHertz = 60'000,
            .preferred = id == QStringLiteral("full")};
}

inline Display::Output output(const QString &id, const QString &connector,
                              const QPoint &position, const bool primary,
                              const quint32 priority)
{
    return {.stableId = id,
            .connectorName = connector,
            .runtimeCompositorUuid = QStringLiteral("runtime-%1").arg(connector),
            .label = connector,
            .manufacturer = QStringLiteral("QIN"),
            .model = QStringLiteral("Panel"),
            .physicalSizeMillimeters = QSize(500, 300),
            .hasSerial = true,
            .internal = false,
            .ambiguousIdentity = false,
            .enabled = true,
            .primary = primary,
            .modeId = QStringLiteral("full"),
            .position = position,
            .logicalSize = QSize(1920, 1080),
            .scale = 1.0,
            .transform = Display::Transform::Normal,
            .priority = priority,
            .replicationSourceStableId = {},
            .modes = {mode(QStringLiteral("full"), QSize(1920, 1080)),
                      mode(QStringLiteral("small"), QSize(1280, 720))}};
}

inline Display::Snapshot snapshot(const bool dual = false, const quint64 revision = 1,
                                  const QString &epoch = QStringLiteral("epoch"))
{
    QList<Display::Output> outputs{
        output(QStringLiteral("edid:a"), QStringLiteral("DP-1"), {}, true, 1)};
    if (dual) {
        outputs.push_back(output(QStringLiteral("edid:b"), QStringLiteral("DP-2"),
                                 QPoint(1920, 0), false, 2));
    }
    Display::Snapshot value{.protocolVersion = Display::kProtocolVersion,
                            .serviceEpoch = epoch,
                            .revision = revision,
                            .liveFingerprint = {},
                            .outputs = std::move(outputs),
                            .transactions = {},
                            .wireValid = true};
    value.liveFingerprint = DisplayTopology::canonicalFingerprint(
        DisplayTopology::candidateFromSnapshot(value));
    return value;
}

inline Display::Candidate changedCandidate(const Display::Snapshot &snapshot)
{
    Display::Candidate candidate = DisplayTopology::candidateFromSnapshot(snapshot);
    candidate.outputs[0].scale = 1.25;
    return candidate;
}

inline Display::Snapshot observed(const Display::Snapshot &base,
                                  const Display::Candidate &candidate,
                                  const quint64 revision)
{
    Display::Snapshot value = base;
    value.revision = revision;
    for (Display::Output &live : value.outputs) {
        const auto configured = std::find_if(
            candidate.outputs.cbegin(), candidate.outputs.cend(),
            [&](const Display::CandidateOutput &output) {
                return output.stableId == live.stableId;
            });
        if (configured == candidate.outputs.cend()) {
            continue;
        }
        live.enabled = configured->enabled;
        live.primary = configured->primary;
        live.modeId = configured->modeId;
        live.position = configured->position;
        live.scale = configured->scale;
        live.transform = configured->transform;
        live.priority = configured->priority;
        live.replicationSourceStableId = configured->replicationSourceStableId;
        const auto selected = std::find_if(live.modes.cbegin(), live.modes.cend(),
                                           [&](const Display::Mode &mode) {
                                               return mode.id == live.modeId;
                                           });
        if (live.enabled && selected != live.modes.cend()) {
            live.logicalSize = DisplayTopology::logicalSizeForMode(
                *selected, live.scale, live.transform);
        }
    }
    value.liveFingerprint = DisplayTopology::canonicalFingerprint(candidate);
    return value;
}

inline quint64 previewToObserving(Machine &machine, FakePort &port,
                                  const Display::Candidate &candidate,
                                  const QString &transactionId = QStringLiteral("tx"))
{
    require(machine.stage(transactionId, candidate).accepted, "stage");
    require(machine.preview(transactionId).accepted, "preview");
    require(!port.requests.isEmpty(), "forward request");
    const quint64 token = port.requests.last().token;
    require(machine.applyCompleted(token, ApplyOutcome::Applied).accepted,
            "forward completion");
    return token;
}

inline void previewToAwaitingConfirmation(Machine &machine, FakePort &port,
                                          const Display::Snapshot &base,
                                          const Display::Candidate &candidate,
                                          const QString &transactionId = QStringLiteral("tx"))
{
    previewToObserving(machine, port, candidate, transactionId);
    require(machine.observedSnapshot(observed(base, candidate, base.revision + 1)).accepted,
            "target observation");
    require(machine.view().state == MachineState::AwaitingConfirmation,
            "awaiting confirmation");
}

} // namespace QindaQt::DisplayTransaction::Test
