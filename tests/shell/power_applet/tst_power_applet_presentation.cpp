// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/shell/power_applet/power_applet_presentation.h>

#include <qindaqt/services/brightness_model/brightness_types.h>
#include <qindaqt/services/power_protocol/power_limits.h>
#include <qindaqt/services/power_protocol/power_types.h>

#include <QtTest>

#include <cmath>
#include <limits>
#include <utility>

using namespace QindaQt::Shell::PowerApplet;
namespace Brightness = QindaQt::Brightness;
namespace Power = QindaQt::Power;

namespace {

constexpr quint64 kEpoch = 7;

Power::Handle handle(const QString &id) {
  return {.epoch = kEpoch, .opaqueId = id};
}

Power::PowerSupply supply(const QString &id) {
  Power::PowerSupply value;
  value.handle = handle(id);
  value.kind = Power::SupplyKind::Battery;
  value.vendor = QStringLiteral("Qinda");
  value.model = QStringLiteral("Cell");
  value.present = true;
  value.percentageKnown = true;
  value.percentage = 50.0;
  value.level = Power::BatteryLevel::None;
  value.state = Power::ChargeState::Discharging;
  value.rateKnown = true;
  value.energyRateWatts = 10.0;
  value.warning = Power::WarningLevel::Discharging;
  return value;
}

Power::Snapshot readySnapshot() {
  Power::Snapshot snapshot;
  snapshot.epoch = kEpoch;
  snapshot.revision = 42;
  snapshot.availability = Power::Availability::Ready;
  snapshot.capabilities.setFlag(Power::Capability::Supplies);
  snapshot.composite.present = true;
  snapshot.composite.sourceCount = 1;
  snapshot.composite.percentageKnown = true;
  snapshot.composite.percentage = 63.0;
  snapshot.composite.level = Power::BatteryLevel::None;
  snapshot.composite.state = Power::ChargeState::Discharging;
  snapshot.composite.netRateKnown = true;
  snapshot.composite.netRateWatts = -12.5;
  snapshot.composite.warning = Power::WarningLevel::None;
  return snapshot;
}

Brightness::DisplayControl displayControl(const QString &stableId) {
  Brightness::DisplayControl value;
  value.stableId = stableId;
  value.availability = Brightness::ControlAvailability::Available;
  value.currentKnown = true;
  value.rawCurrent = 400;
  value.rawMaximum = 1000;
  value.normalizedCurrent = 4000;
  return value;
}

Brightness::KeyboardControl keyboardControl(const QString &id) {
  Brightness::KeyboardControl value;
  value.handle = handle(id);
  value.name = QStringLiteral("Lenovo kbd");
  value.availability = Brightness::ControlAvailability::Available;
  value.currentKnown = true;
  value.rawCurrent = 2;
  value.rawMaximum = 4;
  value.normalizedCurrent = 5000;
  value.canSet = true;
  return value;
}

BrightnessView composedView() {
  BrightnessView view;
  view.ownerAvailable = true;
  view.model.displays.append(displayControl(QStringLiteral("dp-1")));
  view.model.keyboards.append(keyboardControl(QStringLiteral("kb0")));
  return view;
}

} // namespace

class PowerAppletPresentationTests final : public QObject {
    Q_OBJECT

private slots:
    void ownerLossAndHostileGenerationsFailClosed();
    void loadingPhaseHidesRowsAndAnnouncesStarting();
    void readyModelProjectsSortedRowsAndSummary();
    void chargeStatesProjectIncludingHostileRawValues();
    void timeRemainingAppearsOnlyWhenKnownAndConsistent();
    void criticalLowFullSemanticsMapSeverity();
    void hostileNumbersDegradeToUnknownTruth();
    void boundsAndCapabilityGatesDegradeNotCrash();
    void equalInputsProjectEqualModelsAndOrderIndependence();
    void timeRemainingFormattingStaysBounded();
};

