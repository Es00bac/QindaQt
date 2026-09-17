// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/power_service/adapters/upstream_composition.h>

#include <qindaqt/services/power_service/adapters/logind_backlight_writer.h>
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
    // ADR-0186: the internal panel's kernel attribute is root-owned on real
    // laptops, so production composes the sysfs observer with the logind
    // writer. The unavailable mode composes no writer at all, and tests inject
    // their own, which is why this is the only place the two meet.
    auto backlights = std::make_unique<SysfsBacklightSource>(
        backlightRoot, std::make_unique<LogindBacklightWriter>(upstreamBus));
    composition.battery = std::make_unique<ProductionBatteryCollaborator>(
        std::move(upower), std::move(backlights));
    composition.profiles = std::make_unique<PowerProfilesCollaborator>(upstreamBus);
    composition.session = std::make_unique<LogindSessionCollaborator>(upstreamBus);
    return composition;
}

} // namespace QindaQt::Power::Upstream
