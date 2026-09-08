// SPDX-License-Identifier: GPL-3.0-or-later
#include "model/local_directory_lister.h"
#include "preview/preview_provider.h"
#include <QQuickTextureFactory>
#include <QSemaphore>
#include <QTemporaryDir>
#include <QtTest>
using namespace QindaQt::Apps::FileManager;
namespace {
class BlockingDecoder final : public PreviewDecoder {
public:
  mutable std::atomic_int active{0}, peak{0}, calls{0};
  mutable QSemaphore release;
  QImage decode(const DirectoryEntry &,
                const std::atomic_bool &cancelled) const override {
    ++calls;
    const int count = ++active;
    peak.store(std::max(peak.load(), count));
    while (!cancelled && !release.tryAcquire(1, 10)) {
    }
    --active;
    QImage image(32, 32, QImage::Format_RGB32);
    image.fill(Qt::red);
    return image;
  }
};
QString idFor(const DirectoryEntry &entry, quint64 generation) {
  return previewUrl(entry, generation)
      .mid(QStringLiteral("image://previews/").size());
}
QImage responseImage(QQuickImageResponse *response) {
  std::unique_ptr<QQuickTextureFactory> factory(response->textureFactory());
  return factory->image();
}
} // namespace
class PreviewProviderTest : public QObject {
  Q_OBJECT
private slots:
  void boundsConcurrencyAndCancelsObsoleteGeneration();
  void cacheRejectsReplacedFile();
};
void PreviewProviderTest::boundsConcurrencyAndCancelsObsoleteGeneration() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  QImage source(20, 20, QImage::Format_RGB32);
  source.fill(Qt::blue);
  QVERIFY(source.save(dir.filePath("image.png")));
  const auto entry = LocalDirectoryLister().list(dir.path()).entries.first();
  auto decoder = std::make_unique<BlockingDecoder>();
  auto *fake = decoder.get();
  PreviewProvider provider(std::move(decoder));
  provider.setGeneration(1);
  QList<QQuickImageResponse *> responses;
  std::vector<std::unique_ptr<QSignalSpy>> spies;
  for (int i = 0; i < 8; ++i) {
    auto *response = provider.requestImageResponse(idFor(entry, 1), {});
    responses.append(response);
    spies.push_back(
        std::make_unique<QSignalSpy>(response, &QQuickImageResponse::finished));
  }
  QTRY_COMPARE(fake->active.load(), 2);
  QCOMPARE(fake->peak.load(), 2);
  provider.setGeneration(2);
  for (const auto &spy : spies)
    QTRY_COMPARE(spy->count(), 1);
  for (auto *response : responses) {
    QCOMPARE(responseImage(response).size(), QSize(1, 1));
    delete response;
  }
  QCOMPARE(fake->calls.load(), 2);
}
void PreviewProviderTest::cacheRejectsReplacedFile() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const auto path = dir.filePath("image.png");
  QImage source(20, 20, QImage::Format_RGB32);
  source.fill(Qt::blue);
  QVERIFY(source.save(path));
  const auto entry = LocalDirectoryLister().list(dir.path()).entries.first();
  PreviewProvider provider(std::make_unique<LocalPreviewDecoder>());
  provider.setGeneration(1);
  auto *first = provider.requestImageResponse(idFor(entry, 1), {});
  QSignalSpy firstSpy(first, &QQuickImageResponse::finished);
  QTRY_COMPARE(firstSpy.count(), 1);
  QCOMPARE(responseImage(first).pixelColor(0, 0), QColor(Qt::blue));
  delete first;
  QVERIFY(QFile::remove(path));
  source.fill(Qt::green);
  QVERIFY(source.save(path));
  auto *stale = provider.requestImageResponse(idFor(entry, 1), {});
  QSignalSpy staleSpy(stale, &QQuickImageResponse::finished);
  QTRY_COMPARE(staleSpy.count(), 1);
  QCOMPARE(responseImage(stale).size(), QSize(1, 1));
  delete stale;
}
QTEST_MAIN(PreviewProviderTest)
#include "tst_preview_provider.moc"
