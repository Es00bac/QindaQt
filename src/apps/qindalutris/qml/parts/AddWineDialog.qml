// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QindaTK as Tk

// Hand-adding a Windows game: a title, the executable, a prefix, and what
// runs it. The controller validates on submit; the dialog only collects.
Tk.Dialog {
    id: dialog

    property var protonChoices: []
    signal addRequested(var values)

    title: qsTr("Add Windows game")
    subtitle: qsTr("An executable run through Wine or Proton")
    primaryText: qsTr("Add game")
    secondaryText: qsTr("Cancel")

    readonly property var protonModel: {
        let entries = [{ name: qsTr("Any discovered Proton"), path: "" }]
        for (const proton of dialog.protonChoices) {
            entries.push(proton)
        }
        return entries
    }

    function resetFields() {
        titleField.text = ""
        exeField.text = ""
        prefixField.text = ""
        runnerCombo.currentIndex = 0
        protonCombo.currentIndex = 0
    }

    onOpened: resetFields()
    onAccepted: {
        dialog.addRequested({
            title: titleField.text,
            executable: exeField.text,
            prefix: prefixField.text,
            runner: runnerCombo.currentIndex === 1 ? "proton" : "wine",
            proton: dialog.protonModel[protonCombo.currentIndex].path,
        })
    }

    Tk.Flex {
        direction: Tk.Flex.Column
        gap: Tk.Theme.space.sm
        width: dialog.dialogWidth - Tk.Theme.space.lg * 2

        Tk.Caption { text: qsTr("Title") }
        Tk.TextField {
            id: titleField
            objectName: "addWineTitle"
            placeholderText: qsTr("The name in your library")
            Tk.Flex.alignSelf: Tk.Flex.Stretch
        }
        Tk.Caption { text: qsTr("Executable") }
        Tk.TextField {
            id: exeField
            objectName: "addWineExecutable"
            mono: true
            placeholderText: "/games/SomeGame/game.exe"
            tooltip: qsTr("The full path of the .exe to start")
            Tk.Flex.alignSelf: Tk.Flex.Stretch
        }
        Tk.Caption { text: qsTr("Wine prefix") }
        Tk.TextField {
            id: prefixField
            objectName: "addWinePrefix"
            mono: true
            placeholderText: qsTr("~/.wine")
            tooltip: qsTr("The prefix directory; required for Proton")
            Tk.Flex.alignSelf: Tk.Flex.Stretch
        }
        Tk.Caption { text: qsTr("Runner") }
        Tk.ComboBox {
            id: runnerCombo
            objectName: "addWineRunner"
            model: [{ key: "wine", label: qsTr("Wine") },
                    { key: "proton", label: qsTr("Proton") }]
            textRole: "label"
            Tk.Flex.alignSelf: Tk.Flex.Stretch
        }
        Tk.Caption {
            visible: runnerCombo.currentIndex === 1
            text: qsTr("Proton version")
        }
        Tk.ComboBox {
            id: protonCombo
            objectName: "addWineProton"
            visible: runnerCombo.currentIndex === 1
            model: dialog.protonModel
            textRole: "name"
            Tk.Flex.alignSelf: Tk.Flex.Stretch
        }
    }
}
