// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <qindaqt/services/power_protocol/power_types.h>
#include <qindaqt/services/settings_client/settings_client.h>
#include <qindaqt/session/idle_policy/display_off_stage.h>

#include <optional>

namespace QindaQt::Session::IdlePolicy {

enum class PowerSourceProfile {
    Ac,
    Battery,
    LowBattery,
};

// Uses only a validated Power1 snapshot. AC takes precedence whenever the
// admitted source is not on battery. On battery, LowBattery is the bounded
// default at Power1 WarningLevel::Low or worse; unknown warning stays Battery.
[[nodiscard]] std::optional<PowerSourceProfile>
selectPowerSourceProfile(const Power::Snapshot &snapshot);
[[nodiscard]] QString powerSourceProfileKey(PowerSourceProfile profile);
[[nodiscard]] QStringList perSourceDisplayOffSettingsKeys();

// Settings1 values are effective merged truth; the caller must compare the
// snapshot owner with SettingsClient::currentOwner() before relying on them.
[[nodiscard]] std::optional<DisplayOffPreferences>
displayOffPreferencesFor(const Services::SettingsClient::SettingsSnapshot &snapshot,
                         const QString &expectedOwner,
                         PowerSourceProfile profile);

} // namespace QindaQt::Session::IdlePolicy
