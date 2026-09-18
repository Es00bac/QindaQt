// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/tablet_devices/tablet_device_port.h>

#include <QList>
#include <QPointF>
#include <QString>

namespace QindaQt::Services::TabletDevices {

// Pure geometry for the Pen & tablet route. Nothing here talks to KWin; the
// route computes a value and the port writes it, so every rule below is
// testable without a device.

// KWin's identity calibration: a 4x4 row-major matrix as sixteen
// comma-separated numbers, exactly the form
// org.kde.KWin.InputDevice.calibrationMatrix reads and writes.
[[nodiscard]] QString identityCalibrationMatrix();

// The calibration matrix that carries `measured` points onto `targets`.
//
// AGENT-CONTRACT: Both lists are normalized 0..1 points in the same frame
// and must have the same size, at least three entries. `measured` is where
// the pen actually landed with the calibration reset to default; `targets`
// is where the crosshair was drawn. The result is the least-squares affine
// fit serialized in KWin's 16-value row-major form.
//
// AGENT-GUARD: Returns an empty string for degenerate input (collinear or
// coincident measurements). The wizard must ask again rather than write a
// matrix that would make the pen unusable, and an empty string is never a
// legal value to send.
[[nodiscard]] QString
calibrationMatrixFor(const QList<QPointF> &measured,
                     const QList<QPointF> &targets);

// The largest centered rectangle of an output with the tablet's own
// proportions — the "keep the tablet's shape, letterbox the rest" mapping.
// `tabletAspect` is width/height of the tablet's active area; the output
// extents are in the same units as each other. Returns the whole surface
// when either input is not positive, because a computed ratio would then be
// a guess.
[[nodiscard]] TabletArea letterboxArea(double tabletAspect, double outputWidth,
                                       double outputHeight);

// True when `curve` is a pressure curve KWin can read: at least two
// "x,y;" points, every coordinate inside 0..1, and x strictly increasing.
// KWin silently keeps its old curve for anything else, which would present
// an unobserved change as applied.
[[nodiscard]] bool isValidPressureCurve(const QString &curve);

// The two-point curve for a tip threshold: pressure below `threshold`
// registers as nothing, and the rest of the range is spread over what is
// left. `threshold` outside 0..0.9 is clamped.
[[nodiscard]] QString pressureCurveForThreshold(double threshold);

} // namespace QindaQt::Services::TabletDevices
