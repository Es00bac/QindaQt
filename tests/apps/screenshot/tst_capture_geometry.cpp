// SPDX-License-Identifier: GPL-3.0-or-later
// KWin payload validation and region geometry: the two places a wrong
// number turns into a wrong (or unsafe) image.
#include "raw_capture_decoder.h"
#include "region_geometry.h"

#include <QTest>

using namespace QindaQt::Screenshot;

namespace {

QVariantMap metadataFor(const QImage &image, qreal scale = 1.0)
{
    return {{QStringLiteral("type"), QStringLiteral("raw")},
            {QStringLiteral("width"), uint(image.width())},
            {QStringLiteral("height"), uint(image.height())},
            {QStringLiteral("stride"), uint(image.bytesPerLine())},
            {QStringLiteral("format"), uint(image.format())},
            {QStringLiteral("scale"), scale}};
}

QByteArray bytesOf(const QImage &image)
{
    return QByteArray(reinterpret_cast<const char *>(image.constBits()), image.sizeInBytes());
}

// A workspace of two 100x50 logical outputs side by side at scale 2, each
// half painted a different colour.
WorkspaceFrame twoOutputs()
{
    QImage image(400, 100, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::red);
    for (int y = 0; y < 100; ++y)
        for (int x = 200; x < 400; ++x)
            image.setPixelColor(x, y, Qt::blue);
    return {image, QPoint(0, 0), 2.0};
}

} // namespace

class CaptureGeometryTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void decodesAValidPayloadIntoAnOwnedCopy();
    void acceptsDeepFormatsKWinMayReturn();
    void refusesEveryMalformedShape();
    void kwinErrorsBecomeSentencesAndCancellationIsNotAnError();
    void dragSelectionIsSweptAndClamped();
    void keyboardNudgingStaysInsideTheDesktop();
    void cropMapsLogicalPixelsToImagePixels();
};

void CaptureGeometryTest::decodesAValidPayloadIntoAnOwnedCopy()
{
    QImage source(3, 2, QImage::Format_ARGB32_Premultiplied);
    source.fill(Qt::green);
    QByteArray bytes = bytesOf(source);
    const DecodedCapture decoded = decodeRawCapture(metadataFor(source, 1.5), bytes);
    QVERIFY2(decoded.ok(), qPrintable(decoded.error));
    QCOMPARE(decoded.image.size(), QSize(3, 2));
    QCOMPARE(decoded.scale, 1.5);
    bytes.fill('\0');
    // AGENT-GUARD: the image must not alias the pipe buffer.
    QCOMPARE(decoded.image.pixelColor(0, 0), QColor(Qt::green));
}

void CaptureGeometryTest::acceptsDeepFormatsKWinMayReturn()
{
    QImage deep(4, 4, QImage::Format_RGBA64_Premultiplied);
    deep.fill(Qt::white);
    const DecodedCapture decoded = decodeRawCapture(metadataFor(deep), bytesOf(deep));
    QVERIFY2(decoded.ok(), qPrintable(decoded.error));
    QCOMPARE(decoded.image.format(), QImage::Format_RGBA64_Premultiplied);
}

void CaptureGeometryTest::refusesEveryMalformedShape()
{
    QImage source(4, 4, QImage::Format_ARGB32_Premultiplied);
    source.fill(Qt::black);
    const QByteArray bytes = bytesOf(source);
    const QVariantMap good = metadataFor(source);

    QVariantMap notRaw = good;
    notRaw.insert(QStringLiteral("type"), QStringLiteral("png"));
    QVERIFY(!decodeRawCapture(notRaw, bytes).ok());
    QVariantMap shortStride = good;
    shortStride.insert(QStringLiteral("stride"), uint(4));
    QVERIFY(!decodeRawCapture(shortStride, bytes).ok());
    QVariantMap indexed = good;
    indexed.insert(QStringLiteral("format"), uint(QImage::Format_Indexed8));
    QVERIFY(!decodeRawCapture(indexed, bytes).ok());
    QVariantMap bogusFormat = good;
    bogusFormat.insert(QStringLiteral("format"), uint(9999));
    QVERIFY(!decodeRawCapture(bogusFormat, bytes).ok());
    QVariantMap huge = good;
    huge.insert(QStringLiteral("width"), uint(40000));
    QVERIFY(!decodeRawCapture(huge, bytes).ok());
    QVariantMap missing = good;
    missing.remove(QStringLiteral("height"));
    QVERIFY(!decodeRawCapture(missing, bytes).ok());
    // A truncated pipe is refused, never padded.
    QVERIFY(!decodeRawCapture(good, bytes.left(bytes.size() - 1)).ok());
    // An absurd scale falls back to 1 rather than distorting geometry.
    QVariantMap badScale = good;
    badScale.insert(QStringLiteral("scale"), 100.0);
    QCOMPARE(decodeRawCapture(badScale, bytes).scale, 1.0);
}

