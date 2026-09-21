// SPDX-License-Identifier: GPL-3.0-or-later

// The controller holds the overview's only mutable state and guards its only
// authority decision. Pure Qt Core: no display, no compositor, no window facts
// producer - every fact is a synthetic task-list projection.

#include "qindaqt/shell/gather_overview/gather_overview_controller.h"

#include <QSignalSpy>
#include <QtTest>

#include <memory>

using namespace QindaQt::ShellGatherOverview;
using QindaQt::ShellTaskList::TaskEntryKind;
using QindaQt::ShellTaskListApplet::TaskListAppletPhase;
using QindaQt::ShellTaskListApplet::TaskListAppletProjection;
using QindaQt::ShellTaskListApplet::TaskListAppletRow;

namespace {

[[nodiscard]] TaskListAppletRow window(const QString &id, bool iconified = false)
{
    TaskListAppletRow row;
    row.taskId = id;
    row.kind = TaskEntryKind::Window;
    row.title = id + QStringLiteral(" title");
    row.applicationId = id + QStringLiteral(".desktop");
    row.applicationName = id.toUpper();
    row.iconText = id.left(1).toUpper();
    row.iconified = iconified;
    row.generationRevision = 31;
    return row;
}

[[nodiscard]] TaskListAppletRow container(const QString &id)
{
    TaskListAppletRow row;
    row.taskId = id;
    row.kind = TaskEntryKind::Container;
    row.title = id + QStringLiteral(" container");
    row.iconText = QStringLiteral("C");
    row.memberWindowIds = {QStringLiteral("m1"), QStringLiteral("m2")};
    row.windowCount = 2;
    row.generationRevision = 31;
    return row;
}

[[nodiscard]] TaskListAppletProjection
readySource(const QVector<TaskListAppletRow> &rows)
{
    TaskListAppletProjection source;
    source.phase = TaskListAppletPhase::Ready;
    source.rows = rows;
    source.totalCount = int(rows.size());
    return source;
}

[[nodiscard]] QVariantList itemsOf(const GatherOverviewController &controller)
{
    return controller.projection().value(QStringLiteral("items")).toList();
}

[[nodiscard]] QVariantMap itemWithId(const GatherOverviewController &controller,
                                     const QString &taskId)
{
    for (const QVariant &entry : itemsOf(controller)) {
        const QVariantMap map = entry.toMap();
        if (map.value(QStringLiteral("taskId")).toString() == taskId)
            return map;
    }
    return {};
}

constexpr QRectF kWorkArea(0, 0, 2560, 1440);

} // namespace

class GatherOverviewControllerTests final : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();

    void aClosedOverviewProjectsNothing();
    void openingProjectsAndAnnounces();
    void openingAlwaysStartsAtTheTop();
    void closingClearsTheProjectionAndSaysItDismissed();
    void toggleFollowsTheOpenState();
    void laneNamesAreStringsTheSurfaceUnderstands();
    void iconNamesComeFromTheInjectedResolver();
    void aContainerWearsTheSymbolicContainerGlyph();
    void aNullResolverLeavesTheSurfaceItsBadge();
    void scrollingDelegatesClampingToThePlanner();
    void repeatedScrollsAtTheEndDoNotAccumulate();
    void activationReportsTheProjectionsOwnGeneration();
    void activationIsRefusedForAnItemNotOnScreen();
    void activationIsRefusedWhileFenced();
    void activationIsRefusedWhileClosed();
    void factsArrivingWhileClosedCostNoProjection();

private:
    std::unique_ptr<GatherOverviewController> make(
        GatherOverviewController::IconNameResolver resolver = {});
};

void GatherOverviewControllerTests::init() {}

std::unique_ptr<GatherOverviewController> GatherOverviewControllerTests::make(
    GatherOverviewController::IconNameResolver resolver)
{
    if (!resolver) {
        resolver = [](const QString &applicationId) {
            return applicationId.isEmpty()
                       ? QString()
                       : QStringLiteral("icon-for-") + applicationId;
        };
    }
    auto controller = std::make_unique<GatherOverviewController>(resolver);
    controller->setWorkArea(kWorkArea);
    return controller;
}

