// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/night_light/night_light_schedule.h>

#include <QTimeZone>
#include <QtTest>

using namespace QindaQt::Services::NightLight;

class NightLightScheduleTests final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void fixedTimesExposePreviousNextAndDaylight();
    void automaticLocationRequiresARealFix();
    void solarTimesAreComputedAndPolarFailureIsTruthful();
};

void NightLightScheduleTests::fixedTimesExposePreviousNextAndDaylight()
{
    NightLightSettings settings;
    settings.output.active = true;
    settings.schedule.source = ScheduleSource::Times;
    settings.schedule.sunriseStart = QTime(6, 0);
    settings.schedule.sunsetStart = QTime(18, 0);
    settings.schedule.transitionSeconds = 1800;
    const QDateTime now(QDate(2026, 6, 12), QTime(7, 0), QTimeZone::UTC);
    const auto frame = calculateSchedule(settings, now);
    QVERIFY2(frame.available, qPrintable(frame.diagnostic));
    QVERIFY(frame.daylight);
    QCOMPARE(frame.previous.start, QDateTime(QDate(2026, 6, 12), QTime(6, 0), QTimeZone::UTC));
    QCOMPARE(frame.previous.end, QDateTime(QDate(2026, 6, 12), QTime(6, 30), QTimeZone::UTC));
    QCOMPARE(frame.next.start, QDateTime(QDate(2026, 6, 12), QTime(18, 0), QTimeZone::UTC));

    const auto beforeSunrise = calculateSchedule(
        settings, QDateTime(QDate(2026, 6, 12), QTime(5, 0), QTimeZone::UTC));
    QVERIFY(beforeSunrise.available);
    QVERIFY(!beforeSunrise.daylight);
    QCOMPARE(beforeSunrise.next.start, frame.previous.start);
}

void NightLightScheduleTests::automaticLocationRequiresARealFix()
{
    NightLightSettings settings;
    settings.output.active = true;
    settings.schedule.automaticLocation = true;
    const auto frame = calculateSchedule(
        settings, QDateTime(QDate(2026, 6, 12), QTime(12, 0), QTimeZone::UTC));
    QVERIFY(!frame.available);
    QVERIFY(frame.diagnostic.contains(QStringLiteral("location")));
}

void NightLightScheduleTests::solarTimesAreComputedAndPolarFailureIsTruthful()
{
    NightLightSettings settings;
    settings.output.active = true;
    settings.schedule.automaticLocation = false;
    settings.schedule.latitudeDegrees = 0.0;
    settings.schedule.longitudeDegrees = 0.0;
    const QDate date(2026, 3, 20);
    const auto equator = calculateSchedule(
        settings, QDateTime(date, QTime(12, 0), QTimeZone::UTC));
    QVERIFY2(equator.available, qPrintable(equator.diagnostic));
    QVERIFY(equator.daylight);
    QVERIFY(std::abs(equator.previous.start.time().hour() - 6) <= 1);
    QVERIFY(std::abs(equator.next.start.time().hour() - 18) <= 1);

    settings.schedule.latitudeDegrees = 89.0;
    const auto polar = calculateSchedule(
        settings, QDateTime(QDate(2026, 6, 21), QTime(12, 0), QTimeZone::UTC));
    QVERIFY(!polar.available);
    QVERIFY(polar.diagnostic.contains(QStringLiteral("solar")));
}

QTEST_GUILESS_MAIN(NightLightScheduleTests)
#include "tst_night_light_schedule.moc"
