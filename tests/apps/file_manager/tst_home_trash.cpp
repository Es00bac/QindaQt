// SPDX-License-Identifier: GPL-3.0-or-later
#include "mutation/local_mutation_backend.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace QindaQt::Apps::FileManager;

namespace {

class SplitDeviceResolver final : public DeviceResolver {
public:
  explicit SplitDeviceResolver(QString trashRoot)
      : m_trashRoot(std::move(trashRoot)) {}

  [[nodiscard]] std::optional<quint64>
  deviceForPath(const QString &path) const override {
    return path.startsWith(m_trashRoot) ? 2 : 1;
  }

private:
  QString m_trashRoot;
};

[[nodiscard]] bool writeFile(const QString &path, const QByteArray &data = "payload") {
  QFile file(path);
  return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
}

[[nodiscard]] MutationRequest trashRequest(const QString &path) {
  MutationRequest request;
  request.kind = MutationKind::Trash;
  request.sourcePath = path;
  request.declaredRoots = {QFileInfo(path).absolutePath()};
  request.expectedSource = LocalMutationBackend::identityForPath(path);
  return request;
}

[[nodiscard]] MutationRequest restoreRequest(const MutationResult &trashed) {
  MutationRequest request;
  request.kind = MutationKind::Restore;
  request.trashToken = trashed.trashToken;
  request.destinationPath = trashed.originalPath;
  request.declaredRoots = {QFileInfo(trashed.originalPath).absolutePath()};
  request.expectedSource = trashed.outputIdentity;
  request.expectedParent = LocalMutationBackend::identityForPath(
      QFileInfo(trashed.originalPath).absolutePath());
  return request;
}

} // namespace

class TestHomeTrash final : public QObject {
  Q_OBJECT

private slots:
  void trashInfoRoundTripHandlesHostileNames();
  void repeatedNameGetsUniquePayload();
  void crossDeviceTrashRefusesWithoutDeleting();
  void restoreCollisionPreservesBothItems();
  void emptyTrashRemovesPayloadsWithoutFollowingSymlinks();
  void symlinkedTrashRootIsRefusedWithoutTouchingTarget();
};

void TestHomeTrash::trashInfoRoundTripHandlesHostileNames() {
  QTemporaryDir fixture;
  QVERIFY(fixture.isValid());
  const QString trashRoot = fixture.filePath(QStringLiteral("data/Trash"));
  LocalMutationBackend backend(trashRoot);
  const auto token = std::make_shared<std::atomic_bool>(false);
  const QString original = fixture.filePath(QStringLiteral("文档\n100%.txt"));
  QVERIFY(writeFile(original));

  const MutationResult trashed = backend.execute(trashRequest(original), token, {});
  QVERIFY2(trashed.ok(), qPrintable(trashed.diagnostic));
  QVERIFY(!QFileInfo::exists(original));
  QVERIFY(QFileInfo(trashed.outputPath).isFile());
  QFile info(QDir(trashRoot).filePath(
      QStringLiteral("info/%1.trashinfo").arg(trashed.trashToken)));
  QVERIFY(info.open(QIODevice::ReadOnly));
  const QByteArray metadata = info.readAll();
  QVERIFY(metadata.startsWith("[Trash Info]\nPath=/"));
  QVERIFY(metadata.contains("%0A"));
  QVERIFY(metadata.contains("%25"));
  QVERIFY(metadata.contains("\nDeletionDate="));

  const MutationResult restored = backend.execute(restoreRequest(trashed), token, {});
  QVERIFY2(restored.ok(), qPrintable(restored.diagnostic));
  QFile restoredFile(original);
  QVERIFY(restoredFile.open(QIODevice::ReadOnly));
  QCOMPARE(restoredFile.readAll(), QByteArray("payload"));
  QVERIFY(!QFileInfo::exists(trashed.outputPath));
}

void TestHomeTrash::repeatedNameGetsUniquePayload() {
  QTemporaryDir fixture;
  QVERIFY(fixture.isValid());
  const QString trashRoot = fixture.filePath(QStringLiteral("Trash"));
  LocalMutationBackend backend(trashRoot);
  const auto cancellation = std::make_shared<std::atomic_bool>(false);
  const QString original = fixture.filePath(QStringLiteral("same"));
  QVERIFY(writeFile(original, "one"));
  const MutationResult first = backend.execute(trashRequest(original), cancellation, {});
  QVERIFY(first.ok());
  QVERIFY(writeFile(original, "two"));
  const MutationResult second = backend.execute(trashRequest(original), cancellation, {});
  QVERIFY(second.ok());
  QVERIFY(first.trashToken != second.trashToken);
  QVERIFY(QFileInfo::exists(first.outputPath));
  QVERIFY(QFileInfo::exists(second.outputPath));
}

