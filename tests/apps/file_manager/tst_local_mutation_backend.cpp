// SPDX-License-Identifier: GPL-3.0-or-later
#include "mutation/local_mutation_backend.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <sys/stat.h>
#include <unistd.h>

using namespace QindaQt::Apps::FileManager;

namespace {

[[nodiscard]] bool writeFile(const QString &path, const QByteArray &contents = "data") {
  QFile file(path);
  return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size();
}

[[nodiscard]] MutationRequest sourceRequest(MutationKind kind,
                                            const QString &source,
                                            const QString &destination = {}) {
  MutationRequest request;
  request.kind = kind;
  request.sourcePath = source;
  request.destinationPath = destination;
  request.declaredRoots = {QFileInfo(source).absolutePath()};
  if (!destination.isEmpty()) {
    request.declaredRoots.append(QFileInfo(destination).absolutePath());
    request.expectedParent = LocalMutationBackend::identityForPath(
        QFileInfo(destination).absolutePath());
  }
  request.expectedSource = LocalMutationBackend::identityForPath(source);
  return request;
}

} // namespace

class TestLocalMutationBackend final : public QObject {
  Q_OBJECT

private slots:
  void createRenameMoveAndCopyPreserveLocalData();
  void staleAndVanishedSourcesFailClosed();
  void sourceVanishingDuringCopyRemovesPartialOutput();
  void symlinkEscapeAndNestedSymlinkCopyAreRejected();
  void nestedDirectorySwapCannotRedirectCopy();
  void destinationParentSwapIsRejected();
  void preCancelledCopyDoesNotCreateDestination();
  void cancellationDuringCopyRemovesPartialDestination();
  void permissionDeniedCreateIsTyped();
};

void TestLocalMutationBackend::createRenameMoveAndCopyPreserveLocalData() {
  QTemporaryDir fixture;
  QVERIFY(fixture.isValid());
  const QString trash = fixture.filePath(QStringLiteral("Trash"));
  LocalMutationBackend backend(trash);
  const auto cancellation = std::make_shared<std::atomic_bool>(false);

  MutationRequest create;
  create.kind = MutationKind::CreateFolder;
  create.destinationPath = fixture.filePath(QStringLiteral("Δ folder\nname"));
  create.declaredRoots = {fixture.path()};
  create.expectedParent = LocalMutationBackend::identityForPath(fixture.path());
  const MutationResult created = backend.execute(create, cancellation, {});
  QVERIFY2(created.ok(), qPrintable(created.diagnostic));
  QVERIFY(created.undoRequest);
  QVERIFY(QFileInfo(created.outputPath).isDir());

  const QString source = fixture.filePath(QStringLiteral("source.txt"));
  QVERIFY(writeFile(source, "payload"));
  QVERIFY(QFile::setPermissions(source, QFileDevice::ReadOwner | QFileDevice::WriteOwner));
  const auto sourcePermissions = QFile(source).permissions();
  const QString renamed = fixture.filePath(QStringLiteral("renamed.txt"));
  const MutationResult rename = backend.execute(
      sourceRequest(MutationKind::Rename, source, renamed), cancellation, {});
  QVERIFY2(rename.ok(), qPrintable(rename.diagnostic));
  QVERIFY(rename.undoRequest);
  QVERIFY(!QFileInfo::exists(source));

  const QString moved = fixture.filePath(QStringLiteral("moved.txt"));
  const MutationResult move = backend.execute(
      sourceRequest(MutationKind::Move, renamed, moved), cancellation, {});
  QVERIFY2(move.ok(), qPrintable(move.diagnostic));
  QCOMPARE(QFile(moved).permissions(), sourcePermissions);

  const QString copied = fixture.filePath(QStringLiteral("copied.txt"));
  int progressCalls = 0;
  const MutationResult copy = backend.execute(
      sourceRequest(MutationKind::Copy, moved, copied), cancellation,
      [&progressCalls](const MutationProgress &) { ++progressCalls; });
  QVERIFY2(copy.ok(), qPrintable(copy.diagnostic));
  QVERIFY(progressCalls > 0);
  QFile copyFile(copied);
  QVERIFY(copyFile.open(QIODevice::ReadOnly));
  QCOMPARE(copyFile.readAll(), QByteArray("payload"));
  QCOMPARE(copyFile.permissions(), QFile(moved).permissions());
}

