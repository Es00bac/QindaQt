// SPDX-License-Identifier: GPL-3.0-or-later
// macro.* through the shared MacroRunner to a fake RemoteDesktop portal, with
// the library read from a fake InputActions1. Both fakes use unique names on
// the test bus, so nothing here can reach the real portal.
#include "support/input_fixture.h"

#include <QtTest/QTest>

using namespace QindaQt::Link;
using namespace QindaQt::Link::Test;
namespace IA = QindaQt::InputActions;

namespace {
QString s(const char *v) { return QString::fromLatin1(v); }

IA::Binding combo(const QStringList &mods, const QStringList &keys)
{
    IA::Binding b;
    b.action = s("combination");
    b.modifiers = mods;
    b.keys = keys;
    return b;
}
IA::MacroStep step(const QString &kind, const IA::Binding &b = {}, int ms = 40, int slot = 0)
{
    IA::MacroStep st;
    st.kind = kind;
    st.binding = b;
    st.milliseconds = ms;
    st.slot = slot;
    return st;
}
IA::Macro macro(const QString &id, const QString &name, const QList<IA::MacroStep> &steps)
{
    IA::Macro m;
    m.id = id;
    m.name = name;
    m.steps = steps;
    return m;
}
IA::Binding text(const QString &t)
{
    IA::Binding b;
    b.action = s("text");
    b.text = t;
    return b;
}
IA::Binding action(const QString &a)
{
    IA::Binding b;
    b.action = a;
    return b;
}
QStringList macroIds(const Envelope &list)
{
    QStringList ids;
    for (const QJsonValue &v : list.b.value(s("macros")).toArray()) ids << v.toObject().value(s("id")).toString();
    return ids;
}
}

class TstLinkMacros : public QObject {
    Q_OBJECT
private slots:
    void listSyncsOnReadyAndHidesSteps();
    void listFollowsLibraryChanges();
    void runMapsChordToPortalKeys();
    void runTypesTextSteps();
    void runNeedsTheInputController();
    void secondRunIsBusyAndCancelReleases();
    void lockCancelsRunAndReleases();
    void compositorOnlyActionFailsTheRun();
    void outputIsNotAllowedWhileLockedOrInactive();
};

static bool beginAndWait(InputFixture &f, ScriptedPhone &phone)
{
    InputFixture::begin(phone, 1);
    return phone.waitFor(s("ok")) && f.input.flowing();
}

void TstLinkMacros::listSyncsOnReadyAndHidesSteps()
{
    InputFixture f;
    QVERIFY(f.usable());
    f.fakeActions.setMacros({macro(s("copy"), s("Copy"), {step(s("tap"), combo({s("Ctrl")}, {s("C")}))}),
                             macro(s("sig"), s("Signature"), {step(s("text"), text(s("secret-signature-text")))})});
    QVERIFY(waitUntil([&] { return f.catalog.macros().size() == 2; }));
    ScriptedPhone phone;
    QVERIFY(f.pairPhone(phone));
    Envelope list;
    QVERIFY(phone.waitFor(s("macro.list"), &list));
    QCOMPARE(macroIds(list), (QStringList{s("copy"), s("sig")}));
    const QString wire = QString::fromUtf8(QJsonDocument(list.b).toJson());
    QVERIFY(!wire.contains(s("secret-signature")));
    QVERIFY(!wire.contains(s("steps")));
    QVERIFY(!wire.contains(s("combination")));
    for (const QJsonValue &v : list.b.value(s("macros")).toArray()) {
        QCOMPARE(v.toObject().keys(), (QStringList{s("id"), s("name")}));
    }
    // The phone can ask again.
    Envelope again;
    again.t = s("macro.list");
    again.id = 3;
    phone.send(again);
    QVERIFY(phone.waitFor(s("macro.list")));
    Envelope ok;
    QVERIFY(phone.waitFor(s("ok"), &ok));
    QCOMPARE(ok.re, 3);
}

void TstLinkMacros::listFollowsLibraryChanges()
{
    InputFixture f;
    QVERIFY(f.usable());
    ScriptedPhone phone;
    QVERIFY(f.pairPhone(phone));
    Envelope list;
    QVERIFY(phone.waitFor(s("macro.list"), &list)); // empty library on READY
    QVERIFY(macroIds(list).isEmpty());
    f.fakeActions.setMacros({macro(s("a"), s("A"), {step(s("wait"), {}, 10)})});
    QVERIFY(phone.waitFor(s("macro.list"), &list));
    QCOMPARE(macroIds(list), (QStringList{s("a")}));
    // A revision bump that changes nothing (a macro merely ran) sends nothing.
    const int before = phone.count(s("macro.list"));
    f.fakeActions.setMacros({macro(s("a"), s("A"), {step(s("wait"), {}, 10)})});
    QTest::qWait(200);
    QCOMPARE(phone.count(s("macro.list")), before);
    // The service going away empties the list.
    f.fakeActions.ready = false;
    f.fakeActions.setMacros({macro(s("a"), s("A"), {step(s("wait"), {}, 10)})});
    QVERIFY(phone.waitFor(s("macro.list"), &list));
    QVERIFY(macroIds(list).isEmpty());
}

