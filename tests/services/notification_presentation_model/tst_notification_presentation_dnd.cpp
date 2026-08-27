// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/services/notification_presentation/presentation_snapshot.h"
#include "qindaqt/services/notification_presentation_client/notification_presentation_client.h"
#include "qindaqt/services/notification_presentation_client/presentation_transport.h"
#include "qindaqt/services/notification_presentation_model/notification_list_model.h"
#include "qindaqt/services/notification_presentation_model/notification_presentation_controller.h"
#include "qindaqt/services/notification_presentation_policy/notification_interruption_policy.h"

#include <QSet>
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
    void invokeAction(quint64, const QString &, quint32, const QString &,
                      const QString &) override
    {
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
        Q_EMIT operationFailed(
            operation.token, operation.owner,
            QStringLiteral("org.freedesktop.DBus.Error.Failed"), message);
    }

    QVector<Request> requests;
    QVector<Operation> operations;
};

NotificationPresentation::PresentationAccessToken token()
{
    auto value = NotificationPresentation::PresentationAccessToken::fromHex(
        QString(64, QLatin1Char('d')));
    Q_ASSERT(value.has_value());
    return std::move(*value);
}

NotificationPresentationClient::ClientTiming clientTiming()
{
    return {.debounceMilliseconds = 1,
            .requestTimeoutMilliseconds = 500,
            .retryMilliseconds = {2, 5}};
}

NotificationPresentation::PresentationNotification notification(
    quint32 id, QString summary, quint32 urgency)
{
    NotificationPresentation::PresentationNotification result;
    result.id = id;
    result.applicationName = QStringLiteral("DND Test");
    result.summary = std::move(summary);
    result.body = QStringLiteral("Plain body");
    result.urgency = urgency;
    result.createdAtMs = 100;
    return result;
}

QVariantMap wire(
    const QString &epoch, quint64 revision,
    QVector<NotificationPresentation::PresentationNotification> notifications)
{
    return NotificationPresentation::PresentationSnapshotCodec::encode(
        {epoch, revision, std::move(notifications)});
}

QVariant notificationIdAt(QAbstractItemModel *model, int row)
{
    return model->data(
        model->index(row, 0),
        NotificationPresentationModel::NotificationListModel::NotificationIdRole);
}

NotificationPresentationModel::PresentationTiming longPopupTiming()
{
    return {.lowUrgencyMilliseconds = 1'000,
            .normalUrgencyMilliseconds = 1'000,
            .criticalUrgencyMilliseconds = 1'000,
            .operationErrorMilliseconds = 100,
            .maximumPopups = 8,
            .maximumHistory = 10};
}

} // namespace

class NotificationPresentationDndTests final : public QObject {
    Q_OBJECT

private slots:
    void filtersPopupsWithoutLosingStateOrReplaying();
    void handlesUrgencyChangingReplacements();
    void survivesOwnerAndEpochRebaselineWithoutReplay();
    void keepsSuppressionWhenAnOperationIsRejected();
};

