// SPDX-License-Identifier: GPL-3.0-or-later

#include "power_protocol_test_data.h"

#include <qindaqt/services/power_protocol/power_dbus.h>
#include <qindaqt/services/power_protocol/power_validation.h>

#include <QtDBus/QDBusMetaType>
#include <QtTest>

#include <limits>

using namespace QindaQt::Power;

class PowerProtocolValuesTests final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void acceptsCanonicalSnapshot();
  void exposesFixedPrivacyPreservingStructures();
  void marshalsChargingTimeToFullThroughFixedDbusStructure();
  void rejectsEveryCollectionBeyondItsCap();
  void enforcesTextBoundsAndSanitization();
  void rejectsUnknownEnumsCapabilitiesAndNonfiniteNumbers();
  void rejectsMixedAndDuplicateLineage();
  void validatesBacklightStatusVocabulary_data();
  void validatesBacklightStatusVocabulary();
  void validatesWaylandBindingAndResultLineage();
};

void PowerProtocolValuesTests::acceptsCanonicalSnapshot() {
  const ValidationResult result = validateSnapshot(TestData::validSnapshot());
  QVERIFY2(result.accepted, qPrintable(result.reasonCode));
}

void PowerProtocolValuesTests::exposesFixedPrivacyPreservingStructures() {
  registerDBusTypes();
  QCOMPARE(QDBusMetaType::typeToSignature(QMetaType::fromType<Handle>()),
           "(ts)");
  QCOMPARE(QDBusMetaType::typeToSignature(QMetaType::fromType<PowerSupply>()),
           "((ts)ussbbduubddbdbxbxu)");
  QCOMPARE(
      QDBusMetaType::typeToSignature(QMetaType::fromType<CompositeBattery>()),
      "(bubduubdbxbxu)");
  QCOMPARE(QDBusMetaType::typeToSignature(QMetaType::fromType<Inhibitor>()),
           "(ssss)");
  const QByteArray snapshotSignature =
      QDBusMetaType::typeToSignature(QMetaType::fromType<Snapshot>());
  QVERIFY(!snapshotSignature.isEmpty());
  QVERIFY(!snapshotSignature.contains("a{sv}"));

  // AGENT-GUARD: This four-field aggregate is an intentional compile-time
  // privacy assertion. Adding UID/PID would break this initializer and the
  // fixed `(ssss)` wire signature before any adapter could leak them.
  const Inhibitor inhibitor{QStringLiteral("sleep"), QStringLiteral("who"),
                            QStringLiteral("why"), QStringLiteral("delay")};
  QCOMPARE(inhibitor.mode, QStringLiteral("delay"));
}

void PowerProtocolValuesTests::
    marshalsChargingTimeToFullThroughFixedDbusStructure() {
  registerDBusTypes();
  Snapshot snapshot = TestData::validSnapshot();
  snapshot.composite.state = ChargeState::Charging;
  snapshot.composite.netRateWatts = 12.5;
  snapshot.composite.timeToEmptyKnown = false;
  snapshot.composite.timeToEmptySeconds = 0;
  snapshot.composite.timeToFullKnown = true;
  snapshot.composite.timeToFullSeconds = 5'400;
  snapshot.supplies[0].state = ChargeState::Charging;
  snapshot.supplies[0].timeToEmptyKnown = false;
  snapshot.supplies[0].timeToEmptySeconds = 0;
  snapshot.supplies[0].timeToFullKnown = true;
  snapshot.supplies[0].timeToFullSeconds = 5'400;
  QVERIFY(validateSnapshot(snapshot).accepted);

  QDBusArgument argument;
  QVERIFY(QDBusMetaType::marshall(
      argument, QMetaType::fromType<CompositeBattery>(), &snapshot.composite));
  QCOMPARE(
      QDBusMetaType::typeToSignature(QMetaType::fromType<CompositeBattery>()),
      "(bubduubdbxbxu)");
}

