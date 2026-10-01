// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/portal/file_chooser_adaptor.h>
#include <qindaqt/services/portal/app_chooser_adaptor.h>
#include "../foundation/support/private_bus.h"
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QtTest>
using namespace QindaQt::Services::Portal;
class Ui final : public ChooserUi {
public:
    bool admitted() const override { return allowed; }
    void openFile(RequestToken value, const FileChooserRequest &r) override { token = value; file = r; ++opens; }
    void chooseApplication(RequestToken value, const AppChooserRequest &r) override { token = value; apps = r; ++opens; }
    void updateApplications(RequestToken value, const ApplicationCandidates &c) override { if (value == token) { apps.candidates = c; ++updates; } }
    void cancel(RequestToken value) override { if (value == token) { token = 0; ++cancels; } }
    RequestToken token = 0; bool allowed = true; int opens = 0, updates = 0, cancels = 0;
    FileChooserRequest file; AppChooserRequest apps;
};
class ChooserRequestsTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void init() {
        fixture = std::make_unique<PortalPrivateBus>(); QVERIFY(fixture->start()); backend = fixture->connect(); frontend = fixture->connect(); stranger = fixture->connect();
        QVERIFY(frontend->registerService(QStringLiteral("org.freedesktop.portal.Desktop")));
        registry = std::make_unique<RequestRegistry>(*backend); ui = std::make_unique<Ui>(); host = std::make_unique<QObject>();
        new FileChooserAdaptor(*host, *registry, *ui);
        new AppChooserAdaptor(*host, *registry, *ui, [this] { return catalog; }, *backend);
        QVERIFY(backend->registerObject(QStringLiteral("/org/freedesktop/portal/desktop"), host.get(), QDBusConnection::ExportAdaptors));
        QVERIFY(backend->registerService(QStringLiteral("org.test.Choosers")));
        QindaQt::ApplicationCatalog::ScannedApplication one, two; one.entry.id = QStringLiteral("org.test.One"); one.entry.name = QStringLiteral("One"); two.entry.id = QStringLiteral("org.test.Two"); two.entry.name = QStringLiteral("Two");
        catalog.applications = {one, two};
    }
    void cleanup() { host.reset(); registry.reset(); ui.reset(); backend.reset(); frontend.reset(); stranger.reset(); fixture.reset(); }
    void exactFileMethodsAndCancellation() {
        for (const auto *method : {"OpenFile", "SaveFile", "SaveFiles"}) {
            QVariantMap options; if (QByteArray(method) == "SaveFiles") options.insert(QStringLiteral("files"), QVariant::fromValue(FileNames{QByteArray("one\0", 4)}));
            auto reply = file(method, options); const int expected = ui->opens + 1; QTRY_COMPARE(ui->opens, expected);
            Q_EMIT ui->completed(ui->token, RequestResponse::Success, QJsonObject{{"uris", QJsonArray{"file:///private/one"}}, {"choices", QJsonArray{}}, {"filter", -1}});
            QTRY_VERIFY(reply->isFinished()); const QDBusPendingReply<quint32, QVariantMap> result = *reply; QVERIFY(!result.isError()); QCOMPARE(result.argumentAt<0>(), 0U);
        }
        auto pending = file("OpenFile", {}); QTRY_VERIFY(ui->token != 0); const auto retired = ui->token;
        auto close = QDBusMessage::createMethodCall(QStringLiteral("org.test.Choosers"), path, QStringLiteral("org.freedesktop.impl.portal.Request"), QStringLiteral("Close"));
        QDBusPendingCallWatcher closing(frontend->asyncCall(close)); QTRY_VERIFY(closing.isFinished()); QTRY_VERIFY(pending->isFinished());
        const QDBusPendingReply<quint32, QVariantMap> cancelled = *pending; QCOMPARE(cancelled.argumentAt<0>(), 1U);
        Q_EMIT ui->completed(retired, RequestResponse::Success, QJsonObject{{"uris", QJsonArray{"file:///private/leak"}}}); QVERIFY(!registry->live(retired));
    }
    void appUpdateReplacesOfferedResults() {
        auto reply = app(); QTRY_COMPARE(ui->opens, 1); const auto token = ui->token;
        auto update = call("org.freedesktop.impl.portal.AppChooser", "UpdateChoices", {QVariant::fromValue(QDBusObjectPath(path)), QStringList{QStringLiteral("org.test.Two")}});
        QTRY_VERIFY(update->isFinished()); QCOMPARE(ui->updates, 1); QCOMPARE(ui->apps.candidates.first().id, QStringLiteral("org.test.Two"));
        Q_EMIT ui->completed(token, RequestResponse::Success, QJsonObject{{"choice", "org.test.One"}});
        QTRY_VERIFY(reply->isFinished()); const QDBusPendingReply<quint32, QVariantMap> result = *reply; QCOMPARE(result.argumentAt<0>(), 2U); QVERIFY(result.argumentAt<1>().isEmpty());
    }
    void appUpdatesFenceOwnerHandleAndKnownTypes() {
        auto pending = app(); QTRY_COMPARE(ui->opens, 1);
        const QVariantList offered{QVariant::fromValue(QDBusObjectPath(path)), QStringList{QStringLiteral("org.test.Two")}};
        auto denied = call("org.freedesktop.impl.portal.AppChooser", "UpdateChoices", offered, *stranger);
        QTRY_VERIFY(denied->isFinished()); const QDBusPendingReply<> deniedResult = *denied;
        QVERIFY(deniedResult.isError()); QCOMPARE(deniedResult.error().name(), QStringLiteral("org.freedesktop.DBus.Error.AccessDenied")); QCOMPARE(ui->updates, 0);
        auto missing = call("org.freedesktop.impl.portal.AppChooser", "UpdateChoices",
            {QVariant::fromValue(QDBusObjectPath(path + QStringLiteral("missing"))), QStringList{QStringLiteral("org.test.Two")}});
        QTRY_VERIFY(missing->isFinished()); const QDBusPendingReply<> missingResult = *missing;
        QVERIFY(missingResult.isError()); QCOMPARE(missingResult.error().name(), QStringLiteral("org.freedesktop.portal.Error.NotFound")); QVERIFY(ui->token);
        auto invalid = call("org.freedesktop.impl.portal.AppChooser", "UpdateChoices",
            {QVariant::fromValue(QDBusObjectPath(path)), QStringList{QStringLiteral("not/a/canonical/id")}});
        QTRY_VERIFY(invalid->isFinished()); const QDBusPendingReply<> invalidResult = *invalid;
        QVERIFY(invalidResult.isError()); QCOMPARE(invalidResult.error().name(), QStringLiteral("org.freedesktop.DBus.Error.InvalidArgs"));
        QTRY_VERIFY(pending->isFinished()); const QDBusPendingReply<quint32, QVariantMap> retired = *pending;
        QCOMPARE(retired.argumentAt<0>(), 2U); QVERIFY(retired.argumentAt<1>().isEmpty()); QVERIFY(!ui->token);
    }
    void frontendLossAuthorityLossAndUnauthorizedCaller() {
        auto denied = file("OpenFile", {}, *stranger); QTRY_VERIFY(denied->isFinished()); const QDBusPendingReply<quint32, QVariantMap> denial = *denied; QVERIFY(denial.isError()); QCOMPARE(ui->opens, 0);
        auto pending = file("OpenFile", {}); QTRY_VERIFY(ui->token != 0); ui->allowed = false; Q_EMIT ui->authorityLost();
        QTRY_VERIFY(pending->isFinished()); const QDBusPendingReply<quint32, QVariantMap> lost = *pending; QCOMPARE(lost.argumentAt<0>(), 2U);
        ui->allowed = true; auto current = app(); QTRY_VERIFY(ui->token != 0);
        QVERIFY(frontend->unregisterService(QStringLiteral("org.freedesktop.portal.Desktop")));
        QTRY_VERIFY(current->isFinished()); const QDBusPendingReply<quint32, QVariantMap> churn = *current; QCOMPARE(churn.argumentAt<0>(), 2U); QVERIFY(!ui->token);
    }
