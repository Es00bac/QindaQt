// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/workspaces/workspace_store.h"
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>
using namespace QindaQt;
using namespace QindaQt::Workspaces;
namespace {
Workspace sample(bool sameApp = false) {
  Workspace workspace;
  workspace.id = QStringLiteral("thesis");
  workspace.name = QStringLiteral("Thesis");
  workspace.color = QStringLiteral("#C26139");
  const auto root = Core::LayoutNode::makeSplit(
      QStringLiteral("split"), Core::SplitOrientation::Horizontal, 0.35,
      Core::LayoutNode::makeLeaf(QStringLiteral("left"),
                                 QStringLiteral("research")),
      Core::LayoutNode::makeLeaf(QStringLiteral("right"),
                                 QStringLiteral("writing")));
  if (!workspace.layout.addPage(
          Core::ContainerPage(QStringLiteral("page"), root)))
    qFatal("Invalid fixture");
  workspace.applicationSlots = {{QStringLiteral("research"),
                                 QStringLiteral("Research"),
                                 QStringLiteral("org.qindaqt.Terminal"),
                                 {}},
                                {QStringLiteral("writing"),
                                 QStringLiteral("Writing"),
                                 sameApp
                                     ? QStringLiteral("org.qindaqt.Terminal")
                                     : QStringLiteral("org.qindaqt.TextEditor"),
                                 {QStringLiteral("file:///tmp/thesis.txt")}}};
  return workspace;
}
QList<AvailableWindow> windows(bool sameApp = false) {
  return {
      {QStringLiteral("live-a"), QStringLiteral("org.qindaqt.Terminal"), true},
      {QStringLiteral("live-b"),
       sameApp ? QStringLiteral("org.qindaqt.Terminal")
               : QStringLiteral("org.qindaqt.TextEditor"),
       true}};
}
} // namespace
class WorkspaceTest final : public QObject {
  Q_OBJECT
private slots:
  void captureRestoresAllPagesAndRejectsStaleInventory() {
    auto original = sample();
    auto live = instantiate(original, assignWindows(original, windows()),
                            QStringLiteral("session-container"));
    QVERIFY(live.has_value());
    QVERIFY(live->addPage(QStringLiteral("second-page"),
                          QStringLiteral("third-leaf"),
                          QStringLiteral("live-c")));
    QVERIFY(live->activatePage(QStringLiteral("second-page")));
    QMap<QString, ApplicationSlot> intent{
        {QStringLiteral("live-a"), original.applicationSlots.at(0)},
        {QStringLiteral("live-b"), original.applicationSlots.at(1)},
        {QStringLiteral("live-c"),
         {QStringLiteral("notes"),
          QStringLiteral("Notes"),
          QStringLiteral("org.qindaqt.TextEditor"),
          {}}}};
    const auto saved =
        capture(original.id, original.name, original.color, *live, intent);
    QVERIFY(saved.has_value());
    QVERIFY(saved->layout.findWindow(QStringLiteral("notes")));
    QVERIFY(!saved->layout.findWindow(QStringLiteral("live-c")));
    QCOMPARE(saved->layout.activePageId(), QStringLiteral("second-page"));
    auto inventory = windows();
    inventory.append({QStringLiteral("new-c"),
                      QStringLiteral("org.qindaqt.TextEditor"), true});
    const auto rebound = instantiate(
        *saved,
        assignWindows(*saved, inventory,
                      {{QStringLiteral("notes"), QStringLiteral("new-c")}}),
        QStringLiteral("next-session"));
    QVERIFY(rebound.has_value());
    QCOMPARE(rebound->pages().size(), 2);
    QCOMPARE(rebound->activePageId(), QStringLiteral("second-page"));
    QVERIFY(rebound->findWindow(QStringLiteral("new-c")));
    intent.remove(QStringLiteral("live-c"));
    QVERIFY(
        !capture(original.id, original.name, original.color, *live, intent));
    QVERIFY(live->findWindow(QStringLiteral("live-c")));
  }

