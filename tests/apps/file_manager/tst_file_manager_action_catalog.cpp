// SPDX-License-Identifier: GPL-3.0-or-later
#include "app_shell/file_manager_action_catalog.h"

#include "qindaqt/app_shell/action_registry.h"

#include <QHash>
#include <QSet>
#include <QTest>

#include <algorithm>

using namespace QindaQt::Apps::FileManager;

class TestFileManagerActionCatalog final : public QObject {
  Q_OBJECT

private slots:
  void catalogIsValidStableAndKeyboardComplete();
  void s2ViewEditGoActionsAreCatalogued();
  void rightClickSetIsCataloguedWithDistinctShortcuts();
};

void TestFileManagerActionCatalog::catalogIsValidStableAndKeyboardComplete() {
  const auto actions = fileManagerActionCatalog();
  QindaQt::AppShell::ActionRegistry registry;
  const auto result = registry.replaceActions(actions);
  QVERIFY2(result.ok(), qPrintable(result.message));
  QCOMPARE(actions.size(), 51);

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
      QStringLiteral("go.forward"), QStringLiteral("go.up"), QStringLiteral("go.applications"),
      QStringLiteral("go.network"), QStringLiteral("network.connect"),
      QStringLiteral("app.preferences"),
      QStringLiteral("view.refresh"),
      // ADR-0262: the Applications place's own actions.
      QStringLiteral("application.show-entry-file"), QStringLiteral("view.group-by-category"),
      // ADR-0269: the right-click set (file.open replaced application.open).
      QStringLiteral("file.open"), QStringLiteral("file.open-with"),
      QStringLiteral("file.open-new-window"), QStringLiteral("file.new-file"),
      QStringLiteral("file.open-terminal"), QStringLiteral("file.duplicate"),
      QStringLiteral("file.make-link"), QStringLiteral("file.compress"),
      QStringLiteral("file.extract"), QStringLiteral("file.add-to-sidebar"),
      QStringLiteral("file.delete"), QStringLiteral("file.put-back"),
      QStringLiteral("edit.copy-path"), QStringLiteral("view.sort-name"),
      QStringLiteral("view.sort-size"), QStringLiteral("view.sort-kind"),
      QStringLiteral("view.sort-modified")};
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
  QCOMPARE(properties->order, 50);
  QCOMPARE(properties->label, QStringLiteral("Get Info"));
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

  // ADR-0194: the network pair sits at the end of the Go menu, after
  // Applications, so the ordinary folder destinations stay on top.
  const auto network = find("go.network");
  QVERIFY(network != actions.cend());
  QCOMPARE(network->menuId, QStringLiteral("go"));
  QCOMPARE(network->menuOrder, 3);
  QCOMPARE(network->order, 6);
  QCOMPARE(network->shortcut, QKeySequence(QStringLiteral("Alt+N")));
  QVERIFY(!network->checkable);
  QVERIFY(!network->destructive);

  const auto connect = find("network.connect");
  QVERIFY(connect != actions.cend());
  QCOMPARE(connect->menuId, QStringLiteral("go"));
  QCOMPARE(connect->menuOrder, 3);
  QCOMPARE(connect->order, 7);
  QCOMPARE(connect->shortcut, QKeySequence(QStringLiteral("Ctrl+Shift+S")));
  QVERIFY(!connect->checkable);
  QVERIFY(!connect->destructive);

  // ADR-0198: Preferences uses the platform-standard Ctrl+, and sits after
  // the item-specific File entries.
  const auto preferences = find("app.preferences");
  QVERIFY(preferences != actions.cend());
  QCOMPARE(preferences->menuId, QStringLiteral("file"));
  QCOMPARE(preferences->menuOrder, 0);
  QCOMPARE(preferences->order, 60);
  QCOMPARE(preferences->shortcut, QKeySequence(QStringLiteral("Ctrl+,")));
  QVERIFY(!preferences->checkable);
  QVERIFY(!preferences->destructive);

  const auto bookmarkAdd = find("bookmark.add");
  QVERIFY(bookmarkAdd != actions.cend());
  QCOMPARE(bookmarkAdd->menuId, QStringLiteral("go"));
  QCOMPARE(bookmarkAdd->menuOrder, 3);
  QCOMPARE(bookmarkAdd->order, 1);
  QCOMPARE(bookmarkAdd->shortcut, QKeySequence(QStringLiteral("Ctrl+D")));
  QVERIFY(!bookmarkAdd->checkable);

  // ADR-0262/ADR-0269: Open heads the File menu for every place, Show
  // Desktop Entry File sits with Get Info, and Group by Category is a View
  // toggle.
  const auto open = find("file.open");
  QVERIFY(open != actions.cend());
  QCOMPARE(open->menuId, QStringLiteral("file"));
  QCOMPARE(open->order, 0);
  QCOMPARE(open->shortcut, QKeySequence(QStringLiteral("Ctrl+O")));
  QVERIFY(!open->checkable);
  QVERIFY(find("application.open") == actions.cend());
  const auto showEntry = find("application.show-entry-file");
  QVERIFY(showEntry != actions.cend());
  QCOMPARE(showEntry->menuId, QStringLiteral("file"));
  QCOMPARE(showEntry->order, 51);
  QCOMPARE(showEntry->shortcut, QKeySequence(QStringLiteral("Ctrl+Shift+E")));
  const auto group = find("view.group-by-category");
  QVERIFY(group != actions.cend());
  QCOMPARE(group->menuId, QStringLiteral("view"));
  QCOMPARE(group->menuOrder, 2);
  QCOMPARE(group->shortcut, QKeySequence(QStringLiteral("Ctrl+G")));
  QVERIFY(group->checkable);
  QVERIFY(!group->destructive);
}

