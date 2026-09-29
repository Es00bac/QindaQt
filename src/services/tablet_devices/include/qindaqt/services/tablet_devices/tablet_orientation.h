// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/tablet_devices/tablet_device_port.h>

#include <QtGlobal>

#include <optional>

namespace QindaQt::Services::TabletDevices {

// Pure rotation algebra for tablet orientation (ADR-0285). Nothing here talks
// to KWin: the placement planner composes these values and the port writes
// them, so every rule is testable without a device.

// A turn of the unit square by whole clockwise quarter turns, in screen
// coordinates (x grows to the right, y grows DOWN). Cw90 carries the top edge
// to the right edge: the point (x, y) lands on (1 - y, x).
//
// AGENT-CONTRACT: one enum serves every rotation in the tablet pipeline
// because all three use this same convention:
//  - KWin's orientation calibration: Qt::PortraitOrientation is the "90 deg
//    cw" matrix (x, y) -> (1 - y, x) (kwin-6.6.6
//    src/backends/libinput/device.cpp setOrientedCalibrationMatrix);
//  - KWin's output transform as it is applied to tablet input: Rotate90 maps
//    a native panel point to (height - y, x) (connection.cpp
//    devicePointToGlobalPosition), the same map once normalized;
//  - the user's choice: "turned 90° clockwise" means the tablet's top edge now
//    faces right, so a pen at the physical top-right reads native (0, 0) and
//    must land on the screen's top-right, (1 - 0, 0).
// A change to any one of these conventions must change all three call sites.
enum class Rotation : quint8 {
    None = 0,
    Cw90 = 1,
    Cw180 = 2,
    Cw270 = 3,
};

[[nodiscard]] int rotationDegrees(Rotation rotation) noexcept;
// Exactly 0, 90, 180 or 270. Anything else, including 360 and -90, is not a
// value a record or a control may carry, so it is refused rather than wrapped.
[[nodiscard]] std::optional<Rotation> rotationFromDegrees(int degrees) noexcept;
// Rotations of the square commute, so the order of the two turns is free.
[[nodiscard]] Rotation composeRotations(Rotation first, Rotation second) noexcept;
[[nodiscard]] Rotation inverseRotation(Rotation rotation) noexcept;
// True for the quarter turns that swap a surface's width and height.
[[nodiscard]] bool swapsAxes(Rotation rotation) noexcept;

// KWin's `orientationDBus` carries a Qt::ScreenOrientation as a plain int.
//
// AGENT-GUARD: KWin casts ANY int to the enum and persists it; an unknown
// value falls through to "no rotation" in the matrix while staying stored in
// kcminputrc. Only the five enum values are ever sent, and an unknown value
// read back is reported as nothing (std::nullopt), never as upright.
[[nodiscard]] int kwinOrientationFor(Rotation rotation) noexcept;
[[nodiscard]] std::optional<Rotation>
rotationFromKWinOrientation(int orientation) noexcept;

// Where a normalized rectangle lands when the whole unit square turns by
// `rotation`. Each member is a sum or difference of the inputs, so a turn
// followed by its inverse returns the rectangle to within a few ulps; callers
// that write the result pass it through normalizedArea() first.
[[nodiscard]] TabletArea rotateArea(const TabletArea &area,
                                    Rotation rotation) noexcept;

// What the user asked for on a desk tablet, in the frames they SEE
// (ADR-0285): how the tablet is turned on the desk relative to the screen's
// up, the used part of the tablet as it lies in front of them, and the part
// of the mapped screen (or workspace) as it appears on screen.
//
// AGENT-CONTRACT: these are intents, not KWin values. The planner derives
// KWin's orientationDBus/inputArea/outputArea from them and the mapped
// output's rotation, so a monitor turned in Displays re-bases every value and
// the pen keeps doing what the user chose. An absent member was never
// recorded; the planner adopts it from the device (see tablet_placement.h).
struct TabletPlacementIntent {
    std::optional<Rotation> rotation;
    std::optional<TabletArea> inputArea;
    std::optional<TabletArea> outputArea;

    friend bool operator==(const TabletPlacementIntent &,
                           const TabletPlacementIntent &) = default;
};

} // namespace QindaQt::Services::TabletDevices
