// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/display_service/resident_display_service.h>

#include "display_service_object_p.h"

#include <qindaqt/services/display_protocol/display_dbus.h>
#include <qindaqt/services/display_protocol/display_limits.h>

#include <QtDBus/QDBusConnectionInterface>

#include <algorithm>
#include <limits>

namespace QindaQt::DisplayService
{
namespace
{

constexpr int kTopologyQuietWindowMilliseconds = 500;

} // namespace

ResidentDisplayService::ResidentDisplayService(
    std::unique_ptr<InventorySource> inventorySource,
    std::unique_ptr<TransactionPort> transactionPort,
    std::unique_ptr<DisplayTransaction::MonotonicClock> clock,
    EpochFactory epochFactory, const QDBusConnection &connection, QString serviceName,
    DisplayTransaction::Timing timing,
    std::optional<DisplayTransaction::Journal> startupJournal, QObject *parent)
    : QObject(parent)
    , m_inventorySource(std::move(inventorySource))
    , m_clock(std::move(clock))
    , m_transactionPort(std::move(transactionPort))
    , m_connection(connection)
    , m_serviceName(serviceName.isEmpty() ? QString::fromLatin1(Display::kServiceName)
                                         : std::move(serviceName))
{
    Q_ASSERT(m_inventorySource != nullptr);
    Q_ASSERT(m_clock != nullptr);
    Q_ASSERT(m_transactionPort != nullptr);
    Display::registerDBusTypes();
    m_model = std::make_unique<DisplayServiceModel>(
        *m_clock, *m_transactionPort, std::move(epochFactory), timing,
        std::move(startupJournal));
    m_serviceObject = std::make_unique<DisplayServiceObject>(
        *m_model, [this](const bool changed) { modelTransitioned(changed); },
        [this] { serviceBrightness(); });
    m_brightnessTimer = new QTimer(this);
    m_brightnessTimer->setSingleShot(true);
    m_brightnessTimer->setTimerType(Qt::PreciseTimer);
    QObject::connect(m_brightnessTimer, &QTimer::timeout, this, [this] {
        m_model->brightnessTick();
        serviceBrightness();
    });
    m_deadlineTimer = new QTimer(this);
    m_deadlineTimer->setSingleShot(true);
    m_deadlineTimer->setTimerType(Qt::PreciseTimer);
    QObject::connect(m_deadlineTimer, &QTimer::timeout, this, [this] {
        const DisplayTransaction::CommandResult result = m_model->tick();
        modelTransitioned(result.stateChanged);
        if (!result.stateChanged) {
            // AGENT-GUARD: Even a precise timer may observe a clock that has
            // not reached the model deadline. Keep the deadline live instead
            // of silently dropping the only tick.
            armDeadline();
        }
        if (result.stateChanged) {
            m_serviceObject->notifyChanged();
        }
    });
    m_topologySettleTimer = new QTimer(this);
    m_topologySettleTimer->setSingleShot(true);
    m_topologySettleTimer->setTimerType(Qt::PreciseTimer);
    m_topologySettleTimer->setInterval(kTopologyQuietWindowMilliseconds);
    QObject::connect(m_topologySettleTimer, &QTimer::timeout, this, [this] {
        const DisplayTransaction::CommandResult result =
            m_model->topologySettled();
        modelTransitioned(result.stateChanged);
        if (result.stateChanged) {
            m_serviceObject->notifyChanged();
        }
    });
}

ResidentDisplayService::~ResidentDisplayService()
{
    stop();
}

ServiceStartStatus ResidentDisplayService::start()
{
    if (isRunning()) {
        return ServiceStartStatus::Started;
    }
    if (!m_connection.isConnected() || m_connection.interface() == nullptr) {
        return ServiceStartStatus::InvalidConnection;
    }
    const QString objectPath = QString::fromLatin1(Display::kObjectPath);
    if (!m_connection.registerObject(objectPath, m_serviceObject.get(),
                                     QDBusConnection::ExportScriptableSlots
                                         | QDBusConnection::ExportScriptableSignals)) {
        return ServiceStartStatus::ObjectRegistrationFailed;
    }
    m_objectRegistered = true;
    if (!m_connection.registerService(m_serviceName)) {
        const bool alreadyOwned = m_connection.lastError().name()
            == QStringLiteral("org.freedesktop.DBus.Error.NameExists");
        stop();
        return alreadyOwned ? ServiceStartStatus::NameAlreadyOwned
                            : ServiceStartStatus::NameRegistrationFailed;
    }
    m_nameRegistered = true;
    m_inventorySource->setObserver(this);
    m_transactionPort->setObserver(this);
    if (m_inventorySource->start() != InventorySourceStartStatus::Started) {
        stop();
        return ServiceStartStatus::InventorySourceFailed;
    }
    return ServiceStartStatus::Started;
}

void ResidentDisplayService::stop()
{
    m_deadlineTimer->stop();
    m_topologySettleTimer->stop();
    m_inventorySource->stop();
    m_inventorySource->setObserver(nullptr);
    m_transactionPort->setObserver(nullptr);
    (void)m_model->transportLost();
    // An accepted immediate request receives its typed uncertainty before the
    // object leaves the bus; stop() itself publishes no Changed hint.
    (void)m_model->takeBrightnessPublicationChanged();
    for (const BrightnessFinish &finish : m_model->takeBrightnessFinishes()) {
        m_serviceObject->finishBrightness(finish);
    }
    m_brightnessTimer->stop();
    if (m_nameRegistered) {
        m_connection.unregisterService(m_serviceName);
        m_nameRegistered = false;
    }
    if (m_objectRegistered) {
        m_connection.unregisterObject(QString::fromLatin1(Display::kObjectPath));
        m_objectRegistered = false;
    }
}

bool ResidentDisplayService::isRunning() const noexcept
{
    return m_nameRegistered && m_objectRegistered;
}

DisplayServiceModel *ResidentDisplayService::model() noexcept
{
    return m_model.get();
}

DisplayTransaction::CommandResult ResidentDisplayService::setSafetyState(
    const DisplayTransaction::SafetyState safety)
{
    const DisplayTransaction::CommandResult result = m_model->safetyChanged(safety);
    modelTransitioned(result.stateChanged);
    if (result.stateChanged) {
        m_serviceObject->notifyChanged();
    }
    return result;
}

DisplayTransaction::CommandResult ResidentDisplayService::prepareForSuspend()
{
    const DisplayTransaction::CommandResult result = m_model->prepareForSuspend();
    modelTransitioned(result.stateChanged);
    if (result.stateChanged) {
        m_serviceObject->notifyChanged();
    }
    return result;
}

void ResidentDisplayService::inventoryObserved(const InventoryFrame &frame)
{
    const InventoryObservationResult result = m_model->observeInventory(frame);
    if (!result.accepted()) {
        // AGENT-CONTRACT: DisplayServiceModel owns exact-owner replacement and
        // reports its resulting availability edge through stateChanged. Every
        // rejected same-owner observation preserves live truth and reports no
        // change; failed replacement-owner establishment revokes the old
        // lineage and reports a change. A reason code is diagnostic, never
        // authority policy.
        if (result.stateChanged) {
            modelTransitioned(true);
            m_serviceObject->notifyChanged();
        }
        serviceBrightness(result.stateChanged);
        return;
    }
    modelTransitioned(result.stateChanged);
    syncTopologySettleTimer(
        result.status == InventoryObservationStatus::AcceptedNewLineage
        || result.status == InventoryObservationStatus::AcceptedChanged);
    const bool published = result.stateChanged
        || result.status == InventoryObservationStatus::AcceptedNewLineage;
    if (published) {
        m_serviceObject->notifyChanged();
    }
    serviceBrightness(published);
}

void ResidentDisplayService::inventoryUnavailable()
{
    const bool lost = m_model->transportLost();
    if (lost) {
        m_deadlineTimer->stop();
        m_topologySettleTimer->stop();
        Q_EMIT modelStateChanged();
        m_serviceObject->notifyChanged();
    }
    serviceBrightness(lost);
}

void ResidentDisplayService::applyCompleted(
    const quint64 machineLineage, const quint64 token,
    const DisplayTransaction::ApplyOutcome outcome)
{
    const DisplayTransaction::CommandResult result =
        m_model->applyCompleted(machineLineage, token, outcome);
    modelTransitioned(result.stateChanged);
    if (result.stateChanged) {
        m_serviceObject->notifyChanged();
    }
}

void ResidentDisplayService::brightnessDevicesObserved(const DeviceBrightnessFrame &frame)
{
    m_model->brightnessDevicesObserved(frame);
    serviceBrightness();
}

void ResidentDisplayService::brightnessCompleted(const quint64 requestId,
                                                 const BrightnessApplyOutcome outcome)
{
    m_model->brightnessCompleted(requestId, outcome);
    serviceBrightness();
}

void ResidentDisplayService::serviceBrightness(const bool changedAlreadyPublished)
{
    // AGENT-CONTRACT: Publish before replying. One Changed hint covers both
    // publications, and an accepted immediate request replies only after the
    // hint for the republish that proved it.
    if (m_model->takeBrightnessPublicationChanged() && !changedAlreadyPublished) {
        m_serviceObject->notifyChanged();
    }
    for (const BrightnessFinish &finish : m_model->takeBrightnessFinishes()) {
        m_serviceObject->finishBrightness(finish);
    }
    const quint64 deadline = m_model->brightnessDeadlineMonotonicMilliseconds();
    if (deadline == 0) {
        m_brightnessTimer->stop();
        return;
    }
    const quint64 now = m_clock->nowMilliseconds();
    const quint64 remaining = deadline > now ? deadline - now : 0;
    m_brightnessTimer->start(static_cast<int>(std::min<quint64>(
        remaining, static_cast<quint64>(std::numeric_limits<int>::max()))));
}

void ResidentDisplayService::modelTransitioned(const bool changed)
{
    if (changed) {
        armDeadline();
        syncTopologySettleTimer();
        Q_EMIT modelStateChanged();
    }
}

void ResidentDisplayService::armDeadline()
{
    const DisplayTransaction::MachineView *view = m_model->view();
    if (view == nullptr || view->deadlineMonotonicMilliseconds == 0) {
        m_deadlineTimer->stop();
        return;
    }
    const quint64 now = m_clock->nowMilliseconds();
    const quint64 remaining = view->deadlineMonotonicMilliseconds > now
        ? view->deadlineMonotonicMilliseconds - now
        : 0;
    const int interval = static_cast<int>(std::min<quint64>(
        remaining, static_cast<quint64>(std::numeric_limits<int>::max())));
    m_deadlineTimer->start(interval);
}

void ResidentDisplayService::syncTopologySettleTimer(
    const bool acceptedInventoryChanged)
{
    const DisplayTransaction::MachineView *view = m_model->view();
    if (view == nullptr
        || view->state != DisplayTransaction::MachineState::SettlingTopology) {
        m_topologySettleTimer->stop();
        return;
    }
    if (acceptedInventoryChanged || !m_topologySettleTimer->isActive()) {
        // AGENT-CONTRACT: D2 owns the accepted-inventory quiet window; D1
        // deliberately has no wall-clock hotplug policy. Every changed frame
        // restarts this single timer, and only its expiry routes settle.
        m_topologySettleTimer->start();
    }
}

} // namespace QindaQt::DisplayService
