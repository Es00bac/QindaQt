// SPDX-License-Identifier: GPL-3.0-or-later
#include "app_shell/calendar_action_catalog.h"

#include "qindaqt/app_shell/action_registry.h"

#include <QSet>
#include <QTest>

#include <algorithm>

using namespace QindaQt::Apps::Calendar;

class TestCalendarActionCatalog final : public QObject {
  Q_OBJECT

private slots:
  void catalogIsValidStableAndKeyboardComplete();
  void viewActionsAreCheckableAndExclusiveByConvention();
};

void TestCalendarActionCatalog::catalogIsValidStableAndKeyboardComplete() {
  const auto actions = calendarActionCatalog();
  QindaQt::AppShell::ActionRegistry registry;
  const auto result = registry.replaceActions(actions);
  QVERIFY2(result.ok(), qPrintable(result.message));
  QCOMPARE(actions.size(), 11);

  QSet<QString> identities;
  QSet<QString> menus;
  for (const auto &action : actions) {
    QVERIFY2(!action.shortcut.isEmpty(), qPrintable(action.id));
    QVERIFY(!action.accessibleDescription.isEmpty());
    QVERIFY(!action.menuLabel.isEmpty());
    identities.insert(action.id);
    menus.insert(action.menuId);
  }
  const QSet<QString> expected = {
      QStringLiteral("event.new"), QStringLiteral("file.import-ics"),
      QStringLiteral("file.export-ics"), QStringLiteral("event.delete"),
      QStringLiteral("view.month"), QStringLiteral("view.week"),
      QStringLiteral("view.day"), QStringLiteral("go.today"),
      QStringLiteral("go.previous-period"), QStringLiteral("go.next-period"),
      QStringLiteral("calendar.new")};
  QCOMPARE(identities, expected);
  QCOMPARE(menus, (QSet<QString>{QStringLiteral("file"),
                                 QStringLiteral("edit"),
                                 QStringLiteral("view"),
                                 QStringLiteral("go"),
                                 QStringLiteral("calendar")}));

  const auto remove = std::find_if(actions.cbegin(), actions.cend(),
      [](const auto &action) {
        return action.id == QLatin1String("event.delete");
      });
  QVERIFY(remove != actions.cend() && remove->destructive);
}

void TestCalendarActionCatalog::viewActionsAreCheckableAndExclusiveByConvention() {
  const auto actions = calendarActionCatalog();
  const auto find = [&actions](const char *id) {
    return std::find_if(actions.cbegin(), actions.cend(),
                        [id](const auto &action) {
                          return action.id == QLatin1String(id);
                        });
  };
  for (const char *id : {"view.month", "view.week", "view.day"}) {
    const auto action = find(id);
    QVERIFY(action != actions.cend());
    QVERIFY(action->checkable);
    QVERIFY(!action->checked);
  }
  QCOMPARE(find("view.month")->shortcut,
           QKeySequence(QStringLiteral("Ctrl+1")));
  QCOMPARE(find("go.previous-period")->shortcut,
           QKeySequence(QStringLiteral("Ctrl+PageUp")));
  QCOMPARE(find("go.next-period")->shortcut,
           QKeySequence(QStringLiteral("Ctrl+PageDown")));
}

QTEST_GUILESS_MAIN(TestCalendarActionCatalog)
#include "tst_calendar_action_catalog.moc"