void TstLinkMacros::runMapsChordToPortalKeys()
{
    InputFixture f;
    QVERIFY(f.usable());
    f.fakeActions.setMacros({macro(s("copy"), s("Copy"), {step(s("tap"), combo({s("Ctrl")}, {s("C")}))})});
    QVERIFY(waitUntil([&] { return !f.catalog.macros().isEmpty(); }));
    ScriptedPhone phone;
    QVERIFY(f.pairPhone(phone));
    QVERIFY(beginAndWait(f, phone));
    f.fakePortal.calls.clear();
    phone.send(InputFixture::frame(s("macro.run"), QJsonObject{{s("id"), s("copy")}}, 9));
    Envelope ok;
    QVERIFY(phone.waitFor(s("ok"), &ok, 6000));
    QCOMPARE(ok.re, 9);
    QVERIFY(waitUntil([&] { return f.fakePortal.calls.size() >= 4; })); // portal calls trail the reply
    QCOMPARE(f.fakePortal.calls, (QStringList{s("NotifyKeyboardKeysym:65507:1"), s("NotifyKeyboardKeysym:99:1"),
                                              s("NotifyKeyboardKeysym:99:0"), s("NotifyKeyboardKeysym:65507:0")}));
    QStringList states;
    for (const Envelope &e : phone.all()) {
        if (e.t == QLatin1String("macro.state")) states << e.b.value(s("state")).toString();
    }
    QCOMPARE(states, (QStringList{s("running"), s("done")}));
}

void TstLinkMacros::runTypesTextSteps()
{
    InputFixture f;
    QVERIFY(f.usable());
    f.fakeActions.setMacros({macro(s("t"), s("Type"), {step(s("text"), text(QStringLiteral("a\u20ac")))})});
    QVERIFY(waitUntil([&] { return !f.catalog.macros().isEmpty(); }));
    ScriptedPhone phone;
    QVERIFY(f.pairPhone(phone));
    QVERIFY(beginAndWait(f, phone));
    f.fakePortal.calls.clear();
    phone.send(InputFixture::frame(s("macro.run"), QJsonObject{{s("id"), s("t")}}, 4));
    Envelope ok;
    QVERIFY(phone.waitFor(s("ok"), &ok, 6000));
    QVERIFY(waitUntil([&] { return f.fakePortal.calls.size() >= 4; }));
    QCOMPARE(f.fakePortal.calls, (QStringList{s("NotifyKeyboardKeysym:97:1"), s("NotifyKeyboardKeysym:97:0"),
                                              QStringLiteral("NotifyKeyboardKeysym:%1:1").arg(0x010020ac),
                                              QStringLiteral("NotifyKeyboardKeysym:%1:0").arg(0x010020ac)}));
}

void TstLinkMacros::runNeedsTheInputController()
{
    InputFixture f;
    QVERIFY(f.usable());
    f.fakeActions.setMacros({macro(s("copy"), s("Copy"), {step(s("tap"), combo({s("Ctrl")}, {s("C")}))})});
    QVERIFY(waitUntil([&] { return !f.catalog.macros().isEmpty(); }));
    ScriptedPhone phone;
    QVERIFY(f.pairPhone(phone));
    // Without input.begin a run is refused and never opens the portal.
    phone.send(InputFixture::frame(s("macro.run"), QJsonObject{{s("id"), s("copy")}}, 2));
    Envelope err;
    QVERIFY(phone.waitFor(s("error"), &err));
    QCOMPARE(err.b.value(s("code")).toString(), s("denied"));
    QVERIFY(f.fakePortal.calls.isEmpty());
    QVERIFY(beginAndWait(f, phone));
    phone.send(InputFixture::frame(s("macro.run"), QJsonObject{{s("id"), s("nope")}}, 3));
    QVERIFY(phone.waitFor(s("error"), &err));
    QCOMPARE(err.b.value(s("code")).toString(), s("bad-request"));
}

