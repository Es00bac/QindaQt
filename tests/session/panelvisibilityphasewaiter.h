// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QJsonArray>
#include <QSize>

#include <functional>

namespace QindaQt::Test::PanelVisibilityPhase {

class SurfaceAuthority {
public:
    virtual ~SurfaceAuthority() = default;
    virtual QJsonArray snapshot() = 0;
};

class PollTimer {
public:
    virtual ~PollTimer() = default;
    virtual bool expired() const = 0;
    virtual void waitForNextPoll() = 0;
};

using PhasePredicate = std::function<bool(const QJsonArray &)>;

bool mappedPanel(const QJsonArray &items, const QString &edge, int thickness,
                 const QSize &output, int zone = -2);
bool snapshotGeometryIsSettled(const QJsonArray &items, const QSize &output);
bool waitForSettledPhase(SurfaceAuthority &authority, PollTimer &timer,
                         const QSize &output, const PhasePredicate &ready,
                         QJsonArray *observed);

} // namespace QindaQt::Test::PanelVisibilityPhase
