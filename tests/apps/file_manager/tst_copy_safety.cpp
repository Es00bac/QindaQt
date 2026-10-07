// SPDX-License-Identifier: GPL-3.0-or-later
#include "mutation/local_mutation_backend.h"
#include "mutation/safe_tree_operations.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

using namespace QindaQt::Apps::FileManager;

namespace {
bool writeBytes(const QString &path, const QByteArray &bytes) {
  QFile file(path);
  return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}
QByteArray readBytes(const QString &path) {
  QFile file(path);
  return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}
MutationRequest copyRequest(const QString &source, const QString &destination) {
  MutationRequest request;
  request.kind = MutationKind::Copy;
  request.sourcePath = source;
  request.destinationPath = destination;
  request.declaredRoots = {QFileInfo(source).absolutePath(),
                           QFileInfo(destination).absolutePath()};
  request.expectedSource = LocalMutationBackend::identityForPath(source);
  request.expectedParent = LocalMutationBackend::identityForPath(
      QFileInfo(destination).absolutePath());
  return request;
}
} // namespace

class CopySafetyTests final : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void failedExclusiveCreatePreservesExistingDestination_data() {
    QTest::addColumn<bool>("directory");
    QTest::newRow("foreign-file") << false;
    QTest::newRow("foreign-tree") << true;
  }
  void failedExclusiveCreatePreservesExistingDestination() {
    QFETCH(bool, directory);
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto source = temp.filePath(QStringLiteral("source"));
    const auto destination = temp.filePath(QStringLiteral("destination"));
    QVERIFY(writeBytes(source, "source bytes"));
    QString sentinel = destination;
    if (directory) {
      QVERIFY(QDir().mkdir(destination));
      sentinel = QDir(destination).filePath(QStringLiteral("sentinel"));
    }
    QVERIFY(writeBytes(sentinel, "foreign bytes must survive"));
    const auto parent = LocalMutationBackend::identityForPath(temp.path());
    QVERIFY(parent);
    // Direct real helper call models a destination appearing after the
    // backend's advisory existence check; failed O_EXCL grants no ownership.
    const auto result = copyLocalTreeNoFollow(
        source, destination, *parent, std::make_shared<std::atomic_bool>(false),
        {}, 20'000);
    QCOMPARE(result.error, MutationError::AlreadyExists);
    QCOMPARE(result.outputObservation.disposition, MutationOutputDisposition::None);
    QVERIFY(!result.outputObservation.writtenIdentity);
    QCOMPARE(readBytes(sentinel), QByteArray("foreign bytes must survive"));
    QCOMPARE(readBytes(source), QByteArray("source bytes"));
  }
  void cancellationCannotDeleteAReplacementDestination() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto source = temp.filePath(QStringLiteral("source"));
    const auto destination = temp.filePath(QStringLiteral("destination"));
    const auto displaced = temp.filePath(QStringLiteral("displaced-owned-copy"));
    QVERIFY(writeBytes(source, QByteArray(256 * 1024, 'x')));
    const auto parent = LocalMutationBackend::identityForPath(temp.path());
    QVERIFY(parent);
    const auto cancel = std::make_shared<std::atomic_bool>(false);
    bool replaced = false;
    const auto result = copyLocalTreeNoFollow(
        source, destination, *parent, cancel,
        [&](const MutationProgress &) {
          if (replaced)
            return;
          QVERIFY(QFile::rename(destination, displaced));
          QVERIFY(writeBytes(destination, "foreign replacement"));
          replaced = true;
          cancel->store(true);
        }, 20'000);
    QVERIFY(replaced);
    QCOMPARE(result.error, MutationError::Cancelled);
    QCOMPARE(result.outputObservation.disposition, MutationOutputDisposition::Replaced);
    QCOMPARE(result.outputObservation.path, destination);
    QVERIFY(result.outputObservation.exclusiveCreation);
    QVERIFY(!result.outputObservation.copyFinished);
    QVERIFY(result.outputObservation.writtenIdentity != result.outputObservation.observedIdentity);
    QCOMPARE(readBytes(destination), QByteArray("foreign replacement"));
    QVERIFY(QFileInfo::exists(displaced));
    QCOMPARE(readBytes(source), QByteArray(256 * 1024, 'x'));
  }
  void vanishedSourcePostcheckCannotDeleteReplacementDestination() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto source = temp.filePath(QStringLiteral("source"));
    const auto destination = temp.filePath(QStringLiteral("destination"));
    const auto displaced = temp.filePath(QStringLiteral("displaced-owned-copy"));
    const QByteArray payload(256 * 1024, 'x');
    QVERIFY(writeBytes(source, payload));
    const auto request = copyRequest(source, destination);
    LocalMutationBackend backend(temp.filePath(QStringLiteral("Trash")));
    bool replaced = false;
    const auto result = backend.execute(
        request, std::make_shared<std::atomic_bool>(false),
        [&](const MutationProgress &) {
          if (replaced)
            return;
          // The read descriptor remains open. The backend's final path
          // check, rather than its copier, observes that source vanished.
          QVERIFY(QFile::remove(source));
          QVERIFY(QFile::rename(destination, displaced));
          QVERIFY(writeBytes(destination, "foreign replacement"));
          replaced = true;
        });
    QVERIFY(replaced);
    QCOMPARE(result.error, MutationError::Vanished);
    QCOMPARE(result.outputObservation.disposition, MutationOutputDisposition::Replaced);
    QVERIFY(result.outputObservation.copyFinished);
    QVERIFY(result.outputPath.isEmpty());
    QVERIFY(!result.outputIdentity);
    QCOMPARE(readBytes(destination), QByteArray("foreign replacement"));
    QCOMPARE(readBytes(displaced), payload);
  }
  void destinationParentReplacementIsUnconfirmedAndPreserved() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto source = temp.filePath(QStringLiteral("source"));
    const auto parentPath = temp.filePath(QStringLiteral("parent"));
    const auto movedParent = temp.filePath(QStringLiteral("moved-parent"));
    QVERIFY(QDir().mkdir(parentPath));
    const auto destination = QDir(parentPath).filePath(QStringLiteral("copy"));
    QVERIFY(writeBytes(source, QByteArray(256 * 1024, 'x')));
    const auto request = copyRequest(source, destination);
    const auto cancel = std::make_shared<std::atomic_bool>(false);
    LocalMutationBackend backend(temp.filePath(QStringLiteral("Trash")));
    bool replaced = false;
    const auto result = backend.execute(request, cancel, [&](const MutationProgress &) {
      if (replaced)
        return;
      QVERIFY(QDir().rename(parentPath, movedParent));
      QVERIFY(QDir().mkdir(parentPath));
      QVERIFY(writeBytes(destination, "foreign under replacement parent"));
      replaced = true;
      cancel->store(true);
    });
    QVERIFY(replaced);
    QCOMPARE(result.error, MutationError::Cancelled);
    QCOMPARE(result.outputObservation.disposition, MutationOutputDisposition::Unconfirmed);
    QCOMPARE(readBytes(destination), QByteArray("foreign under replacement parent"));
    QVERIFY(QFileInfo::exists(QDir(movedParent).filePath(QStringLiteral("copy"))));
  }
  void sourcePostcheckRetainsTheCompletedCopy() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto source = temp.filePath(QStringLiteral("source"));
    const auto destination = temp.filePath(QStringLiteral("copy"));
    const QByteArray payload(256 * 1024, 'x');
    QVERIFY(writeBytes(source, payload));
    const auto request = copyRequest(source, destination);
    LocalMutationBackend backend(temp.filePath(QStringLiteral("Trash")));
    bool removed = false;
    const auto result = backend.execute(request, {}, [&](const MutationProgress &) {
      if (!removed) {
        QVERIFY(QFile::remove(source));
        removed = true;
      }
    });
    QVERIFY(removed);
    QCOMPARE(result.error, MutationError::Vanished);
    QCOMPARE(result.outputObservation.disposition, MutationOutputDisposition::RetainedCopy);
    QVERIFY(result.outputObservation.copyFinished);
    QCOMPARE(readBytes(destination), payload);
    QVERIFY(result.outputPath.isEmpty());
    QVERIFY(!result.outputIdentity);
  }
  void successStillCopiesFilesAndNestedFolders() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto source = temp.filePath(QStringLiteral("source"));
    const auto destination = temp.filePath(QStringLiteral("copy"));
    QVERIFY(QDir().mkpath(QDir(source).filePath(QStringLiteral("nested"))));
    QVERIFY(writeBytes(QDir(source).filePath(QStringLiteral("nested/file")), "nested bytes"));
    LocalMutationBackend backend(temp.filePath(QStringLiteral("Trash")));
    const auto result = backend.execute(copyRequest(source, destination), {}, {});
    QVERIFY2(result.ok(), qPrintable(result.diagnostic));
    QCOMPARE(result.outputObservation.disposition, MutationOutputDisposition::RetainedCopy);
    QVERIFY(!result.outputObservation.exclusiveCreation);
    QVERIFY(result.outputObservation.copyFinished);
    QCOMPARE(readBytes(QDir(destination).filePath(QStringLiteral("nested/file"))),
             QByteArray("nested bytes"));
  }

};

QTEST_GUILESS_MAIN(CopySafetyTests)
#include "tst_copy_safety.moc"
