// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QindaQt.Controls 1.0

// One coordinate editor whose uncommitted text never becomes display truth by
// accident. Return/Enter is the explicit commit boundary; focus loss discards.
TextField {
    id: control

    required property string outputId
    required property int authoritativeValue
    required property string coordinateName

    property string sessionOutputId: ""
    property bool userDirty: false

    signal validCommitRequested(string originOutputId, int value)

    function resynchronize() {
        // AGENT-GUARD: Clear dirtiness before writing text. A selection or
        // snapshot signal may arrive reentrantly while focus is moving; a late
        // editingFinished must then be a discard, never a model mutation.
        userDirty = false
        sessionOutputId = outputId
        text = authoritativeValue.toString()
    }

    function beginSession() {
        sessionOutputId = outputId
        userDirty = false
        text = authoritativeValue.toString()
    }

    function commitUserEdit() {
        const origin = sessionOutputId
        const dirty = userDirty
        const candidateText = text.trim()
        const candidate = Number(candidateText)
        const validInteger = /^-?\d+$/.test(candidateText)
                && Number.isSafeInteger(candidate)
                && candidate >= -2147483648
                && candidate <= 2147483647

        // AGENT-CONTRACT: Only explicit Return/Enter from a still-current edit
        // session may cross into DisplaySettingsModel. Blur, output changes,
        // invalid text, and externally refreshed truth are discard paths.
        userDirty = false
        if (!dirty || !validInteger || origin.length === 0
                || origin !== outputId) {
            resynchronize()
            return
        }
        if (candidate === authoritativeValue) {
            resynchronize()
            return
        }

        validCommitRequested(origin, candidate)
        Qt.callLater(resynchronize)
    }

    text: control.authoritativeValue.toString()
    accessibleName: coordinateName
    inputMethodHints: Qt.ImhFormattedNumbersOnly

    Binding {
        target: control
        property: "text"
        value: control.authoritativeValue.toString()
        when: !control.activeFocus
    }

    onActiveFocusChanged: {
        if (activeFocus) {
            beginSession()
        }
    }
    onTextEdited: {
        if (sessionOutputId !== outputId) {
            sessionOutputId = outputId
        }
        userDirty = true
    }
    onEditingFinished: resynchronize()
    Keys.onReturnPressed: event => {
        commitUserEdit()
        event.accepted = true
    }
    Keys.onEnterPressed: event => {
        commitUserEdit()
        event.accepted = true
    }

    onOutputIdChanged: {
        resynchronize()
        Qt.callLater(resynchronize)
    }
    onAuthoritativeValueChanged: resynchronize()
}
