// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/power_service/adapters/production_battery_collaborator.h>

namespace QindaQt::Power::Upstream {

ProductionBatteryCollaborator::ProductionBatteryCollaborator(
    std::unique_ptr<UpowerBatteryCollaborator> upower,
    std::unique_ptr<SysfsBacklightSource> backlights, QObject *parent)
    : BatteryCollaborator(parent)
    , m_upower(std::move(upower))
    , m_backlights(std::move(backlights))
{
    Q_ASSERT(m_upower != nullptr && m_backlights != nullptr);
    connect(m_upower.get(), &UpowerBatteryCollaborator::factsChanged, this,
            [this](const quint64, const BatteryFacts &facts) {
                if (!m_running) {
                    return;
                }
                m_upowerFacts = facts;
                m_haveUpowerFacts = true;
                mergeAndEmit();
            });
    connect(m_upower.get(), &UpowerBatteryCollaborator::statusUnavailable, this,
            [this](const quint64, const QString &reasonCode) {
                if (!m_running) {
                    return;
                }
                // Fail closed for the whole domain: backlight truth is not
                // published without the AC/on-battery authority.
                m_haveUpowerFacts = false;
                m_upowerFacts = BatteryFacts{};
                Q_EMIT statusUnavailable(m_generation, reasonCode);
            });
    connect(m_upower.get(), &UpowerBatteryCollaborator::authorityReplaced, this,
            [this](const quint64) {
                if (!m_running) {
                    return;
                }
                Q_EMIT authorityReplaced(m_generation);
            });
    connect(m_upower.get(), &UpowerBatteryCollaborator::operationFinished, this,
            [this](const quint64, const quint64 operationId,
                   const CollaboratorOutcome &outcome) {
                if (!m_running) {
                    return;
                }
                Q_EMIT operationFinished(m_generation, operationId, outcome);
            });
    connect(m_backlights.get(), &SysfsBacklightSource::devicesChanged, this,
            [this](const QList<InternalBacklight> &devices) {
                if (!m_running) {
                    return;
                }
                m_backlightDevices = devices;
                mergeAndEmit();
            });
}

ProductionBatteryCollaborator::~ProductionBatteryCollaborator()
{
    stop();
}

quint64 ProductionBatteryCollaborator::start()
{
    ++m_nextGeneration;
    if (m_nextGeneration == 0) {
        ++m_nextGeneration;
    }
    m_generation = m_nextGeneration;
    m_running = true;
    m_haveUpowerFacts = false;
    m_upowerFacts = BatteryFacts{};
    m_backlightDevices.clear();
    m_upower->start();
    m_backlights->start();
    return m_generation;
}

void ProductionBatteryCollaborator::stop()
{
    m_running = false;
    m_upower->stop();
    m_backlights->stop();
}

void ProductionBatteryCollaborator::submitSetKeyboardBrightness(
    const quint64 operationId, const Handle &device, const quint32 value)
{
    m_upower->submitSetKeyboardBrightness(operationId, device, value);
}

void ProductionBatteryCollaborator::mergeAndEmit()
{
    if (!m_haveUpowerFacts) {
        return;
    }
    BatteryFacts merged = m_upowerFacts;
    merged.internalBacklights = m_backlightDevices;
    Q_EMIT factsChanged(m_generation, merged);
}

} // namespace QindaQt::Power::Upstream
