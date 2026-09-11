// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/night_light/night_light_values.h>

#include <cmath>
#include <limits>
#include <QtTest>

using namespace QindaQt::Services::NightLight;

namespace {

constexpr int kFloor = kTemperatureFloorKelvin;
constexpr int kCeiling = kTemperatureCeilingKelvin;
constexpr int kStep = kTemperatureStepKelvin;

} // namespace

class NightLightValuesTests final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void temperatureBoundsAndStep();
    void temperatureSnapping();
    void coordinateValidation();
    void transitionValidation();
    void scheduleTimeValidation();
    void configTokenMappingRejectsForeignTokens();
    void busModeMappingRejectsOutdatedDocumentation();
    void completeValueValidation();
};

void NightLightValuesTests::temperatureBoundsAndStep()
{
    static_assert(kFloor == 1000 && kCeiling == 6500 && kStep == 100);
    QVERIFY(isValidTemperature(kFloor));
    QVERIFY(isValidTemperature(3400));
    QVERIFY(isValidTemperature(kDefaultDayTemperatureKelvin));
    QVERIFY(isValidTemperature(kDefaultNightTemperatureKelvin));
    QVERIFY(isValidTemperature(kCeiling));

    // Negative controls: off-range and off-grid values are invalid.
    QVERIFY(!isValidTemperature(kFloor - 1));
    QVERIFY(!isValidTemperature(kCeiling + 1));
    QVERIFY(!isValidTemperature(3450));
    QVERIFY(!isValidTemperature(0));
    QVERIFY(!isValidTemperature(-3400));

    QCOMPARE(clampTemperature(kFloor - 1), kFloor);
    QCOMPARE(clampTemperature(kCeiling + 1), kCeiling);
    QCOMPARE(clampTemperature(3400), 3400);
}

void NightLightValuesTests::temperatureSnapping()
{
    QCOMPARE(snapTemperature(3400), 3400);
    QCOMPARE(snapTemperature(3449), 3400);
    QCOMPARE(snapTemperature(3450), 3500);
    QCOMPARE(snapTemperature(3499), 3500);
    QCOMPARE(snapTemperature(kFloor - 500), kFloor);
    QCOMPARE(snapTemperature(kCeiling + 500), kCeiling);
    // No tie can push a snap past the documented ceiling.
    QCOMPARE(snapTemperature(kCeiling - 1), kCeiling);
    QCOMPARE(snapTemperature(kFloor + 1), kFloor);
}

void NightLightValuesTests::coordinateValidation()
{
    QVERIFY(isValidLatitude(0.0));
    QVERIFY(isValidLatitude(90.0));
    QVERIFY(isValidLatitude(-90.0));
    QVERIFY(isValidLongitude(180.0));
    QVERIFY(isValidLongitude(-180.0));

    QVERIFY(!isValidLatitude(90.0001));
    QVERIFY(!isValidLatitude(-90.0001));
    QVERIFY(!isValidLongitude(180.0001));
    QVERIFY(!isValidLongitude(-180.0001));
    QVERIFY(!isValidLatitude(std::numeric_limits<double>::quiet_NaN()));
    QVERIFY(!isValidLongitude(std::numeric_limits<double>::infinity()));
    QVERIFY(!isValidLongitude(-std::numeric_limits<double>::infinity()));
}

void NightLightValuesTests::transitionValidation()
{
    QVERIFY(!isValidTransitionSeconds(kMinTransitionSeconds - 1));
    QVERIFY(isValidTransitionSeconds(kMinTransitionSeconds));
    QVERIFY(isValidTransitionSeconds(kDefaultTransitionSeconds));
    QVERIFY(isValidTransitionSeconds(kMaxTransitionSeconds));
    QVERIFY(!isValidTransitionSeconds(kMaxTransitionSeconds + 1));
    QVERIFY(!isValidTransitionSeconds(0));
    QVERIFY(!isValidTransitionSeconds(-1800));
}