void PowerAppletPresentationTests::
    ownerLossAndHostileGenerationsFailClosed()
{
    const Power::Snapshot snapshot = readySnapshot();
    const BrightnessView view = composedView();

    const PowerAppletModel ownerGone = projectPowerApplet(snapshot, false, view);
    QCOMPARE(ownerGone.phase, ServicePhase::Unavailable);
    QVERIFY(!ownerGone.diagnostic.isEmpty());
    QVERIFY(ownerGone.supplies.isEmpty());
    QVERIFY(ownerGone.displayControls.isEmpty());
    QVERIFY(ownerGone.keyboardControls.isEmpty());
    QVERIFY(!ownerGone.summary.present);

    Power::Snapshot invalidWire = snapshot;
    invalidWire.wireValid = false;
    const PowerAppletModel wireFailed =
        projectPowerApplet(invalidWire, true, view);
    QCOMPARE(wireFailed.phase, ServicePhase::Unavailable);
    QVERIFY(!wireFailed.diagnostic.isEmpty());
    QVERIFY(wireFailed.supplies.isEmpty());

    Power::Snapshot hostileAvailability = snapshot;
    hostileAvailability.availability =
        static_cast<Power::Availability>(99U);
    const PowerAppletModel hostile =
        projectPowerApplet(hostileAvailability, true, view);
    QCOMPARE(hostile.phase, ServicePhase::Unavailable);
    QVERIFY(hostile.supplies.isEmpty());
    QVERIFY(hostile.displayControls.isEmpty());

    Power::Snapshot serviceGone = snapshot;
    serviceGone.availability = Power::Availability::Unavailable;
    serviceGone.reasonCode = QStringLiteral("power-offline");
    const PowerAppletModel unavailable =
        projectPowerApplet(serviceGone, true, view);
    QCOMPARE(unavailable.phase, ServicePhase::Unavailable);
    QCOMPARE(unavailable.diagnostic, QStringLiteral("power-offline"));
    QVERIFY(unavailable.supplies.isEmpty());
}

void PowerAppletPresentationTests::loadingPhaseHidesRowsAndAnnouncesStarting()
{
    Power::Snapshot starting = readySnapshot();
    starting.availability = Power::Availability::Starting;

    const PowerAppletModel model =
        projectPowerApplet(starting, true, composedView());
    QCOMPARE(model.phase, ServicePhase::Loading);
    QVERIFY(!model.diagnostic.isEmpty());
    QVERIFY(model.supplies.isEmpty());
    QVERIFY(!model.summary.present);
    QVERIFY(model.displayControls.isEmpty());
    QVERIFY(model.keyboardControls.isEmpty());
}

void PowerAppletPresentationTests::readyModelProjectsSortedRowsAndSummary()
{
    Power::Snapshot snapshot = readySnapshot();
    snapshot.composite.timeToFullKnown = true;
    snapshot.composite.timeToFullSeconds = 7800;
    snapshot.composite.state = Power::ChargeState::Charging;
    snapshot.composite.netRateWatts = 30.0;
    snapshot.supplies.append(supply(QStringLiteral("bat-b")));
    snapshot.supplies.append(supply(QStringLiteral("bat-a")));

    const PowerAppletModel model =
        projectPowerApplet(snapshot, true, composedView());
    QCOMPARE(model.phase, ServicePhase::Ready);
    QCOMPARE(model.supplies.size(), 2);
    QCOMPARE(model.supplies.at(0).supplyId, QStringLiteral("bat-a"));
    QCOMPARE(model.supplies.at(1).supplyId, QStringLiteral("bat-b"));

    QVERIFY(model.summary.present);
    QCOMPARE(model.summary.sourceCount, 1U);
    QVERIFY(model.summary.percentageKnown);
    QCOMPARE(model.summary.percentage, 63.0);
    QCOMPARE(model.summary.state, ChargePhase::Charging);
    QCOMPARE(model.summary.severity, ChargeSeverity::Normal);
    QVERIFY(model.summary.netRateKnown);
    QCOMPARE(model.summary.netRateWatts, 30.0);
    QVERIFY(model.summary.timeRemainingKnown);
    QCOMPARE(model.summary.timeDirection, ChargePhase::Full);
    QCOMPARE(model.summary.timeRemainingSeconds, 7800);
    QVERIFY(!model.summary.accessibleName.isEmpty());
    QVERIFY(model.summary.accessibleDescription.contains(
        QStringLiteral("until full")));
    QVERIFY(model.summary.accessibleDescription.contains(
        QStringLiteral("charge rate")));

    const BatteryRow &first = model.supplies.at(0);
    QVERIFY(first.percentageKnown);
    QCOMPARE(first.percentage, 50.0);
    QCOMPARE(first.state, ChargePhase::Discharging);
    QCOMPARE(first.availability, RowAvailability::Available);
    QVERIFY(first.accessibleName.startsWith(QStringLiteral("Qinda Cell")));
    QVERIFY(!first.accessibleDescription.isEmpty());
}

