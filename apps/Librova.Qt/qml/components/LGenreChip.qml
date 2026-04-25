import QtQuick
import QtQuick.Controls.Basic
import LibrovaQt

// Pill-shaped toggle chip for facet filters (language, genre).
//
// Checked state: amber border + amber text (SemiBold).
// Unchecked state: BorderStrong + TextSecondary.
// Hover/pressed/focus-visible states all animated at 120 ms.

Button {
    id: root

    checkable: true

    implicitHeight: 28
    implicitWidth:  Math.max(60, implicitContentWidth + 2 * LibrovaTheme.sp2)

    opacity: enabled ? 1.0 : 0.4

    background: Rectangle {
        radius:       LibrovaTheme.radiusLarge
        color:        root._bg()
        border.color: root._borderColor()
        border.width: root.activeFocus ? 2 : 1

        Behavior on color        { ColorAnimation { duration: LibrovaTheme.animFast } }
        Behavior on border.color { ColorAnimation { duration: LibrovaTheme.animFast } }
    }

    contentItem: Text {
        leftPadding:         LibrovaTheme.sp2
        rightPadding:        LibrovaTheme.sp2
        text:                root.text
        font.family:         LibrovaTypography.fontFamily
        font.pixelSize:      LibrovaTypography.sizeSm
        font.weight:         root.checked
                             ? LibrovaTypography.weightSemiBold
                             : LibrovaTypography.weightRegular
        color:               root._fg()
        verticalAlignment:   Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter

        Behavior on color { ColorAnimation { duration: LibrovaTheme.animFast } }
    }

    HoverHandler { cursorShape: Qt.PointingHandCursor }

    function _bg(): color {
        if (root.checked) {
            if (root.pressed) return LibrovaTheme.accentMuted
            if (root.hovered) return LibrovaTheme.accentSurfaceHover
            return LibrovaTheme.accentBorder
        }
        if (root.pressed) return LibrovaTheme.surfaceMuted
        if (root.hovered) return LibrovaTheme.surfaceHover
        return LibrovaTheme.surfaceAlt
    }

    function _borderColor(): color {
        if (root.activeFocus)           return LibrovaTheme.accentBright
        if (root.checked && root.hovered) return LibrovaTheme.accentBright
        if (root.checked)               return LibrovaTheme.accent
        return LibrovaTheme.borderStrong
    }

    function _fg(): color {
        if (root.checked) return root.hovered ? LibrovaTheme.accentBright : LibrovaTheme.accent
        if (root.hovered) return LibrovaTheme.textPrimary
        return LibrovaTheme.textSecondary
    }
}
