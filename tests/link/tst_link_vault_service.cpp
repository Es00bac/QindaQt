// SPDX-License-Identifier: GPL-3.0-or-later
// The real qindaqt-link-service on a private bus (dbus-run-session) with a fake RemoteDesktop portal
// registered under a unique name: who may publish the focus fact (ADR-0440), and desktop.focus plus
// vault.type end to end. Only throwaway strings; nothing here can reach the real portal or the user's
// bus. The service gets --test-portal and --test-open-gate, which exist for exactly this.
#include "support/fake_portal.h"
#include "support/scripted_phone.h"

#include <QtCore/QFile>
#include <QtCore/QProcess>
#include <QtCore/QTemporaryDir>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusMessage>
#include <QtTest/QtTest>

using namespace QindaQt::Link;
using namespace QindaQt::Link::Test;

namespace {

constexpr auto kDenied = "org.qindaqt.Link1.Error.Denied";
constexpr auto kBadRequest = "org.qindaqt.Link1.Error.BadRequest";
QString s(const char *v) { return QString::fromLatin1(v); }

QString fact(const QString &uuid, int revision, const QString &title = s("Terminal"))
{
    return QString::fromUtf8(QJsonDocument(QJsonObject{{s("uuid"), uuid}, {s("appId"), s("org.example.term")},
                                                       {s("title"), title}, {s("revision"), revision}})
                                 .toJson(QJsonDocument::Compact));
}

struct Service {
    QTemporaryDir data, cfg, cache, state;
    QProcess proc;
    quint16 port = 0;

    bool start(const QStringList &extra)
    {
        proc.setProcessChannelMode(QProcess::ForwardedChannels);
        QStringList args{s("--data-dir"), data.path(), s("--config-dir"), cfg.path(), s("--cache-dir"), cache.path(),
                         s("--state-dir"), state.path(), s("--port"), s("0"), s("--no-adb"), s("--no-notifications"),
                         s("--recording-notifier"), data.filePath(s("n.jsonl"))};
        proc.start(QStringLiteral(QINDAQT_LINK_SERVICE_BINARY), args + extra);
        if (!proc.waitForStarted()) return false;
        QDBusInterface link(s("org.qindaqt.Link1"), s("/org/qindaqt/Link1"), s("org.qindaqt.Link1"), QDBusConnection::sessionBus());
        if (!waitUntil([&] { return link.isValid(); }, 10000)) return false;
        port = quint16(link.call(s("ListenPort")).arguments().value(0).toUInt());
        return port != 0 && port != 47160;
    }
    ~Service()
    {
        proc.terminate(); // our own child, by handle
        proc.waitForFinished(5000);
    }
};

QDBusMessage publish(QDBusConnection bus, const QString &json)
{
    QDBusMessage m = QDBusMessage::createMethodCall(s("org.qindaqt.Link1"), s("/org/qindaqt/Link1"), s("org.qindaqt.Link1.Focus"), s("Publish"));
    m << json;
    return bus.call(m, QDBus::Block, 10000);
}

} // namespace

class TstLinkVaultService : public QObject {
    Q_OBJECT
private slots:
    void clientsThatAreNotTheShellCannotPublish();
    void shellPublishesAndPhoneTypesEndToEnd();
    void focusIsForgottenWhenThePublisherLeavesTheBus();
};

