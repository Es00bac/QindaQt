// SPDX-License-Identifier: GPL-3.0-or-later
// Cross-implementation: a real JVM phone (Session + VaultTypeFeature + VaultTypeFlow over JSSE TLS 1.3) sends
// desktop.focus and vault.type to the real qindaqt-link-service on a private bus. The portal is the fake from
// support/fake_portal.h and this binary plays the shell that publishes the focus fact (--trusted-shell-caller).
// Run it through phone/engine-tests/run-cross-vault.sh, which compiles the JVM phone and exports
// QINDALINK_VAULT_PHONE_CP; without it the test skips. Throwaway data only; no secret is printed.
#include "support/fake_portal.h"
#include "support/scripted_phone.h"

#include <QtCore/QProcess>
#include <QtCore/QTemporaryDir>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusMessage>
#include <QtTest/QtTest>

using namespace QindaQt::Link::Test;

namespace {
QString s(const char *v) { return QString::fromLatin1(v); }

QDBusMessage publish(const QString &uuid, int revision, const QString &title)
{
    const QString json = QString::fromUtf8(QJsonDocument(QJsonObject{{s("uuid"), uuid}, {s("appId"), s("org.example.editor")},
                                                                    {s("title"), title}, {s("revision"), revision}})
                                               .toJson(QJsonDocument::Compact));
    QDBusMessage m = QDBusMessage::createMethodCall(s("org.qindaqt.Link1"), s("/org/qindaqt/Link1"), s("org.qindaqt.Link1.Focus"), s("Publish"));
    m << json;
    return QDBusConnection::sessionBus().call(m, QDBus::Block, 10000);
}

QDBusMessage publishNone()
{
    QDBusMessage m = QDBusMessage::createMethodCall(s("org.qindaqt.Link1"), s("/org/qindaqt/Link1"), s("org.qindaqt.Link1.Focus"), s("Publish"));
    m << s("{}");
    return QDBusConnection::sessionBus().call(m, QDBus::Block, 10000);
}
}

class TstLinkVaultCross : public QObject {
    Q_OBJECT
private slots:
    void jvmPhoneTypesThroughTheRealService();
};

