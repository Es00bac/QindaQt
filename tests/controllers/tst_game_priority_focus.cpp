// SPDX-License-Identifier: GPL-3.0-or-later
// ADR-0428: which holders of a controller node count as "a game" for the
// application that owns the focused window. Every case builds a fake proc tree;
// nothing reads a live process.
#include "fake_proc_tree.h"
#include "focus_ownership_p.h"
#include "proc_files_p.h"
#include "qindaqt/controllers/game_priority.h"
#include <QDir>
#include <QtTest>
#include <unistd.h>
using namespace QindaQt::Controllers;
using namespace QindaQt::Controllers::Testing;

class GamePriorityFocusTest final : public QObject {
    Q_OBJECT
private:
    static GamePriorityState scan(const FakeProcTree &proc, const QString &device, qint64 focused) {
        return inspectGamePriority(proc.query({device}, focused), proc.root());
    }
    static bool paused(const FakeProcTree &proc, const QString &device, qint64 focused) {
        return scan(proc, device, focused).gamePaths.contains(device);
    }
private Q_SLOTS:
    // The reported bug: a background holder (here a game) must not pause the
    // pad, but the same holder must the moment its application is focused.
    void backgroundHolderPausesOnlyOnceItsApplicationHasFocus() {
        FakeProcTree proc;
        const auto event = proc.device("event17");
        proc.add(5001, {.comm = "mygame", .holds = {event}});
        proc.add(6001, {.comm = "terminal"});
        const auto background = scan(proc, event, 6001);
        QCOMPARE(background.busyPaths, QSet<QString>{event});
        QVERIFY(background.gamePaths.isEmpty());
        QVERIFY(background.consumerPresent);
        const auto focused = scan(proc, event, 5001);
        QCOMPARE(focused.gamePaths, QSet<QString>{event});
        QCOMPARE(focused.busyPaths, QSet<QString>{event});
        QCOMPARE(focused.gameConsumers.value(event), "mygame");
        const auto none = scan(proc, event, 0);
        QVERIFY(none.gamePaths.isEmpty());
        QVERIFY(none.consumerPresent);
        QVERIFY(scan(proc, event, -1).gamePaths.isEmpty());
        QVERIFY(scan(proc, event, 4040).gamePaths.isEmpty());
        // The focus-agnostic overload reports who holds the node, never a game.
        const auto agnostic = inspectGamePriority({event}, proc.root(), FakeProcTree::CompositorPid);
        QCOMPARE(agnostic.busyPaths, QSet<QString>{event});
        QVERIFY(agnostic.gamePaths.isEmpty());
    }
    void passiveWebEngineHostsNeverPauseEvenWhenFocused_data() {
        QTest::addColumn<QByteArray>("mapsLine");
        QTest::addColumn<bool>("passive");
        QTest::newRow("qtwebengine") << QByteArray("7f00-7f01 r-xp 0 00:00 1 /usr/lib64/libQt6WebEngineCore.so.6.11.1\n") << true;
        QTest::newRow("gecko") << QByteArray("7f00-7f01 r-xp 0 00:00 1 /usr/lib64/firefox/libxul.so\n") << true;
        QTest::newRow("cef") << QByteArray("7f00-7f01 r-xp 0 00:00 1 /opt/app/libcef.so\n") << true;
        QTest::newRow("chromium-icu-data") << QByteArray("7f00-7f01 r--s 0 00:00 1 /opt/google/chrome/icudtl.dat\n") << true;
        // System ICU is not a web engine; this guards against an over-broad match.
        QTest::newRow("system-icu-is-not-a-web-engine") << QByteArray("7f00-7f01 r--p 0 00:00 1 /usr/lib64/libicudata.so.78.3\n") << false;
        QTest::newRow("ordinary-game") << QByteArray("7f00-7f01 r-xp 0 00:00 1 /usr/lib64/libSDL3.so.0\n") << false;
    }
    void passiveWebEngineHostsNeverPauseEvenWhenFocused() {
        QFETCH(QByteArray, mapsLine);
        QFETCH(bool, passive);
        FakeProcTree proc;
        const auto event = proc.device("event3");
        proc.add(5001, {.comm = "holder", .maps = mapsLine, .holds = {event}});
        const auto state = scan(proc, event, 5001);
        QCOMPARE(state.busyPaths, QSet<QString>{event});
        QCOMPARE(state.gamePaths.isEmpty(), passive);
        QCOMPARE(state.consumerPresent, !passive);
    }
    // The match must survive a read-chunk boundary, and a holder whose mappings
    // run past the read bound is unclassified, which keeps the old outcome.
    void passiveClassificationHandlesChunkBoundariesAndTheReadBound() {
        const QByteArray needle = "/libxul.so\n";
        FakeProcTree straddle;
        const auto first = straddle.device("event3");
        straddle.add(5001, {.comm = "straddle", .maps = QByteArray(ProcFiles::ChunkBytes - 4, '.') + needle, .holds = {first}});
        const auto split = scan(straddle, first, 5001);
        QVERIFY(split.gamePaths.isEmpty());
        QVERIFY(!split.consumerPresent);
        FakeProcTree late;
        const auto second = late.device("event3");
        late.add(5002, {.comm = "late", .maps = QByteArray(ProcFiles::MapsLimit, '.') + needle, .holds = {second}});
        const auto unclassified = scan(late, second, 5002);
        QCOMPARE(unclassified.gamePaths, QSet<QString>{second});
        QVERIFY(unclassified.consumerPresent);
    }
    void passiveHolderNeverHidesARealGameOrItsName() {
        FakeProcTree proc;
        const auto event = proc.device("event3");
        proc.add(5001, {.comm = "firefox-bin", .maps = "/usr/lib64/firefox/libxul.so\n", .holds = {event}});
        proc.add(5002, {.comm = "mygame", .holds = {event}});
        const auto browser = scan(proc, event, 5001);
        QVERIFY(browser.gamePaths.isEmpty());
        QVERIFY(browser.consumerPresent);
        const auto game = scan(proc, event, 5002);
        QCOMPARE(game.gamePaths, QSet<QString>{event});
        QCOMPARE(game.gameConsumers.value(event), "mygame");
    }
    void ruleTwoAncestryCountsInEitherDirection() {
        FakeProcTree proc;
        const auto event = proc.device("event3");
        // gamescope (the focused window) launched a shell that launched the game.
        proc.add(5001, {.comm = "gamescope"});
        proc.add(5002, {.comm = "sh", .parent = 5001});
        proc.add(5003, {.comm = "mygame", .parent = 5002, .holds = {event}});
        QVERIFY(paused(proc, event, 5001));
        QVERIFY(paused(proc, event, 5002));
        // The holder as the ancestor: a launcher holding the pad while its child owns the window.
        FakeProcTree launcher;
        const auto node = launcher.device("event4");
        launcher.add(6001, {.comm = "launcher", .holds = {node}});
        launcher.add(6002, {.comm = "child", .parent = 6001});
        QVERIFY(paused(launcher, node, 6002));
        // Siblings are not ancestors of one another.
        FakeProcTree siblings;
        const auto sibling = siblings.device("event5");
        siblings.add(7000, {.comm = "parent"});
        siblings.add(7001, {.comm = "mygame", .parent = 7000, .holds = {sibling}});
        siblings.add(7002, {.comm = "other", .parent = 7000});
        QVERIFY(!paused(siblings, sibling, 7002));
    }
    void ruleTwoWalkIsBoundedAndStopsAtThePidOneCompositorAndMissingProcesses() {
        const auto chain = [](int hops) {
            FakeProcTree proc;
            const auto event = proc.device("event3");
            proc.add(8000, {.comm = "app"});
            for (int hop = 1; hop < hops; ++hop) proc.add(8000 + hop, {.comm = "middle", .parent = 8000 + hop - 1});
            proc.add(8000 + hops, {.comm = "mygame", .parent = 8000 + hops - 1, .holds = {event}});
            return paused(proc, event, 8000);
        };
        QVERIFY(chain(32));
        QVERIFY(!chain(33));
        // Both are children of the compositor, which is itself a child of the
        // focused process: the compositor's ancestry must not make them one app.
        FakeProcTree compositor;
        const auto event = compositor.device("event3");
        compositor.add(6001, {.comm = "session"});
        compositor.add(99, {.comm = "qindaqt-kwin", .parent = 6001});
        compositor.add(5001, {.comm = "mygame", .parent = 99, .holds = {event}});
        QVERIFY(!paused(compositor, event, 6001));
        QVERIFY(!paused(compositor, event, 99));
        // A vanished ancestor ends the walk without a relation or a crash.
        FakeProcTree vanished;
        const auto node = vanished.device("event4");
        vanished.add(5001, {.comm = "mygame", .parent = 4999, .holds = {node}});
        vanished.add(6001, {.comm = "app"});
        QVERIFY(!paused(vanished, node, 6001));
    }
    void ruleThreeSharedSessionCountsExceptTheCompositorsOwn() {
        FakeProcTree proc;
        const auto event = proc.device("event3");
        proc.add(5001, {.comm = "mygame", .session = 8100, .holds = {event}});
        proc.add(6001, {.comm = "launcher-window", .session = 8100});
        proc.add(6002, {.comm = "unrelated", .session = 8200});
        QVERIFY(paused(proc, event, 6001));
        QVERIFY(!paused(proc, event, 6002));
        // Services started by the session share the compositor's session id.
        FakeProcTree shared;
        const auto node = shared.device("event4");
        shared.add(5001, {.comm = "service", .session = FakeProcTree::CompositorSession, .holds = {node}});
        shared.add(6001, {.comm = "settings", .session = FakeProcTree::CompositorSession});
        QVERIFY(!paused(shared, node, 6001));
    }
    void ruleFourSharedPrivateCgroupCountsExceptSessionScopesAndTheCompositors() {
        const auto related = [](const QString &cgroup, const QString &otherCgroup) {
            FakeProcTree proc;
            const auto event = proc.device("event3");
            proc.add(5001, {.comm = "mygame", .cgroup = cgroup, .holds = {event}});
            proc.add(6001, {.comm = "window", .cgroup = otherCgroup});
            return paused(proc, event, 6001);
        };
        const QString scope = "/user.slice/user-1000.slice/user@1000.service/app.slice/run-r12ab.scope";
        QVERIFY(related(scope, scope));
        QVERIFY(!related(scope, scope + "-other"));
        const QString session = "/user.slice/user-1000.slice/session-2.scope";
        QVERIFY(!related(session, session));
        QVERIFY(!related(FakeProcTree::CompositorCgroup, FakeProcTree::CompositorCgroup));
        QVERIFY(!related("/", "/"));
    }
    void ruleFiveWineProcessesShareAPrefixAcrossSessions() {
        const QString prefix = "/home/user/Games/pfx";
        const auto wine = [&](FakeProcess holder, FakeProcess window) {
            FakeProcTree proc;
            const auto event = proc.device("event3");
            holder.holds = {event};
            proc.add(5001, std::move(holder));
            proc.add(6001, std::move(window));
            return paused(proc, event, 6001);
        };
        const auto env = [](const QString &value) { return QByteArray("PATH=/usr/bin") + '\0' + "WINEPREFIX=" + value.toUtf8() + '\0'; };
        // winedevice.exe holds the HID nodes; the game's .exe owns the window.
        QVERIFY(wine({.comm = "winedevice.exe", .environment = env(prefix)}, {.comm = "Game.exe", .environment = env(prefix)}));
        // Wine truncates comm to 15 bytes, so the loader image identifies the window process.
        QVERIFY(wine({.comm = "winedevice.exe", .environment = env(prefix)},
                     {.comm = "Cyberpunk2077.e", .environment = env(prefix), .exe = "/opt/proton/files/bin/wine64-preloader"}));
        QVERIFY(wine({.comm = "mygame", .maps = "7f00-7f01 r-xp 0 00:00 1 /usr/lib/wine/wine64-preloader\n", .environment = env(prefix)},
                     {.comm = "Game.exe", .environment = env(prefix)}));
        QVERIFY(!wine({.comm = "winedevice.exe", .environment = env(prefix)}, {.comm = "Game.exe", .environment = env(prefix + "-other")}));
        // An unset WINEPREFIX is the user's default prefix, however it is spelled.
        const QByteArray bare = "PATH=/usr/bin";
        QVERIFY(wine({.comm = "winedevice.exe", .environment = bare}, {.comm = "Game.exe", .environment = bare}));
        QVERIFY(wine({.comm = "winedevice.exe", .environment = bare}, {.comm = "Game.exe", .environment = env(QDir::homePath() + "/.wine/")}));
        QVERIFY(!wine({.comm = "winedevice.exe", .environment = bare}, {.comm = "Game.exe", .environment = env(prefix)}));
        // Both sides must be Wine processes; an environment variable alone is not enough.
        QVERIFY(!wine({.comm = "winedevice.exe", .environment = env(prefix)}, {.comm = "browser", .environment = env(prefix)}));
        QVERIFY(!wine({.comm = "mygame", .environment = env(prefix)}, {.comm = "Game.exe", .environment = env(prefix)}));
        // An unreadable environment cannot prove the prefixes are equal.
        QVERIFY(!wine({.comm = "winedevice.exe"}, {.comm = "Game.exe", .environment = env(prefix)}));
        // Mappings are read only for holders: a focused process that merely maps a
        // Wine loader is not Wine unless its comm or image says so.
        QVERIFY(!wine({.comm = "winedevice.exe", .environment = env(prefix)},
                      {.comm = "window", .maps = "7f00-7f01 r-xp 0 00:00 1 /usr/lib/wine/wine64-preloader\n", .environment = env(prefix)}));
    }
    // The Wine loader named by a holder's mappings counts even when nothing asked
    // whether that holder is a web engine first.
    void holderMappingsNamingAWineLoaderCountWithoutAPriorPassiveCheck() {
        FakeProcTree proc;
        const QByteArray environment = "WINEPREFIX=/home/user/pfx\0";
        proc.add(5001, {.comm = "mygame", .maps = "7f00-7f01 r-xp 0 00:00 1 /usr/lib/wine/wine64-preloader\n", .environment = environment});
        proc.add(6001, {.comm = "Game.exe", .environment = environment});
        FocusOwnership owner(proc.root(), 6001, {FakeProcTree::CompositorPid, FakeProcTree::CompositorSession, FakeProcTree::CompositorCgroup});
        QVERIFY(owner.partOfFocusedApplication(5001));
        FocusOwnership none(proc.root(), 0, {FakeProcTree::CompositorPid, FakeProcTree::CompositorSession, FakeProcTree::CompositorCgroup});
        QVERIFY(!none.partOfFocusedApplication(5001));
    }
    void unknownCompositorIdentityDisablesTheSessionAndCgroupRules() {
        const QString scope = "/user.slice/user-1000.slice/user@1000.service/app.slice/run-r12ab.scope";
        FakeProcTree sessions;
        const auto first = sessions.device("event3");
        sessions.add(5001, {.comm = "mygame", .session = 8100, .holds = {first}});
        sessions.add(6001, {.comm = "window", .session = 8100});
        QVERIFY(paused(sessions, first, 6001));
        auto query = sessions.query({first}, 6001);
        query.compositorSession = -1;
        QVERIFY(inspectGamePriority(query, sessions.root()).gamePaths.isEmpty());
        FakeProcTree groups;
        const auto second = groups.device("event3");
        groups.add(5001, {.comm = "mygame", .cgroup = scope, .holds = {second}});
        groups.add(6001, {.comm = "window", .cgroup = scope});
        QVERIFY(paused(groups, second, 6001));
        query = groups.query({second}, 6001);
        query.compositorCgroup = QString();
        QVERIFY(inspectGamePriority(query, groups.root()).gamePaths.isEmpty());
    }
    // Without an injected identity the scan uses the running process's own.
    void defaultCompositorIdentityIsTheRunningProcess() {
        const qint64 ownSession = ::getsid(0);
        QVERIFY(ownSession > 0);
        const auto related = [&](qint64 session) {
            FakeProcTree proc;
            const auto event = proc.device("event3");
            proc.add(5001, {.comm = "mygame", .session = session, .holds = {event}});
            proc.add(6001, {.comm = "window", .session = session});
            auto query = proc.query({event}, 6001);
            query.compositorSession.reset();
            return inspectGamePriority(query, proc.root()).gamePaths.contains(event);
        };
        QVERIFY(!related(ownSession));
        QVERIFY(related(ownSession + 1));
        const auto ownCgroup = ProcFiles::cgroupPath("/proc/self");
        if (ownCgroup.isEmpty()) QSKIP("no cgroup v2 hierarchy on this host");
        FakeProcTree proc;
        const auto event = proc.device("event3");
        proc.add(5001, {.comm = "mygame", .cgroup = ownCgroup + "/private.scope", .holds = {event}});
        proc.add(6001, {.comm = "window", .cgroup = ownCgroup + "/private.scope"});
        auto query = proc.query({event}, 6001);
        query.compositorCgroup.reset();
        QVERIFY(inspectGamePriority(query, proc.root()).gamePaths.contains(event));
    }
    void focusedProcessThatCannotBeReadRelatesOnlyByIdentity() {
        FakeProcTree proc;
        const auto event = proc.device("event3");
        proc.add(5001, {.comm = "mygame", .holds = {event}});
        QVERIFY(paused(proc, event, 5001));
        QVERIFY(!paused(proc, event, 6001));
    }
    void processNamesAreBoundedPrintableAndParsedPastParentheses() {
        FakeProcTree proc;
        const auto event = proc.device("event3");
        // The stat command field may hold spaces and parentheses; the session id
        // after the last ')' must still be found for the shared-session rule.
        proc.add(5001, {.comm = "ev) 1 2 3 (il\x1b[31m\xe2\x80\xae" "mygame-with-a-very-long-name-1234567890", .session = 8100, .holds = {event}});
        proc.add(6001, {.comm = "window", .session = 8100});
        const auto name = scan(proc, event, 6001).gameConsumers.value(event);
        QVERIFY2(!name.isEmpty() && name.size() <= 32, qPrintable(name));
        for (const QChar ch : name) QVERIFY2(ch.isLetterOrNumber() || QStringLiteral(" ._-+").contains(ch), qPrintable(name));
        QVERIFY(!name.contains(QChar(0x202e)));
    }
    void holdingAnotherNodeIsIgnored() {
        FakeProcTree proc;
        const auto event = proc.device("event3");
        const auto other = proc.device("event9");
        proc.add(5001, {.comm = "mygame", .holds = {other}});
        const auto state = scan(proc, event, 5001);
        QVERIFY(state.busyPaths.isEmpty());
        QVERIFY(state.gamePaths.isEmpty());
        QVERIFY(!state.consumerPresent);
    }
};
QTEST_GUILESS_MAIN(GamePriorityFocusTest)
#include "tst_game_priority_focus.moc"
