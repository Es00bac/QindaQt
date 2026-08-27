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

QTEST_MAIN(NotificationInterruptionPolicyTest)
#include "tst_notification_interruption_policy.moc"
