// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/notification_presentation/presentation_snapshot.h"
#include "qindaqt/services/notification_presentation_client/notification_presentation_client.h"
#include "qindaqt/services/notification_presentation_client/presentation_transport.h"
#include "qindaqt/services/notification_presentation_model/notification_list_model.h"
#include "qindaqt/services/notification_presentation_model/notification_presentation_controller.h"
#include "qindaqt/services/notification_presentation_policy/notification_interruption_policy.h"

#include <QSignalSpy>
#include <QtTest>

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
    QString actionKey;
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
        operations.append({token, owner, id, {}});
    }
    void invokeAction(quint64 token, const QString &owner, quint32 id,
                      const QString &actionKey, const QString &) override
    {
        operations.append({token, owner, id, actionKey});
    }

    void owner(const QString &value) { Q_EMIT serviceOwnerChanged(value); }
    void reply(const Request &request, const QVariantMap &wire)
    {
        Q_EMIT snapshotReceived(request.token, request.owner, wire);
    }
    void changed(const QString &owner, const QString &epoch, quint64 revision)
    {
        Q_EMIT snapshotInvalidated(owner, epoch, revision);
    }
    void finish(const Operation &operation, quint64 before, quint64 after)
    {
        Q_EMIT operationFinished(
            operation.token, operation.owner,
            {{QStringLiteral("status"), QStringLiteral("applied")},
             {QStringLiteral("revisionBefore"), before},
             {QStringLiteral("revisionAfter"), after},
             {QStringLiteral("notificationId"), operation.id}});
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
        QString(64, QLatin1Char('e')));
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
    return {.lowUrgencyMilliseconds = 20,
            .normalUrgencyMilliseconds = 30,
            .criticalUrgencyMilliseconds = 40,
            .operationErrorMilliseconds = 60,
            .maximumPopups = 3,
            .maximumHistory = 4};
}

NotificationPresentation::PresentationNotification notification(
    quint32 id, QString summary, quint32 urgency = 1, bool transient = false)
{
    NotificationPresentation::PresentationNotification result;
    result.id = id;
    result.applicationName = QStringLiteral("Model Test");
    result.summary = std::move(summary);
    result.body = QStringLiteral("Plain body");
    result.urgency = urgency;
    result.transient = transient;
    result.createdAtMs = 100;
    result.actions = {{QStringLiteral("open"), QStringLiteral("Open")}};
    return result;
}

QVariantMap wire(const QString &epoch, quint64 revision,
                 QVector<NotificationPresentation::PresentationNotification> items)
{
    return NotificationPresentation::PresentationSnapshotCodec::encode(
        {epoch, revision, std::move(items)});
}

QVariant roleValue(QAbstractItemModel *model, int row,
                   NotificationPresentationModel::NotificationListModel::Role role)
{
    return model->data(model->index(row, 0), role);
}

} // namespace

class NotificationPresentationModelTests final : public QObject {
    Q_OBJECT

private slots:
    void baselinesWithoutReplayThenTracksPopupsAndHistory();
    void expiresPopupsAndOpeningCenterClearsThem();
    void suppressesCenterReplayAndBoundsPopupAndHistoryModels();
    void validatesControllerOperationsThroughTheClient();
};

void NotificationPresentationModelTests::
    baselinesWithoutReplayThenTracksPopupsAndHistory()
{
    FakeTransport transport;
    NotificationPresentationClient::NotificationPresentationClient client(
        transport, token(), clientTiming());
    NotificationPresentationPolicy::NotificationInterruptionPolicy
        interruptionPolicy;
    NotificationPresentationModel::NotificationPresentationController controller(
        client, interruptionPolicy, presentationTiming());
    QVERIFY(client.start());
    const QString owner = QStringLiteral(":1.70");
    const QString epoch = QStringLiteral("77777777-7777-7777-7777-777777777777");
    transport.owner(owner);
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 1, 100);
    transport.reply(transport.requests[0],
                    wire(epoch, 1, {notification(1, QStringLiteral("Existing"))}));
    QTRY_COMPARE_WITH_TIMEOUT(controller.activeModel()->rowCount(), 1, 100);
    QCOMPARE(controller.popupCount(), 0);

    transport.changed(owner, epoch, 2);
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 2, 100);
    transport.reply(
        transport.requests[1],
        wire(epoch, 2,
             {notification(1, QStringLiteral("Existing")),
              notification(2, QStringLiteral("Critical"), 2)}));
    QTRY_COMPARE_WITH_TIMEOUT(controller.popupCount(), 1, 100);
    QCOMPARE(roleValue(controller.popupModel(), 0,
                       NotificationPresentationModel::NotificationListModel::
                           NotificationIdRole)
                 .toUInt(),
             quint32(2));

    transport.changed(owner, epoch, 3);
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 3, 100);
    auto replaced = notification(2, QStringLiteral("Critical updated"), 2);
    replaced.updatedAtMs = 120;
    transport.reply(transport.requests[2], wire(epoch, 3, {replaced}));
    QTRY_COMPARE_WITH_TIMEOUT(controller.activeModel()->rowCount(), 1, 100);
    QCOMPARE(controller.historyModel()->rowCount(), 1);
    QCOMPARE(roleValue(controller.popupModel(), 0,
                       NotificationPresentationModel::NotificationListModel::SummaryRole)
                 .toString(),
             QStringLiteral("Critical updated"));

    transport.changed(owner, epoch, 4);
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 4, 100);
    transport.reply(transport.requests[3], wire(epoch, 4, {}));
    QTRY_COMPARE_WITH_TIMEOUT(controller.activeModel()->rowCount(), 0, 100);
    QCOMPARE(controller.historyModel()->rowCount(), 2);
    QCOMPARE(controller.popupCount(), 0);
}

