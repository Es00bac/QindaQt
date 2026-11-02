// SPDX-License-Identifier: GPL-3.0-or-later
// Desktop notify feature against a real Hub and a scripted phone, with a fake
// presenter: no notification server, no real state directory, ephemeral port.
#include "support/hub_fixture.h"

#include <qindaqt/services/link_notifications/app_settings.h>
#include <qindaqt/services/link_notifications/icon_cache.h>
#include <qindaqt/services/link_notifications/notification_feature.h>
#include <qindaqt/services/link_notifications/process_reply_launcher.h>

#include <QtCore/QCryptographicHash>
#include <QtCore/QJsonArray>
#include <QtTest/QtTest>

using namespace QindaQt::Link;
using namespace QindaQt::Link::Test;

namespace {

constexpr auto kSecretTitle = "TITLE-SECRET-9f3a";
constexpr auto kSecretText = "TEXT-SECRET-77c1";

QByteArray testPng()
{
    return QByteArray("\x89PNG\r\n\x1a\n", 8) + QByteArray("fake-png-body-for-hash-test");
}
QString hashOf(const QByteArray &b) { return QString::fromLatin1(QCryptographicHash::hash(b, QCryptographicHash::Sha256).toHex()); }

class FakePresenter final : public NotificationPresenter {
public:
    struct Shown {
        quint32 handle;
        quint32 replaces;
        PresentedNotification n;
    };
    quint32 present(const PresentedNotification &n, quint32 replaces) override
    {
        const quint32 h = replaces != 0 ? replaces : ++m_next;
        shown.append({h, replaces, n});
        return h;
    }
    void close(quint32 handle) override { closed.append(handle); }
    QList<Shown> shown;
    QList<quint32> closed;

private:
    quint32 m_next = 0;
};

class FakeLauncher final : public ReplyLauncher {
public:
    bool available() const override { return on; }
    void launch(const QString &d, const QString &k, const QString &a) override { launched.append({d, k, a}); }
    bool on = true;
    QList<QStringList> launched;
};

QJsonObject note(const QString &key, const QString &pkg = QStringLiteral("com.chat"), const QString &icon = {},
                 bool reply = true)
{
    QJsonArray actions{QJsonObject{{"id", "0"}, {"title", "Archive"}, {"reply", false}}};
    if (reply) actions.append(QJsonObject{{"id", "1"}, {"title", "Reply"}, {"reply", true}});
    return {{"key", key}, {"pkg", pkg}, {"app", "Chat"}, {"title", QString::fromLatin1(kSecretTitle)},
            {"text", QString::fromLatin1(kSecretText)}, {"when", 1767225600000.0}, {"icon", icon}, {"actions", actions}};
}

Envelope frame(const QString &t, const QJsonObject &b, qint64 id = 0)
{
    Envelope e;
    e.t = t;
    e.b = b;
    e.id = id;
    return e;
}

QStringList g_logs;
void captureLog(QtMsgType, const QMessageLogContext &, const QString &msg) { g_logs << msg; }

} // namespace

