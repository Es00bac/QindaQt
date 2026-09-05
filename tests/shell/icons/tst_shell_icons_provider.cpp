// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/shell/icons/icon_image_provider.h>
#include <qindaqt/shell/icons/icon_theme_limits.h>

#include <QBuffer>
#include <QFile>
#include <QtTest>

#include <malloc.h>

#include "shell_icons_test_fixtures.h"

using namespace QindaQt::Shell::Icons;

// IconImageProvider offscreen rows under QT_FATAL_WARNINGS=1 with the host
// display and bus variables unset (CTest environment): SVG/raster rendering
// at the requested device size, symbolic recoloring, placeholder
// determinism, cache bounds, and hostile URL ids. Negative controls fail on
// a tree without the rule (e.g. without recoloring the symbolic render keeps
// its source pixels).
class ShellIconsProviderTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void rasterRendersAtDeviceSize();
    void svgRendersAtDeviceSize();
    void symbolicSvgRecolors();
    void nonSymbolicIgnoresColor();
    void unresolvedReturnsDeterministicPlaceholder();
    void placeholderNeverEmpty();
    void requestedSizeHonoredWithoutSizeParam();
    void sizeClampsToCeiling();
    void hostileIdsReturnPlaceholder();
    void hostileIdsReturnPlaceholder_data();
    void cacheBoundKeepsResults();
    void overlongIdRefusedBeforeCache();
    void canonicalIdsShareOneCacheEntry();
    void hostileIdFloodKeepsCacheKeysBounded();
    void hostileIdFloodKeepsRssBounded();

private:
    static QByteArray fingerprint(const QImage &image)
    {
        QByteArray bytes;
        QBuffer buffer(&bytes);
        buffer.open(QIODevice::WriteOnly);
        image.convertToFormat(QImage::Format_ARGB32_Premultiplied).save(&buffer, "png");
        return bytes;
    }

    static qint64 processRssKiB()
    {
        QFile file(QStringLiteral("/proc/self/status"));
        if (!file.open(QIODevice::ReadOnly)) {
            return -1;
        }
        const QByteArray contents = file.readAll();
        const qsizetype pos = contents.indexOf("VmRSS:");
        if (pos < 0) {
            return -1;
        }
        const QByteArray line =
            contents.mid(pos + 6, contents.indexOf('\n', pos) - pos - 6);
        return line.simplified().split(' ').value(0).toLongLong();
    }

    QString m_icons1;
    QString m_icons2;
    std::unique_ptr<IconImageProvider> m_provider;
};

void ShellIconsProviderTest::initTestCase()
{
    const QString base = ShellIconsTest::fixtureRoot() + QStringLiteral("/provider");
    QVERIFY2(ShellIconsTest::buildLocatorFixtures(base), "provider fixture tree");
    m_icons1 = base + QStringLiteral("/icons1");
    m_icons2 = base + QStringLiteral("/icons2");
    m_provider = std::make_unique<IconImageProvider>(
        QStringList { m_icons1, m_icons2 }, QStringList { QStringLiteral("fixturetheme") });
}

void ShellIconsProviderTest::rasterRendersAtDeviceSize()
{
    QSize reported;
    const QImage image = m_provider->requestImage(QStringLiteral("exact?size=16"), &reported,
                                                  QSize());
    QVERIFY(!image.isNull());
    QCOMPARE(image.width(), 16);
    QCOMPARE(image.height(), 16);
    QCOMPARE(reported, image.size());
    // The fixture pixel is pure red.
    QCOMPARE(image.pixelColor(image.rect().center()), QColor(255, 0, 0));
}

void ShellIconsProviderTest::svgRendersAtDeviceSize()
{
    const QImage image =
        m_provider->requestImage(QStringLiteral("vector?size=48&scale=2"), nullptr, QSize());
    QVERIFY(!image.isNull());
    QCOMPARE(image.width(), 96);
    // The green circle covers the center.
    const QColor center = image.pixelColor(image.rect().center());
    QVERIFY(center.green() > 200);
    QVERIFY(center.red() < 60);
}

void ShellIconsProviderTest::symbolicSvgRecolors()
{
    const QImage image = m_provider->requestImage(
        QStringLiteral("sym?size=32&symbolic=1&color=%23ff0000"), nullptr, QSize());
    QVERIFY(!image.isNull());
    QCOMPARE(image.width(), 32);
    // The black symbolic square recolored to #ff0000 keeps its alpha shape.
    const QColor center = image.pixelColor(image.rect().center());
    QCOMPARE(center, QColor(255, 0, 0));
    // Without the color parameter the symbolic SVG renders its own pixels.
    const QImage plain =
        m_provider->requestImage(QStringLiteral("sym?size=32&symbolic=1"), nullptr, QSize());
    const QColor plainCenter = plain.pixelColor(plain.rect().center());
    QVERIFY(plainCenter.red() < 60);
}

