// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/portal/foundation_composition.h>
#include <qindaqt/services/portal/resident_portal_service.h>
#include <qindaqt/services/portal/appearance_source.h>
#include <qindaqt/services/notification_host/resident_notification_host.h>
#include <qindaqt/services/notification_host/qt_deadline_scheduler.h>
#include <qindaqt/services/notifications/notification_clock.h>
#include <qindaqt/services/notifications/notification_service.h>
#include <qindaqt/services/secret_portal/secret_portal_adaptor.h>
#include "src/session_supervisor/src/portal_session_lifetime.h"
#include <QDBusConnectionInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusObjectPath>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QUrlQuery>
#include <QtTest>
#include <signal.h>
using namespace QindaQt::Services::Portal;
using namespace QindaQt::Services::NotificationHost;
using namespace QindaQt::Services::Notifications;
class EmptyAppearance final : public AppearanceSource {
public:
    bool start(QString *) override { return true; }
    void stop() override {}
    const std::optional<AppearanceTruth> &current() const override { return empty; }
    QString diagnostic() const override { return {}; }
private: std::optional<AppearanceTruth> empty;
};
class ResponseReceiver final : public QObject {
    Q_OBJECT
public:
    int count = 0; quint32 response = 99; QString path; QVariantMap results;
public Q_SLOTS:
    void receive(quint32 value, const QVariantMap &values, const QDBusMessage &message) {
        ++count; response = value; results = values; path = message.path();
    }
};
class ActionReceiver final : public QObject {
    Q_OBJECT
public: int count = 0; QString id; QString action; QVariantList values;
public Q_SLOTS:
    void receive(const QString &notification, const QString &name, const QVariantList &parameters) {
        ++count; id = notification; action = name; values = parameters;
    }
};
class NativeFrontendTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void initTestCase() {
        QVERIFY(bus.isConnected());
        QVERIFY(bus.registerService(QStringLiteral("org.freedesktop.portal.Documents")));
        QVERIFY(bus.registerService(QStringLiteral("org.freedesktop.impl.portal.PermissionStore")));
        backend = std::make_unique<QDBusConnection>(QDBusConnection::connectToBus(qEnvironmentVariable("DBUS_SESSION_BUS_ADDRESS"), QStringLiteral("native-frontend-backend")));
        provider = std::make_unique<QDBusConnection>(QDBusConnection::connectToBus(qEnvironmentVariable("DBUS_SESSION_BUS_ADDRESS"), QStringLiteral("native-frontend-notifications")));
        const QString data = qEnvironmentVariable("XDG_DATA_HOME");
        QVERIFY(QDir().mkpath(data + QStringLiteral("/applications")));
        QFile desktop(data + QStringLiteral("/applications/org.test.Mail.desktop")); QVERIFY(desktop.open(QIODevice::WriteOnly));
        desktop.write("[Desktop Entry]\nType=Application\nName=Private Mail\nNoDisplay=true\nMimeType=x-scheme-handler/mailto;\nExec=" + qEnvironmentVariable("QINDAQT_PORTAL_TEST_MAIL").toUtf8() + " %u\n"); desktop.close();
        QFile defaults(data + QStringLiteral("/applications/mimeapps.list")); QVERIFY(defaults.open(QIODevice::WriteOnly));
        defaults.write("[Default Applications]\nx-scheme-handler/mailto=org.test.Mail.desktop;\n"); defaults.close();
        QFile app(data + QStringLiteral("/applications/org.test.PrivateApp.desktop")); QVERIFY(app.open(QIODevice::WriteOnly));
        app.write("[Desktop Entry]\nType=Application\nName=Synthetic frontend caller\nExec=/bin/true\n"); app.close();
        host = std::make_unique<ResidentNotificationHost>(*provider, clock, scheduler); QVERIFY(host->start().ok());
        resident = std::make_unique<ResidentPortalService>(appearance, *backend);
        broker = std::make_unique<QindaQt::Services::SecretPortal::QtKeyringPortalBroker>(*backend);
        secret = std::make_unique<QindaQt::Services::SecretPortal::SecretPortalAdaptor>(resident->backendHost(), *broker, *backend);
        composition = std::make_unique<PortalFoundationComposition>(resident->backendHost(), *backend,
            qEnvironmentVariable("XDG_RUNTIME_DIR"), qEnvironmentVariable("QINDAQT_PORTAL_TEST_HELPER"),
            qEnvironmentVariable("QINDAQT_PORTAL_TEST_RELAY"), QStringList{data});
        QCOMPARE(resident->start(), PortalServiceStartStatus::Started); QVERIFY(composition->start());
        session.start(QStringLiteral("/bin/true"), QStringLiteral("qindaqt-7"));
        startFrontend();
        QVERIFY(bus.connect(QStringLiteral("org.freedesktop.portal.Desktop"), {}, QStringLiteral("org.freedesktop.portal.Request"), QStringLiteral("Response"), &responses, SLOT(receive(quint32,QVariantMap,QDBusMessage))));
        QVERIFY(bus.connect(QStringLiteral("org.freedesktop.portal.Desktop"), QStringLiteral("/org/freedesktop/portal/desktop"), QStringLiteral("org.freedesktop.portal.Notification"), QStringLiteral("ActionInvoked"), &actions, SLOT(receive(QString,QString,QVariantList))));
        // Host Registry is frontend-resolved identity, not a backend option.
        auto registration = method("org.freedesktop.host.portal.Registry", "Register", {QStringLiteral("org.test.PrivateApp"), QVariantMap{}});
        QTRY_VERIFY(registration->isFinished()); const QDBusPendingReply<> registered = *registration; QVERIFY2(!registered.isError(), qPrintable(registered.error().message()));
    }
    void init() { responses.count = 0; responses.response = 99; QFile(qEnvironmentVariable("QINDAQT_PORTAL_TEST_AUDIT")).remove(); qputenv("QINDAQT_PORTAL_TEST_MODE", "grant"); }
    void cameraAccessGrantsAndDeniesThroughNativeConsent() {
        request("org.freedesktop.portal.Camera", "AccessCamera", {QVariantMap{{QStringLiteral("handle_token"), QStringLiteral("native_grant")}}});
        QTRY_COMPARE_WITH_TIMEOUT(responses.count, 1, 10000); QCOMPARE(responses.response, 0U); QVERIFY(audit().contains(" exact-peer mapped grant"));
        responses.count = 0; qputenv("QINDAQT_PORTAL_TEST_MODE", "deny");
        request("org.freedesktop.portal.Camera", "AccessCamera", {QVariantMap{{QStringLiteral("handle_token"), QStringLiteral("native_deny")}}});
        QTRY_COMPARE_WITH_TIMEOUT(responses.count, 1, 10000); QCOMPARE(responses.response, 1U); QVERIFY(audit().contains(" exact-peer mapped deny"));
    }
    void publicCloseRetiresActualMappedHelper() {
        qputenv("QINDAQT_PORTAL_TEST_MODE", "hold");
        const QString path = request("org.freedesktop.portal.Camera", "AccessCamera", {QVariantMap{{QStringLiteral("handle_token"), QStringLiteral("native_close")}}});
        QTRY_VERIFY_WITH_TIMEOUT(audit().contains(" mapped hold"), 10000); const auto pid = helperPid(); QVERIFY(pid > 1);
        auto call = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.portal.Desktop"), path, QStringLiteral("org.freedesktop.portal.Request"), QStringLiteral("Close"));
        QDBusPendingCallWatcher closed(bus.asyncCall(call)); QTRY_VERIFY(closed.isFinished()); QTRY_VERIFY(kill(pid, 0) < 0);
        QTest::qWait(100); QCOMPARE(responses.count, 0); // Standard Close withdraws without Response.
    }
    void emailUsesNativeConfiguredHandlerAndSelectedPeer() {
        QFile(qEnvironmentVariable("QINDAQT_PORTAL_TEST_MAIL_AUDIT")).remove();
        request("org.freedesktop.portal.Email", "ComposeEmail", {QString{}, QVariantMap{{QStringLiteral("addresses"), QStringList{QStringLiteral("fixture@example.invalid")}},
            {QStringLiteral("subject"), QStringLiteral("Synthetic %0D & draft")}, {QStringLiteral("body"), QStringLiteral("Private only")}}});
        QTRY_COMPARE_WITH_TIMEOUT(responses.count, 1, 10000); QCOMPARE(responses.response, 0U);
        QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(qEnvironmentVariable("QINDAQT_PORTAL_TEST_MAIL_AUDIT")), 10000);
        QFile file(qEnvironmentVariable("QINDAQT_PORTAL_TEST_MAIL_AUDIT")); QVERIFY(file.open(QIODevice::ReadOnly));
        const auto receipt = QJsonDocument::fromJson(file.readAll()).object(); QVERIFY(receipt.value(QStringLiteral("mapped")).toBool()); QVERIFY(receipt.value(QStringLiteral("exactPeer")).toBool());
        const QUrl uri(receipt.value(QStringLiteral("uri")).toString()); QCOMPARE(uri.scheme(), QStringLiteral("mailto"));
        QCOMPARE(QUrlQuery(uri).queryItemValue(QStringLiteral("subject"), QUrl::FullyDecoded), QStringLiteral("Synthetic %0D & draft"));
        QTest::qWait(150);
    }
    void notificationsForwardActionsAndRemoveThroughNativeHost() {
        auto add = method("org.freedesktop.portal.Notification", "AddNotification", {QStringLiteral("native"), QVariantMap{{QStringLiteral("title"), QStringLiteral("Synthetic native notification")},
            {QStringLiteral("buttons"), QVariant::fromValue(QList<QVariantMap>{QVariantMap{{QStringLiteral("label"), QStringLiteral("Respond")}, {QStringLiteral("action"), QStringLiteral("reply")}}})}}});
        QTRY_VERIFY(add->isFinished()); const QDBusPendingReply<> added = *add; QVERIFY2(!added.isError(), qPrintable(added.error().message()));
        QTRY_COMPARE(host->service().snapshot()->notifications.size(), 1);
        const auto id = host->service().snapshot()->notifications.first().id;
        QVERIFY(host->service().invokeAction(id, QStringLiteral("button0"), {}).ok()); QTRY_COMPARE(actions.count, 1);
        QCOMPARE(actions.id, QStringLiteral("native")); QCOMPARE(actions.action, QStringLiteral("reply"));
        auto remove = method("org.freedesktop.portal.Notification", "RemoveNotification", {QStringLiteral("native")}); QTRY_VERIFY(remove->isFinished());
        QTRY_VERIFY(host->service().snapshot()->notifications.isEmpty());
    }
    void routingRowsAreRequiredWithClosedDefault() {
        frontend.terminate(); QVERIFY(frontend.waitForFinished(5000));
        QTRY_VERIFY(!bus.interface()->isServiceRegistered(QStringLiteral("org.freedesktop.portal.Desktop")).value());
        const QString path = qEnvironmentVariable("XDG_DESKTOP_PORTAL_DIR") + QStringLiteral("/qindaqt-portals.conf");
        QFile file(path); QVERIFY(file.open(QIODevice::ReadOnly)); const QByteArray original = file.readAll(); file.close();
        QByteArray withdrawn = original;
        for (const auto *family : {"Access", "Notification", "Email"}) {
            const QByteArray row = QByteArray("org.freedesktop.impl.portal.") + family + "=qindaqt\n";
            QVERIFY(withdrawn.contains(row)); withdrawn.replace(row, QByteArray{});
        }
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate)); QCOMPARE(file.write(withdrawn), withdrawn.size()); file.close();
        startFrontend();
        auto probe = method("org.freedesktop.DBus.Introspectable", "Introspect", {});
        QTRY_VERIFY(probe->isFinished()); const QDBusPendingReply<QString> reply = *probe; QVERIFY(!reply.isError());
        for (const auto *family : {"Camera", "Notification", "Email"})
            QVERIFY2(!reply.value().contains(QStringLiteral("org.freedesktop.portal.") + QString::fromLatin1(family)), family);
        QVERIFY(audit().isEmpty());
        frontend.terminate(); QVERIFY(frontend.waitForFinished(5000));
        QTRY_VERIFY(!bus.interface()->isServiceRegistered(QStringLiteral("org.freedesktop.portal.Desktop")).value());
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate)); QCOMPARE(file.write(original), original.size()); file.close();
        startFrontend();
        auto registration = method("org.freedesktop.host.portal.Registry", "Register", {QStringLiteral("org.test.PrivateApp"), QVariantMap{}}); QTRY_VERIFY(registration->isFinished());
        const QDBusPendingReply<> registered = *registration; QVERIFY(!registered.isError());
    }
    void frontendOwnerLossRetiresMappedHelper() {
        qputenv("QINDAQT_PORTAL_TEST_MODE", "hold"); request("org.freedesktop.portal.Camera", "AccessCamera", {QVariantMap{}});
        QTRY_VERIFY_WITH_TIMEOUT(audit().contains(" mapped hold"), 10000); const auto pid = helperPid(); frontend.terminate(); QVERIFY(frontend.waitForFinished(5000));
        QTRY_VERIFY(kill(pid, 0) < 0); QTest::qWait(100); QCOMPARE(responses.count, 0);
        startFrontend();
        auto registration = method("org.freedesktop.host.portal.Registry", "Register", {QStringLiteral("org.test.PrivateApp"), QVariantMap{}}); QTRY_VERIFY(registration->isFinished());
    }
    void supervisorOwnerLossRefusesPendingAndLaterDisclosure() {
        qputenv("QINDAQT_PORTAL_TEST_MODE", "hold"); request("org.freedesktop.portal.Camera", "AccessCamera", {QVariantMap{}});
        QTRY_VERIFY_WITH_TIMEOUT(audit().contains(" mapped hold"), 10000); const auto pid = helperPid(); session.stop();
        QTRY_VERIFY(kill(pid, 0) < 0); QTRY_COMPARE_WITH_TIMEOUT(responses.count, 1, 10000); QVERIFY(responses.response != 0);
        QFile(qEnvironmentVariable("QINDAQT_PORTAL_TEST_AUDIT")).remove(); responses.count = 0;
        request("org.freedesktop.portal.Camera", "AccessCamera", {QVariantMap{}}); QTRY_COMPARE_WITH_TIMEOUT(responses.count, 1, 10000); QVERIFY(responses.response != 0); QVERIFY(audit().isEmpty());
    }
    void cleanupTestCase() {
        frontend.terminate(); if (!frontend.waitForFinished(3000)) { frontend.kill(); frontend.waitForFinished(3000); }
        session.stop(); composition.reset(); secret.reset(); broker.reset(); resident.reset(); host.reset();
        backend.reset(); provider.reset(); QDBusConnection::disconnectFromBus(QStringLiteral("native-frontend-backend")); QDBusConnection::disconnectFromBus(QStringLiteral("native-frontend-notifications"));
    }
