// SPDX-License-Identifier: GPL-3.0-or-later
#include "app_shell/file_manager_action_catalog.h"
#include "app_shell/file_manager_browsing_actions.h"
#include "fakes.h"
#include "model/local_directory_lister.h"
#include "model/navigation_controller.h"
#include "model/places_controller.h"
#include "mutation/local_mutation_backend.h"
#include "mutation/mutation_controller.h"
#include "preview/preview_provider.h"
#include "preview/theme_icon_provider.h"

#include <QFile>
#include <QDir>
#include <QWheelEvent>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QStyleHints>
#include <QTemporaryDir>
#include <QtTest>
#include <qindaqt/app_shell/application_coordinator.h>

using namespace QindaQt::Apps::FileManager;

class BrowsingUiTests final : public QObject {
  Q_OBJECT
private slots:
  void keyboardWheelAndFilter_data();
  void keyboardWheelAndFilter();
};

void BrowsingUiTests::keyboardWheelAndFilter_data() {
  QTest::addColumn<QSize>("windowSize");
  QTest::addColumn<Qt::ColorScheme>("scheme");
  QTest::newRow("compact-light") << QSize(480, 360) << Qt::ColorScheme::Light;
  QTest::newRow("desktop-dark") << QSize(1280, 800) << Qt::ColorScheme::Dark;
  QTest::newRow("1080p-dark") << QSize(1920, 1080) << Qt::ColorScheme::Dark;
}

