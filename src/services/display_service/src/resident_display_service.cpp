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

ResidentDisplayService::ResidentDisplayService(
    std::unique_ptr<InventorySource> inventorySource,
    std::unique_ptr<TransactionPort> transactionPort,
    std::unique_ptr<DisplayTransaction::MonotonicClock> clock,
    EpochFactory epochFactory, const QDBusConnection &connection, QString serviceName,
    DisplayTransaction::Timing timing, QObject *parent)
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
        *m_clock, *m_transactionPort, std::move(epochFactory), timing);
    m_serviceObject = std::make_unique<DisplayServiceObject>(
        *m_model, [this](const bool changed) { modelTransitioned(changed); });
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
    m_inventorySource->stop();
    m_inventorySource->setObserver(nullptr);
    m_transactionPort->setObserver(nullptr);
    (void)m_model->transportLost();
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

void ResidentDisplayService::inventoryObserved(const InventoryFrame &frame)
{
    const InventoryObservationResult result = m_model->observeInventory(frame);
    if (result.accepted()) {
        modelTransitioned(result.stateChanged);
        if (result.stateChanged
            || result.status == InventoryObservationStatus::AcceptedNewLineage) {
            m_serviceObject->notifyChanged();
        }
    }
}

void ResidentDisplayService::inventoryUnavailable()
{
    if (m_model->transportLost()) {
        m_deadlineTimer->stop();
        m_serviceObject->notifyChanged();
    }
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

void ResidentDisplayService::modelTransitioned(const bool changed)
{
    if (changed) {
        armDeadline();
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

} // namespace QindaQt::DisplayService
