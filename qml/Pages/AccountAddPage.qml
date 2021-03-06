import QtQuick 1.1
import com.nokia.symbian 1.1

Page {
    id: accAddPage

    tools: toolBarLayout

    Component.onCompleted: {
        if (vars.accJid != "") {
            statusBarText.text = qsTr("Editing ") + vars.accJid
            if (vars.accHost == "chat.facebook.com") {
                selectionDialog.selectedIndex = 0;
            } else {
                if (vars.accHost == "talk.google.com") selectionDialog.selectedIndex = 1; else selectionDialog.selectedIndex = 2;
            }
            tiJid.text = vars.accJid
            tiPass.text = vars.accPass
            tiHost.text = vars.accHost
            tiPort.text = vars.accPort
            tiResource.text = vars.accResource
        } else { statusBarText.text = qsTr("New account") }
    }

    Flickable {
        id: flickArea
        anchors { left: parent.left; leftMargin: 5; right: parent.right; rightMargin: 5; top: parent.top; topMargin: 5; bottom: parent.bottom; }

        contentHeight: contentPage.height
        contentWidth: contentPage.width

        flickableDirection: Flickable.VerticalFlick




        Column {
            id: contentPage
            width: accAddPage.width - flickArea.anchors.rightMargin - flickArea.anchors.leftMargin
            spacing: 5
            SelectionListItem {
                id: serverSelection
                platformInverted: main.platformInverted
                subTitle: selectionDialog.selectedIndex >= 0
                          ? selectionDialog.model.get(selectionDialog.selectedIndex).name
                          : "FB Chat, GTalk or manual"
                anchors { left: parent.left; right: parent.right }
                title: "Server"

                onClicked: selectionDialog.open()

                SelectionDialog {
                    id: selectionDialog
                    titleText: "Available options"
                    selectedIndex: -1
                    platformInverted: main.platformInverted
                    model: ListModel {
                        ListElement { name: "Facebook Chat" }
                        ListElement { name: "Google Talk" }
                        ListElement { name: "Generic XMPP server" }
                    }
                    onSelectedIndexChanged: {
                        tiPass.text = ""
                        tiPort.text = "5222"
                        if (selectionDialog.selectedIndex == 0) {
                            tiJid.text = "@chat.facebook.com";
                            tiHost.text = "chat.facebook.com";
                        }
                        if (selectionDialog.selectedIndex == 1) {
                                tiJid.text = "@gmail.com";
                                tiHost.text = "talk.google.com";
                        }
                        if (selectionDialog.selectedIndex == 2) {
                                tiJid.text = "";
                                tiHost.text = "";
                        }
                    }
                }
            }

            Text {
                text: "Login"
                color: vars.textColor
            }
            TextField {
                id: tiJid
                height: 50
                anchors.horizontalCenter: parent.horizontalCenter
                width: accAddPage.width - 10
                placeholderText: qsTr("login@server.com")
                onActiveFocusChanged: main.splitscreenY = 0
            }

            Item {
                height: 5
                width: accAddPage.width
            }

            Text {
                text: "Password"
                color: vars.textColor
            }

            TextField {
                id: tiPass
                anchors.horizontalCenter: parent.horizontalCenter
                width: accAddPage.width-10
                height: 50
                echoMode: TextInput.Password
                placeholderText: qsTr("Password")
                onActiveFocusChanged: main.splitscreenY = 0
            }

            Item { height: 5; width: accAddPage.width}

            Item {
                height: 5
                width: accAddPage.width
            }

            Text {
                text: "Resource (optional)"
                color: vars.textColor
            }

            TextField {
                id: tiResource
                height: 50
                anchors.horizontalCenter: parent.horizontalCenter
                width: accAddPage.width-10
                placeholderText: qsTr("(default: Lightbulb)")

                onActiveFocusChanged: main.splitscreenY = inputContext.height - (main.height - y) + 1.5*height
            }

            Item {
                height: 5
                width: accAddPage.width
            }

            Text {
                text: "Server details"
                color: vars.textColor
                visible: selectionDialog.selectedIndex == 2
            }

            Rectangle {
                id: somethingInteresting
                height: 50
                width: accAddPage.width-20
                anchors.horizontalCenter: parent.horizontalCenter
                visible: selectionDialog.selectedIndex == 2
                color: "transparent"
                TextField {
                    id: tiHost
                    width: parent.width-10-tiPort.width
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    readOnly: !somethingInteresting.visible
                    placeholderText: "talk.google.com"

                    onActiveFocusChanged: main.splitscreenY = inputContext.height - (main.height - somethingInteresting.y) + 1.5*somethingInteresting.height
                }
                TextField {
                   id: tiPort
                   anchors.right: parent.right
                   anchors.top: parent.top
                   anchors.bottom: parent.bottom
                   width: 60
                   readOnly: !somethingInteresting.visible
                   placeholderText: "5222"

                   onActiveFocusChanged: main.splitscreenY = inputContext.height - (main.height - somethingInteresting.y) + 1.5*somethingInteresting.height
                }
            }

        }

    }


    /******************************************/

    ToolBarLayout {
        id: toolBarLayout
        ToolButton {
            iconSource: main.platformInverted ? "toolbar-back_inverse" : "toolbar-back"
            onClicked: {
                pageStack.pop()
                main.splitscreenY = 0
                statusBarText.text = "Accounts"
            }
        }

        ToolButton {
            iconSource: main.platformInverted ? "qrc:/toolbar/ok_inverse" : "qrc:/toolbar/ok"
            onClicked: {
                var jid = tiJid.text
                var pass = tiPass.text
                if( jid=="" || pass=="" ) return
                var host = tiHost.text
                var port = tiPort.text
                var resource = tiResource.text

                xmppConnectivity.setAccountData( jid, pass, false, resource, host, port,  true )

                pageStack.pop()
                pageStack.replace( "qrc:/pages/Accounts" )
            }
        }
    }
}
