// SPDX-License-Identifier: GPL-3.0-or-later
#include "fake_workspace_transport.h"

#include "qindaqt/shell/workspaces/workspace_controller.h"

#include <QSignalSpy>
#include <QtTest>

using namespace QindaQt::Shell::Workspaces;
using QindaQt::Tests::Workspaces::FakeWorkspaceTransport;
using QindaQt::Tests::Workspaces::fixtureSnapshot;

namespace {

const QString kOwner = QStringLiteral(":1.7");

WorkspaceGrants fullGrants() { return {true, true}; }

void publishReady(FakeWorkspaceTransport &transport, WorkspaceController &controller,
                  const WorkspaceSnapshot &snapshot = fixtureSnapshot())
{
  transport.announce(kOwner);
  QCOMPARE(controller.phaseText(), QStringLiteral("loading"));
  QCOMPARE(transport.snapshotRequests.size(), 1);
  QCOMPARE(transport.snapshotRequests.constLast().owner, kOwner);
  transport.reply(transport.snapshotRequests.constLast(), snapshot);
  QCOMPARE(controller.phaseText(), QStringLiteral("ready"));
}

} // namespace

class WorkspaceControllerTests final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void readDenialWithholdsObservation();
  void ownerAnnouncementFetchesAndPublishes();
  void malformedSnapshotDegradesWithoutRows();
  void changeNotificationsCoalesceIntoOneRefetch();
  void staleAndForeignRepliesAreIgnored();
  void switchIntentsAreFencedAndDispatchedOnce();
  void switchRelativeWrapsAround();
  void showDesktopToggleDispatchesAndReflectsTruth();
  void ownerLossClearsTruthAndEndsPendingAsUncertain();
};

void WorkspaceControllerTests::readDenialWithholdsObservation()
{
  FakeWorkspaceTransport transport;
  WorkspaceController controller(&transport, {false, true});
  QCOMPARE(controller.phaseText(), QStringLiteral("unavailable"));
  QCOMPARE(controller.phaseReasonText(), QStringLiteral("windows-read-not-granted"));
  transport.announce(kOwner);
  QVERIFY(transport.snapshotRequests.isEmpty());
  QVERIFY(!controller.switchTo(QStringLiteral("ws-1"), 0));
  QVERIFY(controller.feedbackPresent());
  QVERIFY(transport.switchRequests.isEmpty());
}

void WorkspaceControllerTests::ownerAnnouncementFetchesAndPublishes()
{
  FakeWorkspaceTransport transport;
  WorkspaceController controller(&transport, fullGrants());
  QSignalSpy stateSpy(&controller, &WorkspaceController::stateChanged);
  QCOMPARE(controller.phaseText(), QStringLiteral("loading"));
  publishReady(transport, controller);
  QCOMPARE(controller.count(), 3);
  QCOMPARE(controller.currentId(), QStringLiteral("ws-2"));
  QCOMPARE(controller.currentIndex(), 1);
  QCOMPARE(controller.currentName(), QStringLiteral("Code"));
  QCOMPARE(controller.revision(), quint64(1));
  QVERIFY(controller.canSwitch());
  QVERIFY(controller.canShowDesktop());
  QVERIFY(controller.accessibleName().contains(QStringLiteral("2 of 3")));
  const QVariantList rows = controller.rows();
  QCOMPARE(rows.size(), 3);
  QCOMPARE(rows.at(2).toMap().value(QStringLiteral("name")).toString(),
           QStringLiteral("Workspace 3"));
  QCOMPARE(rows.at(1).toMap().value(QStringLiteral("current")).toBool(), true);
  QCOMPARE(rows.at(0).toMap().value(QStringLiteral("revision")).toULongLong(), quint64(1));
  QVERIFY(stateSpy.size() >= 2);
}

