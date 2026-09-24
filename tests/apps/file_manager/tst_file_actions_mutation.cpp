// SPDX-License-Identifier: GPL-3.0-or-later
#include "mutation/archive_codec.h"
#include "mutation/karchive_codec.h"
#include "mutation/local_mutation_backend.h"
#include "mutation/mutation_controller.h"

#include <KTar>
#include <KZip>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QVariantMap>
#include <QtTest>

#include <memory>

using namespace QindaQt::Apps::FileManager;

// ADR-0269: the right-click set's file operations, each through the real
// MutationController, LocalMutationBackend and KArchive codec, on temporary
// files only.

namespace {

[[nodiscard]] bool writeFile(const QString &path, const QByteArray &data = "payload") {
  QFile file(path);
  return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
}

[[nodiscard]] QByteArray readFile(const QString &path) {
  QFile file(path);
  return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray("<unreadable>");
}

// The QML entry snapshot of one real item: its path plus the decimal-string
// identity MutationController::identityFromMap consumes.
[[nodiscard]] QVariantMap snapshotOf(const QString &path) {
  const auto identity = LocalMutationBackend::identityForPath(path);
  if (!identity) {
    return {};
  }
  const QFileInfo info(path);
  return {{QStringLiteral("path"), info.absoluteFilePath()},
          {QStringLiteral("name"), info.fileName()},
          {QStringLiteral("isDirectory"), info.isDir()},
          {QStringLiteral("device"), QString::number(identity->device)},
          {QStringLiteral("inode"), QString::number(identity->inode)},
          {QStringLiteral("identitySize"), QString::number(identity->size)},
          {QStringLiteral("modifiedNanoseconds"), QString::number(identity->modifiedNanoseconds)},
          {QStringLiteral("mode"), QString::number(identity->mode)}};
}

// One MutationController over the production backend and codec, with its
// home Trash inside the temporary directory.
struct Fixture final {
  Fixture()
      : root(QDir(temporary.path()).canonicalPath()),
        trashRoot(root + QStringLiteral("/data/Trash")),
        controller(std::make_unique<LocalMutationBackend>(
            trashRoot, std::make_shared<LocalDeviceResolver>(),
            std::make_shared<KArchiveCodec>())) {}

  [[nodiscard]] QString path(const QString &relative) const {
    return root + QLatin1Char('/') + relative;
  }

  QTemporaryDir temporary;
  QString root;
  QString trashRoot;
  MutationController controller;
};

// Waits for the one running operation; true when it succeeded.
#define FINISH(fixture)                                                          \
  QTRY_VERIFY_WITH_TIMEOUT(!(fixture).controller.busy(), 10000);                 \
  QVERIFY2((fixture).controller.failureCode() == QLatin1String("none"),          \
           qPrintable((fixture).controller.failureMessage()))

} // namespace

class FileActionsMutationTest final : public QObject {
  Q_OBJECT

private slots:
  void archiveNamesAreRecognisedBySuffix();
  void duplicateCopiesBesideTheOriginalUnderAFreeName();
  void makeLinkPointsAtTheSiblingByName();
  void deletePermanentlyRemovesTreesButNotChangedItems();
  void putBackReturnsTrashedItemsToTheirFolder();
  void deletingInsideTrashForgetsTheRecord();
  void newFileCreatesAnEmptyFileOrACopyOfATemplate();
  void compressThenExtractRoundTripsATree();
  void extractReadsCompressedTarballs();
  void extractNeverWritesOutsideItsFolder();
  void refusesWhatCannotBeArchived();
};

void FileActionsMutationTest::archiveNamesAreRecognisedBySuffix() {
  QCOMPARE(archiveFormatForName(QStringLiteral("photos.zip")), ArchiveFormat::Zip);
  QCOMPARE(archiveFormatForName(QStringLiteral("PHOTOS.ZIP")), ArchiveFormat::Zip);
  QCOMPARE(archiveFormatForName(QStringLiteral("src.tar")), ArchiveFormat::Tar);
  QCOMPARE(archiveFormatForName(QStringLiteral("src.tar.gz")), ArchiveFormat::TarGzip);
  QCOMPARE(archiveFormatForName(QStringLiteral("src.tgz")), ArchiveFormat::TarGzip);
  QCOMPARE(archiveFormatForName(QStringLiteral("src.tar.bz2")), ArchiveFormat::TarBzip2);
  QCOMPARE(archiveFormatForName(QStringLiteral("src.tar.xz")), ArchiveFormat::TarXz);
  QCOMPARE(archiveFormatForName(QStringLiteral("src.tar.zst")), ArchiveFormat::TarZstd);
  QCOMPARE(archiveFormatForName(QStringLiteral("notes.txt")), ArchiveFormat::None);
  QCOMPARE(archiveFormatForName(QStringLiteral("single.gz")), ArchiveFormat::None);
  QCOMPARE(archiveFormatForName(QStringLiteral(".zip")), ArchiveFormat::None);
  QCOMPARE(archiveBaseName(QStringLiteral("photos.tar.gz")), QStringLiteral("photos"));
  QCOMPARE(archiveBaseName(QStringLiteral("Photos.ZIP")), QStringLiteral("Photos"));
  QCOMPARE(archiveBaseName(QStringLiteral("notes.txt")), QStringLiteral("notes.txt"));
  Fixture fixture;
  QVERIFY(fixture.controller.isExtractable(QStringLiteral("/any/where/bundle.tar.xz")));
  QVERIFY(!fixture.controller.isExtractable(QStringLiteral("/any/where/bundle.txt")));
}

