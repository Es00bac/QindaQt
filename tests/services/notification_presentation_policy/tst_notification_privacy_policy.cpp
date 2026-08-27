// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/services/notification_presentation_policy/notification_privacy_policy.h"

#include <QMetaProperty>
#include <QSignalSpy>
#include <QTest>

using QindaQt::Services::NotificationPresentationPolicy::NotificationPrivacyPolicy;

class NotificationPrivacyPolicyTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void startsDeniedAndCannotBeGrantedThroughTheMetaObject();
    void emitsOnlyForEffectiveStateChanges();
};

void NotificationPrivacyPolicyTest::startsDeniedAndCannotBeGrantedThroughTheMetaObject()
{
    NotificationPrivacyPolicy policy;

    QVERIFY(!policy.privatePresentationAllowed());
    const int propertyIndex =
        policy.metaObject()->indexOfProperty("privatePresentationAllowed");
    QVERIFY(propertyIndex >= 0);
    QVERIFY(!policy.metaObject()->property(propertyIndex).isWritable());
    QCOMPARE(policy.metaObject()->indexOfMethod("setPrivatePresentationAllowed(bool)"),
             -1);

    policy.setPrivatePresentationAllowed(true);
    QVERIFY(policy.privatePresentationAllowed());
    NotificationPrivacyPolicy nextPolicy;
    QVERIFY(!nextPolicy.privatePresentationAllowed());
}

void NotificationPrivacyPolicyTest::emitsOnlyForEffectiveStateChanges()
{
    NotificationPrivacyPolicy policy;
    QSignalSpy changed(&policy,
                       &NotificationPrivacyPolicy::privatePresentationAllowedChanged);

    policy.setPrivatePresentationAllowed(false);
    QCOMPARE(changed.count(), 0);
    policy.setPrivatePresentationAllowed(true);
    QCOMPARE(changed.count(), 1);
    QCOMPARE(changed.at(0).at(0).toBool(), true);
    policy.setPrivatePresentationAllowed(true);
    QCOMPARE(changed.count(), 1);
    policy.setPrivatePresentationAllowed(false);
    QCOMPARE(changed.count(), 2);
    QCOMPARE(changed.at(1).at(0).toBool(), false);
}

QTEST_MAIN(NotificationPrivacyPolicyTest)
#include "tst_notification_privacy_policy.moc"
