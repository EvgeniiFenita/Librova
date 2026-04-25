import QtQuick
import LibrovaQt

// Brief non-blocking notification.
//
// Usage: toastId.show("message text", "info"|"warn"|"error")
//
// The toast fades in, stays for durationMs, then fades out.
// Multiple consecutive calls restart the timer.

Item {
    id: root

    property int durationMs: 3500

    anchors {
        bottom:       parent.bottom
        horizontalCenter: parent.horizontalCenter
        bottomMargin: 28
    }
    width:  _bubble.implicitWidth + LibrovaTheme.paddingCard * 2
    height: _bubble.implicitHeight + LibrovaTheme.sp3 * 2
    z:      999
    opacity: 0
    visible: opacity > 0

    function show(message, severity) {
        _label.text     = message
        _bubble.color   = severity === "error" ? LibrovaTheme.dangerSurface
                        : severity === "warn"  ? LibrovaTheme.warningSurface
                        :                        LibrovaTheme.surfaceElevated
        _label.color    = severity === "error" ? LibrovaTheme.danger
                        : severity === "warn"  ? LibrovaTheme.warning
                        :                        LibrovaTheme.textPrimary
        _hideTimer.restart()
        _showAnim.restart()
    }

    NumberAnimation {
        id: _showAnim
        target: root; property: "opacity"; to: 1; duration: LibrovaTheme.animFast
    }

    Timer {
        id: _hideTimer
        interval: root.durationMs
        onTriggered: _hideAnim.restart()
    }

    NumberAnimation {
        id: _hideAnim
        target: root; property: "opacity"; to: 0; duration: LibrovaTheme.animMedium
    }

    Rectangle {
        id: _bubble
        anchors.fill: parent
        radius:       LibrovaTheme.radiusMedium
        border.color: LibrovaTheme.accentBorder
        border.width: 1

        Text {
            id:             _label
            anchors {
                verticalCenter: parent.verticalCenter
                left:           parent.left
                right:          parent.right
                margins:        LibrovaTheme.paddingCard
            }
            font.family:    LibrovaTypography.fontFamily
            font.pixelSize: LibrovaTypography.sizeSm
            wrapMode:       Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
        }
    }
}
