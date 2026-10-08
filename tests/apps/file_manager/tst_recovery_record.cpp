// SPDX-License-Identifier: GPL-3.0-or-later
#include "mutation/recovery_record_store.h"
#include "mutation/recovery_catalog.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>
#include <limits>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
using namespace QindaQt::Apps::FileManager;
namespace {
RecoveryRecord fixture(const QString &directory) {
  RecoveryRecord v;
  v.operationId = QStringLiteral("4360f8c4-7616-4d8c-a648-c056b9a28bb8");
  v.sourcePath = QStringLiteral("/source/file");
  v.destinationPath = QStringLiteral("/destination/file");
  v.stageDirectory = QStringLiteral("/destination/.stage");
  v.recoveryDirectory = directory;
  v.sourceIdentity = {7, 123, 81, -1234, S_IFREG | 0600};
  v.sourceParent = {23, 7, 122, S_IFDIR | 0700, static_cast<quint32>(::getuid())};
  v.destinationParent = {24, 8, 125, S_IFDIR | 0700, static_cast<quint32>(::getuid())};
  v.recoveryStorage = v.sourceParent;
  MutationResult admissionResult;
  auto admitted = RecoveryDirectoryAdmission::open(directory, admissionResult);
  if (admitted) v.recoveryStorage = admitted->observation();
  v.manifestDigest = QByteArray(32, 'a');
  v.retainedBytesEstimate = std::numeric_limits<quint64>::max();
  return v;
}
bool writeBytes(const QString &path, const QByteArray &bytes) {
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
  const bool written = file.write(bytes) == bytes.size();
  file.close();
  return written && ::chmod(QFile::encodeName(path).constData(), 0600) == 0;
}
QByteArray contents(const QString &path) {
  QFile file(path);
  return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}
}
class RecoveryRecordTests final : public QObject {
  Q_OBJECT
private slots:
  void catalogWriterVersion_data();
  void catalogWriterVersion();
  void canonicalRoundTrip();
  void malformed_data();
  void malformed();
  void privateAppendAndRestartInspection();
  void failedBarriers_data();
  void failedBarriers();
  void foreignReplacementAtDurabilityBoundary();
  void sameInodeRewriteAfterFileSyncIsNotDurableSuccess();
  void identicalReplacementBeforeReadbackIsNotWriterEvidence();
  void noSymlinkHardlinkOrFifoRead();
  void immutableOperationAndSequence();
  void changedDirectoryNeverWritesReplacement();
};
void RecoveryRecordTests::catalogWriterVersion_data() {
  QTest::addColumn<bool>("replace");
  QTest::newRow("same-inode-post-sync") << false;
  QTest::newRow("same-bytes-new-inode-readback") << true;
}
void RecoveryRecordTests::catalogWriterVersion() {
  QFETCH(bool, replace);
  QTemporaryDir directory; QVERIFY(directory.isValid());
  const auto catalogPath = directory.filePath(QStringLiteral("catalog"));
  const auto record = fixture(directory.path());
  const auto bytes = encodeRecoveryRecord(record);
  const auto path = QDir(catalogPath).filePath(record.operationId + QStringLiteral(".json"));
  const auto saved = directory.filePath(QStringLiteral("saved-writer"));
  bool altered = false;
  RecoveryCatalog catalog(catalogPath, [&](RecoveryCatalogStep step) {
    if ((replace && step != RecoveryCatalogStep::ReadRecord) ||
        (!replace && step != RecoveryCatalogStep::SyncDirectory)) return MutationError::None;
    if (replace) altered = QFile::rename(path, saved) && writeBytes(path, bytes);
    else {
      const int fd = ::open(QFile::encodeName(path).constData(), O_WRONLY | O_CLOEXEC | O_NOFOLLOW);
      struct stat before {}, after {};
      bool ok = fd >= 0 && ::fstat(fd, &before) == 0 && ::ftruncate(fd, 0) == 0 &&
          ::write(fd, bytes.constData(), static_cast<size_t>(bytes.size())) == bytes.size();
      if (ok) {
        struct timespec times[2] = {before.st_atim, before.st_mtim}; ++times[1].tv_sec;
        ok = ::futimens(fd, times) == 0 && ::fstat(fd, &after) == 0 &&
            before.st_dev == after.st_dev && before.st_ino == after.st_ino;
      }
      if (fd >= 0) ::close(fd);
      altered = ok;
    }
    return altered ? MutationError::None : MutationError::IoError;
  });
  QVERIFY(catalog.admit(true).ok());
  QCOMPARE(catalog.add(record).error, MutationError::Changed); QVERIFY(altered);
  QCOMPARE(contents(path), bytes);
  if (replace) QCOMPARE(contents(saved), bytes);
}
void RecoveryRecordTests::canonicalRoundTrip() {
  auto record = fixture(QStringLiteral("/source/.recovery"));
  const auto bytes = encodeRecoveryRecord(record);
  QVERIFY(!bytes.isEmpty());
  const auto decoded = decodeRecoveryRecord(bytes);
  QVERIFY(decoded.result.ok());
  QVERIFY(decoded.record && *decoded.record == record);
  record.diagnostic = QString(513, QLatin1Char('x'));
  QVERIFY(encodeRecoveryRecord(record).isEmpty());
  record.diagnostic.clear();
  record.sourcePath = QStringLiteral("/source/../other");
  QVERIFY(encodeRecoveryRecord(record).isEmpty());
}
void RecoveryRecordTests::malformed_data() {
  QTest::addColumn<QByteArray>("bytes");
  const auto valid = encodeRecoveryRecord(fixture(QStringLiteral("/source/.recovery")));
  auto changed = [&](const QString &key, const QJsonValue &value) {
    auto object = QJsonDocument::fromJson(valid).object();
    object.insert(key, value);
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
  };
  QTest::newRow("duplicate") << (valid.left(valid.size() - 1) + ",\"schema\":1}");
  QTest::newRow("unknown") << changed(QStringLiteral("extra"), true);
  QTest::newRow("version") << changed(QStringLiteral("schema"), 2);
  QTest::newRow("trailing") << (valid + " ");
  QTest::newRow("truncated") << valid.left(valid.size() - 1);
  QTest::newRow("oversize") << QByteArray(maximumRecoveryRecordBytes + 1, ' ');
  QTest::newRow("numeric-loss") << changed(QStringLiteral("retainedBytesEstimate"), 123);
  QTest::newRow("integer-spelling") << changed(QStringLiteral("sequence"), QStringLiteral("00"));
  QTest::newRow("negative-unsigned") << changed(QStringLiteral("sequence"), QStringLiteral("-1"));
  QTest::newRow("overflow") << changed(QStringLiteral("retainedBytesEstimate"), QStringLiteral("18446744073709551616"));
  QTest::newRow("phase") << changed(QStringLiteral("phase"), QStringLiteral("completed"));
  QTest::newRow("uuid") << changed(QStringLiteral("operation"), QStringLiteral("../payload"));
  QTest::newRow("relative") << changed(QStringLiteral("source"), QStringLiteral("relative"));
  QTest::newRow("null-path") << changed(QStringLiteral("source"), QStringLiteral("/source/") + QChar::Null);
  QTest::newRow("digest") << changed(QStringLiteral("manifest"), QStringLiteral("zz"));
}
void RecoveryRecordTests::malformed() {
  QFETCH(QByteArray, bytes);
  const auto decoded = decodeRecoveryRecord(bytes);
  QVERIFY(!decoded.result.ok());
  QVERIFY(!decoded.record);
}
void RecoveryRecordTests::privateAppendAndRestartInspection() {
  QTemporaryDir directory;
  MutationResult error;
  auto live = RecoveryDirectoryAdmission::open(directory.path(), error);
  QVERIFY2(live.has_value(), qPrintable(error.diagnostic));
  auto record = fixture(directory.path());
  {
    RecoveryRecordStore store(*live);
    const auto first = store.append(record);
    QVERIFY(first.result.ok());
    QVERIFY(first.entryCreated && first.fileSynced && first.directorySynced);
    record.phase = RecoveryPhase::Copying;
    record.sequence = 1;
    QVERIFY(store.append(record).result.ok());
  }
  live.reset();
  auto restarted = RecoveryDirectoryAdmission::open(directory.path(), error);
  QVERIFY(restarted);
  const auto before = QDir(directory.path()).entryList(QDir::Files);
  RecoveryRecordStore inspection(*restarted);
  const auto latest = inspection.inspect();
  QVERIFY(latest.result.ok());
  QVERIFY(latest.record && *latest.record == record);
  QCOMPARE(QDir(directory.path()).entryList(QDir::Files), before);
  QVERIFY(!QFile::exists(directory.filePath(QStringLiteral("payload"))));
}
void RecoveryRecordTests::failedBarriers_data() {
  QTest::addColumn<int>("step");
  for (const auto step : {RecoveryStoreStep::CreateRecord, RecoveryStoreStep::WriteRecord,
                          RecoveryStoreStep::SyncRecord, RecoveryStoreStep::SyncDirectory,
                          RecoveryStoreStep::ReadRecord})
    QTest::newRow(qPrintable(QString::number(static_cast<int>(step)))) << static_cast<int>(step);
}
void RecoveryRecordTests::failedBarriers() {
  QFETCH(int, step);
  QTemporaryDir directory;
  MutationResult error;
  auto live = RecoveryDirectoryAdmission::open(directory.path(), error);
  QVERIFY(live);
  RecoveryRecordStore store(*live, [step](RecoveryStoreStep current) {
    return static_cast<int>(current) == step ? MutationError::DiskFull : MutationError::None;
  });
  const auto result = store.append(fixture(directory.path()));
  QCOMPARE(result.result.error, MutationError::DiskFull);
  const bool created = step != static_cast<int>(RecoveryStoreStep::CreateRecord);
  QCOMPARE(result.entryCreated, created);
  QCOMPARE(QFile::exists(directory.filePath(QStringLiteral("record-00.json"))), created);
  QCOMPARE(result.fileSynced, step >= static_cast<int>(RecoveryStoreStep::SyncDirectory));
  QCOMPARE(result.directorySynced, step == static_cast<int>(RecoveryStoreStep::ReadRecord));
  if (step == static_cast<int>(RecoveryStoreStep::WriteRecord)) {
    RecoveryRecordStore retry(*live);
    QVERIFY(!retry.inspect().result.ok());
    QVERIFY(!retry.append(fixture(directory.path())).result.ok());
    QVERIFY(QFile::exists(directory.filePath(QStringLiteral("record-00.json"))));
  }
}
void RecoveryRecordTests::foreignReplacementAtDurabilityBoundary() {
  QTemporaryDir directory;
  MutationResult error;
  auto live = RecoveryDirectoryAdmission::open(directory.path(), error);
  QVERIFY(live);
  const auto path = directory.filePath(QStringLiteral("record-00.json"));
  const auto held = directory.filePath(QStringLiteral("retained-original"));
  RecoveryRecordStore store(*live, [&](RecoveryStoreStep step) {
    if (step == RecoveryStoreStep::SyncDirectory &&
        (!QFile::rename(path, held) || !writeBytes(path, QByteArray("foreign sentinel"))))
      return MutationError::IoError;
    return MutationError::None;
  });
  QCOMPARE(store.append(fixture(directory.path())).result.error, MutationError::Changed);
  QCOMPARE(contents(path), QByteArray("foreign sentinel"));
  QVERIFY(!contents(held).isEmpty());
}
void RecoveryRecordTests::sameInodeRewriteAfterFileSyncIsNotDurableSuccess() {
  QTemporaryDir directory;
  MutationResult error;
  auto live = RecoveryDirectoryAdmission::open(directory.path(), error);
  QVERIFY(live);
  const auto record = fixture(directory.path());
  const auto expected = encodeRecoveryRecord(record);
  const auto path = directory.filePath(QStringLiteral("record-00.json"));
  bool rewritten = false;
  RecoveryRecordStore store(*live, [&](RecoveryStoreStep step) {
    if (step != RecoveryStoreStep::SyncDirectory) return MutationError::None;
    const auto name = QFile::encodeName(path);
    const int fd = ::open(name.constData(), O_WRONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) return MutationError::IoError;
    struct stat before {};
    struct stat after {};
    bool ok = ::fstat(fd, &before) == 0 && ::ftruncate(fd, 0) == 0;
    if (ok) ok = ::write(fd, expected.constData(), static_cast<size_t>(expected.size())) == expected.size();
    // Deterministic metadata difference: no sleep, replacement inode or
    // positive fsync substitute. Rewrite occurs after the owner's file sync.
    if (ok) {
      struct timespec times[2] = {before.st_atim, before.st_mtim};
      ++times[1].tv_sec;
      ok = ::futimens(fd, times) == 0 && ::fstat(fd, &after) == 0 &&
          before.st_dev == after.st_dev && before.st_ino == after.st_ino;
    }
    ::close(fd);
    rewritten = ok;
    return ok ? MutationError::None : MutationError::IoError;
  });
  const auto result = store.append(record);
  QVERIFY(rewritten);
  QCOMPARE(contents(path), expected);
  QVERIFY(result.entryCreated);
  QVERIFY(result.fileSynced);
  QVERIFY(result.directorySynced);
  QCOMPARE(result.result.error, MutationError::Changed);
}
void RecoveryRecordTests::identicalReplacementBeforeReadbackIsNotWriterEvidence() {
  QTemporaryDir directory;
  MutationResult error;
  auto live = RecoveryDirectoryAdmission::open(directory.path(), error);
  QVERIFY(live);
  const auto record = fixture(directory.path());
  const auto bytes = encodeRecoveryRecord(record);
  const auto path = directory.filePath(QStringLiteral("record-00.json"));
  const auto original = directory.filePath(QStringLiteral("original-synced-record"));
  bool replaced = false;
  RecoveryRecordStore store(*live, [&](RecoveryStoreStep step) {
    if (step != RecoveryStoreStep::ReadRecord) return MutationError::None;
    replaced = QFile::rename(path, original) && writeBytes(path, bytes);
    return replaced ? MutationError::None : MutationError::IoError;
  });
  const auto result = store.append(record);
  QVERIFY(replaced);
  QCOMPARE(result.result.error, MutationError::Changed);
  QVERIFY(result.fileSynced && result.directorySynced);
  QCOMPARE(contents(path), bytes);
  QCOMPARE(contents(original), bytes);
}
void RecoveryRecordTests::noSymlinkHardlinkOrFifoRead() {
  for (const int kind : {0, 1, 2}) {
    QTemporaryDir directory;
    QTemporaryDir outside;
    const auto sentinel = outside.filePath(QStringLiteral("sentinel"));
    QVERIFY(writeBytes(sentinel, QByteArray("foreign sentinel")));
    const auto name = QFile::encodeName(directory.filePath(QStringLiteral("record-00.json")));
    const auto target = QFile::encodeName(sentinel);
    QCOMPARE(kind == 0 ? ::symlink(target.constData(), name.constData()) :
             kind == 1 ? ::link(target.constData(), name.constData()) :
                         ::mkfifo(name.constData(), 0600), 0);
    MutationResult error;
    auto live = RecoveryDirectoryAdmission::open(directory.path(), error);
    QVERIFY(live);
    RecoveryRecordStore store(*live);
    QVERIFY(!store.inspect().result.ok());
    QVERIFY(!store.append(fixture(directory.path())).result.ok());
    QCOMPARE(contents(sentinel), QByteArray("foreign sentinel"));
    struct stat status {};
    QCOMPARE(::lstat(name.constData(), &status), 0);
  }
}
void RecoveryRecordTests::immutableOperationAndSequence() {
  QTemporaryDir directory;
  MutationResult error;
  auto live = RecoveryDirectoryAdmission::open(directory.path(), error);
  QVERIFY(live);
  RecoveryRecordStore store(*live);
  auto record = fixture(directory.path());
  QVERIFY(store.append(record).result.ok());
  record.phase = RecoveryPhase::Retiring;
  record.sequence = 1;
  QVERIFY(!store.append(record).result.ok());
  record.phase = RecoveryPhase::Copying;
  record.sourcePath = QStringLiteral("/foreign/path");
  QVERIFY(!store.append(record).result.ok());
  QVERIFY(!QFile::exists(directory.filePath(QStringLiteral("record-01.json"))));
  const auto original = contents(directory.filePath(QStringLiteral("record-00.json")));
  QVERIFY(!store.append(fixture(directory.path())).result.ok());
  QCOMPARE(contents(directory.filePath(QStringLiteral("record-00.json"))), original);
}
void RecoveryRecordTests::changedDirectoryNeverWritesReplacement() {
  QTemporaryDir root;
  const auto path = root.filePath(QStringLiteral("private"));
  QVERIFY(QDir().mkdir(path));
  QCOMPARE(::chmod(QFile::encodeName(path).constData(), 0700), 0);
  MutationResult error;
  auto live = RecoveryDirectoryAdmission::open(path, error);
  QVERIFY(live);
  const auto record = fixture(path);
  QVERIFY(QDir().rename(path, path + QStringLiteral("-old")));
  QVERIFY(QDir().mkdir(path));
  QCOMPARE(::chmod(QFile::encodeName(path).constData(), 0700), 0);
  RecoveryRecordStore store(*live);
  QCOMPARE(store.append(record).result.error, MutationError::Changed);
  QVERIFY(QDir(path).entryList(QDir::Files).isEmpty());
}
QTEST_GUILESS_MAIN(RecoveryRecordTests)
#include "tst_recovery_record.moc"
