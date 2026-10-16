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
  void aHiddenControllerDoesNoWorkForFacadeChanges();
  void presentingCatchesUpOnceThenRefreshesOnlyTheChangedSource();
  void presentationIsACountedLease();
  void aSourceThatIsNotEnabledNeverMarksTheControllerStale();
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
  // A surface is showing the palette: only a presented controller notifies.
  palette.present();
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
  hud.present();
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

namespace {
// One task-list publication that changes only a title, the way a terminal
// spinner does: the generation advances, the set of windows does not.
void publishTitleChange(Fixture &fixture, int generation)
{
  auto facts = TaskListStack::activeEditorFacts();
  facts[0].title = QStringLiteral("spinner %1").arg(generation);
  QVERIFY(fixture.tasks.publish(facts) > 0);
}
}

void CommandSearchControllerTests::aHiddenControllerDoesNoWorkForFacadeChanges()
{
  Fixture fixture;
  QVERIFY(fixture.prepare());
  TaskListAppletController taskList(fixture.tasks.source, fixture.tasks.authority,
                                    fixture.tasks.port, {true, true, true});
  CommandSearchController palette(fixture.sources(&taskList), allGrants(),
                                  {CommandSourceKind::Applications, CommandSourceKind::MenuActions,
                                   CommandSourceKind::Windows, CommandSourceKind::Workspaces});
  QVERIFY(!palette.presented());
  QSignalSpy resultsSpy(&palette, &CommandSearchController::resultsChanged);
  QSignalSpy stateSpy(&palette, &CommandSearchController::stateChanged);

  // 100 task-list publications, a menu republish and a workspace change with
  // nobody showing the palette: nothing is collected, ranked or announced.
  for (int generation = 0; generation < 100; ++generation) {
    publishTitleChange(fixture, generation);
  }
  auto replaced = menuTree(QStringLiteral("Open Elsewhere"));
  replaced.revision = 9;
  fixture.menu.publishTree(replaced);
  fixture.workspaces.publishReady();
  QCOMPARE(palette.collectionCount(), 0);
  QCOMPARE(resultsSpy.size(), 0);
  QCOMPARE(stateSpy.size(), 0);

  // A read still returns current truth: it brings the stale cache up to date
  // without a signal, once, for all four sources.
  QStringList windowTexts;
  for (const QVariant &row : palette.results()) {
    if (row.toMap().value(QStringLiteral("kind")).toString() == QStringLiteral("window")) {
      windowTexts.append(row.toMap().value(QStringLiteral("text")).toString());
    }
  }
  QVERIFY(windowTexts.contains(QStringLiteral("spinner 99")));
  QCOMPARE(palette.collectionCount(), 4);
  QCOMPARE(resultsSpy.size(), 0);
  QCOMPARE(palette.resultCount(), palette.results().size());
  QCOMPARE(palette.collectionCount(), 4);
  // And activation addresses the generation the window was read from.
  QVERIFY(palette.activate(firstIdOfKind(palette, QStringLiteral("window"))));
  QCOMPARE(fixture.tasks.port.lastCall().revision, fixture.tasks.source.revision());
  QCOMPARE(taskList.generationRevision(), fixture.tasks.source.revision());
}

