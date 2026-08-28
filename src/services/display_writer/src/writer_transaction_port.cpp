// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/display_writer/writer_transaction_port.h>

#include <QtCore/QMetaObject>

#include <algorithm>
#include <limits>

namespace QindaQt::DisplayWriter
{

WriterTransactionPort::WriterTransactionPort(
    std::unique_ptr<OutputManagementPort> outputManagement,
    std::unique_ptr<JournalStore> journalStore,
    const quint64 requestTimeoutMilliseconds, QObject *parent)
    : QObject(parent)
    , m_outputManagement(std::move(outputManagement))
    , m_journalStore(std::move(journalStore))
{
    Q_ASSERT(m_outputManagement != nullptr);
    Q_ASSERT(m_journalStore != nullptr);
    m_timeout.setSingleShot(true);
    m_timeout.setTimerType(Qt::PreciseTimer);
    m_timeout.setInterval(static_cast<int>(std::clamp<quint64>(
        requestTimeoutMilliseconds, 1,
        static_cast<quint64>(std::numeric_limits<int>::max()))));
    QObject::connect(&m_timeout, &QTimer::timeout, this, [this] {
        finishPending(DisplayTransaction::ApplyOutcome::TransportUncertain);
    });
    m_outputManagement->setObserver(this);
}

WriterTransactionPort::~WriterTransactionPort()
{
    stop();
    m_outputManagement->setObserver(nullptr);
}

PortStartStatus WriterTransactionPort::start()
{
    if (m_started) {
        return PortStartStatus::AlreadyStarted;
    }
    // AGENT-GUARD: A concrete transport may detach its borrowed observer when
    // stopped. Rebinding every start makes stop/start a real lifecycle rather
    // than leaving the writer permanently blind after its first stop.
    m_outputManagement->setObserver(this);
    const PortStartStatus status = m_outputManagement->start();
    m_started = status == PortStartStatus::Started
        || status == PortStartStatus::AlreadyStarted;
    return status;
}

void WriterTransactionPort::stop()
{
    if (!m_started) {
        return;
    }
    m_timeout.stop();
    if (m_pending) {
        const Pending pending = *m_pending;
        m_pending.reset();
        finishDeferred(pending.machineLineage, pending.token,
                       DisplayTransaction::ApplyOutcome::TransportUncertain);
    }
    m_outputManagement->stop();
    m_started = false;
    m_available = false;
    m_ownerGeneration = 0;
}

bool WriterTransactionPort::isStarted() const noexcept
{
    return m_started;
}

bool WriterTransactionPort::isOutputManagementAvailable() const noexcept
{
    return m_available;
}

void WriterTransactionPort::setObserver(
    DisplayService::TransactionPortObserver *observer)
{
    m_observer = observer;
}

void WriterTransactionPort::beginMachineLineage(const quint64 machineLineage)
{
    if (m_pending && machineLineage != m_machineLineage) {
        finishPending(DisplayTransaction::ApplyOutcome::TransportUncertain);
    }
    m_machineLineage = machineLineage;
}

bool WriterTransactionPort::storeJournal(
    const DisplayTransaction::Journal &journal)
{
    return m_journalStore->store(journal);
}

bool WriterTransactionPort::clearJournal()
{
    return m_journalStore->clear();
}

void WriterTransactionPort::requestApply(
    const DisplayTransaction::ApplyRequest &request)
{
    const quint64 lineage = m_machineLineage;
    if (lineage == 0 || !m_started || !m_available || m_ownerGeneration == 0) {
        finishDeferred(lineage, request.token,
                       DisplayTransaction::ApplyOutcome::TransportUncertain);
        return;
    }
    if (m_pending) {
        finishDeferred(lineage, request.token,
                       DisplayTransaction::ApplyOutcome::Rejected);
        return;
    }

    const quint64 requestId = nextRequestId();
    const MapResult mapped = mapApplyRequest(request, requestId);
    if (!mapped.accepted()) {
        finishDeferred(lineage, request.token,
                       DisplayTransaction::ApplyOutcome::Rejected);
        return;
    }

    // AGENT-GUARD: Publish the fence before submit(). A hostile injected port
    // may violate its async contract; such a callback must match this exact
    // tuple or be ignored, never attach to a later Display1 token.
    m_pending = Pending{.machineLineage = lineage,
                        .token = request.token,
                        .requestId = requestId,
                        .ownerGeneration = m_ownerGeneration};
    const SubmitStatus status = m_outputManagement->submit(mapped.configuration);
    switch (status) {
    case SubmitStatus::Accepted:
        if (m_pending) {
            m_timeout.start();
        }
        return;
    case SubmitStatus::Unsupported:
    case SubmitStatus::Malformed:
        finishPending(DisplayTransaction::ApplyOutcome::Rejected);
        return;
    case SubmitStatus::Unavailable:
    case SubmitStatus::Busy:
        finishPending(DisplayTransaction::ApplyOutcome::TransportUncertain);
        return;
    }
}

void WriterTransactionPort::outputManagementOwnerChanged(
    const quint64 ownerGeneration, const bool available)
{
    const bool fenceChanged = ownerGeneration == 0
        || ownerGeneration != m_ownerGeneration || !available;
    if (m_pending && fenceChanged) {
        finishPending(DisplayTransaction::ApplyOutcome::TransportUncertain);
    }
    m_ownerGeneration = ownerGeneration;
    m_available = m_started && available && ownerGeneration != 0;
}

void WriterTransactionPort::outputManagementCompleted(
    const quint64 ownerGeneration, const quint64 requestId,
    const CompletionOutcome outcome)
{
    if (!m_pending || m_pending->ownerGeneration != ownerGeneration
        || m_pending->requestId != requestId || ownerGeneration != m_ownerGeneration) {
        return;
    }
    switch (outcome) {
    case CompletionOutcome::Applied:
        finishPending(DisplayTransaction::ApplyOutcome::Applied);
        return;
    case CompletionOutcome::Rejected:
        finishPending(DisplayTransaction::ApplyOutcome::Rejected);
        return;
    case CompletionOutcome::TransportUncertain:
    case CompletionOutcome::Malformed:
        finishPending(DisplayTransaction::ApplyOutcome::TransportUncertain);
        return;
    }
}

void WriterTransactionPort::finishPending(
    const DisplayTransaction::ApplyOutcome outcome)
{
    if (!m_pending) {
        return;
    }
    m_timeout.stop();
    const Pending completed = *m_pending;
    m_pending.reset();
    finishDeferred(completed.machineLineage, completed.token, outcome);
}

void WriterTransactionPort::finishDeferred(
    const quint64 machineLineage, const quint64 token,
    const DisplayTransaction::ApplyOutcome outcome)
{
    QMetaObject::invokeMethod(
        this,
        [this, machineLineage, token, outcome] {
            if (m_observer != nullptr) {
                m_observer->applyCompleted(machineLineage, token, outcome);
            }
        },
        Qt::QueuedConnection);
}

quint64 WriterTransactionPort::nextRequestId()
{
    const quint64 value = m_nextRequestId;
    m_nextRequestId = value == std::numeric_limits<quint64>::max() ? 1 : value + 1;
    return value;
}

} // namespace QindaQt::DisplayWriter