void WorkspaceControllerTests::malformedSnapshotDegradesWithoutRows()
{
  FakeWorkspaceTransport transport;
  WorkspaceController controller(&transport, fullGrants());
  transport.announce(kOwner);
  WorkspaceSnapshot bad = fixtureSnapshot();
  bad.currentId = QStringLiteral("nope");
  transport.reply(transport.snapshotRequests.constLast(), bad);
  QCOMPARE(controller.phaseText(), QStringLiteral("degraded"));
  QCOMPARE(controller.phaseReasonText(), QStringLiteral("current-desktop-unknown"));
  QCOMPARE(controller.count(), 0);
  QVERIFY(!controller.canSwitch());

  transport.change(kOwner);
  transport.fail(transport.snapshotRequests.constLast(), QStringLiteral("desktops-read-failed"));
  QCOMPARE(controller.phaseText(), QStringLiteral("degraded"));
  QCOMPARE(controller.phaseReasonText(), QStringLiteral("desktops-read-failed"));
}

void WorkspaceControllerTests::changeNotificationsCoalesceIntoOneRefetch()
{
  FakeWorkspaceTransport transport;
  WorkspaceController controller(&transport, fullGrants());
  publishReady(transport, controller);
  transport.change(kOwner);
  QCOMPARE(transport.snapshotRequests.size(), 2);
  transport.change(kOwner);
  transport.change(kOwner);
  QCOMPARE(transport.snapshotRequests.size(), 2); // coalesced while in flight
  transport.reply(transport.snapshotRequests.constLast(),
                  fixtureSnapshot(QStringLiteral("ws-3")));
  QCOMPARE(controller.currentIndex(), 2);
  QCOMPARE(controller.revision(), quint64(2));
  QCOMPARE(transport.snapshotRequests.size(), 3); // one dirty refetch
  transport.change(QStringLiteral(":1.99"));      // foreign owner: ignored
  QCOMPARE(transport.snapshotRequests.size(), 3);
}

void WorkspaceControllerTests::staleAndForeignRepliesAreIgnored()
{
  FakeWorkspaceTransport transport;
  WorkspaceController controller(&transport, fullGrants());
  publishReady(transport, controller);
  transport.change(kOwner);
  const auto inflight = transport.snapshotRequests.constLast();
  Q_EMIT transport.snapshotReceived(inflight.token + 40, kOwner,
                                    fixtureSnapshot(QStringLiteral("ws-1")));
  Q_EMIT transport.snapshotReceived(inflight.token, QStringLiteral(":1.8"),
                                    fixtureSnapshot(QStringLiteral("ws-1")));
  QCOMPARE(controller.currentId(), QStringLiteral("ws-2"));
  QCOMPARE(controller.revision(), quint64(1));
  transport.reply(inflight, fixtureSnapshot(QStringLiteral("ws-1")));
  QCOMPARE(controller.currentId(), QStringLiteral("ws-1"));
}

void WorkspaceControllerTests::switchIntentsAreFencedAndDispatchedOnce()
{
  FakeWorkspaceTransport transport;
  WorkspaceController controller(&transport, fullGrants());
  publishReady(transport, controller);

  QVERIFY(!controller.switchTo(QStringLiteral("ws-1"), controller.revision() + 1));
  QVERIFY(controller.feedback().contains(QStringLiteral("changed")));
  QVERIFY(!controller.switchTo(QStringLiteral("ws-9"), controller.revision()));
  QVERIFY(controller.feedback().contains(QStringLiteral("no longer exists")));
  QVERIFY(transport.switchRequests.isEmpty());

  QVERIFY(controller.switchTo(QStringLiteral("ws-2"), controller.revision()));
  QVERIFY(!controller.feedbackPresent());
  QVERIFY(transport.switchRequests.isEmpty()); // already current: no-op

  QVERIFY(controller.switchTo(QStringLiteral("ws-3"), controller.revision()));
  QCOMPARE(transport.switchRequests.size(), 1);
  QCOMPARE(transport.switchRequests.constLast().owner, kOwner);
  QCOMPARE(transport.switchRequests.constLast().desktopId, QStringLiteral("ws-3"));
  QVERIFY(controller.operationPending());
  QVERIFY(!controller.canSwitch());
  QVERIFY(!controller.switchTo(QStringLiteral("ws-1"), controller.revision()));
  QCOMPARE(transport.switchRequests.size(), 1);

  transport.finishSwitch(transport.switchRequests.constLast(), true);
  QVERIFY(!controller.operationPending());
  QVERIFY(!controller.feedbackPresent());

  QVERIFY(controller.switchTo(QStringLiteral("ws-1"), controller.revision()));
  transport.finishSwitch(transport.switchRequests.constLast(), false,
                         QStringLiteral("org.freedesktop.DBus.Error.Failed"));
  QVERIFY(!controller.operationPending());
  QVERIFY(controller.feedback().contains(QStringLiteral("Could not switch")));
  controller.clearFeedback();
  QVERIFY(!controller.feedbackPresent());
}

