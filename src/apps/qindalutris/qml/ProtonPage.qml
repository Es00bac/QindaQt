// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QindaTK as Tk
import "parts" as Parts

// The Proton manager (ADR-0275 section 2): every build on this computer with
// what QindaQt knows about it, the default for NEW installs, and GE-Proton
// releases to add. Nothing here moves a game that is already set up.
Item {
    id: page

    // Rows from Protons.builds: name, displayName, version, origin, pinnable,
    // removable, label, status, notes, isDefault, usedBy.
    property var builds: []
    // Rows from Protons.releases: toolName, tag, published, sizeText,
    // installed, status.
    property var releases: []
    property bool busy: false
    property real progress: 0
    property string stageText: ""
    property string resultMessage: ""
    property bool succeeded: false
    property string details: ""
    // CompatInfo: the compatibility information in use.
    property string compatDate: ""
    property int compatGames: 0
    property bool compatBusy: false
    property string compatMessage: ""

    signal checkCompatRequested()
    signal makeDefaultRequested(string name)
    signal removeRequested(string name)
    signal checkReleasesRequested()
    signal installReleaseRequested(string toolName)
    signal cancelRequested()
    signal copyDetailsRequested(string details)

    function statusText(status, pinnable) {
        if (!pinnable) {
            return qsTr("Updated by Steam")
        }
        return status === "tested" ? qsTr("Tested by QindaQt")
             : status === "known-issues" ? qsTr("Known issues")
             : qsTr("Not tested")
    }
    function statusVariant(status, pinnable) {
        if (!pinnable) {
            return "default"
        }
        return status === "tested" ? "success" : status === "known-issues" ? "warning" : "default"
    }

    Tk.Scroll {
        anchors.fill: parent
        overflowX: Tk.Scroll.Hidden

        Tk.Flex {
            direction: Tk.Flex.Column
            gap: Tk.Theme.space.md
            padding: Tk.Theme.space.md
            width: page.width

            Tk.Heading {
                text: qsTr("Proton")
                level: 1
            }
            Tk.Caption {
                text: qsTr("Proton is what runs Windows games here. Each game stays on the "
                           + "build it was set up with; the default below is only used for "
                           + "games you install from now on.")
                wrapMode: Text.Wrap
                Tk.Flex.alignSelf: Tk.Flex.Stretch
            }

            Tk.Card {
                objectName: "compatInfoCard"
                Tk.Flex.alignSelf: Tk.Flex.Stretch
                Tk.Flex {
                    direction: Tk.Flex.Row
                    align: Tk.Flex.Center
                    gap: Tk.Theme.space.md
                    width: parent.width
                    Tk.Flex {
                        direction: Tk.Flex.Column
                        gap: Tk.Theme.space.xs
                        Tk.Flex.grow: 1
                        Tk.Flex.basis: 0
                        Tk.Label {
                            text: qsTr("Game compatibility information")
                            font.weight: Font.DemiBold
                        }
                        Tk.Caption {
                            objectName: "compatInfoText"
                            text: page.compatBusy ? qsTr("Checking…")
                                : page.compatMessage.length > 0 ? page.compatMessage
                                : page.compatDate.length > 0
                                  ? qsTr("%1 games, from %2.").arg(page.compatGames).arg(page.compatDate)
                                  : qsTr("Not available.")
                            wrapMode: Text.Wrap
                            Tk.Flex.alignSelf: Tk.Flex.Stretch
                        }
                    }
                    Tk.Button {
                        objectName: "compatCheckButton"
                        enabled: !page.compatBusy
                        text: qsTr("Check for newer")
                        iconName: "refresh-cw"
                        onClicked: page.checkCompatRequested()
                    }
                }
            }

            Parts.JobProgress {
                objectName: "protonJob"
                busy: page.busy
                progress: page.progress
                stageText: page.stageText
                resultMessage: page.resultMessage
                succeeded: page.succeeded
                details: page.details
                onCancelRequested: page.cancelRequested()
                onCopyDetailsRequested: function(text) { page.copyDetailsRequested(text) }
                Tk.Flex.alignSelf: Tk.Flex.Stretch
            }

            Tk.SectionHeader {
                title: qsTr("On this computer")
                Tk.Flex.alignSelf: Tk.Flex.Stretch
            }
            Tk.EmptyState {
                visible: page.builds.length === 0
                iconName: "layers"
                title: qsTr("No Proton build installed")
                text: qsTr("Install app-emulation/ge-proton-bin, or add a build below.")
                Tk.Flex.alignSelf: Tk.Flex.Stretch
            }
            Repeater {
                model: page.builds
                delegate: Tk.Card {
                    id: buildCard
                    required property var modelData
                    objectName: "buildCard-" + modelData.name
                    selected: modelData.isDefault
                    Tk.Flex.alignSelf: Tk.Flex.Stretch

                    Tk.Flex {
                        direction: Tk.Flex.Row
                        align: Tk.Flex.Center
                        gap: Tk.Theme.space.md
                        width: parent.width

                        Tk.Flex {
                            direction: Tk.Flex.Column
                            gap: Tk.Theme.space.xs
                            Tk.Flex.grow: 1
                            Tk.Flex.basis: 0
                            Tk.Flex {
                                direction: Tk.Flex.Row
                                align: Tk.Flex.Center
                                gap: Tk.Theme.space.sm
                                Tk.Label {
                                    text: buildCard.modelData.displayName
                                    font.weight: Font.DemiBold
                                }
                                Tk.Badge {
                                    text: page.statusText(buildCard.modelData.status,
                                                          buildCard.modelData.pinnable)
                                    variant: page.statusVariant(buildCard.modelData.status,
                                                                buildCard.modelData.pinnable)
                                }
                                Tk.Badge {
                                    visible: buildCard.modelData.isDefault
                                    text: qsTr("Default")
                                    variant: "accent"
                                }
                            }
                            Tk.Caption {
                                text: {
                                    const where = buildCard.modelData.origin === "system"
                                        ? qsTr("installed by the system")
                                        : buildCard.modelData.origin === "steam"
                                            ? qsTr("from Steam") : qsTr("added by you")
                                    const used = buildCard.modelData.usedBy === 1
                                        ? qsTr("1 game uses it")
                                        : qsTr("%1 games use it").arg(buildCard.modelData.usedBy)
                                    return buildCard.modelData.version + " · " + where + " · " + used
                                }
                                wrapMode: Text.Wrap
                                Tk.Flex.alignSelf: Tk.Flex.Stretch
                            }
                            Tk.Caption {
                                visible: buildCard.modelData.notes.length > 0
                                text: buildCard.modelData.notes
                                wrapMode: Text.Wrap
                                Tk.Flex.alignSelf: Tk.Flex.Stretch
                            }
                        }
                        Tk.Button {
                            visible: buildCard.modelData.pinnable && !buildCard.modelData.isDefault
                            enabled: !page.busy
                            text: qsTr("Make default")
                            iconName: "star"
                            small: true
                            tooltip: qsTr("Use this build for games you install from now on")
                            onClicked: page.makeDefaultRequested(buildCard.modelData.name)
                        }
                        Tk.IconButton {
                            visible: buildCard.modelData.removable
                            enabled: !page.busy && buildCard.modelData.usedBy === 0
                            iconName: "trash-2"
                            danger: true
                            tooltip: buildCard.modelData.usedBy === 0
                                     ? qsTr("Remove this build")
                                     : qsTr("Games use this build, so it cannot be removed")
                            onClicked: page.removeRequested(buildCard.modelData.name)
                        }
                    }
                }
            }

            Tk.Flex {
                direction: Tk.Flex.Row
                align: Tk.Flex.Center
                gap: Tk.Theme.space.sm
                Tk.Flex.alignSelf: Tk.Flex.Stretch
                Tk.SectionHeader {
                    title: qsTr("Add a GE-Proton build")
                    Tk.Flex.grow: 1
                }
                Tk.Button {
                    objectName: "checkReleasesButton"
                    enabled: !page.busy
                    text: qsTr("Check for builds")
                    iconName: "refresh-cw"
                    small: true
                    onClicked: page.checkReleasesRequested()
                }
            }
            Tk.Caption {
                text: qsTr("Builds added here have not been tested by QindaQt unless marked. "
                           + "Try one with a single game before making it the default.")
                wrapMode: Text.Wrap
                Tk.Flex.alignSelf: Tk.Flex.Stretch
            }
            Repeater {
                model: page.releases
                delegate: Tk.Flex {
                    id: releaseRow
                    required property var modelData
                    direction: Tk.Flex.Row
                    align: Tk.Flex.Center
                    gap: Tk.Theme.space.sm
                    Tk.Flex.alignSelf: Tk.Flex.Stretch
                    Tk.Label {
                        text: releaseRow.modelData.tag
                        Tk.Flex.grow: 1
                        Tk.Flex.basis: 0
                    }
                    Tk.Badge {
                        text: page.statusText(releaseRow.modelData.status, true)
                        variant: page.statusVariant(releaseRow.modelData.status, true)
                    }
                    Tk.Caption {
                        text: releaseRow.modelData.published + " · " + releaseRow.modelData.sizeText
                    }
                    Tk.Button {
                        enabled: !page.busy && !releaseRow.modelData.installed
                        text: releaseRow.modelData.installed ? qsTr("Installed") : qsTr("Add")
                        iconName: releaseRow.modelData.installed ? "circle-check" : "download"
                        small: true
                        onClicked: page.installReleaseRequested(releaseRow.modelData.toolName)
                    }
                }
            }
        }
    }
}