void PowerAppletPresentationTests::
    chargeStatesProjectIncludingHostileRawValues()
{
    const struct {
        Power::ChargeState input;
        ChargePhase expected;
    } rows[] = {
        {Power::ChargeState::Charging, ChargePhase::Charging},
        {Power::ChargeState::PendingCharge, ChargePhase::Charging},
        {Power::ChargeState::Discharging, ChargePhase::Discharging},
        {Power::ChargeState::PendingDischarge, ChargePhase::Discharging},
        {Power::ChargeState::FullyCharged, ChargePhase::Full},
        {Power::ChargeState::Empty, ChargePhase::Empty},
        {Power::ChargeState::Unknown, ChargePhase::Unknown},
        {static_cast<Power::ChargeState>(99U), ChargePhase::Unknown},
    };
    for (const auto &row : rows) {
        Power::Snapshot snapshot = readySnapshot();
        snapshot.supplies.append(supply(QStringLiteral("bat-a")));
        snapshot.supplies.first().state = row.input;
        const PowerAppletModel model =
            projectPowerApplet(snapshot, true, {});
        QCOMPARE(model.supplies.at(0).state, row.expected);
    }
}

void PowerAppletPresentationTests::
    timeRemainingAppearsOnlyWhenKnownAndConsistent()
{
    auto supplyWithTime = [&](Power::ChargeState state, bool toEmptyKnown,
                              qint64 toEmpty, bool toFullKnown,
                              qint64 toFull) {
        Power::Snapshot snapshot = readySnapshot();
        Power::PowerSupply value = supply(QStringLiteral("bat-a"));
        value.state = state;
        value.timeToEmptyKnown = toEmptyKnown;
        value.timeToEmptySeconds = toEmpty;
        value.timeToFullKnown = toFullKnown;
        value.timeToFullSeconds = toFull;
        snapshot.supplies.append(value);
        return projectPowerApplet(snapshot, true, {}).supplies.at(0);
    };

    BatteryRow row = supplyWithTime(Power::ChargeState::Discharging, true,
                                    3600, false, 0);
    QVERIFY(row.timeRemainingKnown);
    QCOMPARE(row.timeDirection, ChargePhase::Empty);
    QCOMPARE(row.timeRemainingSeconds, 3600);

    row = supplyWithTime(Power::ChargeState::Discharging, false, 0, true,
                         3600);
    QVERIFY(!row.timeRemainingKnown);

    row = supplyWithTime(Power::ChargeState::Charging, false, 0, true, 60);
    QVERIFY(row.timeRemainingKnown);
    QCOMPARE(row.timeDirection, ChargePhase::Full);
    QCOMPARE(row.timeRemainingSeconds, 60);

    row = supplyWithTime(Power::ChargeState::Unknown, true, 3600, true, 3600);
    QVERIFY(!row.timeRemainingKnown);

    row = supplyWithTime(Power::ChargeState::Discharging, true, -5, false, 0);
    QVERIFY(!row.timeRemainingKnown);

    row = supplyWithTime(Power::ChargeState::Discharging, true,
                         Power::kMaximumEstimateSeconds + 1, false, 0);
    QVERIFY(!row.timeRemainingKnown);
}