void ShellIconsProviderTest::nonSymbolicIgnoresColor()
{
    const QImage image = m_provider->requestImage(
        QStringLiteral("exact?size=16&color=%2300ff00"), nullptr, QSize());
    QCOMPARE(image.pixelColor(image.rect().center()), QColor(255, 0, 0));
}

void ShellIconsProviderTest::unresolvedReturnsDeterministicPlaceholder()
{
    const QImage first =
        m_provider->requestImage(QStringLiteral("no-such-icon?size=24"), nullptr, QSize());
    const QImage second =
        m_provider->requestImage(QStringLiteral("no-such-icon?size=24"), nullptr, QSize());
    QVERIFY(!first.isNull());
    QCOMPARE(first.size(), QSize(24, 24));
    QCOMPARE(fingerprint(first), fingerprint(second));
    // The placeholder is not any resolved fixture image.
    const QImage real = m_provider->requestImage(QStringLiteral("exact?size=16"), nullptr,
                                                 QSize());
    QVERIFY(fingerprint(first) != fingerprint(real));
}

void ShellIconsProviderTest::placeholderNeverEmpty()
{
    const QImage image =
        m_provider->requestImage(QStringLiteral("missing?size=40"), nullptr, QSize());
    QVERIFY(!image.isNull());
    QVERIFY(image.width() > 0 && image.height() > 0);
    // The neutral mark paints something: not fully transparent.
    QVERIFY(image.pixelColor(image.rect().center()).alpha() > 0);
}

void ShellIconsProviderTest::requestedSizeHonoredWithoutSizeParam()
{
    const QImage image = m_provider->requestImage(QStringLiteral("exact"), nullptr,
                                                  QSize(16, 16));
    QCOMPARE(image.size(), QSize(16, 16));
    QCOMPARE(image.pixelColor(image.rect().center()), QColor(255, 0, 0));
}

void ShellIconsProviderTest::sizeClampsToCeiling()
{
    const QImage image = m_provider->requestImage(
        QStringLiteral("exact?size=99999&scale=9"), nullptr, QSize());
    QVERIFY(!image.isNull());
    QVERIFY(image.width() <= kMaxIconLogicalSize * int(kMaxIconScale));
}

void ShellIconsProviderTest::hostileIdsReturnPlaceholder()
{
    QFETCH(QString, id);
    const QImage image = m_provider->requestImage(id, nullptr, QSize());
    QVERIFY2(!image.isNull(), qPrintable(id));
    QVERIFY2(image.width() > 0, qPrintable(id));
}

void ShellIconsProviderTest::hostileIdsReturnPlaceholder_data()
{
    QTest::addColumn<QString>("id");
    QTest::newRow("traversal") << QStringLiteral("../secret?size=32");
    QTest::newRow("absolute") << QStringLiteral("/etc/passwd?size=32");
    QTest::newRow("nul") << QString::fromUtf8("evil\0name?size=32", 15);
    QTest::newRow("space-in-name") << QStringLiteral("two words?size=32");
    QTest::newRow("negative-size") << QStringLiteral("exact?size=-5");
    QTest::newRow("non-numeric-size") << QStringLiteral("exact?size=abc");
    QTest::newRow("nan-scale") << QStringLiteral("exact?size=32&scale=nan");
    QTest::newRow("bad-color") << QStringLiteral("sym?size=32&symbolic=1&color=red");
    QTest::newRow("empty-id") << QString();
}

void ShellIconsProviderTest::cacheBoundKeepsResults()
{
    const QByteArray reference =
        fingerprint(m_provider->requestImage(QStringLiteral("exact?size=16"), nullptr, QSize()));
    // Push the cache past its entry bound with distinct unresolved names.
    for (int i = 0; i < kMaxCachedIconImages + 20; ++i) {
        const QImage evict = m_provider->requestImage(
            QStringLiteral("evict-%1?size=8").arg(i), nullptr, QSize());
        QVERIFY(!evict.isNull());
    }
    const QImage after =
        m_provider->requestImage(QStringLiteral("exact?size=16"), nullptr, QSize());
    QCOMPARE(fingerprint(after), reference);
    QCOMPARE(after.pixelColor(after.rect().center()), QColor(255, 0, 0));
}

