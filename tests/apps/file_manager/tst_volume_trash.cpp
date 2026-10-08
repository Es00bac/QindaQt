// SPDX-License-Identifier: GPL-3.0-or-later
#include "trash_test_support.h"
#include <QTest>
#include <qscopeguard.h>
using namespace TrashTest;
class VolumeTrashTests final : public QObject {
  Q_OBJECT
private slots:
  void privateVolumeRoundTripAndRestart() {
    Fixture f; QVERIFY(f.admit()); QVERIFY(write(f.source(), "payload"));
    auto owner = f.owner();
    const auto saved = owner.trash(f.trash(f.source()), f.cancel);
    QVERIFY2(saved.ok(), qPrintable(saved.diagnostic));
    QCOMPARE(saved.trashReceipt.root, f.privateRoot());
    QVERIFY(!QFileInfo::exists(f.source()));
    QVERIFY(read(saved.trashReceipt.metadataPath).contains("Path=source/item\n"));
    QVERIFY(!read(saved.trashReceipt.metadataPath).contains("Path=/"));
    QCOMPARE(VolumeTrash::originalPathFor(saved.outputPath), f.source());
    auto restarted = f.owner();
    const auto restored = restarted.restore(f.restore(saved), f.cancel);
    QVERIFY2(restored.ok(), qPrintable(restored.diagnostic));
    QCOMPARE(read(f.source()), QByteArray("payload"));
    QVERIFY(!QFileInfo::exists(saved.outputPath));
    QVERIFY(restored.trashReceipt.restoredConfirmed);
    QVERIFY(QFileInfo::exists(saved.trashReceipt.metadataPath)); // No foreign cleanup authority.
  }
  void sharedStickyAndBothStoresDiscovered() {
    Fixture f; QVERIFY(f.admit());
    const auto shared = f.volume.filePath(".Trash");
    QVERIFY(QDir().mkdir(shared)); QVERIFY(::chmod(QFile::encodeName(shared).constData(), 01777) == 0);
    QVERIFY(write(f.source(), "first"));
    auto owner = f.owner(); const auto saved = owner.trash(f.trash(f.source()), f.cancel);
    QVERIFY(saved.ok()); QVERIFY(saved.trashReceipt.root.contains("/.Trash/"));
    MutationResult result;
    QVERIFY(TrashStorage::open({f.privateRoot(), f.volume.path(), false}, true, result));
    QCOMPARE(VolumeTrash::discover(f.volume.path()).size(), 2);
    QCOMPARE(read(saved.outputPath), QByteArray("first"));
  }
  void invalidSharedFallsBackWithoutTouchingTarget() {
    Fixture f; QVERIFY(f.admit());
    const auto outside = f.home.filePath("outside"); QVERIFY(privateDir(outside));
    QVERIFY(write(QDir(outside).filePath("sentinel"), "foreign"));
    QVERIFY(::symlink(QFile::encodeName(outside).constData(),
                       QFile::encodeName(f.volume.filePath(".Trash")).constData()) == 0);
    QVERIFY(write(f.source(), "payload"));
    const auto saved = f.owner().trash(f.trash(f.source()), f.cancel);
    QVERIFY(saved.ok()); QCOMPARE(saved.trashReceipt.root, f.privateRoot());
    QCOMPARE(read(QDir(outside).filePath("sentinel")), QByteArray("foreign"));
    QVERIFY(!QDir(outside).exists(QString::number(::getuid())));
  }
  void unsafePrivateStorageRefusesAndPreserves() {
    Fixture f; QVERIFY(f.admit()); QVERIFY(QDir().mkdir(f.privateRoot()));
    QVERIFY(::chmod(QFile::encodeName(f.privateRoot()).constData(), 0755) == 0);
    QVERIFY(write(f.source(), "payload"));
    const auto result = f.owner().trash(f.trash(f.source()), f.cancel);
    QCOMPARE(result.error, MutationError::PermissionDenied); QCOMPARE(read(f.source()), QByteArray("payload"));
    struct stat mode {}; QVERIFY(::lstat(QFile::encodeName(f.privateRoot()).constData(), &mode) == 0);
    QCOMPARE(mode.st_mode & 0777, mode_t(0755)); // No chmod repair.
  }
  void unwritableTopRefusesWithoutPermanentFallback() {
    Fixture f; QVERIFY(f.admit()); QVERIFY(write(f.source(), "payload"));
    const auto path = QFile::encodeName(f.volume.path());
    const auto reset = qScopeGuard([&] { ::chmod(path.constData(), 0700); });
    QVERIFY(::chmod(path.constData(), 0500) == 0);
    const auto result = f.owner().trash(f.trash(f.source()), f.cancel);
    QCOMPARE(result.error, MutationError::PermissionDenied);
    QCOMPARE(read(f.source()), QByteArray("payload")); QVERIFY(!QFileInfo::exists(f.homeRoot()));
  }
  void symlinkedInfoRefusesWithoutTargetWrites() {
    Fixture f; QVERIFY(f.admit()); MutationResult result;
    QVERIFY(TrashStorage::open({f.privateRoot(), f.volume.path(), false}, true, result));
    const auto info = QDir(f.privateRoot()).filePath("info");
    QVERIFY(QDir().rename(info, f.volume.filePath("saved-info")));
    const auto outside = f.home.filePath("outside"); QVERIFY(privateDir(outside));
    QVERIFY(write(QDir(outside).filePath("sentinel"), "foreign"));
    QVERIFY(::symlink(QFile::encodeName(outside).constData(), QFile::encodeName(info).constData()) == 0);
    QVERIFY(write(f.source(), "payload"));
    QCOMPARE(f.owner().trash(f.trash(f.source()), f.cancel).error, MutationError::SymlinkEscape);
    QCOMPARE(read(f.source()), QByteArray("payload")); QCOMPARE(read(QDir(outside).filePath("sentinel")), QByteArray("foreign"));
    QCOMPARE(QDir(outside).entryList(QDir::Files).size(), 1);
  }
  void orphanMetadataAndPayloadReserveDifferentNames() {
    Fixture f; QVERIFY(f.admit()); MutationResult result;
    auto store = TrashStorage::open({f.privateRoot(), f.volume.path(), false}, true, result); QVERIFY(store);
    QVERIFY(write(QDir(f.privateRoot()).filePath("info/item.trashinfo"), "foreign-record"));
    QVERIFY(write(QDir(f.privateRoot()).filePath("files/item.1"), "foreign-payload"));
    QVERIFY(write(f.source(), "source"));
    const auto saved = f.owner().trash(f.trash(f.source()), f.cancel); QVERIFY(saved.ok());
    QCOMPARE(saved.trashToken, QStringLiteral("item.2")); QCOMPARE(read(saved.outputPath), QByteArray("source"));
    QCOMPARE(read(QDir(f.privateRoot()).filePath("info/item.trashinfo")), QByteArray("foreign-record"));
    QCOMPARE(read(QDir(f.privateRoot()).filePath("files/item.1")), QByteArray("foreign-payload"));
  }
  void symbolicPayload_data() {
    QTest::addColumn<QByteArray>("target");
    QTest::newRow("external") << QByteArray("@external");
    QTest::newRow("broken") << QByteArray("missing");
    QTest::newRow("self-loop") << QByteArray("item");
  }
  void symbolicPayload() {
    QFETCH(QByteArray, target);
    Fixture f; QVERIFY(f.admit());
    const auto outside = f.home.filePath("outside"); QVERIFY(write(outside, "untouched"));
    if (target == "@external") target = QFile::encodeName(outside);
    QVERIFY(::symlink(target.constData(), QFile::encodeName(f.source()).constData()) == 0);
    const auto before = LocalMutationBackend::identityForPath(f.source()); QVERIFY(before);
    const auto saved = f.owner().trash(f.trash(f.source()), f.cancel);
    QVERIFY2(saved.ok(), qPrintable(saved.diagnostic)); QCOMPARE(saved.outputIdentity, before);
    char bytes[4096]; const auto size = ::readlink(QFile::encodeName(saved.outputPath).constData(), bytes, sizeof(bytes));
    QVERIFY(size >= 0); QCOMPARE(QByteArray(bytes, static_cast<qsizetype>(size)), target);
    QVERIFY(f.owner().restore(f.restore(saved), f.cancel).ok());
    QCOMPARE(LocalMutationBackend::identityForPath(f.source()), before);
    QCOMPARE(read(outside), QByteArray("untouched"));
  }
  void directoryAndRepeatedNamesPreserveEveryEntry() {
    Fixture f; QVERIFY(f.admit()); QVERIFY(privateDir(f.source()));
    QVERIFY(write(QDir(f.source()).filePath("child"), "child"));
    const auto first = f.owner().trash(f.trash(f.source()), f.cancel); QVERIFY(first.ok());
    QVERIFY(write(f.source(), "second"));
    const auto second = f.owner().trash(f.trash(f.source()), f.cancel); QVERIFY(second.ok());
    QVERIFY(first.trashToken != second.trashToken);
    QCOMPARE(read(QDir(first.outputPath).filePath("child")), QByteArray("child"));
    QCOMPARE(read(second.outputPath), QByteArray("second"));
  }
  void restoreCollisionAndChosenFolder() {
    Fixture f; QVERIFY(f.admit()); QVERIFY(write(f.source(), "payload"));
    const auto saved = f.owner().trash(f.trash(f.source()), f.cancel); QVERIFY(saved.ok());
    QVERIFY(::symlink("missing", QFile::encodeName(f.source()).constData()) == 0);
    QCOMPARE(f.owner().restore(f.restore(saved), f.cancel).error, MutationError::AlreadyExists);
    QCOMPARE(read(saved.outputPath), QByteArray("payload")); QVERIFY(QFileInfo(f.source()).isSymLink());
    const auto chosen = f.volume.filePath("chosen"); QVERIFY(privateDir(chosen));
    const auto destination = QDir(chosen).filePath("item");
    QVERIFY(f.owner().restore(f.restore(saved, destination), f.cancel).ok());
    QCOMPARE(read(destination), QByteArray("payload")); QVERIFY(QFileInfo(f.source()).isSymLink());
  }
  void missingOriginalParentRequiresExplicitChoice() {
    Fixture f; QVERIFY(f.admit()); QVERIFY(write(f.source(), "payload"));
    const auto saved = f.owner().trash(f.trash(f.source()), f.cancel); QVERIFY(saved.ok());
    QVERIFY(QDir().rmdir(f.sourceFolder()));
    auto original = f.restore(saved);
    // A deliberately stale observation cannot recreate the vanished parent.
    original.expectedParent = FileIdentity{1, 1, 0, 0, S_IFDIR | 0700};
    QCOMPARE(f.owner().restore(original, f.cancel).error, MutationError::Vanished);
    const auto chosen = f.volume.filePath("chosen"); QVERIFY(privateDir(chosen));
    QVERIFY(f.owner().restore(f.restore(saved, QDir(chosen).filePath("item")), f.cancel).ok());
    QVERIFY(!QDir(f.sourceFolder()).exists());
  }
  void chosenCrossDevicePreservesPayload() {
    Fixture f; QVERIFY(f.admit()); QVERIFY(write(f.source(), "payload"));
    const auto saved = f.owner().trash(f.trash(f.source()), f.cancel); QVERIFY(saved.ok());
    QCOMPARE(f.owner().restore(f.restore(saved, f.home.filePath("item")), f.cancel).error, MutationError::CrossDevice);
    QCOMPARE(read(saved.outputPath), QByteArray("payload")); QVERIFY(!QFileInfo::exists(f.home.filePath("item")));
  }
  void unavailableDiscoveryNeverCreatesStorage() {
    Fixture f; QVERIFY(f.admit());
    QVERIFY(VolumeTrash::discover(f.volume.path()).isEmpty());
    QVERIFY(!QFileInfo::exists(f.privateRoot()));
    QVERIFY(!QFileInfo::exists(f.volume.filePath(".Trash")));
    const auto absent = f.volume.filePath("gone");
    QVERIFY(VolumeTrash::discover(absent).isEmpty()); QVERIFY(!QFileInfo::exists(absent));
  }
  void topologyUsesActualMountBoundaryReadOnly() {
    Fixture f; QVERIFY(f.admit()); MutationResult result;
    TrashTopDirectory topology;
    const auto top = topology.forPath(f.source(), result);
    QVERIFY(result.ok()); QVERIFY(!top.isEmpty());
    auto observed = RecoveryDirectoryAdmission::open(top, result); QVERIFY(observed);
    auto source = RecoveryDirectoryAdmission::open(f.sourceFolder(), result); QVERIFY(source);
    QCOMPARE(observed->observation().mountId, source->observation().mountId);
    const auto parent = QFileInfo(top).absolutePath();
    if (parent != top) {
      auto parentAdmission = RecoveryDirectoryAdmission::open(parent, result); QVERIFY(parentAdmission);
      QVERIFY(parentAdmission->observation().mountId != observed->observation().mountId);
    }
  }
};
QTEST_GUILESS_MAIN(VolumeTrashTests)
#include "tst_volume_trash.moc"
