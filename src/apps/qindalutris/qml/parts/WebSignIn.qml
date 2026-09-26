// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtWebEngine
import QindaQt.QindaLutris

// The store's own sign-in page, embedded (ADR-0275 section 8). Built only
// when QtWebEngine is available; StoreAccountCard falls back to the user's
// browser plus a paste field otherwise. QtWebEngine's default profile is
// off the record, so the store's cookies end with the page. The page never
// sees anything but the store; `finished` carries the address (GOG,
// Amazon) or the page text (Epic's code arrives as JSON in the page).
Item {
    id: root

    property string storeId: ""
    property url startUrl: ""
    property bool done: false

    signal finished(string text)

    WebEngineView {
        id: view
        objectName: "signInView"
        anchors.fill: parent
        url: root.startUrl

        function check() {
            if (root.done || !Accounts.isSignInFinishedUrl(root.storeId, view.url)) {
                return
            }
            if (root.storeId === "egs") {
                if (view.loading) {
                    return // the code is in the page body; wait for it
                }
                root.done = true
                view.runJavaScript("document.body.innerText", function(text) {
                    root.finished(text)
                })
                return
            }
            root.done = true
            root.finished(view.url.toString())
        }

        onUrlChanged: check()
        onLoadingChanged: check()
        // "Sign in with Google/Apple/..." opens a window; keep it here.
        onNewWindowRequested: function(request) { view.url = request.requestedUrl }
    }
}
