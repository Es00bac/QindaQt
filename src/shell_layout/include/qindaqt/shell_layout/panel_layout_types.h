// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/profiles/profile_types.h"

#include <QRect>
#include <QString>
#include <QVector>
#include <QtTypes>

namespace QindaQt::ShellLayout {

// A compositor-provided output description. Geometry is already expressed in
// the shared logical desktop coordinate space; scale is metadata used only to
// reject malformed inventories.
struct LogicalOutput {
    QString id;
    QRect geometry;
    qreal scale = 1.0;
};

struct PanelSurface {
    // The pair (panelId, outputId) is the stable expanded-surface identity.
    // Wildcard profile panels intentionally retain the same panelId on each
    // output instead of manufacturing a persistence identifier.
    QString panelId;
    QString outputId;
    QRect geometry;
    Profiles::Edge edge = Profiles::Edge::Top;
    Profiles::Layer layer = Profiles::Layer::Above;
    qsizetype stackIndex = 0;
    bool reservesWorkArea = true;
};

struct OutputLayout {
    QString outputId;
    QRect geometry;
    QRect workArea;
    qreal scale = 1.0;
};

enum class PanelLayoutErrorCode {
    None,
    EmptyOutputInventory,
    InvalidOutputId,
    DuplicateOutputId,
    InvalidOutputGeometry,
    InvalidOutputScale,
    InvalidPanel,
    MissingOutput,
    DuplicatePanelInstance,
    OverConstrainedOutput,
    ArithmeticOverflow,
};

struct PanelLayoutError {
    PanelLayoutErrorCode code = PanelLayoutErrorCode::None;
    QString panelId;
    QString outputId;
    QString message;
};

struct PanelLayoutResult {
    QVector<PanelSurface> surfaces;
    QVector<OutputLayout> outputs;
    PanelLayoutError error;

    [[nodiscard]] bool ok() const noexcept
    {
        return error.code == PanelLayoutErrorCode::None;
    }
};

} // namespace QindaQt::ShellLayout
