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
    QCOMPARE(readBytes(destination), QByteArray("foreign replacement"));
    QCOMPARE(readBytes(displaced), payload);
  }
};

QTEST_GUILESS_MAIN(CopySafetyTests)
#include "tst_copy_safety.moc"