class LinkNotificationsTest : public QObject {
    Q_OBJECT
    struct Rig {
        explicit Rig(int iconTimeoutMs = 10'000, int actionGraceMs = 2'000)
            : icons(dir.path() + "/icons"), apps(dir.path() + "/notifications.json"),
              feature(NotificationFeatureDeps{&presenter, &icons, &apps, &launcher,
                                              [this](const QString &id) { return fixture.hub.session(id); },
                                              iconTimeoutMs, actionGraceMs})
        {
            fixture.hub.addFeature(&feature);
        }
        bool connectPhone()
        {
            return fixture.pair(phone, {QStringLiteral("notify")})
                && waitUntil([&] {
                       Session *s = fixture.hub.session(phone.deviceId());
                       return s != nullptr && s->state() == SessionState::Ready;
                   });
        }
        QTemporaryDir dir;
        HubFixture fixture;
        FakePresenter presenter;
        FakeLauncher launcher;
        IconCache icons;
        AppSettings apps;
        NotificationFeature feature;
        ScriptedPhone phone;
    };

private Q_SLOTS:
    void postUpdateAndRemove()
    {
        Rig r;
        QVERIFY(r.connectPhone());
        r.phone.send(frame("notify.posted", note("k1")));
        QVERIFY(waitUntil([&] { return r.presenter.shown.size() == 1; }));
        const auto first = r.presenter.shown.first();
        QCOMPARE(first.n.appName, QStringLiteral("Chat (phone)"));
        QCOMPARE(first.n.summary, QString::fromLatin1(kSecretTitle));
        QCOMPARE(first.n.actions.size(), 2);
        QCOMPARE(first.n.actions[0].first, QStringLiteral("a:0"));
        QCOMPARE(first.n.actions[1].first, QStringLiteral("r:1"));
        QCOMPARE(first.n.actions[1].second, QStringLiteral("Reply…"));

        QJsonObject changed = note("k1");
        changed["text"] = "newer";
        r.phone.send(frame("notify.posted", changed));
        QVERIFY(waitUntil([&] { return r.presenter.shown.size() == 2; }));
        QCOMPARE(r.presenter.shown[1].replaces, first.handle);
        r.phone.send(frame("notify.posted", changed));  // identical: no new presentation
        r.phone.send(frame("notify.removed", {{"key", "k1"}}));
        QVERIFY(waitUntil([&] { return !r.presenter.closed.isEmpty(); }));
        QCOMPARE(r.presenter.closed, QList<quint32>{first.handle});
        QCOMPARE(r.presenter.shown.size(), 2);
        QVERIFY(r.feature.current().isEmpty());
    }

    void userDismissMapsToNotifyDismissAndRemovedDoesNot()
    {
        Rig r;
        QVERIFY(r.connectPhone());
        r.phone.send(frame("notify.posted", note("k1")));
        r.phone.send(frame("notify.posted", note("k2")));
        QVERIFY(waitUntil([&] { return r.presenter.shown.size() == 2; }));
        Q_EMIT r.presenter.userDismissed(r.presenter.shown[0].handle);
        Envelope d;
        QVERIFY(r.phone.waitFor("notify.dismiss", &d));
        QCOMPARE(d.b.value("key").toString(), QStringLiteral("k1"));
        // phone-side removal closes silently and sends nothing back
        r.phone.send(frame("notify.removed", {{"key", "k2"}}));
        QVERIFY(waitUntil([&] { return r.presenter.closed.size() == 1; }));
        QTest::qWait(100);
        QCOMPARE(r.phone.count("notify.dismiss"), 1);
        // a removal for a notification the desktop already dropped is ignored
        r.phone.send(frame("notify.removed", {{"key", "k1"}}));
        QTest::qWait(50);
        QCOMPARE(r.presenter.closed.size(), 1);
    }

    void actionButtonSendsNotifyActionAndKeepsPhoneNotificationOnClose()
    {
        Rig r;
        QVERIFY(r.connectPhone());
        r.phone.send(frame("notify.posted", note("k1")));
        QVERIFY(waitUntil([&] { return r.presenter.shown.size() == 1; }));
        const quint32 h = r.presenter.shown[0].handle;
        Q_EMIT r.presenter.actionInvoked(h, QStringLiteral("a:0"));
        Envelope a;
        QVERIFY(r.phone.waitFor("notify.action", &a));
        QCOMPARE(a.b.value("key").toString(), QStringLiteral("k1"));
        QCOMPARE(a.b.value("action").toString(), QStringLiteral("0"));
        QCOMPARE(a.b.value("title").toString(), QStringLiteral("Archive"));  // the phone checks it against its current action
        QVERIFY(!a.b.contains("text"));
        QVERIFY(a.id != 0);
        // the server closes it after the click (reason 2): not a dismissal
        Q_EMIT r.presenter.userDismissed(h);
        QTest::qWait(100);
        QCOMPARE(r.phone.count("notify.dismiss"), 0);
    }

