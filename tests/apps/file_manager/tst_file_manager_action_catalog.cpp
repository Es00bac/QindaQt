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
  QCOMPARE(actions.size(), 24);

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
      QStringLiteral("file.empty-trash"), QStringLiteral("edit.undo"),
      QStringLiteral("operation.cancel"), QStringLiteral("edit.select-all"),
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
  QCOMPARE(selectAll->order, 2);
  QCOMPARE(selectAll->shortcut, QKeySequence(QStringLiteral("Ctrl+A")));
  QVERIFY(!selectAll->checkable);
  QVERIFY(!selectAll->destructive);

  // AGENT-CONTRACT: The checkable view actions mirror
  // NavigationController's showHidden/viewMode state; main.cpp syncs their
  // checked flags on presentationChanged, so the checkable bit and shortcut
  // here are part of the UI contract the S2 UI probe drives.
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
  QVERIFY(gridMode->checkable);

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
