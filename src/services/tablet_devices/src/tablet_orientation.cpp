// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/tablet_devices/tablet_orientation.h>

#include <QtCore/qnamespace.h>

namespace QindaQt::Services::TabletDevices {
namespace {

constexpr int QuarterTurnsPerTurn = 4;
constexpr int DegreesPerQuarterTurn = 90;

int quarterTurns(Rotation rotation) noexcept {
    return static_cast<int>(rotation);
}

Rotation fromQuarterTurns(int turns) noexcept {
    const int normalized =
        ((turns % QuarterTurnsPerTurn) + QuarterTurnsPerTurn) %
        QuarterTurnsPerTurn;
    return static_cast<Rotation>(normalized);
}

constexpr int orientationValue(Qt::ScreenOrientation orientation) noexcept {
    return static_cast<int>(orientation);
}

} // namespace

int rotationDegrees(Rotation rotation) noexcept {
    return quarterTurns(rotation) * DegreesPerQuarterTurn;
}

std::optional<Rotation> rotationFromDegrees(int degrees) noexcept {
    switch (degrees) {
    case 0:
        return Rotation::None;
    case 90:
        return Rotation::Cw90;
    case 180:
        return Rotation::Cw180;
    case 270:
        return Rotation::Cw270;
    default:
        return std::nullopt;
    }
}

Rotation composeRotations(Rotation first, Rotation second) noexcept {
    return fromQuarterTurns(quarterTurns(first) + quarterTurns(second));
}

Rotation inverseRotation(Rotation rotation) noexcept {
    return fromQuarterTurns(QuarterTurnsPerTurn - quarterTurns(rotation));
}

bool swapsAxes(Rotation rotation) noexcept {
    return rotation == Rotation::Cw90 || rotation == Rotation::Cw270;
}

int kwinOrientationFor(Rotation rotation) noexcept {
    // AGENT-NOTE: the three non-trivial values follow KWin's matrix table,
    // not Qt's names: Portrait is 90° clockwise, InvertedLandscape 180°,
    // InvertedPortrait 270° (kwin-6.6.6 src/backends/libinput/device.cpp
    // setOrientedCalibrationMatrix).
    switch (rotation) {
    case Rotation::Cw90:
        return orientationValue(Qt::PortraitOrientation);
    case Rotation::Cw180:
        return orientationValue(Qt::InvertedLandscapeOrientation);
    case Rotation::Cw270:
        return orientationValue(Qt::InvertedPortraitOrientation);
    case Rotation::None:
        break;
    }
    // Primary is KWin's own default ("Orientation" absent from kcminputrc).
    return orientationValue(Qt::PrimaryOrientation);
}

std::optional<Rotation> rotationFromKWinOrientation(int orientation) noexcept {
    switch (orientation) {
    case orientationValue(Qt::PrimaryOrientation):
    case orientationValue(Qt::LandscapeOrientation):
        // KWin's matrix table treats both as the identity.
        return Rotation::None;
    case orientationValue(Qt::PortraitOrientation):
        return Rotation::Cw90;
    case orientationValue(Qt::InvertedLandscapeOrientation):
        return Rotation::Cw180;
    case orientationValue(Qt::InvertedPortraitOrientation):
        return Rotation::Cw270;
    default:
        return std::nullopt;
    }
}

TabletArea rotateArea(const TabletArea &area, Rotation rotation) noexcept {
    switch (rotation) {
    case Rotation::Cw90:
        // (x, y) -> (1 - y, x): the bottom edge becomes the left edge.
        return TabletArea{1.0 - (area.y + area.height), area.x, area.height,
                          area.width};
    case Rotation::Cw180:
        return TabletArea{1.0 - (area.x + area.width),
                          1.0 - (area.y + area.height), area.width,
                          area.height};
    case Rotation::Cw270:
        // (x, y) -> (y, 1 - x): the right edge becomes the top edge.
        return TabletArea{area.y, 1.0 - (area.x + area.width), area.height,
                          area.width};
    case Rotation::None:
        break;
    }
    return area;
}

} // namespace QindaQt::Services::TabletDevices
