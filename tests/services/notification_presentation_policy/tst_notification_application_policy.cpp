// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/notification_presentation/presentation_snapshot.h"
#include "qindaqt/services/notification_presentation_policy/notification_application_policy.h"

#include <QSignalSpy>
#include <QtTest>

using namespace QindaQt::Services::NotificationPresentationPolicy;
using namespace QindaQt::Services;

namespace {

QVariantMap policyRecord(bool muted, bool soundEnabled)
{
    return {{QStringLiteral("muted"), muted},
            {QStringLiteral("soundEnabled"), soundEnabled}};
}

} // namespace

class NotificationApplicationPolicyTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void encodesAndDecodesTheOnePersistedContract();
    void refusesMalformedDocumentsWithoutPublishingPartialRules();
    void boundsCanonicalDesktopIdRules();
    void matchesOnlyStableDesktopEntryIds();
    void muteAndSoundRemainIndependentFromUrgency();
};

void NotificationApplicationPolicyTest::encodesAndDecodesTheOnePersistedContract()
{
    const PerApplicationNotificationPolicies policies{
        {QStringLiteral("org.example.Chat"), {.muted = true, .soundEnabled = false}},
        {QStringLiteral("org.example.Mail"), {.muted = false, .soundEnabled = true}},
    };
    QVariantMap encoded;
    QString error;
    QVERIFY2(NotificationApplicationPolicy::encodeSettingsValue(policies, &encoded,
                                                                &error),
             qPrintable(error));
    QCOMPARE(encoded.value(QStringLiteral("org.example.Chat")).toMap(),
             policyRecord(true, false));
    QCOMPARE(encoded.value(QStringLiteral("org.example.Mail")).toMap(),
             policyRecord(false, true));

    const auto decoded = NotificationApplicationPolicy::decodeSettingsValue(encoded);
    QVERIFY2(decoded.ok(), qPrintable(decoded.error));
    QVERIFY(*decoded.policies == policies);

    PerApplicationNotificationPolicies withDefault = policies;
    withDefault.insert(QStringLiteral("org.example.Terminal"), {});
    QVERIFY2(NotificationApplicationPolicy::encodeSettingsValue(withDefault, &encoded,
                                                                 &error),
             qPrintable(error));
    QVERIFY(!encoded.contains(QStringLiteral("org.example.Terminal")));
    QVERIFY(*NotificationApplicationPolicy::decodeSettingsValue(encoded).policies ==
            policies);
}

