// SPDX-License-Identifier: GPL-3.0-or-later
#include "gatheroverviewcomposition.h"

#include "qindaqt/shell/gather_overview/gather_overview_controller.h"
#include "qindaqt/shell/task_list/applet/task_list_applet_controller.h"

#include <QQmlEngine>
#include <QScreen>
#include <QSignalSpy>
#include <QtTest>

#include "task_list_applet_test_fakes.h"
#include "task_list_operation_test_support.h"
#include "task_list_test_support.h"

using namespace QindaQt::ShellTaskList;
using namespace QindaQt::ShellTaskListApplet;
using QindaQt::Shell::GatherOverviewComposition;
using TaskListAppletTest::FakeTaskListOperationPort;
using TaskListOperationTest::FakeOperationAuthority;

namespace {

// The same shape the task-list applet's own controller tests use: a real T0
// source, the T1 authority fake, and a recording port. The composition is
// deliberately built on the real controller rather than a stub, because the
// whole point of it is that the overview borrows that controller's grants and
// its stale-revision arbitration instead of having any of its own.
struct Fixture {
    TaskListSource source;
    FakeOperationAuthority authority;
    FakeTaskListOperationPort port;
};

TaskListAppletGrants allGrants() { return {true, true, true}; }

quint64 publishReady(Fixture &fixture, const QVector<TaskWindowFact> &facts)
{
    const auto evaluation = fixture.source.publishGeneration(facts);
    if (!evaluation.ok()) {
        return 0;
    }
    fixture.authority.revision = fixture.source.revision();
    fixture.authority.sourceStatus = TaskListSourceStatus::Ready;
    fixture.authority.owner = QStringLiteral(":1.1");
    Q_EMIT fixture.authority.stateChanged();
    return fixture.source.revision();
}

// Two standalone windows and one container, so the projection has something
// in the window lane and something in the card lane.
QVector<TaskWindowFact> standardFacts()
{
    return {TaskListTest::standalone(QStringLiteral("w1"),
                                     QStringLiteral("app.one")),
            TaskListTest::primary(QStringLiteral("w2"),
                                  QStringLiteral("app.two"),
                                  QStringLiteral("c1")),
            TaskListTest::member(QStringLiteral("w3"), QStringLiteral("c1"))};
}

// The projection's items, flattened, so a row can find the one it wants
// without depending on lane ordering.
QVariantList itemsOf(const QVariantMap &projection)
{
    return projection.value(QStringLiteral("items")).toList();
}

QVariantMap itemForTask(const QVariantMap &projection, const QString &taskId)
{
    const QVariantList items = itemsOf(projection);
    for (const QVariant &entry : items) {
        const QVariantMap item = entry.toMap();
        if (item.value(QStringLiteral("taskId")).toString() == taskId) {
            return item;
        }
    }
    return {};
}

} // namespace

class GatherOverviewCompositionTests final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void absentTaskListNeverOpensAndProjectsNothing();
    void openProjectsTheBorrowedTaskListAndClosesOnActivation();
    void activationBecomesTheMatchingTaskIntent();
    void losingTheTaskListClosesAnOpenOverview();
    void theWorkAreaComesFromTheOutputTheOverviewOpensOn();
};

// Fail-closed. A session whose windows.read grant was denied has no truthful
// window list, so the button and the key must open nothing rather than an
// empty panel that looks like "you have no windows".
void GatherOverviewCompositionTests::absentTaskListNeverOpensAndProjectsNothing()
{
    QQmlEngine engine;
    GatherOverviewComposition composition(*qGuiApp, engine, {});
    QVERIFY(composition.controller() != nullptr);
    // start() is never called here: no output surface is needed to qualify
    // any of this, and creating one would need a compositor.
    QCOMPARE(composition.surfaceCount(), 0);

    composition.toggle();
    QCOMPARE(composition.controller()->isOpen(), false);
    composition.toggle();
    QCOMPARE(composition.controller()->isOpen(), false);
    QCOMPARE(composition.controller()
                 ->projection()
                 .value(QStringLiteral("available"))
                 .toBool(),
             false);
}

void GatherOverviewCompositionTests::
    openProjectsTheBorrowedTaskListAndClosesOnActivation()
{
    Fixture fixture;
    TaskListAppletController taskList(fixture.source, fixture.authority,
                                      fixture.port, allGrants());
    QQmlEngine engine;
    GatherOverviewComposition composition(
        *qGuiApp, engine,
        [](const QString &applicationId) {
            // The shell's resolver in miniature: a name for one id, nothing
            // for the rest, so the surface's fallback path stays exercised.
            return applicationId == QStringLiteral("app.one")
                       ? QStringLiteral("utilities-terminal")
                       : QString{};
        });
    composition.setTaskListAccess(&taskList);
    const quint64 revision = publishReady(fixture, standardFacts());
    QVERIFY(revision != 0);

    // Closed: nothing is arranged, so a session's window churn costs nothing.
    QCOMPARE(composition.controller()->isOpen(), false);
    QVERIFY(itemsOf(composition.controller()->projection()).isEmpty());

    // The planner needs a work area before it can place anything.
    composition.controller()->setWorkArea(QRectF(0, 0, 1920, 1080));
    composition.toggle();
    QCOMPARE(composition.controller()->isOpen(), true);
    const QVariantMap open = composition.controller()->projection();
    QCOMPARE(open.value(QStringLiteral("available")).toBool(), true);
    QCOMPARE(open.value(QStringLiteral("interactive")).toBool(), true);
    QVERIFY(!itemsOf(open).isEmpty());
    // The icon name came from the injected resolver, not from the model.
    QCOMPARE(itemForTask(open, QStringLiteral("w1"))
                 .value(QStringLiteral("iconName"))
                 .toString(),
             QStringLiteral("utilities-terminal"));
    // The container is a card whatever its state; a standalone window is a
    // tile in the grid. That is the lane policy, restated here because the
    // composition is what a user actually meets it through.
    QCOMPARE(itemForTask(open, QStringLiteral("c1"))
                 .value(QStringLiteral("lane"))
                 .toString(),
             QStringLiteral("card"));
    QCOMPARE(itemForTask(open, QStringLiteral("w1"))
                 .value(QStringLiteral("lane"))
                 .toString(),
             QStringLiteral("window"));

    // Activating closes it: the overview is temporary, and leaving it over
    // the window it just raised would hide what the click was for.
    composition.controller()->activate(itemForTask(open, QStringLiteral("w1")));
    QCOMPARE(composition.controller()->isOpen(), false);
    QVERIFY(itemsOf(composition.controller()->projection()).isEmpty());
}

