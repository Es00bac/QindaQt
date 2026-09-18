// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/services/notification_presentation/presentation_snapshot.h"
#include "qindaqt/services/notification_presentation_policy/notification_interruption_policy.h"

#include <QSignalSpy>
#include <QTest>

using QindaQt::Services::NotificationPresentation::PresentationNotification;
using QindaQt::Services::NotificationPresentationPolicy::
    NotificationInterruptionPolicy;

namespace {

PresentationNotification notificationWithUrgency(quint32 urgency)
{
    PresentationNotification notification;
    notification.id = 1;
    notification.urgency = urgency;
    return notification;
}

} // namespace

class NotificationInterruptionPolicyTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void startsWithSessionVolatileDndDisabled();
    void emitsOnlyForStateChanges();
    void disabledDndAllowsEveryUrgency();
    void enabledDndAllowsOnlyCriticalUrgency();
    void admissionTracksCurrentState();
    void quietHoursStartOffAndDefaultToNight();
    void quietHoursWindowHandlesWrapAndBoundaries_data();
    void quietHoursWindowHandlesWrapAndBoundaries();
    void anEmptyWindowIsNeverAllDay();
    void quietHoursQuietLikeTheSwitchAndStillAdmitCritical();
    void anOutOfRangeWindowIsRefusedWhole();
    void quietHoursEmitOnlyForRealChanges();
};

void NotificationInterruptionPolicyTest::startsWithSessionVolatileDndDisabled()
{
    NotificationInterruptionPolicy first;
    QVERIFY(!first.doNotDisturbEnabled());

    first.setDoNotDisturbEnabled(true);
    NotificationInterruptionPolicy newSessionPolicy;
    QVERIFY(!newSessionPolicy.doNotDisturbEnabled());
}

void NotificationInterruptionPolicyTest::emitsOnlyForStateChanges()
{
    NotificationInterruptionPolicy policy;
    QSignalSpy changed(&policy,
                       &NotificationInterruptionPolicy::doNotDisturbEnabledChanged);

    policy.setDoNotDisturbEnabled(false);
    QCOMPARE(changed.count(), 0);

    policy.setDoNotDisturbEnabled(true);
    QCOMPARE(changed.count(), 1);
    QCOMPARE(changed.at(0).at(0).toBool(), true);

    policy.setDoNotDisturbEnabled(true);
    QCOMPARE(changed.count(), 1);

    policy.setDoNotDisturbEnabled(false);
    QCOMPARE(changed.count(), 2);
    QCOMPARE(changed.at(1).at(0).toBool(), false);
}

void NotificationInterruptionPolicyTest::disabledDndAllowsEveryUrgency()
{
    NotificationInterruptionPolicy policy;

    QVERIFY(policy.allowsPopup(notificationWithUrgency(0)));
    QVERIFY(policy.allowsPopup(notificationWithUrgency(1)));
    QVERIFY(policy.allowsPopup(notificationWithUrgency(2)));
    QVERIFY(policy.allowsPopup(notificationWithUrgency(3)));
}

void NotificationInterruptionPolicyTest::enabledDndAllowsOnlyCriticalUrgency()
{
    NotificationInterruptionPolicy policy;
    policy.setDoNotDisturbEnabled(true);

    QVERIFY(!policy.allowsPopup(notificationWithUrgency(0)));
    QVERIFY(!policy.allowsPopup(notificationWithUrgency(1)));
    QVERIFY(policy.allowsPopup(notificationWithUrgency(2)));
    QVERIFY(!policy.allowsPopup(notificationWithUrgency(3)));
}

void NotificationInterruptionPolicyTest::admissionTracksCurrentState()
{
    NotificationInterruptionPolicy policy;
    const PresentationNotification normal = notificationWithUrgency(1);

    QVERIFY(policy.allowsPopup(normal));
    policy.setDoNotDisturbEnabled(true);
    QVERIFY(!policy.allowsPopup(normal));
    policy.setDoNotDisturbEnabled(false);
    QVERIFY(policy.allowsPopup(normal));
}

void NotificationInterruptionPolicyTest::quietHoursStartOffAndDefaultToNight()
{
    NotificationInterruptionPolicy policy;
    const auto quietHours = policy.quietHours();
    // A first run must behave exactly as it did before the schedule existed.
    QVERIFY(!quietHours.enabled);
    QVERIFY(!policy.quietHoursActive());
    QCOMPARE(quietHours.startMinutes, 22 * 60);
    QCOMPARE(quietHours.endMinutes, 7 * 60);
}

void NotificationInterruptionPolicyTest::quietHoursWindowHandlesWrapAndBoundaries_data()
{
    QTest::addColumn<int>("start");
    QTest::addColumn<int>("end");
    QTest::addColumn<int>("now");
    QTest::addColumn<bool>("inside");

    // The default night window, which crosses midnight.
    QTest::newRow("22:00 start is inside") << 22 * 60 << 7 * 60 << 22 * 60 << true;
    QTest::newRow("just before 22:00 is outside") << 22 * 60 << 7 * 60 << 22 * 60 - 1 << false;
    QTest::newRow("midnight is inside") << 22 * 60 << 7 * 60 << 0 << true;
    QTest::newRow("06:59 is inside") << 22 * 60 << 7 * 60 << 7 * 60 - 1 << true;
    // AGENT-GUARD: the end is exclusive, so the alarm at 07:00 is heard.
    QTest::newRow("07:00 end is outside") << 22 * 60 << 7 * 60 << 7 * 60 << false;
    QTest::newRow("afternoon is outside") << 22 * 60 << 7 * 60 << 15 * 60 << false;
    // A same-day window, which does not wrap.
    QTest::newRow("same-day start inside") << 9 * 60 << 17 * 60 << 9 * 60 << true;
    QTest::newRow("same-day end outside") << 9 * 60 << 17 * 60 << 17 * 60 << false;
    QTest::newRow("same-day night outside") << 9 * 60 << 17 * 60 << 2 * 60 << false;
    QTest::newRow("same-day middle inside") << 9 * 60 << 17 * 60 << 12 * 60 << true;
}

