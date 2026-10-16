// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/shell/icons/icon_availability_cache.h>
#include <qindaqt/shell/icons/icon_theme_limits.h>

#include <QDir>
#include <QFile>
#include <QtTest>

#include "shell_icons_test_fixtures.h"

using namespace QindaQt::Shell::Icons;

// The cache answers a yes/no for presentation hints; the confined locator
// stays uncached (tst_shell_icons_locator filesystemChangesRemainFresh). Every
// row drives time through an injected clock, so nothing here sleeps.
class ShellIconsAvailabilityCacheTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void liveVerdictsAnswerWithoutTheDisk();
    void negativeVerdictExpiresSoonerAndPicksUpNewArtwork();
    void positiveVerdictRevalidatesWithOneMetadataCheck();
    void positiveVerdictForARemovedFileIsLookedUpAgain();
    void invalidateForgetsEveryVerdict();
    void sizeScaleAndSymbolicAreDistinctRequests();
    void refusedNamesAreAnsweredWithoutBeingRetained();
    void memoStaysBoundedWhateverIsAsked();
    void identicalRequestsAfterNormalizationShareOneVerdict();

private:
    [[nodiscard]] QString iconPath(const QString &name) const
    {
        return m_root + QLatin1Char('/') + name + QStringLiteral(".png");
    }
    [[nodiscard]] bool makeIcon(const QString &name) const
    {
        return ShellIconsTest::writePng(iconPath(name), 32, QColor(1, 2, 3));
    }
    [[nodiscard]] std::unique_ptr<IconAvailabilityCache> makeCache()
    {
        return std::make_unique<IconAvailabilityCache>(
            QStringList{m_root}, QStringList{}, [this] { return m_nowMs; });
    }

    QString m_root;
    qint64 m_nowMs = 0;
};

void ShellIconsAvailabilityCacheTest::init()
{
    m_root = ShellIconsTest::fixtureRoot() + QStringLiteral("/availability-cache");
    QVERIFY(ShellIconsTest::recreateDir(m_root));
    m_nowMs = 1'000'000;
}

void ShellIconsAvailabilityCacheTest::liveVerdictsAnswerWithoutTheDisk()
{
    QVERIFY(makeIcon(QStringLiteral("present")));
    const auto cache = makeCache();

    QVERIFY(cache->hasIcon(QStringLiteral("present"), 18));
    QVERIFY(!cache->hasIcon(QStringLiteral("absent"), 18));
    QCOMPARE(cache->statistics().diskLookups, quint64(2));

    // The file disappears and a new one appears, yet within the verdicts'
    // lifetime nothing reaches the disk: this is what takes the per-row cost
    // of a task-list publication from a theme walk to a hash lookup.
    QVERIFY(QFile::remove(iconPath(QStringLiteral("present"))));
    QVERIFY(makeIcon(QStringLiteral("absent")));
    for (int round = 0; round < 50; ++round) {
        m_nowMs += 10;
        QVERIFY(cache->hasIcon(QStringLiteral("present"), 18));
        QVERIFY(!cache->hasIcon(QStringLiteral("absent"), 18));
    }
    const IconAvailabilityCache::Statistics statistics = cache->statistics();
    QCOMPARE(statistics.diskLookups, quint64(2));
    QCOMPARE(statistics.memoHits, quint64(100));
    QCOMPARE(statistics.queries, quint64(102));
    QCOMPARE(statistics.entries, 2);
}

void ShellIconsAvailabilityCacheTest::negativeVerdictExpiresSoonerAndPicksUpNewArtwork()
{
    const auto cache = makeCache();
    QVERIFY(!cache->hasIcon(QStringLiteral("qindaqt-wine-late"), 18));

    // The compositor writes a Wine program's icon after its window appears.
    QVERIFY(makeIcon(QStringLiteral("qindaqt-wine-late")));
    m_nowMs += kAvailabilityNegativeTtlMs - 1;
    QVERIFY(!cache->hasIcon(QStringLiteral("qindaqt-wine-late"), 18));
    QCOMPARE(cache->statistics().diskLookups, quint64(1));

    m_nowMs += 1;
    QVERIFY(cache->hasIcon(QStringLiteral("qindaqt-wine-late"), 18));
    QCOMPARE(cache->statistics().diskLookups, quint64(2));
    // The negative lifetime is well under the positive one by design.
    QVERIFY(kAvailabilityNegativeTtlMs < kAvailabilityPositiveTtlMs);
}

void ShellIconsAvailabilityCacheTest::positiveVerdictRevalidatesWithOneMetadataCheck()
{
    QVERIFY(makeIcon(QStringLiteral("steady")));
    const auto cache = makeCache();
    QVERIFY(cache->hasIcon(QStringLiteral("steady"), 18));

    m_nowMs += kAvailabilityPositiveTtlMs;
    QVERIFY(cache->hasIcon(QStringLiteral("steady"), 18));
    IconAvailabilityCache::Statistics statistics = cache->statistics();
    QCOMPARE(statistics.revalidations, quint64(1));
    QCOMPARE(statistics.diskLookups, quint64(1));

    // Revalidation renews the verdict for another full lifetime.
    m_nowMs += kAvailabilityPositiveTtlMs - 1;
    QVERIFY(cache->hasIcon(QStringLiteral("steady"), 18));
    statistics = cache->statistics();
    QCOMPARE(statistics.revalidations, quint64(1));
    QCOMPARE(statistics.memoHits, quint64(1));
}

