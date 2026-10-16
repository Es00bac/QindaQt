// SPDX-License-Identifier: GPL-3.0-or-later
//
// ADR-0384: the program behind an opaque launcher window is resolved once per
// window, and the Steam/PE-icon filesystem work never runs on the thread that
// samples task facts.

#include "foreignidentityresolver.h"

#include "qindaqt/compositor/wineidentitycache.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QThread>
#include <QtTest>

#include <atomic>

using namespace QindaQt::Compositor;
using namespace QindaQt::Compositor::KWinIntegration;

namespace {

QByteArray commandLine(std::initializer_list<const char *> arguments)
{
    QByteArray bytes;
    for (const char *argument : arguments) {
        bytes += argument;
        bytes += '\0';
    }
    return bytes;
}

class SteamHome final
{
public:
    SteamHome()
    {
        const QString steamapps = m_dir.filePath(QStringLiteral("home/.steam/steam/steamapps"));
        QDir().mkpath(steamapps);
        QFile manifest(steamapps + QStringLiteral("/appmanifest_620.acf"));
        if (!manifest.open(QIODevice::WriteOnly)) {
            qFatal("could not write the Steam manifest fixture");
        }
        manifest.write("\"AppState\"\n{\n\t\"appid\"\t\t\"620\"\n\t\"name\"\t\t\"Portal 2\"\n}\n");
    }
    std::shared_ptr<WineIdentityCache> cache() const
    {
        return std::make_shared<WineIdentityCache>(m_dir.filePath(QStringLiteral("icons")),
                                                   m_dir.filePath(QStringLiteral("home")));
    }

private:
    QTemporaryDir m_dir;
};

ForeignIdentityResolver::Executor inlineExecutor()
{
    return [](ForeignIdentityResolver::Job job) { job(); };
}

} // namespace

class ForeignIdentityResolverTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void readsEachWindowsCommandLineOnce();
    void rereadsWhenThePidOrClassChanges();
    void steamNameArrivesAfterTheFirstSample();
    void dropsALateResultForAForgottenWindow();
    void runsFilesystemWorkOnItsOwnThread();
};

// Before ADR-0384 every task-facts sample re-read /proc/<pid>/cmdline, stat'ed
// the Steam library and checked the icon cache for every Wine window.
void ForeignIdentityResolverTest::readsEachWindowsCommandLineOnce()
{
    SteamHome home;
    int reads = 0;
    ForeignIdentityResolver resolver(
        home.cache(),
        {.commandLine = [&reads](qint64) {
             ++reads;
             return commandLine({"wine", "C:\\Program Files\\Battle.net\\Battle.net.exe"});
         },
         .environment = [](qint64) { return QByteArray(); }},
        inlineExecutor());
    for (int sample = 0; sample < 50; ++sample) {
        const auto identity = resolver.identity(QStringLiteral("window-1"), 4100,
                                                QStringLiteral("steam_app_0"));
        QCOMPARE(identity.clientExecutable, QStringLiteral("Battle.net.exe"));
    }
    QCOMPARE(reads, 1);
    QCOMPARE(resolver.commandLineReads(), qsizetype(1));
}

void ForeignIdentityResolverTest::rereadsWhenThePidOrClassChanges()
{
    SteamHome home;
    int reads = 0;
    ForeignIdentityResolver resolver(
        home.cache(),
        {.commandLine = [&reads](qint64 pid) {
             ++reads;
             return pid == 2 ? commandLine({"/usr/bin/other"}) : commandLine({"/usr/bin/first"});
         },
         .environment = [](qint64) { return QByteArray(); }},
        inlineExecutor());
    QCOMPARE(resolver.identity(QStringLiteral("w"), 1, QStringLiteral("wine")).clientExecutable,
             QStringLiteral("first"));
    QCOMPARE(resolver.identity(QStringLiteral("w"), 2, QStringLiteral("wine")).clientExecutable,
             QStringLiteral("other"));
    QCOMPARE(resolver.identity(QStringLiteral("w"), 2, QStringLiteral("")).clientExecutable,
             QStringLiteral("other"));
    QCOMPARE(reads, 3);
    resolver.forget(QStringLiteral("w"));
    (void)resolver.identity(QStringLiteral("w"), 2, QStringLiteral(""));
    QCOMPARE(reads, 4);
}

void ForeignIdentityResolverTest::steamNameArrivesAfterTheFirstSample()
{
    SteamHome home;
    ForeignIdentityResolver resolver(
        home.cache(),
        {.commandLine = [](qint64) { return commandLine({"/usr/bin/portal2_linux"}); },
         .environment = [](qint64) { return QByteArray(); }},
        inlineExecutor());
    QSignalSpy changed(&resolver, &ForeignIdentityResolver::identityChanged);
    const auto first = resolver.identity(QStringLiteral("game"), 300,
                                         QStringLiteral("steam_app_620"));
    QCOMPARE(first.clientExecutable, QStringLiteral("portal2_linux"));
    // The worker's answer is delivered through the event loop, never inside
    // the sample that asked for it.
    QVERIFY(first.steamName.isEmpty());
    QCOMPARE(changed.count(), 0);
    QCoreApplication::processEvents();
    QCOMPARE(changed.count(), 1);
    QCOMPARE(changed.constFirst().constFirst().toString(), QStringLiteral("game"));
    QCOMPARE(resolver.identity(QStringLiteral("game"), 300, QStringLiteral("steam_app_620"))
                 .steamName,
             QStringLiteral("Portal 2"));
}

void ForeignIdentityResolverTest::dropsALateResultForAForgottenWindow()
{
    SteamHome home;
    ForeignIdentityResolver resolver(
        home.cache(),
        {.commandLine = [](qint64) { return commandLine({"/usr/bin/portal2_linux"}); },
         .environment = [](qint64) { return QByteArray(); }},
        inlineExecutor());
    QSignalSpy changed(&resolver, &ForeignIdentityResolver::identityChanged);
    (void)resolver.identity(QStringLiteral("game"), 300, QStringLiteral("steam_app_620"));
    resolver.forget(QStringLiteral("game"));
    QCoreApplication::processEvents();
    QCOMPARE(changed.count(), 0);
}

void ForeignIdentityResolverTest::runsFilesystemWorkOnItsOwnThread()
{
    SteamHome home;
    std::atomic<QThread *> environmentThread{nullptr};
    {
        ForeignIdentityResolver resolver(
            home.cache(),
            {.commandLine = [](qint64) {
                 return commandLine({"wine", "C:\\Games\\Game\\game.exe"});
             },
             .environment = [&environmentThread](qint64) {
                 environmentThread = QThread::currentThread();
                 return QByteArray("WINEPREFIX=/nonexistent\0", 25);
             }});
        const auto identity = resolver.identity(QStringLiteral("game"), 300,
                                                QStringLiteral("steam_app_0"));
        QCOMPARE(identity.clientExecutable, QStringLiteral("game.exe"));
        QTRY_VERIFY_WITH_TIMEOUT(environmentThread.load() != nullptr, 5000);
        QVERIFY(environmentThread.load() != QThread::currentThread());
    }
    // Destruction joined the idle worker without hanging.
}

QTEST_GUILESS_MAIN(ForeignIdentityResolverTest)
#include "tst_foreignidentityresolver.moc"
