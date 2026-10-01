// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/portal/access_adaptor.h>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>
using namespace QindaQt::Services::Portal;
class Consent final : public AccessConsent {
public:
    using AccessConsent::AccessConsent;
    bool admitted() const override { return allowed; }
    void ask(RequestToken t, const AccessQuestion &q) override { token = t; question = q; ++asks; }
    void cancel(RequestToken t) override { if (token == t) { token = 0; ++cancels; } }
    bool allowed = true;
    RequestToken token = 0;
    AccessQuestion question;
    int asks = 0, cancels = 0;
};
class AccessTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void init() {
        directory = std::make_unique<QTemporaryDir>(); QVERIFY(directory->isValid());
        QFile config(directory->filePath(QStringLiteral("bus.conf"))); QVERIFY(config.open(QIODevice::WriteOnly));
        config.write("<busconfig><type>session</type><listen>unix:tmpdir=" + directory->path().toUtf8()
            + "</listen><auth>EXTERNAL</auth><policy context='default'><allow send_destination='*'/>"
              "<allow receive_sender='*'/><allow own='*'/></policy></busconfig>"); config.close();
        daemon.start(QStringLiteral("/usr/bin/dbus-daemon"), {QStringLiteral("--nofork"),
            QStringLiteral("--print-address=1"), QStringLiteral("--config-file=") + config.fileName()});
        QVERIFY(daemon.waitForStarted()); QVERIFY(daemon.waitForReadyRead());
        address = QString::fromUtf8(daemon.readLine()).trimmed();
        serviceName = QUuid::createUuid().toString(); clientName = QUuid::createUuid().toString();
        service = std::make_unique<QDBusConnection>(QDBusConnection::connectToBus(address, serviceName));
        client = std::make_unique<QDBusConnection>(QDBusConnection::connectToBus(address, clientName));
        QVERIFY(service->isConnected()); QVERIFY(client->isConnected());
        QVERIFY(client->registerService(QStringLiteral("org.freedesktop.portal.Desktop")));
        host = std::make_unique<QObject>(); registry = std::make_unique<RequestRegistry>(*service);
        consent = std::make_unique<Consent>(); new AccessAdaptor(*host, *registry, *consent);
        QVERIFY(service->registerObject(QStringLiteral("/org/freedesktop/portal/desktop"), host.get(),
                                       QDBusConnection::ExportAdaptors));
        QVERIFY(service->registerService(QStringLiteral("org.test.Portal")));
    }
    void cleanup() {
        host.reset(); registry.reset(); consent.reset();
        service.reset(); client.reset();
        QDBusConnection::disconnectFromBus(serviceName); QDBusConnection::disconnectFromBus(clientName);
        daemon.terminate(); if (!daemon.waitForFinished(2000)) { daemon.kill(); daemon.waitForFinished(2000); }
        directory.reset();
    }
    void grantChoices() {
        AccessChoices choices{{QStringLiteral("audio"), QStringLiteral("Sound"), {}, QStringLiteral("false")},
            {QStringLiteral("device"), QStringLiteral("Device"), {{QStringLiteral("one"), QStringLiteral("First")}}, QStringLiteral("one")}};
        auto reply = call({{QStringLiteral("choices"), QVariant::fromValue(choices)}});
        QTRY_COMPARE(consent->asks, 1); QCOMPARE(consent->question.choices.size(), 2);
        const ChoiceValues selected{{QStringLiteral("audio"), QStringLiteral("true")}, {QStringLiteral("device"), QStringLiteral("one")}};
        Q_EMIT consent->completed(consent->token, RequestResponse::Success, selected);
        QTRY_VERIFY(reply->isFinished()); const QDBusPendingReply<quint32, QVariantMap> result = *reply;
        QVERIFY(!result.isError()); QCOMPARE(result.argumentAt<0>(), 0U);
        QCOMPARE(qdbus_cast<ChoiceValues>(result.argumentAt<1>().value(QStringLiteral("choices"))).size(), 2);
        QCOMPARE(consent->cancels, 1);
    }
    void deny() {
        auto reply = call(); QTRY_COMPARE(consent->asks, 1);
        Q_EMIT consent->completed(consent->token, RequestResponse::Cancelled, ChoiceValues{});
        QTRY_VERIFY(reply->isFinished()); const QDBusPendingReply<quint32, QVariantMap> result = *reply;
        QCOMPARE(result.argumentAt<0>(), 1U); QVERIFY(result.argumentAt<1>().isEmpty());
    }
    void closeAndLateCompletion() {
        auto reply = call(); QTRY_COMPARE(consent->asks, 1); const auto token = consent->token;
        auto close = QDBusMessage::createMethodCall(QStringLiteral("org.test.Portal"), path,
            QStringLiteral("org.freedesktop.impl.portal.Request"), QStringLiteral("Close"));
        QDBusPendingCallWatcher closing(client->asyncCall(close)); QTRY_VERIFY(closing.isFinished());
        QTRY_VERIFY(reply->isFinished()); const QDBusPendingReply<quint32, QVariantMap> result = *reply;
        QCOMPARE(result.argumentAt<0>(), 1U); QCOMPARE(consent->cancels, 1);
        Q_EMIT consent->completed(token, RequestResponse::Success, ChoiceValues{});
        QVERIFY(!registry->live(token));
    }
    void nativeAuthorityLost() {
        auto reply = call(); QTRY_COMPARE(consent->asks, 1);
        consent->allowed = false; Q_EMIT consent->authorityLost();
        QTRY_VERIFY(reply->isFinished()); const QDBusPendingReply<quint32, QVariantMap> result = *reply;
        QCOMPARE(result.argumentAt<0>(), 2U); QCOMPARE(consent->cancels, 1);
    }
    void frontendOwnerLoss() {
        auto reply = call(); QTRY_COMPARE(consent->asks, 1);
        QVERIFY(client->unregisterService(QStringLiteral("org.freedesktop.portal.Desktop")));
        QTRY_VERIFY(reply->isFinished()); const QDBusPendingReply<quint32, QVariantMap> result = *reply;
        QCOMPARE(result.argumentAt<0>(), 2U); QCOMPARE(consent->cancels, 1);
        QVERIFY(client->registerService(QStringLiteral("org.freedesktop.portal.Desktop")));
        QCOMPARE(consent->asks, 1);
    }
    void duplicateRequest() {
        auto first = call(); QTRY_COMPARE(consent->asks, 1);
        auto second = call(); QTRY_VERIFY(second->isFinished());
        const QDBusPendingReply<quint32, QVariantMap> result = *second;
        QVERIFY(result.isError()); QCOMPARE(result.error().name(), QStringLiteral("org.freedesktop.portal.Error.Exists"));
        QCOMPARE(consent->asks, 1); registry->retire(consent->token, RequestResponse::Failed);
        QTRY_VERIFY(first->isFinished());
    }
    void malformedAndUnavailable() {
        auto malformed = call({{QStringLiteral("modal"), QStringLiteral("true")}});
        QTRY_VERIFY(malformed->isFinished()); const QDBusPendingReply<quint32, QVariantMap> result = *malformed;
        QCOMPARE(result.argumentAt<0>(), 2U); QCOMPARE(consent->asks, 0);
        consent->allowed = false; auto unavailable = call(); QTRY_VERIFY(unavailable->isFinished());
        const QDBusPendingReply<quint32, QVariantMap> unavailableResult = *unavailable;
        QCOMPARE(unavailableResult.argumentAt<0>(), 2U); QCOMPARE(consent->asks, 0);
    }
    void invalidSelectionsFail() {
        auto reply = call(); QTRY_COMPARE(consent->asks, 1);
        Q_EMIT consent->completed(consent->token, RequestResponse::Success,
                                  ChoiceValues{{QStringLiteral("unoffered"), QStringLiteral("true")}});
        QTRY_VERIFY(reply->isFinished()); const QDBusPendingReply<quint32, QVariantMap> result = *reply;
        QCOMPARE(result.argumentAt<0>(), 2U); QVERIFY(result.argumentAt<1>().isEmpty());
    }
    void policyBounds() {
        QVERIFY(!accessQuestion({}, QStringLiteral("x11:42"), {}, {}, {}, {}));
        QVERIFY(!accessQuestion({}, {}, QString(513, QLatin1Char('a')), {}, {}, {}));
        AccessChoices duplicate{{QStringLiteral("same"), {}, {}, QStringLiteral("true")},
                               {QStringLiteral("same"), {}, {}, QStringLiteral("false")}};
        QVERIFY(!accessQuestion({}, {}, {}, {}, {}, {{QStringLiteral("choices"), QVariant::fromValue(duplicate)}}));
        QVERIFY(accessQuestion({}, {}, {}, {}, {}, {})); // Primary spec permits host app_id empty.
    }
private:
    std::unique_ptr<QDBusPendingCallWatcher> call(const QVariantMap &options = {}) {
        auto message = QDBusMessage::createMethodCall(QStringLiteral("org.test.Portal"),
            QStringLiteral("/org/freedesktop/portal/desktop"), QStringLiteral("org.freedesktop.impl.portal.Access"), QStringLiteral("AccessDialog"));
        message.setArguments({QVariant::fromValue(QDBusObjectPath(path)), QStringLiteral("org.test.App"),
            QString{}, QStringLiteral("Allow access?"), QStringLiteral("Synthetic application"), QStringLiteral("Private test"), options});
        return std::make_unique<QDBusPendingCallWatcher>(client->asyncCall(message));
    }
    const QString path = QStringLiteral("/org/freedesktop/portal/desktop/request/fixture/test");
    QProcess daemon; QString address, serviceName, clientName;
    std::unique_ptr<QTemporaryDir> directory;
    std::unique_ptr<QDBusConnection> service, client;
    std::unique_ptr<QObject> host;
    std::unique_ptr<RequestRegistry> registry;
    std::unique_ptr<Consent> consent;
};
QTEST_GUILESS_MAIN(AccessTest)
#include "tst_access.moc"
