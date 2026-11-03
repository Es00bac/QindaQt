// SPDX-License-Identifier: GPL-3.0-or-later
// QindaLink input.* against a fake RemoteDesktop portal on the test bus. The
// fake registers under a unique name, so nothing here can reach the real portal.
#include "support/input_fixture.h"

#include <qindaqt/services/link_input/input_frames.h>

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtTest/QTest>

using namespace QindaQt::Link;
using namespace QindaQt::Link::Test;

namespace {
QJsonObject o(std::initializer_list<std::pair<QString, QJsonValue>> kv)
{
    QJsonObject r;
    for (const auto &p : kv) r.insert(p.first, p.second);
    return r;
}
QString s(const char *v) { return QString::fromLatin1(v); }
}

class TstLinkInput : public QObject {
    Q_OBJECT
private slots:
    void parsesAndBoundsFrames();
    void beginApprovesAndMapsEveryFrame();
    void secondBeginReusesRestoreToken();
    void deniedAtStartReplies();
    void unavailablePortalReplies();
    void revokeEndsControlAndNextBeginPromptsAgain();
    void releasesHeldInputOnDisconnect();
    void releasesOnEndAndOnLock();
    void onlyTheControllerDrivesInput();
    void invalidFramesAreRejectedOrDropped();
    void textIsBatchedAndNeverLogged();
    void frameBeforeBeginIsDropped();
    void lockMidTextReplyAndNextTextWorks();
    void watchdogAndBeginReleaseStuckKeys();
    void keyPressesAreRateLimitedButReleasesAreNot();
    void sessionControlKeysymsAndC1TextAreRefused();
    void lockedBeginDoesNotClaimTheController();
    void refusedSelectDevicesClearsTheToken();
    void shutdownReleasesBeforeClosing();
    void gateReasonsAreReportedTruthfully();
};

void TstLinkInput::parsesAndBoundsFrames()
{
    using namespace InputFrames;
    QVERIFY(parse(s("input.pointer"), o({{s("dx"), 3}, {s("dy"), -4}})));
    QVERIFY(!parse(s("input.pointer"), o({{s("dx"), 3000}, {s("dy"), 0}})));
    QVERIFY(!parse(s("input.pointer"), o({{s("dx"), 1}, {s("dy"), QStringLiteral("2")}})));
    QVERIFY(!parse(s("input.pointer"), o({{s("dx"), 1}})));
    QVERIFY(parse(s("input.button"), o({{s("button"), s("left")}, {s("state"), s("click")}})));
    QVERIFY(!parse(s("input.button"), o({{s("button"), s("back")}, {s("state"), s("down")}})));
    QVERIFY(parse(s("input.scroll"), o({{s("mode"), s("discrete")}, {s("dx"), 0}, {s("dy"), 3}})));
    QVERIFY(!parse(s("input.scroll"), o({{s("mode"), s("discrete")}, {s("dx"), 0}, {s("dy"), 2.5}})));
    QVERIFY(!parse(s("input.scroll"), o({{s("mode"), s("smooth")}, {s("dx"), 0}, {s("dy"), 501}})));
    QVERIFY(parse(s("input.key"), o({{s("name"), s("Escape")}, {s("state"), s("tap")}})));
    QVERIFY(parse(s("input.key"), o({{s("keysym"), 0x01000000 | 0x20ac}, {s("state"), s("down")}})));
    QVERIFY(!parse(s("input.key"), o({{s("keysym"), 7}, {s("state"), s("down")}})));
    QVERIFY(!parse(s("input.key"), o({{s("keysym"), 65}, {s("name"), s("Tab")}, {s("state"), s("down")}})));
    QVERIFY(!parse(s("input.key"), o({{s("name"), s("Bogus")}, {s("state"), s("down")}})));
    QVERIFY(parse(s("input.text"), o({{s("text"), s("hi")}})));
    QVERIFY(!parse(s("input.text"), o({{s("text"), QString(4097, QLatin1Char('a'))}})));
    QVERIFY(!parse(s("input.text"), o({{s("text"), QString()}})));
    QCOMPARE(keysymForCodepoint(0xe9), 0xe9);
    QCOMPARE(keysymForCodepoint(0x20ac), 0x010020ac);
}