void TstLinkVaultService::clientsThatAreNotTheShellCannotPublish()
{
    QVERIFY2(!qEnvironmentVariableIsEmpty("DBUS_SESSION_BUS_ADDRESS"), "run under dbus-run-session");
    Service svc;
    // This binary is a trusted CLIENT (it may use Setup, Messages, ...) but not a shell.
    QVERIFY(svc.start({s("--trusted-caller"), QCoreApplication::applicationFilePath()}));
    QDBusConnection bus = QDBusConnection::sessionBus();
    const QDBusMessage reply = publish(bus, fact(s("{w}"), 1));
    QCOMPARE(reply.type(), QDBusMessage::ErrorMessage);
    QCOMPARE(reply.errorName(), s(kDenied));
    // The same call is fine for a client on an ordinary protected member, so the difference is the role.
    QDBusInterface notif(s("org.qindaqt.Link1"), s("/org/qindaqt/Link1"), s("org.qindaqt.Link1.Notifications"), bus);
    QVERIFY(notif.call(s("Current")).type() == QDBusMessage::ReplyMessage);
    // An untrusted program is denied too.
    QProcess g;
    g.setProcessChannelMode(QProcess::MergedChannels);
    g.start(s("/usr/bin/gdbus"), {s("call"), s("--session"), s("--dest"), s("org.qindaqt.Link1"), s("--object-path"),
                                  s("/org/qindaqt/Link1"), s("--method"), s("org.qindaqt.Link1.Focus.Publish"), s("{}")});
    QVERIFY(g.waitForFinished(15000));
    QVERIFY(QString::fromUtf8(g.readAll()).contains(s(kDenied)));
}

// Pairs `phone` (caps status+vault) with the service, confirming through the trusted client path.
bool pairWithService(ScriptedPhone &phone, Service &svc)
{
    QDBusInterface link(s("org.qindaqt.Link1"), s("/org/qindaqt/Link1"), s("org.qindaqt.Link1"), QDBusConnection::sessionBus());
    if (!phone.connectTo(svc.port) || !phone.waitFor(s("hello"))) return false;
    phone.sendHello(false, {s("status"), s("vault")});
    QString code;
    if (!phone.pairUntilCode(&code)) return false;
    if (!waitUntil([&] { return link.call(s("PendingPairings")).arguments().value(0).toString().contains(phone.deviceId()); })) return false;
    link.call(s("ConfirmPairing"), phone.deviceId(), true);
    Envelope confirm;
    if (!phone.waitFor(s("pair.confirm"), &confirm)) return false;
    Envelope accept;
    accept.t = s("pair.confirm");
    accept.b = QJsonObject{{s("accept"), true}};
    phone.send(accept);
    return waitUntil([&] { return link.call(s("Devices")).arguments().value(0).toString().contains(s("\"ready\":true")); });
}

Envelope request(const QString &t, qint64 id, QJsonObject body = {})
{
    Envelope e;
    e.t = t;
    e.id = id;
    e.b = std::move(body);
    return e;
}

