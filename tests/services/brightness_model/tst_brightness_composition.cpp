// SPDX-License-Identifier: GPL-3.0-or-later

#include "power_protocol_test_data.h"

#include <qindaqt/services/brightness_model/brightness_composition.h>
#include <qindaqt/services/brightness_model/brightness_limits.h>
#include <qindaqt/services/brightness_model/brightness_validation.h>

#include <QtTest>

#include <algorithm>

using namespace QindaQt::Brightness;
namespace Power = QindaQt::Power;

namespace {

FixtureSnapshot validFixture() {
  return {.ownerAvailable = true,
          .serviceEpoch = QStringLiteral("fixture-epoch-1"),
          .revision = 9,
          .displays = {
              {.stableId = QStringLiteral("panel"),
               .replicationSourceStableId = {},
               .ambiguousIdentity = false,
               .powerBacklightHandle = {.epoch = 41,
                                        .opaqueId = QStringLiteral("panel-a")}},
              {.stableId = QStringLiteral("mirror"),
               .replicationSourceStableId = QStringLiteral("panel"),
               .ambiguousIdentity = true,
               .powerBacklightHandle = {}},
              {.stableId = QStringLiteral("external"),
               .replicationSourceStableId = {},
               .ambiguousIdentity = false,
               .powerBacklightHandle = {}},
          }};
}

PowerView validPower() {
  return {.ownerAvailable = true, .snapshot = Power::TestData::validSnapshot()};
}

const DisplayControl *display(const ModelSnapshot &snapshot,
                              const QString &stableId) {
  const auto found = std::ranges::find_if(
      snapshot.displays, [&stableId](const DisplayControl &entry) {
        return entry.stableId == stableId;
      });
  return found == snapshot.displays.end() ? nullptr : &*found;
}

} // namespace

class BrightnessCompositionTests final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void validatesFixtureBoundsIdentityAndReplication();
  void collapsesMirrorsAndSuppressesAmbiguousPersistence();
  void derivesDisplayAndKeyboardValuesFromRawRanges();
  void scopesCapabilityAndProviderLoss();
  void ownerLossAndHotplugDiscardOnlyStaleRows();
  void rejectsInvalidPowerWithoutPartialProjection();
  void compositionIsIndependentOfEnumerationOrder();
};

void BrightnessCompositionTests::
    validatesFixtureBoundsIdentityAndReplication() {
  QVERIFY(validateFixture(validFixture()).accepted());

  FixtureSnapshot fixture = validFixture();
  fixture.displays[1].stableId = fixture.displays[0].stableId;
  QCOMPARE(validateFixture(fixture).error, FixtureError::DuplicateStableId);
  QCOMPARE(composeBrightness(fixture, validPower()).snapshot, ModelSnapshot{});

  fixture = validFixture();
  fixture.displays[1].replicationSourceStableId = QStringLiteral("missing");
  QCOMPARE(validateFixture(fixture).error, FixtureError::InvalidReplication);

  fixture = validFixture();
  fixture.displays[0].replicationSourceStableId = QStringLiteral("mirror");
  fixture.displays[0].powerBacklightHandle = {};
  QCOMPARE(validateFixture(fixture).reasonCode,
           QStringLiteral("fixture-replication-cycle"));

  fixture = validFixture();
  fixture.displays[2].powerBacklightHandle =
      fixture.displays[0].powerBacklightHandle;
  QCOMPARE(validateFixture(fixture).error,
           FixtureError::DuplicateBacklightMapping);

  fixture = validFixture();
  fixture.displays[0].stableId.append(QChar::Null);
  QCOMPARE(validateFixture(fixture).error, FixtureError::InvalidText);

  fixture = validFixture();
  fixture.displays[0].powerBacklightHandle.epoch = 0;
  QCOMPARE(validateFixture(fixture).error, FixtureError::InvalidLineage);

  fixture = validFixture();
  fixture.displays[0].powerBacklightHandle.opaqueId.clear();
  QCOMPARE(validateFixture(fixture).error, FixtureError::InvalidLineage);

  fixture = validFixture();
  fixture.serviceEpoch = QString(kMaxFixtureEpochUtf8Bytes, QLatin1Char('e'));
  fixture.displays[0].stableId =
      QString(kMaxStableIdUtf8Bytes, QLatin1Char('s'));
  fixture.displays[1].replicationSourceStableId = fixture.displays[0].stableId;
  QVERIFY(validateFixture(fixture).accepted());
  fixture.serviceEpoch.append(QLatin1Char('e'));
  QCOMPARE(validateFixture(fixture).error, FixtureError::InvalidLineage);

  fixture = validFixture();
  fixture.displays[0].stableId =
      QString(kMaxStableIdUtf8Bytes + 1, QLatin1Char('s'));
  QCOMPARE(validateFixture(fixture).error, FixtureError::InvalidText);

  fixture = validFixture();
  while (fixture.displays.size() < kMaxFixtureDisplays) {
    fixture.displays.push_back({
        .stableId = QStringLiteral("extra-%1").arg(fixture.displays.size()),
        .replicationSourceStableId = {},
        .ambiguousIdentity = false,
        .powerBacklightHandle = {},
    });
  }
  QVERIFY(validateFixture(fixture).accepted());
  fixture.displays.push_back({
      .stableId = QStringLiteral("one-too-many"),
      .replicationSourceStableId = {},
      .ambiguousIdentity = false,
      .powerBacklightHandle = {},
  });
  QCOMPARE(validateFixture(fixture).error, FixtureError::TooManyDisplays);
}

