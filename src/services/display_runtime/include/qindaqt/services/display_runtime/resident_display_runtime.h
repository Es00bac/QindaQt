// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/display_runtime/session_safety_port.h>
#include <qindaqt/services/display_service/resident_display_service.h>
#include <qindaqt/services/display_writer/writer_transaction_port.h>

#include <QtCore/QObject>

#include <memory>
#include <optional>

namespace QindaQt::DisplayRuntime
{

enum class RuntimeStartStatus {
    Started,
    AlreadyStarted,
    TerminallyFailed,
    InvalidStartupJournal,
    WriterStartFailed,
    InvalidCompositorIdentity,
    SessionSafetyStartFailed,
};

class ResidentDisplayRuntime final : public QObject,
                                     private SessionSafetyObserver
{
    Q_OBJECT

public:
    // Takes exclusive ownership of every collaborator. The WriterTransactionPort
    // is moved into the resident service but remains addressable for the outer
    // lifetime; service destruction precedes all borrowed dependencies. A valid
    // startup journal is retained until the first complete inventory enters D1
    // recover(). All operations and callbacks are confined to this Qt thread.
    ResidentDisplayRuntime(
        std::unique_ptr<DisplayService::InventorySource> inventorySource,
        std::unique_ptr<DisplayWriter::WriterTransactionPort> transactionPort,
        std::unique_ptr<SessionSafetyPort> sessionSafety,
        std::unique_ptr<DisplayTransaction::MonotonicClock> clock,
        DisplayService::EpochFactory epochFactory,
        const QDBusConnection &sessionConnection,
        QString serviceName = {}, DisplayTransaction::Timing timing = {},
        std::optional<DisplayTransaction::Journal> startupJournal = std::nullopt,
        QObject *parent = nullptr);
    ~ResidentDisplayRuntime() override;

    [[nodiscard]] RuntimeStartStatus start();
    void stop();
    [[nodiscard]] bool isReady() const noexcept;
    [[nodiscard]] bool hasFailed() const noexcept;
    [[nodiscard]] DisplayService::ResidentDisplayService *resident() noexcept;

Q_SIGNALS:
    void ready();
    void fatalError(const QString &reasonCode);

private:
    void sessionSafetyReady() override;
    void sessionSafetyChanged(
        DisplayTransaction::SafetyState safety) override;
    void sessionPreparingForSleep() override;
    void sessionAuthorityLost(const QString &reasonCode) override;
    void writerAuthorityChanged(bool available);
    void updateCombinedSafety();
    void releaseSuspendDelayWhenSafe();
    void fail(QString reasonCode);

    DisplayWriter::WriterTransactionPort *m_writer = nullptr;
    std::unique_ptr<SessionSafetyPort> m_sessionSafety;
    std::unique_ptr<DisplayService::ResidentDisplayService> m_resident;
    std::optional<DisplayTransaction::Journal> m_startupJournal;
    DisplayTransaction::SafetyState m_sessionState =
        DisplayTransaction::SafetyState::Unknown;
    bool m_writerAvailable = false;
    bool m_starting = false;
    bool m_ready = false;
    bool m_failed = false;
    bool m_stopping = false;
    bool m_suspendPending = false;
};

} // namespace QindaQt::DisplayRuntime
