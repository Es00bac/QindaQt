// SPDX-License-Identifier: GPL-3.0-or-later
// MessagesFeature against an in-process scripted phone: request bounds, typed
// replies, validation of hostile phone data, change and send-status events,
// unread totals and the "no message content in logs" rule. Uses throwaway
// identities and an ephemeral port only.
#include "support/hub_fixture.h"

#include <qindaqt/services/link_messages/messages_feature.h>

#include <QtCore/QJsonArray>
#include <QtCore/QLoggingCategory>

using namespace QindaQt::Link;
using namespace QindaQt::Link::Test;

namespace {

QString s(const char *v) { return QString::fromLatin1(v); }

Envelope typedReply(const Envelope &req, const QJsonObject &body)
{
    Envelope e;
    e.t = req.t;
    e.re = req.id;
    e.b = body;
    return e;
}

Envelope errorReply(const Envelope &req, const char *code)
{
    Envelope e;
    e.t = s("error");
    e.re = req.id;
    e.b = QJsonObject{{s("code"), s(code)}, {s("message"), s("x")}};
    return e;
}

QJsonObject thread(int id)
{
    return QJsonObject{{s("id"), id}, {s("addrs"), QJsonArray{s("+1555")}}, {s("names"), QJsonArray{s("")}},
                       {s("snippet"), s("hi")}, {s("date"), 1000 + id}, {s("unread"), 1}, {s("count"), 2}};
}

QJsonObject threadsBody(int n, int unread)
{
    QJsonArray a;
    for (int i = 0; i < n; ++i) a.append(thread(100 - i));
    return QJsonObject{{s("threads"), a}, {s("more"), false}, {s("unread"), unread}};
}

QJsonObject message(int id, const QString &body)
{
    return QJsonObject{{s("id"), id}, {s("kind"), s("sms")}, {s("box"), s("inbox")}, {s("addr"), s("+1555")},
                       {s("date"), 5000 + id}, {s("body"), body}, {s("trunc"), false}, {s("read"), true},
                       {s("parts"), QJsonArray{}}};
}

struct Result {
    bool done = false;
    QJsonObject body;
    QString error;
    MessagesFeature::Reply cb() { return [this](const QJsonObject &b, const QString &e) { done = true; body = b; error = e; }; }
};

struct Rig {
    Rig() : feature([this](const QString &dev) { return hub.hub.session(dev); })
    {
        hub.hub.addFeature(&feature);
        ok = hub.pair(phone, {s("status"), s("sms")});
        id = phone.deviceId();
    }
    HubFixture hub;
    MessagesFeature feature;
    ScriptedPhone phone;
    QString id;
    bool ok = false;
};

QStringList g_log;
void capture(QtMsgType, const QMessageLogContext &, const QString &msg) { g_log << msg; }

} // namespace

class LinkMessagesTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void threadRequestIsClampedAndCursorForwarded()
    {
        Rig r;
        QVERIFY(r.ok);
        Result res;
        r.feature.listThreads(r.id, 5000, 1234, 9, res.cb());
        Envelope req;
        QVERIFY(r.phone.waitFor(s("sms.threads"), &req));
        QCOMPARE(req.b.value(s("limit")).toInt(), 100);
        QCOMPARE(req.b.value(s("before")).toInt(), 1234);
        QCOMPARE(req.b.value(s("beforeId")).toInt(), 9);
        r.phone.send(typedReply(req, threadsBody(3, 5)));
        QVERIFY(waitUntil([&] { return res.done; }));
        QVERIFY(res.error.isEmpty());
        QCOMPARE(res.body.value(s("threads")).toArray().size(), 3);
        QCOMPARE(r.feature.unreadTotal(), 5);

        Result first;
        r.feature.listThreads(r.id, 0, -1, -1, first.cb());
        QVERIFY(r.phone.waitFor(s("sms.threads"), &req));
        QCOMPARE(req.b.value(s("limit")).toInt(), 1);
        QVERIFY(!req.b.contains(s("before")));
    }

    void oversizedOrMistypedRepliesAreRejected()
    {
        Rig r;
        QVERIFY(r.ok);
        Result big;
        r.feature.listThreads(r.id, 100, -1, -1, big.cb());
        Envelope req;
        QVERIFY(r.phone.waitFor(s("sms.threads"), &req));
        r.phone.send(typedReply(req, threadsBody(101, 1)));
        QVERIFY(waitUntil([&] { return big.done; }));
        QCOMPARE(big.error, s("bad-reply"));
        QVERIFY(big.body.isEmpty());

        Result wrong;
        r.feature.listMessages(r.id, 4, 500, -1, -1, wrong.cb());
        QVERIFY(r.phone.waitFor(s("sms.messages"), &req));
        QCOMPARE(req.b.value(s("limit")).toInt(), 200);
        QJsonObject m = message(1, s("x"));
        m.insert(s("box"), s("weird"));
        r.phone.send(typedReply(req, QJsonObject{{s("thread"), 4}, {s("more"), false}, {s("messages"), QJsonArray{m}}}));
        QVERIFY(waitUntil([&] { return wrong.done; }));
        QCOMPARE(wrong.error, s("bad-reply"));

        Result ok;
        r.feature.listMessages(r.id, 4, 10, -1, -1, ok.cb());
        QVERIFY(r.phone.waitFor(s("sms.messages"), &req));
        QJsonObject good = message(2, s("body"));
        good.insert(s("extra"), 1);
        r.phone.send(typedReply(req, QJsonObject{{s("thread"), 4}, {s("more"), true}, {s("messages"), QJsonArray{good}}}));
        QVERIFY(waitUntil([&] { return ok.done; }));
        QVERIFY(ok.error.isEmpty());
        QVERIFY(!ok.body.value(s("messages")).toArray().first().toObject().contains(s("extra")));
    }

    void phoneErrorsAndMissingPeersMapToCodes()
    {
        Rig r;
        QVERIFY(r.ok);
        Result denied;
        r.feature.getPart(r.id, 7, 99999, denied.cb());
        Envelope req;
        QVERIFY(r.phone.waitFor(s("sms.part"), &req));
        QCOMPARE(req.b.value(s("maxEdge")).toInt(), 2048);
        r.phone.send(errorReply(req, "denied"));
        QVERIFY(waitUntil([&] { return denied.done; }));
        QCOMPARE(denied.error, s("denied"));

        Result gone;
        r.feature.markRead(s("nobody"), 1, gone.cb());
        QVERIFY(gone.done);
        QCOMPARE(gone.error, s("not-connected"));

        HubFixture nohub;
        MessagesFeature f2([&](const QString &id) { return nohub.hub.session(id); });
        nohub.hub.addFeature(&f2);
        ScriptedPhone plain;
        QVERIFY(nohub.pair(plain, {s("status")}));
        Result nocap;
        f2.markRead(plain.deviceId(), 1, nocap.cb());
        QVERIFY(nocap.done);
        QCOMPARE(nocap.error, s("unsupported"));
    }

    void changeEventsUpdateTheUnreadTotal()
    {
        Rig r;
        QVERIFY(r.ok);
        QSignalSpy changed(&r.feature, &MessagesFeature::threadsChanged);
        QSignalSpy total(&r.feature, &MessagesFeature::unreadTotalChanged);
        Envelope e;
        e.t = s("sms.changed");
        e.b = QJsonObject{{s("threads"), QJsonArray{3, 4}}, {s("all"), false}, {s("unread"), 6}};
        r.phone.send(e);
        QVERIFY(waitUntil([&] { return changed.count() == 1; }));
        QCOMPARE(r.feature.unreadTotal(), 6);
        QCOMPARE(total.count(), 1);
        QVERIFY(changed.first().at(1).toString().contains(s("\"unread\":6")));

        QJsonArray many;
        for (int i = 0; i < 101; ++i) many.append(i);
        e.b = QJsonObject{{s("threads"), many}, {s("all"), true}, {s("unread"), 99}};
        r.phone.send(e);
        QTest::qWait(150);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(r.feature.unreadTotal(), 6);

        Result read;
        r.feature.markRead(r.id, 3, read.cb());
        Envelope req;
        QVERIFY(r.phone.waitFor(s("sms.read"), &req));
        r.phone.send(typedReply(req, QJsonObject{{s("thread"), 3}, {s("unread"), 2}}));
        QVERIFY(waitUntil([&] { return read.done; }));
        QCOMPARE(r.feature.unreadTotal(), 2);

        r.phone.sendRaw(QByteArray::fromHex("00000000"));
        QVERIFY(waitUntil([&] { return r.feature.unreadTotal() == 0; }));
    }

    void sendStatusMapsPhoneEventsAndIgnoresForgedRefs()
    {
        Rig r;
        QVERIFY(r.ok);
        QSignalSpy status(&r.feature, &MessagesFeature::sendStatus);
        Result bad;
        r.feature.send(r.id, {}, s("x"), -1, bad.cb());
        QCOMPARE(bad.error, s("bad-request"));
        r.feature.send(r.id, {s("1")}, QString(1601, QLatin1Char('a')), -1, bad.cb());
        QCOMPARE(bad.error, s("bad-request"));
        QCOMPARE(r.hub.hub.session(r.id)->state(), SessionState::Ready);

        Result res;
        r.feature.send(r.id, {s("+1555")}, s("hello"), 2, res.cb());
        Envelope req;
        QVERIFY(r.phone.waitFor(s("sms.send"), &req));
        QCOMPARE(req.b.value(s("sub")).toInt(), 2);
        r.phone.send(typedReply(req, QJsonObject{{s("ref"), 3}}));
        QVERIFY(waitUntil([&] { return res.done; }));
        QCOMPARE(res.body.value(s("ref")).toInt(), 3);

        auto event = [&](int ref, const char *state, const char *code = nullptr) {
            Envelope e;
            e.t = s("sms.sent");
            e.b = QJsonObject{{s("ref"), ref}, {s("state"), s(state)}};
            if (code != nullptr) e.b.insert(s("code"), s(code));
            r.phone.send(e);
        };
        event(99, "sent");      // never issued by this desktop
        event(3, "sent");
        event(3, "bogus");      // invalid state
        event(3, "failed", "no-service");
        QVERIFY(waitUntil([&] { return status.count() == 2; }));
        QCOMPARE(status.at(0).at(2).toString(), s("sent"));
        QCOMPARE(status.at(1).at(2).toString(), s("failed"));
        QCOMPARE(status.at(1).at(3).toString(), s("no-service"));
    }

    void logsCarryNoMessageContent()
    {
        g_log.clear();
        const QtMessageHandler old = qInstallMessageHandler(capture);
        QLoggingCategory::setFilterRules(s("qindaqt.link.frames.debug=true"));
        {
            Rig r;
            QVERIFY(r.ok);
            Result res;
            r.feature.listMessages(r.id, 1, 5, -1, -1, res.cb());
            Envelope req;
            QVERIFY(r.phone.waitFor(s("sms.messages"), &req));
            QJsonObject m = message(1, s("CANARY-BODY-7f3a"));
            m.insert(s("addr"), s("CANARY-ADDR-91c2"));
            r.phone.send(typedReply(req, QJsonObject{{s("thread"), 1}, {s("more"), false}, {s("messages"), QJsonArray{m}}}));
            QVERIFY(waitUntil([&] { return res.done; }));
            QVERIFY(res.body.value(s("messages")).toArray().first().toObject().value(s("body")).toString().contains(s("CANARY")));
            // A rejected reply must not echo its content either.
            Result rej;
            r.feature.listThreads(r.id, 1, -1, -1, rej.cb());
            QVERIFY(r.phone.waitFor(s("sms.threads"), &req));
            r.phone.send(typedReply(req, QJsonObject{{s("threads"), QJsonArray{QJsonObject{{s("snippet"), s("CANARY-SNIP-55")}}}}}));
            QVERIFY(waitUntil([&] { return rej.done; }));
            QCOMPARE(rej.error, s("bad-reply"));
        }
        qInstallMessageHandler(old);
        QLoggingCategory::setFilterRules(QString());
        QVERIFY2(!g_log.isEmpty(), "frame logging was not captured; the test would prove nothing");
        QVERIFY(g_log.join(QLatin1Char('\n')).contains(s("sms.messages")));
        QVERIFY(!g_log.join(QLatin1Char('\n')).contains(s("CANARY")));
    }
};

QTEST_MAIN(LinkMessagesTest)
#include "tst_link_messages.moc"
