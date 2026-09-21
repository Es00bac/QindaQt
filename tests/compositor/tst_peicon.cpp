// SPDX-License-Identifier: GPL-3.0-or-later

// ADR-0230: the PE-icon extractor parses untrusted binary input inside the
// compositor process, so the fuzz-shaped rows here ARE the feature: every
// truncation, absurd length, and cycle must yield no icon and no crash. All
// fixtures are built byte-by-byte in peicon_test_fixture.h - no real
// executable content.

#include "qindaqt/compositor/peicon.h"

#include "peicon_test_fixture.h"

#include <QBuffer>
#include <QtTest>

using namespace QindaQt::Compositor;

namespace {

constexpr int kTarget = 48;

[[nodiscard]] QImage extract(QByteArray bytes, int target = kTarget)
{
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::ReadOnly);
    return extractPeIcon(buffer, bytes.size(), target);
}

[[nodiscard]] PeFixture::Options singlePngIcon(int size, QRgb color,
                                               quint32 id = 1)
{
    PeFixture::Options options;
    QByteArray group = PeFixture::groupDirectory(1);
    const QByteArray payload = PeFixture::pngPayload(size, size, color);
    PeFixture::appendGroupEntry(group, quint8(size == 256 ? 0 : size),
                                quint8(size == 256 ? 0 : size), 0,
                                quint32(payload.size()), quint16(id));
    options.groups.append(PeFixture::GroupResource{id, group});
    options.icons.append(PeFixture::IconResource{id, payload});
    return options;
}

[[nodiscard]] PeFixture::Options singleBmpIcon(int size, quint16 bitCount,
                                               QRgb color, bool zeroAlpha,
                                               quint32 id = 1,
                                               const QList<int> &masked = {})
{
    PeFixture::Options options;
    const QByteArray payload = bitCount == 24
        ? PeFixture::dibPayload24(size, size, color)
        : PeFixture::dibPayload32(size, size, color, zeroAlpha, masked);
    QByteArray group = PeFixture::groupDirectory(1);
    PeFixture::appendGroupEntry(group, quint8(size), quint8(size), bitCount,
                                quint32(payload.size()), quint16(id));
    options.groups.append(PeFixture::GroupResource{id, group});
    options.icons.append(PeFixture::IconResource{id, payload});
    return options;
}

[[nodiscard]] PeFixture::Options validBase()
{
    return singlePngIcon(48, qRgba(10, 20, 30, 255));
}

} // namespace

class PeIconTests final : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void pngInIcoRoundTrips();
    void bmp32KeepsSourceAlpha();
    void bmp32ZeroAlphaMeansOpaque();
    void bmp32AppliesTheAndMask();
    void bmp24DecodesOpaque();
    void prefersThirtyTwoBitAtOrAboveTarget();
    void thirtyTwoBitBeatsLargerLowerDepth();
    void withoutThirtyTwoBitPrefersSmallestAboveTarget();
    void withoutThirtyTwoBitFallsToLargestBelow();
    void pe32PlusImagesParse();
    void emptyAndTruncatedFilesYieldNothing();
    void wrongMagicsYieldNothing();
    void outOfBoundsNtHeadersYieldNothing();
    void unknownOptionalHeaderMagicYieldsNothing();
    void sectionCountViolationsYieldNothing();
    void sectionTablePastEndYieldsNothing();
    void resourcesOutsideSectionsYieldNothing();
    void absentResourceDirectoryYieldsNothing();
    void zeroSizedIconPayloadYieldsNothing();
    void absurdGroupEntryCountYieldsNothing();
    void emptyGroupDirectoryYieldsNothing();
    void absurdIconLengthIsSkipped();
    void unknownIconIdIsSkipped();
    void garbagePayloadYieldsNothing();
    void absurdBitmapDimensionsYieldNothing();
    void cycleInResourceTreeYieldsNothing();
    void dataEntryOutsideSectionsIsSkipped();
};

void PeIconTests::pngInIcoRoundTrips()
{
    const QImage icon = extract(PeFixture::buildPe(singlePngIcon(48, qRgba(200, 30, 40, 255))));
    QVERIFY(!icon.isNull());
    QCOMPARE(icon.size(), QSize(48, 48));
    const QRgb pixel = icon.convertToFormat(QImage::Format_ARGB32).pixel(0, 0);
    QCOMPARE(qRed(pixel), 200);
    QCOMPARE(qGreen(pixel), 30);
    QCOMPARE(qBlue(pixel), 40);
}

