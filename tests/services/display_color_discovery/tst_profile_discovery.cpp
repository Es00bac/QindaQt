// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/display_color_discovery/profile_discovery.h>
#include <qindaqt/services/display_color_model/color_model.h>

#include "icc_text_metadata_p.h"
#include "support/icc_file_builder.h"

#include <QtCore/QTemporaryDir>
#include <QtTest>

using namespace QindaQt::DisplayColor;
using namespace QindaQt::DisplayColor::Testing;

namespace
{

DiscoveryRoot systemRoot(const QString &path)
{
    return DiscoveryRoot{path, DiscoveryOrigin::System};
}

DiscoveryRoot userRoot(const QString &path)
{
    return DiscoveryRoot{path, DiscoveryOrigin::UserImported};
}

// Relative template keeps every test file under the test's working
// directory (the assigned build root); the process never touches /tmp.
QTemporaryDir makeTree()
{
    return QTemporaryDir(QStringLiteral("display-color-discovery-XXXXXX"));
}

// The one diagnostic code recorded for a path, or an empty string when the
// scan produced none for it.
QStringList codesFor(const DiscoveryResult &result, const QString &path)
{
    QStringList codes;
    for (const DiscoveryDiagnostic &d : result.diagnostics) {
        if (d.path == path) {
            codes.append(d.code);
        }
    }
    return codes;
}

} // namespace

class ProfileDiscoveryTests final : public QObject
{
    Q_OBJECT

private slots:
    void discoversOnlyFromInjectedRoots();
    void rejectsSymlinkedRootAncestors();
    void rejectsParentReferencesBeforeCanonicalization();
    void rejectsInvalidInjectedOrigins();
    void invalidOriginsStayInvalidDuringDescriptorAssembly();
    void classifiesOriginFromTheInjectedRoot();
    void skipsHostileFilesWithDiagnostics();
    void rejectsConflictingDuplicateIdsOrderIndependently();
    void collapsesExactDuplicatesDeterministically();
    void rejectsBodyOnlyDuplicateConflicts();
    void boundsEnumerationTruthfully();
    void parsesDescAndMlucDescriptions();
    void acceptsExtensionsCaseInsensitively();
    void ignoresDotPrefixedNames();
    void degradesBoundedTagsWithDiagnostics();
    void unprovenSemanticsCanNeverBecomeSrgbDefault();

private:
    QTemporaryDir m_tree = makeTree();
};

void ProfileDiscoveryTests::discoversOnlyFromInjectedRoots()
{
    QVERIFY(m_tree.isValid());
    const QString system = m_tree.filePath(QStringLiteral("system"));
    const QString outside = m_tree.filePath(QStringLiteral("outside"));
    QVERIFY(QDir().mkpath(system) && QDir().mkpath(outside));
    QVERIFY(writeFileBytes(system + QStringLiteral("/srgb.icc"),
                           buildIccFileBytes(1024, QStringLiteral("Test sRGB profile"))));
    QVERIFY(writeFileBytes(outside + QStringLiteral("/secret.icc"),
                           buildIccFileBytes(512, QStringLiteral("Not injected"))));

    ProfileDiscovery discovery({systemRoot(system)});
    const DiscoveryResult result = discovery.discoverCatalog();
    QVERIFY(result.complete);
    QCOMPARE(result.profiles.size(), 1);
    QCOMPARE(result.profiles.first().descriptor.profileId, QStringLiteral("srgb"));
    QCOMPARE(result.profiles.first().descriptor.displayName, QStringLiteral("Test sRGB profile"));
    QCOMPARE(result.profiles.first().descriptor.byteSize, quint32{1024});
    // A directory that was never injected must stay invisible even though it
    // exists on the same machine.
    QVERIFY(!result.profiles.first().sourcePath.contains(QStringLiteral("secret")));
}

