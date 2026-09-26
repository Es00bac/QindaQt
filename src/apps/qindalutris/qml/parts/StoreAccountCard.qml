// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QindaTK as Tk

// One store account (ADR-0275 section 8): sign in on the store's own page,
// then the games you own there, each one button from installed. Values in,
// signals out; the page owns which card is signing in.
Tk.Card {
    id: card

    // An Accounts.accounts row: id, name, available, signedIn, userName,
    // busy, message, gameCount.
    property var account: ({})
    // Accounts.ownedGames rows: id, title, imageUrl, installedTitleId.
    property var games: []
    property bool installBusy: false
    property bool webSignInAvailable: false
    // Set while this card's sign-in is open.
    property url signInUrl: ""
    readonly property bool signingIn: signInUrl.toString().length > 0

    signal signInRequested()
    signal signInCancelled()
    signal signInFinished(string text)
    signal signOutRequested()
    signal refreshRequested()
    signal installRequested(string gameId, string title)
    signal openTitleRequested(string titleId)
    signal openExternally(url address)

    readonly property int shownLimit: 50
    property string filter: ""
    readonly property var matching: card.games.filter(game =>
        card.filter.length === 0 || game.title.toLowerCase().indexOf(card.filter.toLowerCase()) >= 0)

    readonly property string pasteHint: card.account.id === "egs"
        ? qsTr("After you sign in, Epic shows a page of text. Select all of it, copy it, and paste it here.")
        : card.account.id === "gog"
          ? qsTr("After you sign in, the page goes blank. Copy the address from your browser's address bar and paste it here.")
          : qsTr("After you sign in, Amazon shows its home page. Copy the address from your browser's address bar and paste it here.")

    Tk.Flex {
        direction: Tk.Flex.Column
        gap: Tk.Theme.space.sm
        width: parent.width

        Tk.Flex {
            direction: Tk.Flex.Row
            align: Tk.Flex.Center
            gap: Tk.Theme.space.sm
            Tk.Flex.alignSelf: Tk.Flex.Stretch
            Tk.Label {
                text: card.account.name !== undefined ? card.account.name : ""
                font.weight: Font.DemiBold
            }
            Tk.Badge {
                objectName: "accountBadge-" + card.account.id
                visible: card.account.available === true
                text: card.account.signedIn
                      ? (card.account.userName.length > 0
                         ? qsTr("Signed in as %1").arg(card.account.userName) : qsTr("Signed in"))
                      : qsTr("Not signed in")
                variant: card.account.signedIn ? "success" : "default"
            }
            Item { Tk.Flex.grow: 1; implicitHeight: 1 }
            Tk.Button {
                objectName: "accountRefresh-" + card.account.id
                visible: card.account.signedIn === true
                enabled: !card.account.busy
                text: qsTr("Refresh")
                iconName: "refresh-cw"
                small: true
                onClicked: card.refreshRequested()
            }
            Tk.Button {
                objectName: "accountSignIn-" + card.account.id
                visible: card.account.available === true && !card.signingIn
                enabled: !card.account.busy
                text: card.account.signedIn ? qsTr("Sign out") : qsTr("Sign in")
                iconName: card.account.signedIn ? "circle-x" : "key-round"
                variant: card.account.signedIn ? "default" : "accent"
                small: true
                onClicked: card.account.signedIn ? card.signOutRequested() : card.signInRequested()
            }
        }
        Tk.Caption {
            visible: card.account.available !== true
            text: qsTr("Not available: its support package is not installed.")
            wrapMode: Text.Wrap
            Tk.Flex.alignSelf: Tk.Flex.Stretch
        }
        Tk.Caption {
            objectName: "accountMessage-" + card.account.id
            visible: text.length > 0
            text: card.account.busy ? qsTr("Working…")
                                    : (card.account.message !== undefined ? card.account.message : "")
            wrapMode: Text.Wrap
            Tk.Flex.alignSelf: Tk.Flex.Stretch
        }

        // Sign-in: the store's own page, embedded when possible.
        Loader {
            id: webSignIn
            objectName: "webSignIn-" + card.account.id
            active: card.signingIn && card.webSignInAvailable
            visible: active
            // Tk.Flex sizes children from Flex.basis or implicitHeight, never
            // `height`; a Loader's implicitHeight is its item's (0 here).
            Tk.Flex.basis: active ? 560 : 0
            Tk.Flex.alignSelf: Tk.Flex.Stretch
            source: "WebSignIn.qml"
            onLoaded: {
                item.storeId = card.account.id
                item.startUrl = card.signInUrl
                item.finished.connect(text => card.signInFinished(text))
            }
        }
        Tk.Flex {
            objectName: "pasteSignIn-" + card.account.id
            visible: card.signingIn && !card.webSignInAvailable
            direction: Tk.Flex.Column
            gap: Tk.Theme.space.sm
            Tk.Flex.alignSelf: Tk.Flex.Stretch
            Tk.Button {
                text: qsTr("Open the sign-in page")
                iconName: "external-link"
                variant: "accent"
                onClicked: card.openExternally(card.signInUrl)
            }
            Tk.Caption {
                text: card.pasteHint
                wrapMode: Text.Wrap
                Tk.Flex.alignSelf: Tk.Flex.Stretch
            }
            Tk.TextField {
                id: pasted
                objectName: "pasteField-" + card.account.id
                placeholderText: qsTr("Paste here")
                Tk.Flex.alignSelf: Tk.Flex.Stretch
            }
        }
        Tk.Flex {
            visible: card.signingIn
            direction: Tk.Flex.Row
            gap: Tk.Theme.space.sm
            Tk.Flex.alignSelf: Tk.Flex.End
            Tk.Button {
                text: qsTr("Cancel")
                small: true
                onClicked: card.signInCancelled()
            }
            Tk.Button {
                objectName: "pasteFinish-" + card.account.id
                visible: !card.webSignInAvailable
                enabled: pasted.text.trim().length > 0
                text: qsTr("Finish signing in")
                variant: "accent"
                small: true
                onClicked: {
                    card.signInFinished(pasted.text)
                    pasted.text = ""
                }
            }
        }

        // The games you own there.
        Tk.SearchField {
            objectName: "ownedSearch-" + card.account.id
            visible: card.games.length > 10
            placeholderText: qsTr("Search your %1 games").arg(card.games.length)
            Tk.Flex.alignSelf: Tk.Flex.Stretch
            onSearched: function(text) { card.filter = text }
        }
        Repeater {
            model: card.matching.slice(0, card.shownLimit)
            delegate: Tk.ListRow {
                id: gameRow
                required property var modelData
                objectName: "ownedGame-" + modelData.id
                text: modelData.title
                Tk.Flex.alignSelf: Tk.Flex.Stretch
                trailing: Tk.Button {
                    small: true
                    enabled: gameRow.modelData.installedTitleId.length > 0 || !card.installBusy
                    text: gameRow.modelData.installedTitleId.length > 0 ? qsTr("Open") : qsTr("Install")
                    iconName: gameRow.modelData.installedTitleId.length > 0 ? "gamepad-2" : "download"
                    variant: gameRow.modelData.installedTitleId.length > 0 ? "default" : "accent"
                    onClicked: gameRow.modelData.installedTitleId.length > 0
                               ? card.openTitleRequested(gameRow.modelData.installedTitleId)
                               : card.installRequested(gameRow.modelData.id, gameRow.modelData.title)
                }
            }
        }
        Tk.Caption {
            visible: card.matching.length > card.shownLimit
            text: qsTr("Showing %1 of %2. Search to find the others.")
                      .arg(card.shownLimit).arg(card.matching.length)
            Tk.Flex.alignSelf: Tk.Flex.Stretch
        }
    }
}
