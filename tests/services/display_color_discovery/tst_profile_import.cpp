// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/display_color_discovery/profile_discovery.h>

#include "support/icc_file_builder.h"

#include <QtCore/QCryptographicHash>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>
#include <QtTest>

using namespace QindaQt::DisplayColor;
using namespace QindaQt::DisplayColor::Testing;

namespace
{

constexpr auto kTemporarySuffix = ".qindaqt-import-tmp";

// Relative templates keep every test file under the test's working
// directory (the assigned build root); the process never touches /tmp.
QTemporaryDir makeTree(const QString &prefix)
{
    return QTemporaryDir(QStringLiteral("display-color-import-") + prefix + QStringLiteral("-XXXXXX"));
}

QList<DiscoveryRoot> rootsWithImport(const QString &importRoot)
{
    return {DiscoveryRoot{importRoot, DiscoveryOrigin::UserImported}};
}

QByteArray sha256(const QByteArray &bytes)
{
    return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256);
}

qsizetype countFiles(const QString &directory)
{
    return QDir(directory).entryList(QDir::Files | QDir::Hidden).size();
}

} // namespace

class ProfileImportTests final : public QObject
{
    Q_OBJECT

private slots:
    void importsValidatedProfileAtomically();
    void reimportOfIdenticalContentIsAlreadyPresent();
    void rejectsHostileSourcesWithoutMutatingUserRoot();
    void rejectsDestinationConflicts();
    void recoversFromInterruptedWrite();
    void failsClosedWithoutAUsableUserRoot();
    void importedProfileReappearsInDiscovery();

private:
    QTemporaryDir m_tree = makeTree(QStringLiteral("tree"));
};

void ProfileImportTests::importsValidatedProfileAtomically()
{
    QVERIFY(m_tree.isValid());
    const QString user = m_tree.filePath(QStringLiteral("import-user"));
    const QString sources = m_tree.filePath(QStringLiteral("sources"));
    QVERIFY(QDir().mkpath(user) && QDir().mkpath(sources));
    const QByteArray content = buildIccFileBytes(800, QStringLiteral("Imported profile"));
    const QString source = sources + QStringLiteral("/goodname.icc");
    QVERIFY(writeFileBytes(source, content));

    const ImportResult result = ProfileDiscovery(rootsWithImport(user)).importUserProfile(source);
    QCOMPARE(result.status, ImportStatus::Imported);
    QVERIFY(result.imported());
    QCOMPARE(result.profile.descriptor.profileId, QStringLiteral("goodname"));
    QCOMPARE(result.profile.descriptor.origin, ProfileOrigin::UserImported);
    QCOMPARE(result.profile.descriptor.checksumSha256, sha256(content));
    QCOMPARE(result.profile.descriptor.byteSize, quint32{800});
    // The stored copy is byte-identical and carries no leftover temporary.
    QFile stored(user + QStringLiteral("/goodname.icc"));
    QVERIFY(stored.open(QIODevice::ReadOnly));
    QCOMPARE(stored.readAll(), content);
    stored.close();
    QCOMPARE(countFiles(user), qsizetype{1});
}

void ProfileImportTests::reimportOfIdenticalContentIsAlreadyPresent()
{
    QVERIFY(m_tree.isValid());
    const QString user = m_tree.filePath(QStringLiteral("reimport"));
    const QString sources = m_tree.filePath(QStringLiteral("reimport-src"));
    QVERIFY(QDir().mkpath(user) && QDir().mkpath(sources));
    const QByteArray content = buildIccFileBytes(800, QStringLiteral("Stable"));
    const QString source = sources + QStringLiteral("/stable.icc");
    QVERIFY(writeFileBytes(source, content));

    ProfileDiscovery discovery(rootsWithImport(user));
    QVERIFY(discovery.importUserProfile(source).imported());
    const ImportResult again = discovery.importUserProfile(source);
    QCOMPARE(again.status, ImportStatus::AlreadyPresent);
    QCOMPARE(again.profile.descriptor.checksumSha256, sha256(content));
    QCOMPARE(countFiles(user), qsizetype{1});
}

