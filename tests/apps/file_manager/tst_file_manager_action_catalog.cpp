// SPDX-License-Identifier: GPL-3.0-or-later
#include "app_shell/file_manager_action_catalog.h"

#include "qindaqt/app_shell/action_registry.h"

#include <QSet>
#include <QTest>

#include <algorithm>

using namespace QindaQt::Apps::FileManager;

class TestFileManagerActionCatalog final : public QObject {
  Q_OBJECT

private slots:
  void catalogIsValidStableAndKeyboardComplete();
  void s2ViewEditGoActionsAreCatalogued();
};

void TestFileManagerActionCatalog::catalogIsValidStableAndKeyboardComplete() {
  const auto actions = fileManagerActionCatalog();
  QindaQt::AppShell::ActionRegistry registry;
  const auto result = registry.replaceActions(actions);
  QVERIFY2(result.ok(), qPrintable(result.message));
  QCOMPARE(actions.size(), 28);

  QSet<QString> identities;
  for (const auto &action : actions) {
    QVERIFY(!action.shortcut.isEmpty());
    QVERIFY(!action.accessibleDescription.isEmpty());
    identities.insert(action.id);
  }
  const QSet<QString> expected = {
      QStringLiteral("file.new-folder"), QStringLiteral("file.rename"),
      QStringLiteral("file.copy"), QStringLiteral("file.move"),
      QStringLiteral("file.trash"), QStringLiteral("file.restore-last"),
      QStringLiteral("file.empty-trash"), QStringLiteral("file.properties"),
      QStringLiteral("edit.undo"),
      QStringLiteral("operation.cancel"), QStringLiteral("edit.select-all"),
      QStringLiteral("edit.cut"), QStringLiteral("edit.copy"),
      QStringLiteral("edit.paste"),
      QStringLiteral("view.show-hidden"), QStringLiteral("view.grid-mode"),
      QStringLiteral("view.details-mode"), QStringLiteral("view.zoom-in"),
      QStringLiteral("view.zoom-out"), QStringLiteral("view.zoom-reset"),
      QStringLiteral("view.filter"),
      QStringLiteral("view.focus-location"), QStringLiteral("go.home"),
      QStringLiteral("bookmark.add"), QStringLiteral("go.back"),
      QStringLiteral("go.forward"), QStringLiteral("go.up"), QStringLiteral("view.refresh")};
  QCOMPARE(identities, expected);

  const auto trash = std::find_if(actions.cbegin(), actions.cend(), [](const auto &action) {
    return action.id == QLatin1String("file.trash");
  });
  const auto empty = std::find_if(actions.cbegin(), actions.cend(), [](const auto &action) {
    return action.id == QLatin1String("file.empty-trash");
  });
  QVERIFY(trash != actions.cend() && trash->destructive);
  QVERIFY(empty != actions.cend() && empty->destructive);
}

