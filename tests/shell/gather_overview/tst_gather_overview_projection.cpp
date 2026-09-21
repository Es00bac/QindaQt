// SPDX-License-Identifier: GPL-3.0-or-later

// The gather overview's classification policy. Every input is a synthetic
// task-list projection: no KWin, no display, no window facts producer.

#include "qindaqt/shell/gather_overview/gather_overview_projection.h"

#include <QtTest>

using namespace QindaQt::ShellGatherOverview;
using QindaQt::ShellTaskList::TaskEntryKind;
using QindaQt::ShellTaskListApplet::TaskListAppletPhase;
using QindaQt::ShellTaskListApplet::TaskListAppletProjection;
using QindaQt::ShellTaskListApplet::TaskListAppletRow;

namespace {

[[nodiscard]] TaskListAppletRow window(const QString &id, bool iconified = false,
                                       bool minimized = false)
{
    TaskListAppletRow row;
    row.taskId = id;
    row.kind = TaskEntryKind::Window;
    row.title = id + QStringLiteral(" title");
    row.applicationId = id + QStringLiteral(".desktop");
    row.applicationName = id.toUpper();
    row.iconText = id.left(1).toUpper();
    row.iconified = iconified;
    row.minimized = minimized;
    row.generationRevision = 7;
    row.accessibleName = id + QStringLiteral(" accessible");
    return row;
}

[[nodiscard]] TaskListAppletRow container(const QString &id,
                                          const QStringList &members,
                                          bool iconified = false)
{
    TaskListAppletRow row;
    row.taskId = id;
    row.kind = TaskEntryKind::Container;
    row.title = id + QStringLiteral(" container");
    row.iconText = QStringLiteral("C");
    row.colorHex = QStringLiteral("#3366cc");
    row.memberWindowIds = members;
    row.windowCount = quint32(members.size());
    row.iconified = iconified;
    row.generationRevision = 7;
    return row;
}

[[nodiscard]] GatherOverviewRequest
requestFor(const QVector<TaskListAppletRow> &rows,
           TaskListAppletPhase phase = TaskListAppletPhase::Ready,
           QRectF area = QRectF(0, 0, 2560, 1440))
{
    GatherOverviewRequest request;
    request.source.phase = phase;
    request.source.rows = rows;
    request.source.totalCount = int(rows.size());
    request.workArea = area;
    return request;
}

[[nodiscard]] QVector<GatherOverviewItem>
lane(const GatherOverviewProjection &projection, GatherLane which)
{
    QVector<GatherOverviewItem> out;
    for (const GatherOverviewItem &item : projection.items) {
        if (item.lane == which)
            out.append(item);
    }
    return out;
}

[[nodiscard]] QStringList idsIn(const GatherOverviewProjection &projection,
                                GatherLane which)
{
    QStringList out;
    for (const GatherOverviewItem &item : lane(projection, which))
        out.append(item.taskId);
    return out;
}

} // namespace

class GatherOverviewProjectionTests final : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void eachKindLandsInItsOwnLane();
    void aContainerIsACardEvenWhenItIsNotRolledUp();
    void aContainerMemberIsNeverAWindowTile();
    void minimizedIsNotIconified();
    void lanesKeepSourceOrderAndComeIconsCardsWindows();
    void everyItemCarriesTheIdentityAnIntentNeeds();
    void framesComeFromThePlannerAndSitInsideTheField();
    void readyAndEmptyAreInteractive();
    void degradedDrawsButRefusesIntents();
    void loadingAndUnavailableDrawNothing();
    void anUnusableWorkAreaIsRefusedNotArranged();
    void aFieldTooNarrowForOneCellHidesEveryWindow();
    void scrollingHidesTilesAndSaysHowMany();
    void scrollOffsetIsClampedByThePlanner();
    void anEmptySourceIsAvailableAndEmpty();
    void aRowWithoutIdentityIsDropped();
    void upstreamOverflowIsCarriedThrough();
    void theSameRequestAlwaysYieldsTheSameProjection();
};

void GatherOverviewProjectionTests::eachKindLandsInItsOwnLane()
{
    const auto projection = projectGatherOverview(requestFor(
        {window(QStringLiteral("free")), window(QStringLiteral("hidden"), true),
         container(QStringLiteral("box"), {QStringLiteral("m1")})}));

    QVERIFY(projection.available);
    QCOMPARE(idsIn(projection, GatherLane::Icon),
             QStringList{QStringLiteral("hidden")});
    QCOMPARE(idsIn(projection, GatherLane::Card),
             QStringList{QStringLiteral("box")});
    QCOMPARE(idsIn(projection, GatherLane::Window),
             QStringList{QStringLiteral("free")});
    QCOMPARE(projection.iconCount, 1);
    QCOMPARE(projection.cardCount, 1);
    QCOMPARE(projection.windowCount, 1);
}

