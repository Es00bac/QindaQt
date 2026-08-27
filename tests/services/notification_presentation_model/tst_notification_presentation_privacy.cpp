// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/services/notification_presentation/presentation_snapshot.h"
#include "qindaqt/services/notification_presentation_client/notification_presentation_client.h"
#include "qindaqt/services/notification_presentation_client/presentation_transport.h"
#include "qindaqt/services/notification_presentation_model/notification_presentation_controller.h"
#include "qindaqt/services/notification_presentation_policy/notification_interruption_policy.h"
#include "qindaqt/services/notification_presentation_policy/notification_privacy_policy.h"

#include <QMetaProperty>
#include <QSignalSpy>
#include <QtTest>

#include <utility>

using namespace QindaQt::Services;

namespace {

struct Request final {
    quint64 token = 0;
    QString owner;
};

struct Operation final {
    quint64 token = 0;
    QString owner;
    quint32 id = 0;
};

class FakeTransport final
    : public NotificationPresentationClient::PresentationTransport {
public:
    using PresentationTransport::PresentationTransport;

    bool start(QString *) override { return true; }
    void stop() override {}
    void registerPresenter(quint64 token, const QString &owner,
                           const QString &) override
    {
        requests.append({token, owner});
    }
    void requestSnapshot(quint64 token, const QString &owner) override
    {
        requests.append({token, owner});
    }
    void releasePresenter(const QString &) override {}
    void dismiss(quint64 token, const QString &owner, quint32 id) override
    {
        operations.append({token, owner, id});
    }
    void invokeAction(quint64 token, const QString &owner, quint32 id, const QString &,
                      const QString &) override
    {
        operations.append({token, owner, id});
    }

    void owner(const QString &value) { Q_EMIT serviceOwnerChanged(value); }
    void changed(const QString &owner, const QString &epoch, quint64 revision)
    {
        Q_EMIT snapshotInvalidated(owner, epoch, revision);
    }
    void reply(const Request &request, const QVariantMap &snapshot)
    {
        Q_EMIT snapshotReceived(request.token, request.owner, snapshot);
    }
    void reject(const Operation &operation, const QString &message)
    {
        Q_EMIT operationFailed(operation.token, operation.owner,
                               QStringLiteral("org.freedesktop.DBus.Error.Failed"),
                               message);
    }

    QVector<Request> requests;
    QVector<Operation> operations;
};

NotificationPresentation::PresentationAccessToken token()
{
    auto value = NotificationPresentation::PresentationAccessToken::fromHex(
        QString(64, QLatin1Char('c')));
    Q_ASSERT(value.has_value());
    return std::move(*value);
}

NotificationPresentationClient::ClientTiming clientTiming()
{
    return {.debounceMilliseconds = 1,
            .requestTimeoutMilliseconds = 500,
            .retryMilliseconds = {2, 5}};
}

NotificationPresentationModel::PresentationTiming presentationTiming()
{
    return {.lowUrgencyMilliseconds = 80,
            .normalUrgencyMilliseconds = 80,
            .criticalUrgencyMilliseconds = 80,
            .operationErrorMilliseconds = 80,
            .maximumPopups = 8,
            .maximumHistory = 10};
}

NotificationPresentation::PresentationNotification
notification(quint32 id, QString summary, quint32 urgency = 1)
{
    NotificationPresentation::PresentationNotification result;
    result.id = id;
    result.applicationName = QStringLiteral("Privacy Test");
    result.summary = std::move(summary);
    result.body = QStringLiteral("Private body");
    result.urgency = urgency;
    result.createdAtMs = 100;
    result.actions = {{QStringLiteral("open"), QStringLiteral("Open")}};
    return result;
}

QVariantMap
wire(const QString &epoch, quint64 revision,
     QVector<NotificationPresentation::PresentationNotification> notifications)
{
    return NotificationPresentation::PresentationSnapshotCodec::encode(
        {epoch, revision, std::move(notifications)});
}

} // namespace

class NotificationPresentationPrivacyTests final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void startsDeniedAndBaselinesOnFirstGrant();
    void denialClearsEveryPresentationProjection();
    void denialSuppressesAnInFlightOutcomeAcrossUnlock();
    void privacyOutranksDndAndCriticalUrgency();
};