void ProfileDiscoveryTests::rejectsSymlinkedRootAncestors()
{
    QVERIFY(m_tree.isValid());
    const QString injected = m_tree.filePath(QStringLiteral("ancestor-injected"));
    const QString outside = m_tree.filePath(QStringLiteral("ancestor-outside"));
    const QString outsideIcc = outside + QStringLiteral("/icc");
    QVERIFY(QDir().mkpath(injected) && QDir().mkpath(outsideIcc));
    QVERIFY(writeFileBytes(outsideIcc + QStringLiteral("/outside.icc"),
                           buildIccFileBytes(512, QStringLiteral("Outside"))));
    QVERIFY(QFile::link(QFileInfo(outside).absoluteFilePath(),
                        injected + QStringLiteral("/redirect")));

    // AGENT-NOTE: P1.1 rejected a scan that checked only the final root and
    // followed an ancestor symlink outside the injected directory boundary.
    const QString redirectedRoot = injected + QStringLiteral("/redirect/icc");
    const DiscoveryResult result =
        ProfileDiscovery({systemRoot(redirectedRoot)}).discoverCatalog();
    QVERIFY(result.profiles.isEmpty());
    QVERIFY(codesFor(result, redirectedRoot)
                .contains(QStringLiteral("root-ancestor-is-symlink")));
}

void ProfileDiscoveryTests::rejectsParentReferencesBeforeCanonicalization()
{
    QVERIFY(m_tree.isValid());
    const QString injected = m_tree.filePath(QStringLiteral("dotdot-injected"));
    const QString outside = m_tree.filePath(QStringLiteral("dotdot-outside"));
    QVERIFY(QDir().mkpath(injected));
    QVERIFY(QDir().mkpath(outside + QStringLiteral("/bridge")));
    QVERIFY(QDir().mkpath(outside + QStringLiteral("/icc")));
    QVERIFY(writeFileBytes(outside + QStringLiteral("/icc/outside.icc"),
                           buildIccFileBytes(512, QStringLiteral("Outside"))));
    QVERIFY(QFile::link(QFileInfo(outside + QStringLiteral("/bridge")).absoluteFilePath(),
                        injected + QStringLiteral("/redirect")));

    // P1.1: lexical cleaning used to erase redirect/.., then POSIX path
    // resolution followed redirect and enumerated outside/icc.
    const QString root = injected + QStringLiteral("/redirect/../icc");
    const DiscoveryResult result = ProfileDiscovery({systemRoot(root)}).discoverCatalog();
    QVERIFY(result.profiles.isEmpty());
    QVERIFY(codesFor(result, root).contains(QStringLiteral("root-ancestor-is-symlink")));
}

void ProfileDiscoveryTests::rejectsInvalidInjectedOrigins()
{
    QVERIFY(m_tree.isValid());
    const QString root = m_tree.filePath(QStringLiteral("invalid-origin"));
    QVERIFY(QDir().mkpath(root));
    QVERIFY(writeFileBytes(root + QStringLiteral("/profile.icc"),
                           buildIccFileBytes(512, QStringLiteral("Profile"))));

    // AGENT-NOTE: P2.1 showed an out-of-range public origin silently acquiring
    // BuiltIn provenance through the descriptor's default initializer.
    const DiscoveryRoot invalid{root, static_cast<DiscoveryOrigin>(99)};
    const DiscoveryResult result = ProfileDiscovery({invalid}).discoverCatalog();
    QVERIFY(result.profiles.isEmpty());
    QVERIFY(!result.complete);
    QVERIFY(codesFor(result, root).contains(QStringLiteral("invalid-origin")));
}

void ProfileDiscoveryTests::invalidOriginsStayInvalidDuringDescriptorAssembly()
{
    const QByteArray bytes = buildIccFileBytes(512, QStringLiteral("Invalid origin"));
    const IccProfileDescriptor descriptor = assembleDescriptor(
        static_cast<DiscoveryOrigin>(99), QStringLiteral("invalid.icc"),
        bytes.left(static_cast<qsizetype>(IccHeaderSizeBytes)), quint32{512}, {}, {});

    // P3.1: descriptor assembly is a private defense-in-depth boundary. Its
    // invalid marker must survive all common-field initialization below the
    // origin switch.
    QVERIFY(!descriptor.wireValid);
    QCOMPARE(validateProfileDescriptor(descriptor), ProfileValidationStatus::MalformedMetadata);
}