void BrowsingUiTests::keyboardWheelAndFilter() {
  QFETCH(QSize, windowSize);
  QFETCH(Qt::ColorScheme, scheme);
  // ADR-0116: appearance comes from the platform theme; the declarative color
  // scheme stands in for the session palette under the generic test theme.
  QGuiApplication::styleHints()->setColorScheme(scheme);
  const QString sourceRoot = QStringLiteral(QINDAQT_SOURCE_DIR);
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  const QString folder = temporary.filePath("files");
  QVERIFY(QDir().mkpath(folder));
  for (int i = 0; i < 160; ++i) {
    QFile file(folder + QStringLiteral("/Document-%1.txt").arg(i, 3, 10, QLatin1Char('0')));
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write("test"), qint64(4));
  }
  NavigationController navigation(std::make_unique<LocalDirectoryLister>(),
                                  std::make_unique<Test::FakeFileLauncher>());
  MutationController mutation(std::make_unique<LocalMutationBackend>(temporary.filePath("Trash")));
  PlacesController places(std::make_unique<BookmarksStore>(temporary.filePath("state")));
  QindaQt::AppShell::ApplicationCoordinator coordinator;
  QVERIFY(coordinator.replaceActions(fileManagerActionCatalog()).ok());
  bindFileManagerBrowsingActions(coordinator, navigation);
  navigation.navigateTo(folder);

  QQmlApplicationEngine engine;
  QIcon::setThemeSearchPaths({sourceRoot + "/data/icons"});
  QIcon::setThemeName(QStringLiteral("QindaQt"));
  QVERIFY(QIcon::hasThemeIcon(QStringLiteral("list-add-symbolic")));
  QVERIFY(QIcon::hasThemeIcon(QStringLiteral("list-remove-symbolic")));
  auto *previews = new PreviewProvider(std::make_unique<LocalPreviewDecoder>());
  engine.addImageProvider("previews", previews);
  engine.addImageProvider("theme-icons", new ThemeIconProvider());
  previews->setGeneration(navigation.listingGeneration());
  engine.setInitialProperties({
      {"navigationController", QVariant::fromValue(static_cast<QObject *>(&navigation))},
      {"mutationController", QVariant::fromValue(static_cast<QObject *>(&mutation))},
      {"placesController", QVariant::fromValue(static_cast<QObject *>(&places))},
      {"coordinator", QVariant::fromValue(static_cast<QObject *>(&coordinator))}});
  engine.load(QUrl::fromLocalFile(sourceRoot + "/src/apps/file_manager/ui/Main.qml"));
  QVERIFY(!engine.rootObjects().isEmpty());
  auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
  QVERIFY(window);
  window->resize(windowSize);
  window->requestActivate();
  QTRY_VERIFY(window->isExposed());
  QTRY_VERIFY(window->isActive());
  auto *grid = window->findChild<QQuickItem *>("entryGridView");
  auto *list = window->findChild<QQuickItem *>("entryListView");
  QVERIFY(grid && list);
  grid->forceActiveFocus();
  QTest::keyClick(window, Qt::Key_End);
  QTRY_COMPARE(grid->property("currentIndex").toInt(), 159);
  QVERIFY(grid->property("contentY").toReal() > 0);

  QTest::keyClick(window, Qt::Key_1, Qt::ControlModifier);
  QTRY_COMPARE(navigation.viewMode(), QStringLiteral("list"));
  QTRY_VERIFY(list->hasActiveFocus());
  QCOMPARE(list->property("currentIndex").toInt(), 159);
  QTest::keyClick(window, Qt::Key_2, Qt::ControlModifier);
  QTRY_VERIFY(grid->hasActiveFocus());
  QTRY_COMPARE(navigation.viewMode(), QStringLiteral("grid"));
  QTest::keyClick(window, Qt::Key_2, Qt::ControlModifier);
  QCOMPARE(navigation.viewMode(), QStringLiteral("grid"));
  QObject *gridAction = nullptr;
  for (QObject *object : window->findChildren<QObject *>()) {
    if (object->property("modelData").toMap().value("id").toString()
        == QStringLiteral("view.grid-mode")) gridAction = object;
  }
  QVERIFY(gridAction);
  QVERIFY(!gridAction->property("checkable").toBool());
  QVERIFY(!gridAction->property("checked").toBool());

  const QPointF local = grid->mapToScene(QPointF(grid->width() / 2, grid->height() / 2));
  const auto wheel = [&](Qt::KeyboardModifiers modifiers) {
    QWheelEvent event(local, window->mapToGlobal(local.toPoint()), {}, QPoint(0, 120),
                      Qt::NoButton, modifiers, Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(window, &event);
  };
  wheel(Qt::ControlModifier);
  QTRY_COMPARE(navigation.iconSize(), 96);
  QCOMPARE(grid->property("currentIndex").toInt(), 159);
  QTest::keyClick(window, Qt::Key_0, Qt::ControlModifier);
  QTRY_COMPARE(navigation.iconSize(), 64);
  wheel(Qt::NoModifier);
  QCOMPARE(navigation.iconSize(), 64);

  QTest::keyClick(window, Qt::Key_F, Qt::ControlModifier);
  auto *field = window->findChild<QQuickItem *>("folderFilterField");
  QVERIFY(field);
  QTRY_VERIFY(field->hasActiveFocus());
  for (QChar character : QStringLiteral("Document-159"))
    QTest::keyClick(window, character.toLatin1());
  QTRY_COMPARE(navigation.entries().size(), 1);
  QCOMPARE(navigation.entries().first().toMap().value("name").toString(), QStringLiteral("Document-159.txt"));
  QTest::keyClick(window, Qt::Key_A, Qt::ControlModifier);
  QTest::keyClick(window, Qt::Key_Delete);
  QTRY_VERIFY(navigation.nameFilter().isEmpty());
  auto *trashDialog = window->findChild<QObject *>("trashConfirmationDialog");
  QVERIFY(trashDialog && !trashDialog->property("visible").toBool());
  QTest::keyClick(window, Qt::Key_Escape);
  QTRY_COMPARE(navigation.entries().size(), 160);
  QTRY_VERIFY(grid->hasActiveFocus());

  auto *forward = window->findChild<QQuickItem *>("navigateForwardButton");
  QVERIFY(forward && forward->isVisible());
  const auto toolbar = window->findChild<QQuickItem *>("fileManagerToolbar");
  QVERIFY(toolbar && toolbar->width() <= window->width());
}

QTEST_MAIN(BrowsingUiTests)
#include "tst_browsing_ui.moc"
