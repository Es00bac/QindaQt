// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/power_service/power_service_coordinator.h>
#include <qindaqt/services/power_protocol/power_validation.h>
#include <limits>
namespace QindaQt::Power {
PeripheralSnapshot PowerServiceCoordinator::peripheralSnapshot() const
{
    return m_peripheralSnapshot;
}
void PowerServiceCoordinator::connectPeripherals()
{
    m_peripheralSnapshot.epoch = m_snapshot.epoch;
    m_peripheralSnapshot.revision = 1;
    m_peripheralSnapshot.reasonCode = QStringLiteral("starting");
    connect(m_battery, &BatteryCollaborator::peripheralFactsChanged, this,
            &PowerServiceCoordinator::acceptPeripheralFacts);
    // Connected before aggregate handlers: loss cannot leave old peripheral rows live.
    connect(m_battery, &BatteryCollaborator::statusUnavailable, this,
            [this](quint64 generation, const QString &) {
                if (generationCurrent(m_batteryDomain.state,generation))
                    clearPeripherals(Availability::Unavailable,QStringLiteral("peripherals-unavailable"));
            });
    connect(m_battery,&BatteryCollaborator::authorityReplaced,this,[this](quint64 generation) {
        if (generationCurrent(m_batteryDomain.state,generation))
            clearPeripherals(Availability::Starting,QStringLiteral("authority-replaced"));
    });
    connect(this,&PowerServiceCoordinator::invalidated,this,[this](quint64 epoch,quint64) {
        if (m_peripheralSnapshot.epoch != epoch)
            publishPeripherals(m_peripheralSnapshot);
    });
}
void PowerServiceCoordinator::clearPeripherals(Availability availability,const QString &reason)
{
    PeripheralSnapshot value;
    value.availability=availability; value.reasonCode=reason;
    publishPeripherals(std::move(value));
}
void PowerServiceCoordinator::publishPeripherals(PeripheralSnapshot value)
{
    if (m_peripheralSnapshot.revision == std::numeric_limits<quint64>::max()) return;
    value.epoch=m_snapshot.epoch;
    value.revision=m_peripheralSnapshot.revision+1;
    for (auto &row:value.devices) row.handle.epoch=value.epoch;
    if (!validatePeripheralSnapshot(value)) {
        value=PeripheralSnapshot{};
        value.epoch=m_snapshot.epoch; value.revision=m_peripheralSnapshot.revision+1;
        value.availability=Availability::Degraded;
        value.reasonCode=QStringLiteral("peripherals-malformed");
    }
    m_peripheralSnapshot=std::move(value);
    Q_EMIT peripheralsInvalidated(m_peripheralSnapshot.epoch,m_peripheralSnapshot.revision);
}
void PowerServiceCoordinator::acceptPeripheralFacts(quint64 generation,const PeripheralFacts &facts)
{
    if (!generationCurrent(m_batteryDomain.state,generation)) return;
    PeripheralSnapshot value;
    value.devices=facts.devices; value.omittedCount=facts.omittedCount;
    value.truncated=facts.truncated;
    value.availability=(facts.omittedCount || facts.truncated) ? Availability::Degraded : Availability::Ready;
    if (value.availability==Availability::Degraded) value.reasonCode=QStringLiteral("peripherals-omitted");
    for (auto &row:value.devices) {
        row.vendor=sanitizeText(row.vendor,256); row.model=sanitizeText(row.model,256);
    }
    publishPeripherals(std::move(value));
}
}