    void replyActionLaunchesTheAppAndIsHiddenWithoutIt()
    {
        Rig r;
        QVERIFY(r.connectPhone());
        r.phone.send(frame("notify.posted", note("k1")));
        QVERIFY(waitUntil([&] { return r.presenter.shown.size() == 1; }));
        Q_EMIT r.presenter.actionInvoked(r.presenter.shown[0].handle, QStringLiteral("r:1"));
        QCOMPARE(r.launcher.launched.size(), 1);
        QCOMPARE(r.launcher.launched[0], (QStringList{r.phone.deviceId(), "k1", "1"}));
        QCOMPARE(r.phone.count("notify.action"), 0);

        r.launcher.on = false;
        r.phone.send(frame("notify.posted", note("k2")));
        QVERIFY(waitUntil([&] { return r.presenter.shown.size() == 2; }));
        QCOMPARE(r.presenter.shown[1].n.actions.size(), 1);
    }

    void replyThroughLink1SendsTextAndReportsPhoneError()
    {
        Rig r;
        QVERIFY(r.connectPhone());
        r.phone.send(frame("notify.posted", note("k1")));
        QVERIFY(waitUntil([&] { return r.presenter.shown.size() == 1; }));
        QString result = QStringLiteral("pending");
        r.feature.reply(r.phone.deviceId(), "k1", "1", "hello there", [&](const QString &e) { result = e; });
        Envelope a;
        QVERIFY(r.phone.waitFor("notify.action", &a));
        QCOMPARE(a.b.value("text").toString(), QStringLiteral("hello there"));
        Envelope ok;
        ok.t = "ok";
        ok.re = a.id;
        r.phone.send(ok);
        QVERIFY(waitUntil([&] { return result.isNull() || result.isEmpty(); }));

        result = QStringLiteral("pending");
        r.feature.reply(r.phone.deviceId(), "k1", "1", "again", [&](const QString &e) { result = e; });
        Envelope b;
        QVERIFY(r.phone.waitFor("notify.action", &b));
        r.phone.send(makeError("target-changed", "gone", b.id));
        QVERIFY(waitUntil([&] { return result != QLatin1String("pending"); }));
        QCOMPARE(result, QStringLiteral("target-changed"));

        // local checks
        r.feature.reply(r.phone.deviceId(), "k1", "9", {}, [&](const QString &e) { result = e; });
        QCOMPARE(result, QStringLiteral("not-found"));
        r.feature.reply(r.phone.deviceId(), "k1", "0", "text for a non-reply", [&](const QString &e) { result = e; });
        QCOMPARE(result, QStringLiteral("bad-request"));
        r.feature.reply(r.phone.deviceId(), "k1", "1", QString(4097, QLatin1Char('x')), [&](const QString &e) { result = e; });
        QCOMPARE(result, QStringLiteral("bad-request"));
    }

    void muteSuppressesAndPersists()
    {
        Rig r;
        QVERIFY(r.connectPhone());
        r.phone.send(frame("notify.posted", note("k1", "com.chat")));
        QVERIFY(waitUntil([&] { return r.presenter.shown.size() == 1; }));
        r.feature.setAppMuted("com.chat", true);
        QCOMPARE(r.presenter.closed.size(), 1);  // already-shown notifications of the app close
        r.phone.send(frame("notify.posted", note("k2", "com.chat")));
        r.phone.send(frame("notify.posted", note("k3", "com.other")));
        QVERIFY(waitUntil([&] { return r.presenter.shown.size() == 2; }));
        QCOMPARE(r.presenter.shown[1].n.appName, QStringLiteral("Chat (phone)"));
        QCOMPARE(r.feature.current().size(), 1);
        AppSettings reread(r.dir.path() + "/notifications.json");
        QVERIFY(reread.isMuted("com.chat"));
        QVERIFY(!reread.isMuted("com.other"));
        const QJsonArray apps = r.feature.listApps();
        QCOMPARE(apps.size(), 2);
        r.feature.setAppMuted("com.chat", false);
        QVERIFY(!AppSettings(r.dir.path() + "/notifications.json").isMuted("com.chat"));
    }