void BrightnessCompositionTests::
    collapsesMirrorsAndSuppressesAmbiguousPersistence() {
  FixtureSnapshot fixture = validFixture();
  fixture.displays.push_back({
      .stableId = QStringLiteral("mirror-chain"),
      .replicationSourceStableId = QStringLiteral("mirror"),
      .ambiguousIdentity = false,
      .powerBacklightHandle = {},
  });
  const CompositionResult result = composeBrightness(fixture, validPower());
  QVERIFY2(result.succeeded(), qPrintable(result.reasonCode));
  QCOMPARE(result.snapshot.displays.size(), 2);
  const DisplayControl *panel =
      display(result.snapshot, QStringLiteral("panel"));
  QVERIFY(panel != nullptr);
  QCOMPARE(
      panel->memberStableIds,
      QList<QString>({QStringLiteral("mirror"), QStringLiteral("mirror-chain"),
                      QStringLiteral("panel")}));
  QCOMPARE(panel->persistenceAllowed, false);
  QVERIFY(display(result.snapshot, QStringLiteral("mirror")) == nullptr);
  QVERIFY(display(result.snapshot, QStringLiteral("mirror-chain")) == nullptr);
  QCOMPARE(panel->availability, ControlAvailability::Available);
  const DisplayControl *external =
      display(result.snapshot, QStringLiteral("external"));
  QVERIFY(external != nullptr);
  QCOMPARE(external->availability, ControlAvailability::Unavailable);
  QCOMPARE(external->reason, ControlReason::DeviceNotMapped);
}