void WorkspaceControllerTests::switchRelativeWrapsAround()
{
  FakeWorkspaceTransport transport;
  WorkspaceController controller(&transport, fullGrants());
  publishReady(transport, controller, fixtureSnapshot(QStringLiteral("ws-3")));
  QVERIFY(controller.switchRelative(1));
  QCOMPARE(transport.switchRequests.constLast().desktopId, QStringLiteral("ws-1"));
  transport.finishSwitch(transport.switchRequests.constLast(), true);
  transport.change(kOwner);
  transport.reply(transport.snapshotRequests.constLast(),
                  fixtureSnapshot(QStringLiteral("ws-1")));
  QVERIFY(controller.switchRelative(-1));
  QCOMPARE(transport.switchRequests.constLast().desktopId, QStringLiteral("ws-3"));
}

void WorkspaceControllerTests::showDesktopToggleDispatchesAndReflectsTruth()
{
  FakeWorkspaceTransport transport;
  WorkspaceController controller(&transport, fullGrants());
  publishReady(transport, controller);
  QVERIFY(!controller.showingDesktop());
  QVERIFY(controller.toggleShowingDesktop());
  QCOMPARE(transport.showDesktopRequests.size(), 1);
  QCOMPARE(transport.showDesktopRequests.constLast().showing, true);
  QVERIFY(!controller.canShowDesktop());
  transport.finishShowDesktop(transport.showDesktopRequests.constLast(), true);
  transport.change(kOwner);
  transport.reply(transport.snapshotRequests.constLast(),
                  fixtureSnapshot(QStringLiteral("ws-2"), true));
  QVERIFY(controller.showingDesktop());
  QVERIFY(controller.setShowingDesktop(true)); // already shown: no dispatch
  QCOMPARE(transport.showDesktopRequests.size(), 1);
  QVERIFY(controller.toggleShowingDesktop());
  QCOMPARE(transport.showDesktopRequests.constLast().showing, false);
}

void WorkspaceControllerTests::ownerLossClearsTruthAndEndsPendingAsUncertain()
{
  FakeWorkspaceTransport transport;
  WorkspaceController controller(&transport, fullGrants());
  publishReady(transport, controller);
  QVERIFY(controller.switchTo(QStringLiteral("ws-1"), controller.revision()));
  const auto pending = transport.switchRequests.constLast();
  transport.announce(QString{}, QStringLiteral("compositor-owner-changed"));
  QCOMPARE(controller.phaseText(), QStringLiteral("unavailable"));
  QCOMPARE(controller.phaseReasonText(), QStringLiteral("compositor-owner-changed"));
  QCOMPARE(controller.count(), 0);
  QVERIFY(!controller.operationPending());
  QVERIFY(controller.feedback().contains(QStringLiteral("compositor changed")));
  transport.finishSwitch(pending, true); // late reply: ignored
  QVERIFY(controller.feedbackPresent());

  transport.announce(QStringLiteral(":1.8"));
  QCOMPARE(controller.phaseText(), QStringLiteral("loading"));
  QCOMPARE(transport.snapshotRequests.constLast().owner, QStringLiteral(":1.8"));
}

QTEST_GUILESS_MAIN(WorkspaceControllerTests)
#include "tst_workspace_controller.moc"
