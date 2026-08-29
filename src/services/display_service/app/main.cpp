// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/display_service/resident_display_service.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QElapsedTimer>
#include <QtCore/QUuid>
#include <QtDBus/QDBusConnection>

using namespace QindaQt;

namespace
{

class SystemMonotonicClock final : public DisplayTransaction::MonotonicClock
{
public:
    SystemMonotonicClock() { m_timer.start(); }

    quint64 nowMilliseconds() const noexcept override
    {
        return static_cast<quint64>(m_timer.elapsed());
    }

private:
    QElapsedTimer m_timer;
};

class UnavailableTransactionPort final : public DisplayService::TransactionPort
{
public:
    void setObserver(DisplayService::TransactionPortObserver *observer) override
    {
        (void)observer;
    }
    void beginMachineLineage(const quint64 machineLineage) override
    {
        (void)machineLineage;
    }
    DisplayTransaction::JournalMutationOutcome storeJournal(
        const DisplayTransaction::Journal &) override
    {
        return DisplayTransaction::JournalMutationOutcome::Unchanged;
    }
    DisplayTransaction::JournalMutationOutcome clearJournal() override
    {
        return DisplayTransaction::JournalMutationOutcome::Unchanged;
    }
    void requestApply(const DisplayTransaction::ApplyRequest &) override { }

};

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("qindaqt-display-service"));
    QCoreApplication::setApplicationVersion(QStringLiteral(QINDAQT_VERSION));
    QCoreApplication::setOrganizationDomain(QStringLiteral("qindaqt.org"));

    QDBusConnection connection = QDBusConnection::sessionBus();
    // AGENT-GUARD: Display1 lineage belongs to the constructing bus. A bus
    // replacement terminates the process instead of reusing epochs or ports.
    if (!connection.connect(QString{}, QStringLiteral("/org/freedesktop/DBus/Local"),
                            QStringLiteral("org.freedesktop.DBus.Local"),
                            QStringLiteral("Disconnected"), &application,
                            SLOT(quit()))) {
        qCritical("Display1 could not bind constructing-bus lifetime");
        return 1;
    }

    auto inventory = DisplayService::makeCompositorInventorySource(connection);
    auto transaction = std::make_unique<UnavailableTransactionPort>();
    auto clock = std::make_unique<SystemMonotonicClock>();
    DisplayService::ResidentDisplayService service(
        std::move(inventory), std::move(transaction), std::move(clock),
        [] { return QUuid::createUuid().toString(QUuid::WithoutBraces); }, connection);
    const DisplayService::ServiceStartStatus status = service.start();
    if (status != DisplayService::ServiceStartStatus::Started) {
        qCritical("Display1 startup failed with status %u",
                  static_cast<unsigned int>(status));
        return 1;
    }
    QObject::connect(&application, &QCoreApplication::aboutToQuit, &service,
                     &DisplayService::ResidentDisplayService::stop);
    return QCoreApplication::exec();
}