void ProfileDiscoveryTests::classifiesOriginFromTheInjectedRoot()
{
    QVERIFY(m_tree.isValid());
    const QString system = m_tree.filePath(QStringLiteral("sys2"));
    const QString imported = m_tree.filePath(QStringLiteral("usr2"));
    QVERIFY(QDir().mkpath(system) && QDir().mkpath(imported));
    QVERIFY(writeFileBytes(system + QStringLiteral("/a-profile.icc"),
                           buildIccFileBytes(640, QStringLiteral("A"))));
    QVERIFY(writeFileBytes(imported + QStringLiteral("/z-profile.icm"),
                           buildIccFileBytes(640, QStringLiteral("Z"))));

    ProfileDiscovery discovery({userRoot(imported), systemRoot(system)});
    const DiscoveryResult result = discovery.discoverCatalog();
    QCOMPARE(result.profiles.size(), 2);
    // C0 order: origin BuiltIn < System < UserImported, then name, then ID.
    QCOMPARE(result.profiles.first().descriptor.origin, ProfileOrigin::System);
    QCOMPARE(result.profiles.last().descriptor.origin, ProfileOrigin::UserImported);
    // The .icm extension is equally accepted.
    QCOMPARE(result.profiles.last().descriptor.fileName, QStringLiteral("z-profile.icm"));
}

void ProfileDiscoveryTests::skipsHostileFilesWithDiagnostics()
{
    QVERIFY(m_tree.isValid());
    const QString root = m_tree.filePath(QStringLiteral("hostile"));
    QVERIFY(QDir().mkpath(root));

    const QByteArray valid = buildIccFileBytes(1024, QStringLiteral("Valid profile"));
    QVERIFY(writeFileBytes(root + QStringLiteral("/valid.icc"), valid));
    // Empty and truncated files.
    QVERIFY(writeFileBytes(root + QStringLiteral("/empty.icc"), QByteArray()));
    QVERIFY(writeFileBytes(root + QStringLiteral("/truncated.icc"), valid.left(64)));
    // Garbage bytes with an .icc suffix (mislabeled file).
    QVERIFY(writeFileBytes(root + QStringLiteral("/garbage.icc"),
                           QByteArray(256, '\xa5')));
    // Declared size smaller than the actual size: the header passes the C0
    // header check but the exact-equality contract rejects it.
    QByteArray trailing = buildIccFileBytes(512, QStringLiteral("Trailing bytes"));
    trailing.append(QByteArray(100, '\x00'));
    QVERIFY(writeFileBytes(root + QStringLiteral("/size-mismatch.icc"), trailing));
    // Declared size beyond the profile cap.
    QByteArray declaredHuge = buildIccFileBytes(512, QStringLiteral("Huge"));
    {
        uchar *base = reinterpret_cast<uchar *>(declaredHuge.data());
        const quint32 huge = qToBigEndian(MaxIccProfileSizeBytes + 1);
        std::memcpy(base, &huge, 4);
    }
    QVERIFY(writeFileBytes(root + QStringLiteral("/declared-huge.icc"), declaredHuge));
    // A planted symlink must never be followed. The target is made absolute
    // explicitly: the scan roots live under a relative temporary template,
    // so a plain relative target string would be resolved relative to the
    // link's own directory and dangle (QDir::Files hides dangling links).
    QVERIFY(QFile::link(QFileInfo(root + QStringLiteral("/valid.icc")).absoluteFilePath(),
                        root + QStringLiteral("/link.icc")));
    // A space in the file name breaks the C0 fileName safety grammar.
    QVERIFY(writeFileBytes(root + QStringLiteral("/my profile.icc"), valid));

    ProfileDiscovery discovery({systemRoot(root)}, DiscoveryLimits{});
    const DiscoveryResult result = discovery.discoverCatalog();
    QCOMPARE(result.profiles.size(), 1);
    QCOMPARE(result.profiles.first().descriptor.profileId, QStringLiteral("valid"));
    QVERIFY(!codesFor(result, root + QStringLiteral("/empty.icc")).isEmpty());
    QVERIFY(!codesFor(result, root + QStringLiteral("/truncated.icc")).isEmpty());
    QVERIFY(!codesFor(result, root + QStringLiteral("/garbage.icc")).isEmpty());
    QVERIFY(!codesFor(result, root + QStringLiteral("/size-mismatch.icc")).isEmpty());
    QVERIFY(!codesFor(result, root + QStringLiteral("/declared-huge.icc")).isEmpty());
    QVERIFY(!codesFor(result, root + QStringLiteral("/link.icc")).isEmpty());
    QVERIFY(!codesFor(result, root + QStringLiteral("/my profile.icc")).isEmpty());
    for (const DiscoveryDiagnostic &d : result.diagnostics) {
        QCOMPARE(d.severity, DiscoverySeverity::Warning);
        QVERIFY(d.code.size() <= 512);
    }
}

