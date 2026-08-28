// SPDX-License-Identifier: GPL-3.0-or-later

#include "power_protocol_test_data.h"

#include <qindaqt/services/power_protocol/power_aggregation.h>

#include <QtTest>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

using namespace QindaQt::Power;

namespace {

PowerSupply supply(const QString &id, const double percentage,
                   const ChargeState state, const double rate) {
  PowerSupply value = TestData::validSnapshot().supplies.front();
  value.handle.opaqueId = id;
  value.percentageKnown = true;
  value.percentage = percentage;
  value.level = BatteryLevel::None;
  value.state = state;
  value.energyKnown = false;
  value.energyWattHours = 0.0;
  value.energyFullWattHours = 0.0;
  value.rateKnown = true;
  value.energyRateWatts = rate;
  value.timeToEmptyKnown = false;
  value.timeToEmptySeconds = 0;
  value.timeToFullKnown = false;
  value.timeToFullSeconds = 0;
  value.warning = WarningLevel::None;
  return value;
}

PowerSupply absentSupply(const QString &id) {
  PowerSupply value = supply(id, 0.0, ChargeState::Unknown, 0.0);
  value.present = false;
  value.percentageKnown = false;
  value.level = BatteryLevel::Unknown;
  value.rateKnown = false;
  value.warning = WarningLevel::Unknown;
  return value;
}

} // namespace

class PowerAggregationTests final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void emptyAndAbsentInputsProduceNoComposite();
  void singleBatteryPassesAuthoritativeTruth();
  void dualBatteryUsesEnergyWeighting();
  void upsAndBatteryPreserveRateSign();
  void coarseUnknownLevelAndWarningFailClosed();
  void coarseLevelAndWarningPrecedenceIsClosed();
  void unknownRateRetainsConservativeState();
  void estimatesArePassedThroughNeverRecomputed();
  void resultIsOrderIndependent();
  void fullEnergyAcrossExponentSpreadStaysAtHundredPercent();
  void aggregateRateBoundariesDoNotOverflow();
  void rejectsBoundsLineageDuplicatesAndHostileNumbers();
};

void PowerAggregationTests::emptyAndAbsentInputsProduceNoComposite() {
  const AggregationResult empty = aggregatePowerSupplies({});
  QVERIFY(empty.succeeded());
  QCOMPARE(empty.composite, CompositeBattery{});

  const AggregationResult absent =
      aggregatePowerSupplies({absentSupply("absent")});
  QVERIFY(absent.succeeded());
  QCOMPARE(absent.composite, CompositeBattery{});

  PowerSupply contradictory = absentSupply(QStringLiteral("contradictory"));
  contradictory.warning = WarningLevel::Action;
  QCOMPARE(aggregatePowerSupplies({contradictory}).error,
           AggregationError::InvalidSupply);
}

