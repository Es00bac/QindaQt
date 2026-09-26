// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QindaTK as Tk
import "parts" as Parts

// "Get games" (ADR-0275 sections 4-5): one card per store, one card for any
// setup file, and an honest card for what cannot work. Every flow is one
// button with visible progress and ONE plain sentence at the end. Values in,
// signals out, so the page constructs over stubs in tests.
Item {
    id: page

    // Rows from Installs.stores: id, name, notes, installed, titleId,
    // adoptable, prefixPath.
    property var stores: []
    property bool busy: false
    property real progress: 0
    property string stageText: ""
    property string resultMessage: ""
    property string resultNote: ""
    property bool succeeded: false
    property string details: ""
    // After a setup-file install: rows path, name, sizeBytes.
    property var candidates: []
    // Store accounts (Accounts): rows id, name, available, signedIn,
    // userName, busy, message, gameCount; owned games by store id.
    property var accounts: []
    property var ownedGamesByStore: ({})
    property bool webSignInAvailable: false
    // The account whose sign-in page is open, and that page.
    property string signInStore: ""
    property url signInUrl: ""

    function showSignIn(storeId, url) {
        page.signInStore = storeId
        page.signInUrl = url
    }
    function closeSignIn() {
        page.signInStore = ""
        page.signInUrl = ""
    }

    signal installStoreRequested(string recipeId)
    signal adoptStoreRequested(string recipeId, string prefixPath)
    signal openTitleRequested(string titleId)
    signal installSetupRequested(string title, string installerPath)
    signal candidateChosen(string executablePath)
    signal cancelRequested()
    signal copyDetailsRequested(string details)
    signal signInRequested(string storeId)
    signal signInFinished(string storeId, string text)
    signal signOutRequested(string storeId)
    signal refreshAccountRequested(string storeId)
    signal installOwnedRequested(string storeId, string gameId, string title)
    signal openExternallyRequested(url address)

    Tk.Scroll {
        anchors.fill: parent
        overflowX: Tk.Scroll.Hidden

        Tk.Flex {
            direction: Tk.Flex.Column
            gap: Tk.Theme.space.md
            padding: Tk.Theme.space.md
            width: page.width

            Tk.Heading {
                text: qsTr("Get games")
                level: 1
            }
            Tk.Caption {
                text: qsTr("Pick where your games come from. QindaLutris sets up the store "
                           + "for you, with a Proton build that is known to work, and keeps "
                           + "it on that build until you choose otherwise.")
                wrapMode: Text.Wrap
                Tk.Flex.alignSelf: Tk.Flex.Stretch
            }

            Parts.JobProgress {
                objectName: "getGamesJob"
                busy: page.busy
                progress: page.progress
                stageText: page.stageText
                resultMessage: page.resultMessage
                resultNote: page.resultNote
                succeeded: page.succeeded
                details: page.details
                onCancelRequested: page.cancelRequested()
                onCopyDetailsRequested: function(text) { page.copyDetailsRequested(text) }
                Tk.Flex.alignSelf: Tk.Flex.Stretch
            }

            // Which program is the game? Shown after a setup-file install.
            Tk.Card {
                objectName: "candidateCard"
                visible: !page.busy && page.candidates.length > 0
                Tk.Flex.alignSelf: Tk.Flex.Stretch
                Tk.Flex {
                    direction: Tk.Flex.Column
                    gap: Tk.Theme.space.sm
                    width: parent.width
                    Tk.Label {
                        text: qsTr("Which program starts the game?")
                        font.weight: Font.DemiBold
                    }
                    Repeater {
                        model: page.candidates
                        delegate: Tk.Flex {
                            required property var modelData
                            direction: Tk.Flex.Row
                            align: Tk.Flex.Center
                            gap: Tk.Theme.space.sm
                            Tk.Flex.alignSelf: Tk.Flex.Stretch
                            Tk.Label {
                                text: modelData.name
                                elide: Text.ElideMiddle
                                Tk.Flex.grow: 1
                                Tk.Flex.basis: 0
                            }
                            Tk.Button {
                                text: qsTr("This one")
                                variant: "accent"
                                small: true
                                tooltip: modelData.path
                                onClicked: page.candidateChosen(modelData.path)
                            }
                        }
                    }
                }
            }

            Tk.SectionHeader {
                title: qsTr("Your store accounts")
                Tk.Flex.alignSelf: Tk.Flex.Stretch
            }
            Tk.Caption {
                text: qsTr("Sign in to Epic, GOG or Amazon once, then install any game you own "
                           + "with one button. You sign in on the store's own page; QindaLutris "
                           + "never sees your password.")
                wrapMode: Text.Wrap
                Tk.Flex.alignSelf: Tk.Flex.Stretch
            }
            // AGENT-GUARD: the model is the COUNT, not the array. Accounts
            // publishes a new array on every step of every store; an array
            // model would rebuild each card -- and a sign-in page the user is
            // halfway through -- whenever another store finished a refresh.
            Repeater {
                model: page.accounts.length
                delegate: Parts.StoreAccountCard {
                    id: accountCard
                    required property int index
                    readonly property string storeId: account.id !== undefined ? account.id : ""
                    objectName: "accountCard-" + storeId
                    account: page.accounts[index] !== undefined ? page.accounts[index] : ({})
                    games: page.ownedGamesByStore[storeId] !== undefined
                           ? page.ownedGamesByStore[storeId] : []
                    installBusy: page.busy
                    webSignInAvailable: page.webSignInAvailable
                    signInUrl: page.signInStore === storeId ? page.signInUrl : ""
                    Tk.Flex.alignSelf: Tk.Flex.Stretch
                    onSignInRequested: page.signInRequested(accountCard.storeId)
                    onSignInCancelled: page.closeSignIn()
                    onSignInFinished: function(text) {
                        page.closeSignIn()
                        page.signInFinished(accountCard.storeId, text)
                    }
                    onSignOutRequested: page.signOutRequested(accountCard.storeId)
                    onRefreshRequested: page.refreshAccountRequested(accountCard.storeId)
                    onInstallRequested: function(gameId, title) {
                        page.installOwnedRequested(accountCard.storeId, gameId, title)
                    }
                    onOpenTitleRequested: function(titleId) { page.openTitleRequested(titleId) }
                    onOpenExternally: function(address) { page.openExternallyRequested(address) }
                }
            }

            Tk.SectionHeader {
                title: qsTr("Store apps")
                Tk.Flex.alignSelf: Tk.Flex.Stretch
            }
            Repeater {
                model: page.stores
                delegate: Tk.Card {
                    id: storeCard
                    required property var modelData
                    objectName: "storeCard-" + modelData.id
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
                                    text: storeCard.modelData.name
                                    font.weight: Font.DemiBold
                                }
                                Tk.Badge {
                                    visible: storeCard.modelData.installed
                                    text: qsTr("Installed")
                                    variant: "success"
                                }
                                Tk.Badge {
                                    visible: storeCard.modelData.adoptable
                                    text: qsTr("Found on this computer")
                                    variant: "info"
                                }
                            }
                            Tk.Caption {
                                visible: storeCard.modelData.notes.length > 0
                                text: storeCard.modelData.notes
                                wrapMode: Text.Wrap
                                Tk.Flex.alignSelf: Tk.Flex.Stretch
                            }
                        }
                        Tk.Button {
                            objectName: "storeAction-" + storeCard.modelData.id
                            enabled: !page.busy
                            text: storeCard.modelData.installed ? qsTr("Open")
                                : storeCard.modelData.adoptable ? qsTr("Add to library")
                                : qsTr("Install")
                            iconName: storeCard.modelData.installed ? "gamepad-2"
                                    : storeCard.modelData.adoptable ? "plus" : "download"
                            variant: storeCard.modelData.installed ? "default" : "accent"
                            tooltip: storeCard.modelData.adoptable
                                     ? qsTr("Use the copy already installed in %1")
                                           .arg(storeCard.modelData.prefixPath)
                                     : ""
                            onClicked: {
                                if (storeCard.modelData.installed) {
                                    page.openTitleRequested(storeCard.modelData.titleId)
                                } else if (storeCard.modelData.adoptable) {
                                    page.adoptStoreRequested(storeCard.modelData.id,
                                                             storeCard.modelData.prefixPath)
                                } else {
                                    page.installStoreRequested(storeCard.modelData.id)
                                }
                            }
                        }
                    }
                }
            }

            Tk.SectionHeader {
                title: qsTr("From a setup file")
                Tk.Flex.alignSelf: Tk.Flex.Stretch
            }
            Tk.Card {
                objectName: "setupFileCard"
                Tk.Flex.alignSelf: Tk.Flex.Stretch
                Tk.Flex {
                    direction: Tk.Flex.Column
                    gap: Tk.Theme.space.sm
                    width: parent.width
                    Tk.Caption {
                        text: qsTr("A game you downloaded yourself (for example from GOG): "
                                   + "give it a name and choose its setup file.")
                        wrapMode: Text.Wrap
                        Tk.Flex.alignSelf: Tk.Flex.Stretch
                    }
                    Tk.TextField {
                        id: setupTitle
                        objectName: "setupTitleField"
                        placeholderText: qsTr("Game name")
                        Tk.Flex.alignSelf: Tk.Flex.Stretch
                    }
                    Tk.TextField {
                        id: setupPath
                        objectName: "setupPathField"
                        mono: true
                        placeholderText: qsTr("/home/you/Downloads/setup_game.exe")
                        tooltip: qsTr("The full path of the .exe or .msi installer")
                        Tk.Flex.alignSelf: Tk.Flex.Stretch
                    }
                    Tk.Button {
                        objectName: "setupInstallButton"
                        enabled: !page.busy && setupTitle.text.trim().length > 0
                                 && setupPath.text.trim().length > 0
                        text: qsTr("Install")
                        iconName: "download"
                        variant: "accent"
                        Tk.Flex.alignSelf: Tk.Flex.End
                        onClicked: page.installSetupRequested(setupTitle.text.trim(),
                                                              setupPath.text.trim())
                    }
                }
            }

            Tk.SectionHeader {
                title: qsTr("Not supported")
                Tk.Flex.alignSelf: Tk.Flex.Stretch
            }
            Tk.Notice {
                objectName: "microsoftStoreNotice"
                variant: "info"
                title: qsTr("Microsoft Store and Game Pass for PC")
                text: qsTr("These games are locked to Windows and cannot run on Linux. Many "
                           + "are also sold on Steam or GOG, which work here. Xbox Cloud Gaming "
                           + "and GeForce NOW can stream them in your browser instead.")
                Tk.Flex.alignSelf: Tk.Flex.Stretch
            }
        }
    }
}
