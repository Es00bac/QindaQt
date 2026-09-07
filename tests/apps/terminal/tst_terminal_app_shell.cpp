// SPDX-License-Identifier: GPL-3.0-or-later
#include "app_shell/terminal_action_catalog.h"
#include "app_shell/terminal_app_shell_bridge.h"

#include <QAction>
#include <QHash>
#include <QSet>
#include <QSignalSpy>
#include <QtTest>

using namespace QindaQt::Apps::Terminal;

class TerminalAppShellTest final : public QObject {
  Q_OBJECT

private slots:
  void catalogIsCompleteStableAndShiftModified();
  void bridgeRoutesEnabledActionsToTheirLocalCommand();
};

void TerminalAppShellTest::catalogIsCompleteStableAndShiftModified() {
  const auto catalog = terminalActionCatalog();
  QCOMPARE(catalog.size(), 24);
  QSet<QString> ids;
  for (const auto &spec : catalog) {
    QVERIFY(!spec.id.isEmpty());
    QVERIFY(!ids.contains(spec.id));
    ids.insert(spec.id);
    QVERIFY(!spec.label.isEmpty());
    QVERIFY(!spec.accessibleDescription.isEmpty());
    QVERIFY(!spec.menuId.isEmpty());
    if (!spec.shortcut.isEmpty()) {
      const QString shortcut = spec.shortcut.toString();
      QVERIFY(shortcut.contains(QStringLiteral("Shift")) ||
              shortcut == QStringLiteral("F3"));
    }
  }
  QVERIFY(ids.contains(QString::fromLatin1(AppShellActionIds::SessionNewTab)));
  QVERIFY(
      ids.contains(QString::fromLatin1(AppShellActionIds::SessionCloseTab)));
  QVERIFY(ids.contains(QString::fromLatin1(AppShellActionIds::FileQuit)));
  QVERIFY(ids.contains(QString::fromLatin1(AppShellActionIds::ViewFind)));
  QVERIFY(ids.contains(QString::fromLatin1(AppShellActionIds::LinkCopy)));
  QVERIFY(ids.contains(QString::fromLatin1(AppShellActionIds::LinkOpen)));
}

void TerminalAppShellTest::bridgeRoutesEnabledActionsToTheirLocalCommand() {
  TerminalAppShellBridge bridge;
  QVERIFY(bridge.publishActionCatalog().ok());
  QAction action;
  QSignalSpy triggered(&action, &QAction::triggered);
  const QString id = QString::fromLatin1(AppShellActionIds::SessionNewTab);
  bridge.bindActivationTargets({{id, &action}});

  QVERIFY(bridge.coordinator().activateAction(id));
  QCOMPARE(triggered.count(), 1);
  QVERIFY(bridge.setActionEnabled(id, false).ok());
  QVERIFY(!bridge.coordinator().activateAction(id));
  QCOMPARE(triggered.count(), 1);
  QVERIFY(
      !bridge.coordinator().activateAction(QStringLiteral("unknown.action")));
}

QTEST_MAIN(TerminalAppShellTest)
#include "tst_terminal_app_shell.moc"