void PeIconTests::bmp32KeepsSourceAlpha()
{
    const QImage icon = extract(
        PeFixture::buildPe(singleBmpIcon(32, 32, qRgba(10, 220, 20, 128), false)));
    QVERIFY(!icon.isNull());
    QCOMPARE(icon.size(), QSize(32, 32));
    const QRgb pixel = icon.pixel(0, 0);
    QCOMPARE(qRed(pixel), 10);
    QCOMPARE(qGreen(pixel), 220);
    QCOMPARE(qAlpha(pixel), 128);
}

void PeIconTests::bmp32ZeroAlphaMeansOpaque()
{
    const QImage icon = extract(
        PeFixture::buildPe(singleBmpIcon(32, 32, qRgba(1, 2, 3, 0), true)));
    QVERIFY(!icon.isNull());
    QCOMPARE(qAlpha(icon.pixel(0, 0)), 255);
}

void PeIconTests::bmp32AppliesTheAndMask()
{
    const QImage icon = extract(
        PeFixture::buildPe(singleBmpIcon(8, 8, qRgba(9, 9, 9, 0), true, 1, {0})));
    QVERIFY(!icon.isNull());
    QCOMPARE(qAlpha(icon.pixel(0, 0)), 0);
    QCOMPARE(qAlpha(icon.pixel(1, 0)), 255);
}

void PeIconTests::bmp24DecodesOpaque()
{
    const QImage icon = extract(
        PeFixture::buildPe(singleBmpIcon(32, 24, qRgba(70, 80, 90, 255), false)));
    QVERIFY(!icon.isNull());
    const QRgb pixel = icon.pixel(0, 0);
    QCOMPARE(qRed(pixel), 70);
    QCOMPARE(qAlpha(pixel), 255);
}

void PeIconTests::prefersThirtyTwoBitAtOrAboveTarget()
{
    PeFixture::Options options;
    QByteArray group = PeFixture::groupDirectory(3);
    const QByteArray small32 = PeFixture::pngPayload(16, 16, qRgba(1, 1, 1, 255));
    const QByteArray large32 = PeFixture::pngPayload(64, 64, qRgba(2, 2, 2, 255));
    const QByteArray mid24 = PeFixture::dibPayload24(48, 48, qRgba(3, 3, 3, 255));
    PeFixture::appendGroupEntry(group, 16, 16, 32, quint32(small32.size()), 1);
    PeFixture::appendGroupEntry(group, 64, 64, 32, quint32(large32.size()), 2);
    PeFixture::appendGroupEntry(group, 48, 48, 24, quint32(mid24.size()), 3);
    options.groups.append(PeFixture::GroupResource{1, group});
    options.icons.append(PeFixture::IconResource{1, small32});
    options.icons.append(PeFixture::IconResource{2, large32});
    options.icons.append(PeFixture::IconResource{3, mid24});
    const QImage icon = extract(PeFixture::buildPe(options));
    QVERIFY(!icon.isNull());
    QCOMPARE(icon.size(), QSize(64, 64));
}

void PeIconTests::thirtyTwoBitBeatsLargerLowerDepth()
{
    PeFixture::Options options;
    QByteArray group = PeFixture::groupDirectory(2);
    const QByteArray small32 = PeFixture::pngPayload(16, 16, qRgba(1, 1, 1, 255));
    const QByteArray mid24 = PeFixture::dibPayload24(48, 48, qRgba(3, 3, 3, 255));
    PeFixture::appendGroupEntry(group, 16, 16, 32, quint32(small32.size()), 1);
    PeFixture::appendGroupEntry(group, 48, 48, 24, quint32(mid24.size()), 2);
    options.groups.append(PeFixture::GroupResource{1, group});
    options.icons.append(PeFixture::IconResource{1, small32});
    options.icons.append(PeFixture::IconResource{2, mid24});
    const QImage icon = extract(PeFixture::buildPe(options));
    QVERIFY(!icon.isNull());
    QCOMPARE(icon.size(), QSize(16, 16));
}

void PeIconTests::withoutThirtyTwoBitPrefersSmallestAboveTarget()
{
    PeFixture::Options options;
    QByteArray group = PeFixture::groupDirectory(2);
    const QByteArray small24 = PeFixture::dibPayload24(16, 16, qRgba(1, 1, 1, 255));
    const QByteArray large24 = PeFixture::dibPayload24(64, 64, qRgba(2, 2, 2, 255));
    PeFixture::appendGroupEntry(group, 16, 16, 24, quint32(small24.size()), 1);
    PeFixture::appendGroupEntry(group, 64, 64, 24, quint32(large24.size()), 2);
    options.groups.append(PeFixture::GroupResource{1, group});
    options.icons.append(PeFixture::IconResource{1, small24});
    options.icons.append(PeFixture::IconResource{2, large24});
    const QImage icon = extract(PeFixture::buildPe(options));
    QVERIFY(!icon.isNull());
    QCOMPARE(icon.size(), QSize(64, 64));
}

