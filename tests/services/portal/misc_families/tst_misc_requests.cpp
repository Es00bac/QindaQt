// SPDX-License-Identifier: GPL-3.0-or-later
#include "account_adaptor.h"
#include "usb_adaptor.h"
#include "launcher_adaptor.h"
#include "print_adaptor.h"
#include "../foundation/support/private_bus.h"
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QTemporaryFile>
#include <QtTest>
using namespace QindaQt::Services::Portal;
class Ui final : public MiscUi {
public:
    bool admitted() const override { return allowed; }
    void present(RequestToken value, const QJsonObject &request, int fd = -1) override { token = value; frame = request; printFd = fd; ++opens; }
    void cancel(RequestToken value) override { if (value == token) { token = 0; ++cancels; } }
    RequestToken token = 0; QJsonObject frame; int opens = 0, cancels = 0, printFd = -1; bool allowed = true;
};
class MiscRequestsTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void init() {
        fixture = std::make_unique<PortalPrivateBus>(); QVERIFY(fixture->start()); backend = fixture->connect(); frontend = fixture->connect(); stranger = fixture->connect();
        QVERIFY(frontend->registerService("org.freedesktop.portal.Desktop")); registry = std::make_unique<RequestRegistry>(*backend); ui = std::make_unique<Ui>(); host = std::make_unique<QObject>();
        new AccountAdaptor(*host, *registry, *ui, [] { return AccountInformation{"fixture-id", "Fixture Person", "file:///fixture/avatar.png", "file:///fixture/generic-avatar.png"}; });
        new UsbAdaptor(*host, *registry, *ui); new LauncherAdaptor(*host, *registry, *ui); new PrintAdaptor(*host, *registry, *ui);
        QVERIFY(backend->registerObject("/org/freedesktop/portal/desktop", host.get(), QDBusConnection::ExportAdaptors)); QVERIFY(backend->registerService("org.test.Misc"));
    }
    void cleanup() { host.reset(); registry.reset(); ui.reset(); backend.reset(); frontend.reset(); stranger.reset(); fixture.reset(); }
    void accountConsentOnlySharesSelectedAuthenticData() {
        auto pending = account(); QTRY_COMPARE(ui->opens, 1);
        QCOMPARE(ui->frame.value("id").toString(), QStringLiteral("fixture-id"));
        Q_EMIT ui->completed(ui->token, RequestResponse::Success, QJsonObject{{"id", true}, {"name", false}, {"image", false}});
        QTRY_VERIFY(pending->isFinished()); QDBusPendingReply<quint32, QVariantMap> reply = *pending; QVERIFY(!reply.isError()); QCOMPARE(reply.argumentAt<0>(), 0U); QCOMPARE(reply.argumentAt<1>().value("id").toString(), QStringLiteral("fixture-id")); QVERIFY(reply.argumentAt<1>().value("name").toString().isEmpty()); QCOMPARE(reply.argumentAt<1>().value("image").toString(), QStringLiteral("file:///fixture/generic-avatar.png"));
    }
    void usbExactWireSubsetAndForgedOutput() {
        auto pending = usb(); QTRY_COMPARE(ui->opens, 1); Q_EMIT ui->completed(ui->token, RequestResponse::Success, QJsonObject{{"devices", QJsonArray{"one"}}});
        QTRY_VERIFY(pending->isFinished()); QDBusPendingReply<quint32, QVariantMap> reply = *pending; QVERIFY2(!reply.isError(), qPrintable(reply.error().message())); QCOMPARE(reply.argumentAt<0>(), 0U);
        const auto accepted = qdbus_cast<UsbSelections>(reply.argumentAt<1>().value("devices")); QCOMPARE(accepted.size(), 1); QCOMPARE(accepted.first().first, QStringLiteral("one")); QCOMPARE(accepted.first().second.value("writable"), QVariant(true));
        pending = usb(); QTRY_COMPARE(ui->opens, 2); Q_EMIT ui->completed(ui->token, RequestResponse::Success, QJsonObject{{"devices", QJsonArray{"foreign"}}}); QTRY_VERIFY(pending->isFinished()); reply = *pending; QCOMPARE(reply.argumentAt<0>(), 2U); QVERIFY(reply.argumentAt<1>().isEmpty());
    }
    void launcherPreparationAndAuthenticatedNoninteractiveGate() {
        auto pending = launcher(); QTRY_COMPARE(ui->opens, 1); Q_EMIT ui->completed(ui->token, RequestResponse::Success, QJsonObject{{"name", "Chosen"}}); QTRY_VERIFY(pending->isFinished()); QDBusPendingReply<quint32, QVariantMap> result = *pending; QVERIFY(!result.isError()); QCOMPARE(result.argumentAt<0>(), 0U); QCOMPARE(result.argumentAt<1>().value("name").toString(), QStringLiteral("Chosen"));
        auto token = call("DynamicLauncher", "RequestInstallToken", {QStringLiteral("org.gnome.Software"), QVariantMap{}}); QTRY_VERIFY(token->isFinished()); QDBusPendingReply<quint32> accepted = *token; QCOMPARE(accepted.value(), 0U);
        token = call("DynamicLauncher", "RequestInstallToken", {QStringLiteral("org.gnome.Software"), QVariantMap{}}, *stranger); QTRY_VERIFY(token->isFinished()); accepted = *token; QCOMPARE(accepted.value(), 2U);
    }
    void printTokenAndNoTokenPathsAreActualRequests() {
        auto pending = prepare(); QTRY_COMPARE(ui->opens, 1);
        Q_EMIT ui->completed(ui->token, RequestResponse::Success, QJsonObject{{"settings", QJsonObject{{"n-copies", "1"}}}, {"page-setup", QJsonObject{}}, {"printer", "fixture"}, {"output", ""}, {"cups", QJsonArray{}}});
        QTRY_VERIFY(pending->isFinished()); QDBusPendingReply<quint32, QVariantMap> prepared = *pending; QVERIFY(!prepared.isError()); QCOMPARE(prepared.argumentAt<0>(), 0U); const auto ticket = prepared.argumentAt<1>().value("token").toUInt(); QVERIFY(ticket);
        QTemporaryFile input; QVERIFY(input.open()); input.write("%PDF fixture"); input.flush();
        pending = print(input, {{"token", ticket}}, "org.test.Other"); QTRY_VERIFY(pending->isFinished()); QDBusPendingReply<quint32, QVariantMap> denied = *pending; QCOMPARE(denied.argumentAt<0>(), 2U); QCOMPARE(ui->opens, 1);
        pending = print(input, {{"token", ticket}}); QTRY_COMPARE(ui->opens, 2); QVERIFY(ui->frame.value("configuration").isObject()); QVERIFY(ui->printFd >= 0);
        Q_EMIT ui->completed(ui->token, RequestResponse::Success, {}); QTRY_VERIFY(pending->isFinished()); QDBusPendingReply<quint32, QVariantMap> printed = *pending; QCOMPARE(printed.argumentAt<0>(), 0U);
        pending = print(input, {{"token", ticket}}); QTRY_VERIFY(pending->isFinished()); denied = *pending; QCOMPARE(denied.argumentAt<0>(), 2U); QCOMPARE(ui->opens, 2);
        pending = print(input, {}); QTRY_COMPARE(ui->opens, 3); QVERIFY(!ui->frame.contains("configuration")); Q_EMIT ui->completed(ui->token, RequestResponse::Cancelled, {}); QTRY_VERIFY(pending->isFinished()); printed = *pending; QCOMPARE(printed.argumentAt<0>(), 1U);
    }
    void closeAndOwnerAuthorityLossRetireAllFamilies() {
        for (int family = 0; family < 4; ++family) {
            auto pending = family == 0 ? account() : family == 1 ? usb() : family == 2 ? launcher() : prepare(); QTRY_VERIFY(ui->token); const auto retired = ui->token;
            auto close = QDBusMessage::createMethodCall("org.test.Misc", path, "org.freedesktop.impl.portal.Request", "Close"); QDBusPendingCallWatcher closing(frontend->asyncCall(close)); QTRY_VERIFY(closing.isFinished()); QTRY_VERIFY(pending->isFinished()); QDBusPendingReply<quint32, QVariantMap> cancelled = *pending; QCOMPARE(cancelled.argumentAt<0>(), 1U); QVERIFY(!registry->live(retired));
            Q_EMIT ui->completed(retired, RequestResponse::Success, {}); QVERIFY(!registry->live(retired));
        }
        auto pending = usb(); QTRY_VERIFY(ui->token); ui->allowed = false; Q_EMIT ui->authorityLost(); QTRY_VERIFY(pending->isFinished()); QDBusPendingReply<quint32, QVariantMap> failed = *pending; QCOMPARE(failed.argumentAt<0>(), 2U);
        ui->allowed = true; pending = launcher(); QTRY_VERIFY(ui->token); QVERIFY(frontend->unregisterService("org.freedesktop.portal.Desktop")); QTRY_VERIFY(pending->isFinished()); failed = *pending; QCOMPARE(failed.argumentAt<0>(), 2U); QVERIFY(!ui->token);
    }
