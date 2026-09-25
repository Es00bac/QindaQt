// SPDX-License-Identifier: GPL-3.0-or-later
// ADR-0271: FolderNavigations -- a controller per tab, and the window's
// action states following the one the user works in.
#include "app_shell/file_manager_action_catalog.h"
#include "app_shell/file_manager_browsing_actions.h"
#include "fakes.h"
#include "model/local_directory_lister.h"
#include "model/navigation_controller.h"
#include "runtime/folder_navigations.h"

#include "qindaqt/app_shell/application_coordinator.h"

#include <QDir>
#include <QPointer>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using namespace QindaQt::Apps::FileManager;

namespace {

[[nodiscard]] std::unique_ptr<NavigationController> localNavigation() {
  return std::make_unique<NavigationController>(std::make_unique<LocalDirectoryLister>(),
                                                std::make_unique<Test::FakeFileLauncher>());
}

// The window title is what the browsing binding writes on every change, so
// it names the controller the actions follow.
struct Harness final {
  explicit Harness(const QString &firstPath) : first(localNavigation()) {
    catalogInstalled = coordinator.replaceActions(fileManagerActionCatalog()).ok();
    first->navigateTo(firstPath);
    navigations = std::make_unique<FolderNavigations>(
        *first, [] { return localNavigation(); },
        [this](NavigationController &navigation, QObject &context) {
          ++binds;
          bindFileManagerBrowsingActions(coordinator, navigation, &context);
        });
  }

  [[nodiscard]] bool titleNames(const QString &path) const {
    return coordinator.windowTitle().endsWith(path);
  }

  QindaQt::AppShell::ApplicationCoordinator coordinator;
  bool catalogInstalled = false;
  std::unique_ptr<NavigationController> first;
  std::unique_ptr<FolderNavigations> navigations;
  int binds = 0;
};

} // namespace

class TestFolderNavigations final : public QObject {
  Q_OBJECT

private slots:
  void init();
  void bindsTheFirstControllerAtOnce();
  void theActionsFollowTheActiveController();
  void releasingTheActiveControllerFallsBackToTheFirst();
  void theFirstControllerIsNeverReleased();

private:
  std::unique_ptr<QTemporaryDir> m_temporary;
  QString m_one;
  QString m_two;
};

void TestFolderNavigations::init() {
  m_temporary = std::make_unique<QTemporaryDir>();
  QVERIFY(m_temporary->isValid());
  m_one = m_temporary->filePath(QStringLiteral("one"));
  m_two = m_temporary->filePath(QStringLiteral("two"));
  QVERIFY(QDir().mkpath(m_one + QStringLiteral("/inner")));
  QVERIFY(QDir().mkpath(m_two));
}

void TestFolderNavigations::bindsTheFirstControllerAtOnce() {
  Harness harness(m_one);
  QVERIFY(harness.catalogInstalled);
  QCOMPARE(harness.binds, 1);
  QCOMPARE(harness.navigations->active(), harness.first.get());
  QVERIFY(harness.titleNames(m_one));
  harness.first->navigateTo(m_one + QStringLiteral("/inner"));
  QVERIFY(harness.titleNames(m_one + QStringLiteral("/inner")));
}

void TestFolderNavigations::theActionsFollowTheActiveController() {
  Harness harness(m_one);
  QSignalSpy activeChanged(harness.navigations.get(), &FolderNavigations::activeChanged);
  QObject *second = harness.navigations->create(m_two);
  QVERIFY(second != nullptr);
  QCOMPARE(second->parent(), harness.navigations.get());
  QCOMPARE(qobject_cast<NavigationController *>(second)->currentPath(), m_two);
  // Creating a tab does not move the actions; choosing it does.
  QVERIFY(harness.titleNames(m_one));
  harness.navigations->setActive(second);
  QCOMPARE(activeChanged.count(), 1);
  QCOMPARE(harness.navigations->active(), second);
  QVERIFY(harness.titleNames(m_two));
  // The tab left behind no longer writes the window's state.
  harness.first->navigateTo(m_one + QStringLiteral("/inner"));
  QVERIFY(harness.titleNames(m_two));
  // Choosing the active one again binds nothing twice.
  const int binds = harness.binds;
  harness.navigations->setActive(second);
  QCOMPARE(harness.binds, binds);
}

void TestFolderNavigations::releasingTheActiveControllerFallsBackToTheFirst() {
  Harness harness(m_one);
  QPointer<QObject> second = harness.navigations->create(m_two);
  harness.navigations->setActive(second);
  harness.navigations->release(second);
  QCOMPARE(harness.navigations->active(), harness.first.get());
  QVERIFY(harness.titleNames(m_one));
  // Deleted on the next event-loop turn, after the views bound to it.
  QVERIFY(!second.isNull());
  QTRY_VERIFY(second.isNull());
}

void TestFolderNavigations::theFirstControllerIsNeverReleased() {
  Harness harness(m_one);
  QObject stranger;
  harness.navigations->release(harness.first.get());
  harness.navigations->release(&stranger);
  harness.navigations->setActive(&stranger);
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  QCOMPARE(harness.navigations->active(), harness.first.get());
  QCOMPARE(harness.first->currentPath(), m_one);
}

QTEST_MAIN(TestFolderNavigations)
#include "tst_folder_navigations.moc"
