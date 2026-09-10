// SPDX-License-Identifier: GPL-3.0-or-later
#include "model/entry_properties.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QMimeDatabase>
#include <QTemporaryDir>
#include <QTest>
#include <QVariantMap>

using namespace QindaQt::Apps::FileManager;

namespace {

[[nodiscard]] bool writeFile(const QString &path, const QByteArray &contents) {
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly)) {
    return false;
  }
  return file.write(contents) == contents.size();
}

// Builds the QML listing snapshot the dialog consumes. The permission bits
// arrive as a decimal string because that is how the listing identity map
// carries them across the QML boundary.
[[nodiscard]] QVariantMap entrySnapshot(const QString &path,
                                        const QString &decimalMode) {
  const QFileInfo info(path);
  return {{QStringLiteral("name"), info.fileName()},
          {QStringLiteral("path"), info.absoluteFilePath()},
          {QStringLiteral("isDirectory"), info.isDir()},
          {QStringLiteral("isSymlink"), info.isSymLink()},
          {QStringLiteral("size"), info.isDir() ? qint64(0) : info.size()},
          {QStringLiteral("modified"), info.lastModified()},
          {QStringLiteral("mode"), decimalMode}};
}

} // namespace

class TestEntryProperties final : public QObject {
  Q_OBJECT

private slots:
  void singleFileFieldsAreSynchronous();
  void folderTotalCountsContentsWithoutDescendingSymlinks();
  void multiSelectionReportsCountAndCombinedTotal();
  void reinspectFencesStaleWorkerTotals();
  void clearAndEmptyInspectResetState();
};

void TestEntryProperties::singleFileFieldsAreSynchronous() {
  QTemporaryDir fixture;
  QVERIFY(fixture.isValid());
  const QString filePath = fixture.filePath(QStringLiteral("note.txt"));
  QVERIFY(writeFile(filePath, QByteArray(11, 'x')));
  QVERIFY(QFile::setPermissions(filePath, QFileDevice::ReadOwner |
                                              QFileDevice::WriteOwner |
                                              QFileDevice::ReadGroup));
  const QDateTime modified = QFileInfo(filePath).lastModified();

  // 416 == 0640: the symbolic text below is the exact bit pattern rendered.
  QVariantMap map = entrySnapshot(filePath, QStringLiteral("416"));
  map.insert(QStringLiteral("kindText"), QStringLiteral("Plain text document"));

  EntryPropertiesController controller;
  controller.inspect({map});
  QVERIFY(controller.active());
  QCOMPARE(controller.entryCount(), 1);
  QCOMPARE(controller.name(), QStringLiteral("note.txt"));
  QCOMPARE(controller.kindText(), QStringLiteral("Plain text document"));
  QCOMPARE(controller.mimeText(),
           QMimeDatabase().mimeTypeForFile(QFileInfo(filePath)).name());
  QCOMPARE(controller.pathText(), QFileInfo(filePath).absoluteFilePath());
  QCOMPARE(controller.permissionsText(), QStringLiteral("-rw-r-----"));
  QCOMPARE(controller.sizeText(), QLocale().formattedDataSize(11));
  QCOMPARE(controller.modifiedText(),
           QLocale().toString(modified, QLocale::LongFormat));
  QVERIFY(!controller.computingTotal());
  QVERIFY(controller.totalSizeText().isEmpty());
  QVERIFY(!controller.totalTruncated());
}

void TestEntryProperties::folderTotalCountsContentsWithoutDescendingSymlinks() {
  QTemporaryDir fixture;
  QVERIFY(fixture.isValid());
  const QDir root(fixture.path());
  QVERIFY(root.mkpath(QStringLiteral("sub")));
  QVERIFY(writeFile(root.filePath(QStringLiteral("sub/data.bin")),
                    QByteArray(7, 'y')));
  QVERIFY(writeFile(root.filePath(QStringLiteral("note.txt")), "11-bytes..."));
  const QString linkPath = root.filePath(QStringLiteral("sub/link-up"));
  const QString linkDirPath = root.filePath(QStringLiteral("sub/link-dir"));
  QVERIFY(QFile::link(QStringLiteral("../note.txt"), linkPath));
  QVERIFY(QFile::link(QStringLiteral(".."), linkDirPath));

  EntryPropertiesController controller;
  controller.inspect(
      {entrySnapshot(root.filePath(QStringLiteral("sub")),
                     QStringLiteral("493"))});
  QVERIFY(controller.active());
  QCOMPARE(controller.name(), QStringLiteral("sub"));
  QCOMPARE(controller.mimeText(), QStringLiteral("inode/directory"));
  QVERIFY(controller.sizeText().isEmpty());
  QTRY_VERIFY_WITH_TIMEOUT(!controller.computingTotal(), 5000);

  // The walk counts each symlink's own size but never descends through one;
  // following link-dir back to the root would recount the whole tree (and a
  // cycle would never finish).
  const qint64 expected = 7 + QFileInfo(linkPath).size() +
                          QFileInfo(linkDirPath).size();
  QCOMPARE(controller.totalSizeText(), QLocale().formattedDataSize(expected));
  QVERIFY(!controller.totalTruncated());
}