void NotificationPresentationDndTests::
    filtersPopupsWithoutLosingStateOrReplaying()
{
    FakeTransport transport;
    NotificationPresentationClient::NotificationPresentationClient client(
        transport, token(), clientTiming());
    NotificationPresentationPolicy::NotificationInterruptionPolicy policy;
    NotificationPresentationModel::NotificationPresentationController controller(
        client, policy, longPopupTiming());
    QSignalSpy dndChanges(
        &controller,
        &NotificationPresentationModel::NotificationPresentationController::
            doNotDisturbEnabledChanged);
    QVERIFY(client.start());
    const QString owner = QStringLiteral(":1.74");
    const QString epoch = QStringLiteral("bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb");
    transport.owner(owner);
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 1, 100);
    transport.reply(transport.requests[0], wire(epoch, 0, {}));

    transport.changed(owner, epoch, 1);
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 2, 100);
    transport.reply(
        transport.requests[1],
        wire(epoch, 1,
             {notification(1, QStringLiteral("Low"), 0),
              notification(2, QStringLiteral("Normal"), 1),
              notification(3, QStringLiteral("Critical"), 2)}));
    QTRY_COMPARE_WITH_TIMEOUT(controller.popupCount(), 3, 100);

    controller.setDoNotDisturbEnabled(true);
    QVERIFY(controller.doNotDisturbEnabled());
    QCOMPARE(dndChanges.size(), 1);
    QCOMPARE(controller.activeModel()->rowCount(), 3);
    QCOMPARE(controller.popupCount(), 1);
    QCOMPARE(notificationIdAt(controller.popupModel(), 0).toUInt(), quint32(3));

    transport.changed(owner, epoch, 2);
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 3, 100);
    transport.reply(
        transport.requests[2],
        wire(epoch, 2,
             {notification(1, QStringLiteral("Low"), 0),
              notification(2, QStringLiteral("Normal"), 1),
              notification(3, QStringLiteral("Critical"), 2),
              notification(4, QStringLiteral("Suppressed later"), 1),
              notification(5, QStringLiteral("Critical later"), 2)}));
    QTRY_COMPARE_WITH_TIMEOUT(controller.activeModel()->rowCount(), 5, 100);
    QCOMPARE(controller.popupCount(), 2);

    controller.setDoNotDisturbEnabled(false);
    QVERIFY(!controller.doNotDisturbEnabled());
    QCOMPARE(dndChanges.size(), 2);
    // AGENT-CONTRACT: disabling DND never turns already-active entries into
    // delayed banners; only future changes may enter the popup projection.
    QCOMPARE(controller.popupCount(), 2);

    transport.changed(owner, epoch, 3);
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 4, 100);
    transport.reply(transport.requests[3], wire(epoch, 3, {}));
    QTRY_COMPARE_WITH_TIMEOUT(controller.activeModel()->rowCount(), 0, 100);
    QCOMPARE(controller.historyModel()->rowCount(), 5);
    QSet<quint32> historyIds;
    for (int row = 0; row < controller.historyModel()->rowCount(); ++row) {
        historyIds.insert(notificationIdAt(controller.historyModel(), row).toUInt());
    }
    QCOMPARE(historyIds, QSet<quint32>({1, 2, 3, 4, 5}));
}

void NotificationPresentationDndTests::handlesUrgencyChangingReplacements()
{
    FakeTransport transport;
    NotificationPresentationClient::NotificationPresentationClient client(
        transport, token(), clientTiming());
    NotificationPresentationPolicy::NotificationInterruptionPolicy policy;
    NotificationPresentationModel::NotificationPresentationController controller(
        client, policy, longPopupTiming());
    QVERIFY(client.start());
    const QString owner = QStringLiteral(":1.75");
    const QString epoch = QStringLiteral("cccccccc-cccc-cccc-cccc-cccccccccccc");
    transport.owner(owner);
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 1, 100);
    transport.reply(transport.requests[0], wire(epoch, 0, {}));
    controller.setDoNotDisturbEnabled(true);

    transport.changed(owner, epoch, 1);
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 2, 100);
    transport.reply(transport.requests[1],
                    wire(epoch, 1,
                         {notification(7, QStringLiteral("Normal"), 1)}));
    QTRY_COMPARE_WITH_TIMEOUT(controller.activeModel()->rowCount(), 1, 100);
    QCOMPARE(controller.popupCount(), 0);

    transport.changed(owner, epoch, 2);
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 3, 100);
    transport.reply(transport.requests[2],
                    wire(epoch, 2,
                         {notification(7, QStringLiteral("Critical"), 2)}));
    QTRY_COMPARE_WITH_TIMEOUT(controller.popupCount(), 1, 100);

    transport.changed(owner, epoch, 3);
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 4, 100);
    transport.reply(transport.requests[3],
                    wire(epoch, 3,
                         {notification(7, QStringLiteral("Normal again"), 1)}));
    QTRY_COMPARE_WITH_TIMEOUT(controller.popupCount(), 0, 100);
}

