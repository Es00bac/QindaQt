// SPDX-License-Identifier: GPL-3.0-or-later
#include "fakes.h"
#include "model/navigation_controller.h"
#include "model/navigation_history.h"
#include "model/recents_place.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QUrl>
#include <QtTest>

#include <utility>

using namespace QindaQt::Apps::FileManager;

// ADR-0272: the Recents place reads the desktop's recently-used store (the
// freedesktop XBEL file GTK and KDE applications write) and is browsed by the
// ordinary NavigationController through its lister decorator. Every store
// here is a temporary fixture; the user's own store is never read.

namespace {

[[nodiscard]] bool touch(const QString &path) {
  QFile file(path);
  return file.open(QIODevice::WriteOnly) && file.write("recent") == 6;
}

// One <bookmark>, with GTK's microsecond stamps; an empty stamp is left out.
[[nodiscard]] QByteArray bookmark(const QByteArray &href, const char *added,
                                  const char *modified = "", const char *visited = "") {
  QByteArray row = "  <bookmark href=\"" + href + "\"";
  for (const auto &[name, value] : {std::pair{"added", added}, std::pair{"modified", modified},
                                    std::pair{"visited", visited}}) {
    if (*value != '\0') {
      row += QByteArray(" ") + name + "=\"" + value + "\"";
    }
  }
  return row + ">\n    <info><metadata owner=\"http://freedesktop.org\">"
               "<mime:mime-type type=\"text/plain\"/></metadata></info>\n  </bookmark>\n";
}

[[nodiscard]] QByteArray hrefOf(const QString &path) {
  return QUrl::fromLocalFile(path).toEncoded();
}

[[nodiscard]] bool writeStore(const QString &path, const QByteArray &bookmarks) {
  QFile file(path);
  return file.open(QIODevice::WriteOnly)
      && file.write("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                    "<xbel version=\"1.0\"\n"
                    "      xmlns:bookmark=\"http://www.freedesktop.org/standards/desktop-bookmarks\"\n"
                    "      xmlns:mime=\"http://www.freedesktop.org/standards/shared-mime-info\"\n>\n"
                    + bookmarks + "</xbel>\n") > 0;
}

[[nodiscard]] QStringList namesOf(const ListingResult &result) {
  QStringList names;
  for (const DirectoryEntry &entry : result.entries) {
    names.append(entry.name);
  }
  return names;
}

} // namespace

class RecentsPlaceTests final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void init();
  void aMissingStoreIsAnEmptyPlace();
  void existingLocalFilesAreListedNewestFirst();
  void theLimitKeepsTheNewestAndSaysSo();
  void aDamagedForeignOrOversizedStoreIsATypedError();
  void theListerAnswersOnlyTheRecentsLocation();
  void recentsIsARootPlaceTheWindowBrowses();

private:
  std::unique_ptr<QTemporaryDir> m_temporary;
  QString m_store;
  QString m_files;
};

void RecentsPlaceTests::init() {
  m_temporary = std::make_unique<QTemporaryDir>();
  QVERIFY(m_temporary->isValid());
  m_store = m_temporary->filePath(QStringLiteral("recently-used.xbel"));
  m_files = m_temporary->filePath(QStringLiteral("files"));
  QVERIFY(QDir().mkpath(m_files + QStringLiteral("/project")));
  for (const char *name : {"alpha.txt", "beta.txt", "my notes.txt"}) {
    QVERIFY(touch(m_files + QLatin1Char('/') + QString::fromLatin1(name)));
  }
}

void RecentsPlaceTests::aMissingStoreIsAnEmptyPlace() {
  const ListingResult result = readRecentFiles(m_store);
  QVERIFY2(result.ok(), qPrintable(result.diagnostic));
  QCOMPARE(result.path, RecentsLocation::location());
  QVERIFY(result.entries.isEmpty());
  QVERIFY(!result.truncated);
}

