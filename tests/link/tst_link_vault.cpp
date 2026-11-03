// SPDX-License-Identifier: GPL-3.0-or-later
// QindaLink desktop.focus and vault.type (ADR-0440) against a fake RemoteDesktop portal and a fake focus
// publisher. Only throwaway test strings are typed; they are never printed, only their lengths.
#include "support/vault_fixture.h"

#include <qindaqt/services/link_input/text_typer.h>
#include <qindaqt/services/link_vault/secret_plan.h>

#include <QtCore/QLoggingCategory>
#include <QtTest/QTest>

using namespace QindaQt::Link;
using namespace QindaQt::Link::Test;

namespace {
QString s(const char *v) { return QString::fromLatin1(v); }
QJsonObject typeBody(const QString &token, const QJsonArray &steps) { return VaultFixture::typeBody(token, steps); }
}

class TstLinkVault : public QObject {
    Q_OBJECT
private slots:
    void planBounds();
    void planSkipsControlCharactersAndNeverMakesKeys();
    void focusStoreValidatesAndSanitizes();
    void focusThenTypeDeliversKeysyms();
    void focusRefusalsAreTruthful();
    void tokenExpiryMismatchAndSingleUse();
    void focusChangeBetweenConfirmAndTypeIsTargetChanged();
    void focusChangeMidTypingAbortsAndReleases();
    void lockMidTypingAbortsWithLocked();
    void oneJobAtATimeAndBoundsAreEnforced();
    void capabilityIsRequired();
    void portalApprovalIsRequestedAndClosedAfterwards();
    void deniedPortalIsReported();
    void sharedPortalStaysOpenForTheInputController();
    void sessionCloseMidTypingAbortsWithoutReply();
    void secretsNeverReachTheLog();
    void typerWipesSecretSymbols();
};

void TstLinkVault::planBounds()
{
    QString token;
    auto ok = [&](const QJsonArray &steps) { return SecretPlan::parse(typeBody(s("t"), steps), &token).has_value(); };
    QVERIFY(ok({VaultFixture::text(s("abc"))}));
    QCOMPARE(token, s("t"));
    QJsonArray sixteen;
    for (int i = 0; i < 16; ++i) sixteen.append(VaultFixture::key(s("Tab")));
    QVERIFY(ok(sixteen));
    sixteen.append(VaultFixture::key(s("Tab")));
    QVERIFY(!ok(sixteen));
    QVERIFY(ok({VaultFixture::text(QString(1024, QLatin1Char('a')))}));
    QVERIFY(!ok({VaultFixture::text(QString(1025, QLatin1Char('a')))}));
    QVERIFY(ok({VaultFixture::text(QString(512, QChar(0x20ac)).left(341))}));      // 1023 bytes
    QVERIFY(!ok({VaultFixture::text(QString(342, QChar(0x20ac)))}));               // 1026 bytes
    for (const char *k : {"Tab", "Enter", "Escape", "Left", "Right", "Up", "Down"}) QVERIFY2(ok({VaultFixture::key(s(k))}), k);
    for (const char *k : {"Return", "F1", "Ctrl", "BackSpace", "tab", ""}) QVERIFY2(!ok({VaultFixture::key(s(k))}), k);
    QVERIFY(ok({VaultFixture::text(s("a")), VaultFixture::delay(2000)}));
    QVERIFY(!ok({VaultFixture::text(s("a")), VaultFixture::delay(2001)}));
    QVERIFY(!ok({VaultFixture::text(s("a")), VaultFixture::delay(-1)}));
    QVERIFY(!ok({VaultFixture::text(s("a")), QJsonObject{{s("delayMs"), 1.5}}}));
    QVERIFY(!ok({VaultFixture::delay(100)}));                       // nothing to type
    QVERIFY(!ok({}));                                              // no steps
    QVERIFY(!ok({QJsonObject{{s("text"), s("a")}, {s("key"), s("Tab")}}})); // two kinds in one step
    QVERIFY(!ok({QJsonObject{{s("text"), 5}}}));
    QVERIFY(!ok({QJsonObject{{s("text"), s("")}}}));
    QVERIFY(!SecretPlan::parse(QJsonObject{{s("token"), s("t")}}, &token));
    QVERIFY(!SecretPlan::parse(QJsonObject{{s("token"), 1}, {s("steps"), QJsonArray{VaultFixture::text(s("a"))}}}, &token));
    QVERIFY(!SecretPlan::parse(QJsonObject{{s("token"), s("t")}, {s("steps"), QJsonArray{VaultFixture::text(s("a"))}}, {s("x"), 1}}, &token));
}

