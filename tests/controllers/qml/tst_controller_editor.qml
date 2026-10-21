// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as T
import QtTest
import "../../../src/apps/settings/input/qml" as Input

TestCase {
    id: test
    name: "ControllerEditor"
    width: 1000; height: 900
    visible: true
    when: windowShown
    property var editor: null
    Component { id: combinationComponent; Input.InputCombinationEditor { width: 580; bindingModel: testControllers; initialBinding: ({modifiers:["Meta"], keys:[], mouseButtons:["left"]}) } }
    Component { id: bindingComponent; Input.ControllerBindingEditor { controllerModel: testControllers } }
    Component { id: pageComponent; Input.ControllerPageEditor { controllerModel: testControllers; creating: true } }
    function cleanup() { if (editor !== null) { editor.destroy(); editor = null } }
    function test_01_metaClickCanAddAnyKeyAndSecondMouseButton() {
        editor = createTemporaryObject(combinationComponent, test)
        verify(editor !== null)
        compare(editor.value.modifiers.join(), "Meta")
        compare(editor.value.mouseButtons.join(), "left")
        editor.addKey(Qt.Key_Tab, Qt.ControlModifier)
        compare(editor.value.keys.join(), "Tab")
        verify(editor.value.modifiers.includes("Meta"))
        verify(editor.value.modifiers.includes("Ctrl"))
        editor.mouseButtons = ["left", "right"]
        compare(testControllers.bindingError(editor.value), "")
        editor.addKey(Qt.Key_Escape, 0)
        verify(editor.value.keys.includes("Esc"))
    }
    function test_02_bindingDialogSavesTextAndRetainsControllerPageAuthority() {
        editor = createTemporaryObject(bindingComponent, test, {binding:{id:"south", label:"Cross", action:"text", text:"Hello\n世界 🌎"}, controllerId:testControllers.selectedId, pageId:testControllers.currentPage.id})
        verify(editor !== null); editor.open(); waitForRendering(editor.contentItem)
        const save = findChild(editor, "controllerBindingSave")
        verify(save !== null); verify(save.enabled)
        save.clicked()
        tryCompare(testControllers, "busy", false)
        compare(testControllers.currentPage.bindings.south.text, "Hello\n世界 🌎")
        compare(testControllers.currentPage.bindings.south.action, "text")
    }
    function test_03_newProfileColorAndSwitchButtonReachAuthoritativeReadback() {
        editor = createTemporaryObject(pageComponent, test, {controllerId:testControllers.selectedId, pageId:testControllers.currentPage.id, initialName:"Work", initialColor:"#A83DFF"})
        verify(editor !== null); editor.open()
        const save = findChild(editor, "controllerProfileSave")
        verify(save.enabled); save.clicked()
        tryCompare(testControllers, "busy", false)
        compare(testControllers.pages.length, 3)
        compare(testControllers.currentPage.name, "Work")
        compare(testControllers.currentPage.color, "#A83DFF")
        verify(testControllers.setPageSwitcher("misc1")); tryCompare(testControllers, "busy", false)
        compare(testControllers.pageSwitcher, "misc1")
        for (const page of testControllers.pages) compare(page.bindings.misc1.action, "next-page")
    }
    function test_04_staleEditorCannotWriteAnotherPage() {
        editor = createTemporaryObject(bindingComponent, test, {binding:{id:"righttrigger", label:"R2", action:"combination", modifiers:["Meta"], keys:[], mouseButtons:["left"]}, controllerId:testControllers.selectedId, pageId:"wrong-page"})
        verify(editor !== null); editor.open()
        verify(!findChild(editor,"controllerBindingSave").enabled)
    }
}
