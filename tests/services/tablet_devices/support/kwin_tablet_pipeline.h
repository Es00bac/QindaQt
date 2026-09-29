// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/tablet_devices/tablet_device_port.h>
#include <qindaqt/services/tablet_devices/tablet_geometry.h>
#include <qindaqt/services/tablet_devices/tablet_orientation.h>
#include <qindaqt/services/tablet_devices/tablet_output_matcher.h>
#include <qindaqt/services/tablet_devices/tablet_placement.h>

#include <QRectF>
#include <QString>

#include <array>
#include <cmath>
#include <cstddef>
#include <optional>

// Fixtures and an independent model of the KWin 6.6.6 + libinput 1.31.3
// tablet pipeline, shared by the orientation-value and placement rows
// (ADR-0285). The model is written from those sources, not from the code
// under test, so a drift in the planner's conventions fails a row.
namespace QindaQt::Tests::TabletPipeline {

using namespace QindaQt::Services::TabletDevices;

inline constexpr std::array<Rotation, 4> AllRotations{Rotation::None, Rotation::Cw90,
                                               Rotation::Cw180, Rotation::Cw270};

struct Point {
    double x = 0.0;
    double y = 0.0;
};

inline bool near(double first, double second) { return std::abs(first - second) < 1e-9; }

inline bool nearPoint(Point first, Point second) {
    return near(first.x, second.x) && near(first.y, second.y);
}

inline QString describe(Point point) {
    return QStringLiteral("(%1, %2)").arg(point.x).arg(point.y);
}

// ---------------------------------------------------------------------------
// An independent model of the pipeline under test, written from the KWin
// 6.6.6 and libinput 1.31.3 sources rather than from the code being tested.
// If the planner's conventions drift from these, the rows below fail.
// ---------------------------------------------------------------------------

// The user turns the physical tablet clockwise by `turns` quarter turns. Its
// native top-left corner then sits at the seen top-right: seen = (1 - y, x)
// for one quarter turn. This returns the native point under a seen point.
inline Point nativeUnderSeen(int turns, Point seen) {
    Point native = seen;
    for (int step = 0; step < turns; ++step) {
        // Undo one clockwise quarter turn: (sx, sy) came from (sy, 1 - sx).
        native = Point{native.y, 1.0 - native.x};
    }
    return native;
}

// kwin-6.6.6 src/backends/libinput/device.cpp setOrientedCalibrationMatrix:
// Portrait [0 -1 1; 1 0 0], InvertedLandscape [-1 0 1; 0 -1 1],
// InvertedPortrait [0 1 0; -1 0 1]; Primary/Landscape identity.
inline Point kwinOrientation(int orientation, Point p) {
    switch (orientation) {
    case 1:
        return Point{-p.y + 1.0, p.x};
    case 8:
        return Point{-p.x + 1.0, -p.y + 1.0};
    case 4:
        return Point{p.y, -p.x + 1.0};
    default:
        return p;
    }
}

// kwin-6.6.6 src/backends/libinput/connection.cpp devicePointToGlobalPosition,
// normalized by the output's mode size: Rotate90 (H - y, x), Rotate180
// (W - x, H - y), Rotate270 (y, W - x).
inline Point kwinOutputTransform(Rotation transform, Point p) {
    switch (transform) {
    case Rotation::Cw90:
        return Point{1.0 - p.y, p.x};
    case Rotation::Cw180:
        return Point{1.0 - p.x, 1.0 - p.y};
    case Rotation::Cw270:
        return Point{p.y, 1.0 - p.x};
    case Rotation::None:
        break;
    }
    return p;
}

// libinput apply_tablet_area + KWin transformedPosition's output area.
inline Point crop(Point p, const TabletArea &area) {
    return Point{(p.x - area.x) / area.width, (p.y - area.y) / area.height};
}

inline Point place(Point p, const TabletArea &area) {
    return Point{area.x + p.x * area.width, area.y + p.y * area.height};
}

struct DeviceState {
    int orientation = 0;
    TabletArea inputArea;
    TabletArea outputArea;
};

// Where KWin puts the cursor, normalized to the screen as the user sees it,
// when the user touches `seen` on a tablet turned by `userTurns`.
inline Point cursorFor(Point seen, int userTurns, const DeviceState &state,
                Rotation outputTransform) {
    const Point native = nativeUnderSeen(userTurns, seen);
    const Point oriented = kwinOrientation(state.orientation, native);
    const Point cropped = crop(oriented, state.inputArea);
    const Point placed = place(cropped, state.outputArea);
    return kwinOutputTransform(outputTransform, placed);
}

inline TabletDeviceSnapshot deskTablet() {
    // The owner's Wacom Bamboo Connect CTL-470 as KWin 6.6.6 reported it on
    // qinda-top, 2026-09-28 (libinput built without libwacom).
    TabletDeviceSnapshot pen;
    pen.deviceId = QStringLiteral("event3");
    pen.name = QStringLiteral("Wacom Bamboo Connect Pen");
    pen.vendorId = 1386;
    pen.productId = 221;
    pen.tabletTool = true;
    pen.properties = QVariantMap{
        {QStringLiteral("outputName"), QString()},
        {QStringLiteral("mapToWorkspace"), false},
        {QStringLiteral("outputArea"), QVariantList{0.0, 0.0, 1.0, 1.0}},
        {QStringLiteral("inputArea"), QVariantList{0.0, 0.0, 1.0, 1.0}},
        {QStringLiteral("orientationDBus"), 0},
        {QStringLiteral("rotation"), 0U},
        {QStringLiteral("leftHanded"), false},
        {QStringLiteral("size"), QVariantList{147.2, 92.0}},
        {QStringLiteral("supportsCalibrationMatrix"), true},
        {QStringLiteral("supportsInputArea"), true},
        {QStringLiteral("supportsOutputArea"), true},
        {QStringLiteral("supportsRotation"), false},
        {QStringLiteral("supportsLeftHanded"), true},
    };
    return pen;
}

inline TabletDeviceSnapshot penDisplay() {
    TabletDeviceSnapshot pen = deskTablet();
    pen.deviceId = QStringLiteral("event19");
    pen.name = QStringLiteral("Wacom One Pen Display 13 Pen");
    pen.productId = 934;
    // libinput offers no tablet area for an INPUT_PROP_DIRECT tablet.
    pen.properties.insert(QStringLiteral("supportsInputArea"), false);
    pen.properties.insert(QStringLiteral("outputName"),
                          QStringLiteral("HDMI-A-1"));
    return pen;
}

inline TabletOutputCandidate screen(const QString &connector,
                             std::optional<Rotation> rotation,
                             const QRectF &geometry = QRectF(0, 0, 1920, 1080)) {
    TabletOutputCandidate output{connector, QStringLiteral("Dell Inc."),
                                 QStringLiteral("U2720Q"),
                                 QStringLiteral("U2720Q"), false, true};
    output.rotation = rotation;
    output.logicalGeometry = geometry;
    return output;
}

inline MappedRotation known(Rotation rotation) {
    return MappedRotation{MappedRotation::State::Known, rotation};
}

inline void applyWrites(const TabletPlacementPlan &plan, TabletDeviceSnapshot *tool) {
    for (const TabletPropertyWrite &write : plan.writes) {
        tool->properties.insert(write.property, write.value);
    }
}

inline DeviceState stateOf(const TabletDeviceSnapshot &tool) {
    DeviceState state;
    state.orientation =
        tool.properties.value(QStringLiteral("orientationDBus")).toInt();
    state.inputArea = deviceArea(tool, QStringLiteral("inputArea")).value();
    state.outputArea = deviceArea(tool, QStringLiteral("outputArea")).value();
    return state;
}

} // namespace QindaQt::Tests::TabletPipeline
