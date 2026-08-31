// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/display_runtime/session_safety_port.h>
#include <qindaqt/services/display_runtime/resident_display_runtime.h>
#include <qindaqt/services/display_topology/topology.h>

#include "../../display_service/support/display_service_test_support.h"
#include "../../display_service/support/private_bus_test_support.h"
#include "../../display_writer/support/display_writer_test_support.h"

#include <QtCore/QCoreApplication>
#include <QtDBus/QDBusError>

#include <memory>
#include <optional>
#include <utility>

namespace QindaQt::DisplayRuntime::TestSupport
{

class FakeSessionSafetyPort final : public SessionSafetyPort
{
public:
    void setObserver(SessionSafetyObserver *value) override { observer = value; }

    SessionSafetyStartStatus start(const qint64 processId) override
    {
        ++startCalls;
        expectedProcessId = processId;
        if (startStatus != SessionSafetyStartStatus::Started
            && startStatus != SessionSafetyStartStatus::AlreadyStarted) {
            return startStatus;
        }
        running = true;
        if (readySynchronously && observer != nullptr) {
            observer->sessionSafetyReady();
        }
        return startStatus;
    }

    void stop() override
    {
        ++stopCalls;
        running = false;
        held = false;
        state = DisplayTransaction::SafetyState::Unknown;
    }

    void releaseSleepDelay() override
    {
        ++releaseCalls;
        held = false;
    }

    [[nodiscard]] bool delayHeld() const noexcept override { return held; }

    [[nodiscard]] DisplayTransaction::SafetyState currentSafety()
        const noexcept override
    {
        return state;
    }

    void publishReady()
    {
        Q_ASSERT(observer != nullptr);
        observer->sessionSafetyReady();
    }

    void publishSafety(const DisplayTransaction::SafetyState value)
    {
        state = value;
        Q_ASSERT(observer != nullptr);
        observer->sessionSafetyChanged(value);
    }

    void prepareForSleep()
    {
        state = DisplayTransaction::SafetyState::Unknown;
        Q_ASSERT(observer != nullptr);
        observer->sessionSafetyChanged(state);
        observer->sessionPreparingForSleep();
    }

    void resumeWithDelay()
    {
        held = true;
        state = DisplayTransaction::SafetyState::Safe;
        Q_ASSERT(observer != nullptr);
        observer->sessionSafetyChanged(state);
        observer->sessionSafetyReady();
    }

    void loseAuthority(QString reasonCode)
    {
        Q_ASSERT(observer != nullptr);
        observer->sessionAuthorityLost(std::move(reasonCode));
    }

    SessionSafetyObserver *observer = nullptr;
    SessionSafetyStartStatus startStatus = SessionSafetyStartStatus::Started;
    DisplayTransaction::SafetyState state =
        DisplayTransaction::SafetyState::Safe;
    qint64 expectedProcessId = 0;
    int startCalls = 0;
    int stopCalls = 0;
    int releaseCalls = 0;
    bool held = true;
    bool readySynchronously = true;
    bool running = false;
};

inline DisplayTransaction::Journal recoveryJournal(
    const DisplayService::InventoryFrame &initial,
    const Display::Transform targetTransform = Display::Transform::Rotate180)
{
    const DisplayService::InventoryProjectionResult projected =
        DisplayService::projectInventory(initial, QStringLiteral("prior-epoch"));
    Q_ASSERT(projected.accepted());
    Display::Candidate target =
        DisplayTopology::candidateFromSnapshot(projected.snapshot);
    target.outputs[0].transform = targetTransform;
    return {.schemaVersion = DisplayTransaction::kJournalSchemaVersion,
            .transactionId = QStringLiteral("recover-me"),
            .phase = DisplayTransaction::JournalPhase::AwaitingConfirmation,
            .reason = Display::TransactionReason::None,
            .preimage = DisplayTopology::candidateFromSnapshot(projected.snapshot),
            .target = std::move(target),
            .revertAttempt = 0};
}

class RuntimeFixture final
{
public:
    ~RuntimeFixture()
    {
        runtime.reset();
        if (!connectionName.isEmpty()) {
            QDBusConnection::disconnectFromBus(connectionName);
        }
        connection = QDBusConnection(QStringLiteral("released-runtime"));
        bus.stop();
    }

    bool initialize(
        std::optional<DisplayTransaction::Journal> startupJournal = std::nullopt)
    {
        QString error;
        if (!bus.start(&error)) {
            failure = std::move(error);
            return false;
        }
        connectionName = DisplayService::TestSupport::privateConnectionName(
            QStringLiteral("d6-runtime"));
        connection = QDBusConnection::connectToBus(bus.address(), connectionName);
        if (!connection.isConnected()) {
            failure = connection.lastError().message();
            return false;
        }

        auto inventoryValue =
            std::make_unique<DisplayService::TestSupport::FakeInventorySource>();
        inventory = inventoryValue.get();
        auto outputValue =
            std::make_unique<DisplayWriter::TestSupport::FakeOutputManagementPort>();
        output = outputValue.get();
        output->configuredPeerProcessId = QCoreApplication::applicationPid();
        auto storeValue =
            std::make_unique<DisplayWriter::TestSupport::FakeJournalStore>();
        store = storeValue.get();
        auto writer = std::make_unique<DisplayWriter::WriterTransactionPort>(
            std::move(outputValue), std::move(storeValue), 60'000);
        auto safetyValue = std::make_unique<FakeSessionSafetyPort>();
        safety = safetyValue.get();
        runtime = std::make_unique<ResidentDisplayRuntime>(
            std::move(inventoryValue), std::move(writer), std::move(safetyValue),
            std::make_unique<DisplayService::TestSupport::FakeClock>(),
            [] { return QStringLiteral("runtime-epoch-seed"); }, connection,
            QStringLiteral("org.qindaqt.Display1.RuntimeTest"),
            DisplayTransaction::Timing{}, std::move(startupJournal));
        return true;
    }

    DisplayService::TestSupport::PrivateSessionBus bus;
    QString connectionName;
    QString failure;
    QDBusConnection connection{QStringLiteral("uninitialized-runtime")};
    DisplayService::TestSupport::FakeInventorySource *inventory = nullptr;
    DisplayWriter::TestSupport::FakeOutputManagementPort *output = nullptr;
    DisplayWriter::TestSupport::FakeJournalStore *store = nullptr;
    FakeSessionSafetyPort *safety = nullptr;
    std::unique_ptr<ResidentDisplayRuntime> runtime;
};

} // namespace QindaQt::DisplayRuntime::TestSupport
