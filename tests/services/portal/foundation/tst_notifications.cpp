// SPDX-License-Identifier: GPL-3.0-or-later
#include "support/private_bus.h"
#include <qindaqt/services/portal/notification_adaptor.h>
#include <qindaqt/services/notification_host/resident_notification_host.h>
#include <qindaqt/services/notification_host/qt_deadline_scheduler.h>
#include <qindaqt/services/notifications/notification_clock.h>
#include <qindaqt/services/notifications/notification_service.h>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QImage>
#include <QBuffer>
#include <QtTest>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
using namespace QindaQt::Services::Portal;
using namespace QindaQt::Services::NotificationHost;
using namespace QindaQt::Services::Notifications;
class ActionReceiver final : public QObject {
    Q_OBJECT
public Q_SLOTS:
    void receive(const QString &app, const QString &id, const QString &action, const QVariantList &parameters) {
        appId = app; notificationId = id; actionName = action; values = parameters; ++count;
    }
public:
    int count = 0; QString appId, notificationId, actionName; QVariantList values;
};
class HeldNotifications final : public NativeNotifications {
public:
    QList<quint64> calls;
    void notify(quint64 token, quint32, const PortalNotification &) override { calls.append(token); }
    void close(quint64 token, quint32) override { Q_EMIT removed(token, true); }
};
class NotificationTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void init() {
        fixture = std::make_unique<PortalPrivateBus>(); QVERIFY(fixture->start());
        provider = fixture->connect(); backend = fixture->connect(); frontend = fixture->connect();
        QVERIFY(frontend->registerService(QStringLiteral("org.freedesktop.portal.Desktop")));
        clock = std::make_unique<SteadyNotificationClock>(); scheduler = std::make_unique<QtNotificationDeadlineScheduler>();
        host = std::make_unique<ResidentNotificationHost>(*provider, *clock, *scheduler); QVERIFY(host->start().ok());
        registry = std::make_unique<RequestRegistry>(*backend); native = std::make_unique<QtNativeNotifications>(*backend);
        object = std::make_unique<QObject>(); new NotificationAdaptor(*object, *registry, *native, *backend);
        QVERIFY(backend->registerObject(QStringLiteral("/org/freedesktop/portal/desktop"), object.get(), QDBusConnection::ExportAdaptors));
        QVERIFY(backend->registerService(QStringLiteral("org.test.Portal")));
        QVERIFY(frontend->connect({}, QStringLiteral("/org/freedesktop/portal/desktop"), QStringLiteral("org.freedesktop.impl.portal.Notification"),
            QStringLiteral("ActionInvoked"), &receiver, SLOT(receive(QString,QString,QString,QVariantList))));
    }
    void cleanup() {
        object.reset(); QTest::qWait(30); native.reset(); held.reset(); registry.reset(); host.reset(); scheduler.reset(); clock.reset();
        provider.reset(); backend.reset(); frontend.reset(); fixture.reset(); receiver.count = 0; receiver.values.clear();
    }
    void nativeForwardReplaceIsolationRemove() {
        auto first = add(QStringLiteral("org.test.One"), QStringLiteral("same")); wait(*first);
        QCOMPARE(host->service().snapshot()->notifications.size(), 1);
        const auto firstId = host->service().snapshot()->notifications.first().id;
        auto second = add(QStringLiteral("org.test.Two"), QStringLiteral("same")); wait(*second);
        QCOMPARE(host->service().snapshot()->notifications.size(), 2);
        auto replace = add(QStringLiteral("org.test.One"), QStringLiteral("same"), {{QStringLiteral("title"), QStringLiteral("Updated")}}); wait(*replace);
        QCOMPARE(host->service().snapshot()->notifications.size(), 2);
        bool updated = false; for (const auto &entry : host->service().snapshot()->notifications) if (entry.id == firstId) updated = entry.summary == QStringLiteral("Updated");
        QVERIFY(updated);
        auto remove = call(QStringLiteral("RemoveNotification"), {QStringLiteral("org.test.One"), QStringLiteral("same")}); wait(*remove);
        QCOMPARE(host->service().snapshot()->notifications.size(), 1);
        QCOMPARE(host->service().snapshot()->notifications.first().applicationName, QStringLiteral("org.test.Two"));
    }
    void pendingAdditionsReserveApplicationCapacity() {
        object.reset(); held = std::make_unique<HeldNotifications>(); object = std::make_unique<QObject>();
        new NotificationAdaptor(*object, *registry, *held, *backend);
        backend->unregisterObject(QStringLiteral("/org/freedesktop/portal/desktop"));
        QVERIFY(backend->registerObject(QStringLiteral("/org/freedesktop/portal/desktop"), object.get(), QDBusConnection::ExportAdaptors));
        QList<std::shared_ptr<QDBusPendingCallWatcher>> replies;
        for (int i = 0; i < 9; ++i) replies.append(std::shared_ptr<QDBusPendingCallWatcher>(add(QStringLiteral("org.test.One"), QString::number(i)).release()));
        QTRY_COMPARE(held->calls.size(), 8);
        QTRY_VERIFY(replies.last()->isFinished()); const QDBusPendingReply<> refused = *replies.last(); QVERIFY(refused.isError());
        for (int i = 0; i < 8; ++i) { Q_EMIT held->notified(held->calls[i], true, static_cast<quint32>(i + 1)); wait(*replies[i]); }
    }
    void nativeActionsWithTargetAndActivation() {
        QList<QVariantMap> buttons{{{QStringLiteral("label"), QStringLiteral("Respond")}, {QStringLiteral("action"), QStringLiteral("reply")}, {QStringLiteral("target"), QStringLiteral("private-target")}}};
        auto reply = add(QStringLiteral("org.test.One"), QStringLiteral("action"), {{QStringLiteral("buttons"), QVariant::fromValue(buttons)}}); wait(*reply);
        const auto id = host->service().snapshot()->notifications.first().id;
        QVERIFY(host->service().invokeAction(id, QStringLiteral("button0"), QStringLiteral("synthetic-activation")).ok());
        QTRY_COMPARE(receiver.count, 1); QCOMPARE(receiver.appId, QStringLiteral("org.test.One"));
        QCOMPARE(receiver.notificationId, QStringLiteral("action")); QCOMPARE(receiver.actionName, QStringLiteral("reply"));
        QCOMPARE(receiver.values.size(), 2);
        QCOMPARE(qvariant_cast<QDBusVariant>(receiver.values.first()).variant().toString(), QStringLiteral("private-target"));
        const auto platform = qdbus_cast<QVariantMap>(qvariant_cast<QDBusVariant>(receiver.values.last()).variant());
        QCOMPARE(platform.value(QStringLiteral("activation-token")).toString(), QStringLiteral("synthetic-activation"));
    }
    void frontendLossClearsNativeNotifications() {
        auto reply = add(QStringLiteral("org.test.One"), QStringLiteral("owned")); wait(*reply);
        QVERIFY(frontend->unregisterService(QStringLiteral("org.freedesktop.portal.Desktop")));
        QTRY_VERIFY(host->service().snapshot()->notifications.isEmpty());
    }
    void providerLossFailsExplicitly() {
        host->stop(); QTest::qWait(30);
        auto reply = add(QStringLiteral("org.test.One"), QStringLiteral("unavailable"));
        QTRY_VERIFY(reply->isFinished()); const QDBusPendingReply<> result = *reply; QVERIFY(result.isError());
    }
    void sealedFdIconAndClosedFd() {
        QImage image(3, 2, QImage::Format_RGBA8888); image.fill(Qt::red);
        QByteArray bytes; QBuffer buffer(&bytes); QVERIFY(buffer.open(QIODevice::WriteOnly)); QVERIFY(image.save(&buffer, "PNG"));
        const int fd = memfd_create("portal-synthetic-icon", MFD_CLOEXEC | MFD_ALLOW_SEALING); QVERIFY(fd >= 0);
        QCOMPARE(write(fd, bytes.constData(), static_cast<size_t>(bytes.size())), bytes.size());
        QVERIFY(fcntl(fd, F_ADD_SEALS, F_SEAL_WRITE | F_SEAL_GROW | F_SEAL_SHRINK) == 0);
        QDBusUnixFileDescriptor descriptor(fd); close(fd);
        SerializedIcon icon{QStringLiteral("file-descriptor"), QDBusVariant(QVariant::fromValue(descriptor))};
        auto reply = add(QStringLiteral("org.test.One"), QStringLiteral("icon"), {{QStringLiteral("icon"), QVariant::fromValue(icon)}}); wait(*reply);
        const auto snapshot = host->service().snapshot(); QVERIFY(snapshot->notifications.first().hints.image.has_value());
        QCOMPARE(snapshot->notifications.first().hints.image->width, 3);
        QVariantMap hints; QVERIFY(!decodeNotificationIcon(QDBusUnixFileDescriptor{}, &hints));
    }
    void policyMalformedAndOpaqueBytesRefused() {
        QVERIFY(!portalNotification({}, {}, {}));
        QVERIFY(!portalNotification({}, QStringLiteral("id"), {{QStringLiteral("priority"), QStringLiteral("unknown")}}));
        SerializedIcon bytes{QStringLiteral("bytes"), QDBusVariant(QByteArray("synthetic"))};
        QVERIFY(!portalNotification({}, QStringLiteral("id"), {{QStringLiteral("icon"), QVariant::fromValue(bytes)}}));
        QVERIFY(portalNotification({}, QStringLiteral("host"), {}));
    }