void TestLocalMutationBackend::staleAndVanishedSourcesFailClosed() {
  QTemporaryDir fixture;
  QVERIFY(fixture.isValid());
  LocalMutationBackend backend(fixture.filePath(QStringLiteral("Trash")));
  const auto cancellation = std::make_shared<std::atomic_bool>(false);
  const QString source = fixture.filePath(QStringLiteral("source"));
  QVERIFY(writeFile(source, "before"));
  MutationRequest stale = sourceRequest(
      MutationKind::Rename, source, fixture.filePath(QStringLiteral("after")));
  QFile changed(source);
  QVERIFY(changed.open(QIODevice::Append));
  QVERIFY(changed.write("changed") > 0);
  changed.close();
  QCOMPARE(backend.execute(stale, cancellation, {}).error, MutationError::Changed);
  QVERIFY(QFileInfo::exists(source));

  MutationRequest vanished = sourceRequest(
      MutationKind::Move, source, fixture.filePath(QStringLiteral("gone")));
  QVERIFY(QFile::remove(source));
  QCOMPARE(backend.execute(vanished, cancellation, {}).error, MutationError::Vanished);
}

void TestLocalMutationBackend::sourceVanishingDuringCopyRemovesPartialOutput() {
  QTemporaryDir fixture;
  QVERIFY(fixture.isValid());
  LocalMutationBackend backend(fixture.filePath(QStringLiteral("Trash")));
  const QString source = fixture.filePath(QStringLiteral("source"));
  const QString destination = fixture.filePath(QStringLiteral("copy"));
  QVERIFY(writeFile(source, QByteArray(4096, 'x')));
  MutationRequest request = sourceRequest(MutationKind::Copy, source, destination);
  bool removed = false;
  const MutationResult result = backend.execute(
      request, std::make_shared<std::atomic_bool>(false),
      [&source, &removed](const MutationProgress &) {
        if (!removed) {
          removed = QFile::remove(source);
        }
      });
  QVERIFY(removed);
  QCOMPARE(result.error, MutationError::Vanished);
  QVERIFY(!QFileInfo::exists(destination));
}

void TestLocalMutationBackend::symlinkEscapeAndNestedSymlinkCopyAreRejected() {
  QTemporaryDir fixture;
  QVERIFY(fixture.isValid());
  LocalMutationBackend backend(fixture.filePath(QStringLiteral("Trash")));
  const auto cancellation = std::make_shared<std::atomic_bool>(false);
  const QString outside = fixture.filePath(QStringLiteral("outside"));
  const QString root = fixture.filePath(QStringLiteral("root"));
  QVERIFY(QDir().mkdir(outside));
  QVERIFY(QDir().mkdir(root));
  QVERIFY(writeFile(QDir(outside).filePath(QStringLiteral("secret"))));
  QVERIFY(QFile::link(outside, QDir(root).filePath(QStringLiteral("escape"))));
  const QString escaped = QDir(root).filePath(QStringLiteral("escape/secret"));
  MutationRequest rename = sourceRequest(
      MutationKind::Rename, escaped, QDir(root).filePath(QStringLiteral("stolen")));
  rename.declaredRoots = {root};
  QCOMPARE(backend.execute(rename, cancellation, {}).error,
           MutationError::SymlinkEscape);

  const QString tree = QDir(root).filePath(QStringLiteral("tree"));
  QVERIFY(QDir().mkdir(tree));
  QVERIFY(QFile::link(QDir(outside).filePath(QStringLiteral("secret")),
                      QDir(tree).filePath(QStringLiteral("link"))));
  MutationRequest copy = sourceRequest(
      MutationKind::Copy, tree, QDir(root).filePath(QStringLiteral("copy")));
  QCOMPARE(backend.execute(copy, cancellation, {}).error,
           MutationError::SymlinkEscape);
  QVERIFY(!QFileInfo::exists(copy.destinationPath));
}

void TestLocalMutationBackend::nestedDirectorySwapCannotRedirectCopy() {
  QTemporaryDir fixture;
  QVERIFY(fixture.isValid());
  LocalMutationBackend backend(fixture.filePath(QStringLiteral("Trash")));
  const QString tree = fixture.filePath(QStringLiteral("tree"));
  const QString nested = QDir(tree).filePath(QStringLiteral("nested"));
  const QString movedNested = QDir(tree).filePath(QStringLiteral("nested-old"));
  const QString outside = fixture.filePath(QStringLiteral("outside"));
  QVERIFY(QDir().mkpath(nested));
  QVERIFY(QDir().mkdir(outside));
  QVERIFY(writeFile(QDir(nested).filePath(QStringLiteral("inside")),
                    QByteArray(256 * 1024, 'i')));
  QVERIFY(writeFile(QDir(outside).filePath(QStringLiteral("secret")), "secret"));
  MutationRequest request = sourceRequest(
      MutationKind::Copy, tree, fixture.filePath(QStringLiteral("copy")));
  bool swapped = false;
  const MutationResult result = backend.execute(
      request, std::make_shared<std::atomic_bool>(false),
      [&](const MutationProgress &) {
        if (!swapped) {
          QVERIFY(QDir().rename(nested, movedNested));
          QVERIFY(QFile::link(outside, nested));
          swapped = true;
        }
      });
  QVERIFY(swapped);
  QCOMPARE(result.error, MutationError::Changed);
  QVERIFY(!QFileInfo::exists(request.destinationPath));
  QVERIFY(QFileInfo::exists(QDir(outside).filePath(QStringLiteral("secret"))));
}