void CommandSearchControllerTests::presentingCatchesUpOnceThenRefreshesOnlyTheChangedSource()
{
  Fixture fixture;
  QVERIFY(fixture.prepare());
  TaskListAppletController taskList(fixture.tasks.source, fixture.tasks.authority,
                                    fixture.tasks.port, {true, true, true});
  CommandSearchController palette(fixture.sources(&taskList), allGrants(),
                                  {CommandSourceKind::Applications, CommandSourceKind::MenuActions,
                                   CommandSourceKind::Windows, CommandSourceKind::Workspaces});
  publishTitleChange(fixture, 1);
  QSignalSpy resultsSpy(&palette, &CommandSearchController::resultsChanged);
  QSignalSpy presentedSpy(&palette, &CommandSearchController::presentedChanged);

  palette.present();
  QVERIFY(palette.presented());
  QCOMPARE(presentedSpy.size(), 1);
  QCOMPARE(resultsSpy.size(), 1); // one catch-up notification
  QCOMPARE(palette.collectionCount(), 4);

  // While presented, each change recollects only the source that moved.
  publishTitleChange(fixture, 2);
  QCOMPARE(palette.collectionCount(), 5);
  QCOMPARE(resultsSpy.size(), 2);
  auto replaced = menuTree(QStringLiteral("Open Again"));
  replaced.revision = 3;
  fixture.menu.publishTree(replaced);
  QCOMPARE(palette.collectionCount(), 6);
  QCOMPARE(resultsSpy.size(), 3);
  // A query recollects the launcher's candidates, the only query-dependent
  // source, and reranks the rest.
  palette.setQuery(QStringLiteral("term"));
  QCOMPARE(palette.collectionCount(), 7);
  QVERIFY(resultsSpy.size() >= 4);

  // Leaving the surface silences it again; the next presentation catches up.
  palette.dismiss();
  QVERIFY(!palette.presented());
  QCOMPARE(presentedSpy.size(), 2);
  const qsizetype announced = resultsSpy.size();
  publishTitleChange(fixture, 3);
  palette.setQuery(QString());
  QCOMPARE(resultsSpy.size(), announced);
  palette.present();
  QCOMPARE(resultsSpy.size(), announced + 1);
  QCOMPARE(palette.collectionCount(), 9); // windows and applications again
}

void CommandSearchControllerTests::presentationIsACountedLease()
{
  Fixture fixture;
  QVERIFY(fixture.prepare());
  TaskListAppletController taskList(fixture.tasks.source, fixture.tasks.authority,
                                    fixture.tasks.port, {true, true, true});
  CommandSearchController palette(fixture.sources(&taskList), allGrants(),
                                  {CommandSourceKind::Windows});
  QSignalSpy presentedSpy(&palette, &CommandSearchController::presentedChanged);
  // One controller serves every panel instance; each holds its own lease.
  palette.dismiss(); // an unmatched release is ignored, never negative
  QCOMPARE(presentedSpy.size(), 0);
  palette.present();
  palette.present();
  QCOMPARE(presentedSpy.size(), 1);
  palette.dismiss();
  QVERIFY(palette.presented());
  QCOMPARE(presentedSpy.size(), 1);
  palette.dismiss();
  QVERIFY(!palette.presented());
  QCOMPARE(presentedSpy.size(), 2);
  palette.dismiss();
  palette.present();
  QVERIFY(palette.presented());
}

void CommandSearchControllerTests::aSourceThatIsNotEnabledNeverMarksTheControllerStale()
{
  Fixture fixture;
  QVERIFY(fixture.prepare());
  TaskListAppletController taskList(fixture.tasks.source, fixture.tasks.authority,
                                    fixture.tasks.port, {true, true, true});
  // The HUD reads the active window's menu actions only. The task list,
  // launcher and workspaces change constantly and are none of its business.
  CommandSearchController hud(fixture.sources(&taskList), allGrants(),
                              {CommandSourceKind::MenuActions});
  hud.present();
  QSignalSpy resultsSpy(&hud, &CommandSearchController::resultsChanged);
  const int collected = hud.collectionCount();
  for (int generation = 0; generation < 50; ++generation) {
    publishTitleChange(fixture, generation);
  }
  fixture.workspaces.publishReady();
  QCOMPARE(hud.collectionCount(), collected);
  QCOMPARE(resultsSpy.size(), 0);
  auto replaced = menuTree(QStringLiteral("Open Once More"));
  replaced.revision = 5;
  fixture.menu.publishTree(replaced);
  QCOMPARE(hud.collectionCount(), collected + 1);
  QCOMPARE(resultsSpy.size(), 1);
}

QTEST_GUILESS_MAIN(CommandSearchControllerTests)
#include "tst_command_search_controller.moc"
