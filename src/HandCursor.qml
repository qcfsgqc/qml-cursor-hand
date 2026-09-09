import QtQuick

HoverHandler {
    acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
    cursorShape: Qt.PointingHandCursor
    Component.onCompleted: CursorHand.ensureWatch(parent)
}