void NotificationPresentationPrivacyTests::startsDeniedAndBaselinesOnFirstGrant()
{
    FakeTransport transport;
    NotificationPresentationClient::NotificationPresentationClient client(
        transport, token(), clientTiming());
    NotificationPresentationPolicy::NotificationInterruptionPolicy interruption;
    NotificationPresentationPolicy::NotificationPrivacyPolicy privacy;
    NotificationPresentationModel::NotificationPresentationController controller(
        client, interruption, privacy, presentationTiming());
    QSignalSpy activeResets(controller.activeModel(),
                            &QAbstractItemModel::modelReset);
    QSignalSpy popupResets(controller.popupModel(),
                          &QAbstractItemModel::modelReset);
    QSignalSpy historyResets(controller.historyModel(),
                            &QAbstractItemModel::modelReset);

    QVERIFY(!controller.privatePresentationAllowed());
    const int privacyProperty =
        controller.metaObject()->indexOfProperty("privatePresentationAllowed");
    QVERIFY(privacyProperty >= 0);
    QVERIFY(!controller.metaObject()->property(privacyProperty).isWritable());
    QVERIFY(client.start());
    const QString owner = QStringLiteral(":1.80");
    const QString epoch = QStringLiteral("10101010-1010-1010-1010-101010101010");
    transport.owner(owner);
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 1, 100);
    transport.reply(transport.requests.last(),
                    wire(epoch, 1, {notification(1, QStringLiteral("Critical"), 2)}));
    QCOMPARE(controller.activeModel()->rowCount(), 0);
    QCOMPARE(controller.popupCount(), 0);
    QCOMPARE(controller.historyModel()->rowCount(), 0);
    QCOMPARE(activeResets.size(), 0);
    QCOMPARE(popupResets.size(), 0);
    QCOMPARE(historyResets.size(), 0);
    controller.setCenterOpen(true);
    controller.toggleCenter();
    QVERIFY(!controller.centerOpen());
    QVERIFY(!controller.dismiss(1));
    QVERIFY(!controller.invokeAction(1, QStringLiteral("open")));
    QCOMPARE(transport.operations.size(), 0);
    QVERIFY(controller.operationErrorText().isEmpty());

    privacy.setPrivatePresentationAllowed(true);
    QVERIFY(controller.privatePresentationAllowed());
    QCOMPARE(controller.activeModel()->rowCount(), 1);
    QCOMPARE(controller.popupCount(), 0);
    QCOMPARE(controller.historyModel()->rowCount(), 0);
    QCOMPARE(activeResets.size(), 1);
    QCOMPARE(popupResets.size(), 0);
    QCOMPARE(historyResets.size(), 0);

    transport.changed(owner, epoch, 2);
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 2, 100);
    transport.reply(transport.requests.last(),
                    wire(epoch, 2,
                         {notification(1, QStringLiteral("Critical"), 2),
                          notification(2, QStringLiteral("New critical"), 2)}));
    QTRY_COMPARE_WITH_TIMEOUT(controller.popupCount(), 1, 100);
}

void NotificationPresentationPrivacyTests::denialClearsEveryPresentationProjection()
{
    FakeTransport transport;
    NotificationPresentationClient::NotificationPresentationClient client(
        transport, token(), clientTiming());
    NotificationPresentationPolicy::NotificationInterruptionPolicy interruption;
    NotificationPresentationPolicy::NotificationPrivacyPolicy privacy;
    privacy.setPrivatePresentationAllowed(true);
    NotificationPresentationModel::NotificationPresentationController controller(
        client, interruption, privacy, presentationTiming());
    QVERIFY(client.start());
    const QString owner = QStringLiteral(":1.81");
    const QString epoch = QStringLiteral("20202020-2020-2020-2020-202020202020");
    transport.owner(owner);
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 1, 100);
    transport.reply(transport.requests.last(),
                    wire(epoch, 1, {notification(10, QStringLiteral("Existing"))}));
    QTRY_COMPARE_WITH_TIMEOUT(controller.activeModel()->rowCount(), 1, 100);

    transport.changed(owner, epoch, 2);
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 2, 100);
    transport.reply(transport.requests.last(),
                    wire(epoch, 2,
                         {notification(10, QStringLiteral("Existing")),
                          notification(11, QStringLiteral("Popup"))}));
    QTRY_COMPARE_WITH_TIMEOUT(controller.popupCount(), 1, 100);
    transport.changed(owner, epoch, 3);
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 3, 100);
    transport.reply(transport.requests.last(),
                    wire(epoch, 3, {notification(11, QStringLiteral("Popup"))}));
    QTRY_COMPARE_WITH_TIMEOUT(controller.historyModel()->rowCount(), 1, 100);
    QVERIFY(!controller.dismiss(99));
    QVERIFY(!controller.operationErrorText().isEmpty());
    controller.setCenterOpen(true);
    QVERIFY(controller.centerOpen());

    privacy.setPrivatePresentationAllowed(false);
    QVERIFY(!controller.centerOpen());
    QCOMPARE(controller.activeModel()->rowCount(), 0);
    QCOMPARE(controller.popupCount(), 0);
    QCOMPARE(controller.historyModel()->rowCount(), 0);
    QVERIFY(!controller.operationBusy());
    QVERIFY(controller.operationErrorText().isEmpty());
    QTest::qWait(100);
    QCOMPARE(controller.activeModel()->rowCount(), 0);
    QCOMPARE(controller.popupCount(), 0);
    QCOMPARE(controller.historyModel()->rowCount(), 0);
}

