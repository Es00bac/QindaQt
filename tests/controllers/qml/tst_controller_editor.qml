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
    Component { id:pageRouteComponent; Input.InputPage { width:1000; height:900; initialDestination:"controllers"; inputSettings:({controllers:testControllers, macros:testMacros}) } }
    Component { id:macroComponent; Input.InputMacroEditor { macroModel:testMacros } }
    Component { id:stepComponent; Input.MacroStepEditor { macroModel:testMacros } }
    function initTestCase() { failOnWarning(/.*/) }
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
        const source = testControllers.buttons.find(button => button.id === "south")
        editor = createTemporaryObject(bindingComponent, test, {binding:source, controllerId:testControllers.selectedId, pageId:testControllers.currentPage.id})
        verify(editor !== null); editor.open(); editor.action = "text"
        findChild(editor,"controllerTextBlock").text = "Hello\n世界 🌎"
        waitForRendering(editor.contentItem)
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
        compare(testControllers.pages.length, 5)
        compare(testControllers.currentPage.name, "Work")
        compare(testControllers.currentPage.color, "#A83DFF")
        compare(testControllers.pageSwitcher, "back")
        verify(testControllers.setPageSwitcher("misc1")); tryCompare(testControllers, "busy", false)
        compare(testControllers.pageSwitcher, "misc1")
        for (const page of testControllers.pages) {
            compare(page.bindings.misc1.action, "next-page")
            compare(page.bindings.back.action, "none")
        }
    }
    function test_04_staleEditorCannotWriteAnotherPage() {
        editor = createTemporaryObject(bindingComponent, test, {binding:{id:"righttrigger", label:"R2", action:"combination", modifiers:["Meta"], keys:[], mouseButtons:["left"]}, controllerId:testControllers.selectedId, pageId:"wrong-page"})
        verify(editor !== null); editor.open()
        verify(!findChild(editor,"controllerBindingSave").enabled)
    }    function test_05_macroEditorSavesReusableSequenceAndStepEditorAcceptsHolds() {
        const value = testMacros.create("copy-paste")
        editor = createTemporaryObject(macroComponent, test, {initialMacro:value, expected:""})
        verify(editor !== null); editor.open()
        compare(editor.steps.length, 3)
        editor.steps = [{kind:"text", binding:{action:"text", text:"A line\nUnicode 世界 🌎"}}]
        if (testImageDir.length > 0) { wait(80); grabImage(test.Window.window.contentItem).save(testImageDir + "/macro-editor.png") }
        const save = findChild(editor,"inputMacroSave")
        verify(save !== null); verify(save.enabled); save.clicked()
        tryCompare(testMacros,"busy",false)
        compare(testMacros.macros.length,1)
        compare(testMacros.macros[0].steps[0].binding.text,"A line\nUnicode 世界 🌎")
    }
    function test_06_stepEditorReusesTheCombinationEditor() {
        editor = createTemporaryObject(stepComponent,test,{initialStep:{kind:"press",slot:0,binding:{action:"combination",modifiers:["Meta"],keys:[],mouseButtons:["left"]}}})
        verify(editor !== null); editor.open()
        compare(editor.value.kind,"press")
        compare(editor.value.binding.modifiers.join(),"Meta")
        verify(findChild(editor,"macroStepSave").enabled)
        if (testImageDir.length > 0) { wait(80); grabImage(test.Window.window.contentItem).save(testImageDir + "/macro-step.png") }
    }
    function test_07_inputRouteNavigatesBetweenControllersAndMacros() {
        editor = createTemporaryObject(pageRouteComponent, test)
        verify(editor !== null)
        tryVerify(() => findChild(editor,"controllerActiveProfile") !== null)
        verify(findChild(editor,"controllerActiveProfile").visible)
        editor.selectDestination("macros")
        tryVerify(() => findChild(editor,"inputMacroNew") !== null)
        verify(findChild(editor,"inputMacroNew").visible)
    }
    function test_08_unavailableControllersAndMacrosStillConstructWithoutWarnings() {
        testBackend.setOnline(false)
        editor = createTemporaryObject(pageRouteComponent,test)
        verify(editor !== null)
        verify(!testControllers.available)
        editor.selectDestination("macros")
        wait(20)
        verify(!testMacros.available)
        testBackend.setOnline(true)
        tryVerify(() => testControllers.available && testMacros.available)
    }

}
