// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtTest
import "../../../src/shell/qml" as ShellComponents

Item {
    id: testRoot
    width: 480
    height: 240

    property var theme: ({
        "cornerRadius": 8,
        "colors": {
            "surface": "#222624",
            "surfaceRaised": "#2c312e",
            "text": "#f2f1eb",
            "accent": "#8fc8b7"
        }
    })

    QtObject {
        id: fakeTaskListAccess
        property string phaseText: "ready"
    }

    function taskListApplet(ready) {
        return {
            "id": "tasks",
            "plugin": "task-list",
            "settings": { "zone": "center" },
            "runtime": {
                "ready": ready,
                "entryPoint": "qindaqt.applets.task-list"
            }
        }
    }

    Component {
        id: rowComponent
        ShellComponents.PanelAppletRow {
            width: 280
            height: 40
            panel: ({ "applets": [testRoot.taskListApplet(true)] })
            theme: testRoot.theme
            zone: "center"
            liveApplets: true
            taskListAppletAccess: fakeTaskListAccess
        }
    }

    Component {
        id: columnComponent
        ShellComponents.PanelAppletColumn {
            width: 52
            height: 180
            panel: ({ "applets": [testRoot.taskListApplet(true)] })
            theme: testRoot.theme
            zone: "center"
            liveApplets: true
            taskListAppletAccess: fakeTaskListAccess
        }
    }

    Component {
        id: previewChipComponent
        ShellComponents.AppletChip {
            width: 120
            height: 40
            applet: ({
                "id": "tasks",
                "plugin": "task-list",
                "settings": {}
            })
            theme: testRoot.theme
            liveApplets: false
        }
    }

    TestCase {
        name: "TaskListPanelDispatcher"
        when: windowShown

        function test_horizontalProductionRowCarriesOnlyTaskListFacade() {
            const row = createTemporaryObject(rowComponent, testRoot)
            verify(row !== null)
            const taskList = findChild(row, "taskListApplet")
            verify(taskList !== null)
            verify(taskList.visible)
            verify(!taskList.vertical)
            compare(taskList.access, fakeTaskListAccess)
            verify(taskList.enabled)
        }

        function test_verticalProductionColumnHasKeyboardCapableContent() {
            const column = createTemporaryObject(columnComponent, testRoot)
            verify(column !== null)
            const taskList = findChild(column, "taskListApplet")
            verify(taskList !== null)
            verify(taskList.visible)
            verify(taskList.vertical)
            compare(taskList.access, fakeTaskListAccess)
            verify(taskList.enabled)
        }

        function test_previewRendersCompiledDisabledFallback() {
            const chip = createTemporaryObject(previewChipComponent, testRoot)
            verify(chip !== null)
            verify(chip.usesLiveContent)
            const taskList = findChild(chip, "taskListApplet")
            verify(taskList !== null)
            verify(taskList.visible)
            compare(taskList.access, null)
            verify(!taskList.enabled)
        }
    }
}