void ProfileDiscoveryTests::rejectsConflictingDuplicateIdsOrderIndependently()
{
    QVERIFY(m_tree.isValid());
    const QString left = m_tree.filePath(QStringLiteral("left"));
    const QString right = m_tree.filePath(QStringLiteral("right"));
    QVERIFY(QDir().mkpath(left) && QDir().mkpath(right));
    // Same stem in two System roots, different content: one deterministic
    // identifier, two conflicting descriptors.
    QVERIFY(writeFileBytes(left + QStringLiteral("/clash.icc"),
                           buildIccFileBytes(512, QStringLiteral("Left version"))));
    QVERIFY(writeFileBytes(right + QStringLiteral("/clash.icc"),
                           buildIccFileBytes(768, QStringLiteral("Right version"))));

    const DiscoveryResult forward =
        ProfileDiscovery({systemRoot(left), systemRoot(right)}).discoverCatalog();
    const DiscoveryResult reverse =
        ProfileDiscovery({systemRoot(right), systemRoot(left)}).discoverCatalog();
    // The catalog truth is order-independent: both scans drop the clashing
    // identifier completely. Diagnostics record which scanned path collided
    // and therefore follow scan order.
    QVERIFY(forward.profiles.isEmpty() && reverse.profiles.isEmpty());
    QVERIFY(forward.complete && reverse.complete);
    const auto hasConflict = [](const DiscoveryResult &result) {
        for (const DiscoveryDiagnostic &d : result.diagnostics) {
            if (d.code == QStringLiteral("conflicting-profile-id")) {
                return true;
            }
        }
        return false;
    };
    QVERIFY(hasConflict(forward) && hasConflict(reverse));
}

void ProfileDiscoveryTests::collapsesExactDuplicatesDeterministically()
{
    QVERIFY(m_tree.isValid());
    const QString left = m_tree.filePath(QStringLiteral("dup-left"));
    const QString right = m_tree.filePath(QStringLiteral("dup-right"));
    QVERIFY(QDir().mkpath(left) && QDir().mkpath(right));
    const QByteArray content = buildIccFileBytes(512, QStringLiteral("Same profile"));
    QVERIFY(writeFileBytes(left + QStringLiteral("/same.icc"), content));
    QVERIFY(writeFileBytes(right + QStringLiteral("/same.icc"), content));

    const DiscoveryResult result =
        ProfileDiscovery({systemRoot(left), systemRoot(right)}).discoverCatalog();
    QCOMPARE(result.profiles.size(), 1);
    QVERIFY(result.complete);
}

void ProfileDiscoveryTests::rejectsBodyOnlyDuplicateConflicts()
{
    QVERIFY(m_tree.isValid());
    const QString left = m_tree.filePath(QStringLiteral("body-dup-left"));
    const QString right = m_tree.filePath(QStringLiteral("body-dup-right"));
    QVERIFY(QDir().mkpath(left) && QDir().mkpath(right));
    QByteArray original = buildIccFileBytes(512, QStringLiteral("Same metadata"));
    QByteArray changed = original;
    changed[500] = '\x5a';
    QVERIFY(writeFileBytes(left + QStringLiteral("/same.icc"), original));
    QVERIFY(writeFileBytes(right + QStringLiteral("/same.icc"), changed));

    // AGENT-NOTE: P1.3 rejected descriptor-only duplicate comparison because
    // bytes outside the inspected metadata could differ under the same ID.
    const DiscoveryResult result =
        ProfileDiscovery({systemRoot(left), systemRoot(right)}).discoverCatalog();
    QVERIFY(result.profiles.isEmpty());
    bool sawConflict = false;
    for (const DiscoveryDiagnostic &diagnostic : result.diagnostics) {
        sawConflict = sawConflict || diagnostic.code == QStringLiteral("conflicting-profile-id");
    }
    QVERIFY(sawConflict);
}

