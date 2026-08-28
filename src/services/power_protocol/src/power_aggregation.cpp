// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/power_protocol/power_aggregation.h>

#include <qindaqt/services/power_protocol/power_limits.h>
#include <qindaqt/services/power_protocol/power_validation.h>

#include <QtCore/QSet>

#include <algorithm>
#include <cmath>
#include <optional>

namespace QindaQt::Power {
namespace {

AggregationResult failed(const AggregationError error, const char *reasonCode) {
  return {.composite = {},
          .error = error,
          .reasonCode = QString::fromLatin1(reasonCode)};
}

int warningRank(const WarningLevel value) {
  switch (value) {
  case WarningLevel::Unknown:
    return 0;
  case WarningLevel::None:
    return 1;
  case WarningLevel::Discharging:
    return 2;
  case WarningLevel::Low:
    return 3;
  case WarningLevel::Critical:
    return 4;
  case WarningLevel::Action:
    return 5;
  }
  return 0;
}

long double stableSum(QList<long double> values) {
  // AGENT-GUARD: Enumeration order is upstream-controlled. Sort each numeric
  // multiset before addition so the same supplies produce bit-identical public
  // truth even when UPower returns them in another order.
  std::ranges::sort(values);
  long double result = 0.0L;
  for (const long double value : values) {
    result += value;
  }
  return result;
}

BatteryLevel aggregateCoarseLevel(const QList<const PowerSupply *> &present) {
  // Worst available energy truth wins, independent of enumeration. None
  // means the upstream explicitly has no coarse level; Unknown means no
  // upstream in the set supplied even that distinction.
  constexpr BatteryLevel order[] = {
      BatteryLevel::Critical, BatteryLevel::Low,  BatteryLevel::Normal,
      BatteryLevel::High,     BatteryLevel::Full, BatteryLevel::None,
  };
  for (const BatteryLevel candidate : order) {
    if (std::ranges::any_of(present, [candidate](const PowerSupply *supply) {
          return !supply->percentageKnown && supply->level == candidate;
        })) {
      return candidate;
    }
  }
  return BatteryLevel::Unknown;
}

ChargeState
aggregateStateWithoutRate(const QList<const PowerSupply *> &present) {
  auto has = [&present](const ChargeState state) {
    return std::ranges::any_of(present, [state](const PowerSupply *supply) {
      return supply->state == state;
    });
  };
  auto all = [&present](const ChargeState state) {
    return std::ranges::all_of(present, [state](const PowerSupply *supply) {
      return supply->state == state;
    });
  };
  if (has(ChargeState::Discharging)) {
    return ChargeState::Discharging;
  }
  if (has(ChargeState::Charging)) {
    return ChargeState::Charging;
  }
  if (has(ChargeState::PendingDischarge)) {
    return ChargeState::PendingDischarge;
  }
  if (has(ChargeState::PendingCharge)) {
    return ChargeState::PendingCharge;
  }
  if (all(ChargeState::FullyCharged)) {
    return ChargeState::FullyCharged;
  }
  if (all(ChargeState::Empty)) {
    return ChargeState::Empty;
  }
  return ChargeState::Unknown;
}

std::optional<double> signedRate(const PowerSupply &supply) {
  if (!supply.rateKnown) {
    return std::nullopt;
  }
  switch (supply.state) {
  case ChargeState::Charging:
  case ChargeState::PendingCharge:
    return supply.energyRateWatts;
  case ChargeState::Discharging:
  case ChargeState::PendingDischarge:
    return -supply.energyRateWatts;
  case ChargeState::Empty:
  case ChargeState::FullyCharged:
    return 0.0;
  case ChargeState::Unknown:
    return std::nullopt;
  }
  return std::nullopt;
}

template <typename Known, typename Value>
std::optional<qint64> commonEstimate(const QList<const PowerSupply *> &present,
                                     Known known, Value value) {
  if (present.isEmpty() || !std::ranges::all_of(present, known)) {
    return std::nullopt;
  }
  const qint64 first = value(present.front());
  if (!std::ranges::all_of(present, [first, value](const PowerSupply *supply) {
        return value(supply) == first;
      })) {
    return std::nullopt;
  }
  return first;
}

} // namespace

AggregationResult aggregatePowerSupplies(const QList<PowerSupply> &supplies) {
  if (supplies.size() > kMaxPowerSupplies) {
    return failed(AggregationError::TooManySupplies, "too-many-power-supplies");
  }

  quint64 epoch = 0;
  QSet<QString> ids;
  QList<const PowerSupply *> present;
  present.reserve(supplies.size());
  for (const PowerSupply &supply : supplies) {
    if (const ValidationResult validation =
            validateSupply(supply, supply.handle.epoch);
        !validation.accepted) {
      return failed(AggregationError::InvalidSupply, "invalid-power-supply");
    }
    if (epoch == 0) {
      epoch = supply.handle.epoch;
    } else if (supply.handle.epoch != epoch) {
      return failed(AggregationError::MixedEpoch, "mixed-supply-epoch");
    }
    // AGENT-GUARD: Opaque-ID-only deduplication is sound because epoch
    // unification happens first for every row. Do not move this check before
    // the mixed-epoch rejection or replace that rejection with pair dedup:
    // doing so would admit the same logical handle across owner generations.
    if (ids.contains(supply.handle.opaqueId)) {
      return failed(AggregationError::DuplicateHandle,
                    "duplicate-supply-handle");
    }
    ids.insert(supply.handle.opaqueId);
    if (supply.present) {
      present.push_back(&supply);
    }
  }

  if (present.isEmpty()) {
    return {.composite = {}, .error = AggregationError::None, .reasonCode = {}};
  }

  CompositeBattery composite;
  composite.present = true;
  composite.sourceCount = static_cast<quint32>(present.size());

  const bool allEnergyKnown =
      std::ranges::all_of(present, [](const PowerSupply *supply) {
        return supply->energyKnown && supply->energyFullWattHours > 0.0;
      });
  if (allEnergyKnown) {
    QList<long double> energyValues;
    QList<long double> fullValues;
    energyValues.reserve(present.size());
    fullValues.reserve(present.size());
    for (const PowerSupply *supply : present) {
      energyValues.push_back(static_cast<long double>(supply->energyWattHours));
      fullValues.push_back(
          static_cast<long double>(supply->energyFullWattHours));
    }
    const long double energy = stableSum(std::move(energyValues));
    const long double full = stableSum(std::move(fullValues));
    // AGENT-GUARD: Divide before scaling. When every source is exactly full,
    // the two stable sums are bit-identical and this produces exactly 100;
    // multiplying the large numerator first can round above 100 and falsely
    // reject a valid full aggregate.
    const long double percentage = energy / full * 100.0L;
    if (!std::isfinite(percentage) || percentage < kMinimumPercentage ||
        percentage > kMaximumPercentage) {
      return failed(AggregationError::ArithmeticOverflow,
                    "aggregate-percentage-overflow");
    }
    composite.percentageKnown = true;
    composite.percentage = static_cast<double>(percentage);
  } else if (std::ranges::all_of(present, [](const PowerSupply *supply) {
               return supply->percentageKnown;
             })) {
    QList<long double> percentages;
    percentages.reserve(present.size());
    for (const PowerSupply *supply : present) {
      percentages.push_back(static_cast<long double>(supply->percentage));
    }
    const long double sum = stableSum(std::move(percentages));
    composite.percentageKnown = true;
    composite.percentage = static_cast<double>(sum / present.size());
  }
  composite.level = composite.percentageKnown ? BatteryLevel::None
                                              : aggregateCoarseLevel(present);

  QList<long double> rateContributions;
  rateContributions.reserve(present.size());
  bool allRatesKnown = true;
  for (const PowerSupply *supply : present) {
    const std::optional<double> contribution = signedRate(*supply);
    if (!contribution.has_value()) {
      allRatesKnown = false;
      break;
    }
    rateContributions.push_back(static_cast<long double>(*contribution));
  }
  if (allRatesKnown) {
    const long double rate = stableSum(std::move(rateContributions));
    if (!std::isfinite(rate) || std::abs(rate) > kMaximumAggregateRateWatts) {
      return failed(AggregationError::ArithmeticOverflow,
                    "aggregate-rate-overflow");
    }
    composite.netRateKnown = true;
    composite.netRateWatts = static_cast<double>(rate);
  }

  if (composite.netRateKnown && composite.netRateWatts > 0.0) {
    composite.state = ChargeState::Charging;
  } else if (composite.netRateKnown && composite.netRateWatts < 0.0) {
    composite.state = ChargeState::Discharging;
  } else {
    composite.state = aggregateStateWithoutRate(present);
  }

  composite.warning = WarningLevel::Unknown;
  for (const PowerSupply *supply : present) {
    if (warningRank(supply->warning) > warningRank(composite.warning)) {
      composite.warning = supply->warning;
    }
  }

  if (composite.state == ChargeState::Discharging) {
    if (const auto estimate = commonEstimate(
            present,
            [](const PowerSupply *supply) { return supply->timeToEmptyKnown; },
            [](const PowerSupply *supply) {
              return supply->timeToEmptySeconds;
            })) {
      composite.timeToEmptyKnown = true;
      composite.timeToEmptySeconds = *estimate;
    }
  } else if (composite.state == ChargeState::Charging) {
    if (const auto estimate = commonEstimate(
            present,
            [](const PowerSupply *supply) { return supply->timeToFullKnown; },
            [](const PowerSupply *supply) {
              return supply->timeToFullSeconds;
            })) {
      composite.timeToFullKnown = true;
      composite.timeToFullSeconds = *estimate;
    }
  }

  return {.composite = composite,
          .error = AggregationError::None,
          .reasonCode = {}};
}

} // namespace QindaQt::Power
