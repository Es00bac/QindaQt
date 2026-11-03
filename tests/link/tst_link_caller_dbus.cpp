// SPDX-License-Identifier: GPL-3.0-or-later
// The real qindaqt-link-service on a private bus (dbus-run-session): an
// untrusted caller (gdbus) is denied every protected member and may use the
// open ones; this test binary, passed with --trusted-caller, succeeds. Also
// the Messages.Send rate limit and notice, caps, PendingPairings.
#include "support/scripted_phone.h"

#include <QtCore/QFile>
#include <QtCore/QProcess>
#include <QtCore/QTemporaryDir>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusReply>
#include <QtTest/QtTest>

using namespace QindaQt::Link;
using namespace QindaQt::Link::Test;

namespace {

constexpr auto kDenied = "org.qindaqt.Link1.Error.Denied";

struct GdbusResult { int code; QString out; };

// gdbus is a different executable than this test, so the service must treat it as untrusted.
GdbusResult gdbus(const QString &iface, const QString &path, const QString &method, const QStringList &args)
{
    QProcess p;
    QStringList a{QStringLiteral("call"), QStringLiteral("--session"), QStringLiteral("--dest"), QStringLiteral("org.qindaqt.Link1"),
                  QStringLiteral("--object-path"), path, QStringLiteral("--method"), iface + QLatin1Char('.') + method};
    a += args;
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start(QStringLiteral("/usr/bin/gdbus"), a);
    if (!p.waitForFinished(15000)) { p.kill(); return {-1, {}}; }
    return {p.exitCode(), QString::fromUtf8(p.readAll())};
}

QString iface(const char *suffix = "") { return QStringLiteral("org.qindaqt.Link1") + QLatin1String(suffix); }

} // namespace

class LinkCallerDbusTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void policyAndRateLimitOnTheRealService()
    {
        QVERIFY2(!qEnvironmentVariableIsEmpty("DBUS_SESSION_BUS_ADDRESS"), "run under dbus-run-session");
        QVERIFY(QFile::exists(QStringLiteral("/usr/bin/gdbus")));
        QTemporaryDir data, cfg, cache;
        const QString rec = data.filePath(QStringLiteral("notices.jsonl"));
        QProcess service;
        service.setProcessChannelMode(QProcess::ForwardedChannels);
        service.start(QStringLiteral(QINDAQT_LINK_SERVICE_BINARY),
                      {QStringLiteral("--data-dir"), data.path(), QStringLiteral("--config-dir"), cfg.path(),
                       QStringLiteral("--cache-dir"), cache.path(), QStringLiteral("--port"), QStringLiteral("0"),
                       QStringLiteral("--no-adb"), QStringLiteral("--no-notifications"),
                       QStringLiteral("--recording-notifier"), rec,
                       QStringLiteral("--trusted-caller"), QCoreApplication::applicationFilePath()});
        QVERIFY(service.waitForStarted());

        QDBusConnection bus = QDBusConnection::sessionBus();
        const QString root = QStringLiteral("/org/qindaqt/Link1");
        QDBusInterface link(iface(), root, iface(), bus);
        QDBusInterface msgs(iface(), root + QStringLiteral("/Messages"), iface(".Messages"), bus);
        QDBusInterface notif(iface(), root, iface(".Notifications"), bus);
        QVERIFY(waitUntil([&] { return link.isValid(); }, 10000));
        const quint16 port = QDBusReply<quint16>(link.call(QStringLiteral("ListenPort"))).value();
        QVERIFY(port != 0 && port != 47160);

        // --- untrusted caller: protected members are denied, open ones work
        const QString dev = QString(64, QLatin1Char('a'));
        struct Case { QString iface, path, method; QStringList args; };
        const QList<Case> protectedCalls = {
            {iface(), root, QStringLiteral("ConfirmPairing"), {dev, QStringLiteral("true")}},
            {iface(), root, QStringLiteral("Unpair"), {dev}},
            {iface(), root, QStringLiteral("Ring"), {dev}},
            {iface(".Messages"), root + QStringLiteral("/Messages"), QStringLiteral("ListThreads"), {dev, QStringLiteral("10"), QStringLiteral("0"), QStringLiteral("0")}},
            {iface(".Messages"), root + QStringLiteral("/Messages"), QStringLiteral("ListMessages"), {dev, QStringLiteral("1"), QStringLiteral("10"), QStringLiteral("0"), QStringLiteral("''"), QStringLiteral("0")}},
            {iface(".Messages"), root + QStringLiteral("/Messages"), QStringLiteral("GetPart"), {dev, QStringLiteral("1"), QStringLiteral("100")}},
            {iface(".Messages"), root + QStringLiteral("/Messages"), QStringLiteral("Send"), {dev, QStringLiteral("['+15550100']"), QStringLiteral("hi"), QStringLiteral("0")}},
            {iface(".Messages"), root + QStringLiteral("/Messages"), QStringLiteral("MarkRead"), {dev, QStringLiteral("1")}},
            {iface(".Notifications"), root, QStringLiteral("ListApps"), {}},
            {iface(".Notifications"), root, QStringLiteral("SetAppMuted"), {QStringLiteral("pkg"), QStringLiteral("true")}},
            {iface(".Notifications"), root, QStringLiteral("Reply"), {dev, QStringLiteral("k"), QStringLiteral("1"), QStringLiteral("t")}},
            {iface(".Notifications"), root, QStringLiteral("Current"), {}},
            {iface(".Setup"), root, QStringLiteral("AdbDevices"), {}},
            {iface(".Setup"), root, QStringLiteral("SetupPhone"), {QStringLiteral("DEMO0001")}},
            {iface(".Setup"), root, QStringLiteral("SetupStatus"), {QStringLiteral("DEMO0001")}},
        };
        for (const Case &c : protectedCalls) {
            const GdbusResult r = gdbus(c.iface, c.path, c.method, c.args);
            QVERIFY2(r.code != 0 && r.out.contains(QLatin1String(kDenied)), qPrintable(c.method + QLatin1Char(' ') + r.out));
        }
        for (const QString &open : {QStringLiteral("Devices"), QStringLiteral("PendingPairings"), QStringLiteral("ListenPort")}) {
            const GdbusResult r = gdbus(iface(), root, open, {});
            QVERIFY2(r.code == 0, qPrintable(open + QLatin1Char(' ') + r.out));
        }
        QVERIFY(gdbus(QStringLiteral("org.freedesktop.DBus.Properties"), root + QStringLiteral("/Messages"), QStringLiteral("Get"),
                      {iface(".Messages"), QStringLiteral("UnreadCount")}).code == 0);

        // --- trusted caller (this binary): reaches the member (the answer may be any error but Denied)
        QVERIFY(QDBusReply<QString>(notif.call(QStringLiteral("ListApps"))).isValid());
        QVERIFY(QDBusReply<QString>(notif.call(QStringLiteral("Current"))).isValid());
        // Setup with a trusted caller is not Denied (the service runs --no-adb here, so NotConnected).
        const QDBusMessage setup = QDBusInterface(QStringLiteral("org.qindaqt.Link1"), root, iface(".Setup")).call(QStringLiteral("SetupStatus"), QStringLiteral("DEMO0001"));
        QVERIFY(setup.errorName() != QLatin1String(kDenied));
        const QDBusMessage threads = msgs.call(QStringLiteral("ListThreads"), dev, 10, qlonglong(0), qlonglong(0));
        QVERIFY(threads.type() == QDBusMessage::ErrorMessage && threads.errorName() != QLatin1String(kDenied));
        link.call(QStringLiteral("Ring"), dev);  // void, must not be an error reply
        QVERIFY(QDBusReply<QString>(notif.call(QStringLiteral("Reply"), dev, QStringLiteral("k"), QStringLiteral("1"), QStringLiteral("t"))).isValid());

        // --- caps and PendingPairings
        ScriptedPhone phone;
        QVERIFY(phone.connectTo(port) && phone.waitFor(QStringLiteral("hello")));
        phone.sendHello(false, {QStringLiteral("status"), QStringLiteral("sms")});
        QString code;
        QVERIFY(phone.pairUntilCode(&code));
        QJsonArray pending;
        QVERIFY(waitUntil([&] {
            pending = QJsonDocument::fromJson(gdbus(iface(), root, QStringLiteral("PendingPairings"), {}).out.section(QLatin1Char('\''), 1, -2)
                                                  .replace(QLatin1String("\\\""), QLatin1String("\"")).toUtf8()).array();
            return !pending.isEmpty();
        }));
        QCOMPARE(pending.first().toObject().value(QStringLiteral("code")).toString(), code);
        QCOMPARE(pending.first().toObject().value(QStringLiteral("id")).toString(), phone.deviceId());
        link.call(QStringLiteral("ConfirmPairing"), phone.deviceId(), true);
        Envelope confirm;
        QVERIFY(phone.waitFor(QStringLiteral("pair.confirm"), &confirm));
        Envelope accept;
        accept.t = QStringLiteral("pair.confirm");
        accept.b = QJsonObject{{QStringLiteral("accept"), true}};
        phone.send(accept);
        QJsonObject device;
        QVERIFY(waitUntil([&] {
            const QString json = QDBusReply<QString>(link.call(QStringLiteral("Devices"))).value();
            const QJsonArray list = QJsonDocument::fromJson(json.toUtf8()).array();
            if (list.isEmpty() || !list.first().toObject().value(QStringLiteral("ready")).toBool()) return false;
            device = list.first().toObject();
            return true;
        }));
        const QJsonArray caps = device.value(QStringLiteral("caps")).toArray();
        QVERIFY(caps.contains(QStringLiteral("sms")) && caps.contains(QStringLiteral("status")));
        QVERIFY(QJsonDocument::fromJson(QDBusReply<QString>(link.call(QStringLiteral("PendingPairings"))).value().toUtf8()).array().isEmpty());

        // --- Send: five accepted (each announced), the sixth is rate limited
        QStringList errors;
        int finished = 0;
        QList<QDBusPendingCallWatcher *> watchers;
        for (int i = 0; i < 6; ++i) {
            auto *w = new QDBusPendingCallWatcher(
                msgs.asyncCall(QStringLiteral("Send"), phone.deviceId(), QStringList{QStringLiteral("+15550100")},
                               QStringLiteral("hello %1").arg(i), 0), this);
            connect(w, &QDBusPendingCallWatcher::finished, this, [&](QDBusPendingCallWatcher *done) {
                ++finished;
                const QDBusPendingReply<QString> r = *done;
                if (r.isError()) errors.append(r.error().name());
            });
            watchers << w;
        }
        QVERIFY(waitUntil([&] { return errors.contains(QStringLiteral("org.qindaqt.Link1.Error.RateLimited")); }));
        QCOMPARE(errors.count(QStringLiteral("org.qindaqt.Link1.Error.RateLimited")), 1);
        int notices = 0;
        QVERIFY(waitUntil([&] {
            QFile f(rec);
            if (!f.open(QIODevice::ReadOnly)) return false;
            notices = int(f.readAll().count("\"app\":\"QindaLink\""));
            return notices >= 5;
        }));
        QCOMPARE(notices, 5);
        qDeleteAll(watchers);

        service.terminate();  // our own child, by handle
        QVERIFY(service.waitForFinished(5000));
    }
};

QTEST_GUILESS_MAIN(LinkCallerDbusTest)
#include "tst_link_caller_dbus.moc"