void GatherOverviewProjectionTests::aContainerIsACardEvenWhenItIsNotRolledUp()
{
    // Gather presents every container rolled up. That is a presentation rule,
    // so a container that is NOT really rolled up is still a card - and one
    // that is iconified does not fall through to the icon lane.
    const auto open = projectGatherOverview(requestFor(
        {container(QStringLiteral("open"), {QStringLiteral("m1")}, false)}));
    QCOMPARE(idsIn(open, GatherLane::Card),
             QStringList{QStringLiteral("open")});
    QVERIFY(lane(open, GatherLane::Icon).isEmpty());

    const auto rolled = projectGatherOverview(requestFor(
        {container(QStringLiteral("rolled"), {QStringLiteral("m1")}, true)}));
    QCOMPARE(idsIn(rolled, GatherLane::Card),
             QStringList{QStringLiteral("rolled")});
    QVERIFY(lane(rolled, GatherLane::Icon).isEmpty());
}

void GatherOverviewProjectionTests::aContainerMemberIsNeverAWindowTile()
{
    const auto projection = projectGatherOverview(requestFor(
        {container(QStringLiteral("box"),
                   {QStringLiteral("m1"), QStringLiteral("m2")}),
         window(QStringLiteral("free"))}));

    const QStringList windows = idsIn(projection, GatherLane::Window);
    QCOMPARE(windows, QStringList{QStringLiteral("free")});
    QVERIFY(!windows.contains(QStringLiteral("m1")));
    QVERIFY(!windows.contains(QStringLiteral("m2")));
    // The card reports how many windows it stands for.
    QCOMPARE(lane(projection, GatherLane::Card).at(0).windowCount, 2u);
}

void GatherOverviewProjectionTests::minimizedIsNotIconified()
{
    // A merely minimized window belongs in the grid; only ADR-0203 iconified
    // windows get the round chip lane.
    const auto projection = projectGatherOverview(
        requestFor({window(QStringLiteral("min"), false, true)}));
    QCOMPARE(idsIn(projection, GatherLane::Window),
             QStringList{QStringLiteral("min")});
    QVERIFY(lane(projection, GatherLane::Icon).isEmpty());
}

void GatherOverviewProjectionTests::
    lanesKeepSourceOrderAndComeIconsCardsWindows()
{
    const auto projection = projectGatherOverview(requestFor({
        window(QStringLiteral("w1")),
        container(QStringLiteral("c1"), {QStringLiteral("m")}),
        window(QStringLiteral("i1"), true),
        window(QStringLiteral("w2")),
        container(QStringLiteral("c2"), {QStringLiteral("m")}),
        window(QStringLiteral("i2"), true),
    }));

    QStringList order;
    for (const GatherOverviewItem &item : projection.items)
        order.append(item.taskId);
    QCOMPARE(order,
             (QStringList{QStringLiteral("i1"), QStringLiteral("i2"),
                          QStringLiteral("c1"), QStringLiteral("c2"),
                          QStringLiteral("w1"), QStringLiteral("w2")}));
}

void GatherOverviewProjectionTests::everyItemCarriesTheIdentityAnIntentNeeds()
{
    TaskListAppletRow member = window(QStringLiteral("ungrouped"));
    member.windowId = QStringLiteral("window-42");
    const auto projection = projectGatherOverview(requestFor({member}));

    const GatherOverviewItem item = lane(projection, GatherLane::Window).at(0);
    QCOMPARE(item.taskId, QStringLiteral("ungrouped"));
    QCOMPARE(item.windowId, QStringLiteral("window-42"));
    QCOMPARE(item.generationRevision, 7ull);
    QCOMPARE(item.title, QStringLiteral("ungrouped title"));
    QCOMPARE(item.applicationId, QStringLiteral("ungrouped.desktop"));
    QCOMPARE(item.applicationName, QStringLiteral("UNGROUPED"));
    QCOMPARE(item.iconText, QStringLiteral("U"));
    QCOMPARE(item.accessibleName, QStringLiteral("ungrouped accessible"));

    // A container's user colour survives so the card can wear it.
    const auto boxed = projectGatherOverview(requestFor(
        {container(QStringLiteral("box"), {QStringLiteral("m")})}));
    QCOMPARE(lane(boxed, GatherLane::Card).at(0).colorHex,
             QStringLiteral("#3366cc"));
}