void TstLinkInput::beginApprovesAndMapsEveryFrame()
{
    InputFixture f;
    QVERIFY(f.usable());
    ScriptedPhone phone;
    QVERIFY(f.pairPhone(phone));
    InputFixture::begin(phone, 5);
    Envelope reply;
    QVERIFY(phone.waitFor(s("ok"), &reply));
    QCOMPARE(reply.re, 5);
    QVERIFY(waitUntil([&] { return phone.count(s("input.state")) >= 2; }));
    QCOMPARE(f.fakePortal.calls.mid(0, 3), (QStringList{s("CreateSession"), s("SelectDevices:types=3:persist=2"), s("Start")}));
    QCOMPARE(f.fakePortal.prompts, 1);
    QVERIFY(f.input.flowing());
    // The restore token is kept private.
    QVERIFY(waitUntil([&] { return QFileInfo::exists(f.tokenFile()); }));
    QCOMPARE(QFileInfo(f.tokenFile()).permissions() & (QFileDevice::ReadGroup | QFileDevice::ReadOther | QFileDevice::WriteGroup | QFileDevice::WriteOther), QFileDevice::Permissions());
    QFile token(f.tokenFile());
    QVERIFY(token.open(QIODevice::ReadOnly));
    QCOMPARE(token.readAll().trimmed(), QByteArray("fake-token-1"));
    // The states the phone saw, in order.
    QStringList states;
    for (const Envelope &e : phone.all()) {
        if (e.t == QLatin1String("input.state")) states << e.b.value(s("state")).toString();
    }
    QCOMPARE(states, (QStringList{s("pending"), s("active")}));

    f.fakePortal.calls.clear();
    phone.send(InputFixture::frame(s("input.pointer"), o({{s("dx"), 3.5}, {s("dy"), -2}})));
    phone.send(InputFixture::frame(s("input.button"), o({{s("button"), s("left")}, {s("state"), s("click")}})));
    phone.send(InputFixture::frame(s("input.button"), o({{s("button"), s("right")}, {s("state"), s("down")}})));
    phone.send(InputFixture::frame(s("input.button"), o({{s("button"), s("right")}, {s("state"), s("up")}})));
    phone.send(InputFixture::frame(s("input.scroll"), o({{s("mode"), s("smooth")}, {s("dx"), 0}, {s("dy"), 12.5}, {s("end"), true}})));
    phone.send(InputFixture::frame(s("input.scroll"), o({{s("mode"), s("discrete")}, {s("dx"), -1}, {s("dy"), 3}})));
    phone.send(InputFixture::frame(s("input.key"), o({{s("name"), s("Escape")}, {s("state"), s("tap")}})));
    phone.send(InputFixture::frame(s("input.key"), o({{s("keysym"), 0x61}, {s("state"), s("down")}})));
    phone.send(InputFixture::frame(s("input.key"), o({{s("keysym"), 0x61}, {s("state"), s("up")}})));
    QVERIFY(waitUntil([&] { return f.fakePortal.calls.size() >= 12; }));
    QCOMPARE(f.fakePortal.calls,
             (QStringList{s("NotifyPointerMotion:3.5:-2"), s("NotifyPointerButton:272:1"), s("NotifyPointerButton:272:0"),
                          s("NotifyPointerButton:273:1"), s("NotifyPointerButton:273:0"),
                          s("NotifyPointerAxis:0:12.5:1"), s("NotifyPointerAxisDiscrete:0:3"),
                          s("NotifyPointerAxisDiscrete:1:-1"), s("NotifyKeyboardKeysym:65307:1"),
                          s("NotifyKeyboardKeysym:65307:0"), s("NotifyKeyboardKeysym:97:1"), s("NotifyKeyboardKeysym:97:0")}));
}

void TstLinkInput::secondBeginReusesRestoreToken()
{
    InputFixture f;
    QVERIFY(f.usable());
    ScriptedPhone phone;
    QVERIFY(f.pairPhone(phone));
    InputFixture::begin(phone, 1);
    QVERIFY(phone.waitFor(s("ok")));
    phone.send(InputFixture::frame(s("input.end"), {}, 2));
    Envelope ok;
    QVERIFY(phone.waitFor(s("ok"), &ok));
    QCOMPARE(ok.re, 2);
    QVERIFY(waitUntil([&] { return f.callSeen(s("Close")); }));
    QVERIFY(!f.input.flowing());
    InputFixture::begin(phone, 3);
    QVERIFY(phone.waitFor(s("ok"), &ok));
    QCOMPARE(ok.re, 3);
    QCOMPARE(f.fakePortal.lastSelect.value(s("restore_token")).toString(), s("fake-token-1"));
    QCOMPARE(f.fakePortal.prompts, 1); // the second start was a silent restore
}

