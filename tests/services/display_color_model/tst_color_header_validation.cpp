// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest/QtTest>
#include <qindaqt/services/display_color_model/color_limits.h>
#include <qindaqt/services/display_color_model/color_validation.h>
#include "support/color_test_data.h"

using namespace QindaQt::DisplayColor;
using namespace QindaQt::DisplayColor::Testing;

class ColorHeaderValidationTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testValidHeaders();
    void testTruncatedHeaders();
    void testInvalidMagic();
    void testInvalidDeclaredSize();
    void testUnsupportedColorSpaces();
    void testUnsupportedConnectionSpaces();
    void testUnsupportedDeviceClasses();
    void testVersionBounds();
    void testHeaderLargerThanTotalFileSize();
    void testHeaderBufferExceedsDeclaredSize();
    void testHeaderSummaryExtraction();
};

void ColorHeaderValidationTest::testValidHeaders()
{
    // Valid standard monitor profile
    {
        const QByteArray data = createValidIccHeader(1024, "mntr", "RGB ", "XYZ ");
        const auto [status, summary] = validateIccHeader(data, 1024);
        QCOMPARE(status, ProfileValidationStatus::Valid);
        QVERIFY(summary.valid);
        QCOMPARE(summary.profileSize, 1024u);
        QCOMPARE(summary.deviceClass, QByteArray("mntr"));
        QCOMPARE(summary.dataColorSpace, QByteArray("RGB "));
        QCOMPARE(summary.connectionSpace, QByteArray("XYZ "));
    }

    // Valid colorspace conversion profile with Lab PCS
    {
        const QByteArray data = createValidIccHeader(2048, "spac", "GRAY", "Lab ");
        const auto [status, summary] = validateIccHeader(data, 2048);
        QCOMPARE(status, ProfileValidationStatus::Valid);
        QVERIFY(summary.valid);
        QCOMPARE(summary.deviceClass, QByteArray("spac"));
        QCOMPARE(summary.dataColorSpace, QByteArray("GRAY"));
        QCOMPARE(summary.connectionSpace, QByteArray("Lab "));
    }
}

void ColorHeaderValidationTest::testTruncatedHeaders()
{
    // Empty data
    {
        const auto [status, summary] = validateIccHeader(QByteArray());
        QCOMPARE(status, ProfileValidationStatus::EmptyData);
        QVERIFY(!summary.valid);
    }

    // 1 byte short
    {
        const QByteArray data(127, '\0');
        const auto [status, summary] = validateIccHeader(data);
        QCOMPARE(status, ProfileValidationStatus::HeaderTooSmall);
        QVERIFY(!summary.valid);
    }
}

void ColorHeaderValidationTest::testInvalidMagic()
{
    QByteArray data = createValidIccHeader();
    // Overwrite magic at bytes 36..39
    data[36] = 'N';
    data[37] = 'O';
    data[38] = 'P';
    data[39] = 'E';

    const auto [status, summary] = validateIccHeader(data);
    QCOMPARE(status, ProfileValidationStatus::InvalidMagic);
    QVERIFY(!summary.valid);
}

void ColorHeaderValidationTest::testInvalidDeclaredSize()
{
    // Declared size smaller than header minimum (127 bytes)
    {
        QByteArray data = createValidIccHeader(127);
        const auto [status, summary] = validateIccHeader(data);
        QCOMPARE(status, ProfileValidationStatus::InvalidDeclaredSize);
        QVERIFY(!summary.valid);
    }

    // Declared size exceeds maximum allowable limit (4 MiB + 1 byte)
    {
        QByteArray data = createValidIccHeader(MaxIccProfileSizeBytes + 1);
        const auto [status, summary] = validateIccHeader(data);
        QCOMPARE(status, ProfileValidationStatus::InvalidDeclaredSize);
        QVERIFY(!summary.valid);
    }

    // Declared size exceeds actual file size
    {
        QByteArray data = createValidIccHeader(5000);
        const auto [status, summary] = validateIccHeader(data, 2000);
        QCOMPARE(status, ProfileValidationStatus::InvalidDeclaredSize);
        QVERIFY(!summary.valid);
    }
}

void ColorHeaderValidationTest::testUnsupportedColorSpaces()
{
    // CMYK is unsupported for display color model
    {
        QByteArray data = createValidIccHeader(1024, "mntr", "CMYK", "XYZ ");
        const auto [status, summary] = validateIccHeader(data);
        QCOMPARE(status, ProfileValidationStatus::UnsupportedColorSpace);
        QVERIFY(!summary.valid);
    }

    // Unknown garbage color space
    {
        QByteArray data = createValidIccHeader(1024, "mntr", "????", "XYZ ");
        const auto [status, summary] = validateIccHeader(data);
        QCOMPARE(status, ProfileValidationStatus::UnsupportedColorSpace);
        QVERIFY(!summary.valid);
    }
}