void TstLinkVault::planSkipsControlCharactersAndNeverMakesKeys()
{
    QString token;
    auto plan = SecretPlan::parse(typeBody(s("t"), {VaultFixture::text(QStringLiteral("a\tb\nc\r\x01\x7f\u0085z"))}), &token);
    QVERIFY(plan);
    QCOMPARE(plan->size(), 1);
    // Only a, b, c, z remain: tab and newline are never keys inside text.
    QCOMPARE(plan->at(0).symbols, (QList<qint32>{'a', 'b', 'c', 'z'}));
    // A text of only control characters is dropped; with nothing else the plan is refused.
    QVERIFY(!SecretPlan::parse(typeBody(s("t"), {VaultFixture::text(QStringLiteral("\t\n"))}), &token));
    plan = SecretPlan::parse(typeBody(s("t"), {VaultFixture::text(QStringLiteral("\t")), VaultFixture::key(s("Enter"))}), &token);
    QVERIFY(plan);
    QCOMPARE(plan->size(), 1);
    QCOMPARE(plan->at(0).keysym, 0xff0d);
    plan->wipe();
    QCOMPARE(plan->size(), 0);
}

void TstLinkVault::focusStoreValidatesAndSanitizes()
{
    FocusStore store;
    QSignalSpy changed(&store, &FocusStore::changed);
    auto pub = [&](const QByteArray &j) { return store.publish(j); };
    QCOMPARE(pub("{\"uuid\":\"{a-1}\",\"appId\":\"org.x\",\"title\":\"T\",\"revision\":4}"), FocusStore::Result::Accepted);
    QCOMPARE(changed.size(), 1);
    QCOMPARE(pub("{\"uuid\":\"{a-1}\",\"appId\":\"org.x\",\"title\":\"T\",\"revision\":4}"), FocusStore::Result::Accepted);
    QCOMPARE(changed.size(), 1); // identical: no change signal
    QCOMPARE(store.current()->revision, quint64(4));
    // Rejections leave the previous fact untouched.
    for (const char *bad : {"", "[]", "{\"uuid\":\"\",\"appId\":\"a\",\"title\":\"t\",\"revision\":1}",
                            "{\"uuid\":\"has space\",\"appId\":\"a\",\"title\":\"t\",\"revision\":1}",
                            "{\"uuid\":\"u\",\"appId\":\"a\",\"title\":\"t\",\"revision\":-1}",
                            "{\"uuid\":\"u\",\"appId\":\"a\",\"title\":\"t\",\"revision\":1.5}",
                            "{\"uuid\":\"u\",\"appId\":\"a\",\"title\":7,\"revision\":1}",
                            "{\"uuid\":\"u\",\"appId\":\"a\",\"revision\":1}"}) {
        QCOMPARE(pub(bad), FocusStore::Result::Rejected);
    }
    QCOMPARE(pub(QByteArray(FocusStore::kMaxJsonBytes + 1, ' ')), FocusStore::Result::Rejected);
    QCOMPARE(store.current()->uuid, s("{a-1}"));
    // Titles are display text: controls and bidi overrides become spaces, length is capped.
    QCOMPARE(pub(QJsonDocument(QJsonObject{{s("uuid"), s("u")}, {s("appId"), s("a")},
                                           {s("title"), QStringLiteral("ev") + QChar(0x202e) + QStringLiteral("il\x01title") + QString(500, QLatin1Char('x'))},
                                           {s("revision"), 5}}).toJson()),
             FocusStore::Result::Accepted);
    QVERIFY(!store.current()->title.contains(QChar(0x202e)));
    QVERIFY(!store.current()->title.contains(QChar(1)));
    QVERIFY(store.current()->title.size() <= FocusStore::kMaxTitleChars);
    QCOMPARE(pub("{}"), FocusStore::Result::Cleared);
    QVERIFY(!store.current());
    QCOMPARE(pub("{\"none\":true}"), FocusStore::Result::Cleared);
}