void PowerProtocolValuesTests::rejectsEveryCollectionBeyondItsCap() {
  auto expectOversized = [](Snapshot snapshot) {
    QCOMPARE(validateSnapshot(snapshot).reasonCode,
             QStringLiteral("oversized-snapshot"));
  };

  Snapshot snapshot = TestData::validSnapshot();
  while (snapshot.supplies.size() <= kMaxPowerSupplies) {
    PowerSupply supply = snapshot.supplies.front();
    supply.handle.opaqueId =
        QStringLiteral("supply-%1").arg(snapshot.supplies.size());
    snapshot.supplies.push_back(supply);
  }
  expectOversized(snapshot);

  snapshot = TestData::validSnapshot();
  while (snapshot.profiles.supported.size() <= kMaxProfiles) {
    const QString id =
        QStringLiteral("profile-%1").arg(snapshot.profiles.supported.size());
    snapshot.profiles.supported.push_back({.id = id, .label = id});
  }
  expectOversized(snapshot);

  snapshot = TestData::validSnapshot();
  while (snapshot.profiles.holds.size() <= kMaxProfileHolds) {
    ProfileHold hold = snapshot.profiles.holds.front();
    hold.handle.opaqueId =
        QStringLiteral("hold-%1").arg(snapshot.profiles.holds.size());
    snapshot.profiles.holds.push_back(hold);
  }
  expectOversized(snapshot);

  snapshot = TestData::validSnapshot();
  while (snapshot.inhibitors.size() <= kMaxInhibitors) {
    snapshot.inhibitors.push_back(snapshot.inhibitors.front());
  }
  expectOversized(snapshot);

  snapshot = TestData::validSnapshot();
  while (snapshot.keyboardBacklights.size() <= kMaxKeyboardBacklights) {
    KeyboardBacklight device = snapshot.keyboardBacklights.front();
    device.handle.opaqueId =
        QStringLiteral("kbd-%1").arg(snapshot.keyboardBacklights.size());
    snapshot.keyboardBacklights.push_back(device);
  }
  expectOversized(snapshot);

  snapshot = TestData::validSnapshot();
  while (snapshot.internalBacklights.size() <= kMaxInternalBacklights) {
    InternalBacklight device = snapshot.internalBacklights.front();
    device.handle.opaqueId =
        QStringLiteral("panel-%1").arg(snapshot.internalBacklights.size());
    snapshot.internalBacklights.push_back(device);
  }
  expectOversized(snapshot);
}

void PowerProtocolValuesTests::enforcesTextBoundsAndSanitization() {
  const QString exact(kMaxReasonCodeUtf8Bytes, QLatin1Char('x'));
  QVERIFY(isBoundedText(exact, kMaxReasonCodeUtf8Bytes));
  QVERIFY(!isBoundedText(exact + QLatin1Char('x'), kMaxReasonCodeUtf8Bytes));

  QString hostile = QStringLiteral("ok");
  hostile.append(QChar::Null);
  hostile.append(QChar(0x0001));
  hostile.append(QChar(0x200E));
  hostile.append(QStringLiteral("éé"));
  const QString sanitized = sanitizeText(hostile, 8);
  QVERIFY(!sanitized.contains(QChar::Null));
  QVERIFY(isBoundedText(sanitized, 8));
  QVERIFY(sanitized.toUtf8().size() <= 8);
  QVERIFY(!sanitized.endsWith(QChar::ReplacementCharacter));

  Snapshot snapshot = TestData::validSnapshot();
  snapshot.inhibitors[0].why = QString(QChar(0x0001));
  QCOMPARE(validateSnapshot(snapshot).reasonCode,
           QStringLiteral("invalid-inhibitor"));
}