void NightLightValuesTests::scheduleTimeValidation()
{
    QVERIFY(isValidScheduleTimes(QTime(6, 0, 0), QTime(18, 0, 0)));
    QVERIFY(isValidScheduleTimes(QTime(0, 0, 0), QTime(23, 59, 59)));

    // Negative controls: equal, inverted, or invalid times are refused.
    QVERIFY(!isValidScheduleTimes(QTime(18, 0, 0), QTime(6, 0, 0)));
    QVERIFY(!isValidScheduleTimes(QTime(6, 0, 0), QTime(6, 0, 0)));
    QVERIFY(!isValidScheduleTimes(QTime(), QTime(18, 0, 0)));
    QVERIFY(!isValidScheduleTimes(QTime(6, 0, 0), QTime()));
}

void NightLightValuesTests::configTokenMappingRejectsForeignTokens()
{
    QCOMPARE(modeFromConfigToken(QStringLiteral("Constant")),
             std::optional(Mode::Constant));
    QCOMPARE(modeFromConfigToken(QStringLiteral("DarkLight")),
             std::optional(Mode::DarkLight));
    QCOMPARE(modeToConfigToken(Mode::Constant), QStringLiteral("Constant"));
    QCOMPARE(modeToConfigToken(Mode::DarkLight), QStringLiteral("DarkLight"));

    // The pre-6.6 kwinrc Mode values must not map: accepting them would let
    // an outdated schema write through the new one.
    QVERIFY(!modeFromConfigToken(QStringLiteral("Automatic")).has_value());
    QVERIFY(!modeFromConfigToken(QStringLiteral("TimingsFixed")).has_value());
    QVERIFY(!modeFromConfigToken(QStringLiteral("constant")).has_value());
    QVERIFY(!modeFromConfigToken(QString()).has_value());

    QCOMPARE(sourceFromConfigToken(QStringLiteral("Location")),
             std::optional(ScheduleSource::Location));
    QCOMPARE(sourceFromConfigToken(QStringLiteral("Times")),
             std::optional(ScheduleSource::Times));
    QCOMPARE(sourceToConfigToken(ScheduleSource::Location),
             QStringLiteral("Location"));
    QCOMPARE(sourceToConfigToken(ScheduleSource::Times),
             QStringLiteral("Times"));
    QVERIFY(!sourceFromConfigToken(QStringLiteral("Solar")).has_value());
    QVERIFY(!sourceFromConfigToken(QString()).has_value());
}

void NightLightValuesTests::busModeMappingRejectsOutdatedDocumentation()
{
    // The installed D-Bus XML claims "0 automatic, 1 location, 2 timings,
    // 3 constant"; the kcfg fixes Constant=0 and DarkLight=1 instead. The
    // documented-but-wrong integers must fail closed.
    QCOMPARE(modeFromBusValue(0), std::optional(Mode::Constant));
    QCOMPARE(modeFromBusValue(1), std::optional(Mode::DarkLight));
    QVERIFY(!modeFromBusValue(2).has_value());
    QVERIFY(!modeFromBusValue(3).has_value());
    QVERIFY(!modeFromBusValue(4294967295u).has_value());
}

void NightLightValuesTests::completeValueValidation()
{
    OutputSettings output;
    QVERIFY(isValidOutput(output));
    output.nightTemperatureKelvin = 20000;
    QVERIFY(!isValidOutput(output));
    output.nightTemperatureKelvin = 3450;
    QVERIFY(!isValidOutput(output));

    ScheduleSettings schedule;
    QVERIFY(isValidSchedule(schedule));
    schedule.latitudeDegrees = 91.0;
    QVERIFY(!isValidSchedule(schedule));
    schedule = ScheduleSettings{};
    schedule.source = ScheduleSource::Times;
    QVERIFY(isValidSchedule(schedule));
    schedule.sunsetStart = QTime(5, 0, 0);
    QVERIFY(!isValidSchedule(schedule));
    schedule = ScheduleSettings{};
    schedule.transitionSeconds = kMaxTransitionSeconds + 1;
    QVERIFY(!isValidSchedule(schedule));

    NightLightSettings settings;
    QVERIFY(settings == NightLightSettings{});
}

QTEST_MAIN(NightLightValuesTests)
#include "tst_night_light_values.moc"
