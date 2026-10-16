// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "hybridoutputrescue.h"

#include <QRectF>
#include <QVector>

namespace QindaQt::Compositor::KWinIntegration {

// The outputs KWin currently manages, as ADR-0384 rescue targets: each
// output's logical geometry and its maximize work area on the current
// desktop. Call on the compositor thread only.
[[nodiscard]] QVector<RescueOutput> currentRescueOutputs();
// A saved frame about to be re-applied, unchanged while it overlaps a current
// output and otherwise moved onto the nearest one (rescuedFrame()).
[[nodiscard]] QRectF onLiveOutput(const QRectF &savedFrame);

} // namespace QindaQt::Compositor::KWinIntegration