private:
    void startFrontend() {
        frontend.start(QStringLiteral(QINDAQT_FRONTEND_EXECUTABLE), {QStringLiteral("--verbose")}); QVERIFY(frontend.waitForStarted());
        QTRY_VERIFY_WITH_TIMEOUT(bus.interface()->isServiceRegistered(QStringLiteral("org.freedesktop.portal.Desktop")).value(), 10000);
    }
    std::unique_ptr<QDBusPendingCallWatcher> method(const char *interface, const char *member, const QVariantList &arguments) {
        auto call = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.portal.Desktop"), QStringLiteral("/org/freedesktop/portal/desktop"), QString::fromLatin1(interface), QString::fromLatin1(member));
        call.setArguments(arguments); return std::make_unique<QDBusPendingCallWatcher>(bus.asyncCall(call, 15000));
    }
    QString request(const char *interface, const char *member, const QVariantList &arguments) {
        auto reply = method(interface, member, arguments);
        QElapsedTimer timer; timer.start(); while (!reply->isFinished() && timer.elapsed() < 5000) { QCoreApplication::processEvents(); QTest::qWait(5); }
        const QDBusPendingReply<QDBusObjectPath> result = *reply;
        if (result.isError()) { qWarning().noquote() << result.error().message(); return {}; }
        return result.value().path();
    }
    QByteArray audit() { QFile file(qEnvironmentVariable("QINDAQT_PORTAL_TEST_AUDIT")); return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{}; }
    pid_t helperPid() { return static_cast<pid_t>(audit().split(' ').value(0).toInt()); }
    QDBusConnection bus = QDBusConnection::sessionBus(); QProcess frontend;
    EmptyAppearance appearance; ResponseReceiver responses; ActionReceiver actions;
    SteadyNotificationClock clock; QtNotificationDeadlineScheduler scheduler;
    QindaQt::SessionSupervisor::PortalSessionLifetime session;
    std::unique_ptr<QDBusConnection> backend, provider;
    std::unique_ptr<ResidentNotificationHost> host;
    std::unique_ptr<ResidentPortalService> resident;
    std::unique_ptr<PortalFoundationComposition> composition;
    std::unique_ptr<QindaQt::Services::SecretPortal::SecretPortalAdaptor> secret;
    std::unique_ptr<QindaQt::Services::SecretPortal::QtKeyringPortalBroker> broker;
};
QTEST_GUILESS_MAIN(NativeFrontendTest)
#include "tst_native_frontend.moc"