void GatherOverviewControllerTests::aClosedOverviewProjectsNothing()
{
    auto controller = make();
    controller->setSource(readySource({window(QStringLiteral("w1"))}));

    QVERIFY(!controller->isOpen());
    const QVariantMap projection = controller->projection();
    QVERIFY(!projection.value(QStringLiteral("available")).toBool());
    QVERIFY(projection.value(QStringLiteral("items")).toList().isEmpty());
}

void GatherOverviewControllerTests::openingProjectsAndAnnounces()
{
    auto controller = make();
    controller->setSource(readySource(
        {window(QStringLiteral("w1")), window(QStringLiteral("i1"), true),
         container(QStringLiteral("c1"))}));

    QSignalSpy projectionChanged(controller.get(),
                                 &GatherOverviewController::projectionChanged);
    QSignalSpy openChanged(controller.get(),
                           &GatherOverviewController::openChanged);
    controller->open();

    QVERIFY(controller->isOpen());
    QCOMPARE(openChanged.count(), 1);
    QVERIFY(projectionChanged.count() >= 1);
    QVERIFY(controller->projection().value(QStringLiteral("available")).toBool());
    QCOMPARE(itemsOf(*controller).size(), 3);

    // Opening again is not a second open.
    controller->open();
    QCOMPARE(openChanged.count(), 1);
}

void GatherOverviewControllerTests::openingAlwaysStartsAtTheTop()
{
    QVector<TaskListAppletRow> rows;
    for (int i = 0; i < 60; ++i)
        rows.append(window(QStringLiteral("w%1").arg(i, 2, 10, QChar('0'))));

    auto controller = make();
    controller->setSource(readySource(rows));
    controller->open();
    const qreal maximum = controller->projection()
                              .value(QStringLiteral("maximumScrollOffset"))
                              .toDouble();
    QVERIFY(maximum > 0);

    controller->scrollBy(maximum);
    QCOMPARE(controller->projection()
                 .value(QStringLiteral("appliedScrollOffset"))
                 .toDouble(),
             maximum);

    // Close and open again: a remembered offset from last time is never what
    // the user wants from a fresh overview.
    controller->close();
    controller->open();
    QCOMPARE(controller->projection()
                 .value(QStringLiteral("appliedScrollOffset"))
                 .toDouble(),
             0.0);
}

void GatherOverviewControllerTests::
    closingClearsTheProjectionAndSaysItDismissed()
{
    auto controller = make();
    controller->setSource(readySource({window(QStringLiteral("w1"))}));
    controller->open();

    QSignalSpy dismissed(controller.get(), &GatherOverviewController::dismissed);
    QSignalSpy openChanged(controller.get(),
                           &GatherOverviewController::openChanged);
    controller->close();

    QVERIFY(!controller->isOpen());
    QCOMPARE(dismissed.count(), 1);
    QCOMPARE(openChanged.count(), 1);
    QVERIFY(!controller->projection().value(QStringLiteral("available")).toBool());
    QVERIFY(itemsOf(*controller).isEmpty());

    // Closing again is not a second dismissal.
    controller->close();
    QCOMPARE(dismissed.count(), 1);
}

void GatherOverviewControllerTests::toggleFollowsTheOpenState()
{
    auto controller = make();
    controller->setSource(readySource({window(QStringLiteral("w1"))}));

    controller->toggle();
    QVERIFY(controller->isOpen());
    controller->toggle();
    QVERIFY(!controller->isOpen());
}

void GatherOverviewControllerTests::laneNamesAreStringsTheSurfaceUnderstands()
{
    auto controller = make();
    controller->setSource(readySource(
        {window(QStringLiteral("i1"), true), container(QStringLiteral("c1")),
         window(QStringLiteral("w1"))}));
    controller->open();

    QCOMPARE(itemWithId(*controller, QStringLiteral("i1"))
                 .value(QStringLiteral("lane"))
                 .toString(),
             QStringLiteral("icon"));
    QCOMPARE(itemWithId(*controller, QStringLiteral("c1"))
                 .value(QStringLiteral("lane"))
                 .toString(),
             QStringLiteral("card"));
    QCOMPARE(itemWithId(*controller, QStringLiteral("w1"))
                 .value(QStringLiteral("lane"))
                 .toString(),
             QStringLiteral("window"));
}

