// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0 as QQ
import QindaQt.Tokens 1.0

T.ApplicationWindow {
    id: root
    visible: true
    width: 900
    height: 640
    minimumWidth: 640
    minimumHeight: 480
    title: qsTr("Welcome to QindaQt")
    color: Tokens.bg.base

    property int chapter: 0
    property string launchNotice: ""
    readonly property bool wideNavigation: width >= 760
    readonly property var chapters: [
        {
            nav: qsTr("Welcome"), eyebrow: qsTr("Welcome to QindaQt"),
            title: qsTr("A desktop that fits you."),
            summary: qsTr("QindaQt starts familiar and grows with the way you work. Windows can stay independent, or become organized groups when a task needs more structure."),
            diagram: "desktop", hero: welcomeHeroSource,
            cards: [
                { marker: "1", title: qsTr("Begin with the familiar"), body: qsTr("Open apps from the launcher, keep favorites and running apps in the dock, and move, resize, minimize, or maximize windows normally.") },
                { marker: "2", title: qsTr("Organize only when it helps"), body: qsTr("A deliberate Meta+Shift drag turns related windows into split views or desktop tab pages. Nothing forces every window into a layout.") }
            ]
        },
        {
            nav: qsTr("Desktop"), eyebrow: qsTr("Everyday essentials"),
            title: qsTr("Find things without breaking your flow"),
            summary: qsTr("The desktop keeps common actions close while leaving your work in the center."),
            diagram: "desktop", hero: "",
            cards: [
                { marker: "⌘", title: qsTr("Launcher and dock"), body: qsTr("Use the launcher to search installed applications. Pin favorites to the dock; a running indicator shows what is already open. Click a running app to return to it.") },
                { marker: "☰", title: qsTr("Global application menu"), body: qsTr("When a supported app is active, its File, Edit, View, and other menus appear in the desktop panel. If an app does not export a menu, its own menu remains available.") },
                { marker: "↔", title: qsTr("Ordinary windows stay ordinary"), body: qsTr("Drag a normal title bar to move a floating window. Resize from its edges; use its title controls to minimize, maximize, restore, or close it.") }
            ]
        },
        {
            nav: qsTr("Arrange"), eyebrow: qsTr("A gesture with intent"),
            title: qsTr("Split at an edge. Make a page at the center."),
            summary: qsTr("Hold Meta (the Super or Windows key) and Shift, then left-drag a window. Arrangement targets appear after you begin moving."),
            diagram: "dock", hero: "",
            shortcuts: [qsTr("Meta + Shift + left drag"), qsTr("Escape to cancel")],
            cards: [
                { marker: "↔", title: qsTr("Drop on an edge for a split"), body: qsTr("An edge target places the dragged window beside the target. Splits can be horizontal or vertical, and a later split can sit inside an existing one. Drag the divider between windows to give either side more room.") },
                { marker: "＋", title: qsTr("Drop in the center or tab strip for a page"), body: qsTr("The dragged window becomes another top-level desktop tab page in the group. Each QindaQt document or terminal session is a window; the desktop provides its tabs and splits.") },
                { marker: "Esc", title: qsTr("Change your mind safely"), body: qsTr("Press Escape during the arrangement to cancel it. Drop an independent window outside every valid target to leave it independent; a grouped member dropped there leaves its group and becomes independent.") }
            ]
        },
        {
            nav: qsTr("Groups"), eyebrow: qsTr("Containers and pages"),
            title: qsTr("Keep a whole task together"),
            summary: qsTr("A group is one movable desktop container. Its top tabs select pages; each page can hold one window or a nested arrangement of split windows."),
            diagram: "container", hero: "",
            shortcuts: [qsTr("Meta + Ctrl + PageDown / PageUp"), qsTr("Meta + Ctrl + Shift + PageDown / PageUp")],
            cards: [
                { marker: "1", title: qsTr("Desktop pages are above app content"), body: qsTr("A desktop page can hold a browser, an editor, or an entire split. QindaQt apps open documents and sessions in ordinary windows. Group them here to build your workspace.") },
                { marker: "2", title: qsTr("Switch and reorder pages"), body: qsTr("Click a top-level tab to activate its page. Use Meta+Ctrl+PageDown or PageUp to move between pages; add Shift to reorder the active page.") },
                { marker: "3", title: qsTr("Nested splits stay part of one group"), body: qsTr("A page may split again inside either side. The group still moves, switches, and appears as one task while each member keeps its own content and focus.") }
            ]
        },
        {
            nav: qsTr("Move & detach"), eyebrow: qsTr("Know what the title bar moves"),
            title: qsTr("Move the group, or take one window with you"),
            summary: qsTr("The outer container title belongs to the whole group. A member's preserved title belongs to that one window."),
            diagram: "move", hero: "",
            shortcuts: [qsTr("Enter to commit a keyboard move"), qsTr("Escape to cancel")],
            cards: [
                { marker: "▣", title: qsTr("Move the complete group"), body: qsTr("Drag the outer title to move every page and split together. Its menu also contains group-level choices such as workspace, activity, layer, pinning, and output.") },
                { marker: "□", title: qsTr("Detach one member"), body: qsTr("Plain-drag a grouped member's own title bar. It leaves the group and follows your pointer as an independent window. A Meta+Shift drag can arrange it somewhere else.") },
                { marker: "↕", title: qsTr("Reorder or move a page"), body: qsTr("Drag top-level tabs to reorder them, move a page to another container, or detach it. Edge tab drops are not a split shortcut; use a window arrangement gesture for a split.") }
            ]
        },
        {
            nav: qsTr("Customize"), eyebrow: qsTr("Start broad, then refine"),
            title: qsTr("Choose a preset, then make it yours"),
            summary: qsTr("Presets give you a complete desktop layout. Customize lets you adjust panels and applets directly, with a draft you can apply or discard."),
            diagram: "customize", hero: "",
            shortcuts: [qsTr("Ctrl + Return — Apply"), qsTr("Ctrl + Shift + Return — Discard"), qsTr("Ctrl + Z — Undo")],
            cards: [
                { marker: "1", title: qsTr("Pick a useful starting point"), body: qsTr("Choose the QindaQt layout or a familiar workflow preset. Appearance and workflow are separate, so changing a layout does not force a new color theme.") },
                { marker: "2", title: qsTr("Edit in Customize"), body: qsTr("Drag applet chips and panel items. Each completed drag is one undoable change. Press Escape or release outside a valid target to cancel only the move in progress. Press Space for keyboard move mode, then use Ctrl or Alt with the arrow keys to choose a position.") },
                { marker: "3", title: qsTr("Apply or discard the draft"), body: qsTr("Apply saves all of your draft changes as the active layout. Discard restores the last applied layout. You can also use Undo to step back through completed changes before deciding.") }
            ],
            actions: [{ label: qsTr("Open Customize"), description: qsTr("Open the Customize page in QindaQt Settings"), action: "customize", emphasized: true }]
        },
        {
            nav: qsTr("Appearance & apps"), eyebrow: qsTr("Make yourself at home"),
            title: qsTr("Set the mood, then try the desktop"),
            summary: qsTr("Appearance controls the wallpaper and light or dark look. Accessibility choices tune motion and readability. These controls update the desktop without changing your layout."),
            diagram: "appearance", hero: "",
            cards: [
                { marker: "◐", title: qsTr("Choose light, dark, or system"), body: qsTr("Pick a color scheme and theme that remain comfortable over a long session. This guide follows the same live appearance source, including high-contrast colors.") },
                { marker: "▧", title: qsTr("Choose a wallpaper and fit"), body: qsTr("Select a bundled or local wallpaper, then choose how it is scaled, centered, or tiled. Bundled choices show image previews; Apply makes the selected wallpaper and fit active.") },
                { marker: "A", title: qsTr("Tune accessibility"), body: qsTr("Adjust interface scale and fonts, reduce motion, reduce transparency, or use higher contrast. Controls keep keyboard focus visible and expose meaningful accessible names.") },
                { marker: "?", title: qsTr("Come back any time"), body: qsTr("Open “Welcome to QindaQt” from the launcher whenever you want this guide again. Unchecking Show at next launch only stops automatic opening.") }
            ],
            actions: [
                { label: qsTr("Open Appearance"), description: qsTr("Open the Appearance page in QindaQt Settings"), action: "appearance", emphasized: true },
                { label: qsTr("Open Text Editor"), description: qsTr("Launch the QindaQt Text Editor"), action: "editor" },
                { label: qsTr("Open File Manager"), description: qsTr("Launch the QindaQt File Manager"), action: "files" }
            ]
        }
    ]

    Shortcut { sequence: "Escape"; onActivated: root.close() }
    Shortcut { sequence: "Alt+Left"; enabled: root.chapter > 0; onActivated: root.chapter-- }
    Shortcut { sequence: "Alt+Right"; enabled: root.chapter + 1 < root.chapters.length; onActivated: root.chapter++ }

    function launch(action) {
        root.launchNotice = ""
        if (!welcomeActions.launch(action))
            root.launchNotice = qsTr("That application could not be opened. You can still find it in the launcher.")
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            visible: root.wideNavigation
            Layout.preferredWidth: 224
            Layout.fillHeight: true
            color: Tokens.bg.raised
            border.width: 1
            border.color: Tokens.outline.divider

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Tokens.space["5"]
                spacing: Tokens.space["3"]
                Text { text: qsTr("QindaQt"); color: Tokens.fg.default; font.family: Tokens.type.fontFamily; font.pointSize: Tokens.type.headline; font.weight: Font.DemiBold }
                Text { text: qsTr("Getting started"); color: Tokens.fg.default; font.family: Tokens.type.fontFamily; font.pointSize: Tokens.type.body }
                Item { Layout.preferredHeight: Tokens.space["2"] }
                Repeater {
                    model: root.chapters
                    QQ.Button {
                        required property var modelData
                        required property int index
                        Layout.fillWidth: true
                        text: qsTr("%1.  %2").arg(index + 1).arg(modelData.nav)
                        emphasized: root.chapter === index
                        accessibleDescription: qsTr("Chapter %1 of %2").arg(index + 1).arg(root.chapters.length)
                        onClicked: root.chapter = index
                    }
                }
                Item { Layout.fillHeight: true }
                Text { text: qsTr("%1 of %2").arg(root.chapter + 1).arg(root.chapters.length); color: Tokens.fg.default; font.family: Tokens.type.fontFamily; font.pointSize: Tokens.type.caption }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0
            Rectangle {
                visible: !root.wideNavigation
                Layout.fillWidth: true
                Layout.preferredHeight: 58
                color: Tokens.bg.raised
                border.width: 1
                border.color: Tokens.outline.divider
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Tokens.space["5"]
                    anchors.rightMargin: Tokens.space["5"]
                    Text { text: qsTr("QindaQt"); color: Tokens.fg.default; font.family: Tokens.type.fontFamily; font.pointSize: Tokens.type.title; font.weight: Font.DemiBold }
                    Item { Layout.fillWidth: true }
                    Text { text: qsTr("%1 / %2 · %3").arg(root.chapter + 1).arg(root.chapters.length).arg(root.chapters[root.chapter].nav); color: Tokens.fg.default; font.family: Tokens.type.fontFamily; font.pointSize: Tokens.type.body }
                }
            }

            T.ScrollView {
                id: scroller
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: availableWidth
                TutorialPage {
                    x: Tokens.space["6"]
                    width: Math.max(1, scroller.availableWidth - Tokens.space["6"] * 2)
                    eyebrow: root.chapters[root.chapter].eyebrow
                    title: root.chapters[root.chapter].title
                    summary: root.chapters[root.chapter].summary
                    diagramKind: root.chapters[root.chapter].diagram
                    cards: root.chapters[root.chapter].cards
                    shortcuts: root.chapters[root.chapter].shortcuts || []
                    actions: root.chapters[root.chapter].actions || []
                    heroSource: root.chapters[root.chapter].hero || ""
                    actionHandler: root.launch
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: footer.implicitHeight + Tokens.space["3"] * 2
                color: Tokens.bg.raised
                border.width: 1
                border.color: Tokens.outline.divider
                RowLayout {
                    id: footer
                    anchors.left: parent.left; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: Tokens.space["5"]; anchors.rightMargin: Tokens.space["5"]
                    spacing: Tokens.space["3"]
                    QQ.CheckBox {
                        objectName: "showAtNextLaunch"
                        Layout.fillWidth: true
                        text: qsTr("Show at next launch")
                        checked: welcomePreferences.showAtNextLaunch
                        accessibleDescription: qsTr("When checked, this guide opens automatically with your next desktop session")
                        onToggled: welcomePreferences.showAtNextLaunch = checked
                    }
                    QQ.Button { objectName: "previousButton"; text: qsTr("Previous"); emphasized: false; available: root.chapter > 0; onClicked: root.chapter-- }
                    QQ.Button {
                        objectName: "nextButton"
                        text: root.chapter + 1 === root.chapters.length ? qsTr("Finish") : qsTr("Next")
                        accessibleDescription: root.chapter + 1 === root.chapters.length ? qsTr("Close the welcome guide") : qsTr("Open the next chapter")
                        onClicked: root.chapter + 1 === root.chapters.length ? root.close() : root.chapter++
                    }
                }
            }
        }
    }

    T.ToolTip { visible: root.launchNotice.length > 0; text: root.launchNotice; timeout: 5000 }
}
