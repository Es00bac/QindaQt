// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0
import QindaTK as Tk

// One device-kind inventory list with default-device selection, volume, mute
// and latency-offset intents. Common controls occupy two compact lines;
// device-local Details exposes latency and channels without reserving a
// third line on every device. The Audio Settings wiki owns this layout.
ColumnLayout {
    id: root

    required property var audioSettings
    required property string sectionTitle
    required property string sectionDescription
    required property var deviceRows
    required property string emptyText
    required property string kindPrefix

    // Projected rows register their first/last enabled, admitted action
    // control for the page's focus entry and Tab cycle. Registration is
    // keyed by row index so the section target is always the first enabled,
    // admitted control in traversal order — never a control the projection
    // disabled (AGENT-GUARD: the Settings host forceActiveFocus()es
    // firstActionTarget directly; handing it a disabled control strands host
    // entry focus). Count-based delegates survive projection changes;
    // their action bindings refresh registration, and a destroying delegate
    // withdraws its row so a stale pointer is never handed to forceActiveFocus.
    property Item firstActionTarget: null
    property Item lastActionTarget: null
    property var actionRegistrations: ({})

    function updateActionRegistration(index, delegate) {
        root.actionRegistrations[index] = delegate
        root.refreshActionTargets()
    }

    function removeActionRegistration(index) {
        delete root.actionRegistrations[index]
        root.refreshActionTargets()
    }

    function refreshActionTargets() {
        const indices = Object.keys(root.actionRegistrations).map(Number)
        indices.sort((a, b) => a - b)
        let first = null
        let last = null
        for (const index of indices) {
            const delegate = root.actionRegistrations[index]
            if (first === null && delegate.firstEnabledAction !== null) {
                first = delegate.firstEnabledAction
            }
            if (delegate.lastEnabledAction !== null) {
                last = delegate.lastEnabledAction
            }
        }
        root.firstActionTarget = first
        root.lastActionTarget = last
    }

    Layout.fillWidth: true
    spacing: Tokens.space["1"]

    // Compact: the description is kept for assistive technology only.
    Tk.SectionHeader {
        Layout.fillWidth: true
        title: root.sectionTitle
        count: deviceRepeater.count > 0 ? String(deviceRepeater.count) : ""
        Accessible.description: root.sectionDescription
    }

    // AGENT-GUARD (mirrors ADR-0191, shell audio applet): the model is the
    // row *count*, not the row list. A Repeater handed a QVariantList
    // regenerates every delegate whenever that list is reassigned, and the
    // model reassigns it on every reprojection -- including the one a row's
    // own dispatch triggers. That destroyed the control the user was
    // holding, breaking a drag on its own no matter what the pending rule
    // says. Binding the row by index keeps the delegate alive and updates
    // its values in place. Known limitation (same as the applet): if a
    // device disappears from the middle of the list, indices below it shift,
    // and a held slider can dispatch to the device that took its index for
    // the rest of that one gesture -- the real fix is a QAbstractListModel,
    // out of scope for this slice.
    Repeater {
        id: deviceRepeater
        model: root.deviceRows.length

        delegate: FormSurface {
            id: deviceRow
            required property int index
            readonly property var modelData: root.deviceRows[index] ?? null
            // AGENT-GUARD: a transiently null row (the array shrank between
            // this delegate's index binding and the Repeater's own count
            // update) renders nothing rather than dereferencing modelData
            // fields; QtQuick Layouts excludes an invisible item from
            // sizing, so this never leaves a gap.
            visible: modelData !== null
            Layout.fillWidth: true
            padding: Tokens.space["1"]
            // AGENT-GUARD: disclosure is presentation state for this exact
            // device. A shifted list index must not expose the old device's
            // expanded controls on the replacement row.
            readonly property var targetSerial: modelData?.serial ?? 0
            readonly property bool hasChannels:
                (modelData?.channelVolumeAvailable ?? false)
                && root.audioSettings.canSetChannelVolumes
                && (modelData?.channelVolumes.length ?? 0) > 1
            property bool detailsExpanded: false
            property bool channelsExpanded: false
            onTargetSerialChanged: {
                detailsExpanded = false
                channelsExpanded = false
            }
            Accessible.name: qsTr("%1 %2, %3")
                .arg(deviceRow.modelData?.kindText ?? "")
                .arg(deviceRow.modelData?.displayName ?? "")
                .arg(deviceRow.modelData?.stateText ?? "")

            // Traversal order inside a row is set-default, volume, mute, the
            // Details disclosure, then expanded latency/channel controls.
            // Only nominate Details as a host ACTION target when it exposes
            // an admitted edit; a read-only latency reading must not precede
            // the next device's admitted action. It remains reachable in the
            // ordinary keyboard traversal for inspecting read-only values.
            readonly property bool detailsAdmitsAction:
                latencyControl.editable || channelsToggle.enabled
            readonly property Item firstEnabledAction:
                setDefaultButton.visible && setDefaultButton.enabled
                    ? setDefaultButton
                    : levelRow.entryControl.enabled ? levelRow.entryControl
                    : muteSwitch.enabled ? muteSwitch
                    : detailsToggle.visible && detailsAdmitsAction
                      ? detailsToggle
                    : detailsExpanded && latencyControl.entryControl !== null
                      ? latencyControl.entryControl
                    : detailsExpanded && latencyControl.resetControl !== null
                      ? latencyControl.resetControl
                    : channelsToggle.visible && channelsToggle.enabled
                      ? channelsToggle : null
            readonly property Item lastEnabledAction:
                channelLoader.item !== null
                        && channelLoader.item.lastEnabledControl !== null
                    ? channelLoader.item.lastEnabledControl
                : detailsExpanded && channelsToggle.visible && channelsToggle.enabled
                  ? channelsToggle
                : detailsExpanded && latencyControl.resetControl !== null
                  ? latencyControl.resetControl
                : detailsExpanded && latencyControl.entryControl !== null
                  ? latencyControl.entryControl
                : detailsToggle.visible && detailsAdmitsAction
                  ? detailsToggle
                : muteSwitch.enabled ? muteSwitch
                : levelRow.entryControl.enabled ? levelRow.entryControl
                : setDefaultButton.visible && setDefaultButton.enabled
                  ? setDefaultButton : null

            onFirstEnabledActionChanged:
                root.updateActionRegistration(deviceRow.index, deviceRow)
            onLastEnabledActionChanged:
                root.updateActionRegistration(deviceRow.index, deviceRow)

            Component.onCompleted:
                root.updateActionRegistration(deviceRow.index, deviceRow)
            Component.onDestruction:
                root.removeActionRegistration(deviceRow.index);
            // AGENT-NOTE: the semicolon above is load-bearing for
            // tools/check-source-shape, not QML syntax: its brace-depth
            // scanner resets on `;`/`{`/`}`. Without one here it reads
            // straight through into `contentItem`'s open brace and
            // misattributes this whole delegate body to
            // updateActionRegistration (a real 3-line function above),
            // reporting a false function-lines violation. Keep this
            // semicolon if this block is ever reordered.

            contentItem: ColumnLayout {
                spacing: Tokens.space["1"]

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Tokens.space["2"]

                    Label {
                        Layout.fillWidth: true
                        text: deviceRow.modelData?.displayName ?? ""
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }

                    Label {
                        text: deviceRow.modelData?.stateText ?? ""
                        muted: true
                        elide: Text.ElideRight
                        Layout.maximumWidth: implicitWidth
                    }

                    Tk.Button {
                        id: setDefaultButton
                        objectName: root.kindPrefix + "Default_"
                                    + (deviceRow.modelData?.serial ?? 0)
                        visible: !(deviceRow.modelData?.isDefault ?? true)
                        small: true
                        available: deviceRow.modelData?.setDefaultAvailable ?? false
                        busy: root.audioSettings.busy
                        text: qsTr("Set default")
                        tooltip: qsTr("Make %1 the default %2")
                            .arg(deviceRow.modelData?.displayName ?? "")
                            .arg(deviceRow.modelData?.kindText ?? "")
                        onClicked: deviceRow.modelData !== null
                            && root.audioSettings.setDefaultDevice(
                                   deviceRow.modelData.serial)
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Tokens.space["3"]

                    AudioLevelRow {
                        id: levelRow
                        Layout.fillWidth: true
                        targetRow: deviceRow.modelData
                        kindPrefix: root.kindPrefix
                        targetName: deviceRow.modelData?.displayName ?? ""
                        commit: level => deviceRow.modelData !== null
                            && root.audioSettings.setDeviceVolume(
                                   deviceRow.modelData.serial, level)
                    }

                    Tk.Switch {
                        id: muteSwitch
                        objectName: root.kindPrefix + "Mute_"
                                    + (deviceRow.modelData?.serial ?? 0)
                        small: true
                        text: qsTr("Mute")
                        checked: deviceRow.modelData?.muted ?? false
                        enabled: deviceRow.modelData?.muteAvailable ?? false
                        tooltip: qsTr("Mute %1")
                            .arg(deviceRow.modelData?.displayName ?? "")
                        onToggled: deviceRow.modelData !== null
                            && root.audioSettings.setDeviceMuted(
                                   deviceRow.modelData.serial, checked)
                    }

                    Tk.Button {
                        id: detailsToggle
                        objectName: root.kindPrefix + "Details_"
                                    + deviceRow.targetSerial
                        // AGENT-GUARD: derive presence from projected facts,
                        // never a child's effective visibility inside the
                        // collapsed details row (which would latch it closed).
                        visible: latencyControl.known || deviceRow.hasChannels
                        small: true
                        checkable: true
                        checked: deviceRow.detailsExpanded
                        text: qsTr("Details")
                        tooltip: qsTr("Latency offset and individual channels for %1")
                            .arg(deviceRow.modelData?.displayName ?? "")
                        Accessible.name: qsTr("%1 details")
                            .arg(deviceRow.modelData?.displayName ?? "")
                        Accessible.description: deviceRow.detailsExpanded
                            ? qsTr("Hide latency and channel controls")
                            : qsTr("Show latency and channel controls")
                        onClicked: {
                            deviceRow.detailsExpanded = !deviceRow.detailsExpanded
                            if (!deviceRow.detailsExpanded) {
                                deviceRow.channelsExpanded = false
                                detailsToggle.forceActiveFocus(Qt.OtherFocusReason)
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Tokens.space["2"]
                    visible: deviceRow.detailsExpanded

                    AudioLatencyControl {
                        id: latencyControl
                        targetRow: deviceRow.modelData
                        audioSettings: root.audioSettings
                        kindPrefix: root.kindPrefix
                    }

                    Item { Layout.fillWidth: true }

                    // The per-channel strip is opt-in per device row and only
                    // exists for an admitted multi-channel layout; hardware
                    // with one channel keeps the plain aggregate row.
                    Tk.Button {
                        id: channelsToggle

                        objectName: "audioChannelsToggle_"
                                    + (deviceRow.modelData?.serial ?? 0)
                        visible: deviceRow.hasChannels
                        small: true
                        available: (deviceRow.modelData?.channelVolumeAvailable ?? false)
                                   && root.audioSettings.canSetChannelVolumes
                        busy: root.audioSettings.busy
                        checkable: true
                        text: deviceRow.channelsExpanded
                              ? qsTr("Hide channels")
                              : qsTr("Channels")
                        tooltip: qsTr("Adjust %1 channels individually")
                            .arg(deviceRow.modelData?.displayName ?? "")
                        onClicked: deviceRow.channelsExpanded
                                    = !deviceRow.channelsExpanded
                    }
                }

                // Deferral, not visibility: a collapsed strip must not
                // instantiate its controls at all, so focus order and the
                // a11y tree only meet it once the row is opened.
                Loader {
                    id: channelLoader

                    Layout.fillWidth: true
                    active: deviceRow.detailsExpanded && deviceRow.channelsExpanded
                    visible: active
                    sourceComponent: Component {
                        AudioChannelStrip {
                            targetName: deviceRow.modelData?.displayName ?? ""
                            channelRows: deviceRow.modelData?.channelVolumes ?? []
                            available: (deviceRow.modelData?.channelVolumeAvailable ?? false)
                                       && root.audioSettings.canSetChannelVolumes
                            serial: deviceRow.modelData?.serial ?? 0
                            commit: (channelIndex, level) =>
                                deviceRow.modelData !== null
                                && root.audioSettings.setDeviceChannelVolume(
                                       deviceRow.modelData.serial, channelIndex,
                                       level)
                        }
                    }
                }
            }
        }
    }

    Label {
        Layout.fillWidth: true
        visible: deviceRepeater.count === 0
        text: root.emptyText
        muted: true
    }
}