void TstLinkVaultService::shellPublishesAndPhoneTypesEndToEnd()
{
    QVERIFY2(!qEnvironmentVariableIsEmpty("DBUS_SESSION_BUS_ADDRESS"), "run under dbus-run-session");
    const QString portalName = s("org.qindaqt.test.FakeServicePortal") + QString::number(QCoreApplication::applicationPid());
    FakePortal portal(portalName);
    QVERIFY(portal.ok());
    Service svc;
    QVERIFY(svc.start({s("--trusted-shell-caller"), QCoreApplication::applicationFilePath(), s("--test-open-gate"),
                       s("--test-portal"), portalName}));
    QDBusConnection bus = QDBusConnection::sessionBus();

    // Bad facts are refused and change nothing.
    QCOMPARE(publish(bus, s("not json")).errorName(), s(kBadRequest));
    QCOMPARE(publish(bus, QString(5000, QLatin1Char(' '))).errorName(), s(kBadRequest));
    QCOMPARE(publish(bus, s("{\"uuid\":\"\",\"appId\":\"a\",\"title\":\"t\",\"revision\":1}")).errorName(), s(kBadRequest));

    ScriptedPhone phone;
    QVERIFY(pairWithService(phone, svc));

    // No fact yet: unverified.
    Envelope e;
    phone.send(request(s("desktop.focus"), 1));
    QVERIFY(phone.waitFor(s("error"), &e));
    QCOMPARE(e.b.value(s("code")).toString(), s("unverified"));

    // The shell publishes; the phone asks, confirms and types.
    QCOMPARE(publish(bus, fact(s("{win-1}"), 5, s("Sign in"))).type(), QDBusMessage::ReplyMessage);
    phone.send(request(s("desktop.focus"), 2));
    QVERIFY(phone.waitFor(s("desktop.focus"), &e));
    QCOMPARE(e.re, 2);
    QCOMPARE(e.b.value(s("title")).toString(), s("Sign in"));
    QCOMPARE(e.b.value(s("app")).toString(), s("org.example.term"));
    const QString token = e.b.value(s("token")).toString();
    QVERIFY(!token.isEmpty());
    phone.send(request(s("vault.type"), 3,
                       QJsonObject{{s("token"), token},
                                   {s("steps"), QJsonArray{QJsonObject{{s("text"), s("ab")}}, QJsonObject{{s("key"), s("Tab")}}}}}));
    QVERIFY(phone.waitFor(s("ok"), &e));
    QCOMPARE(e.re, 3);
    QTest::qWait(150); // portal calls trail the phone's reply
    QStringList pressed;
    for (const QString &c : std::as_const(portal.calls)) {
        if (c.endsWith(s(":1")) && c.startsWith(s("NotifyKeyboardKeysym"))) pressed << c.section(QLatin1Char(':'), 1, 1);
    }
    QCOMPARE(pressed, (QStringList{s("97"), s("98"), s("65289")}));

    // A new focus revision published between confirm and type: target-changed, nothing typed.
    phone.send(request(s("desktop.focus"), 4));
    QVERIFY(phone.waitFor(s("desktop.focus"), &e));
    const QString token2 = e.b.value(s("token")).toString();
    QCOMPARE(publish(bus, fact(s("{win-2}"), 6)).type(), QDBusMessage::ReplyMessage);
    portal.calls.clear();
    phone.send(request(s("vault.type"), 5, QJsonObject{{s("token"), token2}, {s("steps"), QJsonArray{QJsonObject{{s("text"), s("zz")}}}}}));
    QVERIFY(phone.waitFor(s("error"), &e));
    QCOMPARE(e.b.value(s("code")).toString(), s("target-changed"));
    QTest::qWait(100);
    QVERIFY(portal.calls.isEmpty());
}

void TstLinkVaultService::focusIsForgottenWhenThePublisherLeavesTheBus()
{
    QVERIFY2(!qEnvironmentVariableIsEmpty("DBUS_SESSION_BUS_ADDRESS"), "run under dbus-run-session");
    const QString portalName = s("org.qindaqt.test.FakeServicePortal2") + QString::number(QCoreApplication::applicationPid());
    FakePortal portal(portalName);
    QVERIFY(portal.ok());
    Service svc;
    QVERIFY(svc.start({s("--trusted-shell-caller"), QCoreApplication::applicationFilePath(), s("--test-open-gate"),
                       s("--test-portal"), portalName}));
    ScriptedPhone phone;
    QVERIFY(pairWithService(phone, svc));
    // A separate connection plays the shell, so it can leave the bus on its own.
    {
        // The handle must go out of scope before disconnectFromBus, or the connection stays open.
        QDBusConnection shell = QDBusConnection::connectToBus(QDBusConnection::SessionBus, s("fake-shell"));
        QVERIFY(shell.isConnected());
        QCOMPARE(publish(shell, fact(s("{win-9}"), 1)).type(), QDBusMessage::ReplyMessage);
    }
    Envelope e;
    phone.send(request(s("desktop.focus"), 1));
    QVERIFY(phone.waitFor(s("desktop.focus"), &e));
    QDBusConnection::disconnectFromBus(s("fake-shell"));
    // Once the service sees the name vanish, the fact is unknown again.
    QVERIFY(waitUntil([&] {
        phone.send(request(s("desktop.focus"), 2));
        Envelope r;
        return phone.waitFor(s("error"), &r, 300) && r.b.value(s("code")).toString() == s("unverified");
    }, 6000));
}

QTEST_GUILESS_MAIN(TstLinkVaultService)
#include "tst_link_vault_service.moc"
