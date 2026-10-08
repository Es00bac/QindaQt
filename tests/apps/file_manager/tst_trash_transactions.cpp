// SPDX-License-Identifier: GPL-3.0-or-later
#include "trash_test_support.h"
#include <QTest>
using namespace TrashTest;
class TrashTransactionTests final : public QObject {
  Q_OBJECT
private slots:
  void barriers_data() {
    QTest::addColumn<int>("step");
    for (const auto value : {TrashStep::OpenRecord, TrashStep::WriteRecord, TrashStep::SyncRecord,
         TrashStep::SyncInfo, TrashStep::ReadRecord, TrashStep::BeforeRename,
         TrashStep::AfterRename, TrashStep::SyncParents, TrashStep::ReadPayload})
      QTest::newRow(qPrintable(QString::number(static_cast<int>(value)))) << static_cast<int>(value);
  }
  void barriers() {
    QFETCH(int, step); Fixture f; QVERIFY(f.admit()); QVERIFY(write(f.source(), "payload"));
    auto owner = f.owner([&](TrashStep current) {
      return static_cast<int>(current) == step ? MutationError::IoError : MutationError::None;
    });
    const auto result = owner.trash(f.trash(f.source()), f.cancel);
    QCOMPARE(result.error, MutationError::IoError);
    const bool renamed = step >= static_cast<int>(TrashStep::AfterRename);
    if (renamed) {
      QVERIFY(!QFileInfo::exists(f.source())); QCOMPARE(read(result.outputPath), QByteArray("payload"));
      QVERIFY(!result.trashReceipt.payloadConfirmed);
      QVERIFY(result.outputObservation.disposition != MutationOutputDisposition::None);
    } else {
      QCOMPARE(read(f.source()), QByteArray("payload"));
      QVERIFY(result.outputPath.isEmpty());
    }
    if (step != static_cast<int>(TrashStep::OpenRecord))
      QVERIFY(QFileInfo::exists(result.trashReceipt.metadataPath));
  }
  void diskFullRefusalBeforeMove() {
    Fixture f; QVERIFY(f.admit()); QVERIFY(write(f.source(), "payload"));
    auto owner = f.owner([](TrashStep current) {
      return current == TrashStep::WriteRecord ? MutationError::DiskFull : MutationError::None;
    });
    const auto result = owner.trash(f.trash(f.source()), f.cancel);
    QCOMPARE(result.error, MutationError::DiskFull); QCOMPARE(read(f.source()), QByteArray("payload"));
    QVERIFY(result.trashReceipt.metadataRetained);
    QVERIFY(QFileInfo::exists(result.trashReceipt.metadataPath));
  }
  void metadataWriterVersion_data() {
    QTest::addColumn<bool>("replace");
    QTest::newRow("same-inode-after-file-sync") << false;
    QTest::newRow("same-bytes-replaced-before-reader") << true;
  }
  void metadataWriterVersion() {
    QFETCH(bool, replace); Fixture f; QVERIFY(f.admit()); QVERIFY(write(f.source(), "payload"));
    bool altered = false; const auto record = QDir(f.privateRoot()).filePath("info/item.trashinfo");
    const auto saved = f.volume.filePath("saved-record");
    auto owner = f.owner([&](TrashStep current) {
      if (current != (replace ? TrashStep::ReadRecord : TrashStep::SyncInfo)) return MutationError::None;
      const auto bytes = read(record);
      if (replace) altered = QFile::rename(record, saved) && write(record, bytes);
      else {
        const int fd = ::open(QFile::encodeName(record).constData(), O_WRONLY | O_CLOEXEC | O_NOFOLLOW);
        struct stat before {};
        altered = fd >= 0 && ::fstat(fd, &before) == 0 &&
            ::ftruncate(fd, 0) == 0 && ::write(fd, bytes.constData(), static_cast<size_t>(bytes.size())) == bytes.size();
        if (altered) {
          struct timespec times[2] = {before.st_atim, before.st_mtim}; ++times[1].tv_sec;
          altered = ::futimens(fd, times) == 0;
        }
        if (fd >= 0) ::close(fd);
      }
      return altered ? MutationError::None : MutationError::IoError;
    });
    const auto result = owner.trash(f.trash(f.source()), f.cancel);
    QCOMPARE(result.error, MutationError::Changed); QVERIFY(altered);
    QCOMPARE(read(f.source()), QByteArray("payload")); QVERIFY(!read(record).isEmpty());
    if (replace) QVERIFY(!read(saved).isEmpty());
  }
  void destinationRaceNeverOverwrites() {
    Fixture f; QVERIFY(f.admit()); QVERIFY(write(f.source(), "source"));
    QString foreign;
    auto owner = f.owner([&](TrashStep step) {
      if (step == TrashStep::BeforeRename) {
        foreign = QDir(f.privateRoot()).filePath("files/item");
        return write(foreign, "foreign") ? MutationError::None : MutationError::IoError;
      }
      return MutationError::None;
    });
    const auto result = owner.trash(f.trash(f.source()), f.cancel);
    QCOMPARE(result.error, MutationError::AlreadyExists);
    QCOMPARE(read(f.source()), QByteArray("source")); QCOMPARE(read(foreign), QByteArray("foreign"));
    QVERIFY(QFileInfo::exists(result.trashReceipt.metadataPath));
  }
  void unexpectedCapturedEntryIsRetained() {
    Fixture f; QVERIFY(f.admit()); QVERIFY(write(f.source(), "selected"));
    const auto saved = f.volume.filePath("original-retained");
    auto owner = f.owner([&](TrashStep step) {
      if (step != TrashStep::BeforeRename) return MutationError::None;
      return QFile::rename(f.source(), saved) && write(f.source(), "foreign")
          ? MutationError::None : MutationError::IoError;
    });
    const auto result = owner.trash(f.trash(f.source()), f.cancel);
    QCOMPARE(result.error, MutationError::Changed);
    QCOMPARE(read(saved), QByteArray("selected")); QCOMPARE(read(result.outputPath), QByteArray("foreign"));
    QVERIFY(!result.trashReceipt.payloadConfirmed);
    QVERIFY(QFileInfo::exists(result.trashReceipt.metadataPath));
  }
  void changedParentNeverMutatesReplacement() {
    Fixture f; QVERIFY(f.admit()); QVERIFY(write(f.source(), "selected"));
    const auto old = f.volume.filePath("old-parent");
    auto owner = f.owner([&](TrashStep step) {
      if (step != TrashStep::BeforeRename) return MutationError::None;
      return QDir().rename(f.sourceFolder(), old) && privateDir(f.sourceFolder()) &&
          write(f.source(), "replacement") ? MutationError::None : MutationError::IoError;
    });
    const auto result = owner.trash(f.trash(f.source()), f.cancel);
    QCOMPARE(result.error, MutationError::Changed); QCOMPARE(read(f.source()), QByteArray("replacement"));
    QCOMPARE(read(QDir(old).filePath("item")), QByteArray("selected"));
    QVERIFY(result.outputPath.isEmpty());
  }
  void cancelledBeforeRenamePreservesSourceAndReservedRecord() {
    Fixture f; QVERIFY(f.admit()); QVERIFY(write(f.source(), "payload"));
    auto owner = f.owner([&](TrashStep step) {
      if (step == TrashStep::BeforeRename) f.cancel->store(true);
      return MutationError::None;
    });
    const auto result = owner.trash(f.trash(f.source()), f.cancel);
    QCOMPARE(result.error, MutationError::Cancelled); QCOMPARE(read(f.source()), QByteArray("payload"));
    QVERIFY(QFileInfo::exists(result.trashReceipt.metadataPath)); QVERIFY(result.outputPath.isEmpty());
  }
  void restoreParentReplacementPreservesEveryEntry() {
    Fixture f; QVERIFY(f.admit()); QVERIFY(write(f.source(), "payload"));
    const auto saved = f.owner().trash(f.trash(f.source()), f.cancel); QVERIFY(saved.ok());
    const auto chosen = f.volume.filePath("chosen"), old = f.volume.filePath("old-chosen");
    QVERIFY(privateDir(chosen)); const auto request = f.restore(saved, QDir(chosen).filePath("item"));
    auto owner = f.owner([&](TrashStep step) {
      if (step != TrashStep::BeforeRename) return MutationError::None;
      return QDir().rename(chosen, old) && privateDir(chosen) &&
          write(QDir(chosen).filePath("sentinel"), "foreign") ? MutationError::None : MutationError::IoError;
    });
    QCOMPARE(owner.restore(request, f.cancel).error, MutationError::Changed);
    QCOMPARE(read(saved.outputPath), QByteArray("payload"));
    QCOMPARE(read(QDir(chosen).filePath("sentinel")), QByteArray("foreign"));
    QVERIFY(!QFileInfo::exists(QDir(chosen).filePath("item")));
  }
  void detachedVolumePathDoesNotMutateReplacement() {
    Fixture f; QVERIFY(f.admit()); QVERIFY(write(f.source(), "payload"));
    QTemporaryDir retired(QStringLiteral("/dev/shm/qindaqt-ed06-detached-XXXXXX")); QVERIFY(retired.isValid());
    const auto moved = retired.filePath("old-volume");
    auto owner = f.owner([&](TrashStep step) {
      if (step != TrashStep::BeforeRename) return MutationError::None;
      return QDir().rename(f.volume.path(), moved) && privateDir(f.volume.path()) &&
          write(f.volume.filePath("sentinel"), "replacement")
          ? MutationError::None : MutationError::IoError;
    });
    const auto result = owner.trash(f.trash(f.source()), f.cancel);
    QCOMPARE(result.error, MutationError::Changed);
    QCOMPARE(read(QDir(moved).filePath("source/item")), QByteArray("payload"));
    QCOMPARE(read(f.volume.filePath("sentinel")), QByteArray("replacement"));
    QVERIFY(!QFileInfo::exists(QDir(f.privateRoot()).filePath("files/item")));
  }
  void hostileMetadataEntriesNeverFollowOrBlock() {
    Fixture f; QVERIFY(f.admit()); MutationResult result;
    auto store = TrashStorage::open({f.privateRoot(), f.volume.path(), false}, true, result); QVERIFY(store);
    const auto outside = f.home.filePath("outside"); QVERIFY(write(outside, "foreign"));
    const auto info = QDir(f.privateRoot()).filePath("info");
    QVERIFY(::symlink(QFile::encodeName(outside).constData(), QFile::encodeName(QDir(info).filePath("link.trashinfo")).constData()) == 0);
    QVERIFY(::mkfifo(QFile::encodeName(QDir(info).filePath("fifo.trashinfo")).constData(), 0600) == 0);
    const auto regular = QDir(info).filePath("hard.trashinfo");
    QVERIFY(write(regular, "foreign")); QVERIFY(::link(QFile::encodeName(regular).constData(), QFile::encodeName(f.volume.filePath("hardlink")).constData()) == 0);
    for (const auto &token : {"link", "fifo", "hard"}) {
      std::optional<TrashRecord> record;
      QVERIFY(!store->read(QString::fromLatin1(token), record).ok()); QVERIFY(!record);
    }
    QCOMPARE(read(outside), QByteArray("foreign")); QCOMPARE(read(regular), QByteArray("foreign"));
  }
  void metadataReplacementBeforeRestoreIsNotDeleted() {
    Fixture f; QVERIFY(f.admit()); QVERIFY(write(f.source(), "payload"));
    const auto saved = f.owner().trash(f.trash(f.source()), f.cancel); QVERIFY(saved.ok());
    const auto old = f.volume.filePath("saved-record");
    auto owner = f.owner([&](TrashStep step) {
      if (step != TrashStep::BeforeRename) return MutationError::None;
      const auto bytes = read(saved.trashReceipt.metadataPath);
      return QFile::rename(saved.trashReceipt.metadataPath, old) && write(saved.trashReceipt.metadataPath, bytes)
          ? MutationError::None : MutationError::IoError;
    });
    QCOMPARE(owner.restore(f.restore(saved), f.cancel).error, MutationError::Changed);
    QCOMPARE(read(saved.outputPath), QByteArray("payload"));
    QVERIFY(!read(old).isEmpty()); QVERIFY(!read(saved.trashReceipt.metadataPath).isEmpty());
  }
  void infoReplacementAfterRenameRetainsPayloadAndForeignInfo() {
    Fixture f; QVERIFY(f.admit()); QVERIFY(write(f.source(), "payload"));
    const auto info = QDir(f.privateRoot()).filePath("info"), old = f.volume.filePath("saved-info");
    auto owner = f.owner([&](TrashStep step) {
      if (step != TrashStep::AfterRename) return MutationError::None;
      return QDir().rename(info, old) && privateDir(info) && write(QDir(info).filePath("sentinel"), "foreign")
          ? MutationError::None : MutationError::IoError;
    });
    const auto result = owner.trash(f.trash(f.source()), f.cancel);
    QCOMPARE(result.error, MutationError::Changed);
    QCOMPARE(read(result.outputPath), QByteArray("payload")); QCOMPARE(read(QDir(info).filePath("sentinel")), QByteArray("foreign"));
    QVERIFY(QFileInfo::exists(QDir(old).filePath("item.trashinfo")));
  }
};
QTEST_GUILESS_MAIN(TrashTransactionTests)
#include "tst_trash_transactions.moc"
