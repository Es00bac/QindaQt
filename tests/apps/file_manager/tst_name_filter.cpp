// SPDX-License-Identifier: GPL-3.0-or-later
#include "fakes.h"
#include "model/navigation_controller.h"

#include <QSignalSpy>
#include <QTest>

using namespace QindaQt::Apps::FileManager;
using namespace QindaQt::Apps::FileManager::Test;

namespace {
DirectoryEntry entry(const QString &name, bool directory = false, bool hidden = false) {
  DirectoryEntry result;
  result.name = name;
  result.absolutePath = QStringLiteral("/folder/") + name;
  result.isDirectory = directory;
  result.isHidden = hidden;
  result.device = 9007199254740993ULL;
  result.inode = 9007199254740995ULL;
  result.identitySize = 128;
  return result;
}

ListingResult listing(QVector<DirectoryEntry> entries) {
  ListingResult result;
  result.entries = std::move(entries);
  return result;
}

struct Fixture {
  FakeDirectoryLister *lister = new FakeDirectoryLister;
  NavigationController controller{DirectoryListerPtr(lister),
                                  std::make_unique<FakeFileLauncher>()};
  Fixture() {
    lister->setResult(QStringLiteral("/folder"),
                      listing({entry(QStringLiteral("notes.txt")),
                               entry(QStringLiteral("Notes"), true),
                               entry(QStringLiteral(".notes"), false, true),
                               entry(QStringLiteral("ÉTÉ.jpg")),
                               entry(QStringLiteral("work[1].txt"))}));
    lister->setResult(QStringLiteral("/"), listing({}));
    controller.navigateTo(QStringLiteral("/folder"));
  }
};
} // namespace

class TestNameFilter final : public QObject {
  Q_OBJECT
private slots:
  void literalUnicodeAndHiddenOrdering();
  void noMatchesDoesNotClaimAnEmptyFolder();
  void filterRetainsIdentityWithoutRelisting();
  void refreshSortAndViewRetainFilter();
  void actualNavigationResetsFilter();
  void boundedInputAndIdempotence();
  void emptyAndFailedListingsRetainTheirStatus();
  void truncationAndHiddenNoticesCompose();
};

void TestNameFilter::literalUnicodeAndHiddenOrdering() {
  Fixture f;
  f.controller.setNameFilter(QStringLiteral("NOTE"));
  QCOMPARE(f.controller.entryCount(), 2);
  QCOMPARE(f.controller.entryAt(0)->name, QStringLiteral("Notes"));
  QCOMPARE(f.controller.entryAt(1)->name, QStringLiteral("notes.txt"));
  QCOMPARE(f.controller.statusMessage(), QStringLiteral("2 matching items; 1 hidden"));
  f.controller.setShowHidden(true);
  QCOMPARE(f.controller.entryCount(), 3);
  QCOMPARE(f.controller.entryAt(0)->name, QStringLiteral("Notes"));
  QCOMPARE(f.controller.entryAt(1)->name, QStringLiteral(".notes"));
  f.controller.setNameFilter(QStringLiteral("été"));
  QCOMPARE(f.controller.entryCount(), 1);
  QCOMPARE(f.controller.entryAt(0)->name, QStringLiteral("ÉTÉ.jpg"));
  QCOMPARE(f.controller.statusMessage(), QStringLiteral("1 matching item"));
  f.controller.setNameFilter(QStringLiteral("[1]"));
  QCOMPARE(f.controller.entryCount(), 1);
  QCOMPARE(f.controller.entryAt(0)->name, QStringLiteral("work[1].txt"));
  f.controller.setNameFilter(QStringLiteral("*.txt"));
  QCOMPARE(f.controller.entryCount(), 0);
}

void TestNameFilter::noMatchesDoesNotClaimAnEmptyFolder() {
  Fixture f;
  f.controller.setNameFilter(QStringLiteral("absent"));
  QCOMPARE(f.controller.statusKey(), QStringLiteral("ready"));
  QCOMPARE(f.controller.entryCount(), 0);
  QCOMPARE(f.controller.indexOfName(QStringLiteral("notes.txt")), -1);
  QCOMPARE(f.controller.statusMessage(), QStringLiteral("No matching items; 1 hidden"));
  f.controller.setNameFilter({});
  QCOMPARE(f.controller.entryCount(), 4);
  QCOMPARE(f.controller.statusMessage(), QStringLiteral("1 hidden"));
}

void TestNameFilter::filterRetainsIdentityWithoutRelisting() {
  Fixture f;
  const auto generation = f.controller.listingGeneration();
  const auto original = f.controller.entries().at(f.controller.indexOfName(QStringLiteral("ÉTÉ.jpg")));
  f.controller.setNameFilter(QStringLiteral("été"));
  QCOMPARE(f.controller.entries().at(0), original);
  QCOMPARE(f.controller.listingGeneration(), generation);
  QCOMPARE(f.lister->requestedPaths().size(), 1);
  QCOMPARE(f.controller.entryAt(0)->inode, 9007199254740995ULL);
}

