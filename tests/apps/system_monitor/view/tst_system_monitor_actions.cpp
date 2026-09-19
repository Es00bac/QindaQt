// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>

#include <qindaqt/app_shell/action_registry.h>

#include "system_monitor_actions.h"

using namespace QindaQt::SystemMonitor;

// The catalog is what the desktop's global menu and the in-window bar are
// both built from, so a catalog the registry rejects means no menu anywhere.
class TestSystemMonitorActions : public QObject {
  Q_OBJECT

private slots:
  void catalogIsAcceptedByTheRegistry();
  void everyActionCarriesAShortcut();
  void intervalIdsRoundTrip();
  void singlePanelWindowsOfferNoLayoutCommands();
  void menusAreOrderedAndNamed();
};

void TestSystemMonitorActions::catalogIsAcceptedByTheRegistry() {
  for (const bool singlePanel : {false, true}) {
    QindaQt::AppShell::ActionRegistry registry;
    const auto error = registry.replaceActions(systemMonitorActions(singlePanel));
    QVERIFY2(error.ok(), qPrintable(error.message));
    QVERIFY(!registry.menus().isEmpty());
  }
}

void TestSystemMonitorActions::everyActionCarriesAShortcut() {
  // AGENT-GUARD: ActionRegistry::validate rejects an action with an empty
  // shortcut ("invalid presentation state") and the rejection is atomic, so
  // one bare entry silently costs the application its entire menu.
  for (const auto &spec : systemMonitorActions(false)) {
    QVERIFY2(!spec.shortcut.isEmpty(), qPrintable(spec.id));
    QVERIFY2(!spec.label.isEmpty(), qPrintable(spec.id));
    QVERIFY2(!spec.menuLabel.isEmpty(), qPrintable(spec.id));
  }
}

void TestSystemMonitorActions::intervalIdsRoundTrip() {
  for (const int milliseconds : {500, 1000, 3000}) {
    const QString id = intervalActionId(milliseconds);
    QVERIFY2(!id.isEmpty(), qPrintable(QString::number(milliseconds)));
    QCOMPARE(intervalForActionId(id), milliseconds);
  }
  // Anything that is not an interval action must report 0, because QML uses
  // "greater than zero" to decide an action selects a sampling rate.
  QCOMPARE(intervalForActionId(QStringLiteral("view.pause")), 0);
  QCOMPARE(intervalForActionId(QStringLiteral("nonsense")), 0);
  QVERIFY(intervalActionId(1234).isEmpty());

  // Every interval action in the catalog must be one QML can resolve.
  for (const auto &spec : systemMonitorActions(false)) {
    if (spec.id.startsWith(QStringLiteral("view.interval"))) {
      QVERIFY2(intervalForActionId(spec.id) > 0, qPrintable(spec.id));
    }
  }
}

void TestSystemMonitorActions::singlePanelWindowsOfferNoLayoutCommands() {
  const auto single = systemMonitorActions(true);
  for (const auto &spec : single) {
    QVERIFY2(spec.menuId != QStringLiteral("layout"), qPrintable(spec.id));
  }
  // A one-panel window has no arrangement, so offering Reset would offer a
  // command that does nothing.
  QVERIFY(single.size() < systemMonitorActions(false).size());
}

void TestSystemMonitorActions::menusAreOrderedAndNamed() {
  QindaQt::AppShell::ActionRegistry registry;
  QVERIFY(registry.replaceActions(systemMonitorActions(false)).ok());
  const QVariantList menus = registry.menus();
  QCOMPARE(menus.size(), 3);
  QCOMPARE(menus.at(0).toMap().value(QStringLiteral("label")).toString(),
           QStringLiteral("View"));
  QCOMPARE(menus.at(1).toMap().value(QStringLiteral("label")).toString(),
           QStringLiteral("Layout"));
  QCOMPARE(menus.at(2).toMap().value(QStringLiteral("label")).toString(),
           QStringLiteral("Help"));
}

QTEST_MAIN(TestSystemMonitorActions)
#include "tst_system_monitor_actions.moc"