void TestLocalMutationBackend::destinationParentSwapIsRejected() {
  QTemporaryDir fixture;
  QVERIFY(fixture.isValid());
  LocalMutationBackend backend(fixture.filePath(QStringLiteral("Trash")));
  const QString source = fixture.filePath(QStringLiteral("source"));
  const QString destinationParent = fixture.filePath(QStringLiteral("destination"));
  const QString movedParent = fixture.filePath(QStringLiteral("destination-old"));
  QVERIFY(writeFile(source));
  QVERIFY(QDir().mkdir(destinationParent));
  MutationRequest request = sourceRequest(
      MutationKind::Copy, source,
      QDir(destinationParent).filePath(QStringLiteral("copy")));
  QVERIFY(QDir().rename(destinationParent, movedParent));
  QVERIFY(QDir().mkdir(destinationParent));

  const MutationResult result = backend.execute(
      request, std::make_shared<std::atomic_bool>(false), {});
  QCOMPARE(result.error, MutationError::Changed);
  QVERIFY(!QFileInfo::exists(request.destinationPath));
  QVERIFY(!QFileInfo::exists(QDir(movedParent).filePath(QStringLiteral("copy"))));
}

void TestLocalMutationBackend::preCancelledCopyDoesNotCreateDestination() {
  QTemporaryDir fixture;
  QVERIFY(fixture.isValid());
  LocalMutationBackend backend(fixture.filePath(QStringLiteral("Trash")));
  const QString source = fixture.filePath(QStringLiteral("source"));
  QVERIFY(writeFile(source));
  MutationRequest request = sourceRequest(
      MutationKind::Copy, source, fixture.filePath(QStringLiteral("copy")));
  const auto cancelled = std::make_shared<std::atomic_bool>(true);
  QCOMPARE(backend.execute(request, cancelled, {}).error, MutationError::Cancelled);
  QVERIFY(!QFileInfo::exists(request.destinationPath));
}

void TestLocalMutationBackend::cancellationDuringCopyRemovesPartialDestination() {
  QTemporaryDir fixture;
  QVERIFY(fixture.isValid());
  LocalMutationBackend backend(fixture.filePath(QStringLiteral("Trash")));
  const QString source = fixture.filePath(QStringLiteral("source"));
  QVERIFY(writeFile(source, QByteArray(256 * 1024, 'x')));
  MutationRequest request = sourceRequest(
      MutationKind::Copy, source, fixture.filePath(QStringLiteral("copy")));
  const auto cancellation = std::make_shared<std::atomic_bool>(false);
  int callbacks = 0;
  const MutationResult result = backend.execute(
      request, cancellation, [&cancellation, &callbacks](const MutationProgress &) {
        ++callbacks;
        cancellation->store(true);
      });
  QCOMPARE(result.error, MutationError::Cancelled);
  QVERIFY(callbacks > 0);
  QVERIFY(!QFileInfo::exists(request.destinationPath));
}

void TestLocalMutationBackend::permissionDeniedCreateIsTyped() {
#ifdef Q_OS_UNIX
  if (::geteuid() == 0) {
    QSKIP("root bypasses POSIX permission checks");
  }
  QTemporaryDir fixture;
  QVERIFY(fixture.isValid());
  const QString parent = fixture.filePath(QStringLiteral("locked"));
  QVERIFY(QDir().mkdir(parent));
  LocalMutationBackend backend(fixture.filePath(QStringLiteral("Trash")));
  MutationRequest request;
  request.kind = MutationKind::CreateFolder;
  request.destinationPath = QDir(parent).filePath(QStringLiteral("child"));
  request.declaredRoots = {parent};
  QVERIFY(QFile::setPermissions(parent, QFileDevice::ReadOwner | QFileDevice::ExeOwner));
  request.expectedParent = LocalMutationBackend::identityForPath(parent);
  const MutationResult result = backend.execute(
      request, std::make_shared<std::atomic_bool>(false), {});
  QFile::setPermissions(parent, QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                                    QFileDevice::ExeOwner);
  QCOMPARE(result.error, MutationError::PermissionDenied);
#else
  QSKIP("POSIX permissions are not exercised on this platform");
#endif
}

QTEST_APPLESS_MAIN(TestLocalMutationBackend)
#include "tst_local_mutation_backend.moc"
