// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/display_runtime/resident_display_runtime.h>

#include <qindaqt/services/display_transaction/transaction_journal.h>

#include <utility>

namespace QindaQt::DisplayRuntime
{

ResidentDisplayRuntime::ResidentDisplayRuntime(
    std::unique_ptr<DisplayService::InventorySource> inventorySource,
    std::unique_ptr<DisplayWriter::WriterTransactionPort> transactionPort,
    std::unique_ptr<SessionSafetyPort> sessionSafety,
    std::unique_ptr<DisplayTransaction::MonotonicClock> clock,
    DisplayService::EpochFactory epochFactory,
    const QDBusConnection &sessionConnection, QString serviceName,
    DisplayTransaction::Timing timing,
    std::optional<DisplayTransaction::Journal> startupJournal, QObject *parent)
    : QObject(parent)
    , m_writer(transactionPort.get())
    , m_sessionSafety(std::move(sessionSafety))
    , m_startupJournal(startupJournal)
{
    Q_ASSERT(m_writer != nullptr);
    Q_ASSERT(m_sessionSafety != nullptr);
    m_resident = std::make_unique<DisplayService::ResidentDisplayService>(
        std::move(inventorySource), std::move(transactionPort), std::move(clock),
        std::move(epochFactory), sessionConnection, std::move(serviceName), timing,
        std::move(startupJournal));
    QObject::connect(m_writer,
                     &DisplayWriter::WriterTransactionPort::mutationAuthorityChanged,
                     this, &ResidentDisplayRuntime::writerAuthorityChanged);
    QObject::connect(m_resident.get(),
                     &DisplayService::ResidentDisplayService::modelStateChanged,
                     this, &ResidentDisplayRuntime::releaseSuspendDelayWhenSafe);
}

ResidentDisplayRuntime::~ResidentDisplayRuntime()
{
    stop();
}

RuntimeStartStatus ResidentDisplayRuntime::start()
{
    if (m_failed) {
        return RuntimeStartStatus::TerminallyFailed;
    }
    if (m_starting || m_ready) {
        return RuntimeStartStatus::AlreadyStarted;
    }
    if (m_startupJournal
        && !DisplayTransaction::isValidJournal(*m_startupJournal)) {
        return RuntimeStartStatus::InvalidStartupJournal;
    }
    const DisplayWriter::PortStartStatus writerStatus = m_writer->start();
    if (writerStatus != DisplayWriter::PortStartStatus::Started
        && writerStatus != DisplayWriter::PortStartStatus::AlreadyStarted) {
        return RuntimeStartStatus::WriterStartFailed;
    }
    const qint64 compositorProcessId = m_writer->compositorProcessId();
    if (compositorProcessId <= 1) {
        m_writer->stop();
        return RuntimeStartStatus::InvalidCompositorIdentity;
    }
    m_writerAvailable = m_writer->isOutputManagementAvailable();
    m_sessionSafety->setObserver(this);
    // AGENT-GUARD: A deterministic/private-bus safety port may establish its
    // delay authority synchronously in start(). Mark startup first so the
    // observer can complete the resident transition without start() later
    // overwriting that completed state.
    m_starting = true;
    const SessionSafetyStartStatus safetyStatus =
        m_sessionSafety->start(compositorProcessId);
    if (safetyStatus != SessionSafetyStartStatus::Started
        && safetyStatus != SessionSafetyStartStatus::AlreadyStarted) {
        m_starting = false;
        m_sessionSafety->setObserver(nullptr);
        m_writer->stop();
        return RuntimeStartStatus::SessionSafetyStartFailed;
    }
    // A deterministic port may synchronously establish its delay descriptor.
    // If that ready callback exposed a resident startup failure, tear down the
    // already-open authorities before returning to a caller that has not yet
    // entered its event loop.
    if (m_failed) {
        m_sessionSafety->stop();
        m_sessionSafety->setObserver(nullptr);
        m_writer->stop();
        return RuntimeStartStatus::TerminallyFailed;
    }
    m_sessionState = m_sessionSafety->currentSafety();
    updateCombinedSafety();
    return RuntimeStartStatus::Started;
}

void ResidentDisplayRuntime::stop()
{
    if (m_stopping) {
        return;
    }
    m_stopping = true;
    m_ready = false;
    m_starting = false;
    m_suspendPending = false;
    m_sessionSafety->stop();
    m_sessionSafety->setObserver(nullptr);
    m_resident->stop();
    m_writer->stop();
    m_writerAvailable = false;
    m_sessionState = DisplayTransaction::SafetyState::Unknown;
    m_stopping = false;
}

bool ResidentDisplayRuntime::isReady() const noexcept
{
    return m_ready && !m_failed;
}

bool ResidentDisplayRuntime::hasFailed() const noexcept
{
    return m_failed;
}

DisplayService::ResidentDisplayService *ResidentDisplayRuntime::resident() noexcept
{
    return m_resident.get();
}

void ResidentDisplayRuntime::sessionSafetyReady()
{
    if (m_stopping || m_failed) {
        return;
    }
    // The safety port emits ready only after a fresh exact-owner delay FD is
    // held. On resume this is the event that closes the suspend-uncertainty
    // interval; a mere unlocked signal cannot do so.
    m_suspendPending = false;
    m_sessionState = m_sessionSafety->currentSafety();
    if (!m_ready) {
        const DisplayService::ServiceStartStatus status = m_resident->start();
        if (status != DisplayService::ServiceStartStatus::Started) {
            fail(QStringLiteral("resident-start-failed"));
            return;
        }
        m_starting = false;
        m_ready = true;
        Q_EMIT ready();
    }
    updateCombinedSafety();
}

void ResidentDisplayRuntime::sessionSafetyChanged(
    const DisplayTransaction::SafetyState safety)
{
    m_sessionState = safety;
    updateCombinedSafety();
}

void ResidentDisplayRuntime::sessionPreparingForSleep()
{
    if (m_stopping || m_failed) {
        return;
    }
    m_suspendPending = true;
    m_sessionState = DisplayTransaction::SafetyState::Unknown;
    updateCombinedSafety();
    if (m_ready) {
        (void)m_resident->prepareForSuspend();
    }
    releaseSuspendDelayWhenSafe();
}

void ResidentDisplayRuntime::sessionAuthorityLost(const QString &reasonCode)
{
    if (m_ready) {
        m_sessionState = DisplayTransaction::SafetyState::Unknown;
        updateCombinedSafety();
        (void)m_resident->prepareForSuspend();
    }
    fail(reasonCode.isEmpty() ? QStringLiteral("session-authority-lost")
                              : reasonCode);
}

void ResidentDisplayRuntime::writerAuthorityChanged(const bool available)
{
    m_writerAvailable = available;
    updateCombinedSafety();
}

void ResidentDisplayRuntime::updateCombinedSafety()
{
    if (!m_ready) {
        return;
    }
    DisplayTransaction::SafetyState combined =
        DisplayTransaction::SafetyState::Unknown;
    if (m_sessionState == DisplayTransaction::SafetyState::Locked) {
        combined = DisplayTransaction::SafetyState::Locked;
    } else if (m_sessionState == DisplayTransaction::SafetyState::Safe
               && m_sessionSafety->delayHeld() && m_writerAvailable
               && !m_suspendPending) {
        combined = DisplayTransaction::SafetyState::Safe;
    }
    (void)m_resident->setSafetyState(combined);
}

void ResidentDisplayRuntime::releaseSuspendDelayWhenSafe()
{
    if (!m_suspendPending || !m_sessionSafety->delayHeld()) {
        return;
    }
    const DisplayTransaction::MachineView *view = m_resident->model()->view();
    if (view != nullptr
        && view->state == DisplayTransaction::MachineState::Applying) {
        return;
    }
    // AGENT-CONTRACT: A forward apply is the only state that must retain the
    // logind delay. Every later mutation-capable state has a durable D5 journal
    // and can recover after resume if the compositor stops processing replies.
    m_sessionSafety->releaseSleepDelay();
}

void ResidentDisplayRuntime::fail(QString reasonCode)
{
    if (m_failed || m_stopping) {
        return;
    }
    m_failed = true;
    m_ready = false;
    m_starting = false;
    Q_EMIT fatalError(reasonCode);
}

} // namespace QindaQt::DisplayRuntime
