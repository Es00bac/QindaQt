// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest/QtTest>
#include <qindaqt/services/display_color_model/color_limits.h>
#include <qindaqt/services/display_color_model/color_validation.h>
#include "support/color_test_data.h"

using namespace QindaQt::DisplayColor;
using namespace QindaQt::DisplayColor::Testing;

class ColorCatalogTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testValidDescriptor();
    void testMalformedIdentifiers();
    void testMalformedDisplayNames();
    void testFilenamePathTraversalPrevention();
    void testInvalidChecksumLength();
    void testHostileEnumCastsRejected();
    void testRawHeaderLargerThanDeclaredSize();
    void testCatalogDeterministicSorting();
    void testCatalogDeduplication();
    void testCatalogCapacityCap();
};

void ColorCatalogTest::testValidDescriptor()
{
    const auto desc = createSampleProfile("srgb-builtin", "sRGB Built-in", ProfileOrigin::BuiltIn);
    QCOMPARE(validateProfileDescriptor(desc), ProfileValidationStatus::Valid);
}

void ColorCatalogTest::testMalformedIdentifiers()
{
    // Empty ID
    {
        auto desc = createSampleProfile("", "Empty ID Profile");
        QCOMPARE(validateProfileDescriptor(desc), ProfileValidationStatus::MalformedMetadata);
    }

    // Invalid characters in ID (e.g. spaces, special symbols)
    {
        auto desc = createSampleProfile("invalid id with spaces", "Invalid ID Profile");
        QCOMPARE(validateProfileDescriptor(desc), ProfileValidationStatus::MalformedMetadata);
    }

    // Oversized ID
    {
        auto desc = createSampleProfile(QString(MaxIdentifierLength + 1, 'x'), "Oversized ID");
        QCOMPARE(validateProfileDescriptor(desc), ProfileValidationStatus::MalformedMetadata);
    }
}

void ColorCatalogTest::testMalformedDisplayNames()
{
    // Empty display name
    {
        auto desc = createSampleProfile("prof-1", "");
        QCOMPARE(validateProfileDescriptor(desc), ProfileValidationStatus::MalformedMetadata);
    }

    // Whitespace-only display name
    {
        auto desc = createSampleProfile("prof-2", "   \t\n  ");
        QCOMPARE(validateProfileDescriptor(desc), ProfileValidationStatus::MalformedMetadata);
    }

    // Oversized display name
    {
        auto desc = createSampleProfile("prof-3", QString(MaxDisplayNameLength + 1, 'A'));
        QCOMPARE(validateProfileDescriptor(desc), ProfileValidationStatus::MalformedMetadata);
    }
}

void ColorCatalogTest::testFilenamePathTraversalPrevention()
{
    // Parent directory traversal
    {
        auto desc = createSampleProfile("prof-path-1", "Path Traversal");
        desc.fileName = "../evil.icc";
        QCOMPARE(validateProfileDescriptor(desc), ProfileValidationStatus::MalformedMetadata);
    }

    // Path separators
    {
        auto desc = createSampleProfile("prof-path-2", "Path Separator");
        desc.fileName = "etc/shadow.icc";
        QCOMPARE(validateProfileDescriptor(desc), ProfileValidationStatus::MalformedMetadata);
    }
}

void ColorCatalogTest::testInvalidChecksumLength()
{
    auto desc = createSampleProfile("prof-chk", "Bad Checksum");
    desc.checksumSha256 = QByteArray(16, 'x'); // 16 instead of 32
    QCOMPARE(validateProfileDescriptor(desc), ProfileValidationStatus::ChecksumMismatch);
}

