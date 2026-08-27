// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtTest
import QindaQt.Tokens 1.0

Item {
    width: 64
    height: 64

    TestCase {
        name: "InstalledTokensModule"
        when: windowShown

        function test_initialReadOnlyBoundary() {
            compare(Tokens.qstRevision, 1)
            compare(Tokens.ready, false)
            compare(Tokens.generation, 0)
            compare(Tokens.sourceThemeId, "")
            compare(Object.keys(Tokens.bg).length, 0)
        }
    }
}