void FileActionsMutationTest::duplicateCopiesBesideTheOriginalUnderAFreeName() {
  Fixture fixture;
  QVERIFY(QDir().mkpath(fixture.path(QStringLiteral("work/Photos"))));
  QVERIFY(writeFile(fixture.path(QStringLiteral("work/report.txt")), "report"));
  QVERIFY(writeFile(fixture.path(QStringLiteral("work/report copy.txt")), "taken"));
  QVERIFY(writeFile(fixture.path(QStringLiteral("work/Photos/inner.txt")), "inner"));
  // Two items bound for one folder: the second must not trip over the
  // folder time stamp the first one changed.
  QVERIFY(fixture.controller.duplicateItems(
      {snapshotOf(fixture.path(QStringLiteral("work/report.txt"))),
       snapshotOf(fixture.path(QStringLiteral("work/Photos")))}));
  FINISH(fixture);
  QCOMPARE(readFile(fixture.path(QStringLiteral("work/report copy 2.txt"))), QByteArray("report"));
  QCOMPARE(readFile(fixture.path(QStringLiteral("work/report copy.txt"))), QByteArray("taken"));
  QCOMPARE(readFile(fixture.path(QStringLiteral("work/Photos copy/inner.txt"))),
           QByteArray("inner"));
  QCOMPARE(readFile(fixture.path(QStringLiteral("work/report.txt"))), QByteArray("report"));
}

void FileActionsMutationTest::makeLinkPointsAtTheSiblingByName() {
  Fixture fixture;
  QVERIFY(QDir().mkpath(fixture.path(QStringLiteral("work/Music"))));
  QVERIFY(writeFile(fixture.path(QStringLiteral("work/song.ogg"))));
  QVERIFY(fixture.controller.makeLinks({snapshotOf(fixture.path(QStringLiteral("work/song.ogg"))),
                                        snapshotOf(fixture.path(QStringLiteral("work/Music")))}));
  FINISH(fixture);
  const QFileInfo fileLink(fixture.path(QStringLiteral("work/Link to song.ogg")));
  const QFileInfo folderLink(fixture.path(QStringLiteral("work/Link to Music")));
  QVERIFY(fileLink.isSymLink());
  QVERIFY(folderLink.isSymLink());
  // A sibling name, so the link survives moving the folder that holds both.
  QCOMPARE(fileLink.readSymLink(), QStringLiteral("song.ogg"));
  QCOMPARE(folderLink.readSymLink(), QStringLiteral("Music"));
  QVERIFY(folderLink.isDir());
}

void FileActionsMutationTest::deletePermanentlyRemovesTreesButNotChangedItems() {
  Fixture fixture;
  QVERIFY(QDir().mkpath(fixture.path(QStringLiteral("work/tree/branch"))));
  QVERIFY(writeFile(fixture.path(QStringLiteral("work/tree/branch/leaf.txt"))));
  QVERIFY(writeFile(fixture.path(QStringLiteral("work/single.txt"))));
  QVERIFY(QFile::link(fixture.path(QStringLiteral("work/single.txt")),
                      fixture.path(QStringLiteral("work/tree/link"))));
  QVERIFY(fixture.controller.deleteItems({snapshotOf(fixture.path(QStringLiteral("work/tree"))),
                                          snapshotOf(fixture.path(QStringLiteral("work/single.txt")))}));
  FINISH(fixture);
  QVERIFY(!QFileInfo::exists(fixture.path(QStringLiteral("work/tree"))));
  QVERIFY(!QFileInfo::exists(fixture.path(QStringLiteral("work/single.txt"))));
  // Nothing went to Trash.
  QVERIFY(!QFileInfo::exists(fixture.trashRoot + QStringLiteral("/files")));

  // An item that changed after the user saw it is not deleted.
  const QString changed = fixture.path(QStringLiteral("work/changed.txt"));
  QVERIFY(writeFile(changed, "before"));
  const QVariantMap seen = snapshotOf(changed);
  QVERIFY(writeFile(changed, "after, and longer"));
  QVERIFY(fixture.controller.deleteItems({seen}));
  QTRY_VERIFY_WITH_TIMEOUT(!fixture.controller.busy(), 10000);
  QCOMPARE(fixture.controller.failureCode(), QStringLiteral("changed"));
  QVERIFY(QFileInfo::exists(changed));
}