void ProfileImportTests::rejectsHostileSourcesWithoutMutatingUserRoot()
{
    QVERIFY(m_tree.isValid());
    const QString user = m_tree.filePath(QStringLiteral("reject-user"));
    const QString sources = m_tree.filePath(QStringLiteral("reject-src"));
    QVERIFY(QDir().mkpath(user) && QDir().mkpath(sources));

    const QByteArray valid = buildIccFileBytes(640, QStringLiteral("Valid"));
    // Truncated relative to its own declared size.
    QVERIFY(writeFileBytes(sources + QStringLiteral("/truncated.icc"), valid.left(300)));
    // Header declares more than the hard cap.
    QByteArray declaredHuge = buildIccFileBytes(512, QStringLiteral("Huge"));
    {
        uchar *base = reinterpret_cast<uchar *>(declaredHuge.data());
        const quint32 huge = qToBigEndian(MaxIccProfileSizeBytes + 1);
        std::memcpy(base, &huge, 4);
    }
    QVERIFY(writeFileBytes(sources + QStringLiteral("/declared-huge.icc"), declaredHuge));
    // Garbage with an .icc suffix.
    QVERIFY(writeFileBytes(sources + QStringLiteral("/garbage.icc"), QByteArray(200, '\x3c')));
    // A symlink source is never followed. linked.icc targets an existing
    // file through an absolute path (the temporary tree uses a relative
    // template); dangling.icc points nowhere.
    QVERIFY(QFile::link(sources + QStringLiteral("/missing-target.icc"),
                        sources + QStringLiteral("/dangling.icc")));
    QVERIFY(QFile::link(QFileInfo(sources + QStringLiteral("/truncated.icc")).absoluteFilePath(),
                        sources + QStringLiteral("/linked.icc")));
    // A space breaks the destination name grammar.
    QVERIFY(writeFileBytes(sources + QStringLiteral("/bad name.icc"), valid));
    // Missing file.
    const QString missing = sources + QStringLiteral("/absent.icc");

    ProfileDiscovery discovery(rootsWithImport(user));
    const ImportResult truncated =
        discovery.importUserProfile(sources + QStringLiteral("/truncated.icc"));
    QCOMPARE(truncated.status, ImportStatus::InvalidSource);
    const ImportResult huge =
        discovery.importUserProfile(sources + QStringLiteral("/declared-huge.icc"));
    QVERIFY(huge.status == ImportStatus::InvalidSource ||
            huge.status == ImportStatus::SourceOversized);
    const ImportResult garbage =
        discovery.importUserProfile(sources + QStringLiteral("/garbage.icc"));
    QCOMPARE(garbage.status, ImportStatus::InvalidSource);
    const ImportResult dangling = discovery.importUserProfile(
        sources + QStringLiteral("/dangling.icc"));
    QVERIFY(dangling.status == ImportStatus::SourceIsSymlink ||
            dangling.status == ImportStatus::SourceUnreadable);
    const ImportResult linked =
        discovery.importUserProfile(sources + QStringLiteral("/linked.icc"));
    QCOMPARE(linked.status, ImportStatus::SourceIsSymlink);
    QCOMPARE(discovery.importUserProfile(sources + QStringLiteral("/bad name.icc")).status,
             ImportStatus::SourceNameUnsafe);
    QCOMPARE(discovery.importUserProfile(missing).status, ImportStatus::SourceUnreadable);

    // Rejection is atomic: the user root is still empty.
    QCOMPARE(countFiles(user), qsizetype{0});
}

void ProfileImportTests::rejectsDestinationConflicts()
{
    QVERIFY(m_tree.isValid());
    const QString user = m_tree.filePath(QStringLiteral("conflict-user"));
    const QString sources = m_tree.filePath(QStringLiteral("conflict-src"));
    QVERIFY(QDir().mkpath(user) && QDir().mkpath(sources));
    const QString source = sources + QStringLiteral("/clash.icc");
    QVERIFY(writeFileBytes(source, buildIccFileBytes(640, QStringLiteral("Clash source"))));
    const QByteArray otherContent = buildIccFileBytes(700, QStringLiteral("Different content"));
    QVERIFY(writeFileBytes(user + QStringLiteral("/clash.icc"), otherContent));

    const ImportResult result = ProfileDiscovery(rootsWithImport(user)).importUserProfile(source);
    QCOMPARE(result.status, ImportStatus::DestinationConflict);
    // The pre-existing file is untouched.
    QFile existing(user + QStringLiteral("/clash.icc"));
    QVERIFY(existing.open(QIODevice::ReadOnly));
    QCOMPARE(existing.readAll(), otherContent);
    existing.close();
}

