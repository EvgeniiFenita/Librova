import QtQuick
import QtQuick.Controls.Basic
import LibrovaQt

// Single-line text input with optional left icon.
//
// Focus ring: 2 px amber border on activeFocus.
// Left icon:  set leftIconPath to show an LIcon inside the field on the left.
// Disabled:   inherited opacity 0.38 from parent lockout or set enabled: false.

TextField {
    id: root

    property string leftIconPath:  ""
    property color  leftIconColor: LibrovaTheme.textSecondary
    property int    leftIconSize:  18
    readonly property bool hasTextInputFocus: activeFocus

    implicitWidth:  200
    implicitHeight: LibrovaTheme.controlHeight

    leftPadding:  leftIconPath.length > 0
                  ? LibrovaTheme.sp2 + leftIconSize + LibrovaTheme.sp2
                  : LibrovaTheme.sp3
    rightPadding: LibrovaTheme.sp3
    topPadding:   0
    bottomPadding: 0

    font.family:    LibrovaTypography.fontFamily
    font.pixelSize: LibrovaTypography.sizeBase
    color:          LibrovaTheme.textPrimary

    placeholderTextColor: LibrovaTheme.textMuted

    selectedTextColor: LibrovaTheme.textOnAccent
    selectionColor:    Qt.rgba(
        LibrovaTheme.accent.r,
        LibrovaTheme.accent.g,
        LibrovaTheme.accent.b,
        0.35)

    background: Rectangle {
        radius:       LibrovaTheme.radiusMedium
        color:        root.activeFocus ? LibrovaTheme.surfaceHover : LibrovaTheme.surfaceAlt
        border.color: root.activeFocus ? LibrovaTheme.accent : LibrovaTheme.border
        border.width: root.activeFocus ? 2 : 1

        Behavior on color        { ColorAnimation { duration: LibrovaTheme.animFast } }
        Behavior on border.color { ColorAnimation { duration: LibrovaTheme.animFast } }
        Behavior on border.width { NumberAnimation { duration: LibrovaTheme.animFast } }

        LIcon {
            visible:                root.leftIconPath.length > 0
            anchors.verticalCenter: parent.verticalCenter
            x:                      LibrovaTheme.sp2
            iconPath:               root.leftIconPath
            iconColor:              root.leftIconColor
            size:                   root.leftIconSize
        }
    }
}

