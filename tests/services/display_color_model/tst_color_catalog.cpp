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
    void testDescriptorSizeConsistency();
    void testIdentifierAsciiGrammar();
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

void ColorCatalogTest::testDescriptorSizeConsistency()
{
    // AGENT-GUARD: descriptor byte size, the header's embedded declared
    // size, and the supplied buffer must agree exactly.

    // Equal: descriptor size, declared size and buffer are consistent.
    {
        auto desc = createSampleProfile("prof-size-eq", "Equal Sizes", ProfileOrigin::BuiltIn,
                                        ColorSpaceGamut::Srgb, 512);
        QCOMPARE(validateProfileDescriptor(desc), ProfileValidationStatus::Valid);
    }

    // Declared size smaller than descriptor byte size: the header validates
    // alone but the sizes disagree, so the descriptor must fail closed.
    {
        auto desc = createSampleProfile("prof-size-small", "Declared Smaller", ProfileOrigin::BuiltIn,
                                        ColorSpaceGamut::Srgb, 512);
        desc.rawHeader = createValidIccHeader(128);
        QCOMPARE(validateProfileDescriptor(desc), ProfileValidationStatus::InvalidSize);
    }

    // Declared size larger than descriptor byte size: rejected earlier by
    // the header's declared-versus-total-size bound.
    {
        auto desc = createSampleProfile("prof-size-large", "Declared Larger", ProfileOrigin::BuiltIn,
                                        ColorSpaceGamut::Srgb, 128);
        desc.rawHeader = createValidIccHeader(512);
        QCOMPARE(validateProfileDescriptor(desc), ProfileValidationStatus::InvalidDeclaredSize);
    }
}

void ColorCatalogTest::testIdentifierAsciiGrammar()
{
    // The documented grammar is exactly [A-Za-z0-9._:-].
    QVERIFY(validateDisplayStableId("DP-1._:aZ09"));
    QCOMPARE(validateProfileDescriptor(createSampleProfile("ok.id:1-A_Z", "Grammar OK")),
             ProfileValidationStatus::Valid);

    // AGENT-GUARD: non-ASCII letters are outside the durable grammar and
    // must fail closed for both stable IDs and profile IDs.
    QVERIFY(!validateDisplayStableId(QString::fromUtf8("écran")));
    QVERIFY(!validateDisplayStableId(QString::fromUtf8("屏幕-1")));
    {
        auto desc = createSampleProfile(QString::fromUtf8("profil-é"), "Unicode ID");
        QCOMPARE(validateProfileDescriptor(desc), ProfileValidationStatus::MalformedMetadata);
    }
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
    // Exact-equal duplicates collapse to a single entry in any input order.
    {
        const auto original = createSampleProfile("prof-dup", "Original Profile", ProfileOrigin::BuiltIn);
        const auto copy = original;

        const auto forward = normalizeAndSortCatalog({original, copy});
        const auto reverse = normalizeAndSortCatalog({copy, original});
        QCOMPARE(forward, reverse);
        QCOMPARE(forward.size(), 1);
        QCOMPARE(forward.first().profileId, QString("prof-dup"));
        QCOMPARE(forward.first().displayName, QString("Original Profile"));
    }

    // AGENT-GUARD: conflicting duplicates (same ID, different bytes) are
    // rejected atomically in both input orders — neither descriptor becomes
    // catalog truth, so publication never depends on input order.
    {
        const auto first = createSampleProfile("prof-conflict", "First Profile", ProfileOrigin::BuiltIn);
        const auto second = createSampleProfile("prof-conflict", "Second Profile", ProfileOrigin::UserImported);

        const auto forward = normalizeAndSortCatalog({first, second});
        const auto reverse = normalizeAndSortCatalog({second, first});
        QCOMPARE(forward, reverse);
        QVERIFY(forward.isEmpty());
    }

    // A conflicting pair never evicts an unrelated profile.
    {
        const auto conflictA = createSampleProfile("prof-conflict", "Alpha Version");
        const auto conflictB = createSampleProfile("prof-conflict", "Beta Version");
        const auto unrelated = createSampleProfile("prof-other", "Unrelated Profile");

        const auto result = normalizeAndSortCatalog({conflictA, unrelated, conflictB});
        QCOMPARE(result.size(), 1);
        QCOMPARE(result.first().profileId, QString("prof-other"));
    }
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