void FileActionsMutationTest::putBackReturnsTrashedItemsToTheirFolder() {
  Fixture fixture;
  QVERIFY(QDir().mkpath(fixture.path(QStringLiteral("work/sub"))));
  const QString first = fixture.path(QStringLiteral("work/first.txt"));
  const QString second = fixture.path(QStringLiteral("work/second.txt"));
  const QString nested = fixture.path(QStringLiteral("work/sub/nested.txt"));
  for (const QString &path : {first, second, nested}) {
    QVERIFY(writeFile(path, path.toUtf8()));
  }
  QVERIFY(fixture.controller.trashItems({snapshotOf(first), snapshotOf(second), snapshotOf(nested)}));
  FINISH(fixture);
  const QString files = fixture.trashRoot + QStringLiteral("/files/");
  QVERIFY(QFileInfo::exists(files + QStringLiteral("first.txt")));

  // Two items back into one folder, in one request.
  QVERIFY(fixture.controller.putBackItems({snapshotOf(files + QStringLiteral("first.txt")),
                                           snapshotOf(files + QStringLiteral("second.txt"))}));
  FINISH(fixture);
  QCOMPARE(readFile(first), first.toUtf8());
  QCOMPARE(readFile(second), second.toUtf8());
  QVERIFY(!QFileInfo::exists(files + QStringLiteral("first.txt")));
  QVERIFY(!QFileInfo::exists(fixture.trashRoot + QStringLiteral("/info/first.txt.trashinfo")));

  // An item whose folder is gone stays in Trash, and says why.
  QVERIFY(QDir(fixture.path(QStringLiteral("work/sub"))).removeRecursively());
  QVERIFY(!fixture.controller.putBackItems({snapshotOf(files + QStringLiteral("nested.txt"))}));
  QCOMPARE(fixture.controller.failureCode(), QStringLiteral("vanished"));
  QVERIFY(QFileInfo::exists(files + QStringLiteral("nested.txt")));

  // Something that is not in Trash has no record to go back to.
  QVERIFY(writeFile(fixture.path(QStringLiteral("work/loose.txt"))));
  QVERIFY(!fixture.controller.putBackItems({snapshotOf(fixture.path(QStringLiteral("work/loose.txt")))}));
  QCOMPARE(fixture.controller.failureCode(), QStringLiteral("invalid-request"));
}

void FileActionsMutationTest::deletingInsideTrashForgetsTheRecord() {
  Fixture fixture;
  QVERIFY(QDir().mkpath(fixture.path(QStringLiteral("work"))));
  const QString doomed = fixture.path(QStringLiteral("work/doomed.txt"));
  QVERIFY(writeFile(doomed));
  QVERIFY(fixture.controller.trashItem(doomed, snapshotOf(doomed)));
  FINISH(fixture);
  const QString payload = fixture.trashRoot + QStringLiteral("/files/doomed.txt");
  const QString record = fixture.trashRoot + QStringLiteral("/info/doomed.txt.trashinfo");
  QVERIFY(QFileInfo::exists(record));
  QVERIFY(fixture.controller.deleteItems({snapshotOf(payload)}));
  FINISH(fixture);
  QVERIFY(!QFileInfo::exists(payload));
  QVERIFY(!QFileInfo::exists(record));
}