void TstLinkInput::deniedAtStartReplies()
{
    InputFixture f;
    QVERIFY(f.usable());
    f.fakePortal.mode = FakePortal::Mode::DenyAtStart;
    ScriptedPhone phone;
    QVERIFY(f.pairPhone(phone));
    InputFixture::begin(phone, 8);
    Envelope err;
    QVERIFY(phone.waitFor(s("error"), &err));
    QCOMPARE(err.re, 8);
    QCOMPARE(err.b.value(s("code")).toString(), s("denied"));
    QVERIFY(waitUntil([&] { return phone.count(s("input.state")) >= 2; }));
    QCOMPARE(phone.all().last().b.value(s("state")).toString(), s("denied"));
    QVERIFY(!f.input.flowing());
    QVERIFY(f.input.controller().isEmpty());
    phone.send(InputFixture::frame(s("input.pointer"), o({{s("dx"), 1}, {s("dy"), 1}})));
    QTest::qWait(100);
    QVERIFY(!f.callSeen(s("NotifyPointerMotion")));
}

void TstLinkInput::unavailablePortalReplies()
{
    InputFixture f;
    QVERIFY(f.usable());
    f.fakePortal.mode = FakePortal::Mode::FailCreate;
    ScriptedPhone phone;
    QVERIFY(f.pairPhone(phone));
    InputFixture::begin(phone, 4);
    Envelope err;
    QVERIFY(phone.waitFor(s("error"), &err));
    QCOMPARE(err.b.value(s("code")).toString(), s("internal"));
}

void TstLinkInput::revokeEndsControlAndNextBeginPromptsAgain()
{
    InputFixture f;
    QVERIFY(f.usable());
    ScriptedPhone phone;
    QVERIFY(f.pairPhone(phone));
    InputFixture::begin(phone, 1);
    QVERIFY(phone.waitFor(s("ok")));
    phone.send(InputFixture::frame(s("input.key"), o({{s("name"), s("Shift")}, {s("state"), s("down")}})));
    QVERIFY(waitUntil([&] { return f.callSeen(s("NotifyKeyboardKeysym:65505:1")); }));
    f.fakePortal.revoke();
    Envelope end;
    QVERIFY(phone.waitFor(s("input.end"), &end));
    QCOMPARE(end.b.value(s("reason")).toString(), s("revoked"));
    QVERIFY(!f.input.flowing());
    QVERIFY(f.input.controller().isEmpty());
    QVERIFY(!QFileInfo::exists(f.tokenFile())); // a revoked grant is not reused
    f.fakePortal.calls.clear();
    phone.send(InputFixture::frame(s("input.pointer"), o({{s("dx"), 1}, {s("dy"), 1}})));
    QTest::qWait(100);
    QVERIFY(!f.callSeen(s("Notify")));
    // The next begin starts a fresh portal session that the user may decline.
    f.fakePortal.mode = FakePortal::Mode::DenyAtStart;
    InputFixture::begin(phone, 2);
    Envelope err;
    QVERIFY(phone.waitFor(s("error"), &err));
    QCOMPARE(err.re, 2);
    QCOMPARE(err.b.value(s("code")).toString(), s("denied"));
    QVERIFY(f.callSeen(s("CreateSession")));
    QVERIFY(!f.fakePortal.lastSelect.contains(s("restore_token")));
}

void TstLinkInput::releasesHeldInputOnDisconnect()
{
    InputFixture f;
    QVERIFY(f.usable());
    {
        ScriptedPhone phone;
        QVERIFY(f.pairPhone(phone));
        InputFixture::begin(phone, 1);
        QVERIFY(phone.waitFor(s("ok")));
        phone.send(InputFixture::frame(s("input.key"), o({{s("name"), s("Ctrl")}, {s("state"), s("down")}})));
        phone.send(InputFixture::frame(s("input.button"), o({{s("button"), s("left")}, {s("state"), s("down")}})));
        QVERIFY(waitUntil([&] { return f.callSeen(s("NotifyPointerButton:272:1")) && f.callSeen(s("NotifyKeyboardKeysym:65507:1")); }));
        f.fakePortal.calls.clear();
    } // the phone socket closes here
    QVERIFY(waitUntil([&] { return f.callSeen(s("NotifyPointerButton:272:0")) && f.callSeen(s("NotifyKeyboardKeysym:65507:0")); }));
    QVERIFY(waitUntil([&] { return f.callSeen(s("Close")); }));
    QVERIFY(f.input.controller().isEmpty());
}