void PowerAggregationTests::singleBatteryPassesAuthoritativeTruth() {
  PowerSupply battery =
      supply(QStringLiteral("battery"), 42.5, ChargeState::Discharging, 8.0);
  battery.warning = WarningLevel::Low;
  battery.timeToEmptyKnown = true;
  battery.timeToEmptySeconds = 4'321;

  const AggregationResult result = aggregatePowerSupplies({battery});
  QVERIFY2(result.succeeded(), qPrintable(result.reasonCode));
  QCOMPARE(result.composite.present, true);
  QCOMPARE(result.composite.sourceCount, 1U);
  QCOMPARE(result.composite.percentage, 42.5);
  QCOMPARE(result.composite.level, BatteryLevel::None);
  QCOMPARE(result.composite.state, ChargeState::Discharging);
  QCOMPARE(result.composite.netRateWatts, -8.0);
  QCOMPARE(result.composite.timeToEmptyKnown, true);
  QCOMPARE(result.composite.timeToEmptySeconds, 4'321);
  QCOMPARE(result.composite.warning, WarningLevel::Low);
}

void PowerAggregationTests::dualBatteryUsesEnergyWeighting() {
  PowerSupply large =
      supply(QStringLiteral("large"), 25.0, ChargeState::Discharging, 10.0);
  large.energyKnown = true;
  large.energyWattHours = 20.0;
  large.energyFullWattHours = 80.0;
  PowerSupply small =
      supply(QStringLiteral("small"), 50.0, ChargeState::Discharging, 5.0);
  small.energyKnown = true;
  small.energyWattHours = 10.0;
  small.energyFullWattHours = 20.0;

  const AggregationResult result = aggregatePowerSupplies({large, small});
  QVERIFY(result.succeeded());
  QCOMPARE(result.composite.sourceCount, 2U);
  QCOMPARE(result.composite.percentage, 30.0);
  QCOMPARE(result.composite.netRateWatts, -15.0);
}

void PowerAggregationTests::upsAndBatteryPreserveRateSign() {
  PowerSupply battery =
      supply(QStringLiteral("battery"), 55.0, ChargeState::Discharging, 20.0);
  PowerSupply ups =
      supply(QStringLiteral("ups"), 80.0, ChargeState::Charging, 5.0);
  ups.kind = SupplyKind::Ups;

  const AggregationResult result = aggregatePowerSupplies({battery, ups});
  QVERIFY(result.succeeded());
  QCOMPARE(result.composite.percentage, 67.5);
  QCOMPARE(result.composite.netRateKnown, true);
  QCOMPARE(result.composite.netRateWatts, -15.0);
  QCOMPARE(result.composite.state, ChargeState::Discharging);
}

void PowerAggregationTests::coarseUnknownLevelAndWarningFailClosed() {
  PowerSupply low =
      supply(QStringLiteral("low"), 0.0, ChargeState::Discharging, 3.0);
  low.percentageKnown = false;
  low.percentage = 0.0;
  low.level = BatteryLevel::Low;
  low.warning = WarningLevel::Low;
  PowerSupply critical =
      supply(QStringLiteral("critical"), 0.0, ChargeState::Discharging, 2.0);
  critical.percentageKnown = false;
  critical.percentage = 0.0;
  critical.level = BatteryLevel::Critical;
  critical.warning = WarningLevel::Action;

  const AggregationResult result = aggregatePowerSupplies({low, critical});
  QVERIFY(result.succeeded());
  QCOMPARE(result.composite.percentageKnown, false);
  QCOMPARE(result.composite.percentage, 0.0);
  QCOMPARE(result.composite.level, BatteryLevel::Critical);
  QCOMPARE(result.composite.warning, WarningLevel::Action);
}

void PowerAggregationTests::coarseLevelAndWarningPrecedenceIsClosed() {
  constexpr std::array levelOrder = {
      BatteryLevel::Critical, BatteryLevel::Low,  BatteryLevel::Normal,
      BatteryLevel::High,     BatteryLevel::Full, BatteryLevel::None,
      BatteryLevel::Unknown,
  };
  for (std::size_t index = 0; index + 1 < levelOrder.size(); ++index) {
    PowerSupply stronger = supply(QStringLiteral("level-stronger"), 0.0,
                                  ChargeState::Unknown, 0.0);
    stronger.percentageKnown = false;
    stronger.level = levelOrder[index];
    stronger.rateKnown = false;
    stronger.warning = WarningLevel::Unknown;
    PowerSupply weaker = stronger;
    weaker.handle.opaqueId = QStringLiteral("level-weaker");
    weaker.level = levelOrder[index + 1];

    AggregationResult result = aggregatePowerSupplies({stronger, weaker});
    QVERIFY(result.succeeded());
    QCOMPARE(result.composite.level, levelOrder[index]);
    result = aggregatePowerSupplies({weaker, stronger});
    QVERIFY(result.succeeded());
    QCOMPARE(result.composite.level, levelOrder[index]);
  }

  constexpr std::array warningOrder = {
      WarningLevel::Action,      WarningLevel::Critical, WarningLevel::Low,
      WarningLevel::Discharging, WarningLevel::None,     WarningLevel::Unknown,
  };
  for (std::size_t index = 0; index + 1 < warningOrder.size(); ++index) {
    PowerSupply stronger = supply(QStringLiteral("warning-stronger"), 50.0,
                                  ChargeState::Unknown, 0.0);
    stronger.rateKnown = false;
    stronger.warning = warningOrder[index];
    PowerSupply weaker = stronger;
    weaker.handle.opaqueId = QStringLiteral("warning-weaker");
    weaker.warning = warningOrder[index + 1];

    AggregationResult result = aggregatePowerSupplies({stronger, weaker});
    QVERIFY(result.succeeded());
    QCOMPARE(result.composite.warning, warningOrder[index]);
    result = aggregatePowerSupplies({weaker, stronger});
    QVERIFY(result.succeeded());
    QCOMPARE(result.composite.warning, warningOrder[index]);
  }
}

void PowerAggregationTests::unknownRateRetainsConservativeState() {
  PowerSupply discharging = supply(QStringLiteral("discharging"), 70.0,
                                   ChargeState::Discharging, 2.0);
  PowerSupply unknown =
      supply(QStringLiteral("unknown"), 30.0, ChargeState::Unknown, 0.0);
  unknown.rateKnown = false;

  const AggregationResult result =
      aggregatePowerSupplies({unknown, discharging});
  QVERIFY(result.succeeded());
  QCOMPARE(result.composite.netRateKnown, false);
  QCOMPARE(result.composite.netRateWatts, 0.0);
  QCOMPARE(result.composite.state, ChargeState::Discharging);
}

void PowerAggregationTests::estimatesArePassedThroughNeverRecomputed() {
  PowerSupply first =
      supply(QStringLiteral("first"), 60.0, ChargeState::Discharging, 4.0);
  first.timeToEmptyKnown = true;
  first.timeToEmptySeconds = 3'600;
  PowerSupply second =
      supply(QStringLiteral("second"), 40.0, ChargeState::Discharging, 6.0);
  second.timeToEmptyKnown = true;
  second.timeToEmptySeconds = 7'200;
  AggregationResult result = aggregatePowerSupplies({first, second});
  QVERIFY(result.succeeded());
  QCOMPARE(result.composite.timeToEmptyKnown, false);
  QCOMPARE(result.composite.timeToEmptySeconds, 0);

  second.timeToEmptySeconds = 3'600;
  result = aggregatePowerSupplies({first, second});
  QVERIFY(result.succeeded());
  QCOMPARE(result.composite.timeToEmptyKnown, true);
  QCOMPARE(result.composite.timeToEmptySeconds, 3'600);

  first = supply(QStringLiteral("first"), 60.0, ChargeState::Charging, 4.0);
  first.timeToFullKnown = true;
  first.timeToFullSeconds = 1'800;
  second = supply(QStringLiteral("second"), 40.0, ChargeState::Charging, 6.0);
  second.timeToFullKnown = true;
  second.timeToFullSeconds = 2'400;
  result = aggregatePowerSupplies({first, second});
  QVERIFY(result.succeeded());
  QCOMPARE(result.composite.state, ChargeState::Charging);
  QCOMPARE(result.composite.timeToFullKnown, false);
  QCOMPARE(result.composite.timeToFullSeconds, 0);

  second.timeToFullSeconds = 1'800;
  result = aggregatePowerSupplies({first, second});
  QVERIFY(result.succeeded());
  QCOMPARE(result.composite.timeToFullKnown, true);
  QCOMPARE(result.composite.timeToFullSeconds, 1'800);
  QCOMPARE(result.composite.timeToEmptyKnown, false);
}

void PowerAggregationTests::resultIsOrderIndependent() {
  QList<PowerSupply> inputs = {
      supply(QStringLiteral("a"), 1.0e-12, ChargeState::Charging, 1.0e-10),
      supply(QStringLiteral("b"), 100.0, ChargeState::Charging,
             kMaximumRateWatts),
      supply(QStringLiteral("c"), 50.0, ChargeState::Discharging,
             kMaximumRateWatts),
  };
  const CompositeBattery expected = aggregatePowerSupplies(inputs).composite;
  std::ranges::sort(inputs,
                    [](const PowerSupply &left, const PowerSupply &right) {
                      return left.handle.opaqueId < right.handle.opaqueId;
                    });
  do {
    const AggregationResult result = aggregatePowerSupplies(inputs);
    QVERIFY(result.succeeded());
    QCOMPARE(result.composite, expected);
  } while (std::next_permutation(
      inputs.begin(), inputs.end(),
      [](const PowerSupply &left, const PowerSupply &right) {
        return left.handle.opaqueId < right.handle.opaqueId;
      }));
}

void PowerAggregationTests::
    fullEnergyAcrossExponentSpreadStaysAtHundredPercent() {
  constexpr std::array fullValues = {
      1.0,
      std::ldexp(1.0, -6),
      std::ldexp(1.0, -12),
      std::ldexp(1.0, -18),
      std::ldexp(1.0, -30),
      std::ldexp(1.0, -42),
      std::ldexp(1.0, -54),
      std::ldexp(1.0, -63),
  };
  QList<PowerSupply> inputs;
  for (std::size_t index = 0; index < fullValues.size(); ++index) {
    PowerSupply value = supply(QStringLiteral("full-%1").arg(index), 100.0,
                               ChargeState::FullyCharged, 0.0);
    value.energyKnown = true;
    value.energyWattHours = fullValues[index];
    value.energyFullWattHours = fullValues[index];
    inputs.push_back(std::move(value));
  }
  const AggregationResult result = aggregatePowerSupplies(inputs);
  QVERIFY2(result.succeeded(), qPrintable(result.reasonCode));
  QCOMPARE(result.composite.percentageKnown, true);
  QCOMPARE(result.composite.percentage, 100.0);
}

void PowerAggregationTests::aggregateRateBoundariesDoNotOverflow() {
  QList<PowerSupply> inputs;
  for (qsizetype index = 0; index < kMaxPowerSupplies; ++index) {
    inputs.push_back(supply(QStringLiteral("s-%1").arg(index), 50.0,
                            ChargeState::Charging, kMaximumRateWatts));
  }
  const AggregationResult result = aggregatePowerSupplies(inputs);
  QVERIFY(result.succeeded());
  QCOMPARE(result.composite.netRateWatts, kMaximumAggregateRateWatts);

  for (PowerSupply &input : inputs) {
    input.state = ChargeState::Discharging;
  }
  const AggregationResult negative = aggregatePowerSupplies(inputs);
  QVERIFY(negative.succeeded());
  QCOMPARE(negative.composite.netRateWatts, -kMaximumAggregateRateWatts);
}

void PowerAggregationTests::rejectsBoundsLineageDuplicatesAndHostileNumbers() {
  QList<PowerSupply> tooMany;
  for (qsizetype index = 0; index <= kMaxPowerSupplies; ++index) {
    tooMany.push_back(supply(QStringLiteral("s-%1").arg(index), 50.0,
                             ChargeState::Charging, 1.0));
  }
  QCOMPARE(aggregatePowerSupplies(tooMany).error,
           AggregationError::TooManySupplies);

  PowerSupply first =
      supply(QStringLiteral("a"), 50.0, ChargeState::Charging, 1.0);
  PowerSupply second =
      supply(QStringLiteral("b"), 50.0, ChargeState::Charging, 1.0);
  second.handle.epoch++;
  QCOMPARE(aggregatePowerSupplies({first, second}).error,
           AggregationError::MixedEpoch);

  second = first;
  QCOMPARE(aggregatePowerSupplies({first, second}).error,
           AggregationError::DuplicateHandle);

  first.energyRateWatts = std::numeric_limits<double>::infinity();
  QCOMPARE(aggregatePowerSupplies({first}).error,
           AggregationError::InvalidSupply);

  first =
      supply(QStringLiteral("zero-epoch"), 50.0, ChargeState::Charging, 1.0);
  first.handle.epoch = 0;
  QCOMPARE(aggregatePowerSupplies({first}).error,
           AggregationError::InvalidSupply);
}

QTEST_GUILESS_MAIN(PowerAggregationTests)
#include "tst_power_aggregation.moc"
