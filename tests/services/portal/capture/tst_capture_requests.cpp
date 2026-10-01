// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/portal/screenshot_adaptor.h>
#include <qindaqt/services/portal/screencast_adaptor.h>
#include "../foundation/support/private_bus.h"
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QtTest>
using namespace QindaQt::Services::Portal;
class UI final : public CaptureUI {
public:
    bool admitted() const override { return allowed; }
    void request(RequestToken value, const CaptureRequest &r) override { token = value; current = r; ++opens; }
    void cancel(RequestToken value) override { if (token == value) { token = 0; ++cancels; } }
    void stop(const QString &path) override { stopped.append(path); }
    void revoke() override { allowed = false; Q_EMIT authorityLost(); }
    RequestToken token = 0; CaptureRequest current{}; bool allowed = true; int opens = 0, cancels = 0; QStringList stopped;
};
class CaptureRequestsTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void init() {
        fixture = std::make_unique<PortalPrivateBus>(); QVERIFY(fixture->start()); backend = fixture->connect(); frontend = fixture->connect(); caller = fixture->connect(); stranger = fixture->connect();
        QVERIFY(frontend->registerService("org.freedesktop.portal.Desktop")); registry = std::make_unique<RequestRegistry>(*backend); ui = std::make_unique<UI>(); host = std::make_unique<QObject>();
        new ScreenshotAdaptor(*host, *registry, *ui); new ScreenCastAdaptor(*host, *registry, *ui, *backend);
        QVERIFY(backend->registerObject("/org/freedesktop/portal/desktop", host.get(), QDBusConnection::ExportAdaptors)); QVERIFY(backend->registerService("org.test.Capture"));
        const auto component = caller->baseService().mid(1).replace('.', '_');
        path = "/org/freedesktop/portal/desktop/request/" + component + "/test"; session = "/org/freedesktop/portal/desktop/session/" + component + "/test";
    }
    void cleanup() { host.reset(); registry.reset(); ui.reset(); backend.reset(); frontend.reset(); caller.reset(); stranger.reset(); fixture.reset(); }
    void screenshotColorShapeCloseAndUnauthorized() {
        auto denied = call("Screenshot", "Screenshot", {object(path), "org.test.Caller", QString{}, QVariantMap{}}, *stranger); QTRY_VERIFY(denied->isFinished()); const QDBusPendingReply<quint32, QVariantMap> unauthorized = *denied; QVERIFY(unauthorized.isError()); QCOMPARE(ui->opens, 0);
        auto pending = call("Screenshot", "Screenshot", {object(path), "org.test.Caller", QString{}, QVariantMap{}}); QTRY_COMPARE(ui->opens, 1);
        const auto retired = ui->token; auto close = closeRequest(); QTRY_VERIFY(close->isFinished()); QTRY_VERIFY(pending->isFinished()); QCOMPARE(response(*pending), 1U);
        Q_EMIT ui->completed(retired, RequestResponse::Success, {{"uri", "file:///private/late.png"}}); QVERIFY(!registry->live(retired));
        pending = call("Screenshot", "PickColor", {object(path), "org.test.Caller", QString{}, QVariantMap{}}); QTRY_COMPARE(ui->opens, 2);
        Q_EMIT ui->completed(ui->token, RequestResponse::Success, {{"color", QVariant::fromValue(CaptureColor{2, 0, 0})}}); QTRY_VERIFY(pending->isFinished()); QCOMPARE(response(*pending), 2U);
        pending = call("Screenshot", "PickColor", {object(path), "org.test.Caller", QString{}, QVariantMap{}}); QTRY_COMPARE(ui->opens, 3);
        Q_EMIT ui->completed(ui->token, RequestResponse::Success, {{"color", QVariant::fromValue(CaptureColor{.2, .3, .4})}}); QTRY_VERIFY(pending->isFinished()); QCOMPARE(response(*pending), 0U);
    }
    void sessionActualCallerHandleFenceAndUnsupportedSelection() {
        create();
        auto wrong = call("ScreenCast", "SelectSources", {object(path), object(session), "org.test.Other", QVariantMap{}}); QTRY_VERIFY(wrong->isFinished()); QCOMPARE(response(*wrong), 2U); QVERIFY(ui->stopped.isEmpty());
        const QString otherPath = "/org/freedesktop/portal/desktop/request/" + stranger->baseService().mid(1).replace('.', '_') + "/test";
        wrong = call("ScreenCast", "SelectSources", {object(otherPath), object(session), "org.test.Caller", QVariantMap{}}); QTRY_VERIFY(wrong->isFinished()); QCOMPARE(response(*wrong), 2U); QVERIFY(ui->stopped.isEmpty());
        auto selection = call("ScreenCast", "SelectSources", {object(path), object(session), "org.test.Caller", QVariantMap{{"multiple", true}}}); QTRY_VERIFY(selection->isFinished()); QCOMPARE(response(*selection), 2U); QTRY_VERIFY(ui->stopped.contains(session));
        auto gone = call("ScreenCast", "Start", {object(path), object(session), "org.test.Caller", QString{}, QVariantMap{}}); QTRY_VERIFY(gone->isFinished()); QCOMPARE(response(*gone), 2U); QCOMPARE(ui->opens, 0);
    }
    void actualStreamShapeCloseLateAndAuthorityLoss() {
        create(); select(); auto start = sharing(); QTRY_COMPARE(ui->opens, 1); const auto token = ui->token;
        const auto results = captureResults(CaptureKind::Stream, {{"node", 31}, {"x", 0}, {"y", 0}, {"width", 800}, {"height", 600}, {"name", "Monitor"}}, {}); QVERIFY(results);
        Q_EMIT ui->completed(token, RequestResponse::Success, *results); QTRY_VERIFY(start->isFinished()); QCOMPARE(response(*start), 0U); QVERIFY(ui->stopped.isEmpty());
        auto close = QDBusMessage::createMethodCall("org.test.Capture", session, "org.freedesktop.impl.portal.Session", "Close"); QDBusPendingCallWatcher stopped(frontend->asyncCall(close)); QTRY_VERIFY(stopped.isFinished()); QTRY_VERIFY(ui->stopped.contains(session));
        Q_EMIT ui->completed(token, RequestResponse::Success, *results); QVERIFY(!registry->live(token));
        ui->stopped.clear(); create(); select(); start = sharing(); QTRY_COMPARE(ui->opens, 2); ui->revoke(); QTRY_VERIFY(start->isFinished()); QCOMPARE(response(*start), 2U); QTRY_VERIFY(ui->stopped.contains(session));
    }
    void callerAndFrontendLossWithdrawSessions() {
        create(); select(); auto start = sharing(); QTRY_COMPARE(ui->opens, 1); const auto token = ui->token;
        const auto connection = caller->name(); caller.reset(); QDBusConnection::disconnectFromBus(connection); QTRY_VERIFY(ui->stopped.contains(session)); QTRY_VERIFY(start->isFinished()); QCOMPARE(response(*start), 2U); QVERIFY(!registry->live(token));
        caller = fixture->connect(); const auto component = caller->baseService().mid(1).replace('.', '_'); path = "/org/freedesktop/portal/desktop/request/"+component+"/test"; session = "/org/freedesktop/portal/desktop/session/"+component+"/test";
        ui->stopped.clear(); create(); QVERIFY(frontend->unregisterService("org.freedesktop.portal.Desktop")); QTRY_VERIFY(ui->stopped.contains(session));
    }