void TstLinkVault::focusThenTypeDeliversKeysyms()
{
    VaultFixture f;
    QVERIFY(f.usable());
    ScriptedPhone phone;
    QVERIFY(f.pairPhone(phone));
    QVERIFY(f.publish(s("{w1}"), 7, s("Login - Browser"), s("org.example.browser")));
    QString app, title;
    const QString token = VaultFixture::askFocus(phone, 3, &app, &title);
    QVERIFY(!token.isEmpty());
    QCOMPARE(app, s("org.example.browser"));
    QCOMPARE(title, s("Login - Browser"));
    phone.send(VaultFixture::frame(s("vault.type"),
                                   typeBody(token, {VaultFixture::text(QStringLiteral("ab€")), VaultFixture::key(s("Tab")),
                                                    VaultFixture::delay(20), VaultFixture::text(s("Z")),
                                                    VaultFixture::key(s("Enter"))}), 4));
    Envelope ok;
    QVERIFY(phone.waitFor(s("ok"), &ok));
    QCOMPARE(ok.re, 4);
    QCOMPARE(f.pressed(), (QList<qint64>{'a', 'b', 0x010020ac, 0xff09, 'Z', 0xff0d}));
    QVERIFY(f.nothingHeld());
    QVERIFY(!f.vault.typing());
    // The session this feature opened is closed again (no lingering remote-control indicator).
    QVERIFY(waitUntil([&] { return !f.fakePortal.sessionOpen(); }));
}

void TstLinkVault::focusRefusalsAreTruthful()
{
    VaultFixture f;
    QVERIFY(f.usable());
    ScriptedPhone phone;
    QVERIFY(f.pairPhone(phone));
    // Nobody has published: focus is unknown.
    phone.send(VaultFixture::frame(s("desktop.focus"), {}, 1));
    Envelope e;
    QVERIFY(phone.waitFor(s("error"), &e));
    QCOMPARE(e.re, 1);
    QCOMPARE(e.b.value(s("code")).toString(), s("unverified"));
    QVERIFY(f.publish(s("{w}"), 1));
    f.gate.setOpen(false); // Locked
    phone.send(VaultFixture::frame(s("desktop.focus"), {}, 2));
    QVERIFY(phone.waitFor(s("error"), &e));
    QCOMPARE(e.b.value(s("code")).toString(), s("locked"));
    f.gate.setReason(InputGate::Reason::Unverified);
    phone.send(VaultFixture::frame(s("desktop.focus"), {}, 3));
    QVERIFY(phone.waitFor(s("error"), &e));
    QCOMPARE(e.b.value(s("code")).toString(), s("unverified"));
    // A focus fact that went away (the shell left) is unknown again.
    f.gate.setOpen(true);
    const QString token = VaultFixture::askFocus(phone, 4);
    QVERIFY(!token.isEmpty());
    f.focus.clear();
    phone.send(VaultFixture::frame(s("vault.type"), typeBody(token, {VaultFixture::text(s("x"))}), 5));
    QVERIFY(phone.waitFor(s("error"), &e));
    QCOMPARE(e.re, 5);
    QCOMPARE(e.b.value(s("code")).toString(), s("unverified"));
    // And the gate closing between confirm and type is reported as such.
    QVERIFY(f.publish(s("{w}"), 1));
    const QString t2 = VaultFixture::askFocus(phone, 6);
    f.gate.setOpen(false);
    phone.send(VaultFixture::frame(s("vault.type"), typeBody(t2, {VaultFixture::text(s("x"))}), 7));
    QVERIFY(phone.waitFor(s("error"), &e));
    QCOMPARE(e.b.value(s("code")).toString(), s("locked"));
    QVERIFY(f.pressed().isEmpty());
    QVERIFY(!f.fakePortal.sessionOpen()); // a refusal never raises the portal
}