private:
    std::unique_ptr<QDBusPendingCallWatcher> call(const char *family, const char *method, QVariantList args, const QDBusConnection &caller) {
        auto message = QDBusMessage::createMethodCall("org.test.Misc", "/org/freedesktop/portal/desktop", QStringLiteral("org.freedesktop.impl.portal.") + QString::fromLatin1(family), QString::fromLatin1(method)); message.setArguments(args); return std::make_unique<QDBusPendingCallWatcher>(caller.asyncCall(message));
    }
    std::unique_ptr<QDBusPendingCallWatcher> call(const char *family, const char *method, QVariantList args) { return call(family, method, args, *frontend); }
    QVariant objectPath() { return QVariant::fromValue(QDBusObjectPath(path)); }
    std::unique_ptr<QDBusPendingCallWatcher> account() { return call("Account", "GetUserInformation", {objectPath(), QStringLiteral("org.test.App"), QString{}, QVariantMap{{"reason", "Fixture purpose"}}}); }
    std::unique_ptr<QDBusPendingCallWatcher> usb() { return call("Usb", "AcquireDevices", {objectPath(), QString{}, QStringLiteral("org.test.App"), QVariant::fromValue(UsbDevices{{"one", {}, QVariantMap{{"writable", true}}}}), QVariantMap{}}); }
    std::unique_ptr<QDBusPendingCallWatcher> launcher() { return call("DynamicLauncher", "PrepareInstall", {objectPath(), QStringLiteral("org.test.App"), QString{}, QStringLiteral("Fixture"), QVariant::fromValue(QDBusVariant(QVariant::fromValue(LauncherIcon{"bytes", QDBusVariant(QByteArray("fixture"))}))), QVariantMap{}}); }
    std::unique_ptr<QDBusPendingCallWatcher> prepare() { return call("Print", "PreparePrint", {objectPath(), QStringLiteral("org.test.App"), QString{}, QStringLiteral("Print"), QVariantMap{}, QVariantMap{}, QVariantMap{}}); }
    std::unique_ptr<QDBusPendingCallWatcher> print(QTemporaryFile &file, QVariantMap options, QString app = QStringLiteral("org.test.App")) { return call("Print", "Print", {objectPath(), app, QString{}, QStringLiteral("Print"), QVariant::fromValue(QDBusUnixFileDescriptor(file.handle())), options}); }
    QString path = QStringLiteral("/org/freedesktop/portal/desktop/request/caller/test");
    std::unique_ptr<PortalPrivateBus> fixture; std::unique_ptr<QDBusConnection> backend, frontend, stranger;
    std::unique_ptr<RequestRegistry> registry; std::unique_ptr<Ui> ui; std::unique_ptr<QObject> host;
};
QTEST_GUILESS_MAIN(MiscRequestsTest)
#include "tst_misc_requests.moc"