void NotificationInterruptionPolicyTest::quietHoursWindowHandlesWrapAndBoundaries()
{
    QFETCH(int, start);
    QFETCH(int, end);
    QFETCH(int, now);
    QFETCH(bool, inside);

    NotificationInterruptionPolicy::QuietHours quietHours;
    quietHours.enabled = true;
    quietHours.startMinutes = start;
    quietHours.endMinutes = end;
    QCOMPARE(NotificationInterruptionPolicy::windowContains(quietHours, now), inside);

    NotificationInterruptionPolicy policy;
    policy.setQuietHours(quietHours);
    policy.setClock([now] { return now; });
    QCOMPARE(policy.quietHoursActive(), inside);

    // A disabled schedule is never active, whatever the clock says.
    quietHours.enabled = false;
    QVERIFY(!NotificationInterruptionPolicy::windowContains(quietHours, now));
}

void NotificationInterruptionPolicyTest::anEmptyWindowIsNeverAllDay()
{
    // AGENT-GUARD: equal ends mean an empty window, not a silent machine. A
    // user who picked the same time twice must not lose every notification.
    NotificationInterruptionPolicy::QuietHours quietHours;
    quietHours.enabled = true;
    quietHours.startMinutes = 9 * 60;
    quietHours.endMinutes = 9 * 60;
    for (const int now : {0, 9 * 60 - 1, 9 * 60, 9 * 60 + 1, 23 * 60 + 59}) {
        QVERIFY2(!NotificationInterruptionPolicy::windowContains(quietHours, now),
                 qPrintable(QStringLiteral("minute %1 must be outside").arg(now)));
    }
}

void NotificationInterruptionPolicyTest::quietHoursQuietLikeTheSwitchAndStillAdmitCritical()
{
    NotificationInterruptionPolicy policy;
    NotificationInterruptionPolicy::QuietHours quietHours;
    quietHours.enabled = true;
    quietHours.startMinutes = 22 * 60;
    quietHours.endMinutes = 7 * 60;
    policy.setQuietHours(quietHours);

    // Outside the window the schedule changes nothing at all.
    policy.setClock([] { return 15 * 60; });
    QVERIFY(!policy.quietHoursActive());
    for (const quint32 urgency : {0u, 1u, 2u}) {
        QVERIFY(policy.allowsPopup(notificationWithUrgency(urgency)));
    }

    // Inside it, the schedule quiets exactly as the switch does.
    policy.setClock([] { return 23 * 60; });
    QVERIFY(policy.quietHoursActive());
    QVERIFY(!policy.allowsPopup(notificationWithUrgency(0)));
    QVERIFY(!policy.allowsPopup(notificationWithUrgency(1)));
    // AGENT-GUARD: a schedule must never be able to hide an emergency.
    QVERIFY(policy.allowsPopup(notificationWithUrgency(2)));
    // And an out-of-protocol urgency still fails closed.
    QVERIFY(!policy.allowsPopup(notificationWithUrgency(7)));

    // The manual switch still wins on its own, with no schedule at all.
    NotificationInterruptionPolicy manual;
    manual.setClock([] { return 15 * 60; });
    manual.setDoNotDisturbEnabled(true);
    QVERIFY(!manual.quietHoursActive());
    QVERIFY(!manual.allowsPopup(notificationWithUrgency(1)));
    QVERIFY(manual.allowsPopup(notificationWithUrgency(2)));
}

void NotificationInterruptionPolicyTest::anOutOfRangeWindowIsRefusedWhole()
{
    NotificationInterruptionPolicy policy;
    const auto before = policy.quietHours();

    NotificationInterruptionPolicy::QuietHours bad;
    bad.enabled = true;
    bad.startMinutes = -1;
    bad.endMinutes = 7 * 60;
    policy.setQuietHours(bad);
    // AGENT-GUARD: refused whole, not clamped. A clamped end is a time the
    // user never chose, and this decides when the machine goes quiet.
    QVERIFY(policy.quietHours() == before);

    bad.startMinutes = 22 * 60;
    bad.endMinutes = 24 * 60;
    policy.setQuietHours(bad);
    QVERIFY(policy.quietHours() == before);

    bad.endMinutes = 23 * 60 + 59;
    policy.setQuietHours(bad);
    QCOMPARE(policy.quietHours().endMinutes, 23 * 60 + 59);
}

void NotificationInterruptionPolicyTest::quietHoursEmitOnlyForRealChanges()
{
    NotificationInterruptionPolicy policy;
    QSignalSpy changed(&policy, &NotificationInterruptionPolicy::quietHoursChanged);

    NotificationInterruptionPolicy::QuietHours quietHours = policy.quietHours();
    quietHours.enabled = true;
    policy.setQuietHours(quietHours);
    QCOMPARE(changed.count(), 1);

    policy.setQuietHours(quietHours);
    QCOMPARE(changed.count(), 1);

    // A refused window emits nothing either.
    NotificationInterruptionPolicy::QuietHours bad = quietHours;
    bad.startMinutes = 5000;
    policy.setQuietHours(bad);
    QCOMPARE(changed.count(), 1);
}

QTEST_MAIN(NotificationInterruptionPolicyTest)
#include "tst_notification_interruption_policy.moc"