void FileActionsMutationTest::newFileCreatesAnEmptyFileOrACopyOfATemplate() {
  Fixture fixture;
  const QString folder = fixture.path(QStringLiteral("work"));
  QVERIFY(QDir().mkpath(folder));
  QVERIFY(QDir().mkpath(fixture.path(QStringLiteral("Templates"))));
  const QString letter = fixture.path(QStringLiteral("Templates/Letter.txt"));
  QVERIFY(writeFile(letter, "Dear ..."));

  QVERIFY(fixture.controller.createFile(folder, QStringLiteral("Untitled")));
  FINISH(fixture);
  QCOMPARE(QFileInfo(folder + QStringLiteral("/Untitled")).size(), qint64(0));
  QVERIFY(QFileInfo(folder + QStringLiteral("/Untitled")).isFile());
  // Like New Folder, an empty new file can be undone (it goes to Trash).
  QVERIFY(fixture.controller.canUndo());
  QVERIFY(fixture.controller.undo());
  FINISH(fixture);
  QVERIFY(!QFileInfo::exists(folder + QStringLiteral("/Untitled")));

  QVERIFY(fixture.controller.createFile(folder, QStringLiteral("To Sam.txt"), letter));
  FINISH(fixture);
  QCOMPARE(readFile(folder + QStringLiteral("/To Sam.txt")), QByteArray("Dear ..."));

  // A taken name is refused, never replaced; a path is not a name.
  QVERIFY(fixture.controller.createFile(folder, QStringLiteral("To Sam.txt")));
  QTRY_VERIFY_WITH_TIMEOUT(!fixture.controller.busy(), 10000);
  QCOMPARE(fixture.controller.failureCode(), QStringLiteral("already-exists"));
  QCOMPARE(readFile(folder + QStringLiteral("/To Sam.txt")), QByteArray("Dear ..."));
  QVERIFY(!fixture.controller.createFile(folder, QStringLiteral("../escape")));
  QCOMPARE(fixture.controller.failureCode(), QStringLiteral("invalid-request"));
}

void FileActionsMutationTest::compressThenExtractRoundTripsATree() {
  Fixture fixture;
  QVERIFY(QDir().mkpath(fixture.path(QStringLiteral("work/project/sub"))));
  QVERIFY(writeFile(fixture.path(QStringLiteral("work/project/a.txt")), "alpha"));
  QVERIFY(writeFile(fixture.path(QStringLiteral("work/project/sub/b.txt")), "beta"));
  QVERIFY(QFile::link(QStringLiteral("a.txt"), fixture.path(QStringLiteral("work/project/link"))));

  QVERIFY(fixture.controller.compressItems({snapshotOf(fixture.path(QStringLiteral("work/project")))}));
  FINISH(fixture);
  const QString archive = fixture.path(QStringLiteral("work/project.zip"));
  QVERIFY(QFileInfo(archive).isFile());

  // The archive holds one folder, so Extract unpacks its contents into the
  // new folder instead of nesting "project" inside "project 2".
  QVERIFY(fixture.controller.extractItems({snapshotOf(archive)}));
  FINISH(fixture);
  const QString out = fixture.path(QStringLiteral("work/project 2"));
  QCOMPARE(readFile(out + QStringLiteral("/a.txt")), QByteArray("alpha"));
  QCOMPARE(readFile(out + QStringLiteral("/sub/b.txt")), QByteArray("beta"));
  QVERIFY(QFileInfo(out + QStringLiteral("/link")).isSymLink());
  QCOMPARE(QFileInfo(out + QStringLiteral("/link")).readSymLink(), QStringLiteral("a.txt"));

  // Several items make "Archive.zip" beside the first one.
  QVERIFY(writeFile(fixture.path(QStringLiteral("work/c.txt")), "gamma"));
  QVERIFY(fixture.controller.compressItems(
      {snapshotOf(fixture.path(QStringLiteral("work/c.txt"))),
       snapshotOf(fixture.path(QStringLiteral("work/project/sub")))}));
  FINISH(fixture);
  KZip zip(fixture.path(QStringLiteral("work/Archive.zip")));
  QVERIFY(zip.open(QIODevice::ReadOnly));
  QVERIFY(zip.directory()->entry(QStringLiteral("c.txt")) != nullptr);
  QVERIFY(zip.directory()->entry(QStringLiteral("sub")) != nullptr);
  QVERIFY(zip.directory()->entry(QStringLiteral("sub"))->isDirectory());
}

void FileActionsMutationTest::extractReadsCompressedTarballs() {
  Fixture fixture;
  QVERIFY(QDir().mkpath(fixture.path(QStringLiteral("work"))));
  const QString tarball = fixture.path(QStringLiteral("work/bundle.tar.gz"));
  {
    KTar tar(tarball, QStringLiteral("application/x-compressed-tar"));
    QVERIFY(tar.open(QIODevice::WriteOnly));
    QVERIFY(tar.writeFile(QStringLiteral("one.txt"), QByteArray("first")));
    QVERIFY(tar.writeFile(QStringLiteral("two/three.txt"), QByteArray("third")));
    QVERIFY(tar.close());
  }
  QVERIFY(fixture.controller.extractItems({snapshotOf(tarball)}));
  FINISH(fixture);
  // Two entries at the root: they land directly in the new folder.
  QCOMPARE(readFile(fixture.path(QStringLiteral("work/bundle/one.txt"))), QByteArray("first"));
  QCOMPARE(readFile(fixture.path(QStringLiteral("work/bundle/two/three.txt"))),
           QByteArray("third"));
}

