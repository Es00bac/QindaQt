// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/notification_presentation/presentation_snapshot.h"
#include "qindaqt/services/notification_presentation_client/notification_presentation_client.h"
#include "qindaqt/services/notification_presentation_client/presentation_transport.h"
#include "qindaqt/services/notification_presentation_model/notification_list_model.h"
#include "qindaqt/services/notification_presentation_model/notification_presentation_controller.h"
#include "qindaqt/services/notification_presentation_policy/notification_application_policy.h"
#include "qindaqt/services/notification_presentation_policy/notification_interruption_policy.h"
#include "qindaqt/services/notification_presentation_policy/notification_privacy_policy.h"

#include <QSignalSpy>
#include <QtTest>

#include <utility>

using namespace QindaQt::Services;

namespace {

struct Request final {
    quint64 token = 0;
    QString owner;
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
    void dismiss(quint64, const QString &, quint32) override {}
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

    QVector<Request> requests;
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

NotificationPresentation::PresentationNotification notification(
    quint32 id, QString applicationId, QString summary, quint32 urgency)
{
    NotificationPresentation::PresentationNotification result;
    result.id = id;
    result.applicationName = applicationId;
    result.desktopEntry = std::move(applicationId);
    result.summary = std::move(summary);
    result.body = QStringLiteral("Plain body");
    result.urgency = urgency;
    result.createdAtMs = qint64(id) * 10;
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

NotificationPresentationModel::PresentationTiming timing()
{
    return {.lowUrgencyMilliseconds = 5'000,
            .normalUrgencyMilliseconds = 5'000,
            .criticalUrgencyMilliseconds = 5'000,
            .operationErrorMilliseconds = 100,
            .maximumPopups = 8,
            .maximumHistory = 10};
}

} // namespace

class NotificationPresentationApplicationPolicyTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void muteSuppressesOnlyItsPopupAndSoundWithoutReplay();
    void dndCriticalBypassAndLockPrivacyKeepTheirOwnAuthority();
};

void NotificationPresentationApplicationPolicyTest::
    muteSuppressesOnlyItsPopupAndSoundWithoutReplay()
{
    FakeTransport transport;
    NotificationPresentationClient::NotificationPresentationClient client(
        transport, token(), clientTiming());
    NotificationPresentationPolicy::NotificationInterruptionPolicy interruption;
    NotificationPresentationPolicy::NotificationApplicationPolicy application;
    NotificationPresentationPolicy::NotificationPrivacyPolicy privacy;
    privacy.setPrivatePresentationAllowed(true);
    NotificationPresentationModel::NotificationPresentationController controller(
        client, interruption, application, privacy, timing());
    QSignalSpy soundRequests(
        &controller,
        &NotificationPresentationModel::NotificationPresentationController::
            notificationSoundRequested);
    QVERIFY(client.start());
    const QString owner = QStringLiteral(":1.81");
    const QString epoch = QStringLiteral("11111111-1111-4111-8111-111111111111");
    transport.owner(owner);
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 1, 100);
    transport.reply(transport.requests.last(), wire(epoch, 0, {}));

    QVERIFY(application.setPolicies({
        {QStringLiteral("org.example.Muted"), {.muted = true, .soundEnabled = true}},
        {QStringLiteral("org.example.Permitted"), {.muted = false, .soundEnabled = true}},
    }));
    transport.changed(owner, epoch, 1);
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 2, 100);
    transport.reply(transport.requests.last(),
                    wire(epoch, 1,
                         {notification(1, QStringLiteral("org.example.Muted"),
                                       QStringLiteral("Muted app"), 1),
                          notification(2, QStringLiteral("org.example.Permitted"),
                                       QStringLiteral("Permitted app"), 1)}));
    QTRY_COMPARE_WITH_TIMEOUT(controller.activeModel()->rowCount(), 2, 100);
    QCOMPARE(controller.popupCount(), 1);
    QCOMPARE(notificationIdAt(controller.popupModel(), 0).toUInt(), quint32(2));
    QCOMPARE(soundRequests.size(), 1);
    QCOMPARE(soundRequests.at(0).at(0).toUInt(), quint32(2));

    QVERIFY(application.setPolicies({
        {QStringLiteral("org.example.Permitted"), {.muted = false, .soundEnabled = true}},
        {QStringLiteral("org.example.Muted"), {.muted = false, .soundEnabled = true}},
    }));
    QCOMPARE(controller.popupCount(), 1);
    QCOMPARE(soundRequests.size(), 1);

    transport.changed(owner, epoch, 2);
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 3, 100);
    transport.reply(transport.requests.last(),
                    wire(epoch, 2,
                         {notification(1, QStringLiteral("org.example.Muted"),
                                       QStringLiteral("Muted app"), 1),
                          notification(2, QStringLiteral("org.example.Permitted"),
                                       QStringLiteral("Permitted app"), 1),
                          notification(4, QStringLiteral("org.example.Muted"),
                                       QStringLiteral("Newly permitted"), 1)}));
    QTRY_COMPARE_WITH_TIMEOUT(controller.popupCount(), 2, 100);
    QCOMPARE(soundRequests.size(), 2);
    QCOMPARE(soundRequests.at(1).at(0).toUInt(), quint32(4));
}