void ShellIconsAvailabilityCacheTest::positiveVerdictForARemovedFileIsLookedUpAgain()
{
    QVERIFY(makeIcon(QStringLiteral("vanishing")));
    const auto cache = makeCache();
    QVERIFY(cache->hasIcon(QStringLiteral("vanishing"), 18));
    QVERIFY(QFile::remove(iconPath(QStringLiteral("vanishing"))));

    m_nowMs += kAvailabilityPositiveTtlMs;
    QVERIFY(!cache->hasIcon(QStringLiteral("vanishing"), 18));
    const IconAvailabilityCache::Statistics statistics = cache->statistics();
    QCOMPARE(statistics.revalidations, quint64(0));
    QCOMPARE(statistics.diskLookups, quint64(2));
}

void ShellIconsAvailabilityCacheTest::invalidateForgetsEveryVerdict()
{
    QVERIFY(makeIcon(QStringLiteral("old-theme-icon")));
    const auto cache = makeCache();
    QVERIFY(cache->hasIcon(QStringLiteral("old-theme-icon"), 18));
    QVERIFY(!cache->hasIcon(QStringLiteral("new-theme-icon"), 18));
    QVERIFY(QFile::remove(iconPath(QStringLiteral("old-theme-icon"))));
    QVERIFY(makeIcon(QStringLiteral("new-theme-icon")));

    cache->invalidate();
    QCOMPARE(cache->statistics().entries, 0);
    QVERIFY(!cache->hasIcon(QStringLiteral("old-theme-icon"), 18));
    QVERIFY(cache->hasIcon(QStringLiteral("new-theme-icon"), 18));
    QCOMPARE(cache->statistics().diskLookups, quint64(4));
}

void ShellIconsAvailabilityCacheTest::sizeScaleAndSymbolicAreDistinctRequests()
{
    QVERIFY(makeIcon(QStringLiteral("plain")));
    const auto cache = makeCache();
    QVERIFY(cache->hasIcon(QStringLiteral("plain"), 18, 1.0, false));
    QVERIFY(cache->hasIcon(QStringLiteral("plain"), 22, 1.0, false));
    QVERIFY(cache->hasIcon(QStringLiteral("plain"), 18, 2.0, false));
    QVERIFY(cache->hasIcon(QStringLiteral("plain"), 18, 1.0, true));
    QCOMPARE(cache->statistics().diskLookups, quint64(4));
    QCOMPARE(cache->statistics().entries, 4);
}

void ShellIconsAvailabilityCacheTest::refusedNamesAreAnsweredWithoutBeingRetained()
{
    const auto cache = makeCache();
    const QStringList hostile{QStringLiteral("../escape"), QStringLiteral("a/b"),
                              QString(), QString(200, QLatin1Char('x')),
                              QStringLiteral("with space"), QStringLiteral("a..b")};
    for (const QString &name : hostile) {
        QVERIFY2(!cache->hasIcon(name, 18), qPrintable(name));
    }
    const IconAvailabilityCache::Statistics statistics = cache->statistics();
    QCOMPARE(statistics.entries, 0);
    QCOMPARE(statistics.diskLookups, quint64(0));
    QCOMPARE(statistics.queries, quint64(hostile.size()));
}

void ShellIconsAvailabilityCacheTest::memoStaysBoundedWhateverIsAsked()
{
    const auto cache = makeCache();
    for (int index = 0; index < kMaxAvailabilityEntries * 3; ++index) {
        QVERIFY(!cache->hasIcon(QStringLiteral("missing-%1").arg(index), 18));
        QVERIFY(cache->statistics().entries <= kMaxAvailabilityEntries);
    }
    // Answers stay correct across the restart of a full memo.
    QVERIFY(makeIcon(QStringLiteral("late")));
    m_nowMs += kAvailabilityNegativeTtlMs;
    QVERIFY(cache->hasIcon(QStringLiteral("late"), 18));
}

void ShellIconsAvailabilityCacheTest::identicalRequestsAfterNormalizationShareOneVerdict()
{
    QVERIFY(makeIcon(QStringLiteral("clamped")));
    const auto cache = makeCache();
    // Sizes and scales are clamped exactly as the locator clamps them, so
    // equivalent requests cannot multiply entries.
    QVERIFY(cache->hasIcon(QStringLiteral("clamped"), 100000, 99.0));
    QVERIFY(cache->hasIcon(QStringLiteral("clamped"), 512, 4.0));
    QVERIFY(cache->hasIcon(QStringLiteral("clamped"), -5, qQNaN()));
    QVERIFY(cache->hasIcon(QStringLiteral("clamped"), 1, 1.0));
    const IconAvailabilityCache::Statistics statistics = cache->statistics();
    QCOMPARE(statistics.diskLookups, quint64(2));
    QCOMPARE(statistics.entries, 2);
}

QTEST_GUILESS_MAIN(ShellIconsAvailabilityCacheTest)
#include "tst_shell_icons_availability_cache.moc"