    void snapshotReconcilesWithoutRepopping()
    {
        Rig r;
        QVERIFY(r.connectPhone());
        r.phone.send(frame("notify.snapshot", {{"items", QJsonArray{note("k1"), note("k2")}}}));
        QVERIFY(waitUntil([&] { return r.presenter.shown.size() == 2; }));
        QVERIFY(r.presenter.shown[0].n.quiet);
        r.phone.send(frame("notify.snapshot", {{"items", QJsonArray{note("k2"), note("k3")}}}));
        QVERIFY(waitUntil([&] { return r.presenter.shown.size() == 3; }));
        QCOMPARE(r.presenter.closed.size(), 1);  // k1 gone, k2 untouched, k3 new
    }

    void boundsAndBadFramesAreRejected()
    {
        Rig r;
        QVERIFY(r.connectPhone());
        QJsonObject longTitle = note("k1");
        longTitle["title"] = QString(257, QLatin1Char('t'));
        QJsonObject nineActions = note("k2");
        QJsonArray many;
        for (int i = 0; i < 9; ++i) many.append(QJsonObject{{"id", QString::number(i)}, {"title", "x"}, {"reply", false}});
        nineActions["actions"] = many;
        QJsonObject badIcon = note("k3");
        badIcon["icon"] = "../../etc/passwd";
        QJsonObject longKey = note(QString(513, QLatin1Char('k')));
        for (const QJsonObject &b : {longTitle, nineActions, badIcon, longKey}) {
            r.phone.send(frame("notify.posted", b, 5));
            Envelope err;
            QVERIFY(r.phone.waitFor("error", &err));
            QCOMPARE(err.b.value("code").toString(), QStringLiteral("bad-request"));
        }
        QJsonArray fiftyOne;
        for (int i = 0; i < 51; ++i) fiftyOne.append(note(QString::number(i)));
        r.phone.send(frame("notify.snapshot", {{"items", fiftyOne}}, 6));
        Envelope err;
        QVERIFY(r.phone.waitFor("error", &err));
        r.phone.send(frame("notify.dismiss", {{"key", "k"}}, 7));  // wrong direction
        QVERIFY(r.phone.waitFor("error", &err));
        QCOMPARE(err.b.value("code").toString(), QStringLiteral("unsupported"));
        QVERIFY(r.presenter.shown.isEmpty());

        // more than 100 tracked: the oldest closes
        for (int i = 0; i < 101; ++i) r.phone.send(frame("notify.posted", note(QString("n%1").arg(i))));
        QVERIFY(waitUntil([&] { return r.presenter.shown.size() == 101; }));
        QCOMPARE(r.presenter.closed.size(), 1);
        QCOMPARE(r.feature.current().size(), 100);
    }

    void iconIsFetchedVerifiedCachedAndUsed()
    {
        Rig r;
        QVERIFY(r.connectPhone());
        const QByteArray png = testPng();
        const QString hash = hashOf(png);
        r.phone.send(frame("notify.posted", note("k1", "com.chat", hash)));
        Envelope req;
        QVERIFY(r.phone.waitFor("notify.icon", &req));
        QCOMPARE(req.b.value("hash").toString(), hash);
        QVERIFY(r.presenter.shown.isEmpty());  // waits for the icon
        // a PNG with the wrong hash is refused and the notification shows without icon
        r.phone.send(frame("notify.icon", {{"hash", hash}, {"png", QString::fromLatin1((png + "x").toBase64())}}));
        QVERIFY(waitUntil([&] { return r.presenter.shown.size() == 1; }));
        QVERIFY(r.presenter.shown[0].n.iconPath.isEmpty());
        QVERIFY(!r.icons.has(hash));
        // a second post asks again, now with the right bytes
        r.phone.send(frame("notify.posted", note("k2", "com.chat", hash)));
        QVERIFY(r.phone.waitFor("notify.icon", &req));
        r.phone.send(frame("notify.icon", {{"hash", hash}, {"png", QString::fromLatin1(png.toBase64())}}));
        QVERIFY(waitUntil([&] { return r.presenter.shown.size() == 2; }));
        QVERIFY(r.icons.has(hash));
        QCOMPARE(r.presenter.shown[1].n.iconPath, r.icons.path(hash));
        QVERIFY(r.presenter.shown[1].n.iconPath.startsWith(r.dir.path()));
        // cached: no further request
        r.phone.send(frame("notify.posted", note("k3", "com.chat", hash)));
        QVERIFY(waitUntil([&] { return r.presenter.shown.size() == 3; }));
        QCOMPARE(r.phone.count("notify.icon"), 2);
    }

