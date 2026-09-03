// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/power_service/adapters/upstream_composition.h>

#include <qindaqt/services/power_service/adapters/logind_session_collaborator.h>
#include <qindaqt/services/power_service/adapters/power_profiles_collaborator.h>
#include <qindaqt/services/power_service/adapters/production_battery_collaborator.h>
#include <qindaqt/services/power_service/unavailable_power_collaborators.h>

namespace QindaQt::Power::Upstream {

std::optional<UpstreamMode> parseUpstreamMode(const QString &value)
{
    if (value == QStringLiteral("unavailable")) {
        return UpstreamMode::Unavailable;
    }
    if (value == QStringLiteral("production")) {
        return UpstreamMode::Production;
    }
    return std::nullopt;
}

UpstreamComposition composeUpstream(const UpstreamMode mode,
                                    const QDBusConnection &upstreamBus,
                                    const QString &backlightRoot)
{
    UpstreamComposition composition;
    if (mode == UpstreamMode::Unavailable) {
        composition.battery = std::make_unique<UnavailableBatteryCollaborator>();
        composition.profiles = std::make_unique<UnavailableProfileCollaborator>();
        composition.session = std::make_unique<UnavailableSessionCollaborator>();
        return composition;
    }
    auto upower = std::make_unique<UpowerBatteryCollaborator>(upstreamBus);
    auto backlights = std::make_unique<SysfsBacklightSource>(backlightRoot);
    composition.battery = std::make_unique<ProductionBatteryCollaborator>(
        std::move(upower), std::move(backlights));
    composition.profiles = std::make_unique<PowerProfilesCollaborator>(upstreamBus);
    composition.session = std::make_unique<LogindSessionCollaborator>(upstreamBus);
    return composition;
}

} // namespace QindaQt::Power::Upstream