void TstLinkVault::tokenExpiryMismatchAndSingleUse()
{
    VaultFixture f;
    QVERIFY(f.usable());
    ScriptedPhone phone;
    QVERIFY(f.pairPhone(phone));
    QVERIFY(f.publish(s("{w}"), 1));
    Envelope e;
    auto attempt = [&](const QString &token, qint64 id) {
        phone.send(VaultFixture::frame(s("vault.type"), typeBody(token, {VaultFixture::text(s("q"))}), id));
        return phone.waitFor(s("ok"), &e) ? QString() : e.b.value(s("code")).toString();
    };
    // Unknown token.
    QVERIFY(!VaultFixture::askFocus(phone, 1).isEmpty());
    phone.send(VaultFixture::frame(s("vault.type"), typeBody(s("not-the-token"), {VaultFixture::text(s("q"))}), 2));
    QVERIFY(phone.waitFor(s("error"), &e));
    QCOMPARE(e.b.value(s("code")).toString(), s("target-changed"));
    // The mismatch consumed the token: the real one is gone too.
    // Expiry.
    f.vault.setTokenLifetime(50);
    QString token = VaultFixture::askFocus(phone, 3);
    QTest::qWait(120);
    phone.send(VaultFixture::frame(s("vault.type"), typeBody(token, {VaultFixture::text(s("q"))}), 4));
    QVERIFY(phone.waitFor(s("error"), &e));
    QCOMPARE(e.b.value(s("code")).toString(), s("target-changed"));
    f.vault.setTokenLifetime(VaultTypingFeature::kTokenLifetimeMs);
    // Single use.
    token = VaultFixture::askFocus(phone, 5);
    QCOMPARE(attempt(token, 6), QString());
    f.fakePortal.calls.clear();
    phone.send(VaultFixture::frame(s("vault.type"), typeBody(token, {VaultFixture::text(s("q"))}), 7));
    QVERIFY(phone.waitFor(s("error"), &e));
    QCOMPARE(e.re, 7);
    QCOMPARE(e.b.value(s("code")).toString(), s("target-changed"));
    QVERIFY(f.pressed().isEmpty());
    // A newer desktop.focus replaces the older token.
    const QString first = VaultFixture::askFocus(phone, 8);
    const QString second = VaultFixture::askFocus(phone, 9);
    QVERIFY(first != second);
    phone.send(VaultFixture::frame(s("vault.type"), typeBody(first, {VaultFixture::text(s("q"))}), 10));
    QVERIFY(phone.waitFor(s("error"), &e));
    QCOMPARE(e.b.value(s("code")).toString(), s("target-changed"));
    // A token never works for another phone.
    ScriptedPhone other(generateIdentity(QStringLiteral("other-phone")));
    QVERIFY(f.pairPhone(other));
    const QString mine = VaultFixture::askFocus(phone, 11);
    other.send(VaultFixture::frame(s("vault.type"), typeBody(mine, {VaultFixture::text(s("q"))}), 12));
    QVERIFY(other.waitFor(s("error"), &e));
    QCOMPARE(e.b.value(s("code")).toString(), s("target-changed"));
}

void TstLinkVault::focusChangeBetweenConfirmAndTypeIsTargetChanged()
{
    VaultFixture f;
    QVERIFY(f.usable());
    ScriptedPhone phone;
    QVERIFY(f.pairPhone(phone));
    QVERIFY(f.publish(s("{w}"), 1));
    Envelope e;
    // Another window.
    QString token = VaultFixture::askFocus(phone, 1);
    QVERIFY(f.publish(s("{other}"), 2));
    phone.send(VaultFixture::frame(s("vault.type"), typeBody(token, {VaultFixture::text(s("q"))}), 2));
    QVERIFY(phone.waitFor(s("error"), &e));
    QCOMPARE(e.b.value(s("code")).toString(), s("target-changed"));
    // The same window with a newer revision (a new focus generation) also counts as changed...
    QVERIFY(f.publish(s("{w}"), 3));
    token = VaultFixture::askFocus(phone, 3);
    QVERIFY(f.publish(s("{w}"), 4));
    phone.send(VaultFixture::frame(s("vault.type"), typeBody(token, {VaultFixture::text(s("q"))}), 4));
    QVERIFY(phone.waitFor(s("error"), &e));
    QCOMPARE(e.b.value(s("code")).toString(), s("target-changed"));
    // ...but a title change alone (same uuid and revision) does not.
    QVERIFY(f.publish(s("{w}"), 5, s("Old title")));
    token = VaultFixture::askFocus(phone, 5);
    QVERIFY(f.publish(s("{w}"), 5, s("New title")));
    phone.send(VaultFixture::frame(s("vault.type"), typeBody(token, {VaultFixture::text(s("q"))}), 6));
    QVERIFY(phone.waitFor(s("ok"), &e));
    QVERIFY(f.nothingHeld());
}

