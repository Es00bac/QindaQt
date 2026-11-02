// SPDX-License-Identifier: GPL-3.0-or-later
// org.qindaqt.Link1.Messages over a private session bus (run under
// dbus-run-session): delayed replies, error mapping, signals and the
// UnreadCount property. The "service" and the client are two connections of
// this process; the phone is a scripted in-process peer.
#include "support/hub_fixture.h"

#include <qindaqt/services/link_messages/messages_adaptor.h>

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusPendingReply>

using namespace QindaQt::Link;
using namespace QindaQt::Link::Test;

namespace {
QString s(const char *v) { return QString::fromLatin1(v); }
constexpr auto kPath = "/org/qindaqt/Link1/Messages";
constexpr auto kIface = "org.qindaqt.Link1.Messages";

// QDBusInterface would introspect synchronously and stall the service sharing this thread.
QDBusPendingCall callMethod(QDBusConnection &cli, const char *method, const QVariantList &args)
{
    QDBusMessage m = QDBusMessage::createMethodCall(s("org.qindaqt.Link1.MessagesTest"), s(kPath), s(kIface), s(method));
    m.setArguments(args);
    return cli.asyncCall(m);
}

// The service shares this thread, so property reads must not block it.
int unreadCount(QDBusConnection &cli)
{
    QDBusMessage m = QDBusMessage::createMethodCall(s("org.qindaqt.Link1.MessagesTest"), s(kPath),
                                                    s("org.freedesktop.DBus.Properties"), s("Get"));
    m << s(kIface) << s("UnreadCount");
    QDBusPendingCall call = cli.asyncCall(m);
    QDBusPendingCallWatcher w(call);
    bool done = false;
    QObject::connect(&w, &QDBusPendingCallWatcher::finished, &w, [&] { done = true; });
    if (!waitUntil([&] { return done; })) return -1;
    QDBusPendingReply<QDBusVariant> r = call;
    return r.isValid() ? r.value().variant().toInt() : -2;
}
}

class LinkMessagesDbusTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void methodsSignalsAndProperty()
    {
        QDBusConnection svc = QDBusConnection::connectToBus(QDBusConnection::SessionBus, s("svc"));
        QDBusConnection cli = QDBusConnection::connectToBus(QDBusConnection::SessionBus, s("cli"));
        QVERIFY(svc.isConnected() && cli.isConnected());
        HubFixture hub;
        MessagesFeature feature([&](const QString &dev) { return hub.hub.session(dev); });
        hub.hub.addFeature(&feature);
        new MessagesAdaptor(&feature, svc);
        QVERIFY(svc.registerObject(s(kPath), &feature, QDBusConnection::ExportAdaptors));
        QVERIFY(svc.registerService(s("org.qindaqt.Link1.MessagesTest")));
        ScriptedPhone phone;
        QVERIFY(hub.pair(phone, {s("status"), s("sms")}));

        // Delayed reply: the phone answers after the call was issued.
        QDBusPendingCall call = callMethod(cli, "ListThreads", {phone.deviceId(), 10, qlonglong(-1), qlonglong(-1)});
        QDBusPendingCallWatcher w(call);
        bool finished = false;
        connect(&w, &QDBusPendingCallWatcher::finished, this, [&] { finished = true; });
        Envelope req;
        QVERIFY(phone.waitFor(s("sms.threads"), &req));
        QVERIFY(!finished);
        Envelope reply;
        reply.t = req.t;
        reply.re = req.id;
        reply.b = QJsonObject{{s("threads"), QJsonArray{}}, {s("more"), false}, {s("unread"), 4}};
        phone.send(reply);
        QVERIFY(waitUntil([&] { return finished; }));
        QDBusPendingReply<QString> r = call;
        QVERIFY(r.isValid());
        QCOMPARE(QJsonDocument::fromJson(r.value().toUtf8()).object().value(s("unread")).toInt(), 4);
        QVERIFY(waitUntil([&] { return unreadCount(cli) == 4; }));

        // Error mapping: phone denial becomes org.qindaqt.Link1.Error.Denied.
        QDBusPendingCall bad = callMethod(cli, "MarkRead", {phone.deviceId(), qlonglong(1)});
        QDBusPendingCallWatcher w2(bad);
        bool done2 = false;
        connect(&w2, &QDBusPendingCallWatcher::finished, this, [&] { done2 = true; });
        QVERIFY(phone.waitFor(s("sms.read"), &req));
        Envelope err;
        err.t = s("error");
        err.re = req.id;
        err.b = QJsonObject{{s("code"), s("denied")}};
        phone.send(err);
        QVERIFY(waitUntil([&] { return done2; }));
        QDBusPendingReply<QString> br = bad;
        QVERIFY(br.isError());
        QCOMPARE(br.error().name(), s("org.qindaqt.Link1.Error.Denied"));

        QDBusPendingCall none = callMethod(cli, "ListMessages", {s("nobody"), qlonglong(1), 5, qlonglong(-1), qlonglong(-1)});
        QDBusPendingCallWatcher w3(none);
        bool done3 = false;
        connect(&w3, &QDBusPendingCallWatcher::finished, this, [&] { done3 = true; });
        QVERIFY(waitUntil([&] { return done3; }));
        QCOMPARE(QDBusPendingReply<QString>(none).error().name(), s("org.qindaqt.Link1.Error.NotConnected"));

        // Signal from a phone event.
        QStringList seen;
        QVERIFY(cli.connect(s("org.qindaqt.Link1.MessagesTest"), s(kPath), s(kIface), s("ThreadsChanged"), this, SLOT(onChanged(QString, QString))));
        m_changed = &seen;
        Envelope ev;
        ev.t = s("sms.changed");
        ev.b = QJsonObject{{s("threads"), QJsonArray{9}}, {s("all"), false}, {s("unread"), 1}};
        phone.send(ev);
        QVERIFY(waitUntil([&] { return !seen.isEmpty(); }));
        QVERIFY(seen.first().contains(s("[9]")));
        QVERIFY(waitUntil([&] { return unreadCount(cli) == 1; }));
        m_changed = nullptr;
        svc.unregisterObject(s(kPath));
    }

public Q_SLOTS:
    void onChanged(const QString &, const QString &json)
    {
        if (m_changed != nullptr) m_changed->append(json);
    }

private:
    QStringList *m_changed = nullptr;
};

QTEST_MAIN(LinkMessagesDbusTest)
#include "tst_link_messages_dbus.moc"