void BrightnessCompositionTests::
    derivesDisplayAndKeyboardValuesFromRawRanges() {
  PowerView power = validPower();
  power.snapshot.keyboardBacklights[0].normalized = 0;
  const CompositionResult result = composeBrightness(validFixture(), power);
  QVERIFY(result.succeeded());

  const DisplayControl *panel =
      display(result.snapshot, QStringLiteral("panel"));
  QVERIFY(panel != nullptr);
  QCOMPARE(panel->rawCurrent, 50U);
  QCOMPARE(panel->rawMaximum, 100U);
  QCOMPARE(panel->normalizedCurrent, 5'000U);
  QCOMPARE(result.snapshot.keyboards.size(), 1);
  QCOMPARE(result.snapshot.keyboards[0].rawCurrent, 2U);
  QCOMPARE(result.snapshot.keyboards[0].rawMaximum, 3U);
  QCOMPARE(result.snapshot.keyboards[0].normalizedCurrent, 6'667U);
  QCOMPARE(result.snapshot.keyboards[0].canSet, true);
}

void BrightnessCompositionTests::scopesCapabilityAndProviderLoss() {
  PowerView power = validPower();
  power.snapshot.capabilities.setFlag(Power::Capability::InternalBacklight,
                                      false);
  CompositionResult result = composeBrightness(validFixture(), power);
  QVERIFY(result.succeeded());
  QCOMPARE(display(result.snapshot, QStringLiteral("panel"))->reason,
           ControlReason::CapabilityUnavailable);
  QCOMPARE(result.snapshot.keyboards.size(), 1);

  power = validPower();
  power.snapshot.capabilities.setFlag(Power::Capability::KeyboardBacklight,
                                      false);
  result = composeBrightness(validFixture(), power);
  QVERIFY(result.succeeded());
  QCOMPARE(result.snapshot.keyboards.size(), 0);
  QCOMPARE(display(result.snapshot, QStringLiteral("panel"))->availability,
           ControlAvailability::Available);

  power = validPower();
  power.snapshot.internalBacklights[0].status =
      Power::BacklightStatus::Degraded;
  power.snapshot.internalBacklights[0].reason =
      Power::BacklightReason::NonConverged;
  result = composeBrightness(validFixture(), power);
  QVERIFY(result.succeeded());
  const DisplayControl *panel =
      display(result.snapshot, QStringLiteral("panel"));
  QCOMPARE(panel->availability, ControlAvailability::Degraded);
  QCOMPARE(panel->reason, ControlReason::ProviderDegraded);
  QCOMPARE(panel->providerReason, Power::BacklightReason::NonConverged);
  QCOMPARE(panel->currentKnown, true);

  power = validPower();
  power.snapshot.internalBacklights.clear();
  result = composeBrightness(validFixture(), power);
  QVERIFY(result.succeeded());
  QCOMPARE(display(result.snapshot, QStringLiteral("panel"))->reason,
           ControlReason::DeviceMissing);

  power = validPower();
  power.snapshot.availability = Power::Availability::Unavailable;
  result = composeBrightness(validFixture(), power);
  QVERIFY(result.succeeded());
  QCOMPARE(display(result.snapshot, QStringLiteral("panel"))->reason,
           ControlReason::PowerServiceUnavailable);
  QCOMPARE(result.snapshot.keyboards.size(), 0);

  power = validPower();
  power.snapshot.keyboardBacklights[0].valueKnown = false;
  power.snapshot.keyboardBacklights[0].value = 0;
  power.snapshot.keyboardBacklights[0].normalized = 0;
  power.snapshot.keyboardBacklights[0].canSet = false;
  result = composeBrightness(validFixture(), power);
  QVERIFY(result.succeeded());
  QCOMPARE(result.snapshot.keyboards.size(), 1);
  QCOMPARE(result.snapshot.keyboards[0].availability,
           ControlAvailability::Unavailable);
  QCOMPARE(result.snapshot.keyboards[0].currentKnown, false);

  FixtureSnapshot staleMapping = validFixture();
  staleMapping.displays[0].powerBacklightHandle.epoch++;
  result = composeBrightness(staleMapping, validPower());
  QVERIFY(result.succeeded());
  QCOMPARE(display(result.snapshot, QStringLiteral("panel"))->reason,
           ControlReason::LineageMismatch);
  QCOMPARE(display(result.snapshot, QStringLiteral("panel"))->currentKnown,
           false);
}

void BrightnessCompositionTests::ownerLossAndHotplugDiscardOnlyStaleRows() {
  FixtureSnapshot noDisplayOwner;
  CompositionResult result = composeBrightness(noDisplayOwner, validPower());
  QVERIFY(result.succeeded());
  QCOMPARE(result.snapshot.displays.size(), 0);
  QCOMPARE(result.snapshot.keyboards.size(), 1);

  PowerView noPowerOwner = validPower();
  noPowerOwner.ownerAvailable = false;
  result = composeBrightness(validFixture(), noPowerOwner);
  QVERIFY(result.succeeded());
  QCOMPARE(result.snapshot.keyboards.size(), 0);
  QCOMPARE(result.snapshot.powerEpoch, 0U);
  const DisplayControl *panel =
      display(result.snapshot, QStringLiteral("panel"));
  QVERIFY(panel != nullptr);
  QCOMPARE(panel->reason, ControlReason::PowerOwnerUnavailable);
  QCOMPARE(panel->currentKnown, false);

  FixtureSnapshot hotplug = validFixture();
  hotplug.revision++;
  hotplug.displays.erase(hotplug.displays.begin() + 2);
  result = composeBrightness(hotplug, validPower());
  QVERIFY(result.succeeded());
  QCOMPARE(result.snapshot.displays.size(), 1);
  QCOMPARE(result.snapshot.displays[0].stableId, QStringLiteral("panel"));

  FixtureSnapshot staleLoss = validFixture();
  staleLoss.ownerAvailable = false;
  QCOMPARE(composeBrightness(staleLoss, validPower()).error,
           CompositionError::InvalidFixture);
}

void BrightnessCompositionTests::rejectsInvalidPowerWithoutPartialProjection() {
  PowerView power = validPower();
  power.snapshot.revision = 0;
  const CompositionResult result = composeBrightness(validFixture(), power);
  QCOMPARE(result.error, CompositionError::InvalidPowerSnapshot);
  QCOMPARE(result.snapshot, ModelSnapshot{});
}

void BrightnessCompositionTests::compositionIsIndependentOfEnumerationOrder() {
  FixtureSnapshot fixture = validFixture();
  PowerView power = validPower();
  Power::KeyboardBacklight second = power.snapshot.keyboardBacklights.front();
  second.handle.opaqueId = QStringLiteral("kbd-b");
  second.name = QStringLiteral("Second Keyboard");
  second.value = 1;
  second.maximum = 4;
  power.snapshot.keyboardBacklights.push_back(second);
  const ModelSnapshot expected = composeBrightness(fixture, power).snapshot;

  std::ranges::reverse(fixture.displays);
  std::ranges::reverse(power.snapshot.keyboardBacklights);
  std::ranges::reverse(power.snapshot.internalBacklights);
  const CompositionResult reordered = composeBrightness(fixture, power);
  QVERIFY(reordered.succeeded());
  QCOMPARE(reordered.snapshot, expected);
}

QTEST_GUILESS_MAIN(BrightnessCompositionTests)
#include "tst_brightness_composition.moc"
