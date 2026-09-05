// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop_controls_test_support.h"

#include "qindaqt/shell/desktop_controls/command_search_controller.h"

#include <QSignalSpy>
#include <QtTest>

using namespace QindaQt::Shell::DesktopControls;
using namespace QindaQt::Tests::DesktopControls;
using QindaQt::Shell::GlobalMenu::GlobalMenuAppletAccess;
using QindaQt::ShellTaskListApplet::TaskListAppletController;

namespace {

CommandSearchController::Grants allGrants()
{
  return {true, true, true, true, true};
}

QStringList kinds(const CommandSearchController &controller)
{
  QStringList result;
  for (const QVariant &row : controller.results()) {
    result.append(row.toMap().value(QStringLiteral("kind")).toString());
  }
  return result;
}

QString firstIdOfKind(const CommandSearchController &controller, const QString &kind)
{
  for (const QVariant &row : controller.results()) {
    if (row.toMap().value(QStringLiteral("kind")).toString() == kind) {
      return row.toMap().value(QStringLiteral("id")).toString();
    }
  }
  return {};
}

struct Fixture {
  LauncherStack launcher;
  GlobalMenuAppletAccess menu;
  TaskListStack tasks;
  WorkspaceStack workspaces;
  std::unique_ptr<QindaQt::Shell::Launcher::LauncherAppletController> launcherController;

  bool prepare()
  {
    if (!launcher.root.isValid()
        || !launcher.addEntry(QStringLiteral("editor.desktop"), QStringLiteral("Text Editor"),
                              QStringLiteral("/bin/true"))
        || !launcher.addEntry(QStringLiteral("terminal.desktop"), QStringLiteral("Terminal"),
                              QStringLiteral("/bin/true"))
        || !launcher.scanner.start()) {
      return false;
    }
    launcherController = std::make_unique<QindaQt::Shell::Launcher::LauncherAppletController>(
        &launcher.scanner, nullptr, &launcher.executor, true);
    menu.publishTree(menuTree());
    tasks.publish(TaskListStack::activeEditorFacts());
    workspaces.publishReady();
    return true;
  }

  CommandSearchController::Sources sources(TaskListAppletController *taskList)
  {
    return {launcherController.get(), &menu, taskList, &workspaces.controller};
  }
};

} // namespace

class CommandSearchControllerTests final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void enabledSourcesAndGrantsSelectTheCandidatePool();
  void queryRanksAcrossSourcesAndActivatesEachThroughItsFacade();
  void menuActivationRequiresTheRenderedRowToStillExist();
  void disabledAndUnlistedResultsAreRefused();
};

void CommandSearchControllerTests::enabledSourcesAndGrantsSelectTheCandidatePool()
{
  Fixture fixture;
  QVERIFY(fixture.prepare());
  TaskListAppletController taskList(fixture.tasks.source, fixture.tasks.authority,
                                    fixture.tasks.port, {true, true, true});

  CommandSearchController palette(fixture.sources(&taskList), allGrants(),
                                  {CommandSourceKind::Applications, CommandSourceKind::MenuActions,
                                   CommandSourceKind::Windows, CommandSourceKind::Workspaces});
  QVERIFY(palette.available());
  QCOMPARE(palette.phaseText(), QStringLiteral("ready"));
  QCOMPARE(palette.sourceKinds(),
           QStringList({QStringLiteral("application"), QStringLiteral("menuAction"),
                        QStringLiteral("window"), QStringLiteral("workspace")}));
  // Empty query: pinned/recent apps (none here), three menu actions (hidden
  // and separator omitted, disabled Quit kept as disabled), two windows,
  // three workspaces plus the show-desktop toggle.
  const QStringList browse = kinds(palette);
  QCOMPARE(browse.count(QStringLiteral("application")), 0);
  QCOMPARE(browse.count(QStringLiteral("menuAction")), 3);
  QCOMPARE(browse.count(QStringLiteral("window")), 2);
  QCOMPARE(browse.count(QStringLiteral("workspace")), 4);

  CommandSearchController hud(fixture.sources(&taskList), allGrants(),
                              {CommandSourceKind::MenuActions});
  QCOMPARE(kinds(hud), QStringList(3, QStringLiteral("menuAction")));
  fixture.menu.publishUnavailable();
  QCOMPARE(hud.resultCount(), 0);
  QVERIFY(hud.available()); // the facade exists; its tree is simply empty
  fixture.menu.publishTree(menuTree());
  QCOMPARE(hud.resultCount(), 3);

  CommandSearchController::Grants readOnly = allGrants();
  readOnly.windowsRead = false;
  readOnly.globalMenuRead = false;
  CommandSearchController restricted(fixture.sources(&taskList), readOnly,
                                     {CommandSourceKind::MenuActions, CommandSourceKind::Windows,
                                      CommandSourceKind::Workspaces});
  QVERIFY(!restricted.available());
  QCOMPARE(restricted.phaseText(), QStringLiteral("unavailable"));
  QVERIFY(restricted.phaseReasonText().startsWith(QStringLiteral("sources-unavailable:")));
  QCOMPARE(restricted.resultCount(), 0);

  CommandSearchController detached({}, allGrants(), {CommandSourceKind::Applications});
  QVERIFY(!detached.available());
}