void PowerProtocolValuesTests::
    rejectsUnknownEnumsCapabilitiesAndNonfiniteNumbers() {
  Snapshot snapshot = TestData::validSnapshot();
  snapshot.availability = static_cast<Availability>(99);
  QCOMPARE(validateSnapshot(snapshot).reasonCode,
           QStringLiteral("invalid-snapshot-state"));

  snapshot = TestData::validSnapshot();
  snapshot.capabilities = Capabilities::fromInt(1U << 31U);
  QCOMPARE(validateSnapshot(snapshot).reasonCode,
           QStringLiteral("invalid-snapshot-state"));

  snapshot = TestData::validSnapshot();
  snapshot.supplies[0].percentage = std::numeric_limits<double>::quiet_NaN();
  QCOMPARE(validateSnapshot(snapshot).reasonCode,
           QStringLiteral("invalid-power-supply"));

  snapshot = TestData::validSnapshot();
  snapshot.composite.netRateWatts = std::numeric_limits<double>::infinity();
  QCOMPARE(validateSnapshot(snapshot).reasonCode,
           QStringLiteral("invalid-composite-battery"));

  snapshot = TestData::validSnapshot();
  snapshot.supplies[0].level = static_cast<BatteryLevel>(99);
  QCOMPARE(validateSnapshot(snapshot).reasonCode,
           QStringLiteral("invalid-power-supply"));

  snapshot = TestData::validSnapshot();
  snapshot.supplies[0].level = BatteryLevel::Low;
  QCOMPARE(validateSnapshot(snapshot).reasonCode,
           QStringLiteral("invalid-power-supply"));

  snapshot = TestData::validSnapshot();
  snapshot.composite.level = BatteryLevel::High;
  QCOMPARE(validateSnapshot(snapshot).reasonCode,
           QStringLiteral("invalid-composite-battery"));
}

void PowerProtocolValuesTests::rejectsMixedAndDuplicateLineage() {
  Snapshot snapshot = TestData::validSnapshot();
  snapshot.supplies[0].handle.epoch++;
  QCOMPARE(validateSnapshot(snapshot).reasonCode,
           QStringLiteral("invalid-power-supply"));

  snapshot = TestData::validSnapshot();
  snapshot.keyboardBacklights[0].handle.opaqueId =
      snapshot.internalBacklights[0].handle.opaqueId;
  QCOMPARE(validateSnapshot(snapshot).reasonCode,
           QStringLiteral("duplicate-handle"));

  snapshot = TestData::validSnapshot();
  snapshot.profiles.holds[0].profileId = QStringLiteral("unknown-profile");
  QCOMPARE(validateSnapshot(snapshot).reasonCode,
           QStringLiteral("invalid-profile-hold"));
}

void PowerProtocolValuesTests::validatesBacklightStatusVocabulary_data() {
  QTest::addColumn<BacklightReason>("reason");
  QTest::newRow("no-backlight") << BacklightReason::NoBacklight;
  QTest::newRow("ambiguous-backlight") << BacklightReason::AmbiguousBacklight;
  QTest::newRow("no-internal-connector")
      << BacklightReason::NoInternalConnector;
  QTest::newRow("ambiguous-internal-topology")
      << BacklightReason::AmbiguousInternalTopology;
}

void PowerProtocolValuesTests::validatesBacklightStatusVocabulary() {
  QFETCH(BacklightReason, reason);
  Snapshot snapshot = TestData::validSnapshot();
  snapshot.internalBacklights[0].status = BacklightStatus::Unavailable;
  snapshot.internalBacklights[0].reason = reason;
  QVERIFY2(validateSnapshot(snapshot).accepted,
           qPrintable(validateSnapshot(snapshot).reasonCode));
}

void PowerProtocolValuesTests::validatesWaylandBindingAndResultLineage() {
  Snapshot snapshot = TestData::validSnapshot();
  snapshot.waylandBinding.available = false;
  QCOMPARE(validateSnapshot(snapshot).reasonCode,
           QStringLiteral("invalid-wayland-binding"));

  OperationResult result = TestData::validOperationResult();
  QVERIFY(validateOperationResult(result).accepted);
  result.observedEpoch++;
  QCOMPARE(validateOperationResult(result).reasonCode,
           QStringLiteral("invalid-success-lineage"));
  result = TestData::validOperationResult();
  result.observedRevision = result.initiatingRevision - 1;
  QCOMPARE(validateOperationResult(result).reasonCode,
           QStringLiteral("invalid-success-lineage"));
}

QTEST_GUILESS_MAIN(PowerProtocolValuesTests)
#include "tst_power_protocol_values.moc"