void FileActionsMutationTest::extractNeverWritesOutsideItsFolder() {
  Fixture fixture;
  QVERIFY(QDir().mkpath(fixture.path(QStringLiteral("work"))));
  const QString archive = fixture.path(QStringLiteral("work/trap.zip"));
  {
    KZip zip(archive);
    QVERIFY(zip.open(QIODevice::WriteOnly));
    QVERIFY(zip.writeFile(QStringLiteral("ok.txt"), QByteArray("fine")));
    // Whether KArchive keeps or drops this name, it must never land beside
    // the archive or above it.
    zip.writeFile(QStringLiteral("../escaped.txt"), QByteArray("escaped"));
    QVERIFY(zip.close());
  }
  QVERIFY(fixture.controller.extractItems({snapshotOf(archive)}));
  QTRY_VERIFY_WITH_TIMEOUT(!fixture.controller.busy(), 10000);
  QVERIFY(!QFileInfo::exists(fixture.path(QStringLiteral("escaped.txt"))));
  QVERIFY(!QFileInfo::exists(fixture.path(QStringLiteral("work/escaped.txt"))));
  if (fixture.controller.failureCode() != QLatin1String("none")) {
    // A refused archive leaves nothing half-extracted behind.
    QVERIFY(!QFileInfo::exists(fixture.path(QStringLiteral("work/trap"))));
  }

  // A cancelled Extract through the backend also leaves nothing behind.
  LocalMutationBackend backend(fixture.trashRoot, std::make_shared<LocalDeviceResolver>(),
                               std::make_shared<KArchiveCodec>());
  MutationRequest request;
  request.kind = MutationKind::Extract;
  request.sourcePath = archive;
  request.destinationPath = fixture.path(QStringLiteral("work/cancelled"));
  request.declaredRoots = {fixture.path(QStringLiteral("work"))};
  request.expectedSource = LocalMutationBackend::identityForPath(archive);
  request.expectedParent = LocalMutationBackend::identityForPath(fixture.path(QStringLiteral("work")));
  const auto cancelled = std::make_shared<std::atomic_bool>(true);
  QCOMPARE(backend.execute(request, cancelled, {}).error, MutationError::Cancelled);
  QVERIFY(!QFileInfo::exists(request.destinationPath));
}

void FileActionsMutationTest::refusesWhatCannotBeArchived() {
  Fixture fixture;
  QVERIFY(QDir().mkpath(fixture.path(QStringLiteral("a"))));
  QVERIFY(QDir().mkpath(fixture.path(QStringLiteral("b"))));
  QVERIFY(writeFile(fixture.path(QStringLiteral("a/same.txt"))));
  QVERIFY(writeFile(fixture.path(QStringLiteral("b/same.txt"))));
  // Search results from two folders may share a name; one archive cannot.
  QVERIFY(!fixture.controller.compressItems({snapshotOf(fixture.path(QStringLiteral("a/same.txt"))),
                                             snapshotOf(fixture.path(QStringLiteral("b/same.txt")))}));
  QCOMPARE(fixture.controller.failureCode(), QStringLiteral("invalid-request"));
  QVERIFY(!fixture.controller.extractItems({snapshotOf(fixture.path(QStringLiteral("a/same.txt")))}));
  QCOMPARE(fixture.controller.failureCode(), QStringLiteral("unsupported"));
  // Without a codec (the Desktop's controller) archives are unsupported.
  LocalMutationBackend plain(fixture.trashRoot);
  MutationRequest request;
  request.kind = MutationKind::Compress;
  request.archiveSources = {fixture.path(QStringLiteral("a/same.txt"))};
  request.archiveSourceIdentities = {*LocalMutationBackend::identityForPath(request.archiveSources.first())};
  request.destinationPath = fixture.path(QStringLiteral("a/same.zip"));
  request.declaredRoots = {fixture.path(QStringLiteral("a"))};
  request.expectedParent = LocalMutationBackend::identityForPath(fixture.path(QStringLiteral("a")));
  QCOMPARE(plain.execute(request, std::make_shared<std::atomic_bool>(false), {}).error,
           MutationError::Unsupported);
}

QTEST_GUILESS_MAIN(FileActionsMutationTest)
#include "tst_file_actions_mutation.moc"
