// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/session/idle_policy/source_preferences.h>

#include <QMetaType>

namespace QindaQt::Session::IdlePolicy {
namespace {
QString suffix(const PowerSourceProfile profile)
{
    switch (profile) {
    case PowerSourceProfile::Ac:
        return QStringLiteral("ac");
    case PowerSourceProfile::Battery:
        return QStringLiteral("battery");
    case PowerSourceProfile::LowBattery:
        return QStringLiteral("lowBattery");
    }
    return {};
}

bool integerValue(const QVariant &value, qint64 *result)
{
    const int type = value.metaType().id();
    if (type != QMetaType::Int && type != QMetaType::UInt &&
        type != QMetaType::LongLong && type != QMetaType::ULongLong) {
        return false;
    }
    bool ok = false;
    const qint64 number = value.toLongLong(&ok);
    if (!ok) return false;
    *result = number;
    return true;
}
} // namespace

std::optional<PowerSourceProfile>
selectPowerSourceProfile(const Power::Snapshot &snapshot)
{
    if (!snapshot.wireValid || snapshot.epoch == 0 ||
        snapshot.availability == Power::Availability::Starting ||
        snapshot.availability == Power::Availability::Unavailable) {
        return std::nullopt;
    }
    if (!snapshot.source.onBattery) return PowerSourceProfile::Ac;

    switch (snapshot.composite.warning) {
    case Power::WarningLevel::Low:
    case Power::WarningLevel::Critical:
    case Power::WarningLevel::Action:
        return PowerSourceProfile::LowBattery;
    case Power::WarningLevel::Unknown:
    case Power::WarningLevel::None:
    case Power::WarningLevel::Discharging:
        return PowerSourceProfile::Battery;
    }
    return std::nullopt;
}

QString powerSourceProfileKey(const PowerSourceProfile profile)
{
    return suffix(profile);
}

QStringList perSourceDisplayOffSettingsKeys()
{
    QStringList keys;
    for (const QString &source : {QStringLiteral("ac"),
                                  QStringLiteral("battery"),
                                  QStringLiteral("lowBattery")}) {
        keys << QStringLiteral("power.idle.%1.displayOffEnabled").arg(source)
             << QStringLiteral("power.idle.%1.displayOffSeconds").arg(source);
    }
    return keys;
}

std::optional<DisplayOffPreferences>
displayOffPreferencesFor(const Services::SettingsClient::SettingsSnapshot &snapshot,
                         const QString &expectedOwner,
                         const PowerSourceProfile profile)
{
    if (expectedOwner.isEmpty() || snapshot.owner != expectedOwner ||
        snapshot.epoch.isEmpty() || snapshot.revision == 0) {
        return std::nullopt;
    }
    const QString source = suffix(profile);
    const QVariant enabledValue = snapshot.values.value(
        QStringLiteral("power.idle.%1.displayOffEnabled").arg(source));
    const QVariant timeoutValue = snapshot.values.value(
        QStringLiteral("power.idle.%1.displayOffSeconds").arg(source));
    if (enabledValue.metaType().id() != QMetaType::Bool) return std::nullopt;
    qint64 seconds = 0;
    if (!integerValue(timeoutValue, &seconds) || seconds < 0 || seconds > 14400)
        return std::nullopt;
    const bool enabled = enabledValue.toBool() && seconds > 0;
    return DisplayOffPreferences{enabled, enabled ? static_cast<int>(seconds) : 0};
}

} // namespace QindaQt::Session::IdlePolicy