void PowerAppletPresentationTests::criticalLowFullSemanticsMapSeverity()
{
    const struct {
        Power::WarningLevel warning;
        ChargeSeverity expected;
    } warnings[] = {
        {Power::WarningLevel::Action, ChargeSeverity::Action},
        {Power::WarningLevel::Critical, ChargeSeverity::Critical},
        {Power::WarningLevel::Low, ChargeSeverity::Low},
        {Power::WarningLevel::Discharging, ChargeSeverity::Normal},
        {Power::WarningLevel::None, ChargeSeverity::Normal},
        {Power::WarningLevel::Unknown, ChargeSeverity::Unknown},
        {static_cast<Power::WarningLevel>(42U), ChargeSeverity::Unknown},
    };
    for (const auto &row : warnings) {
        Power::Snapshot snapshot = readySnapshot();
        snapshot.supplies.append(supply(QStringLiteral("bat-a")));
        snapshot.supplies.first().warning = row.warning;
        const PowerAppletModel model =
            projectPowerApplet(snapshot, true, {});
        QCOMPARE(model.supplies.at(0).severity, row.expected);
    }

    // Without exact percentage truth, coarse-level semantics carry severity.
    const struct {
        Power::BatteryLevel level;
        ChargeSeverity expected;
        bool coarseKnown;
    } levels[] = {
        {Power::BatteryLevel::Critical, ChargeSeverity::Critical, true},
        {Power::BatteryLevel::Low, ChargeSeverity::Low, true},
        {Power::BatteryLevel::Full, ChargeSeverity::Full, true},
        {Power::BatteryLevel::Normal, ChargeSeverity::Normal, true},
        {Power::BatteryLevel::High, ChargeSeverity::Normal, true},
        {Power::BatteryLevel::Unknown, ChargeSeverity::Unknown, false},
        {static_cast<Power::BatteryLevel>(77U), ChargeSeverity::Unknown,
         false},
    };
    for (const auto &row : levels) {
        Power::Snapshot snapshot = readySnapshot();
        Power::PowerSupply value = supply(QStringLiteral("bat-a"));
        value.percentageKnown = false;
        value.percentage = 0.0;
        value.level = row.level;
        // Silence the fixture's Discharging warning so the coarse level is
        // the only severity truth under test.
        value.warning = Power::WarningLevel::Unknown;
        snapshot.supplies.append(value);
        const PowerAppletModel model =
            projectPowerApplet(snapshot, true, {});
        const BatteryRow &projected = model.supplies.at(0);
        QCOMPARE(projected.severity, row.expected);
        QCOMPARE(projected.coarseLevelKnown, row.coarseKnown);
        if (row.coarseKnown) {
            QCOMPARE(projected.coarseLevel, row.level);
        }
    }
}

void PowerAppletPresentationTests::hostileNumbersDegradeToUnknownTruth()
{
    Power::Snapshot snapshot = readySnapshot();
    Power::PowerSupply value = supply(QStringLiteral("bat-a"));
    value.percentageKnown = true;
    value.percentage = std::numeric_limits<double>::quiet_NaN();
    value.level = Power::BatteryLevel::Low;
    snapshot.supplies.append(value);
    const PowerAppletModel model =
        projectPowerApplet(snapshot, true, {});
    const BatteryRow &projected = model.supplies.at(0);
    QVERIFY(!projected.percentageKnown);
    QVERIFY(projected.coarseLevelKnown);
    QCOMPARE(projected.coarseLevel, Power::BatteryLevel::Low);

    Power::Snapshot overRange = readySnapshot();
    Power::PowerSupply hot = supply(QStringLiteral("bat-a"));
    hot.percentage = 100.5;
    overRange.supplies.append(hot);
    QVERIFY(!projectPowerApplet(overRange, true, {})
                 .supplies.at(0)
                 .percentageKnown);

    Power::Snapshot negative = readySnapshot();
    Power::PowerSupply cold = supply(QStringLiteral("bat-a"));
    cold.percentage = -0.001;
    negative.supplies.append(cold);
    QVERIFY(!projectPowerApplet(negative, true, {})
                 .supplies.at(0)
                 .percentageKnown);

    Power::Snapshot rates = readySnapshot();
    rates.composite.netRateKnown = true;
    rates.composite.netRateWatts =
        static_cast<double>(Power::kMaximumAggregateRateWatts);
    QVERIFY(projectPowerApplet(rates, true, {}).summary.netRateKnown);

    rates.composite.netRateWatts =
        static_cast<double>(Power::kMaximumAggregateRateWatts) + 1.0;
    QVERIFY(!projectPowerApplet(rates, true, {}).summary.netRateKnown);

    rates.composite.netRateWatts = std::numeric_limits<double>::infinity();
    QVERIFY(!projectPowerApplet(rates, true, {}).summary.netRateKnown);
}

