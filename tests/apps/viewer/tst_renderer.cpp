// SPDX-License-Identifier: GPL-3.0-or-later
#include "document_renderer.h"
#include "fixtures.h"
#include <QImageReader>
#include <QTest>
#include <limits>

using namespace QindaQt::Viewer;
class RendererTest final : public QObject {
    Q_OBJECT
private slots:
    void pdfPagesAndRotation();
    void imageFormats_data();
    void imageFormats();
    void gifAndSvg();
    void imageMetadataIsNotPdf();
    void boundedRendering();
    void failuresAndStaleRequests();
    void encryptedPdf();
};

void RendererTest::pdfPagesAndRotation()
{
    QTemporaryDir temp;
    auto latest = std::make_shared<std::atomic<quint64>>(1);
    DocumentRenderer renderer(latest);
    RenderRequest request{1, ViewerFixtures::pdf(temp.path())};
    auto first = renderer.render(request);
    QVERIFY2(first.error.isEmpty(), qPrintable(first.error));
    QCOMPARE(first.pageCount, 2);
    QVERIFY(first.pageSize.width() > first.pageSize.height());
    QCOMPARE(first.image.pixelColor(first.image.width() / 2, first.image.height() / 2), QColor(Qt::red));
    request.page = 1;
    const auto second = renderer.render(request);
    QCOMPARE(second.page, 1);
    QCOMPARE(second.image.pixelColor(second.image.width() / 2, second.image.height() / 2), QColor(Qt::blue));
    request.rotation = 90;
    const auto rotated = renderer.render(request);
    QCOMPARE(rotated.pageSize.width(), first.pageSize.height());
    QCOMPARE(rotated.image.width(), second.image.height());
    QCOMPARE(rotated.image.height(), second.image.width());
    request.page = 200;
    QCOMPARE(renderer.render(request).page, 1);
}

void RendererTest::imageFormats_data()
{
    QTest::addColumn<QByteArray>("format");
    for (const auto &format : {"png", "jpeg", "webp", "bmp", "tiff"})
        QTest::newRow(format) << QByteArray(format);
}
void RendererTest::imageFormats()
{
    QFETCH(QByteArray, format);
    QTemporaryDir temp;
    const QString path = ViewerFixtures::image(temp.path(), format.constData());
    QVERIFY2(!path.isEmpty(), "A declared MIME format is missing its installed image plugin");
    auto latest = std::make_shared<std::atomic<quint64>>(1);
    DocumentRenderer renderer(latest);
    RenderRequest request{1, path};
    const auto result = renderer.render(request);
    QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
    QCOMPARE(result.image.size(), QSize(120, 80));
    QCOMPARE(result.pageCount, 1);
    request.rotation = 90;
    const auto rotated = renderer.render(request);
    QCOMPARE(rotated.pageSize, QSizeF(80, 120));
    QCOMPARE(rotated.image.size(), QSize(80, 120));
    request.scale = 2;
    QCOMPARE(renderer.render(request).image.size(), QSize(160, 240));
}

void RendererTest::gifAndSvg()
{
    QTemporaryDir temp;
    const QString gif = ViewerFixtures::bytes(temp.path(), QStringLiteral("pixel.gif"),
        QByteArray::fromBase64("R0lGODlhAQABAIAAAAAAAP///yH5BAEAAAAALAAAAAABAAEAAAIBRAA7"));
    const QString svg = ViewerFixtures::bytes(temp.path(), QStringLiteral("square.svg"),
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"40\" height=\"20\"><rect width=\"40\" height=\"20\" fill=\"red\"/></svg>");
    auto latest = std::make_shared<std::atomic<quint64>>(1);
    DocumentRenderer renderer(latest);
    QVERIFY(!renderer.render({1, gif}).image.isNull());
    const auto result = renderer.render({1, svg});
    QCOMPARE(result.image.size(), QSize(40, 20));
    QCOMPARE(result.image.pixelColor(5, 5), QColor(Qt::red));
}

void RendererTest::boundedRendering()
{
    for (const auto size : {QSizeF(1000000, 1000000), QSizeF(1000000, 1), QSizeF(100, 100)}) {
        const QSize bounded = DocumentRenderer::boundedSize(size, 32);
        QVERIFY(bounded.width() <= DocumentRenderer::MaxDimension);
        QVERIFY(bounded.height() <= DocumentRenderer::MaxDimension);
        QVERIFY(static_cast<qint64>(bounded.width()) * bounded.height() <= DocumentRenderer::MaxPixels);
    }
    QVERIFY(DocumentRenderer::boundedSize(QSizeF(-1, 10), 1).isEmpty());
    QVERIFY(DocumentRenderer::boundedSize(QSizeF(10, 10), std::numeric_limits<double>::infinity()).isEmpty());
    QTemporaryDir temp;
    auto latest = std::make_shared<std::atomic<quint64>>(1);
    DocumentRenderer renderer(latest);
    RenderRequest request{1, ViewerFixtures::pdf(temp.path())};
    request.scale = 10000;
    const auto image = renderer.render(request).image;
    QVERIFY(!image.isNull());
    QVERIFY(image.width() <= DocumentRenderer::MaxDimension);
    QVERIFY(static_cast<qint64>(image.width()) * image.height() <= DocumentRenderer::MaxPixels);
}

void RendererTest::imageMetadataIsNotPdf()
{
    QTemporaryDir temp;
    QImage image(20, 10, QImage::Format_RGB32);
    image.fill(Qt::red);
    image.setText(QStringLiteral("Description"), QStringLiteral("A picture of %PDF-1.7 text"));
    const QString path = temp.path() + QStringLiteral("/metadata.png");
    QVERIFY(image.save(path));
    auto latest = std::make_shared<std::atomic<quint64>>(1);
    DocumentRenderer renderer(latest);
    const auto result = renderer.render({1, path});
    QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
    QCOMPARE(result.image.size(), QSize(20, 10));
}

void RendererTest::failuresAndStaleRequests()
{
    QTemporaryDir temp;
    auto latest = std::make_shared<std::atomic<quint64>>(1);
    DocumentRenderer renderer(latest);
    QVERIFY(!renderer.render({1, temp.path() + QStringLiteral("/missing.pdf")}).error.isEmpty());
    const QString corrupt = ViewerFixtures::bytes(temp.path(), QStringLiteral("bad.pdf"), "%PDF-1.7\ngarbage");
    QVERIFY(!renderer.render({1, corrupt}).error.isEmpty());
    const QString text = ViewerFixtures::bytes(temp.path(), QStringLiteral("plain.txt"), "not a picture");
    QVERIFY(!renderer.render({1, text}).error.isEmpty());
    QVERIFY(!renderer.render({1, temp.path()}).error.isEmpty());
    latest->store(2);
    QVERIFY(renderer.render({1, ViewerFixtures::pdf(temp.path())}).image.isNull());
}

void RendererTest::encryptedPdf()
{
    auto latest = std::make_shared<std::atomic<quint64>>(1);
    DocumentRenderer renderer(latest);
    RenderRequest request{1, QStringLiteral(VIEWER_FIXTURES_DIR "/password.pdf")};
    QVERIFY(renderer.render(request).locked);
    request.password = "wrong";
    QVERIFY(renderer.render(request).locked);
    request.password = "viewer-test";
    const auto result = renderer.render(request);
    QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
    QVERIFY(!result.locked);
    QCOMPARE(result.pageCount, 1);
    QVERIFY(!result.image.isNull());
}

QTEST_MAIN(RendererTest)
#include "tst_renderer.moc"