  void persistsAcrossStoreLifetime() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto workspace = sample();
    QString error;
    {
      WorkspaceStore store(directory.path());
      QVERIFY2(store.save(workspace, &error), qPrintable(error));
    }
    WorkspaceStore reopened(directory.path());
    QCOMPARE(reopened.ids(), QStringList{QStringLiteral("thesis")});
    const auto loaded = reopened.load(workspace.id, &error);
    QVERIFY2(loaded.has_value(), qPrintable(error));
    QCOMPARE(loaded->toJson(), workspace.toJson());
  }
  void rejectedSavePreservesLastWorkspace() {
    QTemporaryDir directory;
    WorkspaceStore store(directory.path());
    auto workspace = sample();
    QVERIFY(store.save(workspace));
    workspace.name.clear();
    QString error;
    QVERIFY(!store.save(workspace, &error));
    QVERIFY(!error.isEmpty());
    const auto loaded = store.load(workspace.id);
    QVERIFY(loaded.has_value());
    QCOMPARE(loaded->name, QStringLiteral("Thesis"));
  }
  void damagedFileReportsError() {
    QTemporaryDir directory;
    QFile file(directory.filePath(QStringLiteral("thesis.json")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write("{broken"), qint64(7));
    file.close();
    QString error;
    QVERIFY(!WorkspaceStore(directory.path())
                 .load(QStringLiteral("thesis"), &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), QByteArray("{broken"));
  }
  void rejectsMismatchedLayoutAndFutureSchema() {
    auto workspace = sample();
    workspace.applicationSlots.first().id = QStringLiteral("other");
    QVERIFY(!workspace.validate());
    auto json = sample().toJson();
    json[QStringLiteral("schemaVersion")] = 2;
    QVERIFY(!Workspace::fromJson(json));
  }
  void uniqueAppsBindFreshWindowIdentities() {
    const auto workspace = sample();
    const auto plan = assignWindows(workspace, windows());
    QVERIFY(plan.complete());
    QString error;
    const auto live =
        instantiate(workspace, plan, QStringLiteral("new-container"), &error);
    QVERIFY2(live.has_value(), qPrintable(error));
    QCOMPARE(live->id(), QStringLiteral("new-container"));
    QCOMPARE(live->activePageId(), workspace.layout.activePageId());
    QCOMPARE(*live->pages().first().root().ratio(), 0.35);
    QVERIFY(live->findWindow(QStringLiteral("live-a")));
    QVERIFY(live->findWindow(QStringLiteral("live-b")));
    QVERIFY(!live->findWindow(QStringLiteral("research")));
    QVERIFY(workspace.layout.findWindow(QStringLiteral("research")));
  }
  void duplicateAppsRequireChoice() {
    const auto workspace = sample(true);
    const auto plan = assignWindows(workspace, windows(true));
    QVERIFY(!plan.complete());
    QCOMPARE(plan.ambiguousSlots.size(), 2);
    QVERIFY(plan.windowsBySlot.isEmpty());
    QVERIFY(!instantiate(workspace, plan, QStringLiteral("new")));
    const auto chosen =
        assignWindows(workspace, windows(true),
                      {{QStringLiteral("writing"), QStringLiteral("live-a")}});
    QVERIFY(chosen.complete());
    QCOMPARE(chosen.windowsBySlot.value(QStringLiteral("research")),
             QStringLiteral("live-b"));
  }
  void unavailableWindowCannotBeStolen() {
    auto inventory = windows();
    inventory.first().eligible = false;
    const auto plan = assignWindows(sample(), inventory);
    QCOMPARE(plan.unassignedSlots, QStringList{QStringLiteral("research")});
    QVERIFY(plan.ambiguousSlots.isEmpty());
    QVERIFY(
        !assignWindows(sample(), inventory,
                       {{QStringLiteral("research"), QStringLiteral("live-a")}})
             .error.isEmpty());
  }
  void manualReplacementAndDuplicateRejection() {
    auto inventory = windows();
    inventory.first().desktopEntryId = QStringLiteral("replacement-terminal");
    QVERIFY(
        assignWindows(sample(), inventory,
                      {{QStringLiteral("research"), QStringLiteral("live-a")}})
            .complete());
    const QMap<QString, QString> duplicate{
        {QStringLiteral("research"), QStringLiteral("live-a")},
        {QStringLiteral("writing"), QStringLiteral("live-a")}};
    QVERIFY(!assignWindows(sample(), inventory, duplicate).error.isEmpty());
    AssignmentPlan forged;
    forged.windowsBySlot = duplicate;
    QVERIFY(!instantiate(sample(), forged, QStringLiteral("new")));
  }
};
QTEST_GUILESS_MAIN(WorkspaceTest)
#include "tst_workspaces.moc"
