import QtQuick
import LibrovaQt

// SVG icon renderer.
//
// Draws a 24×24 SVG path scaled to `size` using the Canvas 2D API.
// All paths are authored on the shared 24×24 icon grid.
// Even-odd fill rule is used so icons with inner cut-outs render correctly
// (search lens hole, gear centre, trash can slots, etc.).
//
// Usage:
//   LIcon {
//       iconPath:  LibrovaIcons.search
//       iconColor: LibrovaTheme.accent
//       size:      20
//   }

Canvas {
    id: root

    property real   size:      20
    property string iconPath:  ""
    property color  iconColor: LibrovaTheme.textSecondary

    implicitWidth:  size
    implicitHeight: size

    onPaint: {
        var ctx = getContext("2d")
        ctx.clearRect(0, 0, width, height)
        if (!root.iconPath || root.width <= 0 || root.height <= 0)
            return
        ctx.save()
        ctx.scale(root.size / 24.0, root.size / 24.0)
        ctx.fillStyle = Qt.rgba(root.iconColor.r, root.iconColor.g, root.iconColor.b, root.iconColor.a)
        ctx.fillRule  = "evenodd"
        ctx.path      = root.iconPath
        ctx.fill()
        ctx.restore()
    }

    onSizeChanged:      requestPaint()
    onIconPathChanged:  requestPaint()
    onIconColorChanged: requestPaint()
}