void TstLinkVaultCross::jvmPhoneTypesThroughTheRealService()
{
    const QString cp = qEnvironmentVariable("QINDALINK_VAULT_PHONE_CP");
    if (cp.isEmpty()) QSKIP("run through phone/engine-tests/run-cross-vault.sh");
    QVERIFY2(!qEnvironmentVariableIsEmpty("DBUS_SESSION_BUS_ADDRESS"), "run under dbus-run-session");
    const QString portalName = s("org.qindaqt.test.FakeCrossPortal") + QString::number(QCoreApplication::applicationPid());
    FakePortal portal(portalName);
    QVERIFY(portal.ok());
    QTemporaryDir data, cfg, cache, state;
    QProcess service;
    service.setProcessChannelMode(QProcess::ForwardedChannels);
    service.start(QStringLiteral(QINDAQT_LINK_SERVICE_BINARY),
                  {s("--data-dir"), data.path(), s("--config-dir"), cfg.path(), s("--cache-dir"), cache.path(), s("--state-dir"), state.path(),
                   s("--port"), s("0"), s("--no-adb"), s("--no-notifications"), s("--recording-notifier"), data.filePath(s("n.jsonl")),
                   s("--trusted-shell-caller"), QCoreApplication::applicationFilePath(), s("--test-open-gate"), s("--test-portal"), portalName});
    QVERIFY(service.waitForStarted());
    QDBusInterface link(s("org.qindaqt.Link1"), s("/org/qindaqt/Link1"), s("org.qindaqt.Link1"), QDBusConnection::sessionBus());
    QVERIFY(waitUntil([&] { return link.isValid(); }, 10000));
    const quint16 port = quint16(link.call(s("ListenPort")).arguments().value(0).toUInt());
    QVERIFY(port != 0 && port != 47160);

    QProcess phone;
    phone.setProcessChannelMode(QProcess::SeparateChannels);
    phone.setProcessEnvironment(QProcessEnvironment::systemEnvironment());
    phone.start(s("java"), {s("-cp"), cp, s("CrossVaultPhone"), QString::number(port)});
    QVERIFY(phone.waitForStarted());
    auto readLine = [&](QString *out, int ms = 30000) {
        if (!waitUntil([&] { return phone.canReadLine() || phone.state() == QProcess::NotRunning; }, ms) || !phone.canReadLine()) return false;
        *out = QString::fromUtf8(phone.readLine()).trimmed();
        return true;
    };
    auto say = [&](const QString &line) { phone.write(line.toUtf8() + '\n'); };

    QString line;
    QVERIFY(readLine(&line));
    QVERIFY2(line.startsWith(s("CODE ")), "pairing did not start");
    const QString deviceId = line.section(QLatin1Char(' '), 2, 2);
    QVERIFY(waitUntil([&] { return link.call(s("PendingPairings")).arguments().value(0).toString().contains(deviceId); }));
    link.call(s("ConfirmPairing"), deviceId, true);
    say(s("ACCEPT"));
    QVERIFY(readLine(&line));
    QCOMPARE(line, s("READY"));
    QVERIFY(waitUntil([&] { return link.call(s("Devices")).arguments().value(0).toString().contains(s("\"vault\"")); }));

    auto pressed = [&] {
        QStringList out;
        for (const QString &c : std::as_const(portal.calls)) {
            if (c.startsWith(s("NotifyKeyboardKeysym")) && c.endsWith(s(":1"))) out << c.section(QLatin1Char(':'), 1, 1);
        }
        return out;
    };

    // 1. Focus unknown: the phone is told, nothing to confirm.
    say(s("TYPE autotype"));
    QVERIFY(readLine(&line));
    QVERIFY2(line.startsWith(s("RESULT failed")), "expected a refusal while focus is unknown");

    // 2. Autotype into the published window: user-X, Tab, pw-X, Enter.
    QCOMPARE(publish(s("{win-1}"), 3, s("Sign in")).type(), QDBusMessage::ReplyMessage);
    say(s("TYPE autotype"));
    QVERIFY(readLine(&line));
    QCOMPARE(line, s("CONFIRM org.example.editor|Sign in"));
    QCOMPARE(portal.calls.size(), 0); // nothing typed before the confirmation
    say(s("CONFIRM"));
    QVERIFY(readLine(&line));
    QVERIFY2(line.startsWith(s("RESULT done")), qPrintable(line));
    QStringList expected;
    for (const char *ch : {"u", "s", "e", "r", "-", "X"}) expected << QString::number(QString::fromLatin1(ch).at(0).unicode());
    expected << s("65289");
    for (const char *ch : {"p", "w", "-", "X"}) expected << QString::number(QString::fromLatin1(ch).at(0).unicode());
    expected << s("65293");
    QCOMPARE(pressed(), expected);

    // 3. The window changes between the confirmation sheet and the tap: target-changed, nothing typed.
    portal.calls.clear();
    QCOMPARE(publish(s("{win-1}"), 4, s("Sign in")).type(), QDBusMessage::ReplyMessage);
    say(s("TYPE password"));
    QVERIFY(readLine(&line));
    QVERIFY(line.startsWith(s("CONFIRM ")));
    QCOMPARE(publish(s("{win-2}"), 5, s("Other")).type(), QDBusMessage::ReplyMessage);
    say(s("CONFIRM"));
    QVERIFY(readLine(&line));
    QVERIFY2(line.startsWith(s("RESULT failed")) && line.contains(s("nothing more was typed")), qPrintable(line));
    QTest::qWait(100);
    QCOMPARE(portal.calls.size(), 0);

    // 4. The shell goes away (focus unknown) between sheet and tap: unverified.
    QCOMPARE(publish(s("{win-2}"), 6, s("Other")).type(), QDBusMessage::ReplyMessage);
    say(s("TYPE username"));
    QVERIFY(readLine(&line));
    QVERIFY(line.startsWith(s("CONFIRM ")));
    QCOMPARE(publishNone().type(), QDBusMessage::ReplyMessage);
    say(s("CONFIRM"));
    QVERIFY(readLine(&line));
    QVERIFY2(line.startsWith(s("RESULT failed")) && line.contains(s("cannot tell which window")), qPrintable(line));
    QCOMPARE(portal.calls.size(), 0);

    say(s("QUIT"));
    phone.waitForFinished(8000);
    if (phone.state() != QProcess::NotRunning) phone.kill(); // our own child, by handle
    service.terminate();
    QVERIFY(service.waitForFinished(5000));
}

QTEST_GUILESS_MAIN(TstLinkVaultCross)
#include "tst_link_vault_cross.moc"
