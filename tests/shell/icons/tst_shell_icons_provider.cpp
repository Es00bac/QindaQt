// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/shell/icons/icon_image_provider.h>
#include <qindaqt/shell/icons/icon_theme_limits.h>

#include <QBuffer>
#include <QtTest>

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

private:
    static QByteArray fingerprint(const QImage &image)
    {
        QByteArray bytes;
        QBuffer buffer(&bytes);
        buffer.open(QIODevice::WriteOnly);
        image.convertToFormat(QImage::Format_ARGB32_Premultiplied).save(&buffer, "png");
        return bytes;
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

QTEST_MAIN(ShellIconsProviderTest)
#include "tst_shell_icons_provider.moc"
