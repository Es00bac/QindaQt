// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/display_service/display_service_model.h>
#include <qindaqt/services/display_service/display_service_ports.h>

#include <QtCore/QObject>
#include <QtCore/QTimer>
#include <QtDBus/QDBusConnection>

#include <memory>

namespace QindaQt::DisplayService
{

class DisplayServiceObject;

enum class ServiceStartStatus {
    Started,
    InvalidConnection,
    ObjectRegistrationFailed,
    NameAlreadyOwned,
    NameRegistrationFailed,
    InventorySourceFailed,
};

class ResidentDisplayService final : public QObject,
                                     private InventoryObserver,
                                     private TransactionPortObserver
{
    Q_OBJECT

public:
    // The resident takes exclusive ownership of all ports and the clock. It
    // destroys the D1 model before those dependencies, unregisters its bus
    // name/object before destruction, and confines every callback to the
    // constructing Qt thread. Timing is copied into the model; the named
    // connection must remain registered.
    ResidentDisplayService(std::unique_ptr<InventorySource> inventorySource,
                           std::unique_ptr<TransactionPort> transactionPort,
                           std::unique_ptr<DisplayTransaction::MonotonicClock> clock,
                           EpochFactory epochFactory, const QDBusConnection &connection,
                           QString serviceName = {},
                           DisplayTransaction::Timing timing = {},
                           QObject *parent = nullptr);
    ~ResidentDisplayService() override;

    [[nodiscard]] ServiceStartStatus start();
    void stop();
    [[nodiscard]] bool isRunning() const noexcept;
    [[nodiscard]] DisplayServiceModel *model() noexcept;

private:
    void inventoryObserved(const InventoryFrame &frame) override;
    void inventoryUnavailable() override;
    void applyCompleted(quint64 machineLineage, quint64 token,
                        DisplayTransaction::ApplyOutcome outcome) override;
    void modelTransitioned(bool changed);
    void armDeadline();

    std::unique_ptr<InventorySource> m_inventorySource;
    std::unique_ptr<DisplayTransaction::MonotonicClock> m_clock;
    std::unique_ptr<TransactionPort> m_transactionPort;
    std::unique_ptr<DisplayServiceModel> m_model;
    std::unique_ptr<DisplayServiceObject> m_serviceObject;
    QDBusConnection m_connection;
    QString m_serviceName;
    QTimer *m_deadlineTimer = nullptr;
    bool m_objectRegistered = false;
    bool m_nameRegistered = false;
};

} // namespace QindaQt::DisplayService
