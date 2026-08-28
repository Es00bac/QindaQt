// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/shell/power_applet/power_applet_presentation.h>

#include "power_control_rows_p.h"

#include <qindaqt/services/power_protocol/power_limits.h>

#include <QtCore/QStringList>

#include <algorithm>
#include <cmath>
#include <tuple>

namespace QindaQt::Shell::PowerApplet {
namespace {

using Power::BatteryLevel;
using Power::ChargeState;
using Power::WarningLevel;

ChargePhase chargePhaseOf(const ChargeState state) {
  if (!detail::inVocabulary(state, 6U)) {
    return ChargePhase::Unknown;
  }
  switch (state) {
  case ChargeState::Charging:
  case ChargeState::PendingCharge:
    return ChargePhase::Charging;
  case ChargeState::Discharging:
  case ChargeState::PendingDischarge:
    return ChargePhase::Discharging;
  case ChargeState::FullyCharged:
    return ChargePhase::Full;
  case ChargeState::Empty:
    return ChargePhase::Empty;
  case ChargeState::Unknown:
    break;
  }
  return ChargePhase::Unknown;
}

ChargeSeverity severityOfWarning(const WarningLevel warning) {
  if (!detail::inVocabulary(warning, 5U)) {
    return ChargeSeverity::Unknown;
  }
  switch (warning) {
  case WarningLevel::Action:
    return ChargeSeverity::Action;
  case WarningLevel::Critical:
    return ChargeSeverity::Critical;
  case WarningLevel::Low:
    return ChargeSeverity::Low;
  case WarningLevel::Discharging:
  case WarningLevel::None:
    return ChargeSeverity::Normal;
  case WarningLevel::Unknown:
    break;
  }
  return ChargeSeverity::Unknown;
}

// Severity from coarse-level truth applies only when exact percentage is
// absent. Full is its own severity per the accepted critical/low/full
// semantics; Normal, High, and the explicit no-level None stay Normal.
ChargeSeverity severityOfCoarseLevel(const BatteryLevel level) {
  if (!detail::inVocabulary(level, 6U)) {
    return ChargeSeverity::Unknown;
  }
  switch (level) {
  case BatteryLevel::Critical:
    return ChargeSeverity::Critical;
  case BatteryLevel::Low:
    return ChargeSeverity::Low;
  case BatteryLevel::Full:
    return ChargeSeverity::Full;
  case BatteryLevel::Normal:
  case BatteryLevel::High:
  case BatteryLevel::None:
    return ChargeSeverity::Normal;
  case BatteryLevel::Unknown:
    break;
  }
  return ChargeSeverity::Unknown;
}

ChargeSeverity worstSeverity(const ChargeSeverity primary,
                             const ChargeSeverity fallback) {
  return primary != ChargeSeverity::Unknown ? primary : fallback;
}

bool trustedPercentage(const bool known, const double value) {
  // AGENT-GUARD: Presentation re-checks the published scalar bound instead of
  // trusting upstream validation. A NaN or 101% must degrade to unknown
  // truth here, never render as a number.
  return known && std::isfinite(value) && value >= 0.0 &&
         value <= Power::kMaximumPercentage;
}

bool trustedEstimate(const qint64 seconds) {
  return seconds >= 0 && seconds <= Power::kMaximumEstimateSeconds;
}

struct Estimate {
  bool known = false;
  ChargePhase direction = ChargePhase::Unknown;
  qint64 seconds = 0;
};

// Time remaining is presented only when the upstream flag is known, the value
// sits inside the wire bound, and the charge state direction matches the
// estimate. PB-0 never derives estimates; this module only filters them.
Estimate timeRemainingOf(const ChargePhase state, const bool toEmptyKnown,
                         const qint64 toEmptySeconds, const bool toFullKnown,
                         const qint64 toFullSeconds) {
  if (state == ChargePhase::Discharging && toEmptyKnown &&
      trustedEstimate(toEmptySeconds)) {
    return {.known = true,
            .direction = ChargePhase::Empty,
            .seconds = toEmptySeconds};
  }
  if (state == ChargePhase::Charging && toFullKnown &&
      trustedEstimate(toFullSeconds)) {
    return {.known = true,
            .direction = ChargePhase::Full,
            .seconds = toFullSeconds};
  }
  return {};
}

QString supplyTitle(const Power::PowerSupply &supply) {
  QStringList parts;
  if (!supply.vendor.isEmpty()) {
    parts.append(supply.vendor);
  }
  if (!supply.model.isEmpty()) {
    parts.append(supply.model);
  }
  if (!parts.isEmpty()) {
    return parts.join(u' ');
  }
  const quint32 rawKind = static_cast<quint32>(supply.kind);
  if (rawKind == static_cast<quint32>(Power::SupplyKind::Ups)) {
    return QStringLiteral("UPS");
  }
  if (rawKind == static_cast<quint32>(Power::SupplyKind::Battery)) {
    return QStringLiteral("Battery");
  }
  return QStringLiteral("Power supply");
}

QString statePhrase(const ChargePhase state) {
  switch (state) {
  case ChargePhase::Charging:
    return QStringLiteral("charging");
  case ChargePhase::Discharging:
    return QStringLiteral("discharging");
  case ChargePhase::Full:
    return QStringLiteral("full");
  case ChargePhase::Empty:
    return QStringLiteral("empty");
  case ChargePhase::Unknown:
    break;
  }
  return QStringLiteral("in an unknown state");
}

QString severityPhrase(const ChargeSeverity severity) {
  switch (severity) {
  case ChargeSeverity::Normal:
    return QStringLiteral("normal");
  case ChargeSeverity::Full:
    return QStringLiteral("full");
  case ChargeSeverity::Low:
    return QStringLiteral("low");
  case ChargeSeverity::Critical:
    return QStringLiteral("critically low");
  case ChargeSeverity::Action:
    return QStringLiteral("in need of action");
  case ChargeSeverity::Unknown:
    break;
  }
  return QStringLiteral("unknown");
}

QString levelPhrase(const BatteryLevel level) {
  if (!detail::inVocabulary(level, 6U)) {
    return QStringLiteral("unknown");
  }
  switch (level) {
  case BatteryLevel::Low:
    return QStringLiteral("low");
  case BatteryLevel::Critical:
    return QStringLiteral("critically low");
  case BatteryLevel::Full:
    return QStringLiteral("full");
  case BatteryLevel::Normal:
  case BatteryLevel::High:
    return QStringLiteral("normal");
  case BatteryLevel::None:
  case BatteryLevel::Unknown:
    break;
  }
  return QStringLiteral("unknown");
}

QString percentPhrase(const double percentage) {
  return QStringLiteral("%1%").arg(percentage, 0, 'f', 0);
}

QString levelPartOf(const bool percentageKnown, const double percentage,
                    const bool coarseKnown, const BatteryLevel coarse) {
  if (percentageKnown) {
    return percentPhrase(percentage);
  }
  if (coarseKnown) {
    return QStringLiteral("level %1").arg(levelPhrase(coarse));
  }
  return QStringLiteral("level unknown");
}

bool severityAddsMeaning(const ChargeSeverity severity) {
  return severity == ChargeSeverity::Low ||
         severity == ChargeSeverity::Critical ||
         severity == ChargeSeverity::Action;
}

QString durationSentence(const Estimate &estimate) {
  if (!estimate.known) {
    return QString();
  }
  const QString duration = formatTimeRemaining(estimate.seconds);
  if (duration.isEmpty()) {
    return QString();
  }
  if (estimate.direction == ChargePhase::Full) {
    return QStringLiteral("%1 until full").arg(duration);
  }
  return QStringLiteral("%1 remaining").arg(duration);
}

QString rateSentence(const ChargePhase state, const bool netRateKnown,
                     const double netRateWatts) {
  if (!netRateKnown) {
    return QString();
  }
  if (state == ChargePhase::Charging) {
    return QStringLiteral("%1 W charge rate")
        .arg(QString::number(netRateWatts, 'f', 1));
  }
  if (state == ChargePhase::Discharging) {
    return QStringLiteral("%1 W draw")
        .arg(QString::number(-netRateWatts, 'f', 1));
  }
  return QString();
}

BatterySummaryRow projectSummary(const Power::CompositeBattery &composite,
                                 const bool suppliesCapable, bool *degraded) {
  BatterySummaryRow row;
  if (composite.present && !suppliesCapable) {
    // Contradictory generation: aggregate content without its capability
    // gate. Keep the row empty and mark the model degraded.
    *degraded = true;
    return row;
  }
  if (!composite.present) {
    return row;
  }
  row.present = true;
  row.sourceCount = composite.sourceCount;
  row.percentageKnown =
      trustedPercentage(composite.percentageKnown, composite.percentage);
  if (row.percentageKnown) {
    row.percentage = composite.percentage;
  }
  const bool coarseUsable = !row.percentageKnown &&
                            detail::inVocabulary(composite.level, 6U) &&
                            composite.level != BatteryLevel::Unknown;
  row.state = chargePhaseOf(composite.state);
  row.severity = worstSeverity(
      severityOfWarning(composite.warning),
      row.percentageKnown ? ChargeSeverity::Unknown
                          : severityOfCoarseLevel(composite.level));
  row.netRateKnown =
      composite.netRateKnown && std::isfinite(composite.netRateWatts) &&
      std::fabs(composite.netRateWatts) <= Power::kMaximumAggregateRateWatts;
  if (row.netRateKnown) {
    row.netRateWatts = composite.netRateWatts;
  }
  const Estimate estimate = timeRemainingOf(
      row.state, composite.timeToEmptyKnown, composite.timeToEmptySeconds,
      composite.timeToFullKnown, composite.timeToFullSeconds);
  row.timeRemainingKnown = estimate.known;
  row.timeDirection = estimate.direction;
  row.timeRemainingSeconds = estimate.seconds;

  const QString base =
      QStringLiteral("Battery %1, %2")
          .arg(levelPartOf(row.percentageKnown, row.percentage, coarseUsable,
                           composite.level),
               statePhrase(row.state));
  row.accessibleName = base;
  QString description =
      detail::appendPhrase(base, rateSentence(row.state, row.netRateKnown,
                                              row.netRateWatts));
  description = detail::appendPhrase(description, durationSentence(estimate));
  description = QStringLiteral("%1. Battery sources: %2.")
                    .arg(description)
                    .arg(row.sourceCount);
  row.accessibleDescription = description;
  return row;
}

BatteryRow projectSupply(const Power::PowerSupply &supply) {
  BatteryRow row;
  row.epoch = supply.handle.epoch;
  row.supplyId = supply.handle.opaqueId;
  const bool validHandle = supply.handle.isValid();
  row.availability =
      validHandle ? RowAvailability::Available : RowAvailability::Degraded;
  if (validHandle) {
    row.title = supplyTitle(supply);
  } else {
    row.title = QStringLiteral("Unknown power supply");
    row.unavailableReason = QStringLiteral("Unidentified power supply");
  }
  row.percentageKnown =
      trustedPercentage(supply.percentageKnown, supply.percentage);
  if (row.percentageKnown) {
    row.percentage = supply.percentage;
  }
  const bool coarseUsable = !row.percentageKnown &&
                            detail::inVocabulary(supply.level, 6U) &&
                            supply.level != BatteryLevel::Unknown;
  row.coarseLevelKnown = coarseUsable;
  row.coarseLevel = coarseUsable ? supply.level : BatteryLevel::Unknown;
  row.state = chargePhaseOf(supply.state);
  row.severity = worstSeverity(
      severityOfWarning(supply.warning),
      row.percentageKnown ? ChargeSeverity::Unknown
                          : severityOfCoarseLevel(supply.level));
  const Estimate estimate = timeRemainingOf(
      row.state, supply.timeToEmptyKnown, supply.timeToEmptySeconds,
      supply.timeToFullKnown, supply.timeToFullSeconds);
  row.timeRemainingKnown = estimate.known;
  row.timeDirection = estimate.direction;
  row.timeRemainingSeconds = estimate.seconds;

  const QString base =
      QStringLiteral("%1 %2, %3")
          .arg(row.title,
               levelPartOf(row.percentageKnown, row.percentage,
                           row.coarseLevelKnown, supply.level),
               statePhrase(row.state));
  row.accessibleName = detail::appendPhrase(
      base, severityAddsMeaning(row.severity) ? severityPhrase(row.severity)
                                              : QString());
  QString description = row.accessibleName;
  if (const QString duration = durationSentence(estimate);
      !duration.isEmpty()) {
    description = detail::appendPhrase(description, duration);
  }
  if (!row.unavailableReason.isEmpty()) {
    description = QStringLiteral("%1. %2.")
                      .arg(description, row.unavailableReason);
  }
  row.accessibleDescription = description;
  return row;
}

void sortSuppliesByIdentity(QList<BatteryRow> &rows) {
  // AGENT-GUARD: Sorting must be total (ID and epoch) so a hostile generation
  // carrying duplicate identities still projects deterministically.
  std::ranges::sort(rows, [](const BatteryRow &left, const BatteryRow &right) {
    return std::tie(left.supplyId, left.epoch) <
           std::tie(right.supplyId, right.epoch);
  });
}

} // namespace

QString formatTimeRemaining(const qint64 seconds) {
  if (!trustedEstimate(seconds)) {
    return QString();
  }
  if (seconds < 60) {
    return QStringLiteral("under a minute");
  }
  const qint64 hours = seconds / 3600;
  const qint64 minutes = (seconds % 3600) / 60;
  QStringList parts;
  if (hours > 0) {
    parts.append(hours == 1 ? QStringLiteral("1 hour")
                            : QStringLiteral("%1 hours").arg(hours));
  }
  if (minutes > 0 || hours == 0) {
    parts.append(minutes == 1 ? QStringLiteral("1 minute")
                              : QStringLiteral("%1 minutes").arg(minutes));
  }
  return parts.join(u' ');
}

PowerAppletModel projectPowerApplet(const Power::Snapshot &snapshot,
                                    const bool powerOwnerAvailable,
                                    const BrightnessView &brightness) {
  PowerAppletModel model;

  if (!powerOwnerAvailable) {
    // AGENT-GUARD: Owner loss fences the whole model. Even a fully populated
    // snapshot must not survive its owner, or the applet would keep showing
    // stale battery and brightness truth after service replacement.
    model.phase = ServicePhase::Unavailable;
    model.diagnostic = QStringLiteral("Power service owner is unavailable.");
    return model;
  }
  if (!snapshot.wireValid) {
    model.phase = ServicePhase::Unavailable;
    model.diagnostic = snapshot.diagnostic.isEmpty()
                           ? QStringLiteral("Power snapshot failed validation.")
                           : snapshot.diagnostic;
    return model;
  }
  if (!detail::inVocabulary(snapshot.availability, 3U)) {
    model.phase = ServicePhase::Unavailable;
    model.diagnostic = QStringLiteral("Power snapshot is not presentable.");
    return model;
  }

  if (snapshot.availability == Power::Availability::Starting) {
    model.phase = ServicePhase::Loading;
    model.diagnostic = QStringLiteral("Power service is starting.");
    return model;
  }
  if (snapshot.availability == Power::Availability::Unavailable) {
    model.phase = ServicePhase::Unavailable;
    model.diagnostic = snapshot.reasonCode.isEmpty()
                           ? QStringLiteral("Power service is unavailable.")
                           : snapshot.reasonCode;
    return model;
  }

  model.phase = snapshot.availability == Power::Availability::Degraded
                    ? ServicePhase::Degraded
                    : ServicePhase::Ready;

  bool degraded = model.phase == ServicePhase::Degraded;
  const bool suppliesCapable =
      snapshot.capabilities.testFlag(Power::Capability::Supplies);

  model.summary =
      projectSummary(snapshot.composite, suppliesCapable, &degraded);

  if (suppliesCapable) {
    // AGENT-GUARD: Power1 bounds supplies at eight. A hostile or
    // drift-corrupted generation carrying more must never render unbounded
    // rows; present the first bounded window and mark the model degraded.
    if (snapshot.supplies.size() > Power::kMaxPowerSupplies) {
      degraded = true;
    }
    const qsizetype boundedCount =
        std::min<qsizetype>(snapshot.supplies.size(),
                            Power::kMaxPowerSupplies);
    for (qsizetype index = 0; index < boundedCount; ++index) {
      model.supplies.append(projectSupply(snapshot.supplies.at(index)));
    }
    sortSuppliesByIdentity(model.supplies);
  }

  detail::projectControls(snapshot, brightness, model);

  if (degraded && model.phase == ServicePhase::Ready) {
    model.phase = ServicePhase::Degraded;
    model.diagnostic = QStringLiteral("Partial power truth is unavailable.");
  }
  return model;
}

} // namespace QindaQt::Shell::PowerApplet
