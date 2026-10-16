// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QRect>
#include <QRectF>
#include <QVector>

#include <optional>

namespace QindaQt::Compositor::KWinIntegration {

// One remaining output: its logical geometry and its work area (geometry
// minus panel struts). An invalid work area falls back to the geometry.
struct RescueOutput final
{
    QRect geometry;
    QRect workArea;
};

// ADR-0384: where a frame that overlaps none of the current outputs belongs.
//
// After an output is unplugged KWin evacuates ordinary windows, but saved
// frames (a container's committed layout, a maximize restore frame, a rolled
// strip, an independent restore state applied when a container dissolves)
// still name the removed output's coordinates and would put a window back
// there. Every such frame is checked here before it is applied.
//
// Returns nullopt when `frame` overlaps at least one output (it is reachable
// and stays where the user put it, even hanging off an edge). Otherwise it
// returns the frame moved, and shrunk only when it cannot fit, into the work
// area of the output nearest to the frame's centre; ties go to the earlier
// output so the answer is deterministic.
[[nodiscard]] std::optional<QRect> rescuedFrame(const QRect &frame,
                                                const QVector<RescueOutput> &outputs);
[[nodiscard]] std::optional<QRectF> rescuedFrame(const QRectF &frame,
                                                 const QVector<RescueOutput> &outputs);
// True when `frame` overlaps none of `outputs` (and there is at least one).
[[nodiscard]] bool isStranded(const QRectF &frame, const QVector<RescueOutput> &outputs);

} // namespace QindaQt::Compositor::KWinIntegration
