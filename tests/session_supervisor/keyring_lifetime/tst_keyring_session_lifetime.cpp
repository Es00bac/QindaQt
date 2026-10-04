// SPDX-License-Identifier: GPL-3.0-or-later
#include "src/session_supervisor/src/keyring_session_lifetime.h"
#include <QDBusConnectionInterface>
#include <QDBusReply>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QtTest>
#include <unistd.h>
#include <algorithm>
#include <csignal>
#include <thread>
using QindaQt::SessionSupervisor::KeyringSessionLifetime;
namespace {
constexpr auto Service = "org.qindaqt.Keyring1";
class FixtureOwner final {
public:
    ~FixtureOwner() { retire(); }
    bool start(const QStringList &arguments = {}) {
        child_.start(QStringLiteral(QINDAQT_KEYRING_LIFETIME_ENDPOINT), arguments);
        if (!child_.waitForStarted(2'000)) return false;
        return waitBlocking(QStringLiteral("ready"));
    }
    // These waits deliberately do not deliver the supervisor's Qt callbacks.
    // They reproduce a replaced owner before its queued watcher reaches stop.
    bool releaseBlocking() {
        child_.write("release\n");
        return waitBlocking(QStringLiteral("released"));
    }
    bool acceptBlocking() {
        child_.write("accept\n");
        return waitBlocking(QStringLiteral("accepting"));
    }
    void retire() {
        if (child_.state() != QProcess::NotRunning) {
            child_.terminate();
            if (!child_.waitForFinished(2'000)) { child_.kill(); child_.waitForFinished(2'000); }
        }
        collect();
    }
    int count(const QString &name) {
        collect();
        return static_cast<int>(std::count_if(events_.cbegin(), events_.cend(), [&](const auto &value) {
            return value.value(QStringLiteral("event")).toString() == name;
        }));
    }
    QJsonObject first(const QString &name) {
        collect();
        for (const auto &value : events_) if (value.value(QStringLiteral("event")).toString() == name) return value;
        return {};
    }
    bool running() const { return child_.state() != QProcess::NotRunning; }
private:
    bool waitBlocking(const QString &name) {
        QElapsedTimer time; time.start();
        while (time.elapsed() < 2'000) {
            if (count(name) > 0) return true;
            if (!child_.waitForReadyRead(100) && !running()) break;
        }
        return count(name) > 0;
    }
    void collect() {
        pending_ += child_.readAllStandardOutput();
        for (auto newline = pending_.indexOf('\n'); newline >= 0; newline = pending_.indexOf('\n')) {
            events_.append(QJsonDocument::fromJson(pending_.left(newline)).object());
            pending_.remove(0, newline + 1);
        }
    }
    QProcess child_;
    QByteArray pending_;
    QList<QJsonObject> events_;
};
}
class KeyringLifetimeTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void init() { qputenv("WAYLAND_DISPLAY", "qindaqt-7"); }
    void stableOwnerAttachedOnceAndShutdownIsExact() {
        FixtureOwner owner; QVERIFY(owner.start());
        KeyringSessionLifetime session; session.start(QStringLiteral("/bin/true"));
        QTRY_COMPARE(owner.count(QStringLiteral("attach")), 1);
        const auto call = owner.first(QStringLiteral("attach"));
        QCOMPARE(call.value(QStringLiteral("display")).toString(), QStringLiteral("qindaqt-7"));
        QCOMPARE(call.value(QStringLiteral("member")).toString(), QStringLiteral("AttachSessionWithDisplay"));
        const auto caller = call.value(QStringLiteral("caller")).toString();
        QVERIFY(caller.startsWith(QLatin1Char(':')));
        QVERIFY(caller != QDBusConnection::sessionBus().baseService());
        const auto unique = owner.first(QStringLiteral("ready")).value(QStringLiteral("owner")).toString();
        QCOMPARE(QDBusConnection::sessionBus().interface()->serviceUid(unique).value(), static_cast<uint>(geteuid()));
        QTest::qWait(300); QCOMPARE(owner.count(QStringLiteral("attach")), 1);
        session.stop(); session.stop();
        QTRY_COMPARE(owner.count(QStringLiteral("shutdown")), 1);
        QTRY_VERIFY(!QDBusConnection::sessionBus().interface()->isServiceRegistered(caller).value());
        QTRY_VERIFY(!owner.running());
    }
    void replacementReattachesRetainedCallerAndDisplay() {
        FixtureOwner first; QVERIFY(first.start());
        KeyringSessionLifetime session; session.start(QStringLiteral("/bin/true"));
        QTRY_COMPARE(first.count(QStringLiteral("attach")), 1);
        QTest::qWait(150);
        const auto original = first.first(QStringLiteral("attach"));
        first.retire();
        FixtureOwner second; QVERIFY(second.start());
        QTRY_COMPARE(second.count(QStringLiteral("attach")), 1);
        QCOMPARE(second.first(QStringLiteral("attach")), original);
        QVERIFY(first.first(QStringLiteral("ready")) != second.first(QStringLiteral("ready")));
        QTest::qWait(300); QCOMPARE(second.count(QStringLiteral("attach")), 1);
        session.stop(); QTRY_COMPARE(second.count(QStringLiteral("shutdown")), 1);
        QCOMPARE(first.count(QStringLiteral("shutdown")), 0);
    }
    void eachNewOwnerGetsFreshBoundedAdmission() {
        FixtureOwner rejected; QVERIFY(rejected.start({QStringLiteral("reject")}));
        KeyringSessionLifetime session; session.start(QStringLiteral("/bin/true"));
        QTRY_COMPARE_WITH_TIMEOUT(rejected.count(QStringLiteral("attach")), 30, 6'000);
        QTest::qWait(400); QCOMPARE(rejected.count(QStringLiteral("attach")), 30);
        rejected.retire();
        FixtureOwner accepted; QVERIFY(accepted.start());
        QTRY_COMPARE(accepted.count(QStringLiteral("attach")), 1);
        QTest::qWait(150);
        session.stop(); QTRY_COMPARE(accepted.count(QStringLiteral("shutdown")), 1);
        QCOMPARE(rejected.count(QStringLiteral("shutdown")), 0);
    }
    void replacementBeforeStopNeverReceivesShutdown() {
        FixtureOwner first; QVERIFY(first.start());
        KeyringSessionLifetime session; session.start(QStringLiteral("/bin/true"));
        QTRY_COMPARE(first.count(QStringLiteral("attach")), 1); QTest::qWait(150);
        QVERIFY(first.releaseBlocking());
        FixtureOwner replacement; QVERIFY(replacement.start({QStringLiteral("reject")}));
        // stop sees the real new owner before queued loss/arrival are delivered.
        session.stop(); QTest::qWait(400);
        QCOMPARE(first.count(QStringLiteral("shutdown")), 0);
        QCOMPARE(replacement.count(QStringLiteral("shutdown")), 0);
        QCOMPARE(replacement.count(QStringLiteral("attach")), 0);
        QVERIFY(replacement.running());
        QCOMPARE(QDBusConnection::sessionBus().interface()->serviceOwner(Service).value(),
            replacement.first(QStringLiteral("ready")).value(QStringLiteral("owner")).toString());
    }
    void stopRetiresPendingReplyAndLaterOwnerEvents() {
        FixtureOwner delayed; QVERIFY(delayed.start({QStringLiteral("delay=300")}));
        KeyringSessionLifetime session; session.start(QStringLiteral("/bin/true"));
        QTRY_COMPARE(delayed.count(QStringLiteral("attach")), 1);
        const auto caller = delayed.first(QStringLiteral("attach")).value(QStringLiteral("caller")).toString();
        session.stop(); QTest::qWait(600);
        QCOMPARE(delayed.count(QStringLiteral("reply")), 1);
        QCOMPARE(delayed.count(QStringLiteral("shutdown")), 0);
        QCOMPARE(delayed.count(QStringLiteral("attach")), 1);
        QVERIFY(!QDBusConnection::sessionBus().interface()->isServiceRegistered(caller).value());
        delayed.retire();
        FixtureOwner later; QVERIFY(later.start());
        QTest::qWait(300); QCOMPARE(later.count(QStringLiteral("attach")), 0);
        QCOMPARE(later.count(QStringLiteral("shutdown")), 0);
    }
    void oldLateSuccessCannotAdmitRejectedReplacement() {
        FixtureOwner delayed; QVERIFY(delayed.start({QStringLiteral("delay=300")}));
        KeyringSessionLifetime session; session.start(QStringLiteral("/bin/true"));
        QTRY_COMPARE(delayed.count(QStringLiteral("attach")), 1);
        QVERIFY(delayed.releaseBlocking());
        FixtureOwner rejected; QVERIFY(rejected.start({QStringLiteral("reject")}));
        QTRY_VERIFY(rejected.count(QStringLiteral("attach")) >= 1);
        QTRY_COMPARE(delayed.count(QStringLiteral("reply")), 1);
        session.stop(); QTest::qWait(150);
        QCOMPARE(delayed.count(QStringLiteral("shutdown")), 0);
        QCOMPARE(rejected.count(QStringLiteral("shutdown")), 0);
        QVERIFY(rejected.running());
    }
    void transientRegistryTimeoutStillRetriesTheSameOwner() {
        FixtureOwner owner; QVERIFY(owner.start({QStringLiteral("reject")}));
        KeyringSessionLifetime session; session.start(QStringLiteral("/bin/true"));
        QTRY_COMPARE(owner.count(QStringLiteral("reply")), 1);
        const auto caller = owner.first(QStringLiteral("attach")).value(QStringLiteral("caller")).toString();
        const auto registryPid = QDBusConnection::sessionBus().interface()->servicePid(QStringLiteral("org.freedesktop.DBus"));
        QVERIFY(registryPid.isValid()); QVERIFY(registryPid.value() > 1);
        const auto registryUid = QDBusConnection::sessionBus().interface()->serviceUid(QStringLiteral("org.freedesktop.DBus"));
        QVERIFY(registryUid.isValid()); QCOMPARE(registryUid.value(), static_cast<uint>(geteuid()));
        QFile stat(QStringLiteral("/proc/%1/stat").arg(registryPid.value()));
        QVERIFY(stat.open(QIODevice::ReadOnly));
        const auto row = stat.readAll();
        const auto fields = row.mid(row.lastIndexOf(')') + 2).split(' ');
        QVERIFY(fields.size() > 1);
        // AGENT-GUARD: only pause the bus forked by OUR dbus-run-session parent.
        // Direct invocation against a live/session-manager bus must fail here.
        QCOMPARE(fields[1].toLongLong(), static_cast<qint64>(getppid()));
        QVERIFY(owner.acceptBlocking());
        const auto pid = static_cast<pid_t>(registryPid.value());
        const std::jthread resume([pid] {
            std::this_thread::sleep_for(std::chrono::milliseconds(600));
            ::kill(pid, SIGCONT);
        });
        QCOMPARE(::kill(pid, SIGSTOP), 0);
        // Actual GetNameOwner exceeds its 250ms timeout, then the same owner
        // remains registered. There is no owner-change event after recovery.
        QTest::qWait(800);
        QTRY_VERIFY(owner.count(QStringLiteral("attach")) >= 2);
        const auto calls = owner.count(QStringLiteral("attach"));
        QTest::qWait(300); QCOMPARE(owner.count(QStringLiteral("attach")), calls);
        QCOMPARE(owner.first(QStringLiteral("attach")).value(QStringLiteral("caller")).toString(), caller);
        session.stop(); QTRY_COMPARE(owner.count(QStringLiteral("shutdown")), 1);
    }
    void arrivalAfterStartupBudgetIsStillWatched() {
        KeyringSessionLifetime session; session.start(QStringLiteral("/bin/true"));
        QTest::qWait(3'400);
        FixtureOwner late; QVERIFY(late.start());
        QTRY_COMPARE(late.count(QStringLiteral("attach")), 1);
        QTest::qWait(150);
        session.stop(); QTRY_COMPARE(late.count(QStringLiteral("shutdown")), 1);
    }
    void legacyNoDisplayRetainsCompatibilityForReplacement() {
        qunsetenv("WAYLAND_DISPLAY");
        FixtureOwner first; QVERIFY(first.start());
        KeyringSessionLifetime session; session.start(QStringLiteral("/bin/true"));
        QTRY_COMPARE(first.count(QStringLiteral("attach")), 1);
        const auto original = first.first(QStringLiteral("attach"));
        QCOMPARE(original.value(QStringLiteral("member")).toString(), QStringLiteral("AttachSession"));
        QCOMPARE(original.value(QStringLiteral("display")).toString(), QString{});
        QTest::qWait(150); first.retire();
        FixtureOwner second; QVERIFY(second.start());
        QTRY_COMPARE(second.count(QStringLiteral("attach")), 1);
        QCOMPARE(second.first(QStringLiteral("attach")), original);
        QTest::qWait(150); session.stop();
        QTRY_COMPARE(second.count(QStringLiteral("shutdown")), 1);
    }
};
QTEST_GUILESS_MAIN(KeyringLifetimeTest)
#include "tst_keyring_session_lifetime.moc"
