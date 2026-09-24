// SPDX-License-Identifier: LGPL-3.0-or-later

#include "power_settings_projection.h"

#include <qindaqt/services/brightness_model/brightness_math.h>
#include <qindaqt/services/power_protocol/power_backlight_selection.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QVariantMap>

#include <algorithm>

namespace QindaQt::Apps::SettingsPower::Projection {
namespace {

QString tr(const char *text) {
  return QCoreApplication::translate("PowerSettings", text);
}

QString stateText(const Power::ChargeState state) {
  switch (state) {
  case Power::ChargeState::Charging: return tr("Charging");
  case Power::ChargeState::Discharging: return tr("Discharging");
  case Power::ChargeState::Empty: return tr("Empty");
  case Power::ChargeState::FullyCharged: return tr("Fully charged");
  case Power::ChargeState::PendingCharge: return tr("Waiting to charge");
  case Power::ChargeState::PendingDischarge: return tr("Waiting to discharge");
  case Power::ChargeState::Unknown: return tr("State unknown");
  }
  return tr("State unknown");
}

QString warningText(const Power::WarningLevel warning) {
  switch (warning) {
  case Power::WarningLevel::None: return tr("No warning");
  case Power::WarningLevel::Discharging: return tr("Discharging");
  case Power::WarningLevel::Low: return tr("Low charge");
  case Power::WarningLevel::Critical: return tr("Critical charge");
  case Power::WarningLevel::Action: return tr("Action required");
  case Power::WarningLevel::Unknown: return tr("Warning unknown");
  }
  return tr("Warning unknown");
}

QString levelText(const Power::BatteryLevel level) {
  switch (level) {
  case Power::BatteryLevel::None: return tr("No coarse level");
  case Power::BatteryLevel::Low: return tr("Low");
  case Power::BatteryLevel::Critical: return tr("Critical");
  case Power::BatteryLevel::Normal: return tr("Normal");
  case Power::BatteryLevel::High: return tr("High");
  case Power::BatteryLevel::Full: return tr("Full");
  case Power::BatteryLevel::Unknown: return tr("Unknown");
  }
  return tr("Unknown");
}

QString durationText(const qint64 seconds) {
  if (seconds < 0) return {};
  if (seconds < 60) return tr("under a minute");
  const qint64 hours = seconds / 3600;
  const qint64 minutes = (seconds % 3600) / 60;
  if (hours == 0) return tr("%1 minutes").arg(minutes);
  if (minutes == 0) return tr("%1 hours").arg(hours);
  return tr("%1 hours %2 minutes").arg(hours).arg(minutes);
}

QString supplyName(const Power::PowerSupply &supply, const qsizetype ordinal) {
  QStringList parts;
  if (!supply.vendor.trimmed().isEmpty()) parts.append(supply.vendor.trimmed());
  if (!supply.model.trimmed().isEmpty()) parts.append(supply.model.trimmed());
  const QString combined = parts.join(QLatin1Char(' '));
  if (!combined.isEmpty()) return combined;
  return supply.kind == Power::SupplyKind::Ups
      ? tr("UPS %1").arg(ordinal) : tr("Battery %1").arg(ordinal);
}

QString internalReason(const Power::InternalBacklight &device) {
  switch (device.reason) {
  case Power::BacklightReason::None: return tr("Adjustment is unavailable for this display");
  case Power::BacklightReason::NoBacklight: return tr("No backlight device");
  case Power::BacklightReason::AmbiguousBacklight: return tr("Backlight device is ambiguous");
  case Power::BacklightReason::NoInternalConnector: return tr("No internal display connector");
  case Power::BacklightReason::AmbiguousInternalTopology: return tr("Internal display topology is ambiguous");
  case Power::BacklightReason::LogindError: return tr("Backlight access failed");
  case Power::BacklightReason::DeviceDisappeared: return tr("Backlight device disappeared");
  case Power::BacklightReason::NonConverged: return tr("Brightness has not converged");
  case Power::BacklightReason::WaylandUnavailable: return tr("Display provider is unavailable");
  }
  return tr("Brightness is unavailable");
}

// The visible explanation for a panel the shared target rule does not admit.
QString internalRefusal(const Power::Snapshot &snapshot,
                        const Power::InternalBacklight &device,
                        const Power::InternalBrightnessAdmission admission) {
  switch (admission) {
  case Power::InternalBrightnessAdmission::Admitted:
    return snapshot.capabilities.testFlag(Power::Capability::InternalBacklight)
        ? QString() : tr("Brightness adjustment is unavailable");
  case Power::InternalBrightnessAdmission::NotSelected:
    return tr("Another backlight device controls this display");
  case Power::InternalBrightnessAdmission::Ambiguous:
    return tr("Backlight device is ambiguous, so brightness is not adjusted");
  case Power::InternalBrightnessAdmission::UnknownDevice:
  case Power::InternalBrightnessAdmission::DeviceUnavailable:
    break;
  }
  if (device.diagnostic == QStringLiteral("backlight-read-only"))
    return tr("Brightness is read-only on this computer");
  // ADR-0186: the kernel attribute is root-owned on every real laptop, so a
  // read-only panel is normal and the honest reason is that the session
  // service that would write it for us is not reachable.
  if (device.diagnostic == QStringLiteral("logind-unavailable"))
    return tr("Brightness cannot be changed without a seat session");
  return internalReason(device);
}

template<typename Device>
QList<const Device *> sortedByHandle(const QList<Device> &devices) {
  QList<const Device *> sorted;
  sorted.reserve(devices.size());
  for (const Device &device : devices) sorted.append(&device);
  std::ranges::sort(sorted, [](const Device *left, const Device *right) {
    return left->handle.opaqueId < right->handle.opaqueId;
  });
  return sorted;
}

} // namespace

QString keyboardRowId(const Power::Snapshot &snapshot,
                      const Power::Handle &handle) {
  const auto sorted = sortedByHandle(snapshot.keyboardBacklights);
  for (qsizetype index = 0; index < sorted.size(); ++index) {
    if (sorted.at(index)->handle == handle)
      return QStringLiteral("keyboard-%1-%2").arg(handle.epoch).arg(index + 1);
  }
  return {};
}

QString internalRowId(const Power::Snapshot &snapshot,
                      const Power::Handle &handle) {
  const auto sorted = sortedByHandle(snapshot.internalBacklights);
  for (qsizetype index = 0; index < sorted.size(); ++index) {
    if (sorted.at(index)->handle == handle)
      return QStringLiteral("internal-%1-%2").arg(handle.epoch).arg(index + 1);
  }
  return {};
}

QVariantList supplies(const Power::Snapshot &snapshot) {
  QVariantList rows;
  rows.append(QVariantMap{
      {QStringLiteral("id"), QStringLiteral("ac-adapter")},
      {QStringLiteral("name"), tr("AC adapter")},
      {QStringLiteral("kindText"), tr("Power adapter")},
      {QStringLiteral("stateText"), snapshot.source.acPresent
           ? tr("Connected") : tr("Not connected")},
      {QStringLiteral("percentageText"), QString{}},
      {QStringLiteral("percentageKnown"), false},
      {QStringLiteral("percentage"), 0.0},
      {QStringLiteral("timeText"), QString{}},
      {QStringLiteral("warningText"), tr("No warning")},
      {QStringLiteral("warningSeverity"), 0},
      {QStringLiteral("accessibleDescription"), snapshot.source.acPresent
           ? tr("AC adapter connected") : tr("AC adapter not connected")},
  });

  const auto sorted = sortedByHandle(snapshot.supplies);
  for (qsizetype index = 0; index < sorted.size(); ++index) {
    const Power::PowerSupply &supply = *sorted.at(index);
    const QString percentage = supply.percentageKnown
        ? tr("%1 percent").arg(supply.percentage, 0, 'f', 0)
        : tr("Level %1").arg(levelText(supply.level));
    QString estimate;
    if (supply.timeToEmptyKnown)
      estimate = tr("%1 remaining").arg(durationText(supply.timeToEmptySeconds));
    else if (supply.timeToFullKnown)
      estimate = tr("%1 until full").arg(durationText(supply.timeToFullSeconds));
    const QString name = supplyName(supply, index + 1);
    const QString state = stateText(supply.state);
    const QString warning = warningText(supply.warning);
    rows.append(QVariantMap{
        {QStringLiteral("id"), QStringLiteral("supply-%1-%2")
             .arg(supply.handle.epoch).arg(index + 1)},
        {QStringLiteral("name"), name},
        {QStringLiteral("kindText"), supply.kind == Power::SupplyKind::Ups
             ? tr("Uninterruptible power supply") : tr("Battery")},
        {QStringLiteral("stateText"), state},
        {QStringLiteral("percentageText"), percentage},
        // AGENT-CONTRACT: `percentage` (0-100) means something only while
        // `percentageKnown` is true. The page then draws no charge meter at
        // all, so the 0.0 placeholder never renders as an empty bar.
        {QStringLiteral("percentageKnown"), supply.percentageKnown},
        {QStringLiteral("percentage"),
         supply.percentageKnown ? supply.percentage : 0.0},
        {QStringLiteral("timeText"), estimate},
        {QStringLiteral("warningText"), warning},
        {QStringLiteral("warningSeverity"), static_cast<quint32>(supply.warning)},
        {QStringLiteral("accessibleDescription"),
         QStringLiteral("%1, %2, %3, %4").arg(name, state, percentage, warning)},
    });
  }
  return rows;
}

QVariantList profileHolds(const Power::Snapshot &snapshot) {
  QVariantList rows;
  for (const Power::ProfileHold &hold : snapshot.profiles.holds) {
    rows.append(QVariantMap{
        {QStringLiteral("profileId"), hold.profileId},
        {QStringLiteral("applicationName"), hold.applicationName},
        {QStringLiteral("reason"), hold.reason},
        {QStringLiteral("accessibleDescription"),
         tr("%1 holds %2: %3").arg(hold.applicationName, hold.profileId,
                                      hold.reason)},
    });
  }
  return rows;
}

QVariantList internalBrightness(const Power::Snapshot &snapshot) {
  QVariantList rows;
  const auto sorted = sortedByHandle(snapshot.internalBacklights);
  for (qsizetype index = 0; index < sorted.size(); ++index) {
    const Power::InternalBacklight &device = *sorted.at(index);
    const auto normalized = device.observedKnown
        ? Brightness::normalizeRaw(0, device.maximum, device.observed)
        : Brightness::NormalizedResult{};
    const bool known = device.observedKnown && normalized.succeeded();
    const QString name = device.deviceName.trimmed().isEmpty()
        ? tr("Internal display") : device.deviceName;
    const auto admission = Power::internalBrightnessAdmission(
        snapshot.internalBacklights, device.handle.opaqueId);
    // `settable` is device truth only; the model adds operation fencing and
    // client state to produce `available`.
    const bool settable =
        snapshot.capabilities.testFlag(Power::Capability::InternalBacklight)
        && admission == Power::InternalBrightnessAdmission::Admitted;
    QString description;
    if (!known)
      description = tr("%1, brightness unavailable, read-only").arg(name);
    else if (settable)
      description = tr("%1, normalized %2 of 10000, raw %3 of %4")
                        .arg(name).arg(normalized.value).arg(device.observed)
                        .arg(device.maximum);
    else
      description = tr("%1, normalized %2 of 10000, raw %3 of %4, read-only")
                        .arg(name).arg(normalized.value).arg(device.observed)
                        .arg(device.maximum);
    rows.append(QVariantMap{
        {QStringLiteral("id"), internalRowId(snapshot, device.handle)},
        {QStringLiteral("name"), name},
        {QStringLiteral("known"), known},
        {QStringLiteral("normalized"), known ? normalized.value : 0U},
        {QStringLiteral("rawValue"), known ? device.observed : 0U},
        {QStringLiteral("rawMaximum"), device.maximum},
        {QStringLiteral("rawText"), known
             ? tr("Raw %1 of %2").arg(device.observed).arg(device.maximum)
             : tr("Raw value unavailable")},
        {QStringLiteral("settable"), settable},
        {QStringLiteral("available"), false},
        {QStringLiteral("reason"), internalRefusal(snapshot, device, admission)},
        {QStringLiteral("accessibleDescription"), description},
    });
  }
  return rows;
}

QVariantList keyboardBrightness(const Power::Snapshot &snapshot) {
  QVariantList rows;
  const auto sorted = sortedByHandle(snapshot.keyboardBacklights);
  for (qsizetype index = 0; index < sorted.size(); ++index) {
    const Power::KeyboardBacklight &device = *sorted.at(index);
    const auto normalized = device.valueKnown
        ? Brightness::normalizeRaw(0, device.maximum, device.value)
        : Brightness::NormalizedResult{};
    const bool known = device.valueKnown && normalized.succeeded();
    const QString name = device.name.trimmed().isEmpty()
        ? tr("Keyboard backlight") : device.name;
    rows.append(QVariantMap{
        {QStringLiteral("id"), keyboardRowId(snapshot, device.handle)},
        {QStringLiteral("name"), name},
        {QStringLiteral("known"), known},
        {QStringLiteral("normalized"), known ? normalized.value : 0U},
        {QStringLiteral("rawValue"), known ? device.value : 0U},
        {QStringLiteral("rawMaximum"), device.maximum},
        {QStringLiteral("rawText"), known
             ? tr("Raw %1 of %2").arg(device.value).arg(device.maximum)
             : tr("Raw value unavailable")},
        {QStringLiteral("canSet"), device.canSet},
        {QStringLiteral("accessibleDescription"), known
             ? tr("%1, normalized %2 of 10000, raw %3 of %4")
                   .arg(name).arg(normalized.value).arg(device.value)
                   .arg(device.maximum)
             : tr("%1, brightness unavailable").arg(name)},
    });
  }
  return rows;
}

} // namespace QindaQt::Apps::SettingsPower::Projection