private:
    std::unique_ptr<QDBusPendingCallWatcher> call(const char *interface, const char *method, QVariantList args, const QDBusConnection &caller) {
        auto message = QDBusMessage::createMethodCall(QStringLiteral("org.test.Choosers"), QStringLiteral("/org/freedesktop/portal/desktop"), QString::fromLatin1(interface), QString::fromLatin1(method));
        message.setArguments(args); return std::make_unique<QDBusPendingCallWatcher>(caller.asyncCall(message));
    }
    std::unique_ptr<QDBusPendingCallWatcher> call(const char *interface, const char *method, QVariantList args) { return call(interface, method, args, *frontend); }
    std::unique_ptr<QDBusPendingCallWatcher> file(const char *method, QVariantMap options, const QDBusConnection &caller) {
        return call("org.freedesktop.impl.portal.FileChooser", method, {QVariant::fromValue(QDBusObjectPath(path)), QStringLiteral("org.test.Caller"), QString{}, QStringLiteral("Chooser"), options}, caller);
    }
    std::unique_ptr<QDBusPendingCallWatcher> file(const char *method, QVariantMap options) { return file(method, options, *frontend); }
    std::unique_ptr<QDBusPendingCallWatcher> app() { return call("org.freedesktop.impl.portal.AppChooser", "ChooseApplication", {QVariant::fromValue(QDBusObjectPath(path)), QStringLiteral("org.test.Caller"), QString{}, QStringList{QStringLiteral("org.test.One")}, QVariantMap{}}); }
    QString path = QStringLiteral("/org/freedesktop/portal/desktop/request/caller/test");
    std::unique_ptr<PortalPrivateBus> fixture; std::unique_ptr<QDBusConnection> backend, frontend, stranger;
    std::unique_ptr<RequestRegistry> registry; std::unique_ptr<Ui> ui; std::unique_ptr<QObject> host;
    QindaQt::ApplicationCatalog::DirectoryScan catalog;
};
QTEST_GUILESS_MAIN(ChooserRequestsTest)
#include "tst_chooser_requests.moc"