void TstLinkInput::releasesOnEndAndOnLock()
{
    InputFixture f;
    QVERIFY(f.usable());
    ScriptedPhone phone;
    QVERIFY(f.pairPhone(phone));
    InputFixture::begin(phone, 1);
    QVERIFY(phone.waitFor(s("ok")));
    phone.send(InputFixture::frame(s("input.key"), o({{s("name"), s("Alt")}, {s("state"), s("down")}})));
    QVERIFY(waitUntil([&] { return f.callSeen(s("NotifyKeyboardKeysym:65513:1")); }));
    f.gate.setOpen(false); // the screen locks
    QVERIFY(waitUntil([&] { return f.callSeen(s("NotifyKeyboardKeysym:65513:0")); }));
    QVERIFY(waitUntil([&] { return phone.all().last().t == QLatin1String("input.state"); }));
    QCOMPARE(phone.all().last().b.value(s("state")).toString(), s("locked"));
    f.fakePortal.calls.clear();
    phone.send(InputFixture::frame(s("input.pointer"), o({{s("dx"), 1}, {s("dy"), 1}})));
    QTest::qWait(100);
    QVERIFY(!f.callSeen(s("NotifyPointerMotion"))); // dropped while locked
    f.gate.setOpen(true);
    QVERIFY(waitUntil([&] { return phone.all().last().b.value(s("state")).toString() == s("active"); }));
    phone.send(InputFixture::frame(s("input.key"), o({{s("name"), s("Meta")}, {s("state"), s("down")}})));
    QVERIFY(waitUntil([&] { return f.callSeen(s("NotifyKeyboardKeysym:65515:1")); }));
    phone.send(InputFixture::frame(s("input.end"), {}));
    QVERIFY(waitUntil([&] { return f.callSeen(s("NotifyKeyboardKeysym:65515:0")); }));
}

void TstLinkInput::onlyTheControllerDrivesInput()
{
    InputFixture f;
    QVERIFY(f.usable());
    ScriptedPhone first, second;
    QVERIFY(f.pairPhone(first));
    QVERIFY(f.pairPhone(second));
    InputFixture::begin(first, 1);
    QVERIFY(first.waitFor(s("ok")));
    InputFixture::begin(second, 2);
    Envelope err;
    QVERIFY(second.waitFor(s("error"), &err));
    QCOMPARE(err.b.value(s("code")).toString(), s("busy"));
    f.fakePortal.calls.clear();
    second.send(InputFixture::frame(s("input.pointer"), o({{s("dx"), 9}, {s("dy"), 9}})));
    QTest::qWait(100);
    QVERIFY(!f.callSeen(s("NotifyPointerMotion")));
    // A locked desktop refuses begin with busy/locked instead of prompting.
    first.send(InputFixture::frame(s("input.end"), {}));
    QVERIFY(waitUntil([&] { return f.input.controller().isEmpty(); }));
    f.gate.setOpen(false);
    InputFixture::begin(second, 3);
    QVERIFY(second.waitFor(s("error"), &err));
    QCOMPARE(err.b.value(s("code")).toString(), s("busy"));
    QCOMPARE(err.b.value(s("message")).toString(), s("locked"));
}

void TstLinkInput::invalidFramesAreRejectedOrDropped()
{
    InputFixture f;
    QVERIFY(f.usable());
    ScriptedPhone phone;
    QVERIFY(f.pairPhone(phone));
    InputFixture::begin(phone, 1);
    QVERIFY(phone.waitFor(s("ok")));
    f.fakePortal.calls.clear();
    phone.send(InputFixture::frame(s("input.pointer"), o({{s("dx"), 5000}, {s("dy"), 0}}), 20));
    Envelope err;
    QVERIFY(phone.waitFor(s("error"), &err));
    QCOMPARE(err.re, 20);
    QCOMPARE(err.b.value(s("code")).toString(), s("bad-request"));
    phone.send(InputFixture::frame(s("input.key"), o({{s("keysym"), 3}, {s("state"), s("down")}})));
    phone.send(InputFixture::frame(s("input.text"), o({{s("text"), QString(5000, QLatin1Char('x'))}})));
    // A flood of pointer frames is rate limited (burst 60), not forwarded unbounded.
    for (int i = 0; i < 400; ++i) phone.send(InputFixture::frame(s("input.pointer"), o({{s("dx"), 1}, {s("dy"), 0}})));
    QTest::qWait(300);
    QVERIFY(!f.callSeen(s("NotifyKeyboardKeysym")));
    const int moves = f.callCount(s("NotifyPointerMotion"));
    QVERIFY2(moves > 0 && moves < 200, qPrintable(QString::number(moves)));
}

