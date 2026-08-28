// SPDX-License-Identifier: GPL-3.0-or-later
#include "model/navigation_history.h"

#include <QTest>

using QindaQt::Apps::FileManager::BreadcrumbSegment;
using QindaQt::Apps::FileManager::NavigationHistory;

class TestNavigationHistory final : public QObject {
  Q_OBJECT

private slots:
  void freshHistoryHasNoCurrent();
  void resetAdoptsPathWithEmptyStacks();
  void navigateToPushesBackAndClearsForward();
  void navigateToSamePathIsNoOp();
  void goBackAndForwardRoundTrip();
  void goBackOnEmptyStackReturnsNullopt();
  void goForwardOnEmptyStackReturnsNullopt();
  void forwardStackIsClearedByANewNavigation();
  void parentOfRoot();
  void parentOfOrdinaryPath();
  void parentOfHandlesTrailingSlashAndDoubleSlash();
  void parentOfEmptyPath();
  void breadcrumbForRoot();
  void breadcrumbForOrdinaryPath();
  void breadcrumbForEmptyPathIsEmpty();
};

void TestNavigationHistory::freshHistoryHasNoCurrent() {
  NavigationHistory history;
  QVERIFY(!history.hasCurrent());
  QVERIFY(history.currentPath().isEmpty());
  QVERIFY(!history.canGoBack());
  QVERIFY(!history.canGoForward());
}

void TestNavigationHistory::resetAdoptsPathWithEmptyStacks() {
  NavigationHistory history;
  history.navigateTo(QStringLiteral("/a"));
  history.navigateTo(QStringLiteral("/a/b"));
  history.reset(QStringLiteral("/fresh"));
  QCOMPARE(history.currentPath(), QStringLiteral("/fresh"));
  QVERIFY(!history.canGoBack());
  QVERIFY(!history.canGoForward());
}

void TestNavigationHistory::navigateToPushesBackAndClearsForward() {
  NavigationHistory history;
  history.reset(QStringLiteral("/a"));
  QVERIFY(history.navigateTo(QStringLiteral("/a/b")));
  QCOMPARE(history.currentPath(), QStringLiteral("/a/b"));
  QVERIFY(history.canGoBack());
  QVERIFY(!history.canGoForward());
}

void TestNavigationHistory::navigateToSamePathIsNoOp() {
  NavigationHistory history;
  history.reset(QStringLiteral("/a"));
  QVERIFY(!history.navigateTo(QStringLiteral("/a")));
  QVERIFY(!history.canGoBack());
}

void TestNavigationHistory::goBackAndForwardRoundTrip() {
  NavigationHistory history;
  history.reset(QStringLiteral("/a"));
  history.navigateTo(QStringLiteral("/a/b"));
  history.navigateTo(QStringLiteral("/a/b/c"));

  const auto back1 = history.goBack();
  QVERIFY(back1.has_value());
  QCOMPARE(*back1, QStringLiteral("/a/b"));
  QVERIFY(history.canGoForward());

  const auto back2 = history.goBack();
  QVERIFY(back2.has_value());
  QCOMPARE(*back2, QStringLiteral("/a"));
  QVERIFY(!history.canGoBack());

  const auto forward1 = history.goForward();
  QVERIFY(forward1.has_value());
  QCOMPARE(*forward1, QStringLiteral("/a/b"));

  const auto forward2 = history.goForward();
  QVERIFY(forward2.has_value());
  QCOMPARE(*forward2, QStringLiteral("/a/b/c"));
  QVERIFY(!history.canGoForward());
}

void TestNavigationHistory::goBackOnEmptyStackReturnsNullopt() {
  NavigationHistory history;
  history.reset(QStringLiteral("/a"));
  QVERIFY(!history.goBack().has_value());
  QCOMPARE(history.currentPath(), QStringLiteral("/a"));
}

void TestNavigationHistory::goForwardOnEmptyStackReturnsNullopt() {
  NavigationHistory history;
  history.reset(QStringLiteral("/a"));
  QVERIFY(!history.goForward().has_value());
  QCOMPARE(history.currentPath(), QStringLiteral("/a"));
}

void TestNavigationHistory::forwardStackIsClearedByANewNavigation() {
  NavigationHistory history;
  history.reset(QStringLiteral("/a"));
  history.navigateTo(QStringLiteral("/a/b"));
  QVERIFY(history.goBack().has_value());
  QVERIFY(history.canGoForward());
  history.navigateTo(QStringLiteral("/a/c"));
  QVERIFY(!history.canGoForward());
}

void TestNavigationHistory::parentOfRoot() {
  QVERIFY(!NavigationHistory::parentOf(QStringLiteral("/")).has_value());
}

void TestNavigationHistory::parentOfOrdinaryPath() {
  const auto parent = NavigationHistory::parentOf(QStringLiteral("/home/jarrod/docs"));
  QVERIFY(parent.has_value());
  QCOMPARE(*parent, QStringLiteral("/home/jarrod"));
}

void TestNavigationHistory::parentOfHandlesTrailingSlashAndDoubleSlash() {
  const auto parent = NavigationHistory::parentOf(QStringLiteral("/home//jarrod/docs/"));
  QVERIFY(parent.has_value());
  QCOMPARE(*parent, QStringLiteral("/home/jarrod"));
}

void TestNavigationHistory::parentOfEmptyPath() {
  QVERIFY(!NavigationHistory::parentOf(QString()).has_value());
}

void TestNavigationHistory::breadcrumbForRoot() {
  const auto segments = NavigationHistory::breadcrumbFor(QStringLiteral("/"));
  QCOMPARE(segments.size(), 1);
  QCOMPARE(segments.first().name, QStringLiteral("/"));
  QCOMPARE(segments.first().path, QStringLiteral("/"));
}

void TestNavigationHistory::breadcrumbForOrdinaryPath() {
  const auto segments = NavigationHistory::breadcrumbFor(QStringLiteral("/home/jarrod"));
  QCOMPARE(segments.size(), 3);
  QCOMPARE(segments.at(0), (BreadcrumbSegment{QStringLiteral("/"), QStringLiteral("/")}));
  QCOMPARE(segments.at(1),
           (BreadcrumbSegment{QStringLiteral("home"), QStringLiteral("/home")}));
  QCOMPARE(segments.at(2),
           (BreadcrumbSegment{QStringLiteral("jarrod"), QStringLiteral("/home/jarrod")}));
}

void TestNavigationHistory::breadcrumbForEmptyPathIsEmpty() {
  QVERIFY(NavigationHistory::breadcrumbFor(QString()).isEmpty());
}

QTEST_APPLESS_MAIN(TestNavigationHistory)
#include "tst_navigation_history.moc"
