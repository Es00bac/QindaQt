// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0
import QindaTK as Tk
import QindaTK.QindaQt

// Pen & tablet rows. Every control a tablet cannot honor is hidden through
// the selection's availability truth; nothing renders disabled-but-visible
// for an unsupported capability (ADR-0134).
ColumnLayout {
    id: root

    required property var inputSettings
    // Device group to select when the destination opens, from the
    // notification's deep link. Empty means "whatever was selected".
    property string initialSelection: ""

    // A pen display turns with its screen; its rotation is set in Displays.
    signal displaySettingsRequested()

    readonly property var tablets: root.inputSettings.tabletDevices
    readonly property var selection: root.tablets.selection
    readonly property var placement: root.selection !== null ? root.selection.placement : null
    readonly property bool hasTablets: root.tablets.count > 0

    readonly property Item firstFocusTarget: degraded.visible ? degraded
                                             : noTablets.visible ? noTablets
                                             : devicePicker.visible ? devicePicker
                                             : mappingSection.firstFocusTarget

    Component.onCompleted: {
        root.tablets.refresh()
        if (root.initialSelection.length > 0)
            root.tablets.selectGroup(root.initialSelection)
    }

    spacing: Tokens.space["2"]

    Label {
        objectName: "tabletStatusText"
        Layout.fillWidth: true
        visible: text.length > 0
        text: root.selection !== null ? root.selection.statusText : ""
        Accessible.role: Accessible.AlertMessage
        Accessible.name: text
    }

    DegradedNotice {
        id: degraded
        objectName: "tabletDegraded"
        Layout.fillWidth: true
        visible: !root.tablets.available
        reason: qsTr("Tablets are unavailable right now.")
    }

    // AGENT-NOTE: feeds the session theme into QindaTK's Theme singleton so
    // the empty state below matches the page even when no other QindaTK
    // surface has been opened yet (bridges are idempotent).
    QindaQtTheme {}

    Tk.EmptyState {
        id: noTablets
        objectName: "tabletNoDevices"
        Layout.fillWidth: true
        visible: root.tablets.available && !root.hasTablets
        iconName: "pen-tool"
        text: qsTr("No pen tablet or pen display is connected. Plug one in and it will appear here.")
    }

    FormRow {
        id: devicePicker
        objectName: "tabletDevicePicker"
        Layout.fillWidth: true
        visible: root.hasTablets && root.tablets.count > 1
        label: qsTr("Tablet")
        description: qsTr("Choose which tablet these settings change")
        editor: ComboBox {
            objectName: "tabletDeviceCombo"
            textRole: "label"
            model: root.tablets
            width: 320
            onActivated: index => root.tablets.select(index)
        }
    }

    // AGENT-GUARD: Same rule as the pointer picker — QQuickComboBox keeps
    // currentIndex at -1 when its model resets into non-empty, so the
    // picker binds to the model's own selected index through a Binding
    // element that survives the combo's imperative write.
    Binding {
        target: devicePicker.editor
        property: "currentIndex"
        value: root.tablets.selectedIndex
        restoreMode: Binding.RestoreBindingOrValue
    }

    TabletMappingSection {
        id: mappingSection
        objectName: "tabletMappingSection"
        Layout.fillWidth: true
        visible: root.hasTablets && root.selection !== null
        selection: root.selection
    }

    SectionHeader {
        Layout.fillWidth: true
        visible: root.hasTablets
        title: qsTr("The pen")
        description: qsTr("Orientation, mode, and how the tip behaves")
    }

    // ADR-0285: a desk tablet's rotation is how it lies on the desk; the
    // placement model keeps its up the screen's up even on a rotated screen.
    FormRow {
        id: rotationRow
        objectName: "tabletRotationRow"
        Layout.fillWidth: true
        visible: root.placement !== null && root.placement.rotationAvailable
        label: qsTr("Rotation")
        description: qsTr("Match how the tablet lies on your desk. Up on the tablet stays up on the screen.")
        editor: ComboBox {
            id: rotationCombo
            objectName: "tabletRotationCombo"
            width: 260
            model: [qsTr("Upright"), qsTr("Turned 90° clockwise"),
                    qsTr("Upside down (180°)"), qsTr("Turned 90° counterclockwise")]
            onActivated: index => root.placement.setRotation(index * 90)
        }
    }

    // AGENT-GUARD: same rule as the device picker. The combo's own write
    // breaks a `currentIndex:` binding, and a refused rotation would then
    // show a turn the tablet never took.
    Binding {
        target: rotationCombo
        property: "currentIndex"
        value: root.placement !== null ? Math.round(root.placement.rotation / 90) % 4 : 0
        restoreMode: Binding.RestoreBindingOrValue
    }

    Label {
        objectName: "tabletRotationNote"
        Layout.fillWidth: true
        visible: rotationRow.visible && text.length > 0
        text: root.placement !== null ? root.placement.rotationNote : ""
        muted: true
        Accessible.name: text
    }

    // A pen display has no rotation of its own: it turns with its screen
    // (ADR-0285), so the row says which one and leads to where it is set,
    // the way the Display card leads here.
    RowLayout {
        objectName: "tabletFollowsScreenRow"
        Layout.fillWidth: true
        visible: root.placement !== null && root.placement.penDisplay
        spacing: Tokens.space["2"]

        Label {
            objectName: "tabletFollowsScreenNote"
            Layout.fillWidth: true
            muted: true
            text: root.placement !== null ? root.placement.followNote : ""
        }

        Button {
            objectName: "tabletOpenDisplays"
            text: qsTr("Display settings…")
            emphasized: false
            accessibleDescription: qsTr("Open Displays to rotate the screen; the pen display turns with it")
            onClicked: root.displaySettingsRequested()
        }
    }

    FormRow {
        objectName: "tabletLeftHandedRow"
        Layout.fillWidth: true
        visible: root.selection !== null && root.selection.leftHandedAvailable
        label: qsTr("Left-handed")
        description: qsTr("Flip the tablet so the buttons sit on the other side")
        editor: Switch {
            objectName: "tabletLeftHandedSwitch"
            checked: root.selection !== null && root.selection.leftHanded
            accessibleDescription: qsTr("Rotate the tablet surface for left-handed use")
            onToggled: root.selection.leftHanded = checked
        }
    }

    FormRow {
        objectName: "tabletPenModeRow"
        Layout.fillWidth: true
        visible: root.selection !== null && root.selection.relativeModeAvailable
        label: qsTr("Pen mode")
        description: qsTr("Absolute puts the cursor where the pen is; relative moves it like a mouse")
        editor: ComboBox {
            objectName: "tabletPenModeCombo"
            width: 200
            model: [qsTr("Absolute"), qsTr("Relative")]
            currentIndex: root.selection !== null && root.selection.relativeMode ? 1 : 0
            onActivated: index => root.selection.relativeMode = (index === 1)
        }
    }

    FormRow {
        objectName: "tabletEnabledRow"
        Layout.fillWidth: true
        visible: root.selection !== null && root.selection.deviceEnabledAvailable
        label: qsTr("Enable this tablet")
        description: qsTr("Turn the tablet off without unplugging it")
        editor: Switch {
            objectName: "tabletEnabledSwitch"
            checked: root.selection !== null && root.selection.deviceEnabled
            accessibleDescription: qsTr("Whether this tablet sends input")
            onToggled: root.selection.deviceEnabled = checked
        }
    }

    TabletCalibrationSection {
        objectName: "tabletCalibrationSection"
        Layout.fillWidth: true
        visible: root.selection !== null && root.selection.calibrationAvailable
        selection: root.selection
        onCalibrationRequested: calibrationLoader.active = true
    }

    TabletPressureSection {
        objectName: "tabletPressureSection"
        Layout.fillWidth: true
        visible: root.selection !== null && root.selection.pressureCurveAvailable
        selection: root.selection
    }

    TabletPadSection {
        objectName: "tabletPadSection"
        Layout.fillWidth: true
        visible: root.selection !== null && root.selection.hasPad
        selection: root.selection
    }

    RowLayout {
        Layout.fillWidth: true
        visible: root.hasTablets
        spacing: Tokens.space["2"]

        Button {
            objectName: "tabletResetDevice"
            text: qsTr("Reset this tablet")
            emphasized: false
            destructive: true
            accessibleDescription: qsTr("Put every setting for this tablet back the way it shipped")
            onClicked: root.selection.resetDevice()
        }
    }

    // AGENT-GUARD: the wizard is its own full-screen window on the mapped
    // output, not an overlay inside this form. Measuring inside the settings
    // window would calibrate against whichever screen that window sits on.
    Loader {
        id: calibrationLoader
        objectName: "tabletCalibrationLoader"
        active: false
        sourceComponent: TabletCalibrationOverlay {
            selection: root.selection
            targetOutput: root.selection !== null ? root.selection.outputName : ""
            visible: true
            onFinished: calibrationLoader.active = false
        }
    }
}