    void iconTimeoutShowsWithoutIcon()
    {
        Rig r(150);
        QVERIFY(r.connectPhone());
        r.phone.send(frame("notify.posted", note("k1", "com.chat", hashOf(testPng()))));
        QVERIFY(waitUntil([&] { return r.presenter.shown.size() == 1; }));
        QVERIFY(r.presenter.shown[0].n.iconPath.isEmpty());
    }

    void unverifiedActivityLaunchIsReportedNotSuccess()
    {
        Rig r;
        QVERIFY(r.connectPhone());
        r.phone.send(frame("notify.posted", note("k1")));
        QVERIFY(waitUntil([&] { return r.presenter.shown.size() == 1; }));
        QString result = QStringLiteral("pending");
        r.feature.reply(r.phone.deviceId(), "k1", "0", {}, [&](const QString &e) { result = e; });
        Envelope a;
        QVERIFY(r.phone.waitFor("notify.action", &a));
        r.phone.send(makeError("unsupported", "unverified", a.id));
        QVERIFY(waitUntil([&] { return result != QLatin1String("pending"); }));
        QCOMPARE(result, QStringLiteral("unverified"));
    }

    void closeAfterActionIsIgnoredOnlyBrieflyThenDismissalCounts()
    {
        Rig r(10'000, 150);
        QVERIFY(r.connectPhone());
        r.phone.send(frame("notify.posted", note("k1")));
        QVERIFY(waitUntil([&] { return r.presenter.shown.size() == 1; }));
        const quint32 h = r.presenter.shown[0].handle;
        Q_EMIT r.presenter.actionInvoked(h, QStringLiteral("a:0"));
        QTest::qWait(400);  // the server never closed it; the grace period lapses
        Q_EMIT r.presenter.userDismissed(h);
        Envelope d;
        QVERIFY(r.phone.waitFor("notify.dismiss", &d));
        QCOMPARE(d.b.value("key").toString(), QStringLiteral("k1"));
    }

    void changesAreAnnouncedPerEntryNotAsTheWholeSet()
    {
        Rig r;
        QVERIFY(r.connectPhone());
        QList<QJsonObject> items;
        connect(&r.feature, &NotificationFeature::entryChanged, this, [&](const QJsonObject &o) { items.append(o); });
        for (int i = 0; i < 5; ++i) r.phone.send(frame("notify.posted", note(QString("k%1").arg(i))));
        QVERIFY(waitUntil([&] { return items.size() == 5; }));
        items.clear();
        r.phone.send(frame("notify.removed", {{"key", "k2"}}));
        QVERIFY(waitUntil([&] { return !items.isEmpty(); }));
        QCOMPARE(items.size(), 1);
        QVERIFY(items[0].value("removed").toBool());
        QCOMPARE(items[0].value("key").toString(), QStringLiteral("k2"));
        QVERIFY(!items[0].contains("title"));
        items.clear();
        QJsonObject changed = note("k3");
        changed["text"] = "other";
        r.phone.send(frame("notify.posted", changed));
        QVERIFY(waitUntil([&] { return !items.isEmpty(); }));
        QCOMPARE(items.size(), 1);
        QCOMPARE(items[0].value("key").toString(), QStringLiteral("k3"));
    }

    void iconCacheKeepsAtMostMaxEntriesEvictingLeastRecentlyUsed()
    {
        QTemporaryDir dir;
        IconCache c(dir.path() + "/icons", 3);
        QList<QByteArray> pngs;
        QStringList hashes;
        for (int i = 0; i < 5; ++i) {
            pngs.append(testPng() + QByteArray::number(i));
            hashes.append(hashOf(pngs.last()));
        }
        QVERIFY(c.store(hashes[0], pngs[0]));
        QTest::qWait(1100);  // mtime granularity
        QVERIFY(c.store(hashes[1], pngs[1]));
        QTest::qWait(1100);
        QVERIFY(c.store(hashes[2], pngs[2]));
        QTest::qWait(1100);
        QVERIFY(c.has(hashes[0]));  // refreshes 0: now 1 is the least recently used
        QVERIFY(c.store(hashes[3], pngs[3]));
        QVERIFY(!c.has(hashes[1]));
        QVERIFY(c.has(hashes[0]) && c.has(hashes[2]) && c.has(hashes[3]));
        QCOMPARE(QDir(dir.path() + "/icons").entryList({"*.png"}).size(), 3);
    }

    void replyLauncherSeparatesPhoneControlledArgumentsWithDoubleDash()
    {
        QTemporaryDir dir;
        const QString script = dir.path() + "/fake-link.sh";
        QFile f(script);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QString("#!/bin/sh\nfor a in \"$@\"; do echo \"$a\"; done > '%1/args'\n").arg(dir.path()).toUtf8());
        f.close();
        f.setPermissions(f.permissions() | QFileDevice::ExeOwner);
        ProcessReplyLauncher launcher(script);
        launcher.launch("dev", "-x--evil", "1");
        QVERIFY(waitUntil([&] { return QFile::exists(dir.path() + "/args") && QFileInfo(dir.path() + "/args").size() > 0; }));
        QFile args(dir.path() + "/args");
        QVERIFY(args.open(QIODevice::ReadOnly));
        QCOMPARE(QString::fromUtf8(args.readAll()).split('\n', Qt::SkipEmptyParts),
                 (QStringList{"--reply", "--", "dev", "-x--evil", "1"}));
    }