void GatherOverviewProjectionTests::framesComeFromThePlannerAndSitInsideTheField()
{
    const auto projection = projectGatherOverview(requestFor({
        window(QStringLiteral("i1"), true),
        container(QStringLiteral("c1"), {QStringLiteral("m")}),
        window(QStringLiteral("w1")),
    }));

    QVERIFY(projection.available);
    QVERIFY(projection.field.isValid());
    // The 90 px default buffer inset the work area on every side.
    QCOMPARE(projection.field, QRectF(90, 90, 2560 - 180, 1440 - 180));

    for (const GatherOverviewItem &item : projection.items) {
        QVERIFY2(item.frame.isValid(),
                 qPrintable(QStringLiteral("empty frame for %1").arg(item.taskId)));
        QVERIFY2(projection.field.contains(item.frame),
                 qPrintable(QStringLiteral("%1 escaped the field").arg(item.taskId)));
    }
    // Window tiles additionally sit inside the scrolling viewport.
    for (const GatherOverviewItem &item : lane(projection, GatherLane::Window))
        QVERIFY(projection.gridViewport.intersects(item.frame));
}

void GatherOverviewProjectionTests::readyAndEmptyAreInteractive()
{
    for (const auto phase :
         {TaskListAppletPhase::Ready, TaskListAppletPhase::Empty}) {
        const auto projection = projectGatherOverview(
            requestFor({window(QStringLiteral("w"))}, phase));
        QVERIFY(projection.available);
        QVERIFY(projection.interactive);
        QVERIFY(projection.diagnostic.isEmpty());
    }
}

void GatherOverviewProjectionTests::degradedDrawsButRefusesIntents()
{
    auto request = requestFor({window(QStringLiteral("w"))},
                              TaskListAppletPhase::Degraded);
    request.source.phaseReason = QStringLiteral("owner-retired");
    const auto projection = projectGatherOverview(request);

    QVERIFY(projection.available);
    QVERIFY(!projection.interactive);
    QCOMPARE(projection.diagnostic, QStringLiteral("owner-retired"));
    // The retained generation is still drawn - that is the point.
    QCOMPARE(projection.items.size(), 1);

    // Without a reason the phase still names itself rather than going silent.
    auto bare = requestFor({window(QStringLiteral("w"))},
                           TaskListAppletPhase::Degraded);
    QCOMPARE(projectGatherOverview(bare).diagnostic,
             QStringLiteral("source-degraded"));
}

void GatherOverviewProjectionTests::loadingAndUnavailableDrawNothing()
{
    for (const auto phase :
         {TaskListAppletPhase::Loading, TaskListAppletPhase::Unavailable}) {
        const auto projection = projectGatherOverview(
            requestFor({window(QStringLiteral("w"))}, phase));
        QVERIFY(!projection.available);
        QVERIFY(!projection.interactive);
        QVERIFY(!projection.diagnostic.isEmpty());
        QVERIFY(projection.items.isEmpty());
        // No partial truth: the counts stay zero too.
        QCOMPARE(projection.windowCount, 0);
        QCOMPARE(projection.iconCount, 0);
        QCOMPARE(projection.cardCount, 0);
    }
}

void GatherOverviewProjectionTests::anUnusableWorkAreaIsRefusedNotArranged()
{
    const auto projection = projectGatherOverview(
        requestFor({window(QStringLiteral("w"))}, TaskListAppletPhase::Ready,
                   QRectF()));
    QVERIFY(!projection.available);
    QVERIFY(!projection.diagnostic.isEmpty());
    QVERIFY(projection.items.isEmpty());
    // The session held a window, but nothing was arranged, so the counts must
    // not claim otherwise.
    QCOMPARE(projection.windowCount, 0);
}

