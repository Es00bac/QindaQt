// SPDX-License-Identifier: GPL-3.0-or-later
#include "fakes.h"
#include "model/navigation_controller.h"

#include <QSignalSpy>
#include <QTest>
#include <limits>

using namespace QindaQt::Apps::FileManager;
using namespace QindaQt::Apps::FileManager::Test;

class TestIconZoom final : public QObject {
  Q_OBJECT
private slots:
  void boundedStepsAndReset();
  void zoomPreservesListingAndFilter();
};

void TestIconZoom::boundedStepsAndReset() {
  NavigationController controller(std::make_unique<FakeDirectoryLister>(),
                                  std::make_unique<FakeFileLauncher>());
  QSignalSpy presentation(&controller, &NavigationController::presentationChanged);
  QCOMPARE(controller.iconSize(), 64);
  QVERIFY(controller.canZoomIn());
  QVERIFY(controller.canZoomOut());
  controller.zoomBy(-1);
  QCOMPARE(controller.iconSize(), 48);
  controller.zoomBy(-1);
  QCOMPARE(controller.iconSize(), 32);
  QVERIFY(!controller.canZoomOut());
  controller.zoomBy(std::numeric_limits<int>::min());
  QCOMPARE(presentation.count(), 2);
  controller.zoomBy(std::numeric_limits<int>::max());
  QCOMPARE(controller.iconSize(), 128);
  QVERIFY(!controller.canZoomIn());
  controller.zoomBy(1);
  controller.zoomBy(0);
  QCOMPARE(presentation.count(), 3);
  controller.zoomBy(-1);
  QCOMPARE(controller.iconSize(), 96);
  controller.resetZoom();
  QCOMPARE(controller.iconSize(), 64);
  QCOMPARE(presentation.count(), 5);
  controller.resetZoom();
  QCOMPARE(presentation.count(), 5);
}

void TestIconZoom::zoomPreservesListingAndFilter() {
  auto lister = std::make_unique<FakeDirectoryLister>();
  auto *observed = lister.get();
  DirectoryEntry entry;
  entry.name = QStringLiteral("image.png");
  entry.absolutePath = QStringLiteral("/folder/image.png");
  entry.device = 9007199254740993ULL;
  entry.inode = 9007199254740995ULL;
  entry.identitySize = 128;
  ListingResult result;
  result.entries = {entry};
  observed->setResult(QStringLiteral("/folder"), result);
  NavigationController controller(std::move(lister), std::make_unique<FakeFileLauncher>());
  controller.navigateTo(QStringLiteral("/folder"));
  controller.setNameFilter(QStringLiteral("image"));
  const auto entries = controller.entries();
  const auto generation = controller.listingGeneration();
  QSignalSpy changes(&controller, &NavigationController::entriesChanged);
  controller.zoomBy(1);
  controller.setViewMode(QStringLiteral("list"));
  controller.zoomBy(-2);
  QCOMPARE(controller.entries(), entries);
  QCOMPARE(controller.listingGeneration(), generation);
  QCOMPARE(controller.nameFilter(), QStringLiteral("image"));
  QCOMPARE(observed->requestedPaths().size(), 1);
  QCOMPARE(changes.count(), 0);
  controller.navigateTo(QStringLiteral("/"));
  QCOMPARE(controller.iconSize(), 48);
}

QTEST_GUILESS_MAIN(TestIconZoom)
#include "tst_icon_zoom.moc"
