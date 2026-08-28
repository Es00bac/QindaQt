// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/power_protocol/power_validation.h>

#include <qindaqt/services/power_protocol/power_limits.h>

#include <QtCore/QSet>

#include <cmath>

namespace QindaQt::Power {
namespace {

ValidationResult accepted() { return {.accepted = true, .reasonCode = {}}; }

ValidationResult rejected(const char *reasonCode) {
  return {.accepted = false, .reasonCode = QString::fromLatin1(reasonCode)};
}

bool safeText(const QString &value) {
  for (const QChar character : value) {
    if (character.category() == QChar::Other_Control ||
        character.category() == QChar::Other_Format) {
      return false;
    }
  }
  return true;
}

bool boundedRequiredText(const QString &value, const qsizetype maximum) {
  return !value.isEmpty() && isBoundedText(value, maximum);
}

bool validAvailability(const Availability value) {
  return static_cast<quint32>(value) <=
         static_cast<quint32>(Availability::Degraded);
}

bool validSupplyKind(const SupplyKind value) {
  return static_cast<quint32>(value) <= static_cast<quint32>(SupplyKind::Ups);
}

bool validChargeState(const ChargeState value) {
  return static_cast<quint32>(value) <=
         static_cast<quint32>(ChargeState::PendingDischarge);
}

bool validWarning(const WarningLevel value) {
  return static_cast<quint32>(value) <=
         static_cast<quint32>(WarningLevel::Action);
}

bool validBacklightKind(const BacklightKind value) {
  return static_cast<quint32>(value) <=
         static_cast<quint32>(BacklightKind::Raw);
}

bool validBacklightStatus(const BacklightStatus value) {
  return static_cast<quint32>(value) <=
         static_cast<quint32>(BacklightStatus::Unavailable);
}

bool validBacklightReason(const BacklightReason value) {
  return static_cast<quint32>(value) <=
         static_cast<quint32>(BacklightReason::WaylandUnavailable);
}

bool validOperationKind(const OperationKind value) {
  return static_cast<quint32>(value) <=
         static_cast<quint32>(OperationKind::SetKeyboardBrightness);
}

bool validOperationStatus(const OperationStatus value) {
  return static_cast<quint32>(value) <=
         static_cast<quint32>(OperationStatus::Inhibited);
}

bool finiteRange(const double value, const double minimum,
                 const double maximum) {
  return std::isfinite(value) && value >= minimum && value <= maximum;
}

bool validKnownPercentage(const bool known, const double value) {
  return known ? finiteRange(value, kMinimumPercentage, kMaximumPercentage)
               : value == 0.0;
}

bool validKnownNonNegative(const bool known, const double value,
                           const double maximum) {
  return known ? finiteRange(value, 0.0, maximum) : value == 0.0;
}

bool validEstimate(const bool known, const qint64 value) {
  return known ? value >= 0 && value <= kMaximumEstimateSeconds : value == 0;
}

bool validHandle(const Handle &handle, const quint64 epoch) {
  return handle.epoch == epoch &&
         boundedRequiredText(handle.opaqueId, kMaxOpaqueIdUtf8Bytes);
}

bool insertHandle(const Handle &handle, QSet<QString> &ids) {
  if (ids.contains(handle.opaqueId)) {
    return false;
  }
  ids.insert(handle.opaqueId);
  return true;
}

bool validCapabilities(const Capabilities capabilities) {
  constexpr quint32 known =
      static_cast<quint32>(Capability::Supplies) |
      static_cast<quint32>(Capability::Profiles) |
      static_cast<quint32>(Capability::ProfileHolds) |
      static_cast<quint32>(Capability::Inhibitors) |
      static_cast<quint32>(Capability::KeyboardBacklight) |
      static_cast<quint32>(Capability::InternalBacklight) |
      static_cast<quint32>(Capability::Lid) |
      static_cast<quint32>(Capability::IdleHint);
  return (static_cast<quint32>(capabilities.toInt()) & ~known) == 0;
}

} // namespace

bool isBoundedText(const QString &value, const qsizetype maximumUtf8Bytes) {
  return maximumUtf8Bytes >= 0 && !value.contains(QChar::Null) &&
         value.toUtf8().size() <= maximumUtf8Bytes && safeText(value);
}

QString sanitizeText(QString value, const qsizetype maximumUtf8Bytes) {
  for (QChar &character : value) {
    if (character == QChar::Null ||
        character.category() == QChar::Other_Control ||
        character.category() == QChar::Other_Format) {
      character = QLatin1Char(' ');
    }
  }
  if (maximumUtf8Bytes < 0) {
    return {};
  }
  QByteArray bytes = value.toUtf8();
  if (bytes.size() <= maximumUtf8Bytes) {
    return value;
  }
  bytes.truncate(maximumUtf8Bytes);
  while (!bytes.isEmpty() && QString::fromUtf8(bytes).toUtf8() != bytes) {
    bytes.chop(1);
  }
  return QString::fromUtf8(bytes);
}

ValidationResult validateSupply(const PowerSupply &supply,
                                const quint64 epoch) {
  if (!validHandle(supply.handle, epoch) || !validSupplyKind(supply.kind) ||
      !isBoundedText(supply.vendor, kMaxNameUtf8Bytes) ||
      !isBoundedText(supply.model, kMaxNameUtf8Bytes) ||
      !validKnownPercentage(supply.percentageKnown, supply.percentage) ||
      !validChargeState(supply.state) ||
      !validKnownNonNegative(supply.energyKnown, supply.energyWattHours,
                             kMaximumEnergyWattHours) ||
      !validKnownNonNegative(supply.energyKnown, supply.energyFullWattHours,
                             kMaximumEnergyWattHours) ||
      (supply.energyKnown &&
       supply.energyWattHours > supply.energyFullWattHours) ||
      !validKnownNonNegative(supply.rateKnown, supply.energyRateWatts,
                             kMaximumRateWatts) ||
      !validEstimate(supply.timeToEmptyKnown, supply.timeToEmptySeconds) ||
      !validEstimate(supply.timeToFullKnown, supply.timeToFullSeconds) ||
      !validWarning(supply.warning)) {
    return rejected("invalid-power-supply");
  }
  if (!supply.present &&
      (supply.percentageKnown || supply.energyKnown || supply.rateKnown ||
       supply.timeToEmptyKnown || supply.timeToFullKnown ||
       supply.state != ChargeState::Unknown)) {
    return rejected("inconsistent-absent-supply");
  }
  return accepted();
}

ValidationResult validateSnapshot(const Snapshot &snapshot) {
  if (!snapshot.wireValid) {
    return rejected("malformed-payload");
  }
  if (snapshot.protocolVersion != kProtocolVersion) {
    return rejected("unsupported-version");
  }
  if (snapshot.epoch == 0 || snapshot.revision == 0) {
    return rejected("invalid-snapshot-lineage");
  }
  if (!validAvailability(snapshot.availability) ||
      !validCapabilities(snapshot.capabilities)) {
    return rejected("invalid-snapshot-state");
  }
  if (!isBoundedText(snapshot.reasonCode, kMaxReasonCodeUtf8Bytes) ||
      !isBoundedText(snapshot.diagnostic, kMaxDiagnosticUtf8Bytes)) {
    return rejected("invalid-snapshot-text");
  }
  if (snapshot.supplies.size() > kMaxPowerSupplies ||
      snapshot.profiles.supported.size() > kMaxProfiles ||
      snapshot.profiles.holds.size() > kMaxProfileHolds ||
      snapshot.inhibitors.size() > kMaxInhibitors ||
      snapshot.keyboardBacklights.size() > kMaxKeyboardBacklights ||
      snapshot.internalBacklights.size() > kMaxInternalBacklights) {
    return rejected("oversized-snapshot");
  }
  if (snapshot.source.lidClosed && !snapshot.source.lidPresent) {
    return rejected("inconsistent-source-truth");
  }

  const CompositeBattery &composite = snapshot.composite;
  if (composite.sourceCount > static_cast<quint32>(kMaxPowerSupplies) ||
      !validKnownPercentage(composite.percentageKnown, composite.percentage) ||
      !validChargeState(composite.state) ||
      !(composite.netRateKnown
            ? finiteRange(composite.netRateWatts, -kMaximumRateWatts,
                          kMaximumRateWatts)
            : composite.netRateWatts == 0.0) ||
      !validEstimate(composite.timeToEmptyKnown,
                     composite.timeToEmptySeconds) ||
      !validEstimate(composite.timeToFullKnown, composite.timeToFullSeconds) ||
      !validWarning(composite.warning)) {
    return rejected("invalid-composite-battery");
  }
  if (!composite.present &&
      (composite.sourceCount != 0 || composite.percentageKnown ||
       composite.netRateKnown || composite.timeToEmptyKnown ||
       composite.timeToFullKnown || composite.state != ChargeState::Unknown)) {
    return rejected("inconsistent-absent-composite");
  }
  if (composite.present && composite.sourceCount == 0) {
    return rejected("invalid-composite-source-count");
  }

  QSet<QString> handles;
  for (const PowerSupply &supply : snapshot.supplies) {
    if (const auto result = validateSupply(supply, snapshot.epoch);
        !result.accepted) {
      return result;
    }
    if (!insertHandle(supply.handle, handles)) {
      return rejected("duplicate-handle");
    }
  }

  QSet<QString> profileIds;
  for (const Profile &profile : snapshot.profiles.supported) {
    if (!boundedRequiredText(profile.id, kMaxProfileIdUtf8Bytes) ||
        !isBoundedText(profile.label, kMaxNameUtf8Bytes) ||
        profileIds.contains(profile.id)) {
      return rejected("invalid-profile");
    }
    profileIds.insert(profile.id);
  }
  if ((!snapshot.profiles.activeProfileId.isEmpty() &&
       !profileIds.contains(snapshot.profiles.activeProfileId)) ||
      !isBoundedText(snapshot.profiles.activeProfileId,
                     kMaxProfileIdUtf8Bytes) ||
      !isBoundedText(snapshot.profiles.degradationReason,
                     kMaxDiagnosticUtf8Bytes)) {
    return rejected("invalid-profile-state");
  }
  for (const ProfileHold &hold : snapshot.profiles.holds) {
    if (!validHandle(hold.handle, snapshot.epoch) ||
        !profileIds.contains(hold.profileId) ||
        !isBoundedText(hold.applicationName, kMaxNameUtf8Bytes) ||
        !isBoundedText(hold.reason, kMaxDiagnosticUtf8Bytes)) {
      return rejected("invalid-profile-hold");
    }
    if (!insertHandle(hold.handle, handles)) {
      return rejected("duplicate-handle");
    }
  }

  for (const Inhibitor &inhibitor : snapshot.inhibitors) {
    if (!boundedRequiredText(inhibitor.what, kMaxInhibitorWhatUtf8Bytes) ||
        !isBoundedText(inhibitor.who, kMaxInhibitorWhoUtf8Bytes) ||
        !isBoundedText(inhibitor.why, kMaxInhibitorWhyUtf8Bytes) ||
        !boundedRequiredText(inhibitor.mode, kMaxInhibitorModeUtf8Bytes)) {
      return rejected("invalid-inhibitor");
    }
  }

  for (const KeyboardBacklight &device : snapshot.keyboardBacklights) {
    if (!validHandle(device.handle, snapshot.epoch) ||
        !isBoundedText(device.name, kMaxNameUtf8Bytes) ||
        device.maximum > kMaximumRawBrightness ||
        (device.valueKnown
             ? device.maximum == 0 || device.value > device.maximum ||
                   device.normalized > kNormalizedBrightnessMaximum
             : device.value != 0 || device.normalized != 0) ||
        (device.canSet && !device.valueKnown)) {
      return rejected("invalid-keyboard-backlight");
    }
    if (!insertHandle(device.handle, handles)) {
      return rejected("duplicate-handle");
    }
  }

  for (const InternalBacklight &device : snapshot.internalBacklights) {
    if (!validHandle(device.handle, snapshot.epoch) ||
        !boundedRequiredText(device.deviceName, kMaxNameUtf8Bytes) ||
        !device.internal || !validBacklightKind(device.kind) ||
        device.maximum > kMaximumRawBrightness ||
        (device.observedKnown
             ? device.maximum == 0 || device.observed > device.maximum
             : device.observed != 0) ||
        !validBacklightStatus(device.status) ||
        !validBacklightReason(device.reason) ||
        !isBoundedText(device.diagnostic, kMaxDiagnosticUtf8Bytes) ||
        (device.status == BacklightStatus::Ok &&
         device.reason != BacklightReason::None) ||
        (device.status != BacklightStatus::Ok &&
         device.reason == BacklightReason::None)) {
      return rejected("invalid-internal-backlight");
    }
    if (!insertHandle(device.handle, handles)) {
      return rejected("duplicate-handle");
    }
  }

  const WaylandBinding &binding = snapshot.waylandBinding;
  if (binding.available
          ? !boundedRequiredText(binding.socketName,
                                 kMaxWaylandSocketUtf8Bytes) ||
                binding.protocolVersion == 0 || binding.bindingEpoch == 0
          : !binding.socketName.isEmpty() || binding.protocolVersion != 0 ||
                binding.bindingEpoch != 0) {
    return rejected("invalid-wayland-binding");
  }

  return accepted();
}

ValidationResult validateOperationResult(const OperationResult &result) {
  if (!result.wireValid || !validOperationKind(result.kind) ||
      !validOperationStatus(result.status) || result.initiatingEpoch == 0 ||
      result.initiatingRevision == 0 || result.observedEpoch == 0 ||
      result.observedRevision == 0 ||
      !isBoundedText(result.reasonCode, kMaxReasonCodeUtf8Bytes) ||
      !isBoundedText(result.diagnostic, kMaxDiagnosticUtf8Bytes)) {
    return rejected("invalid-operation-result");
  }
  if (result.status == OperationStatus::Succeeded &&
      (result.observedEpoch != result.initiatingEpoch ||
       result.observedRevision < result.initiatingRevision)) {
    return rejected("invalid-success-lineage");
  }
  return accepted();
}

} // namespace QindaQt::Power
