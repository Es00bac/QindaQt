// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/session/desktop_controls/idle_display_policy.h>

#include <QtTest>

using namespace QindaQt::Session::DesktopControls;

namespace {

class FakePreferences final : public IdlePreferencesProvider {
    Q_OBJECT

public:
    using IdlePreferencesProvider::IdlePreferencesProvider;

    [[nodiscard]] IdleDisplayPreferences currentPreferences() const override
    {
        return current;
    }
    void refresh() override { ++refreshCount; }

    void publish(const IdleDisplayPreferences &next)
    {
        current = next;
        Q_EMIT preferencesChanged(current);
    }

    IdleDisplayPreferences current{true, 10};
    int refreshCount = 0;
};

class FakeIdleTracker final : public IdleTracker {
    Q_OBJECT

public:
    using IdleTracker::IdleTracker;

    void armTimeout(int milliseconds) override { armed.insert(milliseconds); }
    void disarmTimeout(int milliseconds) override { armed.remove(milliseconds); }

    void simulateTimeout(int milliseconds)
    {
        if (armed.contains(milliseconds)) {
            Q_EMIT timeoutReached(milliseconds);
        }
    }
    void simulateResume() { Q_EMIT resumingFromIdle(); }

    QSet<int> armed;
};

class FakeDpms final : public DpmsController {
    Q_OBJECT

public:
    using DpmsController::DpmsController;

    [[nodiscard]] bool available() const override { return available_; }
    void requestDisplaysOff() override { ++offRequests; }
    void requestDisplaysOn() override { ++onRequests; }

    void setAvailable(bool available)
    {
        if (available_ == available) {
            return;
        }
        available_ = available;
        Q_EMIT availabilityChanged(available);
    }

    bool available_ = true;
    int offRequests = 0;
    int onRequests = 0;
};

} // namespace

class IdleDisplayPolicyTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void init();
    void startArmsConfiguredTimeoutAndRefreshesPreferences();
    void reachingTheArmedTimeoutRequestsDisplaysOff();
    void resumingFromIdleRequestsDisplaysBackOn();
    void disabledPreferenceDisarmsAndNeverTurnsDisplaysOff();
    void unavailableDpmsKeepsPolicyPassive();
    void preferenceChangeRearmsTheNewTimeoutOnly();
    void preferenceChangeToDisabledLeavesDisplaysOn();
    void unavailableAtStartThenAvailableArmsTheCurrentPreference();
    void lateAvailabilityWithDisabledPreferenceStaysPassive();
    void recoveredAvailabilityRearmsAfterAnEqualSnapshot();

private:
    FakePreferences m_preferences;
    FakeIdleTracker m_tracker;
    FakeDpms m_dpms;
};

void IdleDisplayPolicyTest::init() {
    // The fakes live for the whole binary; reset their scripted truth so one
    // case's armed timeout or DPMS request never leaks into the next.
    m_preferences.current = IdleDisplayPreferences{true, 10};
    m_preferences.refreshCount = 0;
    m_tracker.armed.clear();
    m_dpms.available_ = true;
    m_dpms.offRequests = 0;
    m_dpms.onRequests = 0;
}