void ColorHeaderValidationTest::testUnsupportedConnectionSpaces()
{
    // RGB as PCS is invalid according to ICC specs
    {
        QByteArray data = createValidIccHeader(1024, "mntr", "RGB ", "RGB ");
        const auto [status, summary] = validateIccHeader(data);
        QCOMPARE(status, ProfileValidationStatus::UnsupportedConnectionSpace);
        QVERIFY(!summary.valid);
    }
}

void ColorHeaderValidationTest::testUnsupportedDeviceClasses()
{
    // Unknown device class
    {
        QByteArray data = createValidIccHeader(1024, "xxxx", "RGB ", "XYZ ");
        const auto [status, summary] = validateIccHeader(data);
        QCOMPARE(status, ProfileValidationStatus::UnsupportedProfileClass);
        QVERIFY(!summary.valid);
    }
}

void ColorHeaderValidationTest::testVersionBounds()
{
    // ICC version lives at bytes 8..11 with the major generation in the top
    // byte; known published generations are 2 through 5.

    // Version 4.4 is accepted
    {
        QByteArray data = createValidIccHeader(1024);
        data[8] = 0x04; // major 4
        const auto [status, summary] = validateIccHeader(data, 1024);
        QCOMPARE(status, ProfileValidationStatus::Valid);
        QCOMPARE(summary.version, 0x04400000u);
    }

    // Pre-spec major 1 is rejected
    {
        QByteArray data = createValidIccHeader(1024);
        data[8] = 0x01;
        const auto [status, summary] = validateIccHeader(data, 1024);
        QCOMPARE(status, ProfileValidationStatus::InvalidVersion);
        QVERIFY(!summary.valid);
    }

    // Unknown future major 6 is rejected
    {
        QByteArray data = createValidIccHeader(1024);
        data[8] = 0x06;
        const auto [status, summary] = validateIccHeader(data, 1024);
        QCOMPARE(status, ProfileValidationStatus::InvalidVersion);
        QVERIFY(!summary.valid);
    }

    // Hostile all-ones version is rejected
    {
        QByteArray data = createValidIccHeader(1024);
        data[8] = char(0xFF);
        data[9] = char(0xFF);
        data[10] = char(0xFF);
        data[11] = char(0xFF);
        const auto [status, summary] = validateIccHeader(data, 1024);
        QCOMPARE(status, ProfileValidationStatus::InvalidVersion);
        QVERIFY(!summary.valid);
    }
}

void ColorHeaderValidationTest::testHeaderLargerThanTotalFileSize()
{
    // A header buffer larger than the profile it claims to come from is
    // inconsistent hostile input, not a truncation. The declared size stays
    // consistent here (1024 within a 2048-byte file) so this row isolates the
    // supplied-buffer bound from the declared-size bound.
    QByteArray data = createValidIccHeader(1024);
    data.append(QByteArray(4096, '\0'));
    const auto [status, summary] = validateIccHeader(data, 2048);
    QCOMPARE(status, ProfileValidationStatus::InvalidSize);
    QVERIFY(!summary.valid);
}

void ColorHeaderValidationTest::testHeaderBufferExceedsDeclaredSize()
{
    // AGENT-GUARD: the supplied buffer must never exceed the profile's own
    // declared size. Here the declared size (128) fits the total file size
    // (512), and the 256-byte buffer fits the file too, so only the
    // buffer-versus-declared bound rejects this inconsistent input.
    QByteArray data = createValidIccHeader(128);
    data.append(QByteArray(128, '\0'));
    const auto [status, summary] = validateIccHeader(data, 512);
    QCOMPARE(status, ProfileValidationStatus::InvalidSize);
    QVERIFY(!summary.valid);
}

void ColorHeaderValidationTest::testHeaderSummaryExtraction()
{
    const QByteArray data = createValidIccHeader(4096, "mntr", "RGB ", "XYZ ");
    const auto [status, summary] = validateIccHeader(data, 4096);
    QCOMPARE(status, ProfileValidationStatus::Valid);
    QVERIFY(summary.valid);
    QCOMPARE(summary.profileSize, 4096u);
    QCOMPARE(summary.version, 0x02400000u);
    QCOMPARE(summary.cmmType, 0x4150504Cu);
    QCOMPARE(summary.profileIdMd5.size(), 16);
}

QTEST_MAIN(ColorHeaderValidationTest)
#include "tst_color_header_validation.moc"