void TstLinkMacros::secondRunIsBusyAndCancelReleases()
{
    InputFixture f;
    QVERIFY(f.usable());
    f.fakeActions.setMacros({macro(s("hold"), s("Hold"),
                                   {step(s("press"), combo({s("Shift")}, {}), 40, 0), step(s("wait"), {}, 5000),
                                    step(s("release"), {}, 40, 0)})});
    QVERIFY(waitUntil([&] { return !f.catalog.macros().isEmpty(); }));
    ScriptedPhone phone;
    QVERIFY(f.pairPhone(phone));
    QVERIFY(beginAndWait(f, phone));
    f.fakePortal.calls.clear();
    phone.send(InputFixture::frame(s("macro.run"), QJsonObject{{s("id"), s("hold")}}, 11));
    QVERIFY(waitUntil([&] { return f.callSeen(s("NotifyKeyboardKeysym:65505:1")); }));
    phone.send(InputFixture::frame(s("macro.run"), QJsonObject{{s("id"), s("hold")}}, 12));
    Envelope err;
    QVERIFY(phone.waitFor(s("error"), &err));
    QCOMPARE(err.re, 12);
    QCOMPARE(err.b.value(s("code")).toString(), s("busy"));
    phone.send(InputFixture::frame(s("macro.cancel"), {}, 13));
    QVERIFY(waitUntil([&] { return f.callSeen(s("NotifyKeyboardKeysym:65505:0")); }));
    Envelope end;
    QVERIFY(phone.waitFor(s("error"), &end)); // the run's reply: cancelled
    QCOMPARE(end.re, 11);
    QCOMPARE(end.b.value(s("message")).toString(), s("cancelled"));
    QVERIFY(!f.output.holding());
}

void TstLinkMacros::lockCancelsRunAndReleases()
{
    InputFixture f;
    QVERIFY(f.usable());
    f.fakeActions.setMacros({macro(s("hold"), s("Hold"),
                                   {step(s("press"), combo({s("Alt")}, {}), 40, 0), step(s("wait"), {}, 5000),
                                    step(s("release"), {}, 40, 0)})});
    QVERIFY(waitUntil([&] { return !f.catalog.macros().isEmpty(); }));
    ScriptedPhone phone;
    QVERIFY(f.pairPhone(phone));
    QVERIFY(beginAndWait(f, phone));
    f.fakePortal.calls.clear();
    phone.send(InputFixture::frame(s("macro.run"), QJsonObject{{s("id"), s("hold")}}, 21));
    QVERIFY(waitUntil([&] { return f.callSeen(s("NotifyKeyboardKeysym:65513:1")); }));
    f.gate.setOpen(false);
    QVERIFY(waitUntil([&] { return f.callSeen(s("NotifyKeyboardKeysym:65513:0")); }));
    Envelope err;
    QVERIFY(phone.waitFor(s("error"), &err));
    QCOMPARE(err.re, 21);
    QCOMPARE(err.b.value(s("message")).toString(), s("input-lost"));
}

void TstLinkMacros::compositorOnlyActionFailsTheRun()
{
    InputFixture f;
    QVERIFY(f.usable());
    f.fakeActions.setMacros({macro(s("max"), s("Max"), {step(s("tap"), action(s("maximize")))})});
    QVERIFY(waitUntil([&] { return !f.catalog.macros().isEmpty(); }));
    ScriptedPhone phone;
    QVERIFY(f.pairPhone(phone));
    QVERIFY(beginAndWait(f, phone));
    f.fakePortal.calls.clear();
    phone.send(InputFixture::frame(s("macro.run"), QJsonObject{{s("id"), s("max")}}, 31));
    Envelope err;
    QVERIFY(phone.waitFor(s("error"), &err, 6000));
    QCOMPARE(err.re, 31);
    QCOMPARE(err.b.value(s("message")).toString(), s("input-unavailable"));
    QVERIFY(f.fakePortal.calls.isEmpty());
}

void TstLinkMacros::outputIsNotAllowedWhileLockedOrInactive()
{
    InputFixture f;
    QVERIFY(f.usable());
    QVERIFY(!f.output.allowed()); // portal not started
    ScriptedPhone phone;
    QVERIFY(f.pairPhone(phone));
    QVERIFY(beginAndWait(f, phone));
    QVERIFY(f.output.allowed());
    f.gate.setOpen(false);
    QVERIFY(!f.output.allowed());
    f.gate.setOpen(true);
    QVERIFY(f.output.allowed());
    f.fakePortal.revoke();
    QVERIFY(waitUntil([&] { return !f.output.allowed(); }));
}

QTEST_GUILESS_MAIN(TstLinkMacros)
#include "tst_link_macros.moc"
