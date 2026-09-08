// SPDX-License-Identifier: GPL-3.0-or-later
#include "model/local_directory_lister.h"
#include "preview/local_preview.h"
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>
using namespace QindaQt::Apps::FileManager;
class LocalPreviewTest : public QObject {
  Q_OBJECT
private slots:
  void decodesAndScalesRaster();
  void refusesChangedCancelledCorruptAndSymlink();
  void boundsBytesAndPixels();
  void classifiesIconsAndVersionsUrls();
};
void LocalPreviewTest::decodesAndScalesRaster() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  QImage source(800, 400, QImage::Format_RGB32);
  source.fill(Qt::red);
  QVERIFY(source.save(dir.filePath("photo.png")));
  const auto entry = LocalDirectoryLister().list(dir.path()).entries.first();
  std::atomic_bool cancelled{false};
  const auto preview = LocalPreviewDecoder().decode(entry, cancelled);
  QCOMPARE(preview.size(), QSize(192, 96));
  QCOMPARE(preview.pixelColor(10, 10), QColor(Qt::red));
}
void LocalPreviewTest::refusesChangedCancelledCorruptAndSymlink() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString path = dir.filePath("photo.png");
  QImage source(20, 20, QImage::Format_RGB32);
  source.fill(Qt::red);
  QVERIFY(source.save(path));
  const auto entry = LocalDirectoryLister().list(dir.path()).entries.first();
  std::atomic_bool cancelled{true};
  QVERIFY(LocalPreviewDecoder().decode(entry, cancelled).isNull());
  cancelled = false;
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write("not a PNG");
  file.close();
  QVERIFY(LocalPreviewDecoder().decode(entry, cancelled).isNull());
  QVERIFY(source.save(path));
  QVERIFY(file.open(QIODevice::ReadWrite));
  QVERIFY(file.resize(40));
  file.close();
  const auto truncated =
      LocalDirectoryLister().list(dir.path()).entries.first();
  QVERIFY(LocalPreviewDecoder().decode(truncated, cancelled).isNull());
  const auto corrupt = LocalDirectoryLister().list(dir.path()).entries.first();
  QVERIFY(LocalPreviewDecoder().decode(corrupt, cancelled).isNull());
  QVERIFY(QFile::link(path, dir.filePath("link.png")));
  for (const auto &item : LocalDirectoryLister().list(dir.path()).entries)
    if (item.isSymlink) {
      QVERIFY(LocalPreviewDecoder().decode(item, cancelled).isNull());
      QVERIFY(previewUrl(item, 1).isEmpty());
    }
}
void LocalPreviewTest::boundsBytesAndPixels() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  QFile file(dir.filePath("huge.bmp"));
  QVERIFY(file.open(QIODevice::WriteOnly));
  QVERIFY(file.resize(LocalPreviewDecoder::maximumBytes + 1));
  file.close();
  const auto large = LocalDirectoryLister().list(dir.path()).entries.first();
  std::atomic_bool cancelled{false};
  QVERIFY(LocalPreviewDecoder().decode(large, cancelled).isNull());
  QVERIFY(previewUrl(large, 1).isEmpty());
  // A tiny BMP header claims 40,010,000 pixels: reject dimensions before
  // allocation.
  QByteArray bmp(54, '\0');
  bmp[0] = 'B';
  bmp[1] = 'M';
  bmp[10] = 54;
  bmp[14] = 40;
  const auto put32 = [&bmp](int at, quint32 value) {
    for (int i = 0; i < 4; ++i)
      bmp[at + i] = static_cast<char>((value >> (8 * i)) & 255);
  };
  put32(18, 10000);
  put32(22, 4001);
  bmp[26] = 1;
  bmp[28] = 24;
  QVERIFY(file.open(QIODevice::WriteOnly));
  QCOMPARE(file.write(bmp), qint64(bmp.size()));
  file.close();
  const auto dimensions =
      LocalDirectoryLister().list(dir.path()).entries.first();
  QVERIFY(LocalPreviewDecoder().decode(dimensions, cancelled).isNull());
}
void LocalPreviewTest::classifiesIconsAndVersionsUrls() {
  DirectoryEntry entry;
  entry.name = "photo.png";
  entry.absolutePath = "/tmp/photo.png";
  entry.identitySize = 100;
  QCOMPARE(entryIconName(entry), QStringLiteral("image-x-generic"));
  QVERIFY(previewUrl(entry, 1) != previewUrl(entry, 2));
  const auto before = previewUrl(entry, 1);
  ++entry.modifiedNanoseconds;
  QVERIFY(previewUrl(entry, 1) != before);
  entry.isDirectory = true;
  QCOMPARE(entryIconName(entry), QStringLiteral("folder"));
  QVERIFY(previewUrl(entry, 1).isEmpty());
  entry.isDirectory = false;
  entry.name = "notes.txt";
  QCOMPARE(entryIconName(entry), QStringLiteral("text-x-generic"));
  QVERIFY(previewUrl(entry, 1).isEmpty());
}
QTEST_GUILESS_MAIN(LocalPreviewTest)
#include "tst_local_preview.moc"
