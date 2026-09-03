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
};

void TestFileManagerActionCatalog::catalogIsValidStableAndKeyboardComplete() {
  const auto actions = fileManagerActionCatalog();
  QindaQt::AppShell::ActionRegistry registry;
  const auto result = registry.replaceActions(actions);
  QVERIFY2(result.ok(), qPrintable(result.message));
  QCOMPARE(actions.size(), 9);

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
      QStringLiteral("operation.cancel")};
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

QTEST_MAIN(TestFileManagerActionCatalog)
#include "tst_file_manager_action_catalog.moc"