void TstLinkVault::focusChangeMidTypingAbortsAndReleases()
{
    VaultFixture f;
    QVERIFY(f.usable());
    ScriptedPhone phone;
    QVERIFY(f.pairPhone(phone));
    QVERIFY(f.publish(s("{w}"), 1));
    const QString token = VaultFixture::askFocus(phone, 1);
    // 4 x 1024 symbols is about 700 ms of batches, long enough to interleave a focus change.
    phone.send(VaultFixture::frame(s("vault.type"),
                                   typeBody(token, {VaultFixture::text(QString(1024, QLatin1Char('k'))),
                                                    VaultFixture::text(QString(1024, QLatin1Char('m'))),
                                                    VaultFixture::text(QString(1024, QLatin1Char('n'))),
                                                    VaultFixture::text(QString(1024, QLatin1Char('p')))}), 2));
    QVERIFY(waitUntil([&] { return f.pressed().size() >= 30; }));
    QVERIFY(f.publish(s("{elsewhere}"), 2));
    Envelope e;
    QVERIFY(phone.waitFor(s("error"), &e));
    QCOMPARE(e.re, 2);
    QCOMPARE(e.b.value(s("code")).toString(), s("target-changed"));
    const qsizetype typedAtAbort = f.pressed().size();
    QVERIFY(typedAtAbort < 4096);
    QTest::qWait(150);
    QCOMPARE(f.pressed().size(), typedAtAbort); // nothing more after the abort
    QVERIFY(f.nothingHeld());
    QVERIFY(!f.vault.typing());
    QVERIFY(!f.pressed().contains('n') && !f.pressed().contains('p')); // later steps never started
}

void TstLinkVault::lockMidTypingAbortsWithLocked()
{
    VaultFixture f;
    QVERIFY(f.usable());
    ScriptedPhone phone;
    QVERIFY(f.pairPhone(phone));
    QVERIFY(f.publish(s("{w}"), 1));
    const QString token = VaultFixture::askFocus(phone, 1);
    phone.send(VaultFixture::frame(s("vault.type"),
                                   typeBody(token, {VaultFixture::text(QString(1024, QLatin1Char('k'))),
                                                    VaultFixture::text(QString(1024, QLatin1Char('m')))}), 2));
    QVERIFY(waitUntil([&] { return f.pressed().size() >= 20; }));
    f.gate.setOpen(false);
    Envelope e;
    QVERIFY(phone.waitFor(s("error"), &e));
    QCOMPARE(e.b.value(s("code")).toString(), s("locked"));
    QVERIFY(f.nothingHeld());
}

void TstLinkVault::oneJobAtATimeAndBoundsAreEnforced()
{
    VaultFixture f;
    QVERIFY(f.usable());
    ScriptedPhone phone;
    QVERIFY(f.pairPhone(phone));
    QVERIFY(f.publish(s("{w}"), 1));
    QString token = VaultFixture::askFocus(phone, 1);
    phone.send(VaultFixture::frame(s("vault.type"), typeBody(token, {VaultFixture::text(QString(1024, QLatin1Char('k'))),
                                                                     VaultFixture::delay(200)}), 2));
    QVERIFY(waitUntil([&] { return f.vault.typing(); }));
    const QString token2 = VaultFixture::askFocus(phone, 3);
    phone.send(VaultFixture::frame(s("vault.type"), typeBody(token2, {VaultFixture::text(s("z"))}), 4));
    Envelope e;
    QVERIFY(phone.waitFor(s("error"), &e));
    QCOMPARE(e.re, 4);
    QCOMPARE(e.b.value(s("code")).toString(), s("busy"));
    QVERIFY(phone.waitFor(s("ok"), &e));
    QCOMPARE(e.re, 2);
    // Over-bound requests are bad-request and consume nothing.
    token = VaultFixture::askFocus(phone, 5);
    phone.send(VaultFixture::frame(s("vault.type"), typeBody(token, {VaultFixture::text(QString(1025, QLatin1Char('a')))}), 6));
    QVERIFY(phone.waitFor(s("error"), &e));
    QCOMPARE(e.b.value(s("code")).toString(), s("bad-request"));
    phone.send(VaultFixture::frame(s("vault.type"), typeBody(token, {VaultFixture::text(s("ok"))}), 7));
    QVERIFY(phone.waitFor(s("ok"), &e));
    QCOMPARE(e.re, 7);
}