void IdleDisplayPolicyTest::startArmsConfiguredTimeoutAndRefreshesPreferences() {
    m_preferences.current = IdleDisplayPreferences{true, 7};
    IdleDisplayPolicy policy(m_preferences, m_tracker, m_dpms);
    policy.start();

    QCOMPARE(policy.armedTimeoutMilliseconds(), 7 * 60'000);
    QVERIFY(m_tracker.armed.contains(7 * 60'000));
    QCOMPARE(m_preferences.refreshCount, 1);
    QCOMPARE(m_dpms.offRequests, 0);
}

void IdleDisplayPolicyTest::reachingTheArmedTimeoutRequestsDisplaysOff() {
    m_preferences.current = IdleDisplayPreferences{true, 5};
    IdleDisplayPolicy policy(m_preferences, m_tracker, m_dpms);
    policy.start();

    m_tracker.simulateTimeout(5 * 60'000);

    QVERIFY(policy.displaysOffRequested());
    QCOMPARE(m_dpms.offRequests, 1);
    QCOMPARE(m_dpms.onRequests, 0);
}

void IdleDisplayPolicyTest::resumingFromIdleRequestsDisplaysBackOn() {
    m_preferences.current = IdleDisplayPreferences{true, 5};
    IdleDisplayPolicy policy(m_preferences, m_tracker, m_dpms);
    policy.start();
    m_tracker.simulateTimeout(5 * 60'000);
    QCOMPARE(m_dpms.offRequests, 1);

    m_tracker.simulateResume();

    QVERIFY(!policy.displaysOffRequested());
    QCOMPARE(m_dpms.onRequests, 1);
    // The timeout stays armed for the next idle period.
    QVERIFY(m_tracker.armed.contains(5 * 60'000));
}

void IdleDisplayPolicyTest::disabledPreferenceDisarmsAndNeverTurnsDisplaysOff() {
    m_preferences.current = IdleDisplayPreferences{false, 0};
    IdleDisplayPolicy policy(m_preferences, m_tracker, m_dpms);
    policy.start();

    QCOMPARE(policy.armedTimeoutMilliseconds(), 0);
    QVERIFY(m_tracker.armed.isEmpty());
    m_tracker.simulateResume();
    QCOMPARE(m_dpms.offRequests, 0);
}

void IdleDisplayPolicyTest::unavailableDpmsKeepsPolicyPassive() {
    m_preferences.current = IdleDisplayPreferences{true, 3};
    m_dpms.available_ = false;
    IdleDisplayPolicy policy(m_preferences, m_tracker, m_dpms);
    policy.start();

    QVERIFY(m_tracker.armed.isEmpty());
    m_tracker.simulateTimeout(3 * 60'000);
    QCOMPARE(m_dpms.offRequests, 0);
    QVERIFY(!policy.displaysOffRequested());
}

void IdleDisplayPolicyTest::preferenceChangeRearmsTheNewTimeoutOnly() {
    m_preferences.current = IdleDisplayPreferences{true, 4};
    IdleDisplayPolicy policy(m_preferences, m_tracker, m_dpms);
    policy.start();

    m_preferences.publish(IdleDisplayPreferences{true, 6});

    QCOMPARE(policy.armedTimeoutMilliseconds(), 6 * 60'000);
    QVERIFY(!m_tracker.armed.contains(4 * 60'000));
    QVERIFY(m_tracker.armed.contains(6 * 60'000));
    m_tracker.simulateTimeout(4 * 60'000);
    QCOMPARE(m_dpms.offRequests, 0);
    m_tracker.simulateTimeout(6 * 60'000);
    QCOMPARE(m_dpms.offRequests, 1);
}

void IdleDisplayPolicyTest::preferenceChangeToDisabledLeavesDisplaysOn() {
    m_preferences.current = IdleDisplayPreferences{true, 4};
    IdleDisplayPolicy policy(m_preferences, m_tracker, m_dpms);
    policy.start();
    m_tracker.simulateTimeout(4 * 60'000);
    QCOMPARE(m_dpms.offRequests, 1);

    m_preferences.publish(IdleDisplayPreferences{false, 0});

    QVERIFY(m_tracker.armed.isEmpty());
    QCOMPARE(m_dpms.onRequests, 1);
    QVERIFY(!policy.displaysOffRequested());
}

// AGENT-GUARD: regression for the audited P1 — a DPMS controller that is
// unavailable at start must not permanently disarm the policy when it becomes
// available later, even if Settings1 never re-sends the same snapshot.
void IdleDisplayPolicyTest::unavailableAtStartThenAvailableArmsTheCurrentPreference()
{
    m_preferences.current = IdleDisplayPreferences{true, 10};
    m_dpms.available_ = false;
    IdleDisplayPolicy policy(m_preferences, m_tracker, m_dpms);
    policy.start();

    QVERIFY(m_tracker.armed.isEmpty());

    m_dpms.setAvailable(true);

    QCOMPARE(policy.armedTimeoutMilliseconds(), 10 * 60'000);
    QVERIFY(m_tracker.armed.contains(10 * 60'000));
    m_tracker.simulateTimeout(10 * 60'000);
    QCOMPARE(m_dpms.offRequests, 1);
}

void IdleDisplayPolicyTest::lateAvailabilityWithDisabledPreferenceStaysPassive()
{
    m_preferences.current = IdleDisplayPreferences{false, 0};
    m_dpms.available_ = false;
    IdleDisplayPolicy policy(m_preferences, m_tracker, m_dpms);
    policy.start();

    m_dpms.setAvailable(true);

    QVERIFY(m_tracker.armed.isEmpty());
    QCOMPARE(m_dpms.offRequests, 0);
}

void IdleDisplayPolicyTest::recoveredAvailabilityRearmsAfterAnEqualSnapshot()
{
    m_preferences.current = IdleDisplayPreferences{true, 6};
    m_dpms.available_ = false;
    IdleDisplayPolicy policy(m_preferences, m_tracker, m_dpms);
    policy.start();

    // An equal snapshot is suppressed by the provider (no preferencesChanged);
    // only the availability recovery may re-apply it.
    m_dpms.setAvailable(true);
    m_preferences.publish(IdleDisplayPreferences{true, 6});

    QCOMPARE(policy.armedTimeoutMilliseconds(), 6 * 60'000);
    QVERIFY(m_tracker.armed.contains(6 * 60'000));
    QCOMPARE(m_tracker.armed.size(), 1);
}

QTEST_MAIN(IdleDisplayPolicyTest)
#include "tst_idle_display_policy.moc"