void TstLinkInput::textIsBatchedAndNeverLogged()
{
    InputFixture f;
    QVERIFY(f.usable());
    ScriptedPhone phone;
    QVERIFY(f.pairPhone(phone));
    InputFixture::begin(phone, 1);
    QVERIFY(phone.waitFor(s("ok")));
    f.fakePortal.calls.clear();

    static QStringList logged;
    logged.clear();
    const QtMessageHandler previous = qInstallMessageHandler([](QtMsgType, const QMessageLogContext &, const QString &m) { logged << m; });
    const QString secret = QStringLiteral("hunter2é€\n");
    phone.send(InputFixture::frame(s("input.text"), o({{s("text"), secret}}), 30));
    Envelope ok;
    QVERIFY(phone.waitFor(s("ok"), &ok));
    QCOMPARE(ok.re, 30);
    QVERIFY(waitUntil([&] { return f.fakePortal.calls.size() >= 20; })); // portal calls trail the reply
    qInstallMessageHandler(previous);
    // 7 ASCII + é + € + newline, each a press and a release.
    QCOMPARE(f.fakePortal.calls.size(), 20);
    QCOMPARE(f.fakePortal.calls.at(0), s("NotifyKeyboardKeysym:104:1")); // h
    QCOMPARE(f.fakePortal.calls.at(14), s("NotifyKeyboardKeysym:233:1")); // é, a Latin-1 keysym
    QCOMPARE(f.fakePortal.calls.at(16), QStringLiteral("NotifyKeyboardKeysym:%1:1").arg(0x010020ac)); // €
    QCOMPARE(f.fakePortal.calls.at(18), s("NotifyKeyboardKeysym:65293:1")); // Return
    for (const QString &line : std::as_const(logged)) {
        QVERIFY2(!line.contains(s("hunter")) && !line.contains(QStringLiteral("€")), "text content reached a log");
    }
}

void TstLinkInput::frameBeforeBeginIsDropped()
{
    InputFixture f;
    QVERIFY(f.usable());
    ScriptedPhone phone;
    QVERIFY(f.pairPhone(phone));
    phone.send(InputFixture::frame(s("input.pointer"), o({{s("dx"), 2}, {s("dy"), 2}})));
    phone.send(InputFixture::frame(s("input.key"), o({{s("name"), s("Tab")}, {s("state"), s("tap")}})));
    QTest::qWait(150);
    // No portal traffic at all: nothing may raise a prompt without input.begin.
    QVERIFY(f.fakePortal.calls.isEmpty());
}

void TstLinkInput::lockMidTextReplyAndNextTextWorks()
{
    InputFixture f;
    QVERIFY(f.usable());
    ScriptedPhone phone;
    QVERIFY(f.pairPhone(phone));
    InputFixture::begin(phone, 1);
    QVERIFY(phone.waitFor(s("ok")));
    phone.send(InputFixture::frame(s("input.text"), o({{s("text"), QString(3000, QLatin1Char('q'))}}), 40));
    QVERIFY(waitUntil([&] { return f.callCount(s("NotifyKeyboardKeysym")) >= 10; }));
    f.gate.setOpen(false);
    Envelope err;
    QVERIFY(phone.waitFor(s("error"), &err)); // the cancelled text is answered, not left hanging
    QCOMPARE(err.re, 40);
    f.gate.setOpen(true);
    QVERIFY(waitUntil([&] { return phone.all().last().b.value(s("state")).toString() == s("active"); }));
    f.fakePortal.calls.clear();
    phone.send(InputFixture::frame(s("input.text"), o({{s("text"), s("ok")}}), 41));
    Envelope ok;
    QVERIFY(phone.waitFor(s("ok"), &ok));
    QCOMPARE(ok.re, 41); // not refused busy
}