void PeIconTests::withoutThirtyTwoBitFallsToLargestBelow()
{
    PeFixture::Options options;
    QByteArray group = PeFixture::groupDirectory(2);
    const QByteArray tiny24 = PeFixture::dibPayload24(16, 16, qRgba(1, 1, 1, 255));
    const QByteArray mid24 = PeFixture::dibPayload24(32, 32, qRgba(2, 2, 2, 255));
    PeFixture::appendGroupEntry(group, 16, 16, 24, quint32(tiny24.size()), 1);
    PeFixture::appendGroupEntry(group, 32, 32, 24, quint32(mid24.size()), 2);
    options.groups.append(PeFixture::GroupResource{1, group});
    options.icons.append(PeFixture::IconResource{1, tiny24});
    options.icons.append(PeFixture::IconResource{2, mid24});
    const QImage icon = extract(PeFixture::buildPe(options));
    QVERIFY(!icon.isNull());
    QCOMPARE(icon.size(), QSize(32, 32));
}

void PeIconTests::pe32PlusImagesParse()
{
    PeFixture::Options options = singlePngIcon(48, qRgba(5, 6, 7, 255));
    options.optionalMagic = 0x20B;
    const QImage icon = extract(PeFixture::buildPe(options));
    QVERIFY(!icon.isNull());
    QCOMPARE(icon.size(), QSize(48, 48));
}

void PeIconTests::emptyAndTruncatedFilesYieldNothing()
{
    QVERIFY(extract({}).isNull());
    const QByteArray whole = PeFixture::buildPe(validBase());
    for (const qint64 cut :
         {qint64(1), qint64(63), qint64(0x3B), qint64(0x7F), qint64(0x81),
          qint64(0x86), qint64(0x9A), qint64(0x100), qint64(0x187),
          qint64(0x1FF), qint64(0x200), qint64(0x208), qint64(0x210),
          qint64(0x230)}) {
        QVERIFY2(extract(whole.left(int(cut))).isNull(), qPrintable(QString::number(cut)));
    }
    // A payload truncated mid-bitmap fails the span check deterministically.
    const QByteArray bmpWhole = PeFixture::buildPe(
        singleBmpIcon(32, 32, qRgba(1, 2, 3, 255), false));
    QVERIFY(extract(bmpWhole.left(bmpWhole.size() - 3)).isNull());
}

void PeIconTests::wrongMagicsYieldNothing()
{
    PeFixture::Options options = validBase();
    options.mzMagic = false;
    QVERIFY(extract(PeFixture::buildPe(options)).isNull());
    options = validBase();
    options.peSignature = false;
    QVERIFY(extract(PeFixture::buildPe(options)).isNull());
}

void PeIconTests::outOfBoundsNtHeadersYieldNothing()
{
    PeFixture::Options options = validBase();
    options.eLfanew = 0x8000;
    QVERIFY(extract(PeFixture::buildPe(options)).isNull());
    // e_lfanew into the middle of the image payload: not a PE signature.
    options = validBase();
    options.eLfanew = 0x180;
    QVERIFY(extract(PeFixture::buildPe(options)).isNull());
}

void PeIconTests::unknownOptionalHeaderMagicYieldsNothing()
{
    PeFixture::Options options = validBase();
    options.optionalMagic = 0x999;
    QVERIFY(extract(PeFixture::buildPe(options)).isNull());
}

void PeIconTests::sectionCountViolationsYieldNothing()
{
    PeFixture::Options zero = validBase();
    zero.sectionCount = 0;
    QVERIFY(extract(PeFixture::buildPe(zero)).isNull());
    PeFixture::Options flood = validBase();
    flood.sectionCount = 200; // beyond the 96-section cap
    QVERIFY(extract(PeFixture::buildPe(flood)).isNull());
}

void PeIconTests::sectionTablePastEndYieldsNothing()
{
    PeFixture::Options options = validBase();
    options.sectionRawBeyondEnd = true;
    QVERIFY(extract(PeFixture::buildPe(options)).isNull());
}

void PeIconTests::resourcesOutsideSectionsYieldNothing()
{
    PeFixture::Options options = validBase();
    options.resourceRvaOutsideSections = true;
    QVERIFY(extract(PeFixture::buildPe(options)).isNull());
}

void PeIconTests::absentResourceDirectoryYieldsNothing()
{
    PeFixture::Options options = validBase();
    options.resourceRva = 0;
    QVERIFY(extract(PeFixture::buildPe(options)).isNull());
}