private:
    void wait(QDBusPendingCallWatcher &reply) {
        QTRY_VERIFY(reply.isFinished()); const QDBusPendingReply<> result = reply; QVERIFY2(!result.isError(), qPrintable(result.error().name()));
    }
    std::unique_ptr<QDBusPendingCallWatcher> call(const QString &method, const QVariantList &arguments) {
        auto message = QDBusMessage::createMethodCall(QStringLiteral("org.test.Portal"), QStringLiteral("/org/freedesktop/portal/desktop"),
            QStringLiteral("org.freedesktop.impl.portal.Notification"), method); message.setArguments(arguments);
        return std::make_unique<QDBusPendingCallWatcher>(frontend->asyncCall(message));
    }
    std::unique_ptr<QDBusPendingCallWatcher> add(const QString &app, const QString &id, const QVariantMap &options = {}) {
        QVariantMap values{{QStringLiteral("title"), QStringLiteral("Private fixture")}, {QStringLiteral("body"), QStringLiteral("Synthetic notification")}};
        for (auto it = options.cbegin(); it != options.cend(); ++it) values.insert(it.key(), it.value());
        return call(QStringLiteral("AddNotification"), {app, id, values});
    }
    std::unique_ptr<PortalPrivateBus> fixture;
    std::unique_ptr<QDBusConnection> provider, backend, frontend;
    std::unique_ptr<SteadyNotificationClock> clock;
    std::unique_ptr<QtNotificationDeadlineScheduler> scheduler;
    std::unique_ptr<ResidentNotificationHost> host;
    std::unique_ptr<RequestRegistry> registry;
    std::unique_ptr<QtNativeNotifications> native;
    std::unique_ptr<HeldNotifications> held;
    std::unique_ptr<QObject> object; ActionReceiver receiver;
};
QTEST_GUILESS_MAIN(NotificationTest)
#include "tst_notifications.moc"