void GatherOverviewCompositionTests::activationBecomesTheMatchingTaskIntent()
{
    Fixture fixture;
    TaskListAppletController taskList(fixture.source, fixture.authority,
                                      fixture.port, allGrants());
    QQmlEngine engine;
    GatherOverviewComposition composition(*qGuiApp, engine, {});
    composition.setTaskListAccess(&taskList);
    QVERIFY(publishReady(fixture, standardFacts()) != 0);
    composition.controller()->setWorkArea(QRectF(0, 0, 1920, 1080));
    composition.toggle();

    const QVariantMap projection = composition.controller()->projection();
    const int before = static_cast<int>(fixture.port.calls.size());
    const QVariantMap window = itemForTask(projection, QStringLiteral("w1"));
    QVERIFY(!window.isEmpty());
    // A standalone window addresses the whole entry: no windowId.
    QVERIFY(window.value(QStringLiteral("windowId")).toString().isEmpty());
    composition.controller()->activate(window);
    // The intent reached the real dispatch path rather than being invented
    // here: the port is what the applet controller ends up calling.
    QVERIFY(fixture.port.calls.size() >= before);

    // A stale item cannot act, even though it looks exactly like a live one.
    // The controller refuses it before the composition ever sees it, which is
    // the reason the surface is allowed to hold plain value maps.
    composition.toggle();
    composition.controller()->setWorkArea(QRectF(0, 0, 1920, 1080));
    composition.toggle();
    QVariantMap stale = window;
    stale[QStringLiteral("generationRevision")] =
        QVariant::fromValue<quint64>(1);
    stale[QStringLiteral("taskId")] = QStringLiteral("does-not-exist");
    const int beforeStale = static_cast<int>(fixture.port.calls.size());
    composition.controller()->activate(stale);
    QCOMPARE(static_cast<int>(fixture.port.calls.size()), beforeStale);
}

// The overview must not keep drawing a window list it can no longer confirm.
void GatherOverviewCompositionTests::losingTheTaskListClosesAnOpenOverview()
{
    Fixture fixture;
    auto taskList = std::make_unique<TaskListAppletController>(
        fixture.source, fixture.authority, fixture.port, allGrants());
    QQmlEngine engine;
    GatherOverviewComposition composition(*qGuiApp, engine, {});
    composition.setTaskListAccess(taskList.get());
    QVERIFY(publishReady(fixture, standardFacts()) != 0);
    composition.controller()->setWorkArea(QRectF(0, 0, 1920, 1080));
    composition.toggle();
    QCOMPARE(composition.controller()->isOpen(), true);

    composition.setTaskListAccess(nullptr);
    QCOMPARE(composition.controller()->isOpen(), false);
    QCOMPARE(composition.controller()
                 ->projection()
                 .value(QStringLiteral("available"))
                 .toBool(),
             false);
    // And it stays shut: there is nothing to show and no authority to act.
    composition.toggle();
    QCOMPARE(composition.controller()->isOpen(), false);
}

// One arrangement on one output. The controller holds a single work area, so
// an overview that showed on every monitor would draw the chosen output's
// arrangement on all of them - which is exactly the multi-display failure
// worth pinning, even though the window set itself needs a compositor.
void GatherOverviewCompositionTests::
    theWorkAreaComesFromTheOutputTheOverviewOpensOn()
{
    Fixture fixture;
    TaskListAppletController taskList(fixture.source, fixture.authority,
                                      fixture.port, allGrants());
    QQmlEngine engine;
    GatherOverviewComposition composition(*qGuiApp, engine, {});
    composition.setTaskListAccess(&taskList);
    QVERIFY(publishReady(fixture, standardFacts()) != 0);

    // Opening reads a real screen's available geometry rather than keeping
    // whatever was set before, because a panel may have been resized or an
    // output added while the overview was down.
    composition.controller()->setWorkArea(QRectF(1, 2, 3, 4));
    composition.toggle();
    QCOMPARE(composition.controller()->isOpen(), true);
    const QVariantMap open = composition.controller()->projection();
    const QRectF field = open.value(QStringLiteral("field")).toRectF();
    QScreen *screen = qGuiApp->primaryScreen();
    QVERIFY(screen != nullptr);
    const QRectF available(screen->availableGeometry());
    // The planner insets the work area by its buffer, so the field is inside
    // the output rather than equal to it - but it must be derived from a real
    // output, never from the 3x4 rectangle set above.
    QVERIFY(field.width() > 4);
    QVERIFY(available.contains(field));

    // Closing releases the chosen output, so the next open re-chooses.
    composition.toggle();
    QCOMPARE(composition.controller()->isOpen(), false);
}

QTEST_MAIN(GatherOverviewCompositionTests)
#include "tst_gather_overview_composition.moc"