void RecentsPlaceTests::existingLocalFilesAreListedNewestFirst() {
  const QString alpha = m_files + QStringLiteral("/alpha.txt");
  QVERIFY(writeStore(
      m_store,
      // A bookmark's time is the latest of its three stamps.
      bookmark(hrefOf(m_files + QStringLiteral("/beta.txt")), "2026-01-01T09:00:00.000001Z",
               "2026-03-01T09:00:00.500000Z")
          + bookmark(hrefOf(m_files + QStringLiteral("/my notes.txt")),
                     "2026-02-01T09:00:00Z", "", "2026-02-02T09:00:00.176778Z")
          + bookmark(hrefOf(m_files + QStringLiteral("/project")), "2026-01-15T09:00:00Z")
          // Named twice: the later use wins.
          + bookmark(hrefOf(alpha), "2025-12-01T09:00:00Z")
          + bookmark(hrefOf(alpha), "2025-12-01T09:00:00Z", "", "2026-04-01T09:00:00Z")
          // Gone, remote, on another host, or not a URL at all: skipped.
          + bookmark(hrefOf(m_files + QStringLiteral("/deleted.txt")), "2026-05-01T09:00:00Z")
          + bookmark("https://example.org/page.html", "2026-05-01T09:00:00Z")
          + bookmark("file://elsewhere.example" + hrefOf(alpha).mid(7), "2026-05-01T09:00:00Z")
          + bookmark("", "2026-05-01T09:00:00Z")));

  const ListingResult result = readRecentFiles(m_store);
  QVERIFY2(result.ok(), qPrintable(result.diagnostic));
  QCOMPARE(namesOf(result), (QStringList{QStringLiteral("alpha.txt"), QStringLiteral("beta.txt"),
                                         QStringLiteral("my notes.txt"),
                                         QStringLiteral("project")}));
  QVERIFY(!result.truncated);
  // Each row is the entry its folder lists: absolute path, kind and the
  // identity a mutation checks.
  const DirectoryEntry &first = result.entries.constFirst();
  QCOMPARE(first.absolutePath, alpha);
  QVERIFY(!first.isDirectory);
  QVERIFY(first.inode != 0);
  QVERIFY(result.entries.at(3).isDirectory);
}

void RecentsPlaceTests::theLimitKeepsTheNewestAndSaysSo() {
  QVERIFY(writeStore(
      m_store, bookmark(hrefOf(m_files + QStringLiteral("/alpha.txt")), "2026-03-01T09:00:00Z")
                   + bookmark(hrefOf(m_files + QStringLiteral("/beta.txt")), "2026-02-01T09:00:00Z")
                   + bookmark(hrefOf(m_files + QStringLiteral("/my notes.txt")),
                              "2026-01-01T09:00:00Z")));
  const ListingResult result = readRecentFiles(m_store, 2);
  QVERIFY(result.ok());
  QCOMPARE(namesOf(result), (QStringList{QStringLiteral("alpha.txt"), QStringLiteral("beta.txt")}));
  QVERIFY(result.truncated);
}

void RecentsPlaceTests::aDamagedForeignOrOversizedStoreIsATypedError() {
  // Never a partial list: the place shows the failure instead.
  QFile damaged(m_store);
  QVERIFY(damaged.open(QIODevice::WriteOnly));
  QVERIFY(damaged.write("<?xml version=\"1.0\"?>\n<xbel version=\"1.0\">\n"
                        + bookmark(hrefOf(m_files + QStringLiteral("/alpha.txt")),
                                   "2026-03-01T09:00:00Z")
                        + "  <bookmark href=\"") > 0);
  damaged.close();
  ListingResult result = readRecentFiles(m_store);
  QCOMPARE(result.error, ListingError::Unknown);
  QVERIFY(result.entries.isEmpty());
  QVERIFY(!result.diagnostic.isEmpty());

  QVERIFY(damaged.open(QIODevice::WriteOnly | QIODevice::Truncate));
  QVERIFY(damaged.write("<?xml version=\"1.0\"?>\n<html><body/></html>\n") > 0);
  damaged.close();
  result = readRecentFiles(m_store);
  QCOMPARE(result.error, ListingError::Unknown);
  QVERIFY(result.entries.isEmpty());

  // Sparse, so the bound is exercised without writing sixteen megabytes.
  QVERIFY(damaged.resize(maximumRecentStoreBytes + 1));
  result = readRecentFiles(m_store);
  QCOMPARE(result.error, ListingError::Unknown);
  QVERIFY(result.diagnostic.contains(QStringLiteral("too large")));
}

