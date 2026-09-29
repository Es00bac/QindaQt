// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/tablet_devices/tablet_device_port.h>
#include <qindaqt/services/tablet_devices/tablet_mapping_ledger.h>
#include <qindaqt/services/tablet_devices/tablet_orientation.h>
#include <qindaqt/services/tablet_devices/tablet_output_matcher.h>

#include <QList>
#include <QString>
#include <QVariant>

#include <optional>

namespace QindaQt::Services::TabletDevices {

// ADR-0285 placement planning: from what the user asked for and how the
// mapped screen is rotated, the exact KWin property writes that make a tablet
// behave that way. Pure: the session policy and the Settings route both call
// it and write the result through the port, so the two processes can never
// compute different values for the same device.
//
// The pipeline being planned against, in KWin 6.6.6 + libinput 1.31 order:
//   left-handed flip -> orientation (calibration matrix) -> input area crop
//   -> output area placement (native mode frame) -> output transform.
// The input area therefore lives in the post-orientation frame and the
// output area in the output's native frame, which is why both are re-based
// whenever the compensation changes.

// How the screen a tablet reaches is rotated, as KWin applies it to pen input.
struct MappedRotation {
    enum class State {
        // One rotation applies (a named screen, the whole workspace, or every
        // candidate for "the active screen" agrees).
        Known,
        // The tablet follows the active screen and the screens disagree: no
        // single compensation is right, so none is applied.
        Mixed,
        // The rotation is not known (no Display1 answer yet). Nothing is
        // planned; guessing would turn the pen the wrong way.
        Unknown,
    };
    State state = State::Unknown;
    Rotation rotation = Rotation::None; // meaningful only for Known

    // The turn a desk tablet is compensated for.
    [[nodiscard]] Rotation compensation() const noexcept {
        return state == State::Known ? rotation : Rotation::None;
    }

    friend bool operator==(const MappedRotation &,
                           const MappedRotation &) = default;
};

// AGENT-NOTE: KWin maps a tablet through `devicePointToGlobalPosition`, which
// applies the mapped output's transform; `mapToWorkspace` uses the workspace
// geometry with no transform; an outputName naming no present output falls
// back to the active output (kwin-6.6.6 src/backends/libinput/connection.cpp
// tabletToolPosition). This mirrors those three branches.
[[nodiscard]] MappedRotation
mappedRotation(TabletMapChoice choice, const QString &outputName,
               const QList<TabletOutputCandidate> &outputs);

// How a device can be turned at all.
enum class RotationMechanism {
    None,
    // libinput's own rotation (`rotation`, degrees). libinput 1.31 offers it
    // for pointer devices only; kept for a future libinput that adds tablets.
    LibinputRotation,
    // KWin's orientation, folded into the calibration matrix
    // (`orientationDBus`); needs `supportsCalibrationMatrix`.
    KWinOrientation,
};

[[nodiscard]] RotationMechanism
rotationMechanism(const TabletDeviceSnapshot &tool);

// The rotation the device currently applies through `mechanism`, or nothing
// when the property is missing or holds a value no rotation corresponds to.
[[nodiscard]] std::optional<Rotation>
appliedRotation(const TabletDeviceSnapshot &tool, RotationMechanism mechanism);

// The device's current `inputArea` or `outputArea`, or nothing when the
// property is missing or malformed.
[[nodiscard]] std::optional<TabletArea>
deviceArea(const TabletDeviceSnapshot &tool, const QString &property);

// One KWin property write; `property` is always in the port's closed table.
struct TabletPropertyWrite {
    QString property;
    QVariant value;
};

struct TabletPlacementPlan {
    // False when nothing can be planned (the screen rotation is unknown).
    // The caller then writes nothing AND records nothing.
    bool actionable = false;
    // The complete intent: recorded members kept, missing ones adopted from
    // the device. The caller records it so the next rotation re-bases it.
    TabletPlacementIntent intent;
    // In order; empty when the device already matches the plan.
    QList<TabletPropertyWrite> writes;
};

// Plans a desk tablet: rotation = the user's turn composed with the inverse
// of the compensation, both areas re-based into the frames KWin reads them in.
//
// AGENT-GUARD: adoption must round-trip. A missing rotation is Upright, never
// the device's current orientation: that value may already carry an earlier
// compensation, and adopting it would compensate twice after a rotation. A
// missing area is adopted from the device through the SAME relation the
// planner writes it with, so adopting and re-planning in one frame writes
// nothing.
[[nodiscard]] TabletPlacementPlan
planDeskTabletPlacement(const TabletDeviceSnapshot &tool,
                        const TabletPlacementIntent &recorded,
                        const MappedRotation &mapped);

// Plans a pen display: it follows its screen through KWin's output transform,
// so any rotation, orientation or left-handed flip still applied to it is
// stale and is cleared. Always actionable; the intent is left as recorded.
[[nodiscard]] TabletPlacementPlan
planPenDisplayPlacement(const TabletDeviceSnapshot &tool);

} // namespace QindaQt::Services::TabletDevices