void PeIconTests::zeroSizedIconPayloadYieldsNothing()
{
    PeFixture::Options options = validBase();
    options.groups.clear();
    options.groups.append(PeFixture::GroupResource{1, {}});
    QVERIFY(extract(PeFixture::buildPe(options)).isNull());
}

void PeIconTests::absurdGroupEntryCountYieldsNothing()
{
    PeFixture::Options options;
    // Declares 400 entries, carries the bytes of one.
    QByteArray group = PeFixture::groupDirectory(400);
    const QByteArray payload = PeFixture::pngPayload(48, 48, qRgba(1, 2, 3, 255));
    PeFixture::appendGroupEntry(group, 48, 48, 0, quint32(payload.size()), 1);
    options.groups.append(PeFixture::GroupResource{1, group});
    options.icons.append(PeFixture::IconResource{1, payload});
    QVERIFY(extract(PeFixture::buildPe(options)).isNull());
}

void PeIconTests::emptyGroupDirectoryYieldsNothing()
{
    PeFixture::Options options;
    options.groups.append(PeFixture::GroupResource{1, PeFixture::groupDirectory(0)});
    QVERIFY(extract(PeFixture::buildPe(options)).isNull());
}

void PeIconTests::absurdIconLengthIsSkipped()
{
    PeFixture::Options options = validBase();
    QByteArray group = PeFixture::groupDirectory(1);
    PeFixture::appendGroupEntry(group, 48, 48, 32, 0x7FFFFFFFu, 1);
    options.groups.first().payload = group;
    QVERIFY(extract(PeFixture::buildPe(options)).isNull());
}

void PeIconTests::unknownIconIdIsSkipped()
{
    PeFixture::Options options = validBase();
    QByteArray group = PeFixture::groupDirectory(1);
    PeFixture::appendGroupEntry(group, 48, 48, 32, 64, 99); // no RT_ICON id 99
    options.groups.first().payload = group;
    QVERIFY(extract(PeFixture::buildPe(options)).isNull());
}

void PeIconTests::garbagePayloadYieldsNothing()
{
    PeFixture::Options options;
    const QByteArray garbage("not an icon at all", 18);
    QByteArray group = PeFixture::groupDirectory(1);
    PeFixture::appendGroupEntry(group, 16, 16, 32, quint32(garbage.size()), 1);
    options.groups.append(PeFixture::GroupResource{1, group});
    options.icons.append(PeFixture::IconResource{1, garbage});
    QVERIFY(extract(PeFixture::buildPe(options)).isNull());
}

void PeIconTests::absurdBitmapDimensionsYieldNothing()
{
    // A hand-rolled 40-byte header claiming a 100000x200000 bitmap: every
    // computed span overflows the payload ceiling and the dimension cap.
    QByteArray payload;
    payload.append(40, char(0));
    PeFixture::putU32(payload, 0, 40);
    PeFixture::putU32(payload, 4, 100000);
    PeFixture::putU32(payload, 8, 200000);
    PeFixture::putU16(payload, 12, 1);
    PeFixture::putU16(payload, 14, 32);
    payload.append(QByteArray(256, char(0)));
    PeFixture::Options options;
    QByteArray group = PeFixture::groupDirectory(1);
    PeFixture::appendGroupEntry(group, 48, 48, 32, quint32(payload.size()), 1);
    options.groups.append(PeFixture::GroupResource{1, group});
    options.icons.append(PeFixture::IconResource{1, payload});
    QVERIFY(extract(PeFixture::buildPe(options)).isNull());
}

void PeIconTests::cycleInResourceTreeYieldsNothing()
{
    QByteArray image = PeFixture::buildPe(validBase());
    // Point the group name-directory's child back at the ROOT directory.
    // Layout: section at 0x200; root is 16+2*8=32 bytes; the group directory
    // follows, its first entry's target at 0x200 + 32 + 16 + 4.
    PeFixture::putU32(image, 0x200 + 32 + 16 + 4, 0x80000000u);
    QVERIFY(extract(image).isNull());
}

void PeIconTests::dataEntryOutsideSectionsIsSkipped()
{
    QByteArray image = PeFixture::buildPe(validBase());
    // The icon's data entry sits at rsrc offset 144 with one group and one
    // icon (root 32 + group dir 24 + icon dir 24 + two lang dirs 48 = 128,
    // group entry 16 -> 144). Poison its RVA.
    PeFixture::putU32(image, 0x200 + 144, 0x70000000u);
    QVERIFY(extract(image).isNull());
}

QTEST_GUILESS_MAIN(PeIconTests)

#include "tst_peicon.moc"