void TstLinkInput::watchdogAndBeginReleaseStuckKeys()
{
    InputFixture f;
    QVERIFY(f.usable());
    f.input.setHoldLimit(300);
    ScriptedPhone phone;
    QVERIFY(f.pairPhone(phone));
    InputFixture::begin(phone, 1);
    QVERIFY(phone.waitFor(s("ok")));
    phone.send(InputFixture::frame(s("input.key"), o({{s("name"), s("Ctrl")}, {s("state"), s("down")}})));
    QVERIFY(waitUntil([&] { return f.callSeen(s("NotifyKeyboardKeysym:65507:1")); }));
    QVERIFY(waitUntil([&] { return f.callSeen(s("NotifyKeyboardKeysym:65507:0")); }, 4000)); // watchdog
    QVERIFY(!f.input.holding());
    // A new input.begin releases whatever is held.
    phone.send(InputFixture::frame(s("input.key"), o({{s("name"), s("Alt")}, {s("state"), s("down")}})));
    QVERIFY(waitUntil([&] { return f.callSeen(s("NotifyKeyboardKeysym:65513:1")); }));
    f.input.setHoldLimit(60'000);
    InputFixture::begin(phone, 2);
    QVERIFY(waitUntil([&] { return f.callSeen(s("NotifyKeyboardKeysym:65513:0")); }));
}

void TstLinkInput::keyPressesAreRateLimitedButReleasesAreNot()
{
    InputFixture f;
    QVERIFY(f.usable());
    ScriptedPhone phone;
    QVERIFY(f.pairPhone(phone));
    InputFixture::begin(phone, 1);
    QVERIFY(phone.waitFor(s("ok")));
    f.fakePortal.calls.clear();
    for (int i = 0; i < 300; ++i) phone.send(InputFixture::frame(s("input.key"), o({{s("keysym"), 97}, {s("state"), s("down")}})));
    for (int i = 0; i < 300; ++i) phone.send(InputFixture::frame(s("input.key"), o({{s("keysym"), 97}, {s("state"), s("up")}})));
    QVERIFY(waitUntil([&] { return f.callCount(s("NotifyKeyboardKeysym:97:0")) >= 300; }));
    const int downs = f.callCount(s("NotifyKeyboardKeysym:97:1"));
    QVERIFY2(downs > 0 && downs < 150, qPrintable(QString::number(downs)));
    QVERIFY(!f.input.holding());
}

void TstLinkInput::sessionControlKeysymsAndC1TextAreRefused()
{
    using namespace InputFrames;
    QVERIFY(!validKeysym(0x1008ff2a)); // PowerOff
    QVERIFY(!validKeysym(0x1008ff2f)); // Sleep
    QVERIFY(!validKeysym(0x1008ffa7)); // Suspend
    QVERIFY(!validKeysym(0x1008ffa8)); // Hibernate
    QVERIFY(!validKeysym(0x1008ff61)); // LogOff
    QVERIFY(!validKeysym(0x1008ff2b)); // WakeUp
    QVERIFY(validKeysym(0x1008ff13));  // AudioRaiseVolume
    InputFixture f;
    QVERIFY(f.usable());
    ScriptedPhone phone;
    QVERIFY(f.pairPhone(phone));
    InputFixture::begin(phone, 1);
    QVERIFY(phone.waitFor(s("ok")));
    f.fakePortal.calls.clear();
    phone.send(InputFixture::frame(s("input.text"), o({{s("text"), QStringLiteral("a\u0085\u009fb")}}), 7));
    Envelope ok;
    QVERIFY(phone.waitFor(s("ok"), &ok));
    QVERIFY(waitUntil([&] { return f.fakePortal.calls.size() >= 4; }));
    QTest::qWait(50);
    QCOMPARE(f.fakePortal.calls, (QStringList{s("NotifyKeyboardKeysym:97:1"), s("NotifyKeyboardKeysym:97:0"),
                                              s("NotifyKeyboardKeysym:98:1"), s("NotifyKeyboardKeysym:98:0")}));
}

void TstLinkInput::lockedBeginDoesNotClaimTheController()
{
    InputFixture f;
    QVERIFY(f.usable());
    ScriptedPhone first, second;
    QVERIFY(f.pairPhone(first));
    QVERIFY(f.pairPhone(second));
    f.gate.setOpen(false);
    InputFixture::begin(first, 1);
    Envelope err;
    QVERIFY(first.waitFor(s("error"), &err));
    QCOMPARE(err.b.value(s("message")).toString(), s("locked"));
    QVERIFY(f.input.controller().isEmpty());
    f.gate.setOpen(true);
    InputFixture::begin(second, 2); // not busy: the locked attempt claimed nothing
    Envelope ok;
    QVERIFY(second.waitFor(s("ok"), &ok));
    QCOMPARE(ok.re, 2);
}

void TstLinkInput::refusedSelectDevicesClearsTheToken()
{
    InputFixture f;
    QVERIFY(f.usable());
    QVERIFY(QDir().mkpath(QFileInfo(f.tokenFile()).absolutePath()));
    QFile t(f.tokenFile());
    QVERIFY(t.open(QIODevice::WriteOnly));
    t.write("stale-token\n");
    t.close();
    QVERIFY(t.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner));
    f.fakePortal.mode = FakePortal::Mode::RefuseSelect;
    ScriptedPhone phone;
    QVERIFY(f.pairPhone(phone));
    InputFixture::begin(phone, 1);
    Envelope err;
    QVERIFY(phone.waitFor(s("error"), &err));
    QCOMPARE(f.fakePortal.lastSelect.value(s("restore_token")).toString(), s("stale-token"));
    QVERIFY(!QFileInfo::exists(f.tokenFile()));
}