void TestFileManagerActionCatalog::s2ViewEditGoActionsAreCatalogued() {
  const auto actions = fileManagerActionCatalog();
  const auto find = [&actions](const char *id) {
    return std::find_if(actions.cbegin(), actions.cend(), [id](const auto &action) {
      return action.id == QLatin1String(id);
    });
  };

  const auto selectAll = find("edit.select-all");
  QVERIFY(selectAll != actions.cend());
  QCOMPARE(selectAll->menuId, QStringLiteral("edit"));
  QCOMPARE(selectAll->menuOrder, 1);
  QCOMPARE(selectAll->order, 5);
  QCOMPARE(selectAll->shortcut, QKeySequence(QStringLiteral("Ctrl+A")));
  QVERIFY(!selectAll->checkable);
  QVERIFY(!selectAll->destructive);

  // S3 clipboard and properties actions sit in the closed catalog with
  // platform-standard shortcuts; enabled state is driven by the transfer
  // binding, not the catalog.
  const auto cut = find("edit.cut");
  QVERIFY(cut != actions.cend());
  QCOMPARE(cut->menuId, QStringLiteral("edit"));
  QCOMPARE(cut->menuOrder, 1);
  QCOMPARE(cut->shortcut, QKeySequence(QStringLiteral("Ctrl+X")));
  const auto copy = find("edit.copy");
  QVERIFY(copy != actions.cend());
  QCOMPARE(copy->menuId, QStringLiteral("edit"));
  QCOMPARE(copy->shortcut, QKeySequence(QStringLiteral("Ctrl+C")));
  const auto paste = find("edit.paste");
  QVERIFY(paste != actions.cend());
  QCOMPARE(paste->menuId, QStringLiteral("edit"));
  QCOMPARE(paste->shortcut, QKeySequence(QStringLiteral("Ctrl+V")));
  const auto properties = find("file.properties");
  QVERIFY(properties != actions.cend());
  QCOMPARE(properties->menuId, QStringLiteral("file"));
  QCOMPARE(properties->menuOrder, 0);
  QCOMPARE(properties->order, 7);
  QCOMPARE(properties->shortcut, QKeySequence(QStringLiteral("Alt+Return")));

  // Hidden files is a toggle; explicit view selections are idempotent commands.
  // A checkable Qt Action would toggle itself off when selecting the same view.
  const auto showHidden = find("view.show-hidden");
  QVERIFY(showHidden != actions.cend());
  QCOMPARE(showHidden->menuId, QStringLiteral("view"));
  QCOMPARE(showHidden->menuOrder, 2);
  QCOMPARE(showHidden->order, 0);
  QCOMPARE(showHidden->shortcut, QKeySequence(QStringLiteral("Ctrl+H")));
  QVERIFY(showHidden->checkable);

  const auto gridMode = find("view.grid-mode");
  QVERIFY(gridMode != actions.cend());
  QCOMPARE(gridMode->menuId, QStringLiteral("view"));
  QCOMPARE(gridMode->menuOrder, 2);
  QCOMPARE(gridMode->order, 1);
  QCOMPARE(gridMode->shortcut, QKeySequence(QStringLiteral("Ctrl+2")));
  QVERIFY(!gridMode->checkable);
  const auto detailsMode = find("view.details-mode");
  QVERIFY(detailsMode != actions.cend());
  QVERIFY(!detailsMode->checkable);
  QCOMPARE(detailsMode->shortcut, QKeySequence(QStringLiteral("Ctrl+1")));

  const auto focusLocation = find("view.focus-location");
  QVERIFY(focusLocation != actions.cend());
  QCOMPARE(focusLocation->menuId, QStringLiteral("view"));
  QCOMPARE(focusLocation->menuOrder, 2);
  QCOMPARE(focusLocation->order, 2);
  QCOMPARE(focusLocation->shortcut, QKeySequence(QStringLiteral("Ctrl+L")));
  QVERIFY(!focusLocation->checkable);

  const auto goHome = find("go.home");
  QVERIFY(goHome != actions.cend());
  QCOMPARE(goHome->menuId, QStringLiteral("go"));
  QCOMPARE(goHome->menuOrder, 3);
  QCOMPARE(goHome->order, 0);
  QCOMPARE(goHome->shortcut, QKeySequence(QStringLiteral("Alt+Home")));
  QVERIFY(!goHome->checkable);

  const auto bookmarkAdd = find("bookmark.add");
  QVERIFY(bookmarkAdd != actions.cend());
  QCOMPARE(bookmarkAdd->menuId, QStringLiteral("go"));
  QCOMPARE(bookmarkAdd->menuOrder, 3);
  QCOMPARE(bookmarkAdd->order, 1);
  QCOMPARE(bookmarkAdd->shortcut, QKeySequence(QStringLiteral("Ctrl+D")));
  QVERIFY(!bookmarkAdd->checkable);
}

QTEST_MAIN(TestFileManagerActionCatalog)
#include "tst_file_manager_action_catalog.moc"
