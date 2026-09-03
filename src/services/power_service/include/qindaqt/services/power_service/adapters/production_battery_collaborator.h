// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/power_service/power_collaborators.h>
#include <qindaqt/services/power_service/adapters/sysfs_backlight_source.h>
#include <qindaqt/services/power_service/adapters/upower_battery_collaborator.h>

#include <QtCore/QObject>

#include <memory>

namespace QindaQt::Power::Upstream {

// AGENT-CONTRACT: Production composition behind the battery seam. It owns the
// UPower adapter and the sysfs backlight source and merges them into one
// generation-fenced BatteryFacts stream. The bus authority gates the domain:
// no facts are published until UPower truth exists, and UPower unavailability
// drops the backlight rows with the rest rather than publishing device truth
// without AC/on-battery authority. The subprocess generations of the owned
// adapters are internal; only this object's own generation stamps emissions.
class ProductionBatteryCollaborator final : public BatteryCollaborator
{
    Q_OBJECT

public:
    ProductionBatteryCollaborator(
        std::unique_ptr<UpowerBatteryCollaborator> upower,
        std::unique_ptr<SysfsBacklightSource> backlights, QObject *parent = nullptr);
    ~ProductionBatteryCollaborator() override;

    quint64 start() override;
    void stop() override;
    void submitSetKeyboardBrightness(quint64 operationId, const Handle &device,
                                     quint32 value) override;

private:
    void mergeAndEmit();

    std::unique_ptr<UpowerBatteryCollaborator> m_upower;
    std::unique_ptr<SysfsBacklightSource> m_backlights;
    BatteryFacts m_upowerFacts;
    QList<InternalBacklight> m_backlightDevices;
    quint64 m_generation = 0;
    quint64 m_nextGeneration = 0;
    bool m_haveUpowerFacts = false;
    bool m_running = false;
};

} // namespace QindaQt::Power::Upstream
