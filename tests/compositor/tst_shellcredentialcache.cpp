// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/compositor/shellcredentialcache.h"

#include <QtTest>

using namespace QindaQt::Compositor;

class ShellCredentialCacheTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void looksUpEachConnectionOnce();
    void neverCachesFailuresOrWellKnownNames();
    void forgetsADisconnectedOwnerAndDropsItsWatch();
    void evictsTheOldestEntryAtCapacity();
};

namespace {

struct Bus final
{
    QHash<QString, qint64> pids;
    int lookups = 0;
    QStringList watched;
    ShellCredentialCache::Lookup lookup()
    {
        return [this](const QString &name) -> std::optional<qint64> {
            ++lookups;
            const auto found = pids.constFind(name);
            return found == pids.cend() ? std::nullopt : std::optional<qint64>(*found);
        };
    }
    ShellCredentialCache::WatchChange watch()
    {
        return [this](const QString &name, bool watching) {
            if (watching) {
                watched.append(name);
            } else {
                watched.removeAll(name);
            }
        };
    }
};

} // namespace

// The storm before ADR-0383: every TaskListSnapshotChanged hint and every
// TaskListSnapshot read made its own blocking GetConnectionCredentials call.
void ShellCredentialCacheTest::looksUpEachConnectionOnce()
{
    Bus bus;
    bus.pids.insert(QStringLiteral(":1.42"), 4242);
    ShellCredentialCache cache(bus.lookup(), bus.watch());
    for (int request = 0; request < 80; ++request) {
        QCOMPARE(cache.processIdForUniqueName(QStringLiteral(":1.42")),
                 std::optional<qint64>(4242));
    }
    QCOMPARE(bus.lookups, 1);
    QCOMPARE(cache.lookupCount(), 1);
    QCOMPARE(bus.watched, QStringList{QStringLiteral(":1.42")});
}

void ShellCredentialCacheTest::neverCachesFailuresOrWellKnownNames()
{
    Bus bus;
    ShellCredentialCache cache(bus.lookup(), bus.watch());
    QVERIFY(!cache.processIdForUniqueName(QStringLiteral(":1.7")));
    QVERIFY(!cache.processIdForUniqueName(QStringLiteral(":1.7")));
    QCOMPARE(bus.lookups, 2);
    bus.pids.insert(QStringLiteral(":1.7"), 77);
    QCOMPARE(cache.processIdForUniqueName(QStringLiteral(":1.7")), std::optional<qint64>(77));
    bus.pids.insert(QStringLiteral("org.qindaqt.Shell"), 5);
    QCOMPARE(cache.processIdForUniqueName(QStringLiteral("org.qindaqt.Shell")),
             std::optional<qint64>(5));
    QCOMPARE(cache.processIdForUniqueName(QStringLiteral("org.qindaqt.Shell")),
             std::optional<qint64>(5));
    QCOMPARE(bus.lookups, 5);
    QCOMPARE(cache.size(), 1);
    QVERIFY(!cache.processIdForUniqueName(QString()));
    QCOMPARE(bus.lookups, 5);
}

void ShellCredentialCacheTest::forgetsADisconnectedOwnerAndDropsItsWatch()
{
    Bus bus;
    bus.pids.insert(QStringLiteral(":1.42"), 4242);
    ShellCredentialCache cache(bus.lookup(), bus.watch());
    QVERIFY(cache.processIdForUniqueName(QStringLiteral(":1.42")));
    cache.forget(QStringLiteral(":1.42"));
    QVERIFY(bus.watched.isEmpty());
    // A reconnected shell has a new unique name; the old one is gone for good.
    bus.pids.remove(QStringLiteral(":1.42"));
    QVERIFY(!cache.processIdForUniqueName(QStringLiteral(":1.42")));
    QCOMPARE(bus.lookups, 2);
    cache.forget(QStringLiteral(":1.unknown"));
}

void ShellCredentialCacheTest::evictsTheOldestEntryAtCapacity()
{
    Bus bus;
    for (int index = 0; index < 4; ++index) {
        bus.pids.insert(QStringLiteral(":1.%1").arg(index), 100 + index);
    }
    ShellCredentialCache cache(bus.lookup(), bus.watch(), 3);
    for (int index = 0; index < 4; ++index) {
        QVERIFY(cache.processIdForUniqueName(QStringLiteral(":1.%1").arg(index)));
    }
    QCOMPARE(cache.size(), 3);
    QCOMPARE(bus.watched, (QStringList{QStringLiteral(":1.1"), QStringLiteral(":1.2"),
                                       QStringLiteral(":1.3")}));
    QVERIFY(cache.processIdForUniqueName(QStringLiteral(":1.0")));
    QCOMPARE(bus.lookups, 5);
}

QTEST_GUILESS_MAIN(ShellCredentialCacheTest)
#include "tst_shellcredentialcache.moc"