void TstLinkInput::shutdownReleasesBeforeClosing()
{
    InputFixture f;
    QVERIFY(f.usable());
    ScriptedPhone phone;
    QVERIFY(f.pairPhone(phone));
    InputFixture::begin(phone, 1);
    QVERIFY(phone.waitFor(s("ok")));
    phone.send(InputFixture::frame(s("input.key"), o({{s("name"), s("Meta")}, {s("state"), s("down")}})));
    QVERIFY(waitUntil([&] { return f.callSeen(s("NotifyKeyboardKeysym:65515:1")); }));
    f.input.shutdown();
    QVERIFY(waitUntil([&] { return f.callSeen(s("Close")); }));
    const int up = int(f.fakePortal.calls.indexOf(s("NotifyKeyboardKeysym:65515:0")));
    QVERIFY(up >= 0);
    QVERIFY(up < int(f.fakePortal.calls.indexOf(s("Close")))); // released while the session was still open
}

void TstLinkInput::gateReasonsAreReportedTruthfully()
{
    InputFixture f;
    QVERIFY(f.usable());
    // No lock-state source: the phone must hear "unverified", never "locked".
    f.gate.setReason(InputGate::Reason::Unverified);
    ScriptedPhone phone;
    QVERIFY(f.pairPhone(phone));
    InputFixture::begin(phone, 1);
    Envelope err;
    QVERIFY(phone.waitFor(s("error"), &err));
    QCOMPARE(err.b.value(s("message")).toString(), s("unverified"));
    QVERIFY(waitUntil([&] { return phone.count(s("input.state")) >= 1; }));
    QCOMPARE(phone.all().last().b.value(s("state")).toString(), s("unverified"));
    QVERIFY(f.input.controller().isEmpty());
    // Once verified and open, begin works; later transitions name their own reason.
    f.gate.setReason(InputGate::Reason::Open);
    InputFixture::begin(phone, 2);
    Envelope ok;
    QVERIFY(phone.waitFor(s("ok"), &ok));
    f.gate.setReason(InputGate::Reason::Locked);
    QVERIFY(waitUntil([&] { return phone.all().last().b.value(s("state")).toString() == s("locked"); }));
    f.gate.setReason(InputGate::Reason::Unverified);
    QVERIFY(waitUntil([&] { return phone.all().last().b.value(s("state")).toString() == s("unverified"); }));
    f.gate.setReason(InputGate::Reason::Inactive);
    QVERIFY(waitUntil([&] { return phone.all().last().b.value(s("state")).toString() == s("inactive"); }));
    f.gate.setReason(InputGate::Reason::Open);
    QVERIFY(waitUntil([&] { return phone.all().last().b.value(s("state")).toString() == s("active"); }));
}

QTEST_GUILESS_MAIN(TstLinkInput)
#include "tst_link_input.moc"
