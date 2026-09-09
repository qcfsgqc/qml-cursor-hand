import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Cursor.Hand

ApplicationWindow {
    id: root
    width: 560
    height: 480
    visible: true
    title: qsTr("Cursor.Hand")

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 16

        Label {
            text: qsTr("Hover the items below. Default cursor is a pointing hand.")
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Button {
            text: qsTr("Button + attached")
            CursorHand.enabled: true
        }

        Button {
            text: qsTr("Busy override (2s)")
            CursorHand.enabled: true
            onClicked: {
                CursorHand.setOverride(Qt.WaitCursor)
                busyTimer.restart()
            }
        }

        Text {
            text: qsTr("Text + HandCursor child")
            font.pixelSize: 18
            HandCursor {}
        }

        Rectangle {
            Layout.preferredWidth: 240
            Layout.preferredHeight: 56
            radius: 8
            color: CursorHand.hovered ? "#2563eb" : "#1e293b"
            CursorHand.shape: Qt.OpenHandCursor

            Text {
                anchors.centerIn: parent
                text: qsTr("Rectangle attached OpenHand")
                color: "white"
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 56

            Rectangle {
                anchors.fill: parent
                radius: 8
                color: "#0f766e"
                Text {
                    anchors.centerIn: parent
                    text: qsTr("Custom Item + HandCursor")
                    color: "white"
                }
                HandCursor { cursorShape: Qt.PointingHandCursor }
            }
        }

        Item { Layout.fillHeight: true }
    }

    Timer {
        id: busyTimer
        interval: 2000
        onTriggered: CursorHand.restoreOverride()
    }
}