void NotificationPresentationDndTests::
    survivesOwnerAndEpochRebaselineWithoutReplay()
{
    FakeTransport transport;
    NotificationPresentationClient::NotificationPresentationClient client(
        transport, token(), clientTiming());
    NotificationPresentationPolicy::NotificationInterruptionPolicy policy;
    NotificationPresentationModel::NotificationPresentationController controller(
        client, policy, longPopupTiming());
    QVERIFY(client.start());
    const QString firstOwner = QStringLiteral(":1.76");
    const QString firstEpoch =
        QStringLiteral("dddddddd-dddd-dddd-dddd-dddddddddddd");
    transport.owner(firstOwner);
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 1, 100);
    transport.reply(transport.requests.last(), wire(firstEpoch, 0, {}));
    controller.setDoNotDisturbEnabled(true);

    transport.changed(firstOwner, firstEpoch, 1);
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 2, 100);
    transport.reply(
        transport.requests.last(),
        wire(firstEpoch, 1,
             {notification(10, QStringLiteral("Suppressed"), 1),
              notification(11, QStringLiteral("Critical"), 2)}));
    QTRY_COMPARE_WITH_TIMEOUT(controller.activeModel()->rowCount(), 2, 100);
    QCOMPARE(controller.popupCount(), 1);

    transport.owner({});
    QTRY_COMPARE_WITH_TIMEOUT(controller.activeModel()->rowCount(), 0, 100);
    QCOMPARE(controller.popupCount(), 0);
    QVERIFY(controller.doNotDisturbEnabled());

    const QString secondOwner = QStringLiteral(":1.77");
    const QString secondEpoch =
        QStringLiteral("eeeeeeee-eeee-eeee-eeee-eeeeeeeeeeee");
    transport.owner(secondOwner);
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 3, 100);
    transport.reply(
        transport.requests.last(),
        wire(secondEpoch, 0,
             {notification(10, QStringLiteral("Suppressed"), 1),
              notification(11, QStringLiteral("Critical"), 2)}));
    QTRY_COMPARE_WITH_TIMEOUT(controller.activeModel()->rowCount(), 2, 100);
    QCOMPARE(controller.popupCount(), 0);
    QCOMPARE(controller.historyModel()->rowCount(), 0);

    transport.changed(secondOwner, secondEpoch, 1);
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 4, 100);
    transport.reply(
        transport.requests.last(),
        wire(secondEpoch, 1,
             {notification(10, QStringLiteral("Suppressed"), 1),
              notification(11, QStringLiteral("Critical"), 2),
              notification(12, QStringLiteral("Still suppressed"), 1)}));
    QTRY_COMPARE_WITH_TIMEOUT(controller.activeModel()->rowCount(), 3, 100);
    QCOMPARE(controller.popupCount(), 0);

    transport.changed(secondOwner, secondEpoch, 2);
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 5, 100);
    transport.reply(
        transport.requests.last(),
        wire(secondEpoch, 2,
             {notification(10, QStringLiteral("Suppressed"), 1),
              notification(11, QStringLiteral("Critical"), 2),
              notification(12, QStringLiteral("Still suppressed"), 1),
              notification(13, QStringLiteral("New critical"), 2)}));
    QTRY_COMPARE_WITH_TIMEOUT(controller.popupCount(), 1, 100);
    QCOMPARE(notificationIdAt(controller.popupModel(), 0).toUInt(), quint32(13));
}

void NotificationPresentationDndTests::
    keepsSuppressionWhenAnOperationIsRejected()
{
    FakeTransport transport;
    NotificationPresentationClient::NotificationPresentationClient client(
        transport, token(), clientTiming());
    NotificationPresentationPolicy::NotificationInterruptionPolicy policy;
    NotificationPresentationModel::NotificationPresentationController controller(
        client, policy, longPopupTiming());
    QVERIFY(client.start());
    const QString owner = QStringLiteral(":1.78");
    const QString epoch = QStringLiteral("ffffffff-ffff-ffff-ffff-ffffffffffff");
    transport.owner(owner);
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 1, 100);
    transport.reply(transport.requests.last(), wire(epoch, 0, {}));

    transport.changed(owner, epoch, 1);
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 2, 100);
    transport.reply(transport.requests.last(),
                    wire(epoch, 1,
                         {notification(20, QStringLiteral("Actionable"), 1)}));
    QTRY_COMPARE_WITH_TIMEOUT(controller.popupCount(), 1, 100);
    QVERIFY(controller.dismiss(20));
    QCOMPARE(transport.operations.size(), 1);
    QVERIFY(controller.operationBusy());

    controller.setDoNotDisturbEnabled(true);
    QCOMPARE(controller.popupCount(), 0);
    transport.reject(transport.operations[0], QStringLiteral("injected rejection"));
    QTRY_VERIFY_WITH_TIMEOUT(!controller.operationBusy(), 100);
    QVERIFY(controller.doNotDisturbEnabled());
    QCOMPARE(controller.popupCount(), 0);
    QCOMPARE(controller.operationErrorText(), QStringLiteral("injected rejection"));

    controller.setDoNotDisturbEnabled(false);
    QCOMPARE(controller.popupCount(), 0);
}

QTEST_MAIN(NotificationPresentationDndTests)

#include "tst_notification_presentation_dnd.moc"