void GatherOverviewControllerTests::iconNamesComeFromTheInjectedResolver()
{
    auto controller = make();
    controller->setSource(readySource({window(QStringLiteral("w1"))}));
    controller->open();

    QCOMPARE(itemWithId(*controller, QStringLiteral("w1"))
                 .value(QStringLiteral("iconName"))
                 .toString(),
             QStringLiteral("icon-for-w1.desktop"));
}

void GatherOverviewControllerTests::aContainerWearsTheSymbolicContainerGlyph()
{
    // A container is not an application, so it must not be handed to the
    // application icon resolver - the task-list applet makes the same split.
    bool resolverSawContainer = false;
    auto controller = make([&resolverSawContainer](const QString &applicationId) {
        if (applicationId.isEmpty())
            resolverSawContainer = true;
        return QStringLiteral("should-not-be-used");
    });
    controller->setSource(readySource({container(QStringLiteral("c1"))}));
    controller->open();

    const QString iconName = itemWithId(*controller, QStringLiteral("c1"))
                                 .value(QStringLiteral("iconName"))
                                 .toString();
    QCOMPARE(iconName, QStringLiteral("window-duplicate-symbolic"));
    QVERIFY(!resolverSawContainer);
}

void GatherOverviewControllerTests::aNullResolverLeavesTheSurfaceItsBadge()
{
    auto controller = std::make_unique<GatherOverviewController>(
        GatherOverviewController::IconNameResolver{});
    controller->setWorkArea(kWorkArea);
    controller->setSource(readySource({window(QStringLiteral("w1"))}));
    controller->open();

    const QVariantMap item = itemWithId(*controller, QStringLiteral("w1"));
    QVERIFY(item.value(QStringLiteral("iconName")).toString().isEmpty());
    // The one-letter badge is still there, so the tile is not blank.
    QCOMPARE(item.value(QStringLiteral("iconText")).toString(),
             QStringLiteral("W"));
}

