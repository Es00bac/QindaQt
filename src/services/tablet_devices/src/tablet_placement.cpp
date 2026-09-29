// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/tablet_devices/tablet_placement.h>

#include <qindaqt/services/tablet_devices/tablet_geometry.h>

namespace QindaQt::Services::TabletDevices {
namespace {

bool flag(const TabletDeviceSnapshot &tool, const QString &name) {
    const QVariant value = tool.properties.value(name);
    return value.typeId() == QMetaType::Bool && value.toBool();
}

// "The active screen" is whichever enabled output KWin considers active at
// the moment of each pen event, so a compensation is only right when every
// candidate agrees.
MappedRotation followActiveRotation(const QList<TabletOutputCandidate> &outputs) {
    std::optional<Rotation> common;
    bool mixed = false;
    for (const TabletOutputCandidate &output : outputs) {
        if (!output.enabled || output.connectorName.isEmpty()) {
            continue;
        }
        if (!output.rotation.has_value()) {
            return MappedRotation{};
        }
        if (!common.has_value()) {
            common = output.rotation;
        } else if (*common != *output.rotation) {
            mixed = true;
        }
    }
    if (!common.has_value()) {
        return MappedRotation{};
    }
    if (mixed) {
        return MappedRotation{MappedRotation::State::Mixed, Rotation::None};
    }
    return MappedRotation{MappedRotation::State::Known, *common};
}

void appendRotationWrites(const TabletDeviceSnapshot &tool,
                          RotationMechanism mechanism, Rotation target,
                          QList<TabletPropertyWrite> *writes) {
    switch (mechanism) {
    case RotationMechanism::LibinputRotation: {
        // A calibration orientation left behind by an earlier mechanism would
        // turn the pen a second time on top of libinput's rotation.
        const std::optional<Rotation> stale =
            appliedRotation(tool, RotationMechanism::KWinOrientation);
        if (flag(tool, QStringLiteral("supportsCalibrationMatrix")) &&
            tool.properties.contains(QStringLiteral("orientationDBus")) &&
            stale != Rotation::None) {
            writes->append(TabletPropertyWrite{
                QStringLiteral("orientationDBus"),
                QVariant::fromValue(kwinOrientationFor(Rotation::None))});
        }
        if (appliedRotation(tool, mechanism) != target) {
            writes->append(TabletPropertyWrite{
                QStringLiteral("rotation"),
                QVariant::fromValue(
                    static_cast<uint>(rotationDegrees(target)))});
        }
        break;
    }
    case RotationMechanism::KWinOrientation:
        if (appliedRotation(tool, mechanism) != target) {
            writes->append(TabletPropertyWrite{
                QStringLiteral("orientationDBus"),
                QVariant::fromValue(kwinOrientationFor(target))});
        }
        break;
    case RotationMechanism::None:
        break;
    }
}

} // namespace

MappedRotation mappedRotation(TabletMapChoice choice, const QString &outputName,
                              const QList<TabletOutputCandidate> &outputs) {
    switch (choice) {
    case TabletMapChoice::EntireWorkspace:
        // mapToWorkspace positions the pen in workspace coordinates with no
        // output transform at all.
        return MappedRotation{MappedRotation::State::Known, Rotation::None};
    case TabletMapChoice::NamedOutput:
        for (const TabletOutputCandidate &output : outputs) {
            if (!output.enabled || outputName.isEmpty() ||
                output.connectorName != outputName) {
                continue;
            }
            if (!output.rotation.has_value()) {
                return MappedRotation{};
            }
            return MappedRotation{MappedRotation::State::Known,
                                  *output.rotation};
        }
        // Not present: KWin routes the pen to the active output instead.
        break;
    case TabletMapChoice::FollowActiveScreen:
        break;
    }
    return followActiveRotation(outputs);
}

RotationMechanism rotationMechanism(const TabletDeviceSnapshot &tool) {
    if (!tool.tabletTool) {
        return RotationMechanism::None;
    }
    if (flag(tool, QStringLiteral("supportsRotation"))) {
        return RotationMechanism::LibinputRotation;
    }
    // AGENT-GUARD: KWin's setOrientation returns without a word when the
    // device lacks a calibration matrix, so the D-Bus Set succeeds while
    // nothing turns. Offer orientation only where it can take effect.
    if (flag(tool, QStringLiteral("supportsCalibrationMatrix")) &&
        tool.properties.contains(QStringLiteral("orientationDBus"))) {
        return RotationMechanism::KWinOrientation;
    }
    return RotationMechanism::None;
}

std::optional<Rotation> appliedRotation(const TabletDeviceSnapshot &tool,
                                        RotationMechanism mechanism) {
    switch (mechanism) {
    case RotationMechanism::LibinputRotation: {
        const QVariant value = tool.properties.value(QStringLiteral("rotation"));
        bool ok = false;
        const uint degrees = value.toUInt(&ok);
        if (!value.isValid() || !ok || degrees > 270U) {
            return std::nullopt;
        }
        return rotationFromDegrees(static_cast<int>(degrees));
    }
    case RotationMechanism::KWinOrientation: {
        const QVariant value =
            tool.properties.value(QStringLiteral("orientationDBus"));
        bool ok = false;
        const int orientation = value.toInt(&ok);
        if (!value.isValid() || !ok) {
            return std::nullopt;
        }
        return rotationFromKWinOrientation(orientation);
    }
    case RotationMechanism::None:
        break;
    }
    return std::nullopt;
}

std::optional<TabletArea> deviceArea(const TabletDeviceSnapshot &tool,
                                     const QString &property) {
    bool ok = false;
    const TabletArea area =
        TabletArea::fromVariant(tool.properties.value(property), &ok);
    if (!ok) {
        return std::nullopt;
    }
    return area;
}

TabletPlacementPlan planDeskTabletPlacement(const TabletDeviceSnapshot &tool,
                                            const TabletPlacementIntent &recorded,
                                            const MappedRotation &mapped) {
    TabletPlacementPlan plan;
    plan.intent = recorded;
    if (!tool.tabletTool || mapped.state == MappedRotation::State::Unknown) {
        return plan;
    }
    plan.actionable = true;
    const Rotation compensation = mapped.compensation();
    const RotationMechanism mechanism = rotationMechanism(tool);
    const Rotation current =
        appliedRotation(tool, mechanism).value_or(Rotation::None);
    const Rotation user = recorded.rotation.value_or(Rotation::None);
    plan.intent.rotation = user;
    // What the device applies once the plan is written: the user's turn with
    // the screen's turn taken back out, or whatever it has if it cannot turn.
    const Rotation target =
        mechanism == RotationMechanism::None
            ? current
            : composeRotations(user, inverseRotation(compensation));
    appendRotationWrites(tool, mechanism, target, &plan.writes);

    // The input area lives after the orientation. The frame the user sees is
    // that frame turned by (user - applied); a left-handed flip precedes both
    // and cancels out of the relation.
    const std::optional<TabletArea> currentInput =
        flag(tool, QStringLiteral("supportsInputArea"))
            ? deviceArea(tool, QStringLiteral("inputArea"))
            : std::nullopt;
    if (currentInput.has_value()) {
        if (!plan.intent.inputArea.has_value()) {
            plan.intent.inputArea =
                normalizedArea(rotateArea(*currentInput,
                                          composeRotations(
                                              user, inverseRotation(current))))
                    .value_or(TabletArea{});
        }
        const std::optional<TabletArea> wanted = normalizedArea(rotateArea(
            *plan.intent.inputArea,
            composeRotations(target, inverseRotation(user))));
        if (wanted.has_value() && !sameArea(*wanted, *currentInput)) {
            plan.writes.append(TabletPropertyWrite{
                QStringLiteral("inputArea"), wanted->toVariantList()});
        }
    }

    // The output area lives in the output's native frame, before its
    // transform turns it into what the user sees.
    const std::optional<TabletArea> currentOutput =
        deviceArea(tool, QStringLiteral("outputArea"));
    if (currentOutput.has_value()) {
        if (!plan.intent.outputArea.has_value()) {
            plan.intent.outputArea =
                normalizedArea(rotateArea(*currentOutput, compensation))
                    .value_or(TabletArea{});
        }
        const std::optional<TabletArea> wanted = normalizedArea(
            rotateArea(*plan.intent.outputArea, inverseRotation(compensation)));
        if (wanted.has_value() && !sameArea(*wanted, *currentOutput)) {
            plan.writes.append(TabletPropertyWrite{
                QStringLiteral("outputArea"), wanted->toVariantList()});
        }
    }
    return plan;
}

TabletPlacementPlan planPenDisplayPlacement(const TabletDeviceSnapshot &tool) {
    TabletPlacementPlan plan;
    plan.actionable = true;
    if (!tool.tabletTool) {
        return plan;
    }
    if (flag(tool, QStringLiteral("supportsRotation")) &&
        tool.properties.contains(QStringLiteral("rotation")) &&
        appliedRotation(tool, RotationMechanism::LibinputRotation) !=
            Rotation::None) {
        plan.writes.append(TabletPropertyWrite{QStringLiteral("rotation"),
                                               QVariant::fromValue(0U)});
    }
    if (flag(tool, QStringLiteral("supportsCalibrationMatrix")) &&
        tool.properties.contains(QStringLiteral("orientationDBus")) &&
        appliedRotation(tool, RotationMechanism::KWinOrientation) !=
            Rotation::None) {
        plan.writes.append(TabletPropertyWrite{
            QStringLiteral("orientationDBus"),
            QVariant::fromValue(kwinOrientationFor(Rotation::None))});
    }
    // AGENT-NOTE: libinput's left-handed mode turns a tablet 180° before
    // anything else. On a surface that IS the screen that always points the
    // pen away from the tip; libinput itself withholds the option from
    // display tablets when libwacom is present.
    if (flag(tool, QStringLiteral("supportsLeftHanded")) &&
        flag(tool, QStringLiteral("leftHanded"))) {
        plan.writes.append(
            TabletPropertyWrite{QStringLiteral("leftHanded"), QVariant(false)});
    }
    return plan;
}

} // namespace QindaQt::Services::TabletDevices