void ProfileDiscoveryTests::boundsEnumerationTruthfully()
{
    QVERIFY(m_tree.isValid());
    const QString root = m_tree.filePath(QStringLiteral("bounded"));
    QVERIFY(QDir().mkpath(root));
    const QStringList names = {QStringLiteral("a.icc"), QStringLiteral("b.icc"),
                               QStringLiteral("c.icc")};
    for (const QString &name : names) {
        QVERIFY(writeFileBytes(root + QChar(u'/') + name,
                               buildIccFileBytes(512, QStringLiteral("Bounded"))));
    }

    DiscoveryLimits limits;
    limits.maxFilesPerRoot = 2;
    const DiscoveryResult result = ProfileDiscovery({systemRoot(root)}, limits).discoverCatalog();
    QCOMPARE(result.profiles.size(), 2);
    // The deterministic prefix after C0 ordering, not a random subset.
    QCOMPARE(result.profiles.first().descriptor.profileId, QStringLiteral("a"));
    QCOMPARE(result.profiles.last().descriptor.profileId, QStringLiteral("b"));
    QVERIFY(!result.complete);
    bool budgetDiagnostic = false;
    for (const DiscoveryDiagnostic &d : result.diagnostics) {
        budgetDiagnostic = budgetDiagnostic || d.code == QStringLiteral("root-file-budget-exceeded");
    }
    QVERIFY(budgetDiagnostic);
}

void ProfileDiscoveryTests::parsesDescAndMlucDescriptions()
{
    QVERIFY(m_tree.isValid());
    const QString root = m_tree.filePath(QStringLiteral("desc"));
    QVERIFY(QDir().mkpath(root));
    QVERIFY(writeFileBytes(root + QStringLiteral("/desc.icc"),
                           buildIccFileBytes(768, QStringLiteral("Wide Gamut RGB\nsecond line"))));
    QVERIFY(writeFileBytes(root + QStringLiteral("/mluc.icc"),
                           buildMlucIccFileBytes(768, QStringLiteral("ProfilLarge"))));
    // No description tag at all: the sanitized stem becomes the name.
    QVERIFY(writeFileBytes(root + QStringLiteral("/tagless.icc"),
                           buildIccFileBytes(768, QString())));

    const DiscoveryResult result = ProfileDiscovery({systemRoot(root)}).discoverCatalog();
    QCOMPARE(result.profiles.size(), 3);
    bool sawDesc = false;
    bool sawMluc = false;
    bool sawTagless = false;
    for (const DiscoveredProfile &profile : result.profiles) {
        if (profile.descriptor.profileId == QStringLiteral("desc")) {
            sawDesc = true;
            QCOMPARE(profile.descriptor.displayName, QStringLiteral("Wide Gamut RGB"));
            QCOMPARE(profile.descriptor.description,
                     QStringLiteral("Wide Gamut RGB\nsecond line"));
        }
        if (profile.descriptor.profileId == QStringLiteral("mluc")) {
            sawMluc = true;
            QCOMPARE(profile.descriptor.displayName, QStringLiteral("ProfilLarge"));
        }
        if (profile.descriptor.profileId == QStringLiteral("tagless")) {
            sawTagless = true;
            QCOMPARE(profile.descriptor.displayName, QStringLiteral("tagless"));
            QVERIFY(profile.descriptor.description.isEmpty());
        }
    }
    QVERIFY(sawDesc && sawMluc && sawTagless);
}

void ProfileDiscoveryTests::acceptsExtensionsCaseInsensitively()
{
    QVERIFY(m_tree.isValid());
    const QString root = m_tree.filePath(QStringLiteral("case"));
    QVERIFY(QDir().mkpath(root));
    QVERIFY(writeFileBytes(root + QStringLiteral("/UPPER.ICC"),
                           buildIccFileBytes(512, QStringLiteral("Upper"))));
    QVERIFY(writeFileBytes(root + QStringLiteral("/Mixed.Icm"),
                           buildIccFileBytes(512, QStringLiteral("Mixed"))));

    const DiscoveryResult result = ProfileDiscovery({systemRoot(root)}).discoverCatalog();
    QCOMPARE(result.profiles.size(), 2);
    QVERIFY(result.complete);
}