void RecentsPlaceTests::theListerAnswersOnlyTheRecentsLocation() {
  QVERIFY(writeStore(m_store, bookmark(hrefOf(m_files + QStringLiteral("/alpha.txt")),
                                       "2026-03-01T09:00:00Z")));
  auto inner = std::make_unique<Test::FakeDirectoryLister>();
  auto *fake = inner.get();
  ListingResult folder;
  folder.path = m_files;
  fake->setResult(m_files, folder);
  const RecentsDirectoryLister lister(std::move(inner), m_store);

  QVERIFY(lister.list(m_files).ok());
  QCOMPARE(fake->requestedPaths(), QStringList{m_files});
  const ListingResult recents = lister.list(RecentsLocation::location());
  QCOMPARE(namesOf(recents), QStringList{QStringLiteral("alpha.txt")});
  // The store answered; the folder lister was never asked for "recents:".
  QCOMPARE(fake->requestedPaths(), QStringList{m_files});
}

void RecentsPlaceTests::recentsIsARootPlaceTheWindowBrowses() {
  const QString project = m_files + QStringLiteral("/project");
  QVERIFY(writeStore(m_store,
                     bookmark(hrefOf(m_files + QStringLiteral("/alpha.txt")), "2026-03-01T09:00:00Z")
                         + bookmark(hrefOf(project), "2026-02-01T09:00:00Z")));
  auto inner = std::make_unique<Test::FakeDirectoryLister>();
  ListingResult projectListing;
  projectListing.path = project;
  inner->setResult(project, projectListing);
  auto launcher = std::make_unique<Test::FakeFileLauncher>();
  auto *launches = launcher.get();
  NavigationController navigation(
      std::make_unique<RecentsDirectoryLister>(std::move(inner), m_store), std::move(launcher));

  navigation.navigateTo(RecentsLocation::location());
  QCOMPARE(navigation.currentPath(), RecentsLocation::location());
  QVERIFY(navigation.recentsPlace());
  QVERIFY(!navigation.applicationsPlace());
  QCOMPARE(navigation.statusKey(), QStringLiteral("ready"));
  QCOMPARE(navigation.entryCount(), 2);
  // A root of its own: no parent, one breadcrumb.
  QVERIFY(!navigation.canGoUp());
  QVERIFY(!NavigationHistory::parentOf(RecentsLocation::location()).has_value());
  const auto crumbs = NavigationHistory::breadcrumbFor(RecentsLocation::location());
  QCOMPARE(crumbs.size(), 1);
  QCOMPARE(crumbs.constFirst().name, QStringLiteral("Recents"));

  // A file opens from Recents by its own path; a folder is entered.
  const int alpha = navigation.indexOfName(QStringLiteral("alpha.txt"));
  QVERIFY(alpha >= 0);
  navigation.activate(alpha);
  QCOMPARE(launches->requestedPaths(), QStringList{m_files + QStringLiteral("/alpha.txt")});
  navigation.activate(navigation.indexOfName(QStringLiteral("project")));
  QCOMPARE(navigation.currentPath(), project);
  QVERIFY(!navigation.recentsPlace());
  navigation.goBack();
  QCOMPARE(navigation.currentPath(), RecentsLocation::location());
}

QTEST_GUILESS_MAIN(RecentsPlaceTests)
#include "tst_recents_place.moc"