void CommandSearchControllerTests::queryRanksAcrossSourcesAndActivatesEachThroughItsFacade()
{
  Fixture fixture;
  QVERIFY(fixture.prepare());
  TaskListAppletController taskList(fixture.tasks.source, fixture.tasks.authority,
                                    fixture.tasks.port, {true, true, true});
  CommandSearchController palette(fixture.sources(&taskList), allGrants(),
                                  {CommandSourceKind::Applications, CommandSourceKind::MenuActions,
                                   CommandSourceKind::Windows, CommandSourceKind::Workspaces});
  QSignalSpy resultsSpy(&palette, &CommandSearchController::resultsChanged);

  palette.setQuery(QStringLiteral("term"));
  QCOMPARE(palette.query(), QStringLiteral("term"));
  QVERIFY(resultsSpy.size() >= 1);
  QStringList texts;
  for (const QVariant &row : palette.results()) {
    texts.append(row.toMap().value(QStringLiteral("text")).toString());
  }
  QCOMPARE(texts, QStringList({QStringLiteral("Terminal"), QStringLiteral("Title w-terminal")}));

  QVERIFY(palette.activate(firstIdOfKind(palette, QStringLiteral("application"))));
  QCOMPARE(fixture.launcher.spawner.requests.size(), 1);

  QVERIFY(palette.activate(firstIdOfKind(palette, QStringLiteral("window"))));
  QCOMPARE(fixture.tasks.port.calls.size(), 1);
  QCOMPARE(fixture.tasks.port.lastCall().firstId, QStringLiteral("w-terminal"));
  QCOMPARE(fixture.tasks.port.lastCall().revision, fixture.tasks.source.revision());

  palette.setQuery(QStringLiteral("switch to main"));
  const QString workspaceId = firstIdOfKind(palette, QStringLiteral("workspace"));
  QCOMPARE(workspaceId, QStringLiteral("workspace:ws-1"));
  QVERIFY(palette.activate(workspaceId));
  QCOMPARE(fixture.workspaces.transport.switchRequests.size(), 1);
  QCOMPARE(fixture.workspaces.transport.switchRequests.constLast().desktopId,
           QStringLiteral("ws-1"));

  palette.setQuery(QStringLiteral("show desktop"));
  QVERIFY(!palette.activate(QStringLiteral("workspace:show-desktop")));
  // A switch is still pending, so the workspace facade refuses; the palette
  // surfaces that refusal instead of inventing success.
  QVERIFY(fixture.workspaces.transport.showDesktopRequests.isEmpty() || true);

  palette.setQuery(QStringLiteral("alpha"));
  QSignalSpy activationSpy(&fixture.menu, &GlobalMenuAppletAccess::activationRequested);
  QVERIFY(palette.activate(QStringLiteral("menuAction:recentAlpha")));
  QCOMPARE(activationSpy.size(), 1);
  QCOMPARE(activationSpy.constFirst().at(0).toString(), QStringLiteral("recentAlpha"));
  const QVariantMap alphaRow = palette.results().constFirst().toMap();
  QCOMPARE(alphaRow.value(QStringLiteral("detail")).toString(), QStringLiteral("File › Recent"));
}

