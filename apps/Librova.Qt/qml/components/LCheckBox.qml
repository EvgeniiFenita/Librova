import QtQuick
import QtQuick.Controls.Basic
import LibrovaQt

// Themed checkbox using Librova design tokens.
//
// Usage matches standard QML CheckBox: text, checked, enabled, onToggled.
// Disabled state: full control fades to opacity 0.4.

CheckBox {
    id: root

    font.family:    LibrovaTypography.fontFamily
    font.pixelSize: LibrovaTypography.sizeBase

    opacity: root.enabled ? 1.0 : 0.4

    background: null

    indicator: Rectangle {
        implicitWidth:  18
        implicitHeight: 18
        x:              root.leftPadding
        y:              parent.height / 2 - height / 2
        radius:         4

        color:        root.checked ? LibrovaTheme.accent : LibrovaTheme.surfaceAlt
        border.color: root.checked ? LibrovaTheme.accent : LibrovaTheme.borderStrong
        border.width: 1

        Behavior on color        { ColorAnimation { duration: LibrovaTheme.animFast } }
        Behavior on border.color { ColorAnimation { duration: LibrovaTheme.animFast } }

        // Checkmark drawn once; opacity controls visibility.
        Canvas {
            anchors.centerIn: parent
            width:  12
            height: 12

            opacity: root.checked ? 1.0 : 0.0
            Behavior on opacity { NumberAnimation { duration: LibrovaTheme.animFast } }

            onPaint: {
                const ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                ctx.strokeStyle = LibrovaTheme.surface
                ctx.lineWidth   = 1.8
                ctx.lineCap     = "round"
                ctx.lineJoin    = "round"
                ctx.beginPath()
                ctx.moveTo(2, 6)
                ctx.lineTo(5, 9)
                ctx.lineTo(10, 3)
                ctx.stroke()
            }
        }
    }

    contentItem: Text {
        leftPadding:       root.indicator.width + root.spacing
        text:              root.text
        font:              root.font
        color:             LibrovaTheme.textPrimary
        verticalAlignment: Text.AlignVCenter
    }
}