void TestHomeTrash::crossDeviceTrashRefusesWithoutDeleting() {
  QTemporaryDir fixture;
  QVERIFY(fixture.isValid());
  const QString trashRoot = fixture.filePath(QStringLiteral("Trash"));
  LocalMutationBackend backend(
      trashRoot, std::make_shared<SplitDeviceResolver>(trashRoot));
  const QString original = fixture.filePath(QStringLiteral("keep"));
  QVERIFY(writeFile(original));
  const MutationResult result = backend.execute(
      trashRequest(original), std::make_shared<std::atomic_bool>(false), {});
  QCOMPARE(result.error, MutationError::CrossDevice);
  QVERIFY(QFileInfo::exists(original));
  QVERIFY(QDir(QDir(trashRoot).filePath(QStringLiteral("files")))
              .entryList(QDir::NoDotAndDotDot | QDir::AllEntries)
              .isEmpty());
}

void TestHomeTrash::restoreCollisionPreservesBothItems() {
  QTemporaryDir fixture;
  QVERIFY(fixture.isValid());
  LocalMutationBackend backend(fixture.filePath(QStringLiteral("Trash")));
  const auto cancellation = std::make_shared<std::atomic_bool>(false);
  const QString original = fixture.filePath(QStringLiteral("document"));
  QVERIFY(writeFile(original, "trashed"));
  const MutationResult trashed = backend.execute(trashRequest(original), cancellation, {});
  QVERIFY(trashed.ok());
  QVERIFY(writeFile(original, "replacement"));
  const MutationResult restore = backend.execute(
      restoreRequest(trashed), cancellation, {});
  QCOMPARE(restore.error, MutationError::AlreadyExists);
  QVERIFY(QFileInfo::exists(trashed.outputPath));
  QFile replacement(original);
  QVERIFY(replacement.open(QIODevice::ReadOnly));
  QCOMPARE(replacement.readAll(), QByteArray("replacement"));
}

void TestHomeTrash::emptyTrashRemovesPayloadsWithoutFollowingSymlinks() {
  QTemporaryDir fixture;
  QVERIFY(fixture.isValid());
  const QString trashRoot = fixture.filePath(QStringLiteral("Trash"));
  LocalMutationBackend backend(trashRoot);
  const auto cancellation = std::make_shared<std::atomic_bool>(false);
  const QString original = fixture.filePath(QStringLiteral("document"));
  const QString outside = fixture.filePath(QStringLiteral("outside"));
  QVERIFY(writeFile(original));
  QVERIFY(writeFile(outside, "safe"));
  const MutationResult trashed = backend.execute(trashRequest(original), cancellation, {});
  QVERIFY(trashed.ok());
  QVERIFY(QFile::link(outside, QDir(trashRoot).filePath(QStringLiteral("files/link"))));

  MutationRequest empty;
  empty.kind = MutationKind::EmptyTrash;
  int progress = 0;
  const MutationResult result = backend.execute(
      empty, cancellation,
      [&progress](const MutationProgress &) { ++progress; });
  QVERIFY2(result.ok(), qPrintable(result.diagnostic));
  QVERIFY(progress >= 2);
  QVERIFY(QFileInfo::exists(outside));
  QVERIFY(QDir(QDir(trashRoot).filePath(QStringLiteral("files")))
              .entryList(QDir::NoDotAndDotDot | QDir::AllEntries)
              .isEmpty());
  QVERIFY(QDir(QDir(trashRoot).filePath(QStringLiteral("info")))
              .entryList(QDir::NoDotAndDotDot | QDir::AllEntries)
              .isEmpty());
}

void TestHomeTrash::symlinkedTrashRootIsRefusedWithoutTouchingTarget() {
  QTemporaryDir fixture;
  QVERIFY(fixture.isValid());
  const QString target = fixture.filePath(QStringLiteral("outside-trash"));
  const QString trashRoot = fixture.filePath(QStringLiteral("Trash"));
  QVERIFY(QDir().mkpath(QDir(target).filePath(QStringLiteral("files"))));
  QVERIFY(QDir().mkpath(QDir(target).filePath(QStringLiteral("info"))));
  const QString sentinel = QDir(target).filePath(QStringLiteral("files/sentinel"));
  QVERIFY(writeFile(sentinel, "safe"));
  QVERIFY(QFile::link(target, trashRoot));
  LocalMutationBackend backend(trashRoot);
  MutationRequest empty;
  empty.kind = MutationKind::EmptyTrash;
  const MutationResult result = backend.execute(
      empty, std::make_shared<std::atomic_bool>(false), {});
  QCOMPARE(result.error, MutationError::SymlinkEscape);
  QVERIFY(QFileInfo::exists(sentinel));
}

QTEST_APPLESS_MAIN(TestHomeTrash)
#include "tst_home_trash.moc"