void NotificationPresentationModelTests::
    expiresPopupsAndOpeningCenterClearsThem()
{
    FakeTransport transport;
    NotificationPresentationClient::NotificationPresentationClient client(
        transport, token(), clientTiming());
    NotificationPresentationPolicy::NotificationInterruptionPolicy
        interruptionPolicy;
    NotificationPresentationModel::NotificationPresentationController controller(
        client, interruptionPolicy, presentationTiming());
    QVERIFY(client.start());
    const QString owner = QStringLiteral(":1.71");
    const QString epoch = QStringLiteral("88888888-8888-8888-8888-888888888888");
    transport.owner(owner);
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 1, 100);
    transport.reply(transport.requests[0], wire(epoch, 0, {}));
    transport.changed(owner, epoch, 1);
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 2, 100);
    transport.reply(transport.requests[1],
                    wire(epoch, 1,
                         {notification(3, QStringLiteral("Expires"), 0)}));
    QTRY_COMPARE_WITH_TIMEOUT(controller.popupCount(), 1, 100);
    QTRY_COMPARE_WITH_TIMEOUT(controller.popupCount(), 0, 200);

    transport.changed(owner, epoch, 2);
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 3, 100);
    transport.reply(
        transport.requests[2],
        wire(epoch, 2, {notification(3, QStringLiteral("Replacement"), 1)}));
    QTRY_COMPARE_WITH_TIMEOUT(controller.popupCount(), 1, 100);
    controller.setCenterOpen(true);
    QVERIFY(controller.centerOpen());
    QCOMPARE(controller.popupCount(), 0);
}

void NotificationPresentationModelTests::
    suppressesCenterReplayAndBoundsPopupAndHistoryModels()
{
    FakeTransport transport;
    NotificationPresentationClient::NotificationPresentationClient client(
        transport, token(), clientTiming());
    NotificationPresentationPolicy::NotificationInterruptionPolicy
        interruptionPolicy;
    auto timing = presentationTiming();
    timing.lowUrgencyMilliseconds = 1'000;
    timing.normalUrgencyMilliseconds = 1'000;
    timing.criticalUrgencyMilliseconds = 1'000;
    NotificationPresentationModel::NotificationPresentationController controller(
        client, interruptionPolicy, timing);
    QVERIFY(client.start());
    const QString owner = QStringLiteral(":1.73");
    const QString epoch = QStringLiteral("aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa");
    transport.owner(owner);
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 1, 100);
    transport.reply(transport.requests[0], wire(epoch, 0, {}));

    controller.setCenterOpen(true);
    transport.changed(owner, epoch, 1);
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 2, 100);
    transport.reply(
        transport.requests[1],
        wire(epoch, 1,
             {notification(1, QStringLiteral("Seen in center")),
              notification(6, QStringLiteral("Transient"), 1, true)}));
    QTRY_COMPARE_WITH_TIMEOUT(controller.activeModel()->rowCount(), 2, 100);
    QCOMPARE(controller.popupCount(), 0);
    controller.setCenterOpen(false);
    QCOMPARE(controller.popupCount(), 0);

    transport.changed(owner, epoch, 2);
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 3, 100);
    transport.reply(
        transport.requests[2],
        wire(epoch, 2,
             {notification(1, QStringLiteral("Seen in center")),
              notification(2, QStringLiteral("Old low"), 0),
              notification(3, QStringLiteral("Older critical"), 2),
              notification(4, QStringLiteral("Normal"), 1),
              notification(5, QStringLiteral("Newer critical"), 2),
              notification(6, QStringLiteral("Transient"), 1, true)}));
    QTRY_COMPARE_WITH_TIMEOUT(controller.popupCount(), 3, 100);
    QCOMPARE(roleValue(controller.popupModel(), 0,
                       NotificationPresentationModel::NotificationListModel::
                           NotificationIdRole)
                 .toUInt(),
             quint32(5));
    QCOMPARE(roleValue(controller.popupModel(), 1,
                       NotificationPresentationModel::NotificationListModel::
                           NotificationIdRole)
                 .toUInt(),
             quint32(3));

    transport.changed(owner, epoch, 3);
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 4, 100);
    transport.reply(transport.requests[3], wire(epoch, 3, {}));
    QTRY_COMPARE_WITH_TIMEOUT(controller.activeModel()->rowCount(), 0, 100);
    QCOMPARE(controller.historyModel()->rowCount(), 4);
    for (int row = 0; row < controller.historyModel()->rowCount(); ++row) {
        QVERIFY(roleValue(controller.historyModel(), row,
                          NotificationPresentationModel::NotificationListModel::
                              NotificationIdRole)
                    .toUInt() != quint32(6));
    }
}