void ProfileDiscoveryTests::ignoresDotPrefixedNames()
{
    QVERIFY(m_tree.isValid());
    const QString root = m_tree.filePath(QStringLiteral("dot"));
    QVERIFY(QDir().mkpath(root));
    QVERIFY(writeFileBytes(root + QStringLiteral("/.hidden.icc"),
                           buildIccFileBytes(512, QStringLiteral("Hidden"))));
    QVERIFY(writeFileBytes(root + QStringLiteral("/visible.icc"),
                           buildIccFileBytes(512, QStringLiteral("Visible"))));

    const DiscoveryResult result = ProfileDiscovery({systemRoot(root)}).discoverCatalog();
    QCOMPARE(result.profiles.size(), 1);
    QCOMPARE(result.profiles.first().descriptor.profileId, QStringLiteral("visible"));
    QVERIFY(result.complete);
    // Dot-prefixed names are ignored silently: no profile and no diagnostic,
    // which is also what makes a stale import temporary invisible.
    QVERIFY(codesFor(result, root + QStringLiteral("/.hidden.icc")).isEmpty());
}

void ProfileDiscoveryTests::degradesBoundedTagsWithDiagnostics()
{
    QVERIFY(m_tree.isValid());
    const QString root = m_tree.filePath(QStringLiteral("tags"));
    QVERIFY(QDir().mkpath(root));
    // A declared tag-table count beyond the scan bound: the profile is still
    // cataloged over the truncated prefix, with a diagnostic (ADR-0066).
    QByteArray manyTags = buildIccFileBytes(1024, QStringLiteral("Many tags profile"));
    {
        uchar *base = reinterpret_cast<uchar *>(manyTags.data());
        const quint32 tagCountBe = qToBigEndian(quint32{5000});
        std::memcpy(base + IccHeaderSizeBytes, &tagCountBe, 4);
    }
    QVERIFY(writeFileBytes(root + QStringLiteral("/many-tags.icc"), manyTags));
    // A description tag larger than the byte bound: skipped, stem fallback.
    QVERIFY(writeFileBytes(root + QStringLiteral("/oversized-desc.icc"),
                           buildIccFileBytes(
                               1024, QStringLiteral("A description far beyond the byte budget"))));

    DiscoveryLimits limits;
    limits.maxTagTableEntries = 4;
    limits.maxDescriptionTagBytes = 32;
    const DiscoveryResult result = ProfileDiscovery({systemRoot(root)}, limits).discoverCatalog();
    QCOMPARE(result.profiles.size(), 2);
    QVERIFY(result.complete);
    bool sawTruncatedTable = false;
    bool sawOversizedTag = false;
    for (const DiscoveredProfile &profile : result.profiles) {
        const QStringList codes = codesFor(result, profile.sourcePath);
        if (profile.descriptor.profileId == QStringLiteral("many-tags")) {
            QCOMPARE(profile.descriptor.displayName, QStringLiteral("Many tags profile"));
            sawTruncatedTable = codes.contains(QStringLiteral("tag-table-truncated"));
        }
        if (profile.descriptor.profileId == QStringLiteral("oversized-desc")) {
            QCOMPARE(profile.descriptor.displayName, QStringLiteral("oversized-desc"));
            sawOversizedTag = codes.contains(QStringLiteral("description-tag-oversized"));
        }
    }
    QVERIFY(sawTruncatedTable && sawOversizedTag);
}

void ProfileDiscoveryTests::unprovenSemanticsCanNeverBecomeSrgbDefault()
{
    QVERIFY(m_tree.isValid());
    const QString root = m_tree.filePath(QStringLiteral("semantics"));
    QVERIFY(QDir().mkpath(root));
    QVERIFY(writeFileBytes(root + QStringLiteral("/somebody.icc"),
                           buildIccFileBytes(640, QStringLiteral("Some Display Profile"))));

    const DiscoveryResult result = ProfileDiscovery({systemRoot(root)}).discoverCatalog();
    QCOMPARE(result.profiles.size(), 1);
    const IccProfileDescriptor descriptor = result.profiles.first().descriptor;
    // Discovery never proves color semantics; the placeholders must make the
    // profile ineligible as the truthful sRGB default (AGENT-GUARD pair).
    QCOMPARE(descriptor.gamut, ColorSpaceGamut::Custom);
    QCOMPARE(descriptor.checksumSha256, QByteArray());

    ColorModel model(QStringLiteral("discovery-epoch"));
    QVERIFY(model.setCatalog({descriptor}));
    QVERIFY(model.snapshot().catalog.defaultSrgbProfileId.isEmpty());
    QVERIFY(model.snapshot().catalog.profiles.size() == 1);
}

QTEST_MAIN(ProfileDiscoveryTests)
#include "tst_profile_discovery.moc"