void CaptureGeometryTest::kwinErrorsBecomeSentencesAndCancellationIsNotAnError()
{
    QVERIFY(isKWinCancellation(QStringLiteral("org.qindaqt.KWin.ScreenShot2.Error.Cancelled")));
    QVERIFY(!isKWinCancellation(QStringLiteral("org.qindaqt.KWin.ScreenShot2.Error.NoAuthorized")));
    const QString denied = describeKWinError(QStringLiteral("org.qindaqt.KWin.ScreenShot2.Error.NoAuthorized"), {});
    QVERIFY(denied.contains(QStringLiteral("desktop entry")));
    QVERIFY(!describeKWinError(QStringLiteral("org.qindaqt.KWin.ScreenShot2.Error.NoActiveWindow"), {}).isEmpty());
    QVERIFY(describeKWinError(QStringLiteral("x.y.Unknown"), QStringLiteral("boom")).contains(QStringLiteral("boom")));
}

void CaptureGeometryTest::dragSelectionIsSweptAndClamped()
{
    const QRect desktop(0, 0, 200, 50);
    QCOMPARE(selectionFromPoints(QPoint(10, 10), QPoint(20, 30), desktop), QRect(10, 10, 10, 20));
    // Dragging up-left normalizes.
    QCOMPARE(selectionFromPoints(QPoint(20, 30), QPoint(10, 10), desktop), QRect(10, 10, 10, 20));
    // Dragging past the desktop edge clamps; a click selects nothing.
    QCOMPARE(selectionFromPoints(QPoint(190, 40), QPoint(400, 400), desktop), QRect(190, 40, 10, 10));
    QVERIFY(selectionFromPoints(QPoint(5, 5), QPoint(5, 5), desktop).isEmpty());
    // Crossing from the left output into the right one is one rectangle.
    QCOMPARE(selectionFromPoints(QPoint(90, 0), QPoint(111, 50), desktop), QRect(90, 0, 21, 50));
}

void CaptureGeometryTest::keyboardNudgingStaysInsideTheDesktop()
{
    const QRect desktop(0, 0, 200, 50);
    const QRect start = keyboardStartSelection(QRect(100, 0, 100, 50));
    QCOMPARE(start, QRect(125, 12, 50, 25));
    QCOMPARE(nudgedSelection(start, 10, 0, false, desktop), QRect(135, 12, 50, 25));
    QCOMPARE(nudgedSelection(start, 1000, 1000, false, desktop), QRect(150, 25, 50, 25));
    QCOMPARE(nudgedSelection(start, -1000, 0, false, desktop), QRect(0, 12, 50, 25));
    QCOMPARE(nudgedSelection(start, 5, -3, true, desktop), QRect(125, 12, 55, 22));
    // Resizing never collapses below one pixel or grows past the edge.
    QCOMPARE(nudgedSelection(start, -1000, -1000, true, desktop).size(), QSize(1, 1));
    QCOMPARE(nudgedSelection(start, 1000, 1000, true, desktop), QRect(125, 12, 75, 38));
    // A selection that starts outside the bounds still resizes safely.
    QCOMPARE(nudgedSelection(QRect(300, 300, 5, 5), 1, 1, true, desktop).size(), QSize(1, 1));
}

void CaptureGeometryTest::cropMapsLogicalPixelsToImagePixels()
{
    const WorkspaceFrame frame = twoOutputs();
    QCOMPARE(frame.logicalBounds(), QRect(0, 0, 200, 50));
    QCOMPARE(toImagePixels(frame, QRect(10, 5, 20, 10)), QRect(20, 10, 40, 20));
    const QImage left = cropRegion(frame, QRect(0, 0, 100, 50));
    QCOMPARE(left.size(), QSize(200, 100));
    QCOMPARE(left.pixelColor(199, 99), QColor(Qt::red));
    const QImage straddling = cropRegion(frame, QRect(90, 0, 20, 50));
    QCOMPARE(straddling.size(), QSize(40, 100));
    QCOMPARE(straddling.pixelColor(0, 0), QColor(Qt::red));
    QCOMPARE(straddling.pixelColor(39, 0), QColor(Qt::blue));
    // Fractional scales round outward so no selected row is lost.
    WorkspaceFrame fractional = frame;
    fractional.scale = 1.5;
    QCOMPARE(toImagePixels(fractional, QRect(1, 1, 3, 3)), QRect(1, 1, 5, 5));
    QVERIFY(cropRegion(frame, QRect()).isNull());
    QVERIFY(cropRegion(frame, QRect(500, 500, 10, 10)).isNull());
}

QTEST_GUILESS_MAIN(CaptureGeometryTest)
#include "tst_capture_geometry.moc"