void NotificationApplicationPolicyTest::
    refusesMalformedDocumentsWithoutPublishingPartialRules()
{
    NotificationApplicationPolicy policy;
    QVERIFY(policy.setPolicies({
        {QStringLiteral("org.example.Chat"), {.muted = true, .soundEnabled = false}},
    }));
    QSignalSpy changed(&policy, &NotificationApplicationPolicy::policiesChanged);

    const QList<QVariant> malformed{
        QVariant(QStringLiteral("org.example.Chat")),
        QVariantMap{{QStringLiteral("../Chat"), policyRecord(true, false)}},
        QVariantMap{{QStringLiteral("org.example.Chat.desktop"), policyRecord(true, false)}},
        QVariantMap{{QStringLiteral("org.example.Chat"),
                     QVariantMap{{QStringLiteral("muted"), QStringLiteral("true")},
                                 {QStringLiteral("soundEnabled"), false}}}},
        QVariantMap{{QStringLiteral("org.example.Chat"),
                     QVariantMap{{QStringLiteral("muted"), true}}}},
        QVariantMap{{QStringLiteral("org.example.Chat"),
                     QVariantMap{{QStringLiteral("muted"), true},
                                 {QStringLiteral("soundEnabled"), false},
                                 {QStringLiteral("future"), false}}}},
        QVariantMap{{QStringLiteral("org.example.Chat"), policyRecord(false, false)}},
    };
    for (const QVariant &value : malformed) {
        const auto decoded = NotificationApplicationPolicy::decodeSettingsValue(value);
        QVERIFY(!decoded.ok());
        QString error;
        QVERIFY(!policy.setSettingsValue(value, &error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(policy.policies().size(), 1);
        QVERIFY(policy.policies().value(QStringLiteral("org.example.Chat")).muted);
    }
    QCOMPARE(changed.size(), 0);
}

void NotificationApplicationPolicyTest::boundsCanonicalDesktopIdRules()
{
    PerApplicationNotificationPolicies atLimit;
    for (int index = 0;
         index < NotificationApplicationPolicy::MaximumApplicationRules; ++index) {
        atLimit.insert(QStringLiteral("app%1.desktop-id").arg(index),
                       {.muted = true, .soundEnabled = false});
    }
    QVariantMap encoded;
    QString error;
    QVERIFY(NotificationApplicationPolicy::encodeSettingsValue(atLimit, &encoded,
                                                               &error));
    QCOMPARE(encoded.size(), NotificationApplicationPolicy::MaximumApplicationRules);
    atLimit.insert(QStringLiteral("app-over-limit.desktop-id"),
                   {.muted = true, .soundEnabled = false});
    QVERIFY(!NotificationApplicationPolicy::encodeSettingsValue(atLimit, &encoded,
                                                                &error));
    QVERIFY(!error.isEmpty());

    QVERIFY(NotificationApplicationPolicy::isCanonicalDesktopId(
        QStringLiteral("org.example.App-1_2")));
    QVERIFY(!NotificationApplicationPolicy::isCanonicalDesktopId(QString{}));
    QVERIFY(!NotificationApplicationPolicy::isCanonicalDesktopId(
        QStringLiteral("org.example/App")));
    QVERIFY(!NotificationApplicationPolicy::isCanonicalDesktopId(
        QStringLiteral("org.example.App.desktop")));
    QVERIFY(!NotificationApplicationPolicy::isCanonicalDesktopId(
        QStringLiteral("org.example.App") + QChar::Null +
        QStringLiteral("Injected")));
    QVERIFY(!NotificationApplicationPolicy::isCanonicalDesktopId(
        QString(NotificationApplicationPolicy::MaximumDesktopIdCodeUnits + 1,
                QLatin1Char('a'))));
}

void NotificationApplicationPolicyTest::matchesOnlyStableDesktopEntryIds()
{
    NotificationApplicationPolicy policy;
    QVERIFY(policy.setPolicies({
        {QStringLiteral("org.example.Chat"), {.muted = true, .soundEnabled = true}},
    }));

    NotificationPresentation::PresentationNotification known;
    known.desktopEntry = QStringLiteral("org.example.Chat.desktop");
    QVERIFY(!policy.allowsPopup(known));
    QVERIFY(!policy.allowsSound(known));

    known.desktopEntry = QStringLiteral("org.example.Unknown");
    QVERIFY(policy.allowsPopup(known));
    QVERIFY(!policy.allowsSound(known));

    known.desktopEntry = QStringLiteral("../org.example.Chat");
    QVERIFY(policy.allowsPopup(known));
    QVERIFY(!policy.allowsSound(known));
    QVERIFY(NotificationApplicationPolicy::canonicalDesktopEntryId(
                QStringLiteral("org.example.Chat.desktop")) ==
            QStringLiteral("org.example.Chat"));
    QVERIFY(NotificationApplicationPolicy::canonicalDesktopEntryId(
                QStringLiteral("../org.example.Chat"))
                .isEmpty());
}

void NotificationApplicationPolicyTest::muteAndSoundRemainIndependentFromUrgency()
{
    NotificationApplicationPolicy policy;
    NotificationPresentation::PresentationNotification notification;
    notification.desktopEntry = QStringLiteral("org.example.App");
    notification.urgency = 2;

    // A missing rule permits visual notifications and remains silent.
    QVERIFY(policy.allowsPopup(notification));
    QVERIFY(!policy.allowsSound(notification));

    QVERIFY(policy.setPolicies({
        {QStringLiteral("org.example.App"), {.muted = false, .soundEnabled = true}},
    }));
    QVERIFY(policy.allowsPopup(notification));
    QVERIFY(policy.allowsSound(notification));

    // Explicit per-app mute wins at every urgency and suppresses sound even
    // when that app has sound enabled; critical bypass is a separate DND rule.
    QVERIFY(policy.setPolicies({
        {QStringLiteral("org.example.App"), {.muted = true, .soundEnabled = true}},
    }));
    QVERIFY(!policy.allowsPopup(notification));
    QVERIFY(!policy.allowsSound(notification));
    notification.urgency = 0;
    QVERIFY(!policy.allowsPopup(notification));
}

QTEST_MAIN(NotificationApplicationPolicyTest)

#include "tst_notification_application_policy.moc"
