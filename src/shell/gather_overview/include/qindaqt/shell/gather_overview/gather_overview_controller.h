// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/shell/gather_overview/gather_overview_projection.h"

#include <QObject>
#include <QRectF>
#include <QString>
#include <QVariantMap>

#include <functional>

namespace QindaQt::ShellGatherOverview {

// AGENT-CONTRACT: holds the gather overview's only mutable state - is it open,
// and how far is the grid scrolled - and nothing else. Qt Core plus moc, no
// Qml, no Quick, no display: the surface is presentation and this is the thing
// that decides what it presents.
//
// It owns four policies that have to live somewhere and do not belong in a
// pure function or in QML:
//
//  1. A closed overview projects nothing. The surface is only fed while open,
//     so a session's window churn costs no arrangement work.
//  2. Opening starts at the top. A remembered scroll offset from minutes ago
//     is never what the user wants from a fresh overview.
//  3. The planner is the only thing that clamps a scroll position, so
//     `scrollBy` adds the delta and re-projects rather than clamping here.
//     Two clamps would eventually disagree.
//  4. An activation is refused unless the item is actually in the current
//     projection AND the projection is interactive. The surface already makes
//     fenced tiles inert, but authority is not a presentation concern: a QML
//     bug, or a stale item object held across a re-projection, must not be
//     able to act.
//
// The controller never acts on a window itself. It reports
// `activationRequested` with the identity and the generation the item was
// projected from, and the owner turns that into a task intent with its own
// authority - the same split the task-list applet uses.
class GatherOverviewController final : public QObject
{
    Q_OBJECT
    // The value object the surface draws. Empty and unavailable while closed.
    Q_PROPERTY(QVariantMap projection READ projection NOTIFY projectionChanged)
    Q_PROPERTY(bool open READ isOpen NOTIFY openChanged)

public:
    // Resolves a theme icon name from an application id. Injected because
    // resolving one is the shell's business, not this module's; the task-list
    // applet's controller takes the same seam. A null resolver yields no icon
    // names, and the surface falls back to its one-letter badge.
    using IconNameResolver = std::function<QString(const QString &applicationId)>;

    explicit GatherOverviewController(IconNameResolver iconNameResolver,
                                      QObject *parent = nullptr);
    ~GatherOverviewController() override;

    // Live facts, from whatever already observes them. Cheap while closed.
    void setSource(const ShellTaskListApplet::TaskListAppletProjection &source);
    // The output's work area, in desktop-logical coordinates.
    void setWorkArea(const QRectF &workArea);

    [[nodiscard]] QVariantMap projection() const;
    [[nodiscard]] bool isOpen() const noexcept;

    // The geometry knobs the planner takes, for a host that needs to override
    // them. Defaults are the planner's own, including the 90 px buffer.
    void setGeometry(const GatherOverviewRequest &knobs);

public Q_SLOTS:
    void open();
    void close();
    void toggle();
    // Wheel delta in pixels, positive scrolling further down the grid.
    void scrollBy(qreal delta);
    // The item the surface reported. Refused unless it is in the current
    // projection and that projection is interactive.
    void activate(const QVariantMap &item);

Q_SIGNALS:
    void projectionChanged();
    void openChanged();
    void activationRequested(const QString &taskId, const QString &windowId,
                             quint64 generationRevision);
    void dismissed();

private:
    void reproject();

    IconNameResolver m_iconNameResolver;
    ShellTaskListApplet::TaskListAppletProjection m_source;
    // Geometry and scroll position only. Its own `source` field is always
    // empty and is overwritten from `m_source` at every projection, so the
    // facts are stored once rather than in two places that can disagree.
    GatherOverviewRequest m_knobs;
    GatherOverviewProjection m_projection;
    QVariantMap m_projectionMap;
    bool m_open = false;
};

// The QVariantMap shape the surface consumes. Exposed for tests and for a host
// that wants to feed the surface without a controller. `lane` is a string -
// "icon", "card" or "window" - deliberately, so QML never depends on the C++
// enum's ordinals.
[[nodiscard]] QVariantMap
gatherOverviewProjectionMap(const GatherOverviewProjection &projection,
                            const GatherOverviewController::IconNameResolver
                                &iconNameResolver = {});

} // namespace QindaQt::ShellGatherOverview
