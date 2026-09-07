// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/session/desktop_controls/idle_display_preferences.h>

#include <QtTest>

using namespace QindaQt::Session::DesktopControls;

class IdleDisplayPreferencesTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void negativePersistedValueMeansNever();
    void zeroPersistedValueMeansNever();
    void positiveValueIsKeptWithinBounds();
    void hugeValueIsClampedToMaximum();
    void disabledRoundTripsThroughPersistedMinutes();
    void enabledRoundTripsThroughPersistedMinutes();
};

void IdleDisplayPreferencesTest::negativePersistedValueMeansNever() {
    const auto preferences = IdleDisplayPreferences::fromMinutes(-1);
    QVERIFY(!preferences.enabled);
    QCOMPARE(preferences.toPersistedMinutes(), -1);
}

void IdleDisplayPreferencesTest::zeroPersistedValueMeansNever() {
    const auto preferences = IdleDisplayPreferences::fromMinutes(0);
    QVERIFY(!preferences.enabled);
    QCOMPARE(preferences.toPersistedMinutes(), -1);
}

void IdleDisplayPreferencesTest::positiveValueIsKeptWithinBounds() {
    const auto preferences = IdleDisplayPreferences::fromMinutes(15);
    QVERIFY(preferences.enabled);
    QCOMPARE(preferences.minutes, 15);
    QCOMPARE(preferences.toPersistedMinutes(), 15);
}

void IdleDisplayPreferencesTest::hugeValueIsClampedToMaximum() {
    const auto preferences =
        IdleDisplayPreferences::fromMinutes(1'000'000);
    QVERIFY(preferences.enabled);
    QCOMPARE(preferences.minutes, IdleDisplayPreferences::maximumTimeoutMinutes());
}

void IdleDisplayPreferencesTest::disabledRoundTripsThroughPersistedMinutes() {
    const IdleDisplayPreferences preferences{false, 0};
    QCOMPARE(IdleDisplayPreferences::fromMinutes(preferences.toPersistedMinutes()),
             preferences);
}

void IdleDisplayPreferencesTest::enabledRoundTripsThroughPersistedMinutes() {
    const IdleDisplayPreferences preferences{true, 20};
    QCOMPARE(IdleDisplayPreferences::fromMinutes(preferences.toPersistedMinutes()),
             preferences);
}

QTEST_MAIN(IdleDisplayPreferencesTest)
#include "tst_idle_display_preferences.moc"