void TestNameFilter::refreshSortAndViewRetainFilter() {
  Fixture f;
  f.controller.setNameFilter(QStringLiteral("note"));
  f.controller.setSortColumn(QStringLiteral("name"));
  f.controller.setDirectoriesFirst(false);
  f.controller.setViewMode(QStringLiteral("list"));
  QCOMPARE(f.controller.nameFilter(), QStringLiteral("note"));
  QCOMPARE(f.lister->requestedPaths().size(), 1);
  const auto generation = f.controller.listingGeneration();
  f.lister->setResult(QStringLiteral("/folder"), listing({entry(QStringLiteral("new note")),
                                                         entry(QStringLiteral("other"))}));
  f.controller.refresh();
  QCOMPARE(f.controller.nameFilter(), QStringLiteral("note"));
  QCOMPARE(f.controller.entryCount(), 1);
  QCOMPARE(f.controller.entryAt(0)->name, QStringLiteral("new note"));
  QCOMPARE(f.controller.listingGeneration(), generation + 1);
}

void TestNameFilter::actualNavigationResetsFilter() {
  Fixture f;
  f.controller.setNameFilter(QStringLiteral("note"));
  QSignalSpy presentation(&f.controller, &NavigationController::presentationChanged);
  f.controller.navigateTo(QStringLiteral("/folder/../folder"));
  f.controller.goBack();
  QCOMPARE(f.controller.nameFilter(), QStringLiteral("note"));
  QCOMPARE(presentation.count(), 0);
  f.controller.goUp();
  QCOMPARE(f.controller.nameFilter(), QString());
  QCOMPARE(presentation.count(), 1);
  f.controller.setNameFilter(QStringLiteral("other"));
  f.controller.goBack();
  QCOMPARE(f.controller.nameFilter(), QString());
  f.controller.setNameFilter(QStringLiteral("note"));
  f.controller.goForward();
  QCOMPARE(f.controller.nameFilter(), QString());
  f.controller.setNameFilter(QStringLiteral("note"));
  f.controller.navigateTo(QStringLiteral("/missing"));
  QCOMPARE(f.controller.nameFilter(), QString());
  QCOMPARE(f.controller.statusKey(), QStringLiteral("missing"));
}

void TestNameFilter::boundedInputAndIdempotence() {
  Fixture f;
  QSignalSpy presentation(&f.controller, &NavigationController::presentationChanged);
  QSignalSpy entries(&f.controller, &NavigationController::entriesChanged);
  const QString longFilter(1000, QLatin1Char('a'));
  f.controller.setNameFilter(longFilter);
  QCOMPARE(f.controller.nameFilter().size(), NavigationController::maximumNameFilterLength);
  QCOMPARE(f.controller.property("maximumNameFilterLength").toInt(), 256);
  f.controller.setNameFilter(longFilter);
  QCOMPARE(presentation.count(), 1);
  QCOMPARE(entries.count(), 1);
  const QString emoji = QString::fromUtf8("😀");
  f.controller.setNameFilter(QString(255, QLatin1Char('a')) + emoji);
  QCOMPARE(f.controller.nameFilter().size(), 255);
  f.controller.setNameFilter(emoji);
  QCOMPARE(f.controller.nameFilter(), emoji);
}

void TestNameFilter::emptyAndFailedListingsRetainTheirStatus() {
  Fixture f;
  f.controller.navigateTo(QStringLiteral("/"));
  f.controller.setNameFilter(QStringLiteral("note"));
  QCOMPARE(f.controller.statusKey(), QStringLiteral("empty"));
  QVERIFY(f.controller.statusMessage().isEmpty());
  f.controller.navigateTo(QStringLiteral("/missing"));
  const auto diagnostic = f.controller.statusMessage();
  QVERIFY(!diagnostic.isEmpty());
  f.controller.setNameFilter(QStringLiteral("note"));
  f.controller.setSortColumn(QStringLiteral("name"));
  f.controller.setShowHidden(true);
  QCOMPARE(f.controller.statusKey(), QStringLiteral("missing"));
  QCOMPARE(f.controller.statusMessage(), diagnostic);
}

void TestNameFilter::truncationAndHiddenNoticesCompose() {
  Fixture f;
  auto result = listing({entry(QStringLiteral(".secret"), false, true)});
  result.truncated = true;
  f.lister->setResult(QStringLiteral("/folder"), result);
  f.controller.refresh();
  f.controller.setNameFilter(QStringLiteral("secret"));
  QCOMPARE(f.controller.statusKey(), QStringLiteral("ready"));
  QCOMPARE(f.controller.statusMessage(),
           QStringLiteral("No matching items; Showing the first 1 entries; 1 hidden"));
  f.controller.setShowHidden(true);
  QCOMPARE(f.controller.entryCount(), 1);
}

QTEST_GUILESS_MAIN(TestNameFilter)
#include "tst_name_filter.moc"