void GatherOverviewControllerTests::scrollingDelegatesClampingToThePlanner()
{
    QVector<TaskListAppletRow> rows;
    for (int i = 0; i < 60; ++i)
        rows.append(window(QStringLiteral("w%1").arg(i, 2, 10, QChar('0'))));

    auto controller = make();
    controller->setSource(readySource(rows));
    controller->open();
    const qreal maximum = controller->projection()
                              .value(QStringLiteral("maximumScrollOffset"))
                              .toDouble();
    QVERIFY(maximum > 0);

    controller->scrollBy(-10'000);
    QCOMPARE(controller->projection()
                 .value(QStringLiteral("appliedScrollOffset"))
                 .toDouble(),
             0.0);

    controller->scrollBy(10'000'000);
    QCOMPARE(controller->projection()
                 .value(QStringLiteral("appliedScrollOffset"))
                 .toDouble(),
             maximum);
}

void GatherOverviewControllerTests::repeatedScrollsAtTheEndDoNotAccumulate()
{
    // The bug this guards: adding each delta to the LAST REQUESTED offset
    // instead of the applied one lets a run of wheel notches at the bottom
    // build an offset far past the end, and the user then has to scroll all of
    // it back before the view moves at all.
    QVector<TaskListAppletRow> rows;
    for (int i = 0; i < 60; ++i)
        rows.append(window(QStringLiteral("w%1").arg(i, 2, 10, QChar('0'))));

    auto controller = make();
    controller->setSource(readySource(rows));
    controller->open();
    const qreal maximum = controller->projection()
                              .value(QStringLiteral("maximumScrollOffset"))
                              .toDouble();

    for (int notch = 0; notch < 40; ++notch)
        controller->scrollBy(176);
    QCOMPARE(controller->projection()
                 .value(QStringLiteral("appliedScrollOffset"))
                 .toDouble(),
             maximum);

    // One notch back must move immediately.
    controller->scrollBy(-176);
    const qreal back = controller->projection()
                           .value(QStringLiteral("appliedScrollOffset"))
                           .toDouble();
    QVERIFY2(back < maximum,
             "a single notch back after hitting the end did not move the grid");
    QCOMPARE(back, maximum - 176);
}

void GatherOverviewControllerTests::
    activationReportsTheProjectionsOwnGeneration()
{
    auto controller = make();
    controller->setSource(readySource({window(QStringLiteral("w1"))}));
    controller->open();

    QSignalSpy requested(controller.get(),
                         &GatherOverviewController::activationRequested);
    QVariantMap item = itemWithId(*controller, QStringLiteral("w1"));
    // A caller-supplied revision must be ignored: echoing it would defeat the
    // stale-revision arbitration it exists for.
    item.insert(QStringLiteral("generationRevision"), 999999);
    controller->activate(item);

    QCOMPARE(requested.count(), 1);
    QCOMPARE(requested.first().at(0).toString(), QStringLiteral("w1"));
    QCOMPARE(requested.first().at(2).toULongLong(), 31ull);
}

void GatherOverviewControllerTests::activationIsRefusedForAnItemNotOnScreen()
{
    auto controller = make();
    controller->setSource(readySource({window(QStringLiteral("w1"))}));
    controller->open();

    QSignalSpy requested(controller.get(),
                         &GatherOverviewController::activationRequested);

    // Never projected at all.
    QVariantMap invented;
    invented.insert(QStringLiteral("taskId"), QStringLiteral("ghost"));
    controller->activate(invented);
    QCOMPARE(requested.count(), 0);

    // Projected once, then gone - a stale object held across a re-projection.
    const QVariantMap stale = itemWithId(*controller, QStringLiteral("w1"));
    QVERIFY(!stale.isEmpty());
    controller->setSource(readySource({window(QStringLiteral("w2"))}));
    controller->activate(stale);
    QCOMPARE(requested.count(), 0);

    // And an item with no identity at all.
    controller->activate({});
    QCOMPARE(requested.count(), 0);
}

void GatherOverviewControllerTests::activationIsRefusedWhileFenced()
{
    auto controller = make();
    TaskListAppletProjection degraded =
        readySource({window(QStringLiteral("w1"))});
    degraded.phase = TaskListAppletPhase::Degraded;
    degraded.phaseReason = QStringLiteral("owner-retired");
    controller->setSource(degraded);
    controller->open();

    // The retained generation is still drawn.
    QVERIFY(controller->projection().value(QStringLiteral("available")).toBool());
    QVERIFY(!controller->projection().value(QStringLiteral("interactive")).toBool());
    QCOMPARE(itemsOf(*controller).size(), 1);

    QSignalSpy requested(controller.get(),
                         &GatherOverviewController::activationRequested);
    controller->activate(itemWithId(*controller, QStringLiteral("w1")));
    QCOMPARE(requested.count(), 0);
}

void GatherOverviewControllerTests::activationIsRefusedWhileClosed()
{
    auto controller = make();
    controller->setSource(readySource({window(QStringLiteral("w1"))}));
    controller->open();
    const QVariantMap item = itemWithId(*controller, QStringLiteral("w1"));
    controller->close();

    QSignalSpy requested(controller.get(),
                         &GatherOverviewController::activationRequested);
    controller->activate(item);
    QCOMPARE(requested.count(), 0);
}

void GatherOverviewControllerTests::factsArrivingWhileClosedCostNoProjection()
{
    auto controller = make();
    QSignalSpy projectionChanged(controller.get(),
                                 &GatherOverviewController::projectionChanged);

    for (int churn = 0; churn < 25; ++churn) {
        controller->setSource(
            readySource({window(QStringLiteral("w%1").arg(churn))}));
    }
    QCOMPARE(projectionChanged.count(), 0);

    // And the overview opens on the latest facts, not a stale snapshot.
    controller->open();
    QCOMPARE(itemsOf(*controller).size(), 1);
    QVERIFY(!itemWithId(*controller, QStringLiteral("w24")).isEmpty());
}

QTEST_GUILESS_MAIN(GatherOverviewControllerTests)

#include "tst_gather_overview_controller.moc"