void ProfileImportTests::recoversFromInterruptedWrite()
{
    QVERIFY(m_tree.isValid());
    const QString user = m_tree.filePath(QStringLiteral("recover-user"));
    const QString sources = m_tree.filePath(QStringLiteral("recover-src"));
    QVERIFY(QDir().mkpath(user) && QDir().mkpath(sources));
    const QString source = sources + QStringLiteral("/resumable.icc");
    QVERIFY(writeFileBytes(source, buildIccFileBytes(640, QStringLiteral("Resumable"))));

    // A stale temporary from an interrupted previous import is removed by
    // the next import and never becomes catalog truth.
    QVERIFY(writeFileBytes(user + QStringLiteral("/.resumable.icc") + QLatin1String(kTemporarySuffix),
                           QByteArray(64, '\x11')));
    const ImportResult result = ProfileDiscovery(rootsWithImport(user)).importUserProfile(source);
    QCOMPARE(result.status, ImportStatus::Imported);
    QVERIFY(!QFile::exists(user + QStringLiteral("/.resumable.icc") + QLatin1String(kTemporarySuffix)));
    QFile stored(user + QStringLiteral("/resumable.icc"));
    QVERIFY(stored.open(QIODevice::ReadOnly));
    QCOMPARE(stored.readAll(), buildIccFileBytes(640, QStringLiteral("Resumable")));
    stored.close();

    // A directory parked at the temporary name is a hostile collision: the
    // import fails closed instead of removing or writing through it.
    const QString other = sources + QStringLiteral("/blocked.icc");
    QVERIFY(writeFileBytes(other, buildIccFileBytes(640, QStringLiteral("Blocked"))));
    QVERIFY(QDir(user).mkpath(QStringLiteral(".blocked.icc") + QLatin1String(kTemporarySuffix)));
    const ImportResult blocked = ProfileDiscovery(rootsWithImport(user)).importUserProfile(other);
    QCOMPARE(blocked.status, ImportStatus::WriteFailed);
    QCOMPARE(blocked.reasonCode, QStringLiteral("write-failed"));
    QVERIFY(!QFile::exists(user + QStringLiteral("/blocked.icc")));
}

void ProfileImportTests::failsClosedWithoutAUsableUserRoot()
{
    QVERIFY(m_tree.isValid());
    const QString sources = m_tree.filePath(QStringLiteral("rootless-src"));
    QVERIFY(QDir().mkpath(sources));
    const QString source = sources + QStringLiteral("/x.icc");
    QVERIFY(writeFileBytes(source, buildIccFileBytes(512, QStringLiteral("X"))));

    // No injected user root at all.
    const DiscoveryRoot systemOnly{m_tree.filePath(QStringLiteral("nowhere")),
                                   DiscoveryOrigin::System};
    const ImportResult noRoot = ProfileDiscovery({systemOnly}).importUserProfile(source);
    QCOMPARE(noRoot.status, ImportStatus::InvalidUserRoot);
    QCOMPARE(noRoot.reasonCode, QStringLiteral("no-user-root"));

    // A group-writable user root fails the ADR-0051 root validation.
    const QString loose = m_tree.filePath(QStringLiteral("loose-user"));
    QVERIFY(QDir().mkpath(loose));
    QVERIFY(QFile::setPermissions(loose, QFile::Permissions(QFile::ReadOwner | QFile::WriteOwner |
                                                            QFile::ExeOwner | QFile::ReadGroup |
                                                            QFile::WriteGroup | QFile::ExeGroup)));
    const ImportResult unsafe = ProfileDiscovery(rootsWithImport(loose)).importUserProfile(source);
    QCOMPARE(unsafe.status, ImportStatus::WriteFailed);
    QCOMPARE(unsafe.reasonCode, QStringLiteral("invalid-user-root"));
}

void ProfileImportTests::importedProfileReappearsInDiscovery()
{
    QVERIFY(m_tree.isValid());
    const QString user = m_tree.filePath(QStringLiteral("roundtrip"));
    const QString sources = m_tree.filePath(QStringLiteral("roundtrip-src"));
    QVERIFY(QDir().mkpath(user) && QDir().mkpath(sources));
    const QString source = sources + QStringLiteral("/roundtrip.icc");
    QVERIFY(writeFileBytes(source, buildIccFileBytes(640, QStringLiteral("Round Trip"))));

    ProfileDiscovery discovery(rootsWithImport(user));
    const ImportResult imported = discovery.importUserProfile(source);
    QVERIFY(imported.imported());

    const DiscoveryResult scanned = discovery.discoverCatalog();
    QCOMPARE(scanned.profiles.size(), 1);
    QCOMPARE(scanned.profiles.first().descriptor.profileId,
             imported.profile.descriptor.profileId);
    QCOMPARE(scanned.profiles.first().descriptor.origin, ProfileOrigin::UserImported);
    // Discovery itself does not re-verify content digests (bounded reads);
    // the import result remains the provenance proof.
    QVERIFY(scanned.profiles.first().descriptor.checksumSha256.isEmpty());
}

QTEST_MAIN(ProfileImportTests)
#include "tst_profile_import.moc"