void NotificationPresentationModelTests::
    validatesControllerOperationsThroughTheClient()
{
    FakeTransport transport;
    NotificationPresentationClient::NotificationPresentationClient client(
        transport, token(), clientTiming());
    NotificationPresentationPolicy::NotificationInterruptionPolicy
        interruptionPolicy;
    auto timing = presentationTiming();
    timing.normalUrgencyMilliseconds = 120;
    timing.operationErrorMilliseconds = 40;
    NotificationPresentationModel::NotificationPresentationController controller(
        client, interruptionPolicy, timing);
    QVERIFY(client.start());
    const QString owner = QStringLiteral(":1.72");
    const QString epoch = QStringLiteral("99999999-9999-9999-9999-999999999999");
    transport.owner(owner);
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 1, 100);
    transport.reply(transport.requests[0],
                    wire(epoch, 1, {notification(4, QStringLiteral("Action"))}));
    QTRY_COMPARE_WITH_TIMEOUT(controller.activeModel()->rowCount(), 1, 100);
    transport.changed(owner, epoch, 2);
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 2, 100);
    transport.reply(
        transport.requests[1],
        wire(epoch, 2, {notification(4, QStringLiteral("Action updated"))}));
    QTRY_COMPARE_WITH_TIMEOUT(controller.popupCount(), 1, 100);

    QSignalSpy errors(&controller,
                      &NotificationPresentationModel::
                          NotificationPresentationController::operationError);
    QVERIFY(!controller.dismiss(99));
    QCOMPARE(errors.size(), 1);
    QVERIFY(!controller.operationErrorText().isEmpty());
    QVERIFY(controller.invokeAction(4, QStringLiteral("open")));
    QVERIFY(controller.operationBusy());
    QVERIFY(controller.operationErrorText().isEmpty());
    QCOMPARE(controller.popupCount(), 1);
    QCOMPARE(transport.operations.size(), 1);
    QCOMPARE(transport.operations[0].id, quint32(4));
    QCOMPARE(transport.operations[0].actionKey, QStringLiteral("open"));
    QTest::qWait(130);
    QCOMPARE(controller.popupCount(), 1);
    transport.reject(transport.operations[0], QStringLiteral("injected rejection"));
    QTRY_VERIFY_WITH_TIMEOUT(!controller.operationBusy(), 100);
    QCOMPARE(controller.popupCount(), 1);
    QCOMPARE(controller.operationErrorText(), QStringLiteral("injected rejection"));
    QCOMPARE(errors.size(), 2);

    // Rejection renews the card after the old deadline elapsed while pending,
    // and bounded feedback clears without consuming the retry path.
    QCOMPARE(controller.popupCount(), 1);
    QTRY_VERIFY_WITH_TIMEOUT(controller.operationErrorText().isEmpty(), 100);

    QVERIFY(controller.invokeAction(4, QStringLiteral("open")));
    QCOMPARE(transport.operations.size(), 2);
    transport.finish(transport.operations[1], 2, 3);
    QTRY_VERIFY_WITH_TIMEOUT(!controller.operationBusy(), 100);
    QCOMPARE(controller.popupCount(), 0);
    QVERIFY(controller.operationErrorText().isEmpty());
}

QTEST_GUILESS_MAIN(NotificationPresentationModelTests)

#include "tst_notification_presentation_model.moc"