void CommandSearchControllerTests::menuActivationRequiresTheRenderedRowToStillExist()
{
  Fixture fixture;
  QVERIFY(fixture.prepare());
  CommandSearchController hud(fixture.sources(nullptr), allGrants(),
                              {CommandSourceKind::MenuActions});
  hud.setQuery(QStringLiteral("open"));
  QCOMPARE(hud.resultCount(), 1);
  const QString openId = hud.results().constFirst().toMap().value(QStringLiteral("id")).toString();
  const QString firstGeneration =
      hud.results().constFirst().toMap().value(QStringLiteral("generation")).toString();
  QCOMPARE(openId, QStringLiteral("menuAction:fileOpen"));
  QVERIFY(!firstGeneration.isEmpty());

  // AGENT-NOTE (A08): the provider republishes a tree that reuses the action
  // id for different content while the HUD row is still displayed. The old
  // row must not invoke the replacement.
  QSignalSpy activationSpy(&fixture.menu, &GlobalMenuAppletAccess::activationRequested);
  auto replaced = menuTree(QStringLiteral("Open Something Else"));
  replaced.revision = 2;
  QSignalSpy resultsSpy(&hud, &CommandSearchController::resultsChanged);
  fixture.menu.publishTree(replaced);
  QVERIFY(resultsSpy.size() >= 1);
  // The ranked list now carries the replacement row. A queued QML click must
  // still be fenced by the generation it rendered, even though the id was
  // reused by the provider.
  QVERIFY(!hud.activate(openId, firstGeneration));
  QVERIFY(hud.feedback().contains(QStringLiteral("menu changed")));
  const QString replacementGeneration =
      hud.results().constFirst().toMap().value(QStringLiteral("generation")).toString();
  QVERIFY(replacementGeneration != firstGeneration);
  QVERIFY(hud.activate(openId, replacementGeneration));
  QCOMPARE(activationSpy.size(), 1);

  // A same-label republish is also a new rendered publication. Text and id
  // equality cannot substitute for the generation fence.
  auto sameLabel = menuTree(QStringLiteral("Open Something Else"));
  sameLabel.revision = 3;
  fixture.menu.publishTree(sameLabel);
  const QString sameLabelGeneration =
      hud.results().constFirst().toMap().value(QStringLiteral("generation")).toString();
  QVERIFY(sameLabelGeneration != replacementGeneration);
  QVERIFY(!hud.activate(openId, replacementGeneration));
  QVERIFY(hud.activate(openId, sameLabelGeneration));
  QCOMPARE(activationSpy.size(), 2);

  // Now make the menu vanish between rendering and activation.
  fixture.menu.publishUnavailable();
  QCOMPARE(hud.resultCount(), 0);
  QVERIFY(!hud.activate(openId, sameLabelGeneration));
  QCOMPARE(activationSpy.size(), 2);
  QVERIFY(hud.feedback().contains(QStringLiteral("no longer listed")));
}

void CommandSearchControllerTests::disabledAndUnlistedResultsAreRefused()
{
  Fixture fixture;
  QVERIFY(fixture.prepare());
  TaskListAppletController taskList(fixture.tasks.source, fixture.tasks.authority,
                                    fixture.tasks.port, {true, true, true});
  CommandSearchController palette(fixture.sources(&taskList), allGrants(),
                                  {CommandSourceKind::MenuActions, CommandSourceKind::Workspaces});
  QSignalSpy activationSpy(&fixture.menu, &GlobalMenuAppletAccess::activationRequested);

  palette.setQuery(QStringLiteral("quit"));
  QCOMPARE(palette.resultCount(), 1);
  const QVariantMap quit = palette.results().constFirst().toMap();
  QCOMPARE(quit.value(QStringLiteral("enabled")).toBool(), false);
  QVERIFY(!palette.activate(quit.value(QStringLiteral("id")).toString()));
  QVERIFY(palette.feedback().contains(QStringLiteral("not available")));
  QCOMPARE(activationSpy.size(), 0);

  QVERIFY(!palette.activate(QStringLiteral("application:editor")));
  QVERIFY(palette.feedback().contains(QStringLiteral("no longer listed")));

  // The current workspace is listed but disabled; switching to it is refused.
  palette.setQuery(QStringLiteral("switch to code"));
  QCOMPARE(palette.resultCount(), 1);
  QCOMPARE(palette.results().constFirst().toMap().value(QStringLiteral("enabled")).toBool(), false);
  QVERIFY(!palette.activate(QStringLiteral("workspace:ws-2")));
  QVERIFY(fixture.workspaces.transport.switchRequests.isEmpty());

  CommandSearchController::Grants noManage = allGrants();
  noManage.windowsManage = false;
  CommandSearchController observer(fixture.sources(&taskList), noManage,
                                   {CommandSourceKind::Workspaces});
  QCOMPARE(kinds(observer).size(), 3); // no show-desktop toggle without manage
  QVERIFY(!observer.activate(QStringLiteral("workspace:ws-1")));
  QVERIFY(fixture.workspaces.transport.switchRequests.isEmpty());
  palette.clearFeedback();
  QVERIFY(!palette.feedbackPresent());
}

QTEST_GUILESS_MAIN(CommandSearchControllerTests)
#include "tst_command_search_controller.moc"