void TstLinkVault::capabilityIsRequired()
{
    VaultFixture f;
    QVERIFY(f.usable());
    ScriptedPhone phone;
    QVERIFY(f.pairPhone(phone, {QStringLiteral("status")}));
    QVERIFY(f.publish(s("{w}"), 1));
    Envelope e;
    phone.send(VaultFixture::frame(s("desktop.focus"), {}, 1));
    QVERIFY(phone.waitFor(s("error"), &e));
    QCOMPARE(e.b.value(s("code")).toString(), s("denied"));
    phone.send(VaultFixture::frame(s("vault.type"), typeBody(s("t"), {VaultFixture::text(s("a"))}), 2));
    QVERIFY(phone.waitFor(s("error"), &e));
    QCOMPARE(e.b.value(s("code")).toString(), s("denied"));
    QVERIFY(f.pressed().isEmpty());
}

void TstLinkVault::portalApprovalIsRequestedAndClosedAfterwards()
{
    VaultFixture f;
    QVERIFY(f.usable());
    ScriptedPhone phone;
    QVERIFY(f.pairPhone(phone));
    QVERIFY(f.publish(s("{w}"), 1));
    QVERIFY(!f.fakePortal.sessionOpen());
    const QString token = VaultFixture::askFocus(phone, 1);
    QCOMPARE(f.fakePortal.prompts, 0); // asking for the focus never prompts
    phone.send(VaultFixture::frame(s("vault.type"), typeBody(token, {VaultFixture::text(s("ab"))}), 2));
    Envelope e;
    QVERIFY(phone.waitFor(s("ok"), &e));
    QCOMPARE(f.fakePortal.prompts, 1);
    QCOMPARE(f.pressed(), (QList<qint64>{'a', 'b'}));
    QVERIFY(waitUntil([&] { return !f.fakePortal.sessionOpen(); }));
}

void TstLinkVault::deniedPortalIsReported()
{
    VaultFixture f;
    QVERIFY(f.usable());
    f.fakePortal.mode = FakePortal::Mode::DenyAtStart;
    ScriptedPhone phone;
    QVERIFY(f.pairPhone(phone));
    QVERIFY(f.publish(s("{w}"), 1));
    const QString token = VaultFixture::askFocus(phone, 1);
    phone.send(VaultFixture::frame(s("vault.type"), typeBody(token, {VaultFixture::text(s("ab"))}), 2));
    Envelope e;
    QVERIFY(phone.waitFor(s("error"), &e));
    QCOMPARE(e.re, 2);
    QCOMPARE(e.b.value(s("code")).toString(), s("denied"));
    QVERIFY(f.pressed().isEmpty());
    QVERIFY(!f.vault.typing());
}

void TstLinkVault::sharedPortalStaysOpenForTheInputController()
{
    VaultFixture f;
    QVERIFY(f.usable());
    ScriptedPhone phone;
    QVERIFY(f.pairPhone(phone, {QStringLiteral("vault"), QStringLiteral("input")}));
    phone.send(VaultFixture::frame(s("input.begin"), {}, 9));
    Envelope e;
    QVERIFY(phone.waitFor(s("ok"), &e));
    QVERIFY(f.fakePortal.sessionOpen());
    QVERIFY(f.publish(s("{w}"), 1));
    const QString token = VaultFixture::askFocus(phone, 1);
    phone.send(VaultFixture::frame(s("vault.type"), typeBody(token, {VaultFixture::text(s("hi"))}), 2));
    QVERIFY(phone.waitFor(s("ok"), &e));
    QCOMPARE(e.re, 2);
    QCOMPARE(f.fakePortal.prompts, 1); // the controller's approval served both
    QTest::qWait(100);
    QVERIFY(f.fakePortal.sessionOpen()); // the controller still owns the session
    QCOMPARE(f.pressed(), (QList<qint64>{'h', 'i'}));
}