void TestEntryProperties::multiSelectionReportsCountAndCombinedTotal() {
  QTemporaryDir fixture;
  QVERIFY(fixture.isValid());
  const QDir root(fixture.path());
  QVERIFY(root.mkpath(QStringLiteral("sub")));
  QVERIFY(writeFile(root.filePath(QStringLiteral("sub/data.bin")),
                    QByteArray(7, 'y')));
  QVERIFY(writeFile(root.filePath(QStringLiteral("note.txt")),
                    QByteArray(11, 'x')));

  EntryPropertiesController controller;
  controller.inspect(
      {entrySnapshot(root.filePath(QStringLiteral("note.txt")),
                     QStringLiteral("416")),
       entrySnapshot(root.filePath(QStringLiteral("sub")),
                     QStringLiteral("493"))});
  QVERIFY(controller.active());
  QCOMPARE(controller.entryCount(), 2);
  QCOMPARE(controller.name(), QStringLiteral("2 items"));
  QCOMPARE(controller.kindText(), QStringLiteral("Multiple items"));
  QVERIFY(controller.mimeText().isEmpty());
  QVERIFY(controller.pathText().isEmpty());
  QVERIFY(controller.modifiedText().isEmpty());
  QVERIFY(controller.permissionsText().isEmpty());
  QVERIFY(controller.sizeText().isEmpty());
  QTRY_VERIFY_WITH_TIMEOUT(!controller.computingTotal(), 5000);
  QCOMPARE(controller.totalSizeText(), QLocale().formattedDataSize(18));
  QVERIFY(!controller.totalTruncated());
}

void TestEntryProperties::reinspectFencesStaleWorkerTotals() {
  QTemporaryDir fixture;
  QVERIFY(fixture.isValid());
  const QDir root(fixture.path());
  QVERIFY(root.mkpath(QStringLiteral("sub")));
  QVERIFY(writeFile(root.filePath(QStringLiteral("sub/data.bin")), "7-bytes"));
  QVERIFY(writeFile(root.filePath(QStringLiteral("note.txt")), "11-bytes..."));

  EntryPropertiesController controller;
  controller.inspect(
      {entrySnapshot(root.filePath(QStringLiteral("sub")),
                     QStringLiteral("493"))});
  // Replacing the selection joins and disposes the in-flight walk; its queued
  // delivery must be fenced by the generation, never published.
  controller.inspect(
      {entrySnapshot(root.filePath(QStringLiteral("note.txt")),
                     QStringLiteral("416"))});
  QCOMPARE(controller.entryCount(), 1);
  QCOMPARE(controller.name(), QStringLiteral("note.txt"));
  QTest::qWait(300);
  QVERIFY(!controller.computingTotal());
  QVERIFY(controller.totalSizeText().isEmpty());
}

void TestEntryProperties::clearAndEmptyInspectResetState() {
  QTemporaryDir fixture;
  QVERIFY(fixture.isValid());
  const QDir root(fixture.path());
  QVERIFY(root.mkpath(QStringLiteral("sub")));
  QVERIFY(writeFile(root.filePath(QStringLiteral("sub/data.bin")), "7-bytes"));

  EntryPropertiesController controller;
  controller.inspect(
      {entrySnapshot(root.filePath(QStringLiteral("sub")),
                     QStringLiteral("493"))});
  controller.clear();
  QVERIFY(!controller.active());
  QCOMPARE(controller.entryCount(), 0);
  QTest::qWait(300);
  QVERIFY(!controller.computingTotal());
  QVERIFY(controller.totalSizeText().isEmpty());

  controller.inspect({});
  QVERIFY(!controller.active());
  QCOMPARE(controller.entryCount(), 0);
}

QTEST_GUILESS_MAIN(TestEntryProperties)
#include "tst_entry_properties.moc"
