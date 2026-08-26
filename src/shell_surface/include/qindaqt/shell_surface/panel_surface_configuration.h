// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/profiles/profile_types.h"

#include <QFlags>
#include <QMargins>
#include <QRect>
#include <QSize>
#include <QString>
#include <QVector>
#include <QtTypes>

namespace QindaQt::ShellSurface {

struct PanelSurfaceIdentity {
    QString panelId;
    QString outputId;

    friend bool operator==(const PanelSurfaceIdentity &, const PanelSurfaceIdentity &) = default;
};

enum class SurfaceAnchor : quint8 {
    Top = 0x1,
    Bottom = 0x2,
    Left = 0x4,
    Right = 0x8,
};
Q_DECLARE_FLAGS(SurfaceAnchors, SurfaceAnchor)

struct PanelSurfaceConfiguration {
    PanelSurfaceIdentity identity;
    QRect geometry;
    QSize desiredSize;
    QMargins margins;
    SurfaceAnchors anchors;
    Profiles::Edge edge = Profiles::Edge::Top;
    Profiles::Layer layer = Profiles::Layer::Above;
    Profiles::Edge exclusiveEdge = Profiles::Edge::Top;
    int exclusiveZone = -1;
    qsizetype placementOrder = 0;
    bool reservationCarrier = false;

    friend bool operator==(const PanelSurfaceConfiguration &,
                           const PanelSurfaceConfiguration &) = default;
};

enum class PanelSurfacePlanErrorCode {
    None,
    RejectedLayout,
    InvalidOutput,
    DuplicateOutput,
    MissingOutput,
    DuplicateSurface,
    InvalidSurface,
    ReservationMismatch,
    ArithmeticOverflow,
};

struct PanelSurfacePlanError {
    PanelSurfacePlanErrorCode code = PanelSurfacePlanErrorCode::None;
    PanelSurfaceIdentity identity;
    QString message;

    friend bool operator==(const PanelSurfacePlanError &, const PanelSurfacePlanError &) = default;
};

struct PanelSurfacePlan {
    QVector<PanelSurfaceConfiguration> surfaces;
    PanelSurfacePlanError error;

    [[nodiscard]] bool ok() const noexcept
    {
        return error.code == PanelSurfacePlanErrorCode::None;
    }

    friend bool operator==(const PanelSurfacePlan &, const PanelSurfacePlan &) = default;
};

} // namespace QindaQt::ShellSurface

Q_DECLARE_OPERATORS_FOR_FLAGS(QindaQt::ShellSurface::SurfaceAnchors)