void GatherOverviewProjectionTests::aFieldTooNarrowForOneCellHidesEveryWindow()
{
    // Icon and card lanes plus the margin leave less than one cell of width.
    // The planner's honest answer is zero columns; every window is hidden and
    // the count says how many.
    const auto projection = projectGatherOverview(requestFor(
        {container(QStringLiteral("c1"), {QStringLiteral("m")}),
         window(QStringLiteral("w1")), window(QStringLiteral("w2"))},
        TaskListAppletPhase::Ready, QRectF(0, 0, 640, 900)));

    QVERIFY(projection.available);
    QCOMPARE(projection.gridColumns, 0);
    QVERIFY(lane(projection, GatherLane::Window).isEmpty());
    QCOMPARE(projection.windowCount, 2);
    QCOMPARE(projection.windowsHidden, 2);
    // The card lane still drew, so the overview is not useless.
    QCOMPARE(idsIn(projection, GatherLane::Card),
             QStringList{QStringLiteral("c1")});
}

void GatherOverviewProjectionTests::scrollingHidesTilesAndSaysHowMany()
{
    QVector<TaskListAppletRow> rows;
    for (int i = 0; i < 60; ++i)
        rows.append(window(QStringLiteral("w%1").arg(i, 2, 10, QChar('0'))));

    auto request = requestFor(rows, TaskListAppletPhase::Ready,
                              QRectF(0, 0, 1920, 1080));
    const auto top = projectGatherOverview(request);
    QVERIFY(top.available);
    QCOMPARE(top.windowCount, 60);
    QVERIFY(top.gridColumns > 0);
    // More content than viewport, so something is below the fold.
    QVERIFY(top.maximumScrollOffset > 0);
    QVERIFY(top.windowsHidden > 0);
    const int drawnAtTop = int(lane(top, GatherLane::Window).size());
    QCOMPARE(drawnAtTop + top.windowsHidden, 60);

    // Scrolling to the bottom draws a different set of the same size class.
    request.scrollOffset = top.maximumScrollOffset;
    const auto bottom = projectGatherOverview(request);
    QCOMPARE(bottom.appliedScrollOffset, top.maximumScrollOffset);
    QVERIFY(idsIn(bottom, GatherLane::Window)
            != idsIn(top, GatherLane::Window));
    QCOMPARE(int(lane(bottom, GatherLane::Window).size())
                 + bottom.windowsHidden,
             60);
}

void GatherOverviewProjectionTests::scrollOffsetIsClampedByThePlanner()
{
    QVector<TaskListAppletRow> rows;
    for (int i = 0; i < 40; ++i)
        rows.append(window(QStringLiteral("w%1").arg(i)));

    auto request = requestFor(rows);
    request.scrollOffset = -5000;
    QCOMPARE(projectGatherOverview(request).appliedScrollOffset, 0.0);

    request.scrollOffset = 1e9;
    const auto far = projectGatherOverview(request);
    QCOMPARE(far.appliedScrollOffset, far.maximumScrollOffset);
}

void GatherOverviewProjectionTests::anEmptySourceIsAvailableAndEmpty()
{
    const auto projection =
        projectGatherOverview(requestFor({}, TaskListAppletPhase::Empty));
    QVERIFY(projection.available);
    QVERIFY(projection.interactive);
    QVERIFY(projection.empty);
    QVERIFY(projection.items.isEmpty());
    // The field is still real, so the surface can show an empty state in it.
    QVERIFY(projection.field.isValid());
}

void GatherOverviewProjectionTests::aRowWithoutIdentityIsDropped()
{
    TaskListAppletRow nameless = window(QStringLiteral("w"));
    nameless.taskId.clear();
    const auto projection = projectGatherOverview(
        requestFor({nameless, window(QStringLiteral("real"))}));

    QCOMPARE(projection.windowCount, 1);
    QCOMPARE(idsIn(projection, GatherLane::Window),
             QStringList{QStringLiteral("real")});
}

void GatherOverviewProjectionTests::upstreamOverflowIsCarriedThrough()
{
    // The task list already capped presentation. The overview cannot show what
    // it never received, so it reports the number rather than implying it has
    // everything.
    auto request = requestFor({window(QStringLiteral("w"))});
    request.source.totalCount = 300;
    request.source.overflowCount = 299;
    QCOMPARE(projectGatherOverview(request).sourceOverflowCount, 299);
}

void GatherOverviewProjectionTests::theSameRequestAlwaysYieldsTheSameProjection()
{
    const auto request = requestFor({
        window(QStringLiteral("i"), true),
        container(QStringLiteral("c"), {QStringLiteral("m")}),
        window(QStringLiteral("w")),
    });
    QCOMPARE(projectGatherOverview(request), projectGatherOverview(request));
}

QTEST_GUILESS_MAIN(GatherOverviewProjectionTests)

#include "tst_gather_overview_projection.moc"