void TestFileManagerActionCatalog::rightClickSetIsCataloguedWithDistinctShortcuts() {
  const auto actions = fileManagerActionCatalog();
  // One key, one action: a shared shortcut would make Qt treat the key as
  // ambiguous and run neither action.
  QHash<QString, QString> owners;
  for (const auto &action : actions) {
    const QString key = action.shortcut.toString(QKeySequence::PortableText);
    QVERIFY2(!owners.contains(key),
             qPrintable(QStringLiteral("%1 shared by %2 and %3").arg(key, owners.value(key), action.id)));
    owners.insert(key, action.id);
  }
  const auto find = [&actions](const char *id) {
    return std::find_if(actions.cbegin(), actions.cend(), [id](const auto &action) {
      return action.id == QLatin1String(id);
    });
  };
  const struct {
    const char *id;
    const char *menu;
    const char *shortcut;
    bool destructive;
  } expected[] = {
      {"file.open-with", "file", "Ctrl+Shift+O", false},
      {"file.open-new-window", "file", "Ctrl+Alt+O", false},
      {"file.new-file", "file", "Ctrl+Alt+N", false},
      {"file.open-terminal", "file", "Shift+F4", false},
      {"file.duplicate", "file", "Ctrl+Shift+D", false},
      {"file.make-link", "file", "Ctrl+M", false},
      {"file.compress", "file", "Ctrl+Shift+K", false},
      {"file.extract", "file", "Ctrl+Shift+X", false},
      {"file.add-to-sidebar", "file", "Ctrl+Shift+B", false},
      {"file.delete", "file", "Shift+Delete", true},
      {"file.put-back", "file", "Ctrl+Backspace", false},
      {"edit.copy-path", "edit", "Ctrl+Alt+C", false},
      {"view.sort-name", "view", "Ctrl+Alt+1", false},
      {"view.sort-modified", "view", "Ctrl+Alt+4", false},
  };
  for (const auto &row : expected) {
    const auto action = find(row.id);
    QVERIFY2(action != actions.cend(), row.id);
    QCOMPARE(action->menuId, QString::fromLatin1(row.menu));
    QCOMPARE(action->shortcut, QKeySequence(QString::fromLatin1(row.shortcut)));
    QCOMPARE(action->destructive, row.destructive);
    // Sort choices, like view choices, are commands, never checkable Actions.
    QVERIFY2(!action->checkable, row.id);
  }
  // Every new File entry sits between Open (0) and Preferences (60).
  for (const auto &action : actions) {
    if (action.menuId == QLatin1String("file") && action.id != QLatin1String("app.preferences")) {
      QVERIFY2(action.order >= 0 && action.order < 60, qPrintable(action.id));
    }
  }
}

QTEST_MAIN(TestFileManagerActionCatalog)
#include "tst_file_manager_action_catalog.moc"