void ShellIconsProviderTest::overlongIdRefusedBeforeCache()
{
    // An id beyond the request ceiling is refused before parsing and before
    // any cache access: placeholder out, no cache entry, no retained key
    // bytes. Fails on a tree that caches the raw id.
    const int entriesBefore = m_provider->cacheEntryCount();
    const qsizetype keyBytesBefore = m_provider->cacheKeyBytes();
    const QString hostile = QStringLiteral("pad-")
        + QString(kMaxRequestIdUtf8Bytes * 2, QLatin1Char('x'))
        + QStringLiteral("?size=8");
    const QImage image = m_provider->requestImage(hostile, nullptr, QSize());
    QVERIFY(!image.isNull());
    QVERIFY(image.width() > 0);
    QCOMPARE(m_provider->cacheEntryCount(), entriesBefore);
    QCOMPARE(m_provider->cacheKeyBytes(), keyBytesBefore);
}

void ShellIconsProviderTest::canonicalIdsShareOneCacheEntry()
{
    // Spellings that parse to the same bounded request tuple share one cache
    // entry: the cache is keyed on the tuple, not the raw id. Asserted
    // through both occupancy and retained key bytes so it holds regardless
    // of how full the cache already is.
    m_provider->requestImage(QStringLiteral("exact?size=17"), nullptr, QSize());
    const int entriesAfterFirst = m_provider->cacheEntryCount();
    const qsizetype keyBytesAfterFirst = m_provider->cacheKeyBytes();
    const QImage image = m_provider->requestImage(
        QStringLiteral("exact?scale=1&size=017"), nullptr, QSize());
    QCOMPARE(image.width(), 17);
    QCOMPARE(m_provider->cacheEntryCount(), entriesAfterFirst);
    QCOMPARE(m_provider->cacheKeyBytes(), keyBytesAfterFirst);
}

void ShellIconsProviderTest::hostileIdFloodKeepsCacheKeysBounded()
{
    // The review reproduction: a flood of distinct hostile ids beyond the
    // request ceiling must leave cache occupancy and retained key bytes
    // untouched.
    const qsizetype keyBytesBefore = m_provider->cacheKeyBytes();
    const int entriesBefore = m_provider->cacheEntryCount();
    for (int i = 0; i < kMaxCachedIconImages + 16; ++i) {
        const QString hostile = QStringLiteral("flood-%1-").arg(i)
            + QString(256 * 1024, QLatin1Char('x')) + QStringLiteral("?size=8");
        const QImage image = m_provider->requestImage(hostile, nullptr, QSize());
        QVERIFY(!image.isNull());
    }
    QCOMPARE(m_provider->cacheKeyBytes(), keyBytesBefore);
    QCOMPARE(m_provider->cacheEntryCount(), entriesBefore);

    // A churn of distinct in-ceiling names still cannot exceed the entry
    // bound, and every retained key is a bounded canonical tuple.
    for (int i = 0; i < kMaxCachedIconImages + 16; ++i) {
        m_provider->requestImage(QStringLiteral("churn-%1?size=8").arg(i), nullptr,
                                 QSize());
    }
    QVERIFY(m_provider->cacheEntryCount() <= kMaxCachedIconImages);
    QVERIFY(m_provider->cacheKeyBytes() <= kMaxCachedIconImages * 256);
}

void ShellIconsProviderTest::hostileIdFloodKeepsRssBounded()
{
    const qint64 before = processRssKiB();
    if (before < 0) {
        QSKIP("/proc/self/status VmRSS unavailable");
    }
    // Distinct hostile ids of megabytes each must not be retained: 40 ids of
    // 4 Mi chars would pin ~320 MiB of UTF-16 keys in a cache keyed on the
    // raw id. This row fails at runtime on the unrepaired tree.
    for (int i = 0; i < 40; ++i) {
        const QString hostile = QStringLiteral("rss-%1-").arg(i)
            + QString(4 * 1024 * 1024, QLatin1Char('x')) + QStringLiteral("?size=8");
        const QImage image = m_provider->requestImage(hostile, nullptr, QSize());
        QVERIFY(!image.isNull());
    }
    malloc_trim(0);
    const qint64 after = processRssKiB();
    QVERIFY(after > 0);
    // Generous ceiling: transient allocations may linger in the allocator
    // even after the trim, but retained memory must not scale with hostile
    // id byte length.
    QVERIFY2(after - before < qint64(64) * 1024,
             qPrintable(QStringLiteral("RSS delta KiB: %1").arg(after - before)));
}

QTEST_MAIN(ShellIconsProviderTest)
#include "tst_shell_icons_provider.moc"
