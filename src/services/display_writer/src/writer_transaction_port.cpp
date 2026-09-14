// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/display_writer/writer_transaction_port.h>

#include <QtCore/QMetaObject>

#include <algorithm>
#include <limits>
#include <utility>

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
    m_brightnessTimeout.setSingleShot(true);
    m_brightnessTimeout.setTimerType(Qt::PreciseTimer);
    m_brightnessTimeout.setInterval(m_timeout.interval());
    QObject::connect(&m_brightnessTimeout, &QTimer::timeout, this, [this] {
        finishBrightness(DisplayService::BrightnessApplyOutcome::TransportUncertain);
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
    finishBrightness(DisplayService::BrightnessApplyOutcome::TransportUncertain);
    m_outputManagement->stop();
    const bool wasAvailable = m_available;
    m_started = false;
    m_available = false;
    m_ownerGeneration = 0;
    m_hasDevicesFrame = false;
    m_lastDevicesFrame = {};
    outputManagementDevicesObserved(0, {});
    if (wasAvailable) {
        Q_EMIT mutationAuthorityChanged(false);
    }
}

bool WriterTransactionPort::isStarted() const noexcept
{
    return m_started;
}

bool WriterTransactionPort::isOutputManagementAvailable() const noexcept
{
    return m_available;
}

qint64 WriterTransactionPort::compositorProcessId() const noexcept
{
    return m_started ? m_outputManagement->peerProcessId() : 0;
}

void WriterTransactionPort::setObserver(
    DisplayService::TransactionPortObserver *observer)
{
    m_observer = observer;
    // AGENT-GUARD: replay the current accepted device facts to a late binder
    // (the resident service binds only after session-safety readiness, while
    // the compositor publishes brightness devices during runtime startup).
    // Delivered queued, like every live frame, so binding never reenters the
    // caller synchronously and never receives a stale-generation frame.
    if (m_observer == nullptr || !m_hasDevicesFrame || !m_started || !m_available
        || m_lastDevicesFrame.ownerGeneration != m_ownerGeneration) {
        return;
    }
    const DisplayService::DeviceBrightnessFrame frame = m_lastDevicesFrame;
    QMetaObject::invokeMethod(
        this,
        [this, frame] {
            if (m_observer != nullptr) {
                m_observer->brightnessDevicesObserved(frame);
            }
        },
        Qt::QueuedConnection);
}

void WriterTransactionPort::beginMachineLineage(const quint64 machineLineage)
{
    if (m_pending && machineLineage != m_machineLineage) {
        finishPending(DisplayTransaction::ApplyOutcome::TransportUncertain);
    }
    m_machineLineage = machineLineage;
}

DisplayTransaction::JournalMutationOutcome WriterTransactionPort::storeJournal(
    const DisplayTransaction::Journal &journal)
{
    return m_journalStore->store(journal);
}

DisplayTransaction::JournalMutationOutcome WriterTransactionPort::clearJournal()
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
    if (m_pending || m_brightnessPending) {
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

DisplayService::BrightnessSubmitStatus WriterTransactionPort::requestBrightness(
    const DisplayService::BrightnessApplyRequest &request)
{
    using Status = DisplayService::BrightnessSubmitStatus;
    if (!m_started || !m_available || m_ownerGeneration == 0
        || request.ownerGeneration != m_ownerGeneration) {
        return Status::Unavailable;
    }
    if (m_pending || m_brightnessPending) {
        return Status::Busy;
    }
    const quint64 requestId = nextRequestId();
    // AGENT-GUARD: Publish the fence before submitBrightness(), matching the
    // topology path: a callback must match this exact tuple or be ignored.
    m_brightnessPending = BrightnessPending{.serviceRequestId = request.requestId,
                                            .requestId = requestId,
                                            .ownerGeneration = m_ownerGeneration};
    const SubmitStatus status = m_outputManagement->submitBrightness(
        {.requestId = requestId,
         .connectorName = request.connectorName,
         .uuid = request.runtimeUuid,
         .brightness = request.value});
    switch (status) {
    case SubmitStatus::Accepted:
        if (m_brightnessPending) {
            m_brightnessTimeout.start();
        }
        return Status::Accepted;
    case SubmitStatus::Unavailable:
        m_brightnessPending.reset();
        return Status::Unavailable;
    case SubmitStatus::Busy:
        m_brightnessPending.reset();
        return Status::Busy;
    case SubmitStatus::Unsupported:
        m_brightnessPending.reset();
        return Status::Unsupported;
    case SubmitStatus::Malformed:
        m_brightnessPending.reset();
        return Status::Malformed;
    }
    m_brightnessPending.reset();
    return Status::Malformed;
}

void WriterTransactionPort::outputManagementOwnerChanged(
    const quint64 ownerGeneration, const bool available)
{
    const bool fenceChanged = ownerGeneration == 0
        || ownerGeneration != m_ownerGeneration || !available;
    if (m_pending && fenceChanged) {
        finishPending(DisplayTransaction::ApplyOutcome::TransportUncertain);
    }
    if (m_brightnessPending && fenceChanged) {
        finishBrightness(DisplayService::BrightnessApplyOutcome::TransportUncertain);
    }
    m_ownerGeneration = ownerGeneration;
    const bool nextAvailable = m_started && available && ownerGeneration != 0;
    const bool changed = nextAvailable != m_available;
    m_available = nextAvailable;
    if (changed) {
        Q_EMIT mutationAuthorityChanged(m_available);
    }
}

void WriterTransactionPort::outputManagementCompleted(
    const quint64 ownerGeneration, const quint64 requestId,
    const CompletionOutcome outcome)
{
    if (m_brightnessPending && m_brightnessPending->ownerGeneration == ownerGeneration
        && m_brightnessPending->requestId == requestId
        && ownerGeneration == m_ownerGeneration) {
        finishBrightness(outcome == CompletionOutcome::Applied
                             ? DisplayService::BrightnessApplyOutcome::Applied
                             : outcome == CompletionOutcome::Rejected
                             ? DisplayService::BrightnessApplyOutcome::Rejected
                             : DisplayService::BrightnessApplyOutcome::TransportUncertain);
        return;
    }
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

void WriterTransactionPort::outputManagementDevicesObserved(
    const quint64 ownerGeneration, const QList<OutputDeviceState> &devices)
{
    DisplayService::DeviceBrightnessFrame frame;
    if (m_started && m_available && ownerGeneration != 0
        && ownerGeneration == m_ownerGeneration) {
        frame.ownerGeneration = ownerGeneration;
        frame.devices.reserve(devices.size());
        for (const OutputDeviceState &device : devices) {
            frame.devices.push_back({.connectorName = device.connectorName,
                                     .runtimeUuid = device.uuid,
                                     .enabled = device.enabled,
                                     .capable = device.brightnessCapable,
                                     .observed = device.brightnessObserved,
                                     .value = device.brightness});
        }
    }
    // Record accepted facts for the late-binder replay; an owner-loss frame
    // (generation zero) clears them so a rebind never resurrects dead truth.
    if (!frame.devices.isEmpty()) {
        m_lastDevicesFrame = frame;
        m_hasDevicesFrame = true;
    } else if (frame.ownerGeneration == 0) {
        m_hasDevicesFrame = false;
        m_lastDevicesFrame = {};
    }
    // Queued like completions so the port never reenters the D2 observer
    // synchronously and device facts keep their arrival order.
    // AGENT-GUARD: the forwarding observer is captured at publication, not at
    // delivery: a frame published while no resident observer was bound must
    // not land later just because the binding raced the event loop. The late
    // binder instead receives exactly one replay from setObserver().
    auto *const forwardingObserver = m_observer;
    QMetaObject::invokeMethod(
        this,
        [this, forwardingObserver, frame] {
            if (m_observer == forwardingObserver && forwardingObserver != nullptr) {
                forwardingObserver->brightnessDevicesObserved(frame);
            }
        },
        Qt::QueuedConnection);
}

void WriterTransactionPort::finishBrightness(
    const DisplayService::BrightnessApplyOutcome outcome)
{
    if (!m_brightnessPending) {
        return;
    }
    m_brightnessTimeout.stop();
    const quint64 serviceRequestId =
        std::exchange(m_brightnessPending, std::nullopt)->serviceRequestId;
    QMetaObject::invokeMethod(
        this,
        [this, serviceRequestId, outcome] {
            if (m_observer != nullptr) {
                m_observer->brightnessCompleted(serviceRequestId, outcome);
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
