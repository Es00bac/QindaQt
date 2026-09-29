// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/tablet_devices/tablet_device_port.h>

#include <QList>
#include <QPointF>
#include <QString>

#include <optional>

namespace QindaQt::Services::TabletDevices {

// Pure geometry for the Pen & tablet route. Nothing here talks to KWin; the
// route computes a value and the port writes it, so every rule below is
// testable without a device.

// The smallest side the area editor lets a user draw, as a fraction of the
// surface (ADR-0285). Below it a mapping is no longer something a hand can
// use, and a drag that ends there is a slip rather than a choice.
inline constexpr double MinimumEditableAreaExtent = 0.05;

// The rectangle KWin and libinput will both accept, or nothing.
//
// AGENT-GUARD: KWin stores an inputArea without checking it; libinput then
// refuses it (x1 >= x2, or any edge outside 0..1, where x2 is computed as
// x + width) and keeps the OLD area while kcminputrc remembers the new one
// (kwin-6.6.6 device.cpp setInputArea; libinput 1.31 evdev-tablet.c
// tablet_area_set_rectangle). Every area bound for the port passes through
// here: a member that overshoots an edge by less than a rounding error is
// pulled onto it, the far edge is recomputed as 1 - origin (which never
// rounds past 1.0), and anything genuinely outside, non-finite, or with a
// side shorter than `minimumExtent` is refused.
[[nodiscard]] std::optional<TabletArea>
normalizedArea(const TabletArea &area, double minimumExtent = 1e-4);

// True when every member of the two areas agrees within `tolerance`. Used to
// skip writes a floating-point round trip would otherwise repeat forever.
[[nodiscard]] bool sameArea(const TabletArea &first, const TabletArea &second,
                            double tolerance = 1e-9);

// The normalized width/height ratio an output rectangle needs so that its
// on-screen proportions equal the physical proportions of `input` on the
// tablet. Tablet extents are in millimetres as the user sees the tablet;
// surface extents are in logical pixels. 0 when any extent is unknown, so a
// caller can never lock to a guessed ratio.
[[nodiscard]] double proportionalOutputAspect(const TabletArea &input,
                                              double tabletWidth,
                                              double tabletHeight,
                                              double surfaceWidth,
                                              double surfaceHeight);

// `output` reshaped to proportionalOutputAspect(): same centre and same area
// where it fits, shrunk to fit the surface where it does not. Returns
// `output` unchanged when the ratio is unknown.
[[nodiscard]] TabletArea proportionalOutputArea(const TabletArea &input,
                                                double tabletWidth,
                                                double tabletHeight,
                                                const TabletArea &output,
                                                double surfaceWidth,
                                                double surfaceHeight);

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
