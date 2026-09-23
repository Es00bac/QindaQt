// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/shell/task_list/applet/task_list_applet_types.h"

#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QVector>

namespace QindaQt::ShellGatherOverview {

// AGENT-CONTRACT: the gather overview's presentation model. Pure: it takes the
// task-list applet projection the shell already produces plus one output's
// work area, and returns drawable items with frames. No KWin type, no global
// state, no filesystem, nothing to mock.
//
// It is the bridge between two things that already exist:
// QindaQt::ShellTaskListApplet::TaskListAppletProjection (what the session
// holds) and QindaQt::HybridGather::planGather (where it goes). This header
// owns the *classification* policy - which lane each entry belongs in - and
// nothing else; the arrangement stays in the planner.
//
// The overview is transient and moves nothing. That settles the one rule that
// reads like a state change: "every container is rolled up in gather" is a
// PRESENTATION rule, not an operation. A container is drawn as a card whether
// or not it is really rolled up, and no roll-up intent is ever submitted. It
// also settles why a container member is never a window tile: in the grouped
// rows a container is one row, so its members are not rows at all.

enum class GatherLane {
    // A round chip per iconified window (ADR-0203), in iconify order.
    Icon,
    // A strip per container (ADR-0139), whether or not it is rolled up.
    Card,
    // Everything else, in the scrolling grid.
    Window,
};

// One drawable item. Carries the identity an activation needs and the text an
// icon-less presentation needs, so the surface never reaches past this struct.
struct GatherOverviewItem final {
    QString taskId;
    // Set only for an ungrouped row that names one container member; empty on
    // every grouped row. Echoed back on activation exactly as received.
    QString windowId;
    GatherLane lane = GatherLane::Window;
    QRectF frame;
    QString title;
    QString applicationId;
    QString applicationName;
    // Deterministic one-letter badge from the source row: the fallback when a
    // window preview is denied, unavailable or still loading (ADR-0241).
    QString iconText;
    // Exact "#RRGGBB" container colour, empty when the user set none.
    QString colorHex;
    quint32 windowCount = 1;
    bool active = false;
    bool urgent = false;
    // Echoed on every intent so stale-generation arbitration can refuse an
    // action against a generation the user no longer sees.
    quint64 generationRevision = 0;
    QString accessibleName;

    friend bool operator==(const GatherOverviewItem &,
                           const GatherOverviewItem &) = default;
};

struct GatherOverviewRequest final
{
    ShellTaskListApplet::TaskListAppletProjection source;
    // The output's work area in desktop-logical coordinates.
    QRectF workArea;
    // Wheel position in the window grid; the planner clamps it.
    qreal scrollOffset = 0;

    // Geometry knobs, forwarded to the planner. The defaults are the planner's
    // own, including the 90 px buffer the arrangement was specified with.
    qreal margin = 90;
    qreal gap = 16;
    qreal iconExtent = 48;
    QSizeF cardSize = QSizeF(240, 64);
    QSizeF cellSize = QSizeF(260, 176);
};

struct GatherOverviewProjection final
{
    // False when there is nothing honest to draw: the source phase withholds
    // observation, or the planner refused the geometry. `diagnostic` says
    // which, and `items` is then empty - never partially populated.
    bool available = false;
    QString diagnostic;

    // False when the source is drawable but refuses every intent - a Degraded
    // phase keeps the retained generation visible while activation is fenced.
    // The surface must present items as inert rather than let a click look
    // like it worked; `available` alone would hide that distinction.
    bool interactive = false;

    // True when the source is presentable but holds no entry at all, so the
    // surface shows an empty state rather than a bare field.
    bool empty = false;

    QRectF field;
    QRectF gridViewport;
    int gridColumns = 0;
    int gridRows = 0;
    qreal gridContentHeight = 0;
    qreal maximumScrollOffset = 0;
    qreal appliedScrollOffset = 0;

    // Every visible item, icon lane then card lane then window grid, each lane
    // in its source order. An item the planner marked invisible - scrolled out
    // of the viewport, or in a lane with no room - is NOT here, so the surface
    // can draw the vector as given.
    QVector<GatherOverviewItem> items;

    // Counts of what the session holds, before visibility. `windowsHidden` is
    // how many window tiles are scrolled out of view, which is the only
    // honest way for the surface to say "there is more below".
    int iconCount = 0;
    int cardCount = 0;
    int windowCount = 0;
    int windowsHidden = 0;
    // Entries the task-list presentation cap already dropped upstream. Carried
    // through rather than hidden: the overview cannot show what it never got.
    int sourceOverflowCount = 0;

    friend bool operator==(const GatherOverviewProjection &,
                           const GatherOverviewProjection &) = default;
};

// Deterministic: the same request always yields the same projection.
[[nodiscard]] GatherOverviewProjection
projectGatherOverview(const GatherOverviewRequest &request);

} // namespace QindaQt::ShellGatherOverview