    void iconCacheRejectsTraversalAndNonPng()
    {
        QTemporaryDir dir;
        IconCache c(dir.path() + "/icons");
        QVERIFY(c.path("../x").isEmpty());
        QVERIFY(!c.store(hashOf("abc"), "abc"));
        QVERIFY(c.store(hashOf(testPng()), testPng()));
        QCOMPARE(QFileInfo(c.path(hashOf(testPng()))).dir().path(), dir.path() + "/icons");
    }

    void contentNeverReachesLogs()
    {
        Rig r;
        QVERIFY(r.connectPhone());
        QLoggingCategory::setFilterRules(QStringLiteral("qindaqt.link.*.debug=true"));
        g_logs.clear();
        const QtMessageHandler old = qInstallMessageHandler(captureLog);
        r.phone.send(frame("notify.posted", note("k1")));
        QVERIFY(waitUntil([&] { return r.presenter.shown.size() == 1; }));
        r.feature.reply(r.phone.deviceId(), "k1", "1", "REPLY-SECRET-5d", [](const QString &) {});
        Envelope a;
        QVERIFY(r.phone.waitFor("notify.action", &a));
        r.phone.send(makeError("denied", "x", a.id));
        QTest::qWait(100);
        qInstallMessageHandler(old);
        QLoggingCategory::setFilterRules(QString());
        QVERIFY2(!g_logs.isEmpty(), "debug logging must have been exercised");
        const QString all = g_logs.join('\n');
        QVERIFY(!all.contains(QString::fromLatin1(kSecretTitle)));
        QVERIFY(!all.contains(QString::fromLatin1(kSecretText)));
        QVERIFY(!all.contains("REPLY-SECRET"));
        QVERIFY(all.contains("notify.posted"));
    }
};

QTEST_MAIN(LinkNotificationsTest)
#include "tst_link_notifications.moc"