void TstLinkVault::sessionCloseMidTypingAbortsWithoutReply()
{
    VaultFixture f;
    QVERIFY(f.usable());
    {
        ScriptedPhone phone;
        QVERIFY(f.pairPhone(phone));
        QVERIFY(f.publish(s("{w}"), 1));
        const QString token = VaultFixture::askFocus(phone, 1);
        phone.send(VaultFixture::frame(s("vault.type"), typeBody(token, {VaultFixture::text(QString(1024, QLatin1Char('k'))),
                                                                         VaultFixture::text(QString(1024, QLatin1Char('m')))}), 2));
        QVERIFY(waitUntil([&] { return f.pressed().size() >= 10; }));
    } // the phone disconnects here
    QVERIFY(waitUntil([&] { return !f.vault.typing(); }));
    const qsizetype typed = f.pressed().size();
    QTest::qWait(100);
    QCOMPARE(f.pressed().size(), typed);
    QVERIFY(f.nothingHeld());
}

void TstLinkVault::secretsNeverReachTheLog()
{
    static QStringList logged;
    logged.clear();
    QLoggingCategory::setFilterRules(QStringLiteral("qindaqt.link.frames.debug=true"));
    const QtMessageHandler previous = qInstallMessageHandler([](QtMsgType, const QMessageLogContext &, const QString &m) { logged << m; });
    {
        VaultFixture f;
        QVERIFY(f.usable());
        ScriptedPhone phone;
        QVERIFY(f.pairPhone(phone));
        QVERIFY(f.publish(s("{w}"), 1));
        const QString token = VaultFixture::askFocus(phone, 1);
        const QString secret = QStringLiteral("zq-throwaway-Secret-9!é€");
        phone.send(VaultFixture::frame(s("vault.type"), typeBody(token, {VaultFixture::text(secret)}), 2));
        Envelope e;
        QVERIFY(phone.waitFor(s("ok"), &e));
        // A refusal path: its error message must not echo the request either.
        phone.send(VaultFixture::frame(s("vault.type"), typeBody(s("wrong"), {VaultFixture::text(secret)}), 3));
        QVERIFY(phone.waitFor(s("error"), &e));
        QVERIFY(!e.b.value(s("message")).toString().contains(s("Secret")));
    }
    qInstallMessageHandler(previous);
    QLoggingCategory::setFilterRules(QStringLiteral("qindaqt.link.frames.debug=false"));
    QVERIFY(logged.size() > 3); // the frame log was really on, or this proves nothing
    bool sawRedaction = false;
    for (const QString &line : std::as_const(logged)) {
        QVERIFY2(!line.contains(s("Secret")) && !line.contains(s("throwaway")) && !line.contains(QStringLiteral("€")),
                 "secret text reached a log line");
        sawRedaction = sawRedaction || line.contains(s("vault.type (body redacted"));
    }
    QVERIFY(sawRedaction);
}

void TstLinkVault::typerWipesSecretSymbols()
{
    // zero() overwrites the buffer in place (size kept, so the zeros are observable); wipe() then drops it.
    QList<qint32> symbols{'p', 'w', 'x'};
    TextTyper::zero(symbols);
    QCOMPARE(symbols, (QList<qint32>{0, 0, 0}));
    QList<qint32> copy{1, 2, 3};
    QList<qint32> shared = copy;
    TextTyper::wipe(shared);
    QCOMPARE(copy, (QList<qint32>{1, 2, 3})); // wiping a shared list never touches the other owner
    QVERIFY(shared.isEmpty());
}

QTEST_GUILESS_MAIN(TstLinkVault)
#include "tst_link_vault.moc"