void NotificationPresentationApplicationPolicyTest::
    dndCriticalBypassAndLockPrivacyKeepTheirOwnAuthority()
{
    FakeTransport transport;
    NotificationPresentationClient::NotificationPresentationClient client(
        transport, token(), clientTiming());
    NotificationPresentationPolicy::NotificationInterruptionPolicy interruption;
    NotificationPresentationPolicy::NotificationApplicationPolicy application;
    NotificationPresentationPolicy::NotificationPrivacyPolicy privacy;
    privacy.setPrivatePresentationAllowed(true);
    NotificationPresentationModel::NotificationPresentationController controller(
        client, interruption, application, privacy, timing());
    QSignalSpy soundRequests(
        &controller,
        &NotificationPresentationModel::NotificationPresentationController::
            notificationSoundRequested);
    QVERIFY(client.start());
    const QString owner = QStringLiteral(":1.82");
    const QString epoch = QStringLiteral("22222222-2222-4222-8222-222222222222");
    transport.owner(owner);
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 1, 100);
    transport.reply(transport.requests.last(), wire(epoch, 0, {}));

    QVERIFY(application.setPolicies({
        {QStringLiteral("org.example.Muted"), {.muted = true, .soundEnabled = true}},
        {QStringLiteral("org.example.Permitted"), {.muted = false, .soundEnabled = true}},
    }));
    interruption.setDoNotDisturbEnabled(true);
    transport.changed(owner, epoch, 1);
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 2, 100);
    transport.reply(transport.requests.last(),
                    wire(epoch, 1,
                         {notification(10, QStringLiteral("org.example.Permitted"),
                                       QStringLiteral("DND suppressed"), 1),
                          notification(11, QStringLiteral("org.example.Muted"),
                                       QStringLiteral("Muted critical"), 2),
                          notification(12, QStringLiteral("org.example.Permitted"),
                                       QStringLiteral("Critical allowed"), 2)}));
    QTRY_COMPARE_WITH_TIMEOUT(controller.activeModel()->rowCount(), 3, 100);
    QCOMPARE(controller.popupCount(), 1);
    QCOMPARE(notificationIdAt(controller.popupModel(), 0).toUInt(), quint32(12));
    QCOMPARE(soundRequests.size(), 1);
    QCOMPARE(soundRequests.at(0).at(0).toUInt(), quint32(12));

    privacy.setPrivatePresentationAllowed(false);
    transport.changed(owner, epoch, 2);
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 3, 100);
    transport.reply(transport.requests.last(),
                    wire(epoch, 2,
                         {notification(10, QStringLiteral("org.example.Permitted"),
                                       QStringLiteral("DND suppressed"), 1),
                          notification(11, QStringLiteral("org.example.Muted"),
                                       QStringLiteral("Muted critical"), 2),
                          notification(12, QStringLiteral("org.example.Permitted"),
                                       QStringLiteral("Critical allowed"), 2),
                          notification(13, QStringLiteral("org.example.Permitted"),
                                       QStringLiteral("Private critical"), 2)}));
    QTRY_COMPARE_WITH_TIMEOUT(controller.activeModel()->rowCount(), 0, 100);
    QCOMPARE(controller.popupCount(), 0);
    QCOMPARE(soundRequests.size(), 1);
}

QTEST_MAIN(NotificationPresentationApplicationPolicyTest)

#include "tst_notification_presentation_app_policy.moc"