void ColorCatalogTest::testHostileEnumCastsRejected()
{
    // AGENT-GUARD: Decoded storage can carry arbitrary enum casts; every
    // descriptor enum is range checked so unknown values never enter a catalog.
    {
        auto desc = createSampleProfile("prof-enum-1", "Hostile Origin");
        desc.origin = static_cast<ProfileOrigin>(99u);
        QCOMPARE(validateProfileDescriptor(desc), ProfileValidationStatus::MalformedMetadata);
    }
    {
        auto desc = createSampleProfile("prof-enum-2", "Hostile Gamut");
        desc.gamut = static_cast<ColorSpaceGamut>(0xDEADu);
        QCOMPARE(validateProfileDescriptor(desc), ProfileValidationStatus::MalformedMetadata);
    }
    {
        auto desc = createSampleProfile("prof-enum-3", "Hostile Transfer");
        desc.transferFunction = static_cast<TransferFunction>(0xBEEFu);
        QCOMPARE(validateProfileDescriptor(desc), ProfileValidationStatus::MalformedMetadata);
    }
}

void ColorCatalogTest::testRawHeaderLargerThanDeclaredSize()
{
    auto desc = createSampleProfile("prof-size", "Inconsistent Sizes", ProfileOrigin::BuiltIn,
                                    ColorSpaceGamut::Srgb, 256);
    // The declared header size stays consistent (128 within the 256-byte
    // profile) while the supplied buffer is far larger than the profile.
    desc.rawHeader = createValidIccHeader(128);
    desc.rawHeader.append(QByteArray(1024, '\0'));
    QCOMPARE(validateProfileDescriptor(desc), ProfileValidationStatus::InvalidSize);
}

void ColorCatalogTest::testCatalogDeterministicSorting()
{
    const auto pUserB = createSampleProfile("prof-user-b", "Beta Profile", ProfileOrigin::UserImported);
    const auto pUserA = createSampleProfile("prof-user-a", "Alpha Profile", ProfileOrigin::UserImported);
    const auto pSys = createSampleProfile("prof-sys", "System sRGB", ProfileOrigin::System);
    const auto pBuiltinZ = createSampleProfile("prof-built-z", "Zulu Builtin", ProfileOrigin::BuiltIn);
    const auto pBuiltinA = createSampleProfile("prof-built-a", "Alpha Builtin", ProfileOrigin::BuiltIn);

    const QList<IccProfileDescriptor> input = {pUserB, pBuiltinZ, pSys, pUserA, pBuiltinA};
    const auto sorted = normalizeAndSortCatalog(input);

    QCOMPARE(sorted.size(), 5);

    // 1. BuiltIns first, sorted by displayName: Alpha Builtin, then Zulu Builtin
    QCOMPARE(sorted[0].profileId, QString("prof-built-a"));
    QCOMPARE(sorted[1].profileId, QString("prof-built-z"));

    // 2. System next: System sRGB
    QCOMPARE(sorted[2].profileId, QString("prof-sys"));

    // 3. UserImported next, sorted by displayName: Alpha Profile, then Beta Profile
    QCOMPARE(sorted[3].profileId, QString("prof-user-a"));
    QCOMPARE(sorted[4].profileId, QString("prof-user-b"));
}

void ColorCatalogTest::testCatalogDeduplication()
{
    const auto p1 = createSampleProfile("prof-dup", "Original Profile", ProfileOrigin::BuiltIn);
    auto p2 = createSampleProfile("prof-dup", "Duplicate Profile", ProfileOrigin::UserImported);

    const QList<IccProfileDescriptor> input = {p1, p2};
    const auto result = normalizeAndSortCatalog(input);

    QCOMPARE(result.size(), 1);
    QCOMPARE(result.first().profileId, QString("prof-dup"));
    QCOMPARE(result.first().displayName, QString("Original Profile"));
}

void ColorCatalogTest::testCatalogCapacityCap()
{
    QList<IccProfileDescriptor> largeList;
    for (int i = 0; i < 300; ++i) {
        largeList.append(createSampleProfile(QString("prof-%1").arg(i, 4, 10, QChar('0')),
                                             QString("Profile %1").arg(i),
                                             ProfileOrigin::UserImported));
    }

    const auto result = normalizeAndSortCatalog(largeList);
    QCOMPARE(result.size(), static_cast<int>(MaxProfilesInCatalog));
}

QTEST_MAIN(ColorCatalogTest)
#include "tst_color_catalog.moc"