void NotificationPresentationPrivacyTests::
    denialSuppressesAnInFlightOutcomeAcrossUnlock()
{
    FakeTransport transport;
    NotificationPresentationClient::NotificationPresentationClient client(
        transport, token(), clientTiming());
    NotificationPresentationPolicy::NotificationInterruptionPolicy interruption;
    NotificationPresentationPolicy::NotificationPrivacyPolicy privacy;
    privacy.setPrivatePresentationAllowed(true);
    NotificationPresentationModel::NotificationPresentationController controller(
        client, interruption, privacy, presentationTiming());
    QSignalSpy visibleErrors(&controller,
                             &NotificationPresentationModel::
                                 NotificationPresentationController::operationError);
    QVERIFY(client.start());
    const QString owner = QStringLiteral(":1.82");
    const QString epoch = QStringLiteral("30303030-3030-3030-3030-303030303030");
    transport.owner(owner);
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 1, 100);
    transport.reply(transport.requests.last(),
                    wire(epoch, 1, {notification(20, QStringLiteral("Existing"))}));
    QTRY_COMPARE_WITH_TIMEOUT(controller.activeModel()->rowCount(), 1, 100);
    QVERIFY(controller.dismiss(20));
    QCOMPARE(transport.operations.size(), 1);
    QVERIFY(controller.operationBusy());

    privacy.setPrivatePresentationAllowed(false);
    QVERIFY(!controller.operationBusy());
    QCOMPARE(controller.activeModel()->rowCount(), 0);
    privacy.setPrivatePresentationAllowed(true);
    QCOMPARE(controller.activeModel()->rowCount(), 1);
    QCOMPARE(controller.popupCount(), 0);
    QVERIFY(!controller.dismiss(20));
    QCOMPARE(transport.operations.size(), 1);

    transport.reject(transport.operations.first(),
                     QStringLiteral("must remain private"));
    QTRY_VERIFY_WITH_TIMEOUT(!client.operationInFlight(), 100);
    QCOMPARE(visibleErrors.size(), 0);
    QVERIFY(controller.operationErrorText().isEmpty());
    QCOMPARE(controller.popupCount(), 0);

    QVERIFY(controller.dismiss(20));
    QCOMPARE(transport.operations.size(), 2);
}

void NotificationPresentationPrivacyTests::privacyOutranksDndAndCriticalUrgency()
{
    FakeTransport transport;
    NotificationPresentationClient::NotificationPresentationClient client(
        transport, token(), clientTiming());
    NotificationPresentationPolicy::NotificationInterruptionPolicy interruption;
    NotificationPresentationPolicy::NotificationPrivacyPolicy privacy;
    NotificationPresentationModel::NotificationPresentationController controller(
        client, interruption, privacy, presentationTiming());
    controller.setDoNotDisturbEnabled(true);
    QVERIFY(client.start());
    const QString owner = QStringLiteral(":1.83");
    const QString epoch = QStringLiteral("40404040-4040-4040-4040-404040404040");
    transport.owner(owner);
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 1, 100);
    transport.reply(transport.requests.last(),
                    wire(epoch, 1,
                         {notification(30, QStringLiteral("Normal"), 1),
                          notification(31, QStringLiteral("Critical"), 2)}));
    QCOMPARE(controller.activeModel()->rowCount(), 0);
    QCOMPARE(controller.popupCount(), 0);

    privacy.setPrivatePresentationAllowed(true);
    QCOMPARE(controller.activeModel()->rowCount(), 2);
    QCOMPARE(controller.popupCount(), 0);
    QCOMPARE(controller.historyModel()->rowCount(), 0);
    transport.changed(owner, epoch, 2);
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 2, 100);
    transport.reply(transport.requests.last(),
                    wire(epoch, 2,
                         {notification(30, QStringLiteral("Normal updated"), 1),
                          notification(31, QStringLiteral("Critical updated"), 2)}));
    QTRY_COMPARE_WITH_TIMEOUT(controller.popupCount(), 1, 100);

    privacy.setPrivatePresentationAllowed(false);
    QCOMPARE(controller.popupCount(), 0);
    QCOMPARE(controller.activeModel()->rowCount(), 0);
}

QTEST_GUILESS_MAIN(NotificationPresentationPrivacyTests)

#include "tst_notification_presentation_privacy.moc"