private:
    static QVariant object(const QString &path) { return QVariant::fromValue(QDBusObjectPath(path)); }
    static quint32 response(const QDBusPendingCallWatcher &watcher) { const QDBusPendingReply<quint32, QVariantMap> reply = watcher; return reply.isError() ? 99U : reply.argumentAt<0>(); }
    std::unique_ptr<QDBusPendingCallWatcher> call(const char *family, const char *method, QVariantList args, const QDBusConnection &connection) {
        auto message = QDBusMessage::createMethodCall("org.test.Capture", "/org/freedesktop/portal/desktop", "org.freedesktop.impl.portal."+QString::fromLatin1(family), QString::fromLatin1(method)); message.setArguments(args); return std::make_unique<QDBusPendingCallWatcher>(connection.asyncCall(message));
    }
    std::unique_ptr<QDBusPendingCallWatcher> call(const char *family, const char *method, QVariantList args) { return call(family, method, args, *frontend); }
    std::unique_ptr<QDBusPendingCallWatcher> closeRequest() { auto message = QDBusMessage::createMethodCall("org.test.Capture", path, "org.freedesktop.impl.portal.Request", "Close"); return std::make_unique<QDBusPendingCallWatcher>(frontend->asyncCall(message)); }
    void create() { auto reply = call("ScreenCast", "CreateSession", {object(path), object(session), "org.test.Caller", QVariantMap{}}); QTRY_VERIFY(reply->isFinished()); QCOMPARE(response(*reply), 0U); }
    void select() { auto reply = call("ScreenCast", "SelectSources", {object(path), object(session), "org.test.Caller", QVariantMap{{"types", 1U}, {"cursor_mode", 1U}}}); QTRY_VERIFY(reply->isFinished()); QCOMPARE(response(*reply), 0U); }
    std::unique_ptr<QDBusPendingCallWatcher> sharing() { return call("ScreenCast", "Start", {object(path), object(session), "org.test.Caller", QString{}, QVariantMap{}}); }
    QString path, session; std::unique_ptr<PortalPrivateBus> fixture; std::unique_ptr<QDBusConnection> backend, frontend, caller, stranger;
    std::unique_ptr<RequestRegistry> registry; std::unique_ptr<UI> ui; std::unique_ptr<QObject> host;
};
QTEST_GUILESS_MAIN(CaptureRequestsTest)
#include "tst_capture_requests.moc"