void PowerAppletPresentationTests::boundsAndCapabilityGatesDegradeNotCrash()
{
    Power::Snapshot crowded = readySnapshot();
    for (int index = 0; index < 9; ++index) {
        crowded.supplies.append(
            supply(QStringLiteral("bat-%1").arg(index, 2, 10, u'0')));
    }
    const PowerAppletModel model = projectPowerApplet(crowded, true, {});
    QCOMPARE(model.phase, ServicePhase::Degraded);
    QCOMPARE(model.supplies.size(), qsizetype(Power::kMaxPowerSupplies));

    Power::Snapshot ungated = readySnapshot();
    ungated.capabilities = Power::Capabilities{};
    const PowerAppletModel ungatedModel =
        projectPowerApplet(ungated, true, {});
    QCOMPARE(ungatedModel.phase, ServicePhase::Degraded);
    QVERIFY(!ungatedModel.summary.present);
    QVERIFY(ungatedModel.supplies.isEmpty());

    Power::Snapshot anonymous = readySnapshot();
    Power::PowerSupply noHandle = supply(QStringLiteral("bat-a"));
    noHandle.handle.epoch = 0;
    anonymous.supplies.append(noHandle);
    const PowerAppletModel anonymousModel =
        projectPowerApplet(anonymous, true, {});
    QCOMPARE(anonymousModel.phase, ServicePhase::Degraded);
    QCOMPARE(anonymousModel.supplies.at(0).availability,
             RowAvailability::Degraded);
    QVERIFY(!anonymousModel.supplies.at(0).unavailableReason.isEmpty());
    QVERIFY(!anonymousModel.supplies.at(0).accessibleName.isEmpty());
}

void PowerAppletPresentationTests::
    equalInputsProjectEqualModelsAndOrderIndependence()
{
    Power::Snapshot snapshot = readySnapshot();
    snapshot.supplies.append(supply(QStringLiteral("bat-b")));
    snapshot.supplies.append(supply(QStringLiteral("bat-a")));

    const PowerAppletModel first =
        projectPowerApplet(snapshot, true, composedView());
    const PowerAppletModel again =
        projectPowerApplet(snapshot, true, composedView());
    QCOMPARE(first, again);

    Power::Snapshot permuted = snapshot;
    std::swap(permuted.supplies[0], permuted.supplies[1]);
    const PowerAppletModel reordered =
        projectPowerApplet(permuted, true, composedView());
    QCOMPARE(reordered, first);
}

void PowerAppletPresentationTests::timeRemainingFormattingStaysBounded()
{
    QCOMPARE(formatTimeRemaining(59), QStringLiteral("under a minute"));
    QCOMPARE(formatTimeRemaining(60), QStringLiteral("1 minute"));
    QCOMPARE(formatTimeRemaining(3600), QStringLiteral("1 hour"));
    QCOMPARE(formatTimeRemaining(3660), QStringLiteral("1 hour 1 minute"));
    QCOMPARE(formatTimeRemaining(7200), QStringLiteral("2 hours"));
    QCOMPARE(formatTimeRemaining(0), QStringLiteral("under a minute"));
    QVERIFY(formatTimeRemaining(Power::kMaximumEstimateSeconds).isEmpty()
                == false);
    QVERIFY(formatTimeRemaining(Power::kMaximumEstimateSeconds + 1).isEmpty());
    QVERIFY(formatTimeRemaining(-1).isEmpty());
}

QTEST_GUILESS_MAIN(PowerAppletPresentationTests)
#include "tst_power_applet_presentation.moc"
